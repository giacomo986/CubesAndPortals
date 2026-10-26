# Evidence survives a failure. Sourced by the shell gates, not run.
#
# Every gate here builds its world in temporary files and deletes them on the
# way out. That is right for the runs that pass and wrong for exactly the runs
# that matter: TODO.md OPEN-7 and OPEN-9 are the same entry written twice -- a
# red that did not reproduce, and no record of WHICH check failed, because the
# log had already been deleted by the time anybody read the summary line.
#
# So a failing run keeps its files and says where they are. A passing one keeps
# nothing: a suite that leaves a directory behind per green run is a suite
# nobody empties, and then the failing directory is the one nobody notices.
#
# usage, as the first thing a cleanup trap does -- the status has to be read
# before anything else in the trap overwrites it:
#
#   cleanup() {
#       status=$?
#       ...
#       keep_logs bad_link "$status" "$WORK"
#       rm -rf "$WORK"
#   }
#
# CNP_LOG_DIR says where. ctest sets it to <build>/test-logs; run by hand it
# falls back to a fixed place under TMPDIR rather than a fresh mktemp, because
# a path that changes every run is a path nobody can go and look at.

keep_logs() {
    keep_name="$1"
    keep_status="$2"
    shift 2

    [ "$keep_status" = "0" ] && return 0

    keep_dir="${CNP_LOG_DIR:-${TMPDIR:-/tmp}/cnp-test-logs}/${keep_name}-$(date +%Y%m%d-%H%M%S)-$$"
    mkdir -p "$keep_dir" 2>/dev/null || return 0

    keep_any=0
    for keep_path in "$@"; do
        [ -e "$keep_path" ] || continue
        cp -a "$keep_path" "$keep_dir/" 2>/dev/null && keep_any=1
    done

    # Said even when there was nothing to copy. "The logs are in X and X is
    # empty" is a fact about the gate; silence is a person guessing.
    if [ "$keep_any" = "1" ]; then
        echo "  logs kept: $keep_dir"
    else
        echo "  logs kept: $keep_dir (empty -- the run left nothing behind)"
    fi
    return 0
}
