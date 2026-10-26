// A link that is not loopback.
//
// DESIGN.md 8's exit test for M6 says "due server su macchine diverse". Every
// number this milestone has produced was measured at 127.0.0.1, where the round
// trip is a few tenths of a millisecond and nothing is ever lost -- and the
// constants chosen against those numbers (a two second crossing timeout, five
// seconds of heartbeat silence, sixty-four media files per request) have never
// met a link that behaves like a link.
//
// This is a UDP relay that sits between them and misbehaves on purpose.
//
// **Below ENet, not inside it.** The obvious shortcut is to drop messages at
// Transport::send, and it would be wrong: ENet's reliable channel retransmits
// what the network loses, so a message dropped before ENet ever sees it is a
// message that never comes back. Dropping datagrams on the wire is the only
// place where "lost" means what it means in the field -- the retransmission,
// the RTT estimate and the congestion window all get exercised because none of
// them can tell this from a real link.
//
// **No root, no system-wide change.** `tc qdisc ... netem` is the real
// instrument and it reconfigures an interface for every process on the machine.
// This is a process: it starts, it shapes, it prints what it did, it exits.
//
// What it does NOT emulate, said here rather than discovered as a surprise:
// MTU and fragmentation (datagrams pass through whole), path changes,
// bandwidth limits and the queueing delay that comes with them, and any
// asymmetry between the two directions beyond what the same parameters give.
//
// usage:
//   cnp_netshape --listen 25801 --to 127.0.0.1:25491
//                [--delay 60] [--jitter 20] [--loss 2] [--dup 0] [--seed 1]

#include <arpa/inet.h>
#include <cerrno>
#include <cinttypes>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <netinet/in.h>
#include <poll.h>
#include <string>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include <algorithm>
#include <chrono>
#include <vector>

namespace {

double now_seconds()
{
    static const auto t0 = std::chrono::steady_clock::now();
    return std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
}

// Seeded, so a run that found something can be run again and find it again. A
// shaper whose losses are unrepeatable turns every failure into an anecdote.
struct Rng {
    uint64_t s = 1;
    uint32_t next()
    {
        s = s * 6364136223846793005ull + 1442695040888963407ull;
        return (uint32_t)(s >> 32);   // the TOP 32 bits: >> 33 gives 31 of them
    }
    double unit() { return (double)next() / 4294967296.0; }   // [0,1)
};

struct Datagram {
    double               due  = 0.0;
    bool                 to_client = false;   // direction, for the counters
    int                  fd   = -1;           // socket to send it on
    sockaddr_in          to{};
    std::vector<uint8_t> bytes;
    uint64_t             arrived = 0;   // order it came in, to measure reordering
    double               queued_at = 0.0;
};

// One client, and the socket this relay talks to the server on for it. A socket
// each, so the server sees one peer per client exactly as it would without a
// relay in the way.
struct Client {
    sockaddr_in addr{};
    int         fd        = -1;
    double      last_seen = 0.0;
};

struct Counters {
    uint64_t in        = 0;
    uint64_t forwarded = 0;
    uint64_t dropped   = 0;
    uint64_t duplicated = 0;
    uint64_t reordered = 0;
    uint64_t bytes     = 0;
    double   delay_sum = 0.0;
    double   delay_min = 1e9;
    double   delay_max = 0.0;
    uint64_t last_sent = 0;   // the arrival number of the last one sent, for reordering
};

void report(const char *what, const Counters &c)
{
    const double mean = c.forwarded > 0 ? c.delay_sum / (double)c.forwarded : 0.0;
    std::printf("        %-14s %" PRIu64 " in, %" PRIu64 " forwarded, %" PRIu64 " dropped "
                "(%.1f%%), %" PRIu64 " duplicated, %" PRIu64 " out of order; delay %.1f/%.1f/%.1f "
                "ms min/mean/max; %.1f KB\n",
                what, c.in, c.forwarded, c.dropped,
                c.in > 0 ? 100.0 * (double)c.dropped / (double)c.in : 0.0,
                c.duplicated, c.reordered,
                c.delay_min > 1e8 ? 0.0 : c.delay_min * 1e3, mean * 1e3, c.delay_max * 1e3,
                (double)c.bytes / 1024.0);
}

} // namespace

int main(int argc, char **argv)
{
    uint16_t    listen_port = 0;
    std::string to_host     = "127.0.0.1";
    uint16_t    to_port     = 0;

    double delay_ms  = 0.0;    // one way
    double jitter_ms = 0.0;    // +/- this, uniform
    double loss_pct  = 0.0;
    double dup_pct   = 0.0;
    uint64_t seed    = 1;
    double report_every = 5.0;

    for (int i = 1; i < argc; ++i) {
        const char *a = argv[i];
        if (std::strcmp(a, "--listen") == 0 && i + 1 < argc)
            listen_port = (uint16_t)std::atoi(argv[++i]);
        else if (std::strcmp(a, "--to") == 0 && i + 1 < argc) {
            const std::string spec = argv[++i];
            const size_t      colon = spec.rfind(':');
            if (colon == std::string::npos) {
                std::printf("netshape: --to wants host:port, got '%s'\n", spec.c_str());
                return 1;
            }
            to_host = spec.substr(0, colon);
            to_port = (uint16_t)std::atoi(spec.c_str() + colon + 1);
        } else if (std::strcmp(a, "--delay") == 0 && i + 1 < argc)
            delay_ms = std::atof(argv[++i]);
        else if (std::strcmp(a, "--jitter") == 0 && i + 1 < argc)
            jitter_ms = std::atof(argv[++i]);
        else if (std::strcmp(a, "--loss") == 0 && i + 1 < argc)
            loss_pct = std::atof(argv[++i]);
        else if (std::strcmp(a, "--dup") == 0 && i + 1 < argc)
            dup_pct = std::atof(argv[++i]);
        else if (std::strcmp(a, "--seed") == 0 && i + 1 < argc)
            seed = std::strtoull(argv[++i], nullptr, 10);
        else if (std::strcmp(a, "--report") == 0 && i + 1 < argc)
            report_every = std::atof(argv[++i]);
        else {
            std::printf("netshape: unknown argument '%s'\n", a);
            return 1;
        }
    }

    if (listen_port == 0 || to_port == 0) {
        std::printf("usage: cnp_netshape --listen <port> --to <host:port> "
                    "[--delay ms] [--jitter ms] [--loss %%] [--dup %%] [--seed n]\n");
        return 1;
    }

    // Jitter bigger than the delay would schedule datagrams into the past,
    // which is not "a jittery link" -- it is no delay at all on the low half.
    // Refused rather than clamped, because a clamp turns a wrong argument into
    // a run whose numbers do not mean what the command line said.
    if (jitter_ms > delay_ms) {
        std::printf("netshape: --jitter %.0f is more than --delay %.0f; a datagram cannot "
                    "arrive before it was sent\n", jitter_ms, delay_ms);
        return 1;
    }

    const int listen_fd = ::socket(AF_INET, SOCK_DGRAM, 0);
    if (listen_fd < 0) {
        std::printf("netshape: no socket (%s)\n", std::strerror(errno));
        return 1;
    }

    sockaddr_in bind_addr{};
    bind_addr.sin_family      = AF_INET;
    bind_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    bind_addr.sin_port        = htons(listen_port);
    if (::bind(listen_fd, (sockaddr *)&bind_addr, sizeof(bind_addr)) != 0) {
        std::printf("netshape: cannot bind UDP %u (%s)\n", listen_port, std::strerror(errno));
        return 1;
    }

    sockaddr_in server_addr{};
    server_addr.sin_family = AF_INET;
    server_addr.sin_port   = htons(to_port);
    if (::inet_pton(AF_INET, to_host.c_str(), &server_addr.sin_addr) != 1) {
        std::printf("netshape: --to wants a numeric address, got '%s'\n", to_host.c_str());
        return 1;
    }

    std::printf("netshape: UDP %u -> %s:%u, one way %.0f ms +/- %.0f, %.1f%% loss, %.1f%% "
                "duplicate, seed %" PRIu64 "\n",
                listen_port, to_host.c_str(), to_port, delay_ms, jitter_ms, loss_pct, dup_pct,
                seed);
    std::printf("        a round trip therefore costs about %.0f ms\n", delay_ms * 2.0);
    std::fflush(stdout);

    Rng                   rng{seed ? seed : 1};
    std::vector<Client>   clients;
    std::vector<Datagram> queue;   // kept sorted by due time
    Counters              up, down;
    uint64_t              arrivals = 0;
    double                last_report = now_seconds();

    std::vector<uint8_t> buf(65536);

    auto schedule = [&](Counters &c, bool to_client, int fd, const sockaddr_in &to,
                        const uint8_t *data, size_t size) {
        ++c.in;
        if (loss_pct > 0.0 && rng.unit() * 100.0 < loss_pct) {
            ++c.dropped;
            return;
        }

        int copies = 1;
        if (dup_pct > 0.0 && rng.unit() * 100.0 < dup_pct) {
            copies = 2;
            ++c.duplicated;
        }

        for (int k = 0; k < copies; ++k) {
            const double jitter = jitter_ms > 0.0 ? (rng.unit() * 2.0 - 1.0) * jitter_ms : 0.0;
            const double wait   = (delay_ms + jitter) / 1000.0;

            Datagram d;
            d.due       = now_seconds() + (wait < 0.0 ? 0.0 : wait);
            d.to_client = to_client;
            d.fd        = fd;
            d.to        = to;
            d.bytes.assign(data, data + size);
            d.arrived   = ++arrivals;
            d.queued_at = now_seconds();

            // Sorted on insert. The queue holds one round trip's worth of
            // datagrams -- tens, not thousands -- so this beats a heap for the
            // thing that actually matters here, which is being obviously right.
            const auto at = std::upper_bound(queue.begin(), queue.end(), d,
                                             [](const Datagram &x, const Datagram &y) {
                                                 return x.due < y.due;
                                             });
            queue.insert(at, std::move(d));
        }
    };

    for (;;) {
        // Wake for the next datagram that is due, or for something arriving.
        int timeout_ms = 50;
        if (!queue.empty()) {
            const double wait = queue.front().due - now_seconds();
            timeout_ms = wait <= 0.0 ? 0 : (int)(wait * 1000.0) + 1;
            if (timeout_ms > 50) timeout_ms = 50;
        }

        std::vector<pollfd> fds;
        fds.push_back(pollfd{listen_fd, POLLIN, 0});
        for (const Client &c : clients) fds.push_back(pollfd{c.fd, POLLIN, 0});

        const int ready = ::poll(fds.data(), (nfds_t)fds.size(), timeout_ms);
        if (ready < 0 && errno != EINTR) {
            std::printf("netshape: poll failed (%s)\n", std::strerror(errno));
            break;
        }

        if (ready > 0 && (fds[0].revents & POLLIN)) {
            sockaddr_in from{};
            socklen_t   from_len = sizeof(from);
            const ssize_t n = ::recvfrom(listen_fd, buf.data(), buf.size(), 0,
                                         (sockaddr *)&from, &from_len);
            if (n > 0) {
                // Which client this is, or a new one. Matched on address and
                // port together: two clients behind one address are two peers
                // to the server and have to stay two here.
                Client *client = nullptr;
                for (Client &c : clients)
                    if (c.addr.sin_addr.s_addr == from.sin_addr.s_addr &&
                        c.addr.sin_port == from.sin_port) {
                        client = &c;
                        break;
                    }

                if (client == nullptr) {
                    Client c;
                    c.addr = from;
                    c.fd   = ::socket(AF_INET, SOCK_DGRAM, 0);
                    if (c.fd >= 0) {
                        clients.push_back(c);
                        client = &clients.back();
                        char text[INET_ADDRSTRLEN] = {0};
                        ::inet_ntop(AF_INET, &from.sin_addr, text, sizeof(text));
                        std::printf("        a client from %s:%u, socket %d\n", text,
                                    (unsigned)ntohs(from.sin_port), c.fd);
                        std::fflush(stdout);
                    }
                }

                if (client != nullptr) {
                    client->last_seen = now_seconds();
                    up.bytes += (uint64_t)n;
                    schedule(up, false, client->fd, server_addr, buf.data(), (size_t)n);
                }
            }
        }

        for (size_t i = 0; i < clients.size(); ++i) {
            if (i + 1 >= fds.size() || !(fds[i + 1].revents & POLLIN)) continue;
            sockaddr_in from{};
            socklen_t   from_len = sizeof(from);
            const ssize_t n = ::recvfrom(clients[i].fd, buf.data(), buf.size(), 0,
                                         (sockaddr *)&from, &from_len);
            if (n <= 0) continue;
            down.bytes += (uint64_t)n;
            schedule(down, true, listen_fd, clients[i].addr, buf.data(), (size_t)n);
        }

        // Everything that has come due, in due order -- which is not arrival
        // order when the jitter has reshuffled them, and that reshuffling is
        // counted rather than assumed.
        const double t = now_seconds();
        size_t       sent = 0;
        while (sent < queue.size() && queue[sent].due <= t) {
            Datagram &d = queue[sent];
            ::sendto(d.fd, d.bytes.data(), d.bytes.size(), 0, (const sockaddr *)&d.to,
                     sizeof(d.to));

            Counters &c = d.to_client ? down : up;
            ++c.forwarded;
            if (d.arrived < c.last_sent) ++c.reordered;
            c.last_sent = d.arrived;

            // What it actually held the datagram for, not what it meant to.
            // The two differ by the poll granularity, and a shaper that
            // reported its intention would be a shaper nobody could check.
            const double held = t - d.queued_at;
            c.delay_sum += held;
            if (held < c.delay_min) c.delay_min = held;
            if (held > c.delay_max) c.delay_max = held;
            ++sent;
        }
        if (sent > 0) queue.erase(queue.begin(), queue.begin() + (long)sent);

        if (report_every > 0.0 && t - last_report >= report_every) {
            last_report = t;
            std::printf("netshape at %.0fs, %zu client(s), %zu in flight\n", t, clients.size(),
                        queue.size());
            report("to the server", up);
            report("to the client", down);
            std::fflush(stdout);
        }
    }

    return 0;
}
