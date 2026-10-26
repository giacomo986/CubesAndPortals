#!/bin/bash
# The assault's escape, to try it alone: a server with the assault's world
# only, and a client in it. A few seconds after you are in, the strongholds
# and the hall are skipped (tools/assault_escape): you are put in the hall,
# Basoalth breaks the core and rises, the floor falls in, and the tunnel's
# jeeps are waiting. Everybody caught: the road is made again and you try
# again. Its world is not kept.
#
#   tools/escape.sh                 # build, serve, play
#   tools/escape.sh -- --shot 30    # what follows -- goes to the client
set -eu

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BUILD="${BUILD:-$ROOT/build-release}"
PORT="${PORT:-25321}"
NAME="${NAME:-${USER:-giocatore}}"
HERE="${CNP_ESCAPE_DIR:-$ROOT/worlds/escape}"

CLIENT_ARGS=()
while [ $# -gt 0 ]; do
    case "$1" in
        --) shift; CLIENT_ARGS=("$@"); break ;;
        -h|--help) sed -n '2,10p' "$0" | sed 's/^# \{0,1\}//'; exit 0 ;;
        *) echo "escape.sh: non conosco '$1' (-- ARGOMENTI DEL CLIENT)"; exit 2 ;;
    esac
    shift
done

if [ ! -f "$BUILD/build.ninja" ]; then
    echo "== configuro $BUILD (Release)"
    cmake -S "$ROOT" -B "$BUILD" -G Ninja -DCMAKE_BUILD_TYPE=Release > /dev/null
fi
echo "== compilo cnp_server e cnp_client"
cmake --build "$BUILD" -j --target cnp_server cnp_client | tail -1
SRV="$BUILD/src/server/cnp_server"
CLI="$BUILD/src/client/cnp_client"

# The assault's mods and the shortcut beside them, linked.
MODS="$HERE/mods"
rm -rf "$MODS"
mkdir -p "$MODS"
for m in "$ROOT"/games/assault/mods/*/; do ln -s "${m%/}" "$MODS/"; done
ln -s "$ROOT/tools/assault_escape" "$MODS/assault_escape"

if ss -uln 2>/dev/null | grep -q ":$PORT "; then
    echo "escape.sh: la porta UDP $PORT è già in uso; PORT=25331 tools/escape.sh"
    exit 1
fi

SERVER=""
stop_server() {
    [ -n "$SERVER" ] || return 0
    kill -0 "$SERVER" 2>/dev/null || return 0
    kill -TERM "$SERVER" 2>/dev/null || true
    wait "$SERVER" 2>/dev/null || true
}
trap stop_server EXIT
trap 'exit 130' INT TERM

echo "== avvio il server dell'assalto sulla porta $PORT"
# --ticks 0: senza, un server in ascolto si ferma dopo 50 s.
"$SRV" --listen "$PORT" --ticks 0 --mods "$MODS" > "$HERE/server.log" 2>&1 &
SERVER=$!
for _ in $(seq 1 300); do
    grep -q "listening on UDP" "$HERE/server.log" 2>/dev/null && break
    if ! kill -0 "$SERVER" 2>/dev/null; then
        echo "escape.sh: il server si è fermato:"; tail -20 "$HERE/server.log"; exit 1
    fi
    sleep 0.1
done

cat <<INFO

== La fuga

  Tre secondi dopo che sei dentro sei nella sala del nucleo: Basoalth si
  alza, il pavimento crolla, giù fino alle jeep. F per salire (al volante
  guidi, da passeggero spari con la tua arma, 1-9 la sceglie), WASD guida.
  Spara alle valvole, ai massi sotto il soffitto e ai serbatoi per
  rallentarlo; la testa lo fa barcollare.
  Log: $HERE/server.log

INFO

echo "== entro come $NAME"
stdbuf -oL "$CLI" --connect 127.0.0.1 --port "$PORT" --name "$NAME" \
    --player-key "$HERE/player.key" ${CLIENT_ARGS[@]+"${CLIENT_ARGS[@]}"} 2>&1 | tee "$HERE/client.log" || true
