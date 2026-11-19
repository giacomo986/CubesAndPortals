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
//                       [--server-key <hex>] [--stay <seconds>] [--walk <blocks>] [--play]
//
// Exit codes: 0 joined and agreed, 1 failed, 2 refused by the server, 3 this
// client refused what the server sent.

#include "core/net/client_session.h"
#include "core/net/crypto.h"
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
std::string g_room;       // --room NAME: that room of the server, joined straight
float g_hold   = 35.0f;   // --hold: seconds to hover at the end of it
int   g_dig    = 0;       // --dig: blocks to break beside the spawn first (/rollback)
bool  g_pack   = false;   // --pack: wear what is in square 1 on the back, then /packfill
std::vector<std::string> g_say;   // --say TEXT: chat lines (a /command too), before --dig
bool  g_far    = false;   // --far X Y Z: say at the end whether that chunk arrived
float g_far_at[3] = {0.0f, 0.0f, 0.0f};
float g_stay   = 0.0f;    // --stay: seconds to stand at the spawn, saying the sound loops heard
bool  g_play   = false;   // --play: start from where the server puts the player, not the middle
float g_walk   = 0.0f;    // --walk: blocks to walk along +x on the ground, after --stay
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

const char *g_identity = nullptr;

// One pump of the session, and every chat line the server sent since the last,
// on stdout as "chat: <line>" -- for a test that wants to see what it was told.
void pump(ClientSession &client, int timeout_ms)
{
    client.pump(now_seconds(), timeout_ms);
    for (const std::string &line : client.take_chat()) std::printf("chat: %s\n", line.c_str());
}

int join_mode(const char *host, uint16_t port, const char *name, int extent,
              bool claim_wrong_table, bool adopt, const char *pin_hex, bool encrypt)
{
    ClientSession client;
    std::string   err;

    ClientConfig cfg;
    cfg.host    = host;
    cfg.room    = g_room;
    cfg.port    = port;
    cfg.encrypt = encrypt;

    if (pin_hex) {
        if (!from_hex(pin_hex, cfg.server_key, kKeyBytes)) {
            std::printf("--server-key wants 64 hex characters\n");
            return 1;
        }
        cfg.pin_server = true;
    }
    // --identity FILE: a key of its own, made once, so a server can tell the
    // probe from another (bans by key, roles).
    if (g_identity) {
        bool created = false;
        if (!load_or_create_sign_key(g_identity, cfg.identity, created, err)) {
            std::printf("--identity: %s\n", err.c_str());
            return 1;
        }
        cfg.has_identity = true;
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

    while (!client_join_settled(client.state())) pump(client, 10);

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
    float start_x = (float)extent * 0.5f;
    float start_y = (float)kGroundTop + 2.0f;
    float start_z = (float)extent * 0.5f;
    client.send_move(start_x, start_y, start_z, 1.0f, 0.0f, 0.0f, now_seconds());

    // --play: a game's world is not the default terrain and its spawn is not
    // the middle of it -- the server moves the player there. Go where it says,
    // wait for the ground round that, and play from there.
    const double deadline    = now_seconds() + (g_play ? 10.0 : 3.0);
    int          quiet_ticks = 0;
    int          last_count  = 0;
    bool         moved       = false;
    while (now_seconds() < deadline) {
        pump(client, 10);
        if (g_play) {
            Teleport tp;
            Correct  c;
            if (client.take_teleport(tp)) {
                const bool again = moved && tp.x == start_x && tp.y == start_y && tp.z == start_z;
                start_x = tp.x, start_y = tp.y, start_z = tp.z;
                moved = true, quiet_ticks = 0;
                if (!again) std::printf("        put at %.1f %.1f %.1f by the server\n", (double)tp.x, (double)tp.y, (double)tp.z);
            }
            if (client.take_correction(c)) {
                start_x = c.x, start_y = c.y, start_z = c.z;
                moved = true, quiet_ticks = 0;
            }
            // Standing there, said: a placement the client never answers is
            // one the server goes on waiting for.
            static double said = 0.0;
            if (now_seconds() - said >= 0.1) {
                client.send_move(start_x, start_y, start_z, 1.0f, 0.0f, 0.0f, now_seconds());
                said = now_seconds();
            }
        }
        if (client.chunks_received() == last_count) {
            // --play: with nothing arrived yet, the move to the spawn may still be coming.
            if (++quiet_ticks > 60 && (!g_play || (moved && last_count > 0))) break;
        } else {
            quiet_ticks = 0;
            last_count  = client.chunks_received();
        }
    }

    // Under --play, ground is anything solid below the feet, however far down.
    bool ground = client.world().get_block((int)start_x, kGroundTop - 1, (int)start_z) != kAir;
    if (g_play) {
        ground = false;
        for (int gy = (int)start_y; gy >= 0 && !ground; --gy)
            ground = client.world().get_block((int)start_x, gy, (int)start_z) != kAir;
    }
    std::printf("        received %d chunks, ground under the player: %s\n",
                client.chunks_received(), ground ? "solid" : "MISSING");

    // --play asks for a world, not for ground at a particular spot: a game may
    // put the player over a drop it means to be there.
    const bool streamed_ok =
        client.chunks_received() > 0 && client.chunk_error().empty() && (ground || g_play);

    // --pack: three seconds for something to be given, then square 1 onto the
    // back (the fifth worn square) and /packfill, a scratch mod's command that
    // numbers it and fills its container -- a backpack between friend servers.
    if (g_pack && streamed_ok) {
        const auto pump_for = [&](double s) {
            for (const double until = now_seconds() + s; now_seconds() < until;) pump(client, 10);
        };
        pump_for(3.0);
        client.send_move_item(0, Inventory::kDefaultSlots + 4);
        pump_for(1.0);
        client.send_chat("/packfill");
        pump_for(1.0);
        std::printf("        wore square 1 on the back and asked for /packfill\n");
    }

    for (const std::string &line : g_say) {
        if (!streamed_ok) break;
        client.send_chat(line);
        for (const double until = now_seconds() + 0.7; now_seconds() < until;) pump(client, 10);
        std::printf("        said '%s'\n", line.c_str());
    }

    // --dig N: the top of the ground, N cells along +x from under the player,
    // swung at every half second until it comes away (a hard block takes a
    // first swing and its seconds) -- something for /inspect and /rollback.
    for (int i = 0; i < g_dig && streamed_ok; ++i) {
        const int gx = (int)start_x + i, gz = (int)start_z;
        // --play: under a roof, the floor at the feet and not the roof.
        int       gy = g_play ? (int)start_y : kWorldHeight - 1;
        while (gy > 0 && client.world().get_block(gx, gy, gz) == kAir) --gy;
        for (int swing = 0; swing < (g_play ? 6 : 20) && client.world().get_block(gx, gy, gz) != kAir; ++swing) {
            client.send_edit(gx, gy, gz, 0, 1, 0, true, 0);
            for (const double until = now_seconds() + 0.5; now_seconds() < until;) pump(client, 10);
        }
        std::printf("        broke %d %d %d%s%s\n", gx, gy, gz, client.edit_refusal().empty() ? "" : ": ",
                    client.edit_refusal().c_str());
    }

    // --stay N: stand where we came down for N seconds, saying each time the
    // sounds going round (core.sound_loop) change which loops are in it.
    if (g_stay > 0.0f && streamed_ok) {
        const float  y     = start_y;
        const double until = now_seconds() + g_stay;
        double       sent  = 0.0;
        std::string  heard = "-";
        while (now_seconds() < until) {
            pump(client, 10);
            const double now = now_seconds();
            if (now - sent >= 0.05) {
                client.send_move(start_x, y, start_z, 1.0f, 0.0f, 0.0f, now);
                sent = now;
            }
            const auto &all = client.sound_loops();
            std::string loops;
            for (const SoundLoop &l : all) loops += " " + std::to_string(l.id) + ":" + l.name;
            if (loops == heard) continue;
            heard = loops;
            std::printf("        sound loops:%s", loops.empty() ? " none" : loops.c_str());
            if (!all.empty())
                std::printf(" (the first at %.1f %.1f %.1f, range %.0f, gain %.2f; we stand at %.1f %.1f)",
                            (double)all[0].x, (double)all[0].y, (double)all[0].z, (double)all[0].range,
                            (double)all[0].gain, (double)start_x, (double)start_z);
            std::printf("\n");
        }
    }

    // --walk N: N blocks along +x at a walking pace, on top of whatever ground
    // this client holds, never claiming to fly -- what a body that cannot fly
    // does, for the mods that watch a player move. Takes the server's
    // corrections and teleports and goes on from there; gives up after a
    // while rather than walking into a wall for ever.
    if (g_walk > 0.0f && streamed_ok) {
        constexpr float kPace = 4.0f;
        float           x = start_x, z = start_z, walked = 0.0f;
        float           y = start_y;
        int             corrections = 0;
        double          t_prev = now_seconds();
        const double    give_up = t_prev + g_walk / kPace + 5.0;
        while (walked < g_walk && now_seconds() < give_up) {
            pump(client, 5);
            const double now = now_seconds();
            if (now - t_prev < 0.05) continue;
            const float step = std::min(g_walk - walked, kPace * (float)(now - t_prev));
            t_prev           = now;
            x += step;
            walked += step;
            int gy = kWorldHeight - 1;
            while (gy > 0 && client.world().get_block((int)x, gy, (int)z) == kAir) --gy;
            // Up a step no higher than a jump, down any; never through a wall.
            if ((float)(gy + 1) <= y + 1.0f) y = (float)(gy + 1);
            Correct c;
            if (client.take_correction(c)) {
                ++corrections;
                x = c.x; y = c.y; z = c.z;
            }
            Teleport tp;
            if (client.take_teleport(tp)) { x = tp.x; y = tp.y; z = tp.z; }
            client.send_move(x, y, z, 1.0f, 0.0f, 0.0f, now);
        }
        std::printf("        walked %.0f blocks to %.1f %.1f %.1f, %d correction(s)\n",
                    (double)walked, (double)x, (double)y, (double)z, corrections);
    }

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
            pump(client, 5);
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

    pump(client, 0);   // the chat lines of the last moment
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
        if (arg == "--stay" && i + 1 < argc) g_stay = (float)std::atof(argv[++i]);
        if (arg == "--no-adopt") adopt = false;
        if (arg == "--no-encryption") encrypt = false;
        if (arg == "--server-key" && i + 1 < argc) pin = argv[++i];
        if (arg == "--identity" && i + 1 < argc) g_identity = argv[++i];
        if (arg == "--play") g_play = true;
        if (arg == "--room" && i + 1 < argc) g_room = argv[++i];
        if (arg == "--walk" && i + 1 < argc) g_walk = (float)std::atof(argv[++i]);
        if (arg == "--travel" && i + 1 < argc) g_travel = (float)std::atof(argv[++i]);
        if (arg == "--hold" && i + 1 < argc) g_hold = (float)std::atof(argv[++i]);
        if (arg == "--dig" && i + 1 < argc) g_dig = std::atoi(argv[++i]);
        if (arg == "--pack") g_pack = true;
        if (arg == "--say" && i + 1 < argc) g_say.push_back(argv[++i]);
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
