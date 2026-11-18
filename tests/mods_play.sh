#!/bin/sh
# Every mod of the sandbox and of every game keeps running with a player in it.
#
# mods_smoke.sh starts each world with nobody in it, so a handler that only
# runs for a player -- on joining, on a chat line or a command, on moving, on
# a block broken -- is never reached, and a mod that errors there is switched
# off the first time somebody actually plays. This starts each world the same
# way, listening, and sends in a windowless client (cnp_join_probe) that says
# a line and asks for /help, breaks two blocks, stands a few seconds, walks a
# few blocks and leaves. Then the server is told to stop, and the test fails
# on any mod that did not load or was switched off, and on a probe that could
# not join.
#
# usage: mods_play.sh <server binary> <probe binary> <source dir>

set -u

SERVER="$1"
PROBE="$2"
ROOT="$3"
EXTENT=64
WORK="$(mktemp -d -t cnp_play.XXXXXX)"
SERVER_PID=""
# Worlds played at once. Each has its own directory, its own server and its
# own eight ports, so nothing is shared but the CPU.
JOBS="${MODS_PLAY_JOBS:-4}"
PORTS=$((26000 + ($$ % 50) * 160))   # 8 ports a world, 20 worlds

. "$(dirname "$0")/keep_logs.sh"

cleanup() {
    status=$?
    for job in $(jobs -p); do kill "$job" 2>/dev/null; done
    wait 2>/dev/null
    keep_logs mods_play "$status" "$WORK"
    rm -rf "$WORK"
}
trap cleanup EXIT INT TERM

# What is not a failure, as in mods_smoke.sh: a mod for clients, which a
# server offers and does not run, and a mod its mod.conf turns off.
EXPECTED='a mod for clients|disabled in mod.conf'

# Starts the server for one world, listening on a port nobody else is on;
# its log is $LOG. --ticks 0: it runs until it is told to stop.
start_server() {
    PORT=0
    base=$((PORTS + INDEX * 8))
    for candidate in $base $((base + 1)) $((base + 2)) $((base + 3)) $((base + 4)) \
                     $((base + 5)) $((base + 6)) $((base + 7)); do
        : > "$LOG"
        (cd "$DIR" && exec "$SERVER" --listen "$candidate" --ticks 0 --world "$EXTENT" \
                     --key-file "$DIR/server.key" --world-file "$DIR/world.world" "$@") \
            < /dev/null > "$LOG" 2>&1 &
        SERVER_PID=$!
        i=0
        while [ $i -lt 300 ]; do
            if grep -q "listening on UDP $candidate" "$LOG" 2>/dev/null; then
                PORT=$candidate
                return 0
            fi
            kill -0 "$SERVER_PID" 2>/dev/null || break   # exited: port busy?
            sleep 0.1
            i=$((i + 1))
        done
        kill "$SERVER_PID" 2>/dev/null
        wait "$SERVER_PID" 2>/dev/null
        SERVER_PID=""
    done
    return 1
}

fail() {
    echo "  FAIL  $1"
}

# One world, from start to finish, in a subshell of its own; what it says goes
# to $WORK/<name>/result, printed in order once every world is done.
check() {
    name="$1"
    shift
    DIR="$WORK/$name"
    LOG="$DIR/server.log"
    PLOG="$DIR/probe.log"
    mkdir -p "$DIR"

    trap '[ -n "$SERVER_PID" ] && kill "$SERVER_PID" 2>/dev/null; exit 1' TERM INT

    if ! start_server "$@"; then
        fail "$name: the server did not start listening"
        tail -10 "$LOG" | sed 's/^/        /'
        return
    fi

    timeout 60 "$PROBE" --join 127.0.0.1 "$PORT" giocatore "$EXTENT" \
             --say "ciao a tutti" --say "/help" --play --dig 2 --stay 2 --walk 12 > "$PLOG" 2>&1
    probe_status=$?

    # A moment for the leave to arrive and its handlers to run, then stop it
    # the way Ctrl+C does: it finishes the tick and leaves as a --ticks run.
    sleep 0.5
    if kill -0 "$SERVER_PID" 2>/dev/null; then
        kill -TERM "$SERVER_PID" 2>/dev/null
        wait "$SERVER_PID"
        server_status=$?
    else
        wait "$SERVER_PID"
        server_status=$?
        [ "$server_status" -eq 0 ] && server_status=99   # stopped by itself mid-run
    fi
    SERVER_PID=""

    bad=$(grep -E "^mod '[^']*' (not loaded|disabled|reload failed)" "$LOG" | grep -Ev "$EXPECTED")
    loaded=$(grep -c "^mod '[^']*' .* loaded, capabilities" "$LOG")
    failed=0
    if [ "$probe_status" -ne 0 ]; then
        fail "$name: the probe did not join and play (exit $probe_status)"
        sed 's/^/        probe: /' "$PLOG"
        failed=1
    fi
    if [ "$server_status" -ne 0 ]; then
        fail "$name: the server stopped with status $server_status"
        tail -5 "$LOG" | sed 's/^/        /'
        failed=1
    fi
    if [ -n "$bad" ]; then
        fail "$name:"
        echo "$bad" | sed 's/^/        /'
        failed=1
    fi
    if [ "$loaded" -eq 0 ]; then
        fail "$name: no mod loaded"
        failed=1
    fi
    [ "$failed" -eq 0 ] && echo "  ok    $name: $loaded mods, a player joined, played and left"
}

echo "every mod keeps running with a player in the world"

NAMES=""
INDEX=0
play() {
    name="$1"
    mkdir -p "$WORK/$name"
    check "$@" > "$WORK/$name/result" 2>&1 &
    NAMES="$NAMES $name"
    INDEX=$((INDEX + 1))
    # A batch at a time: the next starts when the last of this one is done.
    [ $((INDEX % JOBS)) -eq 0 ] && wait
}

if [ -n "${MODS_PLAY_ONLY:-}" ]; then
    # By hand: one directory of mods, e.g. MODS_PLAY_ONLY=games/track/mods
    # (under the source directory) or an absolute path.
    case "$MODS_PLAY_ONLY" in
        /*) play only --mods "$MODS_PLAY_ONLY" ;;
        *)  play only --mods "$ROOT/$MODS_PLAY_ONLY" ;;
    esac
else
    play sandbox --mods "$ROOT/mods"
    for dir in "$ROOT"/games/*/; do
        game=$(basename "$dir")
        [ -d "$dir/mods" ] || continue
        case "$game" in
            # tools/vehiclelab.sh: the lab on top of the sandbox's own mods.
            vehiclelab) play "$game" --user-mods "$dir/mods" ;;
            *)          play "$game" --mods "$dir/mods" ;;
        esac
    done
fi
wait

FAILURES=0
for name in $NAMES; do
    cat "$WORK/$name/result"
    FAILURES=$((FAILURES + $(grep -c "^  FAIL" "$WORK/$name/result")))
done

if [ "$FAILURES" -ne 0 ]; then
    echo "$FAILURES failure(s): a mod that did not load or was switched off, or a player who could not play"
    exit 1
fi
exit 0
