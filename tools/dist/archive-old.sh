#!/bin/sh
# Moves the packages of every version but this one out of the way: into
# <output dir>/vecchie/, where they stay. Called by each package-*.sh before
# it builds, so a dist/ directory only ever holds the version being handed out
# and nobody sends an old one by mistake.
#
# usage: archive-old.sh <output dir> <version>
#
# Only what a package script makes is moved -- cubes-and-portals-<version>-*,
# files and directories -- and nothing of this version; work directories and
# logs (flatpak-build, appimagetool.log) stay where they are.

set -eu

OUT="$1"
VERSION="$2"
[ -d "$OUT" ] || exit 0

for path in "$OUT"/cubes-and-portals-*; do
    [ -e "$path" ] || continue
    name="$(basename "$path")"
    case "$name" in
        cubes-and-portals-"$VERSION"-*) continue ;;
    esac
    mkdir -p "$OUT/vecchie"
    rm -rf "$OUT/vecchie/$name"   # the same old version packed twice: the later one kept
    mv "$path" "$OUT/vecchie/"
    echo "archived $name"
done
