#!/bin/bash
# Il sandbox (i mod di mods/) con un portale verso l'arena yafps, un'arena in
# in stile arena shooter moderno: pistola, fucile a pompa, mitragliatrice e plasma cannon, e
# ondate di demoni che escono dai portali nel pavimento.
#
#   tools/fps.sh                 # un server con due stanze e un client nel sandbox
#   tools/fps.sh --fresh         # butta via i due mondi salvati
#   tools/fps.sh -- --shot 20    # quello dopo -- va al client
#
# Si nasce davanti alla prima apertura del sandbox (40.5 ... 44.5, guarda +Z):
# dentro si è nell'arena, dietro l'ultima fila di demoni. La stessa bocca
# riporta indietro. Mondi, chiavi e log stanno in worlds/fps/ (ignorata da git).
set -eu

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BUILD="${BUILD:-$ROOT/build-release}"
SEED="${SEED:-1}"
PORT="${PORT:-25321}"
NAME="${NAME:-${USER:-giocatore}}"
HERE="${CNP_FPS_DIR:-$ROOT/worlds/fps}"

FRESH=0
CLIENT_ARGS=()
while [ $# -gt 0 ]; do
    case "$1" in
        --fresh) FRESH=1 ;;
        --)      shift; CLIENT_ARGS=("$@"); break ;;
        -h|--help) sed -n '2,12p' "$0" | sed 's/^# \{0,1\}//'; exit 0 ;;
        *) echo "fps.sh: non conosco '$1' (--fresh, -- ARGOMENTI DEL CLIENT)"; exit 2 ;;
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
    rm -f "$HERE"/sandbox.world* "$HERE"/arena.world*
    echo "== mondi salvati cancellati"
fi

if ss -uln 2>/dev/null | grep -q ":$PORT "; then
    echo "fps.sh: la porta UDP $PORT è già in uso; PORT=25331 tools/fps.sh"
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

echo "== avvio il server (sandbox e arena) sulla porta $PORT"
# --ticks 0: senza, un server in ascolto si ferma dopo 50 s.
"$SRV" --listen "$PORT" --ticks 0 --seed "$SEED" --portals \
    --room sandbox --mods "$ROOT/mods" --world-file "$HERE/sandbox.world" \
        --portal-to @arena/0 --portal-to @arena/1 \
    --room arena --mods "$ROOT/games/yafps/mods" --textures "$ROOT/games/yafps/textures" \
        --world-file "$HERE/arena.world" --portal-to @sandbox/0 --portal-to @sandbox/1 \
    > "$HERE/server.log" 2>&1 &
SERVER=$!
for _ in $(seq 1 300); do
    [ "$(grep -c "listening on UDP" "$HERE/server.log" 2>/dev/null)" -ge 2 ] && break
    if ! kill -0 "$SERVER" 2>/dev/null; then
        echo "fps.sh: il server si è fermato:"; tail -20 "$HERE/server.log"; exit 1
    fi
    sleep 0.1
done
if [ "$(grep -c "listening on UDP" "$HERE/server.log")" -lt 2 ]; then
    echo "fps.sh: le due stanze non ascoltano dopo 30 s:"; tail -20 "$HERE/server.log"; exit 1
fi

cat <<INFO

== La base

  Cammina nella prima apertura del sandbox: si arriva nella stanza d'ingresso,
  a nord c'è l'hub, e da lì la fonderia (nord, passerelle e un ponte a 5), il
  reattore (est, una scala a spirale fino a 10) e l'armeria (ovest). Tasto
  sinistro spara (tenuto, di continuo), 1-9 o la rotella cambiano arma:
  pistola (infinita), fucile a pompa, e dalle ondate la doppietta (sul ponte
  della fonderia, dalla 1), la mitragliatrice (hub, dalla 2) e la plasma cannon
  (in cima alla spirale, dalla 4). Munizioni e medikit tornano a ogni ondata;
  ogni uccisione lascia una sfera blu di salute. Log: $HERE/server.log

INFO

echo "== entro come $NAME"
"$CLI" --connect 127.0.0.1 --port "$PORT" --name "$NAME" \
    --player-key "$HERE/player.key" ${CLIENT_ARGS[@]+"${CLIENT_ARGS[@]}"} || true
