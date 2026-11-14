#!/bin/bash
# Il sandbox (i mod di mods/) con un portale verso il mondo platform: un gioco
# a piattaforme visto di lato, come quelli degli anni ottanta. Una stanza con
# una vetrata sul livello, un tubo verde per entrarci, e da lì la telecamera
# si mette di lato: A e D camminano, W o spazio salta, S si accuccia, Shift
# corre, E torna alla stanza.
#
#   tools/platformer.sh                 # un server con due stanze e un client nel sandbox
#   tools/platformer.sh --fresh         # butta via i due mondi salvati
#   tools/platformer.sh -- --shot 20    # quello dopo -- va al client
#   tools/platformer.sh -- --room platformer   # si nasce già nella stanza del livello
#
# Si nasce davanti alla prima apertura del sandbox: dentro si è nella stanza,
# davanti alla vetrata. Mondi, chiavi e log stanno in worlds/platformer/
# (ignorata da git).
set -eu

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BUILD="${BUILD:-$ROOT/build-release}"
SEED="${SEED:-1}"
PORT="${PORT:-25361}"
NAME="${NAME:-${USER:-giocatore}}"
HERE="${CNP_PLATFORMER_DIR:-$ROOT/worlds/platformer}"

FRESH=0
CLIENT_ARGS=()
while [ $# -gt 0 ]; do
    case "$1" in
        --fresh) FRESH=1 ;;
        --)      shift; CLIENT_ARGS=("$@"); break ;;
        -h|--help) sed -n '2,14p' "$0" | sed 's/^# \{0,1\}//'; exit 0 ;;
        *) echo "platformer.sh: non conosco '$1' (--fresh, -- ARGOMENTI DEL CLIENT)"; exit 2 ;;
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
    rm -f "$HERE"/sandbox.world* "$HERE"/platformer.world*
    echo "== mondi salvati cancellati"
fi

if ss -uln 2>/dev/null | grep -q ":$PORT "; then
    echo "platformer.sh: la porta UDP $PORT è già in uso; PORT=25371 tools/platformer.sh"
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

echo "== avvio il server (sandbox e platformer) sulla porta $PORT"
# --ticks 0: senza, un server in ascolto si ferma dopo 50 s.
"$SRV" --listen "$PORT" --ticks 0 --seed "$SEED" --portals \
    --room sandbox --mods "$ROOT/mods" --world-file "$HERE/sandbox.world" \
        --portal-to @platformer/0 \
    --room platformer --mods "$ROOT/games/platformer/mods" --textures "$ROOT/games/platformer/textures" \
        --world-file "$HERE/platformer.world" --portal-to @sandbox/0 \
    > "$HERE/server.log" 2>&1 &
SERVER=$!
for _ in $(seq 1 300); do
    [ "$(grep -c "listening on UDP" "$HERE/server.log" 2>/dev/null)" -ge 2 ] && break
    if ! kill -0 "$SERVER" 2>/dev/null; then
        echo "platformer.sh: il server si è fermato:"; tail -20 "$HERE/server.log"; exit 1
    fi
    sleep 0.1
done
if [ "$(grep -c "listening on UDP" "$HERE/server.log")" -lt 2 ]; then
    echo "platformer.sh: le due stanze non ascoltano dopo 30 s:"; tail -20 "$HERE/server.log"; exit 1
fi

cat <<INFO

== Il livello 1-1

  Cammina nella prima apertura del sandbox: si arriva in una stanza con una
  vetrata, e oltre il vetro c'è il livello visto di lato. La porta a sinistra
  dà su un ponte e su un tubo verde: entra nel tubo. Nel livello A e D
  camminano, W o spazio salta, S si accuccia, Shift corre, E torna alla
  stanza. Tre vite; saltare su un maiale lo toglie di mezzo, toccarlo o
  cadere costa una vita; la bandiera blu a metà è il punto da cui si riparte,
  il palo in fondo ferma il tempo. Log: $HERE/server.log

INFO

echo "== entro come $NAME"
"$CLI" --connect 127.0.0.1 --port "$PORT" --name "$NAME" \
    --player-key "$HERE/player.key" ${CLIENT_ARGS[@]+"${CLIENT_ARGS[@]}"} || true
