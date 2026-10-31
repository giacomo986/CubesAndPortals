#!/bin/sh
# The Flatpak repository as a web site (GitHub Pages, .github/workflows/
# release.yml): the OSTree repository package-flatpak.sh exported, signed,
# under repo/, a .flatpakref to install the game from it and a .flatpakrepo
# to add it as a remote, both carrying the public key, and a page that says
# how. Installed once from the .flatpakref, `flatpak update` brings each new
# release.
#
# usage: flatpak-site.sh <build dir> <site dir> [base url]

set -eu

APP_ID="io.github.giacomoperin.CubesAndPortals"
BUILD="$(cd "$1" && pwd)"
SITE="$2"
BASE="${3:-https://giacomo986.github.io/CubesAndPortals}"
HERE="$(cd "$(dirname "$0")" && pwd)"
REPO="$BUILD/dist/flatpak-repo"
[ -f "$REPO/config" ] || { echo "flatpak-site.sh: no repository at $REPO (run package-flatpak.sh)" >&2; exit 1; }

rm -rf "$SITE"
mkdir -p "$SITE"
cp -r "$REPO" "$SITE/repo"
KEY="$(base64 -w0 "$HERE/flatpak-repo.gpg")"

cat > "$SITE/cubes-and-portals.flatpakref" <<REF
[Flatpak Ref]
Name=$APP_ID
Branch=master
Title=Cubes and Portals
Url=$BASE/repo/
SuggestRemoteName=cubes-and-portals
RuntimeRepo=https://dl.flathub.org/repo/flathub.flatpakrepo
IsRuntime=false
GPGKey=$KEY
REF

cat > "$SITE/cubes-and-portals.flatpakrepo" <<REPO
[Flatpak Repo]
Title=Cubes and Portals
Url=$BASE/repo/
Homepage=https://github.com/giacomo986/CubesAndPortals
GPGKey=$KEY
REPO

cat > "$SITE/index.html" <<HTML
<!doctype html>
<meta charset="utf-8">
<title>Cubes and Portals: Flatpak</title>
<h1>Cubes and Portals</h1>
<p>Install once, and <code>flatpak update</code> brings every new release:</p>
<pre>flatpak install --user $BASE/cubes-and-portals.flatpakref</pre>
<p>Or add the repository: <code>flatpak remote-add --user cubes-and-portals $BASE/cubes-and-portals.flatpakrepo</code>.</p>
<p>Releases: <a href="https://github.com/giacomo986/CubesAndPortals/releases">github.com/giacomo986/CubesAndPortals/releases</a></p>
HTML
touch "$SITE/.nojekyll"
echo "$SITE ($(du -sh "$SITE" | cut -f1))"
