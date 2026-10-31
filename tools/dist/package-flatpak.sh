#!/bin/sh
# The Linux release as a Flatpak bundle (OPEN-40): the tarball's directory
# under /app/lib/cubes-and-portals, on the freedesktop Platform runtime. No
# flatpak-builder and no SDK: the portable build already runs on any glibc from
# 2.38, and the 25.08 runtime has 2.42, so it is copied in as the AppImage
# copies it and the bundle is made with flatpak's own build-* commands.
#
# usage: package-flatpak.sh <build dir> [output dir]
#
# The build directory is configured as package-linux.sh wants it
# (-DCNP_PORTABLE=ON). Installed with
#   flatpak install --user cubes-and-portals-*.flatpak
# which fetches the runtime from Flathub if it is missing; then the menu entry,
# or `flatpak run $APP_ID`, `... server`, `... modpack`.

set -eu

APP_ID="io.github.giacomoperin.CubesAndPortals"
RUNTIME_BRANCH="25.08"

BUILD="$(cd "$1" && pwd)"
OUT="${2:-$BUILD/dist}"
HERE="$(cd "$(dirname "$0")" && pwd)"
command -v flatpak > /dev/null || { echo "package-flatpak.sh: no flatpak" >&2; exit 1; }
flatpak info org.freedesktop.Platform//$RUNTIME_BRANCH > /dev/null 2>&1 || {
    echo "package-flatpak.sh: install the runtime first:" >&2
    echo "  flatpak install --user flathub org.freedesktop.Platform//$RUNTIME_BRANCH" >&2
    exit 1
}

"$HERE/package-linux.sh" "$BUILD" "$OUT" > /dev/null
VERSION="$(sed -n 's/.*kEngineVersion = "\([^"]*\)".*/\1/p' "$HERE/../../src/core/version.h")"
"$(dirname "$0")/archive-old.sh" "$OUT" "$VERSION"   # older versions to $OUT/vecchie
NAME="cubes-and-portals-$VERSION-linux-$(uname -m)"
DIR="$OUT/flatpak-build"
REPO="$OUT/flatpak-repo"

rm -rf "$DIR"
flatpak build-init "$DIR" "$APP_ID" org.freedesktop.Platform org.freedesktop.Platform \
    "$RUNTIME_BRANCH" > /dev/null
mkdir -p "$DIR/files/lib" "$DIR/files/bin" "$DIR/files/share/applications" \
         "$DIR/files/share/icons/hicolor/256x256/apps"
cp -r "$OUT/$NAME" "$DIR/files/lib/cubes-and-portals"

# The AppImage's AppRun, under the name the .desktop and `flatpak run` use.
cat > "$DIR/files/bin/cubes-and-portals" <<'RUN'
#!/bin/sh
D=/app/lib/cubes-and-portals
case "${1:-}" in
    server)  shift; exec "$D/cnp_server" "$@" ;;
    modpack) shift; exec "$D/cnp_modpack" "$@" ;;
esac
exec "$D/cnp_client" "$@"
RUN
chmod +x "$DIR/files/bin/cubes-and-portals"

cp "$HERE/icon.png" "$DIR/files/share/icons/hicolor/256x256/apps/$APP_ID.png"
cat > "$DIR/files/share/applications/$APP_ID.desktop" <<DESKTOP
[Desktop Entry]
Type=Application
Name=Cubes and Portals
Comment=A voxel sandbox whose portals lead to other worlds
Exec=cubes-and-portals
Icon=$APP_ID
Categories=Game;
Terminal=false
DESKTOP

# X11 only: the client's GLFW is built without Wayland, so on a Wayland
# desktop it runs through XWayland.
flatpak build-finish "$DIR" --command=cubes-and-portals \
    --share=network --share=ipc --socket=x11 --socket=pulseaudio --device=dri > /dev/null
# Signed when a key is given (the CI's, from its secret): the repository it
# exports into is the one tools/dist/flatpak-site.sh publishes for `flatpak
# update`. FLATPAK_GPG_KEY_ID and, if not gpg's own, FLATPAK_GPG_HOMEDIR.
SIGN=""
if [ -n "${FLATPAK_GPG_KEY_ID:-}" ]; then
    SIGN="--gpg-sign=$FLATPAK_GPG_KEY_ID${FLATPAK_GPG_HOMEDIR:+ --gpg-homedir=$FLATPAK_GPG_HOMEDIR}"
fi
rm -rf "$REPO"
# shellcheck disable=SC2086
flatpak build-export $SIGN "$REPO" "$DIR" > /dev/null
# shellcheck disable=SC2086
flatpak build-update-repo $SIGN --title="Cubes and Portals" --default-branch=master \
    --generate-static-deltas "$REPO" > /dev/null

BUNDLE="$OUT/$NAME.flatpak"
rm -f "$BUNDLE"
flatpak build-bundle "$REPO" "$BUNDLE" "$APP_ID" \
    --runtime-repo=https://dl.flathub.org/repo/flathub.flatpakrepo
echo "$BUNDLE ($(du -h "$BUNDLE" | cut -f1))"
