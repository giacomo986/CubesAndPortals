// A client with no window, for the two-process join test.
//
// It drives cnp::net::ClientSession -- the same class the graphical client
// drives -- against a real `cnp_server --listen`, and checks the two things a
// join has to get right: the server lets the right clients in and refuses the
// wrong ones, and the world the client generates from the seed is the world
// the server says it has.
//
// usage: cnp_join_probe --join <host> <port> [name] [extent]
//                       [--wrong-table] [--no-adopt] [--no-encryption]
//                       [--server-key <hex>]
//
// Exit codes: 0 joined and agreed, 1 failed, 2 refused by the server, 3 this
// client refused what the server sent.

#include "core/net/client_session.h"
#include "core/net/protocol.h"
#include "core/net/session.h"
#include "core/net/transport.h"
#include "core/registry/core_nodes.h"
#include "core/registry/node_registry.h"
#include "core/voxel/chunk.h"
#include "core/worldgen/worldgen.h"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <string>

using namespace cnp;
using namespace cnp::net;

namespace {

float g_travel = 0.0f;    // --travel: blocks to fly along +x after joining
float g_hold   = 35.0f;   // --hold: seconds to hover at the end of it
bool  g_far    = false;   // --far X Y Z: say at the end whether that chunk arrived
float g_far_at[3] = {0.0f, 0.0f, 0.0f};
bool  g_flown  = true;    // --walked-down: come down without saying it is flying

double now_seconds()
{
    static const auto t0 = std::chrono::steady_clock::now();
    return std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
}

uint64_t generate_and_hash(uint32_t seed, int extent)
{
    World        world;
    NodeRegistry registry;
    register_core_nodes(registry);

    Mapgen mapgen;
    mapgen.generate(world, registry, seed, extent, kWorldHeight);
    return world_hash(world, extent, kWorldHeight);
}

int join_mode(const char *host, uint16_t port, const char *name, int extent,
              bool claim_wrong_table, bool adopt, const char *pin_hex, bool encrypt)
{
    ClientSession client;
    std::string   err;

    ClientConfig cfg;
    cfg.host    = host;
    cfg.port    = port;
    cfg.encrypt = encrypt;

    if (pin_hex) {
        if (!from_hex(pin_hex, cfg.server_key, kKeyBytes)) {
            std::printf("--server-key wants 64 hex characters\n");
            return 1;
        }
        cfg.pin_server = true;
    }

    Hello hello = make_client_hello(name);
    if (!adopt) hello.features.clear();   // an older client that cannot take a table
    if (claim_wrong_table) {
        // One build tree cannot compile two versions of itself, so it pretends.
        hello.content_hash ^= 0x1ull;
        hello.engine_version = "0.3.0-pretend";
    }

    if (!client.connect(cfg, hello, now_seconds(), err)) {
        std::printf("could not connect: %s\n", err.c_str());
        return 1;
    }
    std::printf("connecting to %s:%u as '%s'\n", host, (unsigned)port, name);

    while (!client_join_settled(client.state())) client.pump(now_seconds(), 10);

    switch (client.state()) {
    case ClientState::Failed:
        std::printf("refused by this client: %s\n", client.error().c_str());
        return 3;
    case ClientState::Refused:
        std::printf("refused by the server: %s (%s)\n",
                    refuse_reason_text(client.refusal().reason),
                    client.refusal().detail.c_str());
        return 2;
    case ClientState::TimedOut:
        std::printf("%s\n", client.error().c_str());
        return 1;
    default:
        break;
    }

    const Welcome &w = client.welcome();
    std::printf("joined: server '%s', protocol %u, seed %u, %zu blocks (%zu unknown here)\n",
                w.engine_version.c_str(), (unsigned)w.protocol, w.seed, w.nodes.size(),
                client.unknown_blocks());

    const uint64_t mine = generate_and_hash(w.seed, extent);
    const bool     same = mine == w.mapgen_hash;
    std::printf("        generated the same world locally: %s\n", same ? "yes" : "NO");

    // Say where we are, and wait for the ground to arrive.
    const float start_x = (float)extent * 0.5f;
    const float start_z = (float)extent * 0.5f;
    client.send_move(start_x, (float)kGroundTop + 2.0f, start_z, 1.0f, 0.0f, 0.0f,
                     now_seconds());

    const double deadline    = now_seconds() + 3.0;
    int          quiet_ticks = 0;
    int          last_count  = 0;
    while (now_seconds() < deadline) {
        client.pump(now_seconds(), 10);
        if (client.chunks_received() == last_count) {
            if (++quiet_ticks > 60) break;
        } else {
            quiet_ticks = 0;
            last_count  = client.chunks_received();
        }
    }

    const bool ground =
        client.world().get_block((int)start_x, kGroundTop - 1, (int)start_z) != kAir;
    std::printf("        received %d chunks, ground under the player: %s\n",
                client.chunks_received(), ground ? "solid" : "MISSING");

    const bool streamed_ok = client.chunks_received() > 0 && client.chunk_error().empty() && ground;

    // --travel N: climb, fly N blocks along +x, hover for --hold seconds, then
    // fly down to the ground and stand there a second. A measurement by hand and
    // not a check (TODO.md OPEN-35, OPEN-38): the server has to make the world
    // ahead as fast as a body crosses it, and let go of what it left behind --
    // and this client has to be told, so what it holds follows it instead of
    // growing. The way down is a flight too, and a flight down is not a fall:
    // --walked-down sends it without the flag, which the server charges.
    if (g_travel > 0.0f && streamed_ok) {
        constexpr float kSpeed  = 12.0f;   // under the default world's 14 m/s of flight
        constexpr float kClimb  = 8.0f;
        constexpr float kCruise = 50.0f;   // over the tallest terrain, under the top of the world

        float        x = start_x, y = (float)kGroundTop + 2.0f;
        float        z = start_z;
        float        travelled = 0.0f;
        long         corrections = 0;
        int          most_held   = 0;
        double       t_prev      = now_seconds();
        const double t_begin     = t_prev;
        double       hold_until  = -1.0;
        double       stand_until = -1.0;
        const float  cruise_y    = kCruise;

        // The top of whatever this client holds under (x, z), or -1.
        auto ground_under = [&](float at_x, float at_z) {
            for (int gy = kWorldHeight - 1; gy >= 0; --gy)
                if (client.world().get_block((int)at_x, gy, (int)at_z) != 0) return gy + 1;
            return -1;
        };

        for (;;) {
            client.pump(now_seconds(), 5);
            const double now = now_seconds();
            if (now - t_prev < 0.05) continue;
            const float dt = (float)(now - t_prev);
            t_prev         = now;

            if (y < kCruise && hold_until < 0.0) {
                y = std::min(kCruise, y + kClimb * dt);
            } else if (travelled < g_travel) {
                const float step = std::min(g_travel - travelled, kSpeed * dt);
                x += step;
                travelled += step;
            } else if (hold_until < 0.0) {
                hold_until = now + g_hold;
                std::printf("        arrived at %.0f after %.1f s: %d chunks received, %d let go, "
                            "%zu held\n",
                            (double)x, now - t_begin, client.chunks_received(),
                            client.chunks_dropped(), client.world().chunk_count());
            } else if (now < hold_until) {
                // hovering
            } else if (stand_until < 0.0) {
                const int floor_y = ground_under(x, z);
                if (floor_y < 0) {
                    std::printf("        nothing under %.0f %.0f to come down on\n", (double)x,
                                (double)z);
                    break;
                }
                y = std::max((float)floor_y, y - kSpeed * dt);
                if (y <= (float)floor_y) {
                    stand_until = now + 1.0;
                    std::printf("        came down %.0f nodes to %d %s\n", (double)(cruise_y - y),
                                floor_y, g_flown ? "flying" : "without saying it flew");
                }
            } else if (now >= stand_until) {
                break;
            }

            Correct c;
            if (client.take_correction(c)) {
                ++corrections;
                std::printf("        corrected to %.1f %.1f %.1f: %s\n", (double)c.x, (double)c.y,
                            (double)c.z, c.reason.c_str());
                x = c.x;
                y = c.y;
            }
            // A portal crossing: carried on from where the server put us, still
            // along +x, so what it was measured against is which opening.
            Teleport tp;
            if (client.take_teleport(tp)) {
                std::printf("        crossed at %.1f %.1f %.1f, came out at %.1f %.1f %.1f "
                            "looking %.2f %.2f %.2f\n", (double)x, (double)y, (double)z,
                            (double)tp.x, (double)tp.y, (double)tp.z, (double)tp.look_x,
                            (double)tp.look_y, (double)tp.look_z);
                x = tp.x;
                y = tp.y;
                z = tp.z;
            }
            // Flying until standing on the ground, as a player's client says it.
            client.send_move(x, y, z, 1.0f, 0.0f, 0.0f, now, g_flown && stand_until < 0.0);
            most_held = std::max(most_held, (int)client.world().chunk_count());
        }

        const ChunkPos here = World::chunk_of((int)x, 0, (int)z);
        std::printf("        travelled %.0f blocks: %ld correction(s), %d chunks received, %d let "
                    "go, %zu held at the end (at most %d), the column below %s\n",
                    (double)travelled, corrections, client.chunks_received(),
                    client.chunks_dropped(), client.world().chunk_count(), most_held,
                    client.world().find_chunk(ChunkPos{here.x, 0, here.z}) ? "arrived" : "MISSING");
        if (g_far) {
            const ChunkPos f = World::chunk_of((int)g_far_at[0], (int)g_far_at[1], (int)g_far_at[2]);
            int held = 0;
            for (int dz = -1; dz <= 1; ++dz)
                for (int dx = -1; dx <= 1; ++dx)
                    if (client.world().find_chunk(ChunkPos{f.x + dx, f.y, f.z + dz})) ++held;
            std::printf("        at %.0f %.0f %.0f: %d of the 9 chunks round it held\n",
                        (double)g_far_at[0], (double)g_far_at[1], (double)g_far_at[2], held);
        }
    }

    client.close();
    return (same && streamed_ok) ? 0 : 1;
}

} // namespace

int main(int argc, char **argv)
{
    if (argc < 4 || std::string(argv[1]) != "--join") {
        std::fprintf(stderr, "usage: %s --join <host> <port> [name] [extent] [options]\n",
                     argv[0]);
        return 1;
    }

    bool        wrong_table = false, adopt = true, encrypt = true;
    const char *pin         = nullptr;
    for (int i = 4; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--wrong-table") wrong_table = true;
        if (arg == "--no-adopt") adopt = false;
        if (arg == "--no-encryption") encrypt = false;
        if (arg == "--server-key" && i + 1 < argc) pin = argv[++i];
        if (arg == "--travel" && i + 1 < argc) g_travel = (float)std::atof(argv[++i]);
        if (arg == "--hold" && i + 1 < argc) g_hold = (float)std::atof(argv[++i]);
        if (arg == "--walked-down") g_flown = false;
        if (arg == "--far" && i + 3 < argc) {
            g_far = true;
            for (int k = 0; k < 3; ++k) g_far_at[k] = (float)std::atof(argv[++i]);
        }
    }

    const char *name   = (argc > 4 && argv[4][0] != '-') ? argv[4] : "probe";
    const int   extent = (argc > 5 && argv[5][0] != '-') ? std::atoi(argv[5]) : 128;
    return join_mode(argv[2], (uint16_t)std::atoi(argv[3]), name, extent, wrong_table, adopt,
                     pin, encrypt);
}
