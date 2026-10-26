#!/bin/bash
# Two servers on two machines, as close as a container gets.
#
# Not a ctest: it needs docker, and a test that fails on a machine without a
# container runtime is a test that teaches people to ignore failures. What it is
# for is the class of defect one host cannot produce -- run it by hand, then put
# whatever it finds into tests/bad_link.sh, which reaches the same thing with
# two spellings of 127.0.0.1.
#
#   tools/two_machines.sh            # MTU 1280, no shaping
#   MTU=576 LOSS=10 tools/two_machines.sh
#   build/src/client/cnp_client --connect 127.0.0.1 --port 25891 --play-seconds 40
#
# What this buys over ~/cnp-run/slow.sh, which shapes loopback: two network
# stacks, a real bridge with a real MTU, and -- the part that matters -- the two
# servers reach each other through the HOST's published ports, which is DNAT in
# and masquerade out. A portal address that survives that is a portal address
# that survives a home router.
#
# What it does not buy: one kernel, one wall clock, one CPU. Linux time
# namespaces do not cover CLOCK_REALTIME, so no container can move the clock a
# ticket is checked against -- and it turns out nothing needs to: tests/two_clocks.sh
# gets two disagreeing wall clocks out of an LD_PRELOAD, on one host, in ctest.
# What is still only a VM's to give is a separate kernel and real CPU contention.
set -u

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
SRV="$ROOT/build/src/server/cnp_server"
IMG=ubuntu:24.04
HERE="${CNP_DOCKER_WORK:-$HOME/cnp-docker}"

MTU="${MTU:-1280}"          # a tunnelled path; 1500 is the boring case
DELAY="${DELAY:-0}"         # ms, one way, applied inside each container
JITTER="${JITTER:-0}"
LOSS="${LOSS:-0}"           # percent

docker rm -f cnp-srv-a cnp-srv-b >/dev/null 2>&1
docker network rm cnp-a cnp-b   >/dev/null 2>&1
rm -rf "$HERE/a" "$HERE/b"
mkdir -p "$HERE/a" "$HERE/b"

# Two networks, not one. On a single bridge the two containers talk directly and
# nothing is translated; on two, everything between them goes through the host.
docker network create --driver bridge --opt com.docker.network.driver.mtu="$MTU" \
    --subnet 172.31.7.0/24 cnp-a > /dev/null
docker network create --driver bridge --opt com.docker.network.driver.mtu="$MTU" \
    --subnet 172.31.8.0/24 cnp-b > /dev/null

GW_A=172.31.7.1              # the host, as container A sees it
GW_B=172.31.8.1              # and as container B sees it

# The source tree at the same path inside as outside. The server resolves its
# mods and textures against CNP_SOURCE_ROOT, which is baked in at compile time:
# without this the container runs 0 mods and generates its own placeholder
# textures, and every gate about block tables and media would be measuring a
# different game.
MOUNT=(-v "$ROOT:$ROOT:ro")

run_server() {
    local name=$1 net=$2 ip=$3 pub=$4 inner=$5 work=$6; shift 6
    docker run -d --name "$name" --network "$net" --ip "$ip" \
        -p "$pub:$inner/udp" "${MOUNT[@]}" -v "$HERE/$work:/work" -w /work \
        --cap-add NET_ADMIN "$IMG" \
        "$SRV" --listen "$inner" "$@" > /dev/null
}

# First pass: just to create the signing keys, the same two-step ~/cnp-run does.
run_server cnp-srv-a cnp-a 172.31.7.10 25891 25491 a \
    --ticks 20 --key-file /work/a.id --ticket-key /work/a.sign --world-file /work/a.world
run_server cnp-srv-b cnp-b 172.31.8.10 25892 25492 b \
    --ticks 20 --key-file /work/b.id --ticket-key /work/b.sign --world-file /work/b.world
docker wait cnp-srv-a cnp-srv-b > /dev/null

KEY_A=$(docker logs cnp-srv-a 2>&1 | sed -n 's/.*--trust \([0-9a-f]*\).*/\1/p' | head -1)
KEY_B=$(docker logs cnp-srv-b 2>&1 | sed -n 's/.*--trust \([0-9a-f]*\).*/\1/p' | head -1)
docker rm -f cnp-srv-a cnp-srv-b > /dev/null

# Second pass: each one's mouth points at the HOST's published port for the
# other, never at the other's container address. That is the whole point.
run_server cnp-srv-a cnp-a 172.31.7.10 25891 25491 a \
    --portals --portal-to "$GW_A:25892/1" --portal-peer-key "$KEY_B" \
    --key-file /work/a.id --ticket-key /work/a.sign --world-file /work/a.world \
    --ticks 100000
run_server cnp-srv-b cnp-b 172.31.8.10 25892 25492 b \
    --portals --portal-to "$GW_B:25891" --portal-peer-key "$KEY_A" --trust "$KEY_A" \
    --key-file /work/b.id --ticket-key /work/b.sign --world-file /work/b.world \
    --ticks 100000

# The link inside each container, if asked for. netem here rather than a relay
# in front: the queue is on the interface the server actually sends from.
if [ "$DELAY" != "0" ] || [ "$LOSS" != "0" ]; then
    for c in cnp-srv-a cnp-srv-b; do
        docker exec "$c" sh -c "command -v tc > /dev/null || (apt-get -qq update && \
            apt-get -qq install -y iproute2) > /dev/null 2>&1"
        docker exec "$c" tc qdisc add dev eth0 root netem \
            delay "${DELAY}ms" "${JITTER}ms" loss "${LOSS}%" 2>&1 | head -1
    done
fi

sleep 3
echo "MTU $MTU, netem ${DELAY}ms +/- ${JITTER} loss ${LOSS}%"
for c in cnp-srv-a cnp-srv-b; do
    printf "  %-11s %s\n" "$c" "$(docker logs "$c" 2>&1 | grep -c 'listening on UDP') listening, \
$(docker logs "$c" 2>&1 | sed -n 's/.*media: \([0-9]*\) file(s).*/\1/p' | head -1) media file(s), \
$(docker logs "$c" 2>&1 | sed -n 's/.*\([0-9]*\) mods running.*/\1/p' | head -1) mods"
done
echo "the client joins with:  --connect 127.0.0.1 --port 25891"
