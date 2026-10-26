#!/bin/sh
# The Windows release (OPEN-40), cross-compiled here: zig's clang and MinGW
# are the whole toolchain (zig-windows.cmake), and the programs need nothing
# but what Windows 10 ships -- the UCRT, OpenGL32, Winsock. A zip, unpacked
# anywhere and run from there: cnp_client.exe reads mods\ and assets\ from
# beside itself, like the Linux tarball.
#
# usage: package-windows.sh [build dir] [output dir]
#
# zig 0.16 on the PATH. The build directory is configured here if it is not
# already (default build-win).

set -eu

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
BUILD="${1:-$ROOT/build-win}"
OUT="${2:-$BUILD/dist}"
command -v zig > /dev/null || { echo "package-windows.sh: no zig on the PATH" >&2; exit 1; }

[ -f "$BUILD/build.ninja" ] ||
    cmake -G Ninja -S "$ROOT" -B "$BUILD" -DCMAKE_BUILD_TYPE=Release -DCNP_PORTABLE=ON \
          -DCMAKE_TOOLCHAIN_FILE="$ROOT/tools/dist/zig-windows.cmake" > /dev/null
cmake --build "$BUILD" --target cnp_client cnp_server cnp_modpack

VERSION="$(sed -n 's/.*kEngineVersion = "\([^"]*\)".*/\1/p' "$ROOT/src/core/version.h")"
"$(dirname "$0")/archive-old.sh" "$OUT" "$VERSION"   # older versions to $OUT/vecchie
NAME="cubes-and-portals-$VERSION-windows-x86_64"
STAGE="$OUT/$NAME"

rm -rf "$STAGE"
mkdir -p "$STAGE"
cp "$BUILD/src/client/cnp_client.exe" "$BUILD/src/server/cnp_server.exe" \
   "$BUILD/tools/cnp_modpack.exe" "$STAGE/"
"$ROOT/tools/dist/stage.sh" "$STAGE"

# CRLF, so Notepad shows it as lines.
sed 's/$/\r/' > "$STAGE/README.txt" <<TXT
Cubes and Portals $VERSION, Windows x86_64

  cnp_client.exe                    the title screen (double-click it); worlds
                                    are kept in %APPDATA%\\cubes-and-portals
  cnp_server.exe --listen 25000     a world other people can join (from cmd)
  cnp_modpack.exe pack mods\\ore     a mod as one file

The programs read mods\\ and assets\\ from the folder they are in, so the
folder moves as a whole. Needs Windows 10 or later and an OpenGL 3.3 driver.
The programs are not signed: Windows SmartScreen asks once, "More info" then
"Run anyway".
TXT

ZIP="$OUT/$NAME.zip"
rm -f "$ZIP"
(cd "$OUT" && zip -qr "$NAME.zip" "$NAME")
echo "$ZIP ($(du -h "$ZIP" | cut -f1))"
