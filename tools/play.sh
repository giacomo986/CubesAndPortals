#!/bin/bash
# Build, start and join the sandbox with everything in mods/, to walk through
# what the world has now: no edge, trees, lakes to swim in, ruined towers, joined
# caves with ore in them, and a furnace.
#
#   tools/play.sh                 # a server with a world file, and a client in it
#   tools/play.sh local           # one process, no server, nothing saved
#   tools/play.sh --fresh         # throw the saved world away first
#   SEED=7 NAME=me tools/play.sh -- --textures assets/packs/kenney
#
# Anything after -- goes to the client. The world, the keys and the server log
# live in worlds/play/ (ignored by git). The coordinates it prints were measured
# for seed 1; another seed has the same things somewhere else.
set -eu

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BUILD="${BUILD:-$ROOT/build-release}"
SEED="${SEED:-1}"
PORT="${PORT:-25301}"
NAME="${NAME:-${USER:-giocatore}}"
HERE="${CNP_PLAY_DIR:-$ROOT/worlds/play}"

MODE=server
FRESH=0
CLIENT_ARGS=()
while [ $# -gt 0 ]; do
    case "$1" in
        local)   MODE=local ;;
        server)  MODE=server ;;
        --fresh) FRESH=1 ;;
        --)      shift; CLIENT_ARGS=("$@"); break ;;
        -h|--help) sed -n '2,13p' "$0" | sed 's/^# \{0,1\}//'; exit 0 ;;
        *) echo "play.sh: non conosco '$1' (local, server, --fresh, -- ARGOMENTI DEL CLIENT)"; exit 2 ;;
    esac
    shift
done

# Release: in Debug a column takes ~30 ms to light, and flying outruns it.
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
    rm -f "$HERE"/mondo.world "$HERE"/mondo.world-shm "$HERE"/mondo.world-wal
    echo "== mondo salvato cancellato"
fi

cat <<EOF

== Cosa provare (seme $SEED; le coordinate valgono per il seme 1, F3 mostra dove sei)

  Si nasce nella piazza, verso 64 20 64. F vola, Shift corre, E borsa.

  0. Creativo   Tasto C (o destro sulla scatola verde a 42 20 47): ogni item del mondo, 21 per
                pagina, un click una pila piena. Da lì escono anche il secchio e l'acqua.
  1. Attrezzi   Tasto destro sul banco a 37 47: piccone e pala. Il ferro vuole il piccone.
                Per farne altri servono i bastoni: 1 tronco = 4 tavole (griglia 2x2),
                2 tavole in colonna = 4 bastoni. Poi, sul tavolo 3x3: 3 pietra in alto
                e 2 bastoni sotto al centro = piccone; 1 pietra = pala; 2+1 pietra a L
                = ascia (taglia il legno).
  2. Alberi     Nella pianura appena fuori dalla piazza. I tronchi sono l'inizio di
                ogni attrezzo.
  3. Lago       A 86 140, profondo tre o più. Dentro si va a metà passo; tieni
                Spazio per risalire. Il più profondo vicino è a 40 -104 (9 blocchi).
                Scava un buco sulla riva o sotto il fondo: l'acqua ci entra da sola,
                una cella ogni mezzo secondo, e si ferma sei celle dopo la sorgente.
                Chiudi il passaggio e il canale si asciuga. Con 3 lingotti di ferro
                a V si fa il secchio: tasto destro sull'acqua lo riempie, tasto
                destro altrove la versa (e quella sorgente ricomincia a scorrere).
  4. Torre      A 152 180: mattoni, muschio, una lampada in cima. Scavala per i mattoni.
                Dentro, nell'angolo, una cassa con carbone e lingotti (a volte un
                piccone di ferro): tasto destro a mani vuote.
  5. Grotte     Scava giù fuori dalla piazza: le gallerie proseguono da un quadrato
                all'altro. Carbone (nero) e ferro (arancio) nelle pareti.
  6. Fornace    4 pietra = 4 mattoni (griglia 2x2 della borsa). Tavolo a 39 20 47
                (o 4 mattoni) per il 3x3: 8 mattoni ad anello = fornace. Tasto destro,
                dentro carbone e ferro o sabbia: un lingotto ogni 2 s, e si accende.
                3 lingotti sopra, 2 bastoni sotto = piccone di ferro.
  7. Senza bordo Vola dritto per qualche centinaio di blocchi: il mondo si genera
                davanti e dietro si libera (la riga "streaming" del client conta i
                chunk lasciati).
  8. Portali    8 core:frame attorno a un lingotto di ferro = 8 cornici blu (o tasto C).
                Una cornice attorno a un buco d'aria, in piedi (dentro almeno 1x2) o
                nel pavimento, si apre con l'ultimo blocco e porta all'ultima rimasta
                sola. Tasto destro sulla cornice: rinominarla (clic sul nome, Invio),
                girare il davanti, ruotare, chiudere, collegarla a un'altra.
EOF

if [ "$MODE" = local ]; then
    cat <<EOF
  9. Un processo solo: senza world file, all'uscita il mondo non resta. Le cornici
                qui non si aprono: le due aperture sono della scena di prova.

EOF
    cd "$HERE"
    exec "$CLI" --quick --mods "$ROOT/mods" --seed "$SEED" ${CLIENT_ARGS[@]+"${CLIENT_ARGS[@]}"}
fi

cat <<EOF
  9. Salvataggio Esci, rilancia lo stesso comando: ritrovi quello che hai cambiato.
                --fresh riparte da capo. Log del server: $HERE/server.log

EOF

if ss -uln 2>/dev/null | grep -q ":$PORT "; then
    echo "play.sh: la porta UDP $PORT è già in uso; PORT=25311 tools/play.sh"
    exit 1
fi

cd "$HERE"
# --ticks 0: without it a listening server stops after 50 s.
"$SRV" --listen "$PORT" --ticks 0 --seed "$SEED" --mods "$ROOT/mods" \
    --world-file "$HERE/mondo.world" --key-file "$HERE/server.key" > "$HERE/server.log" 2>&1 &
SERVER=$!

# SIGTERM makes the server save and close; wait so the file is whole on exit.
stop_server() {
    if kill -0 "$SERVER" 2>/dev/null; then
        echo "== chiudo il server (salva)"
        kill -TERM "$SERVER" 2>/dev/null || true
        wait "$SERVER" 2>/dev/null || true
        grep -E "told to stop|could not save|columns: " "$HERE/server.log" | sed 's/^ */   /' || true
    fi
}
trap stop_server EXIT
trap 'exit 130' INT TERM

echo "== avvio il server sulla porta $PORT"
for _ in $(seq 1 300); do
    if grep -q "listening on UDP" "$HERE/server.log" 2>/dev/null; then
        break
    fi
    if ! kill -0 "$SERVER" 2>/dev/null; then
        # Il caso che capita davvero: un mod ha aggiunto un blocco, quindi la
        # tabella dei blocchi è cambiata e il mondo salvato prima non si può
        # rileggere (gli id delle celle vorrebbero dire altro). Lo dice il
        # server, ma in mezzo a venti righe di mod caricati.
        if grep -q "different block table" "$HERE/server.log"; then
            echo "play.sh: il mondo salvato è di prima che un mod aggiungesse dei blocchi;"
            echo "         gli id nelle celle vorrebbero dire altro, e il server non lo apre:"
            grep "different block table" "$HERE/server.log" | sed 's/^/         /'
            echo "         riparti da capo con:  tools/play.sh --fresh"
            exit 1
        fi
        echo "play.sh: il server si è fermato:"
        tail -20 "$HERE/server.log"
        exit 1
    fi
    sleep 0.1
done
if ! grep -q "listening on UDP" "$HERE/server.log"; then
    echo "play.sh: il server non ascolta dopo 30 s:"
    tail -20 "$HERE/server.log"
    exit 1
fi
grep -E "mods running|seed [0-9]+" "$HERE/server.log" | sed 's/^ */   /' | head -2

echo "== entro come $NAME"
"$CLI" --connect 127.0.0.1 --port "$PORT" --name "$NAME" \
    --player-key "$HERE/player.key" ${CLIENT_ARGS[@]+"${CLIENT_ARGS[@]}"} || true
