#!/bin/bash
# Keeps a cnp_server running: started again when it restarts (/restart,
# --restart-at: it leaves with 75) or crashes (--watchdog included), and not
# when it stops (Ctrl+C, /stop: it leaves with 0). Every line it prints goes to
# one log file a day, with the time in front; the oldest go after -k days.
# The server's stdin stays this script's, so its console still works.
#
# usage: tools/serve.sh [-l LOGDIR] [-k DAYS] SERVER [ARGS...]
#        tools/serve.sh -l logs build-release/src/server/cnp_server --config server.conf
set -u

LOGDIR=logs
KEEP=14
while getopts "l:k:" o; do
    case $o in
        l) LOGDIR=$OPTARG ;;
        k) KEEP=$OPTARG ;;
        *) exit 2 ;;
    esac
done
shift $((OPTIND - 1))
[ $# -gt 0 ] || { echo "usage: $0 [-l LOGDIR] [-k DAYS] SERVER [ARGS...]" >&2; exit 2; }
mkdir -p "$LOGDIR" || exit 1

stamp() {
    while IFS= read -r line; do
        printf -v day '%(%Y-%m-%d)T' -1
        printf '%(%F %T)T %s\n' -1 "$line" | tee -a "$LOGDIR/server-$day.log"
    done
}

say() { printf '%(%F %T)T serve.sh: %s\n' -1 "$1" | tee -a "$LOGDIR/server-$(printf '%(%Y-%m-%d)T' -1).log"; }

# A background job's stdin is /dev/null unless it is given one.
exec 3<&0
pid=0
stop=0
trap 'stop=1; [ $pid -gt 0 ] && kill -TERM $pid 2>/dev/null' INT TERM

crashes=()
while :; do
    find "$LOGDIR" -name 'server-*.log' -mtime +"$KEEP" -delete 2>/dev/null
    "$@" <&3 > >(stamp) 2>&1 &
    pid=$!
    # wait comes back early for a signal; the server is still going down then.
    while :; do
        wait $pid
        rc=$?
        kill -0 $pid 2>/dev/null || break
    done
    pid=0

    if [ $stop = 1 ] || [ $rc = 0 ]; then
        say "the server stopped ($rc)"
        exit $rc
    fi
    if [ $rc = 75 ]; then
        say "the server is restarting"
        continue
    fi

    # Five crashes in ten minutes is not something starting again will fix.
    now=$(printf '%(%s)T' -1)
    kept=()
    for t in "${crashes[@]}"; do [ $((now - t)) -lt 600 ] && kept+=("$t"); done
    crashes=("${kept[@]}" "$now")
    if [ ${#crashes[@]} -ge 5 ]; then
        say "the server crashed 5 times in 10 minutes ($rc); giving up"
        exit $rc
    fi
    say "the server crashed ($rc); starting it again in 5 s"
    sleep 5
done
