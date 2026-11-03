#!/bin/bash
# The portal lab: a scene for checking portals by eye (games/portallab). A
# server with no world file -- the scene is made new every start -- and a
# client on it. /lab wall, glass, head, bounce and view move you between its
# stations.
#
#   tools/portallab.sh                # build-release
#   BUILD=build tools/portallab.sh    # the debug build
#   tools/portallab.sh -- --shot 10   # what follows -- goes to the client
set -eu
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BUILD="${BUILD:-$ROOT/build-release}"
PORT="${PORT:-25381}"
CLIENT_ARGS=()
[ "${1:-}" = "--" ] && { shift; CLIENT_ARGS=("$@"); }

cmake --build "$BUILD" -j --target cnp_server cnp_client | tail -1
"$BUILD/src/server/cnp_server" --listen "$PORT" --ticks 0 --mods "$ROOT/games/portallab/mods" \
    --textures "$ROOT/games/portallab/textures" --fallback-textures "$ROOT/assets/textures" \
    > /tmp/portallab-server.log 2>&1 &
SERVER=$!
trap 'kill $SERVER 2>/dev/null || true' EXIT
for _ in $(seq 1 100); do grep -q "listening on UDP" /tmp/portallab-server.log && break; sleep 0.1; done
"$BUILD/src/client/cnp_client" --connect 127.0.0.1 --port "$PORT" --name "${NAME:-${USER:-tester}}" \
    ${CLIENT_ARGS[@]+"${CLIENT_ARGS[@]}"} || true
