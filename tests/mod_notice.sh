#!/bin/sh
# A mod switched off by an error is told in the chat of whoever may read the
# logs, and of nobody else.
#
# The server's stderr is read by nobody while a world is played. So a mod that
# errors is reported to the players with the `logs` permission. This starts a
# server owned by "giocatore" with one mod that errors on the first block
# broken, sends in a probe with an identity under that name, which breaks a
# block and must be told; then the same with the server owned by somebody
# else, where the mod is switched off all the same and the probe must hear
# nothing of it.
#
# usage: mod_notice.sh <server binary> <probe binary> <source dir>

set -u

SERVER="$1"
PROBE="$2"
ROOT="$3"
EXTENT=64
WORK="$(mktemp -d -t cnp_notice.XXXXXX)"
SERVER_PID=""
PORTS=$((27000 + ($$ % 100) * 8))
FAILURES=0

. "$(dirname "$0")/keep_logs.sh"

cleanup() {
    status=$?
    [ -n "$SERVER_PID" ] && kill "$SERVER_PID" 2>/dev/null && wait "$SERVER_PID" 2>/dev/null
    keep_logs mod_notice "$status" "$WORK"
    rm -rf "$WORK"
}
trap cleanup EXIT INT TERM

# One run: the server owned by $1, its log $2, the probe's $3.
run() {
    owner="$1"
    log="$2"
    plog="$3"
    dir="$WORK/world-$owner"
    mkdir -p "$dir"
    PORT=0
    for candidate in $PORTS $((PORTS + 1)) $((PORTS + 2)) $((PORTS + 3)); do
        : > "$log"
        (cd "$dir" && exec "$SERVER" --listen "$candidate" --ticks 0 --world "$EXTENT" \
             --key-file "$dir/server.key" --world-file "$dir/w.world" \
             --mods "$ROOT/tests/mods_notice" --owner "$owner") < /dev/null > "$log" 2>&1 &
        SERVER_PID=$!
        i=0
        while [ $i -lt 300 ]; do
            grep -q "listening on UDP $candidate" "$log" 2>/dev/null && { PORT=$candidate; break; }
            kill -0 "$SERVER_PID" 2>/dev/null || break
            sleep 0.1
            i=$((i + 1))
        done
        [ "$PORT" -ne 0 ] && break
        kill "$SERVER_PID" 2>/dev/null
        wait "$SERVER_PID" 2>/dev/null
        SERVER_PID=""
    done
    PORTS=$((PORTS + 4))
    if [ "$PORT" -eq 0 ]; then
        echo "  FAIL  the server owned by $owner did not start listening"
        tail -10 "$log" | sed 's/^/        /'
        FAILURES=$((FAILURES + 1))
        return 1
    fi
    timeout 60 "$PROBE" --join 127.0.0.1 "$PORT" giocatore "$EXTENT" \
        --identity "$WORK/probe.key" --play --dig 2 --stay 2 > "$plog" 2>&1
    probe_status=$?
    kill -TERM "$SERVER_PID" 2>/dev/null
    wait "$SERVER_PID" 2>/dev/null
    SERVER_PID=""
    if [ "$probe_status" -ne 0 ]; then
        echo "  FAIL  the probe did not join and play (exit $probe_status)"
        sed 's/^/        probe: /' "$plog"
        FAILURES=$((FAILURES + 1))
        return 1
    fi
    if ! grep -q "^mod 'brokenbreak' disabled: handler for 'core:broken'" "$log"; then
        echo "  FAIL  owner $owner: the mod was not switched off -- the probe broke no block?"
        FAILURES=$((FAILURES + 1))
        return 1
    fi
    return 0
}

echo "a mod switched off by an error is told to its admins, and only to them"

if run giocatore "$WORK/owner.log" "$WORK/owner.probe"; then
    if grep -q "^chat: \[mod\] brokenbreak switched off: .*broken on purpose" "$WORK/owner.probe"; then
        echo "  ok    the owner is told, with the error"
    else
        echo "  FAIL  the owner was not told; the chat it got:"
        grep "^chat:" "$WORK/owner.probe" | sed 's/^/        /'
        FAILURES=$((FAILURES + 1))
    fi
fi

if run qualcunaltro "$WORK/player.log" "$WORK/player.probe"; then
    if grep -q "^chat: \[mod\]" "$WORK/player.probe"; then
        echo "  FAIL  a player with no role was told:"
        grep "^chat: \[mod\]" "$WORK/player.probe" | sed 's/^/        /'
        FAILURES=$((FAILURES + 1))
    else
        echo "  ok    a player with no role is not"
    fi
fi

[ "$FAILURES" -eq 0 ] || { echo "$FAILURES failure(s)"; exit 1; }
exit 0
