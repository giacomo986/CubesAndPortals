#!/bin/bash
# Il mondo di partenza (i mod di mods/) con un portale per ogni mondo a tema --
# pista kart, inferno, arena, rovine, camere di prova, backrooms e parkour -- ognuno in una struttura
# (e il laboratorio, che si apre solo con le sette chiavi, mods/keys)
# a tema sulla piazza (mods/gates), e in ognuno un portale per tornare.
#
#   tools/track.sh                # un server con undici stanze e un client nel sandbox
#   tools/track.sh --fresh        # butta via i mondi salvati; l'assalto con 100000 crediti
#   tools/track.sh --survival     # niente piazza, spawn sulla terra, portali sparsi (worlds/track-survival/)
#   tools/track.sh -- --shot 20   # quello dopo -- va al client
#
# Le aperture del sandbox: 0 (40.5 ... 44.5, guarda +Z) la pista, 1 (60.5 ...
# 60.5, guarda +X) l'inferno, 2 l'arena, 3 le rovine, 4 le camere di prova, 5 le backrooms, 6 il
# parkour (le ultime cinque messe sulla piazza da mods/gates), 7 il laboratorio; 8-14 gli archi
# delle uscite sul bordo nord della piazza (mods/keys), sigillati in rosso a chi non ha la
# chiave di quel mondo; 15 il fronte (90.5 ... 58.5, mods/gates), andata e ritorno; 16 l'assalto
# (90.5 ... 76.5), il cui mondo non si salva; 17-23 una seconda entrata per
# ognuno dei sette mondi con chiave (survival: due siti dello stesso mondo insieme). Il client guarda dentro solo le
# due più vicine. La pista dell'inferno va
# dalla pista all'inferno e ritorno, tre bocche per stanza: la cornice A della
# pista (apertura 1) e B dell'inferno (apertura 2) si portano l'una all'altra, e
# così C dell'inferno (apertura 1) e D della pista (apertura 2). Mondi, chiavi e
# log stanno in worlds/track/ (ignorata da git).
set -eu

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BUILD="${BUILD:-$ROOT/build-release}"
SEED="${SEED:-1}"
PORT="${PORT:-25301}"
NAME="${NAME:-${USER:-giocatore}}"
HERE="${CNP_TRACK_DIR:-$ROOT/worlds/track}"

FRESH=0
SURVIVAL=()
CLIENT_ARGS=()
while [ $# -gt 0 ]; do
    case "$1" in
        --fresh) FRESH=1 ;;
        --survival) SURVIVAL=(--survival); HERE="${CNP_TRACK_DIR:-$ROOT/worlds/track-survival}" ;;
        --)      shift; CLIENT_ARGS=("$@"); break ;;
        -h|--help) sed -n '2,18p' "$0" | sed 's/^# \{0,1\}//'; exit 0 ;;
        *) echo "track.sh: non conosco '$1' (--fresh, --survival, -- ARGOMENTI DEL CLIENT)"; exit 2 ;;
    esac
    shift
done

# Release: in Debug una colonna ci mette ~30 ms a illuminarsi, e il kart corre di più.
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
    rm -f "$HERE"/sandbox.world* "$HERE"/pista.world* "$HERE"/inferno.world* \
          "$HERE"/arena.world* "$HERE"/arpg.world* "$HERE"/chambers.world* "$HERE"/backrooms.world* "$HERE"/parkour.world* "$HERE"/laboratorio.world* "$HERE"/front.world*
    echo "== mondi salvati cancellati"
fi

# L'assalto: con --fresh anche tools/assault_test, 100000 crediti per provare
# il negozio; i suoi mod accanto, in collegamenti simbolici.
ASSAULT_MODS="$ROOT/games/assault/mods"
if [ "$FRESH" = 1 ]; then
    ASSAULT_MODS="$HERE/assault_mods"
    rm -rf "$ASSAULT_MODS"
    mkdir -p "$ASSAULT_MODS"
    for m in "$ROOT"/games/assault/mods/*/; do ln -s "${m%/}" "$ASSAULT_MODS/"; done
    ln -s "$ROOT/tools/assault_test" "$ASSAULT_MODS/assault_test"
    echo "== assalto con 100000 crediti di prova"
fi

if ss -uln 2>/dev/null | grep -q ":$PORT "; then
    echo "track.sh: la porta UDP $PORT è già in uso; PORT=25311 tools/track.sh"
    exit 1
fi

# Un server solo con due stanze (protocollo 56): il sandbox e la pista, sulla
# stessa porta. Ogni bocca dice in quale stanza porta e da quale apertura di là
# si esce: /0 sulla pista è la piazzola, /0 qui è l'apertura davanti a cui si
# nasce. Borsa e vita passano da una stanza all'altra; chiavi e ticket tra le
# stanze li fa il server da sé.
SERVER=""
# SIGTERM fa salvare e chiudere: aspetta, così i due file restano interi.
stop_server() {
    [ -n "$SERVER" ] || return 0
    kill -0 "$SERVER" 2>/dev/null || return 0
    kill -TERM "$SERVER" 2>/dev/null || true
    wait "$SERVER" 2>/dev/null || true
}
trap stop_server EXIT
trap 'exit 130' INT TERM

echo "== avvio il server (sandbox, pista, inferno, arena, arpg, camere di prova, backrooms, parkour, laboratorio, fronte e assalto) sulla porta $PORT"
# --ticks 0: senza, un server in ascolto si ferma dopo 50 s.
"$SRV" --listen "$PORT" --ticks 0 --seed "$SEED" --portals \
    --room sandbox ${SURVIVAL[@]+"${SURVIVAL[@]}"} --mods "$ROOT/mods" --world-file "$HERE/sandbox.world" \
        --portal-to @pista/0 --portal-to @inferno/0 --portal-to @arena/0 --portal-to @arpg/0 \
        --portal-to @chambers/0 --portal-to @backrooms/0 --portal-to @parkour/0 \
        --portal-to @laboratorio/0 \
        --portal-to @pista/3 --portal-to @inferno/3 --portal-to @arena/1 --portal-to @arpg/1 \
        --portal-to @chambers/1 --portal-to @backrooms/1 --portal-to @parkour/1 \
        --portal-to @front/0 --portal-to @assalto/0 \
        --portal-to @pista/0 --portal-to @inferno/0 --portal-to @arena/0 --portal-to @arpg/0 \
        --portal-to @chambers/0 --portal-to @backrooms/0 --portal-to @parkour/0 \
    --room pista --mods "$ROOT/games/track/mods" --textures "$ROOT/games/track/textures" \
        --world-file "$HERE/pista.world" --portal-to @sandbox/0 --portal-to @inferno/2 --portal-to @inferno/1 \
        --portal-to @sandbox/8 \
    --room inferno --mods "$ROOT/games/inferno/mods" --textures "$ROOT/games/inferno/textures" --fallback-textures "$ROOT/assets/textures" \
        --world-file "$HERE/inferno.world" --portal-to @sandbox/1 --portal-to @pista/2 --portal-to @pista/1 \
        --portal-to @sandbox/9 \
    --room arena --mods "$ROOT/games/yafps/mods" --textures "$ROOT/games/yafps/textures" \
        --world-file "$HERE/arena.world" --portal-to @sandbox/2 --portal-to @sandbox/10 \
    --room arpg --mods "$ROOT/games/arpg/mods" \
        --world-file "$HERE/arpg.world" --portal-to @sandbox/3 --portal-to @sandbox/11 \
    --room chambers --mods "$ROOT/games/chambers/mods" --textures "$ROOT/games/chambers/textures" \
        --world-file "$HERE/chambers.world" --portal-to @sandbox/4 --portal-to @sandbox/12 \
    --room backrooms --mods "$ROOT/games/backrooms/mods" --textures "$ROOT/games/backrooms/textures" \
        --world-file "$HERE/backrooms.world" --portal-to @sandbox/5 --portal-to @sandbox/13 \
    --room parkour --mods "$ROOT/games/parkour/mods" --textures "$ROOT/games/parkour/textures" \
        --world-file "$HERE/parkour.world" --portal-to @sandbox/6 --portal-to @sandbox/14 \
    --room laboratorio --mods "$ROOT/games/lab/mods" --textures "$ROOT/games/lab/textures" \
        --world-file "$HERE/laboratorio.world" --portal-to @sandbox/7 --portal-to @sandbox/7 \
    --room front --mods "$ROOT/games/front/mods" \
        --world-file "$HERE/front.world" --portal-to @sandbox/15 \
    --room assalto --mods "$ASSAULT_MODS" \
        --portal-to @sandbox/16 --portal-to @sandbox/16 \
    > "$HERE/server.log" 2>&1 &
SERVER=$!
for _ in $(seq 1 300); do
    [ "$(grep -c "listening on UDP" "$HERE/server.log" 2>/dev/null)" -ge 11 ] && break
    if ! kill -0 "$SERVER" 2>/dev/null; then
        echo "track.sh: il server si è fermato:"; tail -20 "$HERE/server.log"; exit 1
    fi
    sleep 0.1
done
if [ "$(grep -c "listening on UDP" "$HERE/server.log")" -lt 11 ]; then
    echo "track.sh: le dieci stanze non ascoltano dopo 30 s:"; tail -20 "$HERE/server.log"; exit 1
fi

grep -h "leads to room" "$HERE/server.log" | sed 's/^ */   /'

cat <<INFO

== Come si arriva alla pista

  Si nasce davanti alla prima apertura del sandbox: camminaci dentro e sei
  a piedi sulla piazzola della pista. Tasto destro su un kart per salire (e
  un altro per scendere); WASD guida, la telecamera segue da dietro. Il
  portale della piazzola riporta qui.

== E all'inferno

  La seconda apertura, a 60.5 21 60.5 nella piazza: si esce in una camera di
  scoria con la cornice di ossidiana, fra caverne, mari di lava (bruciano),
  glowrock e ponti di mattoni. Borsa e vita vengono con te; lo stesso portale
  riporta nella piazza.

== E agli altri mondi a tema

  Sparsi sulla piazza, ognuno nella sua struttura (mods/gates): l'arena in
  in stile arena shooter dietro una porta di una base con le strisce gialle e nere
  (28.5 21 64.5), le rovine in stile action RPG isometrico sotto un arco di mattoni fra le
  tombe (78.5 21 34.5), le camere di prova in stile puzzle a portali in un muro di
  pannelli bianchi (84.5 21 84.5), le backrooms in un muro di carta da parati
  gialla (16.5 21 28.5), il percorso di parkour sotto un'impalcatura
  arancione e nera (16.5 21 52.5), il fronte in una porta di bunker fra sacchi
  di sabbia e due bandiere (90.5 21 58.5). Si vede dentro solo i due portali
  più vicini.
  Log: $HERE/server.log

INFO

echo "== entro come $NAME"
# What the client says goes to the terminal and to client.log beside the
# server's, for when something only went wrong on this side.
stdbuf -oL "$CLI" --connect 127.0.0.1 --port "$PORT" --name "$NAME" \
    --player-key "$HERE/player.key" ${CLIENT_ARGS[@]+"${CLIENT_ARGS[@]}"} 2>&1 | tee "$HERE/client.log" || true
