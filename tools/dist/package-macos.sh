#!/bin/sh
# The macOS release (OPEN-40): "Cubes and Portals.app" in a .dmg. Runs ON a
# Mac -- the client needs Apple's SDK (Cocoa, OpenGL), which cannot be had on
# Linux -- with the Xcode command line tools, cmake and ninja. CI does it on a
# GitHub macOS runner (.github/workflows/release.yml).
#
# usage: package-macos.sh [build dir] [output dir]
#
# The programs are in Contents/MacOS, the mods and assets in
# Contents/Resources (core/paths.cpp looks there). The app is signed ad hoc,
# not by a developer: the first launch is right-click > Open, or
#   xattr -dr com.apple.quarantine "Cubes and Portals.app"

set -eu

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
BUILD="${1:-$ROOT/build-mac}"
OUT="${2:-$BUILD/dist}"

# OpenGL 3.3 core is what macOS still has (deprecated, not removed); 11.0 is
# the first release that runs on Apple silicon.
[ -f "$BUILD/build.ninja" ] ||
    cmake -G Ninja -S "$ROOT" -B "$BUILD" -DCMAKE_BUILD_TYPE=Release \
          -DCMAKE_OSX_DEPLOYMENT_TARGET=11.0 > /dev/null
cmake --build "$BUILD" --target cnp_client cnp_server cnp_modpack

VERSION="$(sed -n 's/.*kEngineVersion = "\([^"]*\)".*/\1/p' "$ROOT/src/core/version.h")"
"$(dirname "$0")/archive-old.sh" "$OUT" "$VERSION"   # older versions to $OUT/vecchie
NAME="cubes-and-portals-$VERSION-macos-$(uname -m)"
APP="$OUT/Cubes and Portals.app"

rm -rf "$APP"
mkdir -p "$APP/Contents/MacOS" "$APP/Contents/Resources"
cp "$BUILD/src/client/cnp_client" "$BUILD/src/server/cnp_server" "$BUILD/tools/cnp_modpack" \
   "$APP/Contents/MacOS/"
"$ROOT/tools/dist/stage.sh" "$APP/Contents/Resources"

ICONSET="$OUT/icon.iconset"
rm -rf "$ICONSET"
mkdir -p "$ICONSET"
for s in 16 32 128 256; do
    sips -z $s $s "$ROOT/tools/dist/icon.png" --out "$ICONSET/icon_${s}x${s}.png" > /dev/null
done
iconutil -c icns "$ICONSET" -o "$APP/Contents/Resources/icon.icns"
rm -rf "$ICONSET"

cat > "$APP/Contents/Info.plist" <<PLIST
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
    <key>CFBundleName</key>               <string>Cubes and Portals</string>
    <key>CFBundleIdentifier</key>         <string>io.github.giacomoperin.CubesAndPortals</string>
    <key>CFBundleVersion</key>            <string>$VERSION</string>
    <key>CFBundleShortVersionString</key> <string>$VERSION</string>
    <key>CFBundleExecutable</key>         <string>cnp_client</string>
    <key>CFBundleIconFile</key>           <string>icon</string>
    <key>CFBundlePackageType</key>        <string>APPL</string>
    <key>LSMinimumSystemVersion</key>     <string>11.0</string>
    <key>NSHighResolutionCapable</key>    <true/>
</dict>
</plist>
PLIST

# Ad hoc: Apple silicon will not run an unsigned binary at all, and the
# bundle's seal has to cover what was just copied in.
codesign --force --deep --sign - "$APP"

DMG="$OUT/$NAME.dmg"
rm -f "$DMG"
hdiutil create -volname "Cubes and Portals" -srcfolder "$APP" -ov -format UDZO "$DMG" > /dev/null
echo "$DMG ($(du -h "$DMG" | cut -f1))"
