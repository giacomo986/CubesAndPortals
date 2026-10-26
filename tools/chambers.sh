#!/bin/bash
# Il sandbox (i mod di mods/) con un portale verso le camere di prova in stile
# puzzle a portali: un atrio, sei camere in fila (portali fissi, il cubo e il pulsante,
# la portal gun blu, quella blu e arancione, le piattaforme di salto, il
# pulsante a tempo) e una sala finale con il portale di ritorno.
#
#   tools/chambers.sh                 # un server con due stanze e un client nel sandbox
#   tools/chambers.sh --fresh         # butta via i due mondi salvati
#   tools/chambers.sh -- --shot 20    # quello dopo -- va al client
#
# Si nasce davanti alla prima apertura del sandbox: dentro si è nell'atrio.
# Tasto sinistro portale blu, destro arancione (o prende un cubo); con un cubo
# in mano il destro lo posa e il sinistro lo lancia. Mondi, chiavi e log stanno
# in worlds/chambers/ (ignorata da git).
set -eu

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BUILD="${BUILD:-$ROOT/build-release}"
SEED="${SEED:-1}"
PORT="${PORT:-25361}"
NAME="${NAME:-${USER:-giocatore}}"
HERE="${CNP_CHAMBERS_DIR:-$ROOT/worlds/chambers}"

FRESH=0
CLIENT_ARGS=()
while [ $# -gt 0 ]; do
    case "$1" in
        --fresh) FRESH=1 ;;
        --)      shift; CLIENT_ARGS=("$@"); break ;;
        -h|--help) sed -n '2,13p' "$0" | sed 's/^# \{0,1\}//'; exit 0 ;;
        *) echo "chambers.sh: non conosco '$1' (--fresh, -- ARGOMENTI DEL CLIENT)"; exit 2 ;;
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
    rm -f "$HERE"/sandbox.world* "$HERE"/chambers.world*
    echo "== mondi salvati cancellati"
fi

if ss -uln 2>/dev/null | grep -q ":$PORT "; then
    echo "chambers.sh: la porta UDP $PORT è già in uso; PORT=25371 tools/chambers.sh"
    exit 1
fi

SERVER=""
# SIGTERM fa salvare e chiudere: aspetta, così il file resta intero.
stop_server() {
    [ -n "$SERVER" ] || return 0
    kill -0 "$SERVER" 2>/dev/null || return 0
    kill -TERM "$SERVER" 2>/dev/null || true
    wait "$SERVER" 2>/dev/null || true
}
trap stop_server EXIT
trap 'exit 130' INT TERM

echo "== avvio il server (sandbox e camere di prova) sulla porta $PORT"
# --ticks 0: senza, un server in ascolto si ferma dopo 50 s.
"$SRV" --listen "$PORT" --ticks 0 --seed "$SEED" --portals \
    --room sandbox --mods "$ROOT/mods" --world-file "$HERE/sandbox.world" \
        --portal-to @chambers/0 --portal-to @chambers/1 \
    --room chambers --mods "$ROOT/games/chambers/mods" --textures "$ROOT/games/chambers/textures" \
        --world-file "$HERE/chambers.world" --portal-to @sandbox/0 --portal-to @sandbox/1 \
    > "$HERE/server.log" 2>&1 &
SERVER=$!
for _ in $(seq 1 300); do
    [ "$(grep -c "listening on UDP" "$HERE/server.log" 2>/dev/null)" -ge 2 ] && break
    if ! kill -0 "$SERVER" 2>/dev/null; then
        echo "chambers.sh: il server si è fermato:"; tail -20 "$HERE/server.log"; exit 1
    fi
    sleep 0.1
done
if [ "$(grep -c "listening on UDP" "$HERE/server.log")" -lt 2 ]; then
    echo "chambers.sh: le due stanze non ascoltano dopo 30 s:"; tail -20 "$HERE/server.log"; exit 1
fi

echo "== entro come $NAME (log: $HERE/server.log)"
"$CLI" --connect 127.0.0.1 --port "$PORT" --name "$NAME" \
    --player-key "$HERE/player.key" ${CLIENT_ARGS[@]+"${CLIENT_ARGS[@]}"} || true
