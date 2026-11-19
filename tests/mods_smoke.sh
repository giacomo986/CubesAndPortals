#!/bin/sh
# Every mod of the sandbox and of every game loads, and keeps running.
#
# A mod whose load fails is left out with a line on stderr, and one whose
# handler errors is switched off with another -- and the server goes on, which
# is right for a player and lets a broken mod go unnoticed for weeks:
# backrooms_exit's level handler stopped on every level for that long, and
# inferno lost its helpers to a dependency nobody linked. This starts each
# world the way its tools/ script does, runs it for a few hundred ticks with
# nobody in it, and fails on any mod that did not load or was switched off.
#
# Nobody in it: a handler that only runs for a player (on joining, on a key)
# is not reached. What it catches is a mod that does not load, a dependency
# that is missing, and an error at load, in mapgen or in a tick.
#
# usage: mods_smoke.sh <server binary> <source dir>

set -u

SERVER="$1"
ROOT="$2"
OUT="$(mktemp -t cnp_smoke.XXXXXX)"
FAILURES=0

. "$(dirname "$0")/keep_logs.sh"

cleanup() {
    status=$?
    keep_logs mods_smoke "$status" "$OUT"
    rm -f "$OUT"
}
trap cleanup EXIT INT TERM

# What is not a failure: a mod for clients, which a server offers and does
# not run, and a mod its mod.conf turns off.
EXPECTED='a mod for clients|disabled in mod.conf'

check() {
    name="$1"
    shift
    : > "$OUT"
    "$SERVER" --world 64 --ticks 300 "$@" > "$OUT" 2>&1
    status=$?
    bad=$(grep -E "^mod '[^']*' (not loaded|disabled|reload failed)" "$OUT" | grep -Ev "$EXPECTED")
    loaded=$(grep -c "^mod '[^']*' .* loaded, capabilities" "$OUT")
    if [ "$status" -ne 0 ]; then
        echo "  FAIL  $name: the server stopped with status $status"
        tail -5 "$OUT" | sed 's/^/        /'
        FAILURES=$((FAILURES + 1))
    elif [ -n "$bad" ]; then
        echo "  FAIL  $name:"
        echo "$bad" | sed 's/^/        /'
        FAILURES=$((FAILURES + 1))
    elif [ "$loaded" -eq 0 ]; then
        echo "  FAIL  $name: no mod loaded"
        FAILURES=$((FAILURES + 1))
    else
        echo "  ok    $name: $loaded mods"
    fi
}

echo "every mod loads and keeps running"

check sandbox --mods "$ROOT/mods"
for dir in "$ROOT"/games/*/; do
    game=$(basename "$dir")
    [ -d "$dir/mods" ] || continue
    case "$game" in
        # tools/vehiclelab.sh: the lab on top of the sandbox's own mods.
        vehiclelab) check "$game" --user-mods "$dir/mods" ;;
        # A player's workshop (mods/workshop): the worlds' blocks over the
        # sandbox's mods, in a room workshop-* where they leave their worlds
        # out -- of two: with one, core.room() is "".
        workshop) check "$game" --room workshop-test --no-plaza --mods "$ROOT/mods" --user-mods "$dir/mods" \
                      --room other --mods "$ROOT/games/track/mods" ;;
        *)          check "$game" --mods "$dir/mods" ;;
    esac
done

if [ "$FAILURES" -ne 0 ]; then
    echo "$FAILURES world(s) with a mod that did not load or was switched off"
    exit 1
fi
exit 0
