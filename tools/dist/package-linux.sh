#!/bin/sh
# A build that somebody else can run (DESIGN.md M10, "build statiche Linux").
#
# Not an installer and not a package manager's package: a directory with the
# binaries and the files they ship with, tarred. What makes it work on a machine
# that is not this one is two things, and both are elsewhere --
#
#   - core/paths.h, which looks NEXT TO THE PROGRAM before it looks at the tree
#     the program was built from. Without that a copied binary reads mods out of
#     somebody else's checkout, which is the defect this whole step is about.
#   - CNP_PORTABLE, which links the C++ runtime in, because libstdc++ is the one
#     library a distribution cannot assume.
#
# usage: package-linux.sh [--allow-dynamic] <build dir> [output dir]
#
# The build directory has to be configured with -DCNP_PORTABLE=ON; this refuses
# to make a tarball out of binaries that carry a runtime dependency on whichever
# libstdc++ happened to be on the machine that built them. --allow-dynamic packs
# one anyway, which is for a tarball that is going to be unpacked on the machine
# that built it -- a gate, or a look at the layout -- and not for a release.

set -eu

ALLOW_DYNAMIC=0
if [ "${1:-}" = "--allow-dynamic" ]; then
    ALLOW_DYNAMIC=1
    shift
fi

BUILD="$(cd "$1" && pwd)"
OUT="${2:-$BUILD/dist}"
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"

VERSION="$(sed -n 's/.*kEngineVersion = "\([^"]*\)".*/\1/p' "$ROOT/src/core/version.h")"
[ -n "$VERSION" ] || { echo "package-linux.sh: no version in src/core/version.h" >&2; exit 1; }
"$(dirname "$0")/archive-old.sh" "$OUT" "$VERSION"   # older versions to $OUT/vecchie

ARCH="$(uname -m)"
NAME="cubes-and-portals-$VERSION-linux-$ARCH"
STAGE="$OUT/$NAME"

for binary in src/client/cnp_client src/server/cnp_server tools/cnp_modpack; do
    [ -x "$BUILD/$binary" ] || {
        echo "package-linux.sh: $BUILD/$binary is not built" >&2
        exit 1
    }
done

# The check that this is a build that travels. A tarball made out of a build
# that links libstdc++ dynamically runs here and dies on an older machine with a
# message about GLIBCXX versions that names neither the program nor the fix.
if ldd "$BUILD/src/server/cnp_server" | grep -q 'libstdc++'; then
    if [ "$ALLOW_DYNAMIC" = "0" ]; then
        echo "package-linux.sh: this build links libstdc++ dynamically." >&2
        echo "  configure with -DCNP_PORTABLE=ON, or pass --allow-dynamic to pack it anyway." >&2
        exit 1
    fi
    echo "package-linux.sh: packing a build that links libstdc++ dynamically," >&2
    echo "  which will not run on a machine with an older one." >&2
fi

rm -rf "$STAGE"
mkdir -p "$STAGE"

cp "$BUILD/src/client/cnp_client" "$STAGE/"
cp "$BUILD/src/server/cnp_server" "$STAGE/"
cp "$BUILD/tools/cnp_modpack"     "$STAGE/"

"$ROOT/tools/dist/stage.sh" "$STAGE"

# What somebody who unpacked this needs to know, and nothing else: the
# documentation lives in the repository, not in a release.
cat > "$STAGE/README.txt" <<TXT
Cubes and Portals $VERSION, Linux $ARCH

  ./cnp_client                      the title screen: a new world or a saved one,
                                    kept in ~/.local/share/cubes-and-portals/worlds
  ./cnp_client --quick              straight into a test world, nothing saved
  ./cnp_client --connect <host>     somebody else's world
  ./cnp_server --listen 25000       a world other people can join
  ./cnp_modpack pack mods/ore       a mod as one file

The programs read mods/ and assets/ from the directory they are in, so this
directory moves as a whole. A mod is a directory under mods/ or a .cnpmod file
in it.

Needs a GL 3.3 driver and X11 for the client; the server needs neither.
TXT

# Stripped, because a release is not a debugging session: the binaries carry
# -rdynamic for the crash stack, and that table is what stays.
strip --strip-debug "$STAGE/cnp_client" "$STAGE/cnp_server" "$STAGE/cnp_modpack" 2>/dev/null || true

TARBALL="$OUT/$NAME.tar.gz"
rm -f "$TARBALL"
tar -czf "$TARBALL" -C "$OUT" "$NAME"

SIZE="$(du -h "$TARBALL" | cut -f1)"
echo "$TARBALL ($SIZE)"
