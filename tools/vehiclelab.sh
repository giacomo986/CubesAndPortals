#!/bin/bash
# The vehicle lab: vehicles through portals (games/vehiclelab), over the
# sandbox's own mods. A server with no world file and a client on it.
#
#   tools/vehiclelab.sh                # build-release
#   BUILD=build tools/vehiclelab.sh    # the debug build
#   tools/vehiclelab.sh -- --use-at 305,151,330   # what follows -- goes to the client
set -eu
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BUILD="${BUILD:-$ROOT/build-release}"
PORT="${PORT:-25382}"
CLIENT_ARGS=()
[ "${1:-}" = "--" ] && { shift; CLIENT_ARGS=("$@"); }

cmake --build "$BUILD" -j --target cnp_server cnp_client | tail -1
"$BUILD/src/server/cnp_server" --listen "$PORT" --ticks 0 --user-mods "$ROOT/games/vehiclelab/mods" \
    > /tmp/vehiclelab-server.log 2>&1 &
SERVER=$!
trap 'kill $SERVER 2>/dev/null || true' EXIT
for _ in $(seq 1 300); do grep -q "listening on UDP" /tmp/vehiclelab-server.log && break; sleep 0.1; done
"$BUILD/src/client/cnp_client" --connect 127.0.0.1 --port "$PORT" --name "${NAME:-${USER:-tester}}" \
    ${CLIENT_ARGS[@]+"${CLIENT_ARGS[@]}"} || true
