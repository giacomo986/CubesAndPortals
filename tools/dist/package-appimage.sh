#!/bin/sh
# The Linux release as one file (OPEN-40): the tarball's directory made an
# AppDir and packed by appimagetool. Run it and it is the client; its first
# argument `server` or `modpack` runs the other two programs instead.
#
# usage: package-appimage.sh <build dir> [output dir]
#
# The build directory is configured as package-linux.sh wants it
# (-DCNP_PORTABLE=ON). appimagetool is taken from $APPIMAGETOOL, else from the
# PATH: https://github.com/AppImage/appimagetool/releases (continuous).

set -eu

BUILD="$(cd "$1" && pwd)"
OUT="${2:-$BUILD/dist}"
HERE="$(cd "$(dirname "$0")" && pwd)"
TOOL="${APPIMAGETOOL:-$(command -v appimagetool || true)}"
[ -n "$TOOL" ] && [ -x "$TOOL" ] || {
    echo "package-appimage.sh: no appimagetool (set APPIMAGETOOL or put it on the PATH)" >&2
    exit 1
}

# The tarball's staging directory is the AppDir's contents: the programs read
# mods/, games/ and assets/ from beside themselves (core/paths.h).
"$HERE/package-linux.sh" "$BUILD" "$OUT" > /dev/null
VERSION="$(sed -n 's/.*kEngineVersion = "\([^"]*\)".*/\1/p' "$HERE/../../src/core/version.h")"
"$(dirname "$0")/archive-old.sh" "$OUT" "$VERSION"   # older versions to $OUT/vecchie
NAME="cubes-and-portals-$VERSION-linux-$(uname -m)"
STAGE="$OUT/$NAME"
APPDIR="$OUT/$NAME.AppDir"

rm -rf "$APPDIR"
cp -r "$STAGE" "$APPDIR"
cp "$HERE/icon.png" "$APPDIR/cubes-and-portals.png"

cat > "$APPDIR/AppRun" <<'RUN'
#!/bin/sh
HERE="$(dirname "$(readlink -f "$0")")"
case "${1:-}" in
    server)  shift; exec "$HERE/cnp_server" "$@" ;;
    modpack) shift; exec "$HERE/cnp_modpack" "$@" ;;
esac
exec "$HERE/cnp_client" "$@"
RUN
chmod +x "$APPDIR/AppRun"

cat > "$APPDIR/cubes-and-portals.desktop" <<'DESKTOP'
[Desktop Entry]
Type=Application
Name=Cubes and Portals
Comment=A voxel sandbox whose portals lead to other worlds
Exec=AppRun
Icon=cubes-and-portals
Categories=Game;
Terminal=false
DESKTOP

IMAGE="$OUT/$NAME.AppImage"
rm -f "$IMAGE"
ARCH="$(uname -m)" "$TOOL" --no-appstream "$APPDIR" "$IMAGE" > "$OUT/appimagetool.log" 2>&1 || {
    echo "package-appimage.sh: appimagetool failed:" >&2
    tail -20 "$OUT/appimagetool.log" >&2
    exit 1
}
echo "$IMAGE ($(du -h "$IMAGE" | cut -f1))"
