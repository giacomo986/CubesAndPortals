#!/bin/bash
# One admin command to a running cnp_server, over its RCON socket on this
# machine (--rcon PORT --rcon-password PASSWORD, or rcon / rcon-password in its
# --config file). Prints the answer.
#
# usage: CNP_RCON_PASSWORD=secret tools/admin.sh [-p PORT] COMMAND [ARGS...]
#        tools/admin.sh -p 25399 bans
#        tools/admin.sh ban bob 2h griefing
set -eu

PORT=25399
if [ "${1:-}" = "-p" ]; then
    PORT="$2"
    shift 2
fi
[ $# -gt 0 ] || { echo "usage: $0 [-p PORT] COMMAND [ARGS...]" >&2; exit 2; }
: "${CNP_RCON_PASSWORD:?set CNP_RCON_PASSWORD to the rcon password of the server}"

exec 3<>"/dev/tcp/127.0.0.1/$PORT"
printf '%s\n%s\n' "$CNP_RCON_PASSWORD" "$*" >&3

read -r first <&3
if [ "$first" != "ok" ]; then
    echo "rcon: $first" >&2
    exit 1
fi
# The answer, up to the line that is a lone dot.
while read -r line <&3; do
    [ "$line" = "." ] && break
    printf '%s\n' "$line"
done
exec 3>&-
