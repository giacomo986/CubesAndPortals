#!/bin/bash
# Il sandbox (i mod di mods/) con un portale verso il mondo shmup: uno
# sparatutto visto dall'alto su un caccia, da cabinato di sala giochi. Lo
# schermo non scorre: W A S D muovono il caccia, spazio (tenuto) spara e fa
# partire la partita, E torna al ponte della portaerei.
#
#   tools/shmup.sh                 # un server con due stanze e un client nel sandbox
#   tools/shmup.sh --fresh         # butta via i due mondi salvati
#   tools/shmup.sh -- --shot 20    # quello dopo -- va al client
#   tools/shmup.sh -- --room shmup # si nasce già sul ponte
#
# Si nasce davanti alla prima apertura del sandbox: dentro si è sul ponte di
# una portaerei, già in volo e visti dall'alto. Mondi, chiavi e log stanno in
# worlds/shmup/ (ignorata da git).
set -eu

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BUILD="${BUILD:-$ROOT/build-release}"
SEED="${SEED:-1}"
PORT="${PORT:-25381}"
NAME="${NAME:-${USER:-giocatore}}"
HERE="${CNP_SHMUP_DIR:-$ROOT/worlds/shmup}"

FRESH=0
CLIENT_ARGS=()
while [ $# -gt 0 ]; do
    case "$1" in
        --fresh) FRESH=1 ;;
        --)      shift; CLIENT_ARGS=("$@"); break ;;
        -h|--help) sed -n '2,13p' "$0" | sed 's/^# \{0,1\}//'; exit 0 ;;
        *) echo "shmup.sh: non conosco '$1' (--fresh, -- ARGOMENTI DEL CLIENT)"; exit 2 ;;
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

mkdir -p "$HERE"
if [ "$FRESH" = 1 ]; then
    rm -f "$HERE"/sandbox.world* "$HERE"/shmup.world*
    echo "== mondi salvati cancellati"
fi

if ss -uln 2>/dev/null | grep -q ":$PORT "; then
    echo "shmup.sh: la porta UDP $PORT è già in uso; PORT=25391 tools/shmup.sh"
    exit 1
fi

SERVER=""
# SIGTERM fa salvare e chiudere: aspetta, così i file restano interi.
stop_server() {
    [ -n "$SERVER" ] || return 0
    kill -0 "$SERVER" 2>/dev/null || return 0
    kill -TERM "$SERVER" 2>/dev/null || true
    wait "$SERVER" 2>/dev/null || true
}
trap stop_server EXIT
trap 'exit 130' INT TERM

echo "== avvio il server (sandbox e shmup) sulla porta $PORT"
# --ticks 0: senza, un server in ascolto si ferma dopo 50 s.
"$SRV" --listen "$PORT" --ticks 0 --seed "$SEED" --portals \
    --room sandbox --mods "$ROOT/mods" --world-file "$HERE/sandbox.world" \
        --portal-to @shmup/0 \
    --room shmup --mods "$ROOT/games/shmup/mods" --textures "$ROOT/games/shmup/textures" \
        --world-file "$HERE/shmup.world" --portal-to @sandbox/0 \
    > "$HERE/server.log" 2>&1 &
SERVER=$!
for _ in $(seq 1 300); do
    [ "$(grep -c "listening on UDP" "$HERE/server.log" 2>/dev/null)" -ge 2 ] && break
    if ! kill -0 "$SERVER" 2>/dev/null; then
        echo "shmup.sh: il server si è fermato:"; tail -20 "$HERE/server.log"; exit 1
    fi
    sleep 0.1
done
if [ "$(grep -c "listening on UDP" "$HERE/server.log")" -lt 2 ]; then
    echo "shmup.sh: le due stanze non ascoltano dopo 30 s:"; tail -20 "$HERE/server.log"; exit 1
fi

cat <<INFO

== Il cabinato

  Cammina nella prima apertura del sandbox: si arriva sul ponte di una
  portaerei, già caccia e già visti dall'alto. Spazio fa partire la partita e,
  tenuto, spara; W A S D muovono il caccia sullo schermo, che non scorre.
  Tre vite, ondate sempre più fitte, un bombardiere ogni cinque. E riporta
  sul ponte, e la porta in fondo al ponte riporta nel sandbox.
  Log: $HERE/server.log

INFO

echo "== entro come $NAME"
"$CLI" --connect 127.0.0.1 --port "$PORT" --name "$NAME" \
    --player-key "$HERE/player.key" ${CLIENT_ARGS[@]+"${CLIENT_ARGS[@]}"} || true
