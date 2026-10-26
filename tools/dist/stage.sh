#!/bin/sh
# The files every release carries beside its programs, under the names
# core/paths.h looks for: this layout IS the contract with it. Each
# package-*.sh copies its own binaries and then calls this.
#
# usage: stage.sh <directory>

set -eu
DEST="$1"
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"

mkdir -p "$DEST/assets"
cp -r "$ROOT/mods"  "$DEST/mods"
cp -r "$ROOT/games" "$DEST/games"
for d in textures sounds models lang; do
    cp -r "$ROOT/assets/$d" "$DEST/assets/$d"
done
