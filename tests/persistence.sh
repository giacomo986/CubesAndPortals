#!/bin/sh
# A server that is stopped and started again finds the world it left.
#
# The storage test covers the library: save a world, load it, compare. This
# covers the binary, which is where the ordering lives -- the world file is
# opened after the mods have registered their blocks, because the content hash
# it is checked against has to be the final table. Get that wrong and a server
# writes a file it then refuses to reopen, which no library test would notice.
#
# usage: persistence.sh <server binary>

set -u

SERVER="$1"
DB="$(mktemp -u -t cnp_persist.XXXXXX).db"
OUT1="$(mktemp -t cnp_p1.XXXXXX)"
OUT2="$(mktemp -t cnp_p2.XXXXXX)"
OUT3="$(mktemp -t cnp_p3.XXXXXX)"
FAILURES=0

. "$(dirname "$0")/keep_logs.sh"

cleanup() {
    status=$?
    keep_logs persistence "$status" "$OUT1" "$OUT2" "$OUT3"
    rm -f "$DB" "$DB-wal" "$DB-shm" "$OUT1" "$OUT2" "$OUT3"
}
trap cleanup EXIT INT TERM

run() {
    "$SERVER" --world 64 --ticks 100 --world-file "$DB" "$@"
}

echo "a server that stops and starts again"

run > "$OUT1" 2>&1
HASH1=$(grep "^world hash" "$OUT1" | awk '{print $3}')
if [ -n "$HASH1" ]; then
    echo "  ok    the first run finishes and writes a world file"
else
    echo "  FAIL  the first run produced no world hash"
    cat "$OUT1"
    exit 1
fi

if [ -f "$DB" ]; then
    echo "  ok    the file exists ($(wc -c < "$DB") bytes)"
else
    echo "  FAIL  no world file was written"
    FAILURES=$((FAILURES + 1))
fi

run > "$OUT2" 2>&1
HASH2=$(grep "^world hash" "$OUT2" | awk '{print $3}')
# Anchored on the words in front of the number, not on "(N loaded)": the line
# carries two of those now, and a greedy match reads the second one.
LOADED=$(grep "chunks stored" "$OUT2" | sed 's/.*chunks stored (\([0-9]*\) loaded).*/\1/')

if [ "${LOADED:-0}" -gt 0 ]; then
    echo "  ok    the second run loads $LOADED chunks from it"
else
    echo "  FAIL  the second run loaded nothing"
    cat "$OUT2"
    FAILURES=$((FAILURES + 1))
fi

if [ "$HASH1" = "$HASH2" ]; then
    echo "  ok    and ends with the same world: $HASH2"
else
    echo "  FAIL  the world changed across a restart: $HASH1 then $HASH2"
    FAILURES=$((FAILURES + 1))
fi

# A file that is not this world. Another seed is refused -- exiting zero and
# carrying on would put two worlds side by side. Another block table is not: the
# file keeps its blocks by name, so a build without the mods that made some of
# them opens it with those blocks as placeholders, and the mods coming back get
# the world back.
echo ""
echo "a file that is not this world"

if run --seed 7 > "$OUT3" 2>&1; then
    echo "  FAIL  a file made with another seed was accepted"
    FAILURES=$((FAILURES + 1))
else
    echo "  ok    a file made with another seed is refused, and the server exits nonzero"
    grep "world file was made" "$OUT3" | sed 's/^/        /'
fi

if run --no-mods > "$OUT3" 2>&1 && grep -q "until their mod is back" "$OUT3"; then
    echo "  ok    a build without the mods opens it, their blocks placeholders"
    grep "until their mod is back" "$OUT3" | cut -c1-150 | sed 's/^/        /'
else
    echo "  FAIL  a build without the mods refused the file, or did not say what it put in"
    tail -5 "$OUT3"
    FAILURES=$((FAILURES + 1))
fi

run > "$OUT3" 2>&1
HASH3=$(grep "^world hash" "$OUT3" | awk '{print $3}')
if [ "$HASH3" = "$HASH2" ]; then
    echo "  ok    and with the mods back the world is the one it was: $HASH3"
else
    echo "  FAIL  the world changed through a run without its mods: $HASH2 then $HASH3"
    FAILURES=$((FAILURES + 1))
fi

# And what is in the chests. TODO.md OPEN-10: `ContainerSet` was memory, so a
# world that came back from disk came back with the blocks and without anything
# in them -- the worst of the two failures, because the ground coming back says
# it is the same world.
#
# The mod under tests/storage_mods stocks its chest only when it finds it empty,
# so a build with the storage layer taken out fails this rather than quietly
# refilling the chest each run and passing.
echo ""
echo "a chest that was left full"

CHESTDB="$(mktemp -u -t cnp_chest.XXXXXX).db"
CHESTMODS="$(dirname "$0")/storage_mods"

chest_run() {
    "$SERVER" --world 64 --ticks 20 --world-file "$CHESTDB" --mods "$CHESTMODS" --quiet "$@"
}

cleanup_chest() {
    rm -f "$CHESTDB" "$CHESTDB-wal" "$CHESTDB-shm"
}
trap 'cleanup; cleanup_chest' EXIT INT TERM

chest_run > "$OUT1" 2>&1
if grep -q "loot: stocked the chest, 0 did not fit" "$OUT1"; then
    echo "  ok    the first run puts forty stone in a chest nobody has opened"
else
    echo "  FAIL  the first run did not stock the chest"
    cat "$OUT1"
    FAILURES=$((FAILURES + 1))
fi

chest_run > "$OUT2" 2>&1
STORED=$(grep "containers (" "$OUT2" | sed 's/.*containers (\([0-9]*\) loaded).*/\1/')
HELD=$(grep "loot: chest holds" "$OUT2" | tail -1 | awk '{print $NF}')

if [ "${STORED:-0}" -eq 1 ]; then
    echo "  ok    the second run loads the chest out of the file"
else
    echo "  FAIL  the second run loaded no containers"
    cat "$OUT2"
    FAILURES=$((FAILURES + 1))
fi

if [ "${HELD:-0}" = "40" ]; then
    echo "  ok    still holding forty, and the mod did not have to put them back"
else
    echo "  FAIL  the chest came back holding ${HELD:-nothing}"
    cat "$OUT2"
    FAILURES=$((FAILURES + 1))
fi

if grep -q "stocked the chest" "$OUT2"; then
    echo "  FAIL  the second run refilled the chest, so this gate proves nothing"
    FAILURES=$((FAILURES + 1))
else
    echo "  ok    and the gate would have caught a mod that refilled it every run"
fi

if [ "$FAILURES" -eq 0 ]; then
    echo "persistence passed"
    exit 0
fi
echo "PERSISTENCE FAILED"
exit 1
