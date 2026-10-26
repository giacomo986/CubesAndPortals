#!/bin/sh
# Two processes, one socket: the real server binary, and a client that joins it.
#
# This exists because the in-process loopback test could not have caught the bug
# that made it necessary. There, both ends were in one process and the "server"
# was a stub whose world hash came from the same helper the client used to
# check it -- so the check agreed with itself no matter what the real server
# would have sent. The real server was advertising a hash taken after the mods
# had run, which no client can reproduce from a seed, and every join reported a
# desync that did not exist.
#
# The lesson is the one the portal continuity gate already paid for: a reference
# produced by the thing under test is not a reference. So this runs the actual
# `cnp_server`, from its actual main(), with its actual mods.
#
# usage: two_process_join.sh <server binary> <client binary>

set -u

SERVER="$1"
CLIENT="$2"
EXTENT=64
TICKS=400          # 20 seconds at 20 Hz, then the server exits by itself
LOG="$(mktemp -t cnp_server_log.XXXXXX)"
# Its own identity key, in a temporary file. Not the default next to the world,
# because two tests running at once in one build directory would then be two
# servers fighting over one key file, and because a test should not leave a
# server identity lying about afterwards.
KEYFILE="$(mktemp -u -t cnp_server_key.XXXXXX)"
FAILURES=0
SERVER_PID=""

. "$(dirname "$0")/keep_logs.sh"

cleanup() {
    status=$?
    if [ -n "${SERVER_PID:-}" ]; then
        kill "$SERVER_PID" 2>/dev/null
        wait "$SERVER_PID" 2>/dev/null
    fi
    keep_logs two_process_join "$status" "$LOG"
    rm -f "$LOG" "$KEYFILE"
}
trap cleanup EXIT INT TERM

# Starts the server with whatever extra arguments are given, on a port nobody
# else is on. Not a fixed port: a busy one would fail this test for a reason
# that has nothing to do with the code.
start_server() {
    PORT=0
    for candidate in 25301 25302 25303 25304 25305 25306 25307 25308; do
        "$SERVER" --listen "$candidate" --ticks "$TICKS" --world "$EXTENT" \
                  --key-file "$KEYFILE" "$@" > "$LOG" 2>&1 &
        SERVER_PID=$!

        i=0
        while [ $i -lt 100 ]; do
            if grep -q "listening on UDP $candidate" "$LOG" 2>/dev/null; then
                PORT=$candidate
                return 0
            fi
            if ! kill -0 "$SERVER_PID" 2>/dev/null; then
                break            # it exited: port busy, try the next one
            fi
            sleep 0.1
            i=$((i + 1))
        done

        kill "$SERVER_PID" 2>/dev/null
        wait "$SERVER_PID" 2>/dev/null
        SERVER_PID=""
    done
    return 1
}

stop_server() {
    [ -n "$SERVER_PID" ] || return 0
    kill "$SERVER_PID" 2>/dev/null
    wait "$SERVER_PID" 2>/dev/null
    SERVER_PID=""
}

# ---------------------------------------------------------------------------
# 1. A server whose block table the probe shares.
# ---------------------------------------------------------------------------
# --no-mods, and that is not a convenience: the probe has the engine's four core
# nodes and no mods, so against a modded server its block table genuinely is a
# different table, and being refused is the correct answer. That case is part 2.

echo "a server with the same blocks as the client"

if ! start_server --no-mods; then
    echo "  FAIL  could not start a server on any port between 25301 and 25308"
    cat "$LOG"
    exit 1
fi
echo "  ok    the server binary is listening on UDP $PORT"

for name in giacomo secondo; do
    if "$CLIENT" --join 127.0.0.1 "$PORT" "$name" "$EXTENT"; then
        echo "  ok    '$name' joined and generated the same world from the seed"
    else
        echo "  FAIL  '$name' could not join, or generated a different world"
        FAILURES=$((FAILURES + 1))
    fi
done

# A client that claims a different block table and cannot adopt one, against
# the same server. One build tree cannot compile two versions of itself, so the
# client pretends -- which is enough, because the number is all the server sees.
# (Claiming a different table while *being* able to adopt is not an error; that
# is the case part 2 covers.)
"$CLIENT" --join 127.0.0.1 "$PORT" "terzo" "$EXTENT" --wrong-table --no-adopt
case $? in
    2) echo "  ok    a client claiming a different block table is refused by the server" ;;
    0) echo "  FAIL  a client claiming a different block table was let in"
       FAILURES=$((FAILURES + 1)) ;;
    3) echo "  FAIL  only the client's own check caught it; the server admitted it"
       FAILURES=$((FAILURES + 1)) ;;
    *) echo "  FAIL  the join failed for some other reason"
       FAILURES=$((FAILURES + 1)) ;;
esac

if kill -0 "$SERVER_PID" 2>/dev/null; then
    echo "  ok    the server is still running after all three"
else
    echo "  FAIL  the server died during the joins"
    FAILURES=$((FAILURES + 1))
fi

# ---------------------------------------------------------------------------
# The encryption, against the real binaries rather than against two objects in
# one process. crypto_test decides what a tampered frame does; this decides
# whether the two programs actually negotiate a key over a socket, and whether a
# pin is something a player can really use -- the server prints a key and the
# client is handed exactly that string.
# ---------------------------------------------------------------------------

SERVER_KEY="$(sed -n 's/.*--server-key \([0-9a-f]*\).*/\1/p' "$LOG" | head -1)"
if [ -n "$SERVER_KEY" ]; then
    echo "  ok    the server printed an identity key for clients to pin"
else
    echo "  FAIL  the server printed no identity key"
    FAILURES=$((FAILURES + 1))
fi

if "$CLIENT" --join 127.0.0.1 "$PORT" "fidato" "$EXTENT" --server-key "$SERVER_KEY"; then
    echo "  ok    a client pinning that key joins"
else
    echo "  FAIL  a client pinning the key the server printed could not join"
    FAILURES=$((FAILURES + 1))
fi

# The impostor case, from the client's side: the same server, a key that is one
# bit different. Nothing about the server changed, so a green result here would
# mean the pin is not being checked at all.
WRONG_KEY="$(printf '%s' "$SERVER_KEY" | sed 's/^./0/')"
if [ "$WRONG_KEY" = "$SERVER_KEY" ]; then WRONG_KEY="$(printf '%s' "$SERVER_KEY" | sed 's/^./1/')"; fi
"$CLIENT" --join 127.0.0.1 "$PORT" "diffidente" "$EXTENT" --server-key "$WRONG_KEY"
case $? in
    3) echo "  ok    a client pinning a different key refuses the connection" ;;
    0) echo "  FAIL  a client pinning the wrong key joined anyway"
       FAILURES=$((FAILURES + 1)) ;;
    *) echo "  FAIL  pinning the wrong key failed for some other reason"
       FAILURES=$((FAILURES + 1)) ;;
esac

# And a client that does not encrypt at all, against a server that does. Its
# first packet is a hello where a key offer belongs, and there is nothing to do
# with it but refuse.
"$CLIENT" --join 127.0.0.1 "$PORT" "inchiaro" "$EXTENT" --no-encryption
if [ $? -ne 0 ]; then
    echo "  ok    a client with encryption off cannot join an encrypted server"
else
    echo "  FAIL  a client with encryption off joined an encrypted server"
    FAILURES=$((FAILURES + 1))
fi

echo "server said:"
sed 's/^/        /' "$LOG"
stop_server

# ---------------------------------------------------------------------------
# 2. A server running four mods, which is twenty-six blocks the probe has not
#    got. A client that can adopt the server's table joins anyway; one that
#    cannot is refused, because it would read every chunk through its own ids.
# ---------------------------------------------------------------------------

echo ""
echo "a server whose mods the client does not have"

if ! start_server; then
    echo "  FAIL  could not start the modded server"
    cat "$LOG"
    exit 1
fi
echo "  ok    the modded server is listening on UDP $PORT"

if "$CLIENT" --join 127.0.0.1 "$PORT" "quarto" "$EXTENT"; then
    echo "  ok    a client that adopts the server's table joins it"
else
    echo "  FAIL  a client that adopts the table could not join"
    FAILURES=$((FAILURES + 1))
fi

# The same server and an older client that does not know how to take a table.
# Checked for the server's refusal specifically: if only the client's own check
# fires, the server is admitting players who cannot read its world.
"$CLIENT" --join 127.0.0.1 "$PORT" "vecchio" "$EXTENT" --no-adopt
case $? in
    2) echo "  ok    a client that cannot adopt it is refused by the server" ;;
    0) echo "  FAIL  a client that cannot adopt the table was let in"
       FAILURES=$((FAILURES + 1)) ;;
    3) echo "  FAIL  only the client's own check caught it; the server admitted it"
       FAILURES=$((FAILURES + 1)) ;;
    *) echo "  FAIL  the join failed for some other reason"
       FAILURES=$((FAILURES + 1)) ;;
esac

echo "server said:"
sed 's/^/        /' "$LOG"
stop_server

if [ "$FAILURES" -eq 0 ]; then
    echo "two-process join passed"
    exit 0
fi
echo "TWO-PROCESS JOIN FAILED"
exit 1
