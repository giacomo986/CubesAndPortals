#!/bin/bash
# A scene for checking the bodies seen through a window onto another world: a
# server with two rooms, a client in the first looking at a mouth, and in the
# second, where nobody is, six creatures going by. No world files; every start
# is new. The client is stood in front of the mouth, so what the window shows is
# all that happens: they must be there, moving, cut by the mouth's frame.
#
#   tools/windowlab.sh                # build-release
#   BUILD=build tools/windowlab.sh    # the debug build
#   tools/windowlab.sh -- --shot 20   # what follows -- goes to the client
set -eu
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BUILD="${BUILD:-$ROOT/build-release}"
PORT="${PORT:-25382}"
HERE="$(mktemp -d /tmp/cnp-windowlab.XXXXXX)"
CLIENT_ARGS=()
[ "${1:-}" = "--" ] && { shift; CLIENT_ARGS=("$@"); }

mkdir "$HERE/here" "$HERE/there"
ln -s "$ROOT/mods/robot" "$HERE/here/"
ln -s "$ROOT/mods/robot" "$ROOT/mods/mobs" "$HERE/there/"
ln -s "$ROOT/games/windowlab/here/windowlab_here" "$HERE/here/"
ln -s "$ROOT/games/windowlab/there/windowlab_there" "$HERE/there/"

cmake --build "$BUILD" -j --target cnp_server cnp_client | tail -1
"$BUILD/src/server/cnp_server" --listen "$PORT" --ticks 0 --portals --key-file "$HERE/key" \
    --day-length 100000 --no-natural-mobs --textures "$ROOT/assets/textures" \
    --room here --mods "$HERE/here" --portal-to @there/0 \
    --room there --mods "$HERE/there" --portal-to @here/0 \
    > "$HERE/server.log" 2>&1 &
SERVER=$!
trap 'kill $SERVER 2>/dev/null || true; echo "logs in $HERE"' EXIT
for _ in $(seq 1 100); do grep -q "listening on UDP" "$HERE/server.log" && break; sleep 0.1; done
"$BUILD/src/client/cnp_client" --connect 127.0.0.1 --port "$PORT" --name "${NAME:-${USER:-tester}}" \
    ${CLIENT_ARGS[@]+"${CLIENT_ARGS[@]}"} > "$HERE/client.log" 2>&1 || true
