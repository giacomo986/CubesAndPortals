#!/bin/bash
# Type-checks every .luau in the repository against docs/api.d.luau, and
# compiles each one the way the engine does, to catch what the checker cannot.
#
#   tools/typecheck.sh                  # everything: mods/, games/, tests/, tools/
#   tools/typecheck.sh mods/pipes/init.luau games/lab/mods/lab/init.luau
#   ALL=1 tools/typecheck.sh            # with the expected leftovers too
#
# Why a build of its own: Luau's luau-analyze has no way to be handed a
# definitions file, so run as it comes it does not know `core` and drowns the
# real errors in "Unknown global 'core'". This builds it from third_party/luau
# in build-typecheck/ with one flag added, --definitions=<file>, which loads
# the file into the global scope before anything is checked -- what an
# editor's language server does with the same file. The first run takes a few
# minutes; after that only what changed is rebuilt.
#
# The second pass is luau-compile at -O2 (the engine's level, see
# compile_options in src/script/script_luau.cpp): the checker does not count
# registers, and a main chunk past 200 locals type-checks fine and then does
# not load.
#
# What is left out unless ALL=1, because it is not the code's fault:
#   - "Unknown require" and what follows from it ("Unknown type 'L.Zone'",
#     "Cannot require module ...") for `require("./x")` in an init.luau. The
#     engine looks for x beside the file; Luau now reads `./` in an init.luau
#     as the folder above it (`@self` is beside it).
#   - tests/mods/parts: a cyclic require and an undeclared global, on purpose.
#   - tools/api_gen/gen.luau: its globals (API, ROOT, read_file, write_file)
#     are put there by cnp_api_gen, not by the game.
#
# Exit status 1 when something is left.
set -eu

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
LUAU="$ROOT/third_party/luau"
OUT="$ROOT/build-typecheck"
ANALYZE="$OUT/luau-analyze-defs"
COMPILE="$OUT/luau/luau-compile"

# -------------------------------------------------------------------- build
mkdir -p "$OUT"
if [ ! -f "$OUT/luau/build.ninja" ] && [ ! -f "$OUT/luau/Makefile" ]; then
    gen=""
    command -v ninja >/dev/null && gen="-G Ninja"
    cmake -S "$LUAU" -B "$OUT/luau" $gen -DCMAKE_BUILD_TYPE=Release \
        -DLUAU_BUILD_CLI=ON -DLUAU_BUILD_TESTS=OFF >/dev/null
fi
cmake --build "$OUT/luau" --target Luau.Analysis Luau.CLI.lib Luau.Require Luau.Config \
    Luau.Compiler Luau.Ast Luau.Bytecode Luau.VM Luau.Common Luau.Compile.CLI \
    -j"$(nproc 2>/dev/null || echo 4)" >/dev/null

# Analyze.cpp with --definitions, rewritten each time the original changes.
SRC="$OUT/Analyze.cpp"
if [ ! -f "$SRC" ] || [ "$LUAU/CLI/src/Analyze.cpp" -nt "$SRC" ] || [ "$0" -nt "$SRC" ]; then
    python3 - "$LUAU/CLI/src/Analyze.cpp" "$SRC" <<'EOF'
import sys
src, dst = sys.argv[1], sys.argv[2]
s = open(src).read()
anchor = "    Luau::freeze(frontend.globals.globalTypes);"
if s.count(anchor) != 1:
    sys.exit("tools/typecheck.sh: Analyze.cpp has changed shape; the --definitions hook needs a new anchor")
hook = '''    for (int i = 1; i < argc; ++i)
        if (strncmp(argv[i], "--definitions=", 14) == 0)
        {
            std::optional<std::string> defs = readFile(argv[i] + 14);
            if (!defs)
            {
                fprintf(stderr, "cannot read %s\\n", argv[i] + 14);
                return 1;
            }
            Luau::LoadDefinitionFileResult r = frontend.loadDefinitionFile(
                frontend.globals, frontend.globals.globalScope, *defs, "@defs", false, false);
            if (!r.success)
            {
                fprintf(stderr, "%s does not load:\\n", argv[i] + 14);
                for (const Luau::ParseError& e : r.parseResult.errors)
                    fprintf(stderr, "  line %d: %s\\n", e.getLocation().begin.line + 1, e.getMessage().c_str());
                if (r.module)
                    for (const Luau::TypeError& e : r.module->errors)
                        fprintf(stderr, "  line %d: %s\\n", e.location.begin.line + 1, Luau::toString(e).c_str());
                return 1;
            }
        }
'''
open(dst, "w").write(s.replace(anchor, hook + anchor))
EOF
fi
if [ ! -x "$ANALYZE" ] || [ "$SRC" -nt "$ANALYZE" ] || [ "$OUT/luau/libLuau.Analysis.a" -nt "$ANALYZE" ]; then
    INC=()
    for d in Analysis Ast Common Config CLI Require VM; do INC+=("-I$LUAU/$d/include"); done
    c++ -O2 -std=c++17 "${INC[@]}" -c "$SRC" -o "$OUT/Analyze.o"
    c++ -O2 -std=c++17 "${INC[@]}" -c "$LUAU/CLI/src/AnalyzeRequirer.cpp" -o "$OUT/AnalyzeRequirer.o"
    L="$OUT/luau"
    c++ "$OUT/Analyze.o" "$OUT/AnalyzeRequirer.o" -o "$ANALYZE" \
        "$L/libLuau.Analysis.a" "$L/libLuau.CLI.lib.a" "$L/libLuau.Require.a" "$L/libLuau.Config.a" \
        "$L/libLuau.Compiler.a" "$L/libLuau.Ast.a" "$L/libLuau.Bytecode.a" "$L/libLuau.VM.a" \
        "$L/libLuau.Common.a" -lpthread
fi

# -------------------------------------------------------------------- check
cd "$ROOT"
if [ $# -gt 0 ]; then
    FILES=("$@")
else
    mapfile -t FILES < <(find mods games tests tools -name '*.luau' ! -name '*.d.luau' | sort)
fi

LEFTOVER='Unknown require|Unknown type .L\.|Cannot require module|^\./tests/mods/parts/|^\./tools/api_gen/'
# The checker names a file both as given and by its absolute path when another
# file requires it: one form, then each error once.
errors=$("$ANALYZE" --definitions=docs/api.d.luau --formatter=gnu "${FILES[@]}" 2>&1 \
    | grep -E 'TypeError|SyntaxError|does not load|^  line ' \
    | sed "s|^$ROOT/|./|; s|^\([a-z]\)|./\1|" | sort -u || true)
if [ "${ALL:-0}" != 1 ]; then
    errors=$(printf '%s\n' "$errors" | grep -Ev "$LEFTOVER" || true)
fi

compile=""
for f in "${FILES[@]}"; do
    msg=$("$COMPILE" --null -O2 "$f" 2>&1 >/dev/null) || compile+="$msg"$'\n'
done

status=0
if [ -n "$errors" ]; then
    printf '%s\n' "$errors"
    status=1
fi
if [ -n "$compile" ]; then
    printf '%s' "$compile"
    status=1
fi
n=$(printf '%s' "$errors" | grep -c . || true)
echo "typecheck: ${#FILES[@]} files, $n type errors$([ -n "$compile" ] && echo ', and compile errors')" >&2
exit $status
