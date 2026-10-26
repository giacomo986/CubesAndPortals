// The protocol, tested without a socket.
//
// DESIGN.md 3.2 asks for "header versionato + tabella di feature negoziate
// all'handshake, così un client nuovo parla con un server vecchio". That is a
// claim about rules, not about UDP, so it is tested on buffers: an old server
// and a new client are two structs in this process, and the awkward pairings
// that are a scheduling problem over a real network are a loop here.
//
// The other half is that every byte the parser sees came from a peer that may
// not be running our code. So the last three sections do not test features at
// all -- they hand the parser truncated, corrupted and random input and require
// that it refuses without crashing. Run under ASan and UBSan too; see the
// finding note in the M3 doc for what that run reported.

#include <cstring>
#include "core/net/protocol.h"
#include "core/net/session.h"
#include "core/ui/panel.h"
#include "core/world/daylight.h"

#include <cmath>
#include <cstdio>
#include <limits>
#include <string>
#include <vector>

using namespace cnp;
using namespace cnp::net;

namespace {

int g_failures = 0;

void check(bool cond, const char *what)
{
    std::printf(cond ? "  ok    %s\n" : "  FAIL  %s\n", what);
    if (!cond) ++g_failures;
}

// A deterministic generator, so a failure found by the fuzz section can be
// reproduced by running the test again rather than by being lucky twice.
struct Rng {
    uint64_t s;
    explicit Rng(uint64_t seed) : s(seed ? seed : 1) {}
    uint32_t next()
    {
        s ^= s << 13;
        s ^= s >> 7;
        s ^= s << 17;
        return (uint32_t)(s >> 32);
    }
};

// One number stands in for "this build's block table" throughout. The real one
// comes from registry_content_hash; what matters here is only that both sides
// derive it the same way and that a difference is refused.
constexpr uint64_t kTableHash = 0x00c0ffee00c0ffeeull;

Hello make_hello()
{
    Hello h;
    h.player_name    = "giacomo";
    h.engine_version = "0.3.0-m3";
    h.content_hash   = kTableHash;
    h.features       = {"portals", "zstd-chunks", "entity-lerp"};
    return h;
}

ServerConfig make_server()
{
    ServerConfig c;
    c.engine_version = "0.3.0-m3";
    c.content_hash   = kTableHash;
    c.features     = {"zstd-chunks", "portals", "server-only-thing"};
    c.mod_channels = {"yaportal:link", "beacons:sync"};
    c.seed         = 1;
    c.mapgen_hash  = 0x5b194e88b8935d1bull;
    return c;
}

} // namespace

int main()
{
    std::printf("encoding\n");
    {
        const Hello in  = make_hello();
        const auto  buf = in.encode();

        Hello      out;
        ByteReader r(buf);
        check(Hello::decode(r, out), "a hello survives a round trip");
        check(out.player_name == in.player_name && out.features == in.features &&
                  out.protocol == in.protocol && out.magic == kMagic,
              "and comes back with the same fields");

        Welcome w;
        w.protocol   = 1;
        w.seed       = 7;
        w.mapgen_hash = 0xdeadbeefcafef00dull;
        w.features   = {"portals"};
        w.channels   = {{"yaportal:link", kModChannelBase}};
        // The model table, in id order the way the block table is. A body's
        // `model` is a position in here PLUS ONE, so that 0 can mean "nothing
        // said" without the first model losing its id -- and the table itself
        // is only the names: what a model is made of travels as files.
        w.models     = {"core:person", "mymod:crab"};

        // The block table, and the shapes of those blocks that are not plain
        // cubes (TODO.md OPEN-43). A shape names the id it belongs to, because
        // it is sent for the handful of blocks that have one and not for the
        // thousands that do not.
        w.nodes = {"core:air", "core:stone", "steps:stair"};
        NodeShapeDesc stair;
        stair.node = 2;
        stair.turn = 3;   // facedir
        stair.boxes.resize(2);
        stair.boxes[0].max[1] = 0.5f;
        stair.boxes[1].min[1] = 0.5f;
        stair.boxes[1].max[2] = 0.5f;
        stair.boxes[1].part   = "top";

        // And what it is DRAWN as, when that is not its boxes: a position in
        // this welcome's model table plus one, the same number a body carries.
        // A block wearing a model is the other half of M11's drawtype pair.
        stair.model = 2;
        w.shapes = {stair};

        const auto wbuf = w.encode();

        Welcome    wout;
        ByteReader wr(wbuf);
        check(Welcome::decode(wr, wout), "a welcome survives a round trip");
        check(wout.seed == 7 && wout.mapgen_hash == 0xdeadbeefcafef00dull &&
                  wout.channels.size() == 1 && wout.channels[0].id == kModChannelBase,
              "including the seed, the world hash and the channel table");
        check(wout.models.size() == 2 && wout.models[0] == "core:person" &&
                  wout.models[1] == "mymod:crab",
              "and the model table, in the order that gives each model its id");

        check(wout.shapes.size() == 1 && wout.shapes[0].node == 2 && wout.shapes[0].turn == 3 &&
                  wout.shapes[0].boxes.size() == 2,
              "and a block's shape, against the id it belongs to");

        {
            Edit e;
            e.ny = 1; e.turn = 3; e.place_only = true;
            const auto bytes = e.encode();
            ByteReader r(bytes);
            Edit       back;
            check(Edit::decode(r, back) && back.turn == 3 && back.place_only,
                  "an Edit keeps its turn and its Shift beside it in one byte");
        }

        // The coordinates exactly, not nearly: the collision skin is a
        // ten-thousandth of a node, so a wire that rounded a box would put the
        // two ends further apart than the tolerance they judge a position by.
        check(wout.shapes.size() == 1 && wout.shapes[0].boxes[0].max[1] == 0.5f &&
                  wout.shapes[0].boxes[1].min[1] == 0.5f &&
                  wout.shapes[0].boxes[1].max[2] == 0.5f,
              "with its corners unrounded");
        check(wout.shapes.size() == 1 && wout.shapes[0].boxes[1].part == "top" &&
                  wout.shapes[0].boxes[0].part.empty(),
              "and the name that makes a box a piece that moves");

        check(wout.shapes.size() == 1 && wout.shapes[0].model == 2,
              "and the model it is drawn as, if it is drawn as one");

        // How a body moves in the world (TODO.md OPEN-44), which every client
        // is told and moves by -- so exactly, like the corners, and refused
        // when it is something a body cannot be moved by. A welcome is a
        // message a stranger can send.
        {
            Welcome race = w;
            race.movement.walk    = 12.5f;
            race.movement.sprint  = 60.25f;
            race.movement.fly     = 20.0f;
            race.movement.jump    = 10.0f;
            race.movement.gravity = -40.0f;
            const auto mbuf = race.encode();
            Welcome    mout;
            ByteReader mr(mbuf);
            check(Welcome::decode(mr, mout) && mout.movement.walk == 12.5f &&
                      mout.movement.sprint == 60.25f && mout.movement.fly == 20.0f &&
                      mout.movement.jump == 10.0f && mout.movement.gravity == -40.0f,
                  "and how a body moves there, every number unrounded");

            struct Spoilt {
                const char   *what;
                MovementRules rules;
            };
            Spoilt cases[4];
            cases[0].what        = "a welcome with a speed that is not a number is refused";
            cases[0].rules.walk  = std::numeric_limits<float>::quiet_NaN();
            cases[1].what        = "a welcome with a negative speed is refused";
            cases[1].rules.sprint = -1.0f;
            cases[2].what        = "a welcome with a speed over what a world may declare is refused";
            cases[2].rules.fly   = kMaxDeclaredSpeed + 1.0f;
            cases[3].what        = "a welcome whose gravity points up is refused";
            cases[3].rules.gravity = 0.5f;
            for (const Spoilt &c : cases) {
                Welcome bad = w;
                bad.movement = c.rules;
                const auto bbuf = bad.encode();
                Welcome    bout;
                ByteReader brd(bbuf);
                check(!Welcome::decode(brd, bout), c.what);
            }

            // And the same five numbers for ONE body, whenever a mod gives it
            // some (TODO.md OPEN-47): the same round trip and the same refusals,
            // because it is the same thing arriving by another road.
            BodyMovement fast;
            fast.rules = race.movement;
            const auto   fbuf = fast.encode();
            BodyMovement fout;
            ByteReader   fr(fbuf);
            check(BodyMovement::decode(fr, fout) && fout.rules.sprint == 60.25f &&
                      fout.rules.gravity == -40.0f && fout.rules.walk == 12.5f,
                  "one body's movement round trips, every number unrounded");
            {
                // And its dig factor (protocol 82): kept, and refused out of range.
                BodyMovement dug;
                dug.rules = race.movement;
                dug.dig   = 0.8f;
                const auto   dbuf = dug.encode();
                BodyMovement dout;
                ByteReader   dr(dbuf);
                check(BodyMovement::decode(dr, dout) && dout.dig == 0.8f, "a body's dig factor round trips");
                dug.dig = 0.0f;
                const auto zbuf = dug.encode();
                ByteReader zr(zbuf);
                check(!BodyMovement::decode(zr, dout), "and a dig factor of 0 is refused");
            }
            for (const Spoilt &c : cases) {
                BodyMovement bad;
                bad.rules       = c.rules;
                const auto bbuf = bad.encode();
                BodyMovement bout;
                ByteReader   brd(bbuf);
                check(!BodyMovement::decode(brd, bout),
                      "and one body's movement is refused for the same spoilt number");
            }
        }

        // A shape for a block that is not in the table it arrived with. The id
        // is what the mesher and the collision index an array by, and a table
        // of three cannot answer for a fourth.
        {
            Welcome bad_id = w;
            bad_id.shapes[0].node = 9;
            const auto  bad_buf = bad_id.encode();
            Welcome     out_bad;
            ByteReader  br(bad_buf);
            check(!Welcome::decode(br, out_bad),
                  "a shape for a block the table does not have is refused");
        }

        Refuse ref;
        ref.reason = RefuseReason::ServerFull;
        ref.detail = "8/8 players";
        const auto rbuf = ref.encode();

        Refuse     refout;
        ByteReader rr(rbuf);
        check(Refuse::decode(rr, refout), "a refusal survives a round trip");
        check(refout.reason == RefuseReason::ServerFull && refout.detail == "8/8 players",
              "and carries a reason a player can act on");

        check(peek_type(buf.data(), buf.size()) == MsgType::Hello &&
                  peek_type(wbuf.data(), wbuf.size()) == MsgType::Welcome,
              "a dispatcher can read the type without parsing the message");
        check(peek_type(nullptr, 0) == MsgType::Invalid,
              "and an empty buffer is Invalid, not a read of byte zero");

        // Byte order is written out by hand precisely so this holds on a
        // big-endian machine as well; the check is here so a "clever"
        // memcpy of a struct cannot pass.
        check(buf[1] == 'C' && buf[2] == 'N' && buf[3] == 'P' && buf[4] == '1',
              "the magic is little endian on the wire, whatever the host is");
    }

    std::printf("\nversion negotiation\n");
    {
        // A new client against an old server: the design's stated case.
        Hello newer   = make_hello();
        newer.protocol     = 5;
        newer.min_protocol = 1;

        ServerConfig old_server = make_server();
        old_server.protocol     = 2;
        old_server.min_protocol = 1;

        const HandshakeResult r = negotiate(newer, old_server);
        check(r.accepted, "a new client is accepted by an old server");
        check(r.welcome.protocol == 2, "and they speak the older of the two versions");

        // The other direction.
        Hello old_client        = make_hello();
        old_client.protocol     = 2;
        old_client.min_protocol = 1;

        ServerConfig new_server = make_server();
        new_server.protocol     = 5;
        new_server.min_protocol = 1;

        const HandshakeResult r2 = negotiate(old_client, new_server);
        check(r2.accepted && r2.welcome.protocol == 2,
              "an old client is accepted by a new server, at the old version");

        // No overlap, in both directions, with a message that says which side
        // has to move.
        ServerConfig moved_on = make_server();
        moved_on.protocol     = 9;
        moved_on.min_protocol = 8;

        const HandshakeResult r3 = negotiate(old_client, moved_on);
        check(!r3.accepted && r3.reason == RefuseReason::VersionTooOld,
              "a client below the server's floor is refused as too old");
        check(r3.detail.find("2") != std::string::npos &&
                  r3.detail.find("8") != std::string::npos,
              "and told both numbers, not just that it failed");

        Hello from_future        = make_hello();
        from_future.protocol     = 9;
        from_future.min_protocol = 8;

        const HandshakeResult r4 = negotiate(from_future, old_server);
        check(!r4.accepted && r4.reason == RefuseReason::VersionTooNew,
              "and a server below the client's floor is refused as the old one");

        Hello inverted        = make_hello();
        inverted.protocol     = 1;
        inverted.min_protocol = 5;
        check(!negotiate(inverted, make_server()).accepted,
              "an inverted version range is refused, not arithmetic'd around");

        Hello wrong_magic  = make_hello();
        wrong_magic.magic  = 0x59415054u;   // the fork's magic, as it happens
        const HandshakeResult r5 = negotiate(wrong_magic, make_server());
        check(!r5.accepted && r5.reason == RefuseReason::BadMagic,
              "another protocol on the same port is refused by the first four bytes");
    }

    std::printf("\nfeature negotiation\n");
    {
        const HandshakeResult r = negotiate(make_hello(), make_server());
        check(r.accepted, "the ordinary case is accepted");

        const std::vector<std::string> expected = {"zstd-chunks", "portals"};
        check(r.welcome.features == expected,
              "the agreed set is the intersection, in the server's order");
        check(r.welcome.features.size() == 2,
              "so a feature only one side knows is simply not enabled");

        // Determinism of the order, which is the part that bites later: both
        // sides index this table.
        Hello shuffled     = make_hello();
        shuffled.features  = {"entity-lerp", "portals", "zstd-chunks"};
        check(negotiate(shuffled, make_server()).welcome.features == expected,
              "and the client's ordering does not change the result");

        ServerConfig demanding      = make_server();
        demanding.required_features = {"zstd-chunks", "libsodium"};
        const HandshakeResult r2 = negotiate(make_hello(), demanding);
        check(!r2.accepted && r2.reason == RefuseReason::MissingFeature,
              "a feature the server requires and the client lacks is a refusal");
        check(r2.detail == "libsodium", "naming the feature, so it can be installed");

        Hello lots = make_hello();
        for (int i = 0; i < 200; ++i) lots.features.push_back("f" + std::to_string(i));
        const auto lots_buf = lots.encode();
        Hello      lots_out;
        ByteReader lots_reader(lots_buf);
        check(Hello::decode(lots_reader, lots_out) && lots_out.features.size() == kMaxFeatures,
              "a client with more features than the protocol carries is capped, not refused");
    }

    std::printf("\nsame protocol, different content\n");
    {
        // The mismatch a version number does not catch. Both sides speak
        // protocol 1 and call themselves the same build; one of them has a
        // block the other has not, so their ids differ from that point on.
        Hello other_build = make_hello();
        other_build.content_hash ^= 0x1ull;

        const HandshakeResult r = negotiate(other_build, make_server());
        check(!r.accepted && r.reason == RefuseReason::ContentMismatch,
              "a client with a different block table is refused");
        check(r.detail.find("different blocks or different mods") != std::string::npos,
              "and told what kind of difference it is, not just that it failed");

        // Both sides check. The server's check protects its world from a client
        // that would misread it; the client's protects the player from a server
        // that did not check.
        const Hello           sent = make_hello();
        const HandshakeResult ok   = negotiate(sent, make_server());
        Welcome               tampered = ok.welcome;
        tampered.content_hash ^= 0x1ull;
        std::string err;
        check(!accept_welcome(tampered, sent, err),
              "and the client refuses a server whose table is not its own");

        // Two versions with the same string but different tables: refused. Same
        // table with different strings: allowed, because the string is not what
        // anything breaks on.
        Hello renamed          = make_hello();
        renamed.engine_version = "0.9.9-somebody's-branch";
        check(negotiate(renamed, make_server()).accepted,
              "while a different version string with the same table is fine");
    }

    std::printf("\nchannels\n");
    {
        const HandshakeResult r = negotiate(make_hello(), make_server());
        check(r.welcome.channels.size() == 2, "each mod channel is assigned");
        check(r.welcome.channels[0].id == kModChannelBase &&
                  r.welcome.channels[1].id == kModChannelBase + 1,
              "from kModChannelBase up, leaving the engine's own alone");

        // Every engine channel is below the first mod one, checked by name
        // rather than by a number written twice. A channel added to the enum
        // and not accounted for here is a channel a mod is also handed.
        check((uint8_t)Channel::Handshake < kModChannelBase &&
                  (uint8_t)Channel::Chunk < kModChannelBase &&
                  (uint8_t)Channel::Entity < kModChannelBase &&
                  (uint8_t)Channel::Event < kModChannelBase &&
                  (uint8_t)Channel::Media < kModChannelBase &&
                  (uint8_t)Channel::Block < kModChannelBase,
              "and no engine channel is at or above the first one a mod is given");

        // The one that carries block changes is NOT the one that carries the
        // heartbeat, and this is the rule bad_link measured: mods write
        // continuously, ENet orders reliable messages within a channel, and a
        // lost block change on the event channel holds the heartbeat behind it.
        // Four reds in five runs, thirteen lost heartbeats, a 4.09 s gap.
        check((uint8_t)Channel::Block != (uint8_t)Channel::Event,
              "a block change does not travel on the channel the heartbeat is on");
        check(r.welcome.channels[0].name == "yaportal:link",
              "and a mod finds its channel by name, not by a hardcoded number");

        ServerConfig too_many = make_server();
        too_many.mod_channels.clear();
        for (int i = 0; i < 40; ++i) too_many.mod_channels.push_back("mod" + std::to_string(i));
        const HandshakeResult r2 = negotiate(make_hello(), too_many);
        check(!r2.accepted,
              "more mod channels than the protocol has is refused, not truncated");

        // A server that hands out channel 1 would collide with chunk streaming.
        // The client is the side that indexes with this number, so it checks.
        Welcome bad;
        bad.channels = {{"evil:mod", 1}};
        const auto badbuf = bad.encode();
        Welcome    out;
        ByteReader br(badbuf);
        check(!Welcome::decode(br, out),
              "a welcome assigning an engine channel to a mod is rejected by the client");
    }

    std::printf("\nthe client checks the answer\n");
    {
        const Hello           sent = make_hello();
        const HandshakeResult r    = negotiate(sent, make_server());
        std::string           err;
        check(accept_welcome(r.welcome, sent, err), "an honest welcome is accepted");

        Welcome lying = r.welcome;
        lying.features.push_back("kick-everyone");
        check(!accept_welcome(lying, sent, err),
              "a server cannot enable a feature the client never offered");

        Welcome wrong_version = r.welcome;
        wrong_version.protocol = kProtocol + 1;   // newer than either side speaks
        check(!accept_welcome(wrong_version, sent, err),
              "nor pick a version outside the range the client offered");

        Welcome dup = r.welcome;
        dup.channels.push_back(ChannelAssignment{"other:mod", dup.channels[0].id});
        check(!accept_welcome(dup, sent, err),
              "nor hand the same channel to two mods");

        Welcome bad_magic = r.welcome;
        bad_magic.magic   = 0;
        check(!accept_welcome(bad_magic, sent, err), "and the magic is checked here too");
    }

    std::printf("\nwhere the player says it is\n");
    {
        Move m;
        m.seq = 42;
        m.x = 1.5f; m.y = -2.25f; m.z = 3.75f;
        m.look_x = 0.0f; m.look_y = 0.0f; m.look_z = -1.0f;

        const auto buf = m.encode();
        Move       out;
        ByteReader r(buf);
        check(Move::decode(r, out), "a move survives a round trip");
        check(out.seq == 42 && out.x == 1.5f && out.z == 3.75f,
              "with the numbers it was given");

        // What kind of claim it is, which the server needs to know before it
        // decides whether to check it against a world. Default is a body: a
        // claim that says nothing is a claim from a player.
        check(out.scope == (uint8_t)Scope::Enter, "and by default it is a body saying where it is");
        {
            Move       window = m;
            window.scope      = (uint8_t)Scope::Observe;
            const auto wb     = window.encode();
            Move       wo;
            ByteReader wr(wb);
            check(Move::decode(wr, wo) && wo.scope == (uint8_t)Scope::Observe,
                  "and a window's claim says so, all the way across");

            // A kind this build cannot place. Refused rather than guessed:
            // taken as a body it would be checked, taken as a window it would
            // move an interest centre.
            Move       nonsense = m;
            nonsense.scope      = 7;
            const auto nb       = nonsense.encode();
            Move       no;
            ByteReader nr(nb);
            check(!Move::decode(nr, no), "and a kind of claim this build does not know is refused");

            // Flying, which is what keeps a flight down from being a fall.
            Move       flown = m;
            flown.flags      = kMoveFlying;
            const auto fb    = flown.encode();
            Move       fo;
            ByteReader fr(fb);
            check(Move::decode(fr, fo) && fo.flags == kMoveFlying && out.flags == 0,
                  "a flown claim says so, and one that says nothing was walked");

            Move       unknown = m;
            unknown.flags      = 2;
            const auto ub      = unknown.encode();
            Move       uo;
            ByteReader ur(ub);
            check(!Move::decode(ur, uo), "and a flag this build does not know is refused");
        }

        // NaN and infinity are what a float field carries when somebody is
        // having a go at you. A distance of NaN sorts wherever the sort happens
        // to put it and never leaves the queue.
        for (const float bad : {std::numeric_limits<float>::quiet_NaN(),
                                std::numeric_limits<float>::infinity(),
                                -std::numeric_limits<float>::infinity(), 1e30f}) {
            Move hostile = m;
            hostile.x    = bad;
            const auto hb = hostile.encode();
            Move       ho;
            ByteReader hr(hb);
            check(!Move::decode(hr, ho), "a position that is not a number is refused");
        }

        std::vector<uint8_t> truncated = buf;
        truncated.pop_back();
        Move       to;
        ByteReader tr(truncated);
        check(!Move::decode(tr, to), "and so is a truncated one");
    }

    std::printf("\nwhat a cell is now, and which way round\n");
    {
        // A cell that changed carries the way it is TURNED, and not only what
        // block it holds. Without it a block with a front is placed facing the
        // wrong way and stays that way until the whole chunk happens to be sent
        // again -- which for a cell in a chunk the player is standing in may be
        // never.
        BlockChange bc;
        bc.x = -7; bc.y = 20; bc.z = 300;
        bc.id = 42;
        bc.facing = 3;

        const auto  buf = bc.encode();
        BlockChange out;
        ByteReader  r(buf);
        check(BlockChange::decode(r, out), "a changed cell survives a round trip");
        check(out.x == -7 && out.y == 20 && out.z == 300 && out.id == 42 && out.facing == 3,
              "with the cell, the block AND which way it faces");

        // The refusal is the same message with a sentence in it, and it carries
        // the facing of the block that is still there for the same reason: it
        // is what puts back a cell the client drew optimistically.
        BlockChange refused;
        refused.x = 1; refused.y = 2; refused.z = 3;
        refused.id = 9;
        refused.facing = 1;
        refused.why = "a mod said no";

        const auto  rbuf2 = refused.encode();
        BlockChange rout;
        ByteReader  rr(rbuf2);
        check(BlockChange::decode(rr, rout), "and so does a refusal");
        check(rout.facing == 1 && rout.why == refused.why,
              "with the sentence and the facing of what is still there");

        // Four quarter turns and nothing else. A byte that arrives holding 200
        // is a client's cell turned to a face that does not exist, and the
        // mesher would index a permutation with it.
        BlockChange wild;
        wild.facing = 200;
        const auto  wbuf2 = wild.encode();
        BlockChange wout;
        ByteReader  wr(wbuf2);
        check(!BlockChange::decode(wr, wout), "and a facing that is not a quarter turn is refused");
    }

    // A cell somebody has open. TODO OPEN-32: a chest with a lid looks open to
    // the whole room, so it is state of the CELL -- but of a moment and not of
    // a world, which is why it is a message of its own and not a field on the
    // block or a byte in the chunk.
    {
        CellOpen co;
        co.x = 41; co.y = -20; co.z = 47;
        co.open = 1;

        const auto buf = co.encode();
        CellOpen   out;
        ByteReader r(buf);
        check(CellOpen::decode(r, out), "an opened cell survives a round trip");
        check(out.x == 41 && out.y == -20 && out.z == 47 && out.open == 1,
              "with the cell it is about and the fact that it is open");

        CellOpen shut;
        shut.x = 1; shut.y = 2; shut.z = 3;
        const auto  sbuf = shut.encode();
        CellOpen    sout;
        ByteReader  sr(sbuf);
        check(CellOpen::decode(sr, sout) && sout.open == 0,
              "and closing one is the same message saying so");

        // One or nothing. A byte holding 200 is "open" to a test that asks
        // `!= 0` and "shut" to one that asks `== 1`, and the two live in
        // different files.
        //
        // Patched in the BYTES and not set on the struct: the encoder writes
        // `open ? 1 : 0`, so a wild value set here would travel as a tidy 1 and
        // this would be a check on the encoder rather than on what arrives off
        // a wire nobody here controls. The first version of it did exactly
        // that and passed.
        std::vector<uint8_t> wbuf = co.encode();
        wbuf.back()              = 200;
        CellOpen    wout2;
        ByteReader  wr2(wbuf);
        check(!CellOpen::decode(wr2, wout2), "and anything but open or shut is refused");
    }

    std::printf("\nthe server's answer, and everybody else\n");
    {
        Correct c;
        c.seq = 9;
        c.x = 10.0f; c.y = 20.0f; c.z = 30.0f;
        c.reason = "moved 400 blocks sideways in 0.05s";
        c.nitro  = 0.25f;

        const auto buf = c.encode();
        Correct    out;
        ByteReader r(buf);
        check(Correct::decode(r, out), "a correction survives a round trip");
        check(out.seq == 9 && out.y == 20.0f && out.reason == c.reason && out.nitro == 0.25f,
              "and says which claim it answers, why, and the server's nitro");

        // A teleport is not a correction with extra fields: it carries the
        // view and the velocity because sending those apart leaves a frame in
        // the new place with the old orientation.
        Teleport tp;
        tp.seq = 11;
        tp.x = 60.5f; tp.y = 21.0f; tp.z = 44.5f;
        tp.look_x = 1.0f; tp.look_y = 0.0f; tp.look_z = 0.0f;
        tp.vx = 3.0f; tp.vy = -1.0f; tp.vz = 2.0f;
        tp.roll = -1.5707964f;
        tp.reason = "portal";

        const auto tbuf = tp.encode();
        Teleport   tout;
        ByteReader tr(tbuf);
        check(Teleport::decode(tr, tout), "a teleport survives a round trip");
        check(tout.seq == 11 && tout.x == 60.5f && tout.look_x == 1.0f && tout.vz == 2.0f &&
                  std::fabs(tout.roll - tp.roll) < 1e-6f,
              "with the position, the view, the velocity and the roll all in it");

        // The look lands straight in the camera basis, so it is checked rather
        // than trusted: a zero vector collapses the basis and a client that
        // sent one would take the view with it.
        Teleport bad = tp;
        bad.look_x = 0.0f; bad.look_y = 0.0f; bad.look_z = 0.0f;
        const auto bbuf = bad.encode();
        Teleport   bout;
        ByteReader br(bbuf);
        check(!Teleport::decode(br, bout), "and a look that is not a direction is refused");

        // A placement carries a block and a face and nothing else. What it
        // does NOT carry is the point: there is no field for where the opening
        // goes, so a modified client has nothing to put a portal in mid-air
        // with.
        // ------------------------------------------------- the player's identity
        //
        // A key and a signature over the session's binding. Checked here with
        // no socket, because what makes a proof worth anything is arithmetic
        // rather than networking: the same message, key and signature give the
        // same answer in a test as on a wire.
        {
            SignKeyPair player;
            check(SignKeyPair::generate(player), "a player can be given an identity");

            uint8_t binding[32];
            for (int i = 0; i < 32; ++i) binding[i] = (uint8_t)(i * 7 + 1);

            const std::vector<uint8_t> challenge = identity_challenge(binding, "alice");
            uint8_t signature[kSignatureBytes];
            sign_message(player, challenge.data(), challenge.size(), signature);

            check(verify_message(player.public_key, challenge.data(), challenge.size(), signature),
                  "and prove it over a challenge");

            // The three ways a proof stops meaning anything, and each one is a
            // real attack rather than a hypothetical. A different session is a
            // replay; a different name is a proof lifted into somebody else's
            // hello; a different key is wearing a stranger's identity.
            uint8_t other_binding[32];
            for (int i = 0; i < 32; ++i) other_binding[i] = (uint8_t)(i * 7 + 2);
            const std::vector<uint8_t> other_session = identity_challenge(other_binding, "alice");
            check(!verify_message(player.public_key, other_session.data(), other_session.size(),
                                  signature),
                  "a proof from one session does not answer another");

            const std::vector<uint8_t> other_name = identity_challenge(binding, "mallory");
            check(!verify_message(player.public_key, other_name.data(), other_name.size(),
                                  signature),
                  "and a proof for one name does not answer another");

            SignKeyPair stranger;
            SignKeyPair::generate(stranger);
            check(!verify_message(stranger.public_key, challenge.data(), challenge.size(),
                                  signature),
                  "and a key nobody signed with does not verify");

            // The seed is what goes on disk, so the same seed has to give the
            // same identity -- otherwise a player's key changes every restart
            // and the identity is a name again.
            uint8_t seed[kSignSeedBytes];
            for (int i = 0; i < (int)kSignSeedBytes; ++i) seed[i] = (uint8_t)(i + 3);
            SignKeyPair from_file_a, from_file_b;
            SignKeyPair::from_seed(seed, from_file_a);
            SignKeyPair::from_seed(seed, from_file_b);
            check(std::memcmp(from_file_a.public_key, from_file_b.public_key,
                              kSignPublicBytes) == 0,
                  "and a seed read back off disk is the same identity");

            // On the wire. Half an identity is refused: a key with no proof is
            // a claim anybody can make, and a proof with no key is 64 bytes of
            // nothing.
            Hello h = make_hello();
            h.identity.assign(player.public_key, player.public_key + kSignPublicBytes);
            h.proof.assign(signature, signature + kSignatureBytes);

            const auto hbuf = h.encode();
            Hello      hout;
            ByteReader hr(hbuf);
            check(Hello::decode(hr, hout) && hout.identity == h.identity &&
                      hout.proof == h.proof,
                  "a hello carries the identity and the proof through a round trip");

            Hello lonely = h;
            lonely.proof.clear();
            const auto lbuf = lonely.encode();
            Hello      lout;
            ByteReader lr(lbuf);
            check(!Hello::decode(lr, lout), "a key with no proof is refused");

            Hello orphan = h;
            orphan.identity.clear();
            const auto obuf = orphan.encode();
            Hello      oout;
            ByteReader orr(obuf);
            check(!Hello::decode(orr, oout), "and so is a proof with no key");
        }

        PlacePortal pp;
        pp.which = 1;
        pp.x = 38; pp.y = 20; pp.z = 52;
        pp.nx = 1; pp.ny = 0; pp.nz = 0;

        const auto  pbuf = pp.encode();
        PlacePortal pout;
        ByteReader  pr(pbuf);
        check(PlacePortal::decode(pr, pout), "a placement survives a round trip");
        check(pout.which == 1 && pout.x == 38 && pout.z == 52 && pout.nx == 1,
              "with the block it hit and the face of it");

        // Six faces exist. A diagonal is not one of them, and neither is a zero
        // vector -- which would put the opening inside the block it was placed
        // on rather than in front of it.
        PlacePortal diagonal = pp;
        diagonal.ny = 1;
        const auto  dbuf = diagonal.encode();
        PlacePortal dout;
        ByteReader  dr(dbuf);
        check(!PlacePortal::decode(dr, dout), "a face no cube has is refused");

        PlacePortal nowhere = pp;
        nowhere.nx = 0;
        const auto  facebuf = nowhere.encode();
        PlacePortal faceout;
        ByteReader  facer(facebuf);
        check(!PlacePortal::decode(facer, faceout), "and so is no face at all");

        PlacePortal third = pp;
        third.which = 2;
        const auto  tbuf3 = third.encode();
        PlacePortal t3out;
        ByteReader  tr3(tbuf3);
        check(!PlacePortal::decode(tr3, t3out),
              "and a third opening, which would be a write past the end of a pair");

        // The openings coming back the other way. A bitmask rather than a
        // count, because a lone SECOND opening sent as a list of one would
        // arrive as the first and be drawn on the wrong wall.
        Portals ps;
        ps.version = 4;
        ps.present = 0x2;
        ps.opening[1].cx = 45.5f;
        ps.opening[1].cy = 21.0f;
        ps.opening[1].cz = 52.5f;
        ps.opening[1].axis = 1;
        ps.opening[1].normal_sign = -1;
        ps.opening[1].rot = 3;

        const auto  psbuf = ps.encode();
        Portals     psout;
        ByteReader  psr(psbuf);
        check(Portals::decode(psr, psout), "half a pair survives a round trip");
        check(!psout.has(0) && psout.has(1) && psout.opening[1].cx == 45.5f &&
                  psout.opening[1].normal_sign == -1 && psout.opening[1].rot == 3,
              "and it is still the second one, facing the way it faced");
        check(!psout.complete(), "and it is not a pair anybody can walk through");

        // Every field here lands in the renderer's basis or the body's
        // transform. An axis of 7 indexes off the end of the three that exist.
        Portals bad_axis = ps;
        bad_axis.opening[1].axis = 7;
        const auto  babuf = bad_axis.encode();
        Portals     baout;
        ByteReader  bar(babuf);
        check(!Portals::decode(bar, baout), "an axis that does not exist is refused");

        // Where a pair LEADS, when it leads to another server. On the link and
        // not on an opening: the first opening is the mouth here, and this is
        // the world on the other side of it.
        Portals away = ps;
        away.present   = 0x1;
        away.opening[0] = ps.opening[1];
        away.away[0].host = "portal.example";
        away.away[0].port = 25301;

        const auto  abuf = away.encode();
        Portals     aout;
        ByteReader  ar(abuf);
        check(Portals::decode(ar, aout), "a pair that leads away survives a round trip");
        check(aout.leads_away() && aout.away[0].host == "portal.example" &&
                  aout.away[0].port == 25301,
              "with the world it leads to");
        check(aout.complete(),
              "and one opening is a whole mouth: what is behind it is not this server's to send");

        // Two mouths onto two worlds (protocol 34), and the shape that is
        // neither: a second mouth beside a first opening that leads nowhere.
        Portals two = away;
        two.present        = 0x3;
        two.opening[1]     = ps.opening[1];
        two.away[1].host   = "track.example";
        two.away[1].port   = 25303;
        two.away[1].portal = 1;
        const auto twbuf = two.encode();
        Portals    twout;
        ByteReader twr(twbuf);
        check(Portals::decode(twr, twout) && twout.mouths() == 2 &&
                  twout.away[1].host == "track.example" && twout.away[1].portal == 1,
              "each opening can lead to a world of its own");

        // Since 72 past the pair: a third mouth, to a room of this server.
        Portals three = two;
        three.away[2].room   = "inferno";
        three.away[2].portal = 2;
        const auto thbuf = three.encode();
        Portals    thout;
        ByteReader thr(thbuf);
        check(Portals::decode(thr, thout) && thout.mouths() == 3 && thout.away[2].room == "inferno",
              "and a third opening leads to a world of its own too");

        Portals half = two;
        half.away[0] = PortalAway{};
        const auto hfbuf = half.encode();
        Portals    hfout;
        ByteReader hfr(hfbuf);
        check(!Portals::decode(hfr, hfout), "and a second mouth with no first one is refused");

        // Half a destination is not one. A host with no port is somewhere
        // nobody can connect to, and a port with no host is a number.
        Portals hostless = away;
        hostless.away[0].host.clear();
        const auto  hbuf2 = hostless.encode();
        Portals     hout2;
        ByteReader  hr2(hbuf2);
        check(!Portals::decode(hr2, hout2), "a port with no host is refused");

        Portals portless = away;
        portless.away[0].port = 0;
        const auto  pbuf2 = portless.encode();
        Portals     pout2;
        ByteReader  pr2(pbuf2);
        check(!Portals::decode(pr2, pout2), "and a host with no port");

        // Which opening over there this mouth comes out of. On the link
        // because that is what it is about, and bounded to the two this
        // protocol has -- a third would be indexed with by whoever received it.
        Portals second = away;
        second.away[0].portal = 1;
        const auto  sbuf2 = second.encode();
        Portals     sout2;
        ByteReader  sr2(sbuf2);
        check(Portals::decode(sr2, sout2) && sout2.away[0].portal == 1,
              "a mouth can name the far world's second opening");

        // Since 71 a mouth may come out of any of the far world's openings --
        // one a mod opened, as the Inferno track's way back does.
        Portals any_of_them = away;
        any_of_them.away[0].portal = 2;
        const auto  mbuf = any_of_them.encode();
        Portals     mout;
        ByteReader  mr(mbuf);
        check(Portals::decode(mr, mout) && mout.away[0].portal == 2,
              "a mouth can name an opening past the pair, one a mod opened");

        Portals no_such = away;
        no_such.away[0].portal = (uint16_t)kMaxOpenings;
        const auto  nsbuf = no_such.encode();
        Portals     nsout;
        ByteReader  nsr(nsbuf);
        check(!Portals::decode(nsr, nsout), "and an opening this protocol does not have is refused");

        // The crossing message carries the same number back out, and is bounded
        // the same way.
        ChangeScope cs;
        cs.scope     = (uint8_t)Scope::Enter;
        cs.portal_id = 1;
        cs.from.cx = 10.5f; cs.from.cy = 20.0f; cs.from.cz = 30.5f;
        cs.from.half_w = 0.5f; cs.from.half_h = 1.0f;
        cs.x = 10.5f; cs.y = 19.8f; cs.z = 30.5f;
        cs.look_z = -1.0f;
        const auto  cbuf2 = cs.encode();
        ChangeScope cout2;
        ByteReader  cr2(cbuf2);
        check(ChangeScope::decode(cr2, cout2) && cout2.portal_id == 1 && cout2.x == 10.5f,
              "a crossing names which opening it wants to come out of");

        ChangeScope bad_id = cs;
        bad_id.portal_id = (uint32_t)kMaxOpenings;
        const auto  bbuf2 = bad_id.encode();
        ChangeScope bout2;
        ByteReader  br2(bbuf2);
        check(!ChangeScope::decode(br2, bout2), "and a number no opening has is refused");

        // The answer to a refused one. It exists so a client is told instead of
        // waiting out a timeout, so what it has to survive is the round trip
        // and a reason from something newer than this parser.
        ScopeDenied no;
        no.scope  = (uint8_t)Scope::Enter;
        no.reason = RefuseReason::ArrivalBlocked;
        no.detail = "the way out of this world's opening is blocked";
        const auto  nbuf2 = no.encode();
        ScopeDenied nout2;
        ByteReader  nr2(nbuf2);
        check(ScopeDenied::decode(nr2, nout2) && nout2.reason == RefuseReason::ArrivalBlocked &&
                  nout2.detail == no.detail && nout2.scope == (uint8_t)Scope::Enter,
              "a refused scope change survives a round trip");
        std::printf("        a refusal with a sentence in it costs %zu bytes\n", nbuf2.size());

        ByteWriter future;
        future.u8((uint8_t)MsgType::ScopeDenied);
        future.u8((uint8_t)Scope::Enter);
        future.u8((uint8_t)RefuseReason::Last + 40);
        future.str("something this build has no name for");
        ScopeDenied fout2;
        ByteReader  fr2(future.data());
        check(ScopeDenied::decode(fr2, fout2) && fout2.reason == RefuseReason::None,
              "and a reason from something newer arrives as no reason, not as a wrong one");

        // And an opening with no area divides by zero when the image is fitted
        // to it.
        Portals flat = ps;
        flat.opening[1].half_w = 0.0f;
        const auto  fbuf = flat.encode();
        Portals     fout;
        ByteReader  fr(fbuf);
        check(!Portals::decode(fr, fout), "and so is an opening with no area");

        Entities e;
        e.players = {{7, 1.0f, 2.0f, 3.0f, 0.5f, -0.25f}, {9, 4.0f, 5.0f, 6.0f, 0.0f, 0.0f}};
        const auto ebuf = e.encode();
        Entities   eout;
        ByteReader er(ebuf);
        check(Entities::decode(er, eout), "an entity update survives a round trip");
        check(eout.players.size() == 2 && eout.players[1].id == 9 && eout.players[0].y == 2.0f,
              "with everybody in it");
        std::printf("        two players cost %zu bytes\n", ebuf.size());

        // The same hostile-input rules as everywhere else: a count that lies,
        // and a coordinate that is not a number.
        ByteWriter lying;
        lying.u8((uint8_t)MsgType::Entities);
        lying.u16(60000);
        Entities   liedout;
        ByteReader lr(lying.data());
        check(!Entities::decode(lr, liedout), "a count of sixty thousand players is refused");

        Entities nan_entity;
        nan_entity.players = {{1, std::numeric_limits<float>::quiet_NaN(), 0, 0, 0, 0}};
        const auto nbuf = nan_entity.encode();
        Entities   nout;
        ByteReader nr(nbuf);
        check(!Entities::decode(nr, nout), "and so is a player who is not anywhere");

        // A player in a kart says how it sits (protocol 66): only a person may,
        // and only as a kart can.
        Entities kart;
        kart.players = {{3, 1.0f, 2.0f, 3.0f, 0.0f, 0.0f}};
        kart.players[0].chassis    = true;
        kart.players[0].body_pitch = 0.2f;
        kart.players[0].body_roll  = -0.4f;
        kart.players[0].base_dy    = -0.3f;
        std::vector<uint8_t> kbuf = kart.encode();
        Entities             kout;
        ByteReader           kr(kbuf);
        check(Entities::decode(kr, kout) && kout.players[0].chassis &&
                  kout.players[0].body_roll == -0.4f && kout.players[0].base_dy == -0.3f,
              "a player's kart arrives as it sits");
        kbuf[35] = (uint8_t)EntityKind::Mob;   // the kind byte, after the frame, the id and five floats
        ByteReader km(kbuf);
        check(!Entities::decode(km, kout), "and a creature that says it sits in one is refused");
        kart.players[0].base_dy = 50.0f;
        const auto kfar = kart.encode();
        ByteReader kf(kfar);
        check(!Entities::decode(kf, kout), "and so is a kart fifty nodes off its own feet");

        // What each body IS, which used to be read off the item id: a stack on
        // the ground was "the one carrying something" and a mob was a player
        // that happened to be smaller, which the client had no way to see.
        Entities zoo;
        zoo.players = {{1, 0, 0, 0, 0, 0, EntityKind::Player, 0},
                       {2, 0, 0, 0, 0, 0, EntityKind::Mob, 0},
                       {3, 0, 0, 0, 0, 0, EntityKind::Drop, 77}};
        const auto zbuf = zoo.encode();
        Entities   zout;
        ByteReader zr(zbuf);
        check(Entities::decode(zr, zout) && zout.players.size() == 3,
              "a person, a creature and a stack on the ground go out together");
        check(zout.players[0].kind == EntityKind::Player &&
                  zout.players[1].kind == EntityKind::Mob &&
                  zout.players[2].kind == EntityKind::Drop && zout.players[2].item == 77,
              "and each one arrives as what it was");
        std::printf("        three bodies of three kinds cost %zu bytes\n", zbuf.size());

        // And the fourth kind, what a mod made visible (TODO.md OPEN-20): it
        // arrives as a thing and not as a person, and a kind past it -- one no
        // client here draws -- is still refused, whole message and all.
        Entities things;
        things.players = {{7, 1.5f, 2.5f, 3.5f, 0, 0, EntityKind::Thing, 0}};
        const auto thing_buf = things.encode();
        Entities   thing_out;
        ByteReader thing_r(thing_buf);
        check(Entities::decode(thing_r, thing_out) && thing_out.players.size() == 1 &&
                  thing_out.players[0].kind == EntityKind::Thing &&
                  thing_out.players[0].x == 1.5f,
              "a thing a mod made visible arrives as a thing");

        bool bumped = false;
        for (size_t i = 0; i < thing_buf.size(); ++i) {
            // The kind byte, found by what it holds rather than by an offset
            // this test would have to keep in step with the codec.
            std::vector<uint8_t> candidate = thing_buf;
            if (candidate[i] != (uint8_t)EntityKind::Thing) continue;
            candidate[i] = (uint8_t)EntityKind::Thing + 1;
            ByteReader cand_r(candidate);
            Entities   cand_out;
            if (Entities::decode(cand_r, cand_out)) continue;
            bumped = true;
            break;
        }
        check(bumped, "and a kind past it is refused");

        // A body's WHOLE id (TODO.md OPEN-48). An id is an index and a
        // generation, and the generation is the top half: 4294967788 is index
        // 492 the second time round, which is the id a stack had in the loopback
        // gate when the wire carried 32 bits and the swing at it was refused as
        // "nothing with that id".
        const uint64_t reused = 4294967788ull;
        Entities       wide;
        wide.players = {{reused, 0, 0, 0, 0, 0, EntityKind::Drop, 77}};
        const auto wide_buf = wide.encode();
        Entities   wide_out;
        ByteReader wide_r(wide_buf);
        check(Entities::decode(wide_r, wide_out) && wide_out.players.size() == 1 &&
                  wide_out.players[0].id == reused,
              "a body whose index was reused keeps its whole id on the wire");
        Hit at_it;
        at_it.target        = reused;
        const auto hit_buf  = at_it.encode();
        Hit        hit_out;
        ByteReader hit_r(hit_buf);
        check(Hit::decode(hit_r, hit_out) && hit_out.target == reused,
              "and a swing names it by that whole id");

        // The two fields answer two questions -- WHAT it is and WHICH stack --
        // so a message where they disagree is one where somebody meant one of
        // them and a decoder that picked its favourite would pick differently
        // on different clients.
        Entities lying_kind;
        lying_kind.players = {{4, 0, 0, 0, 0, 0, EntityKind::Drop, 0}};
        const auto lkbuf = lying_kind.encode();
        Entities   lkout;
        ByteReader lkr(lkbuf);
        check(!Entities::decode(lkr, lkout), "a stack on the ground that is no stack is refused");

        Entities carrying;
        carrying.players = {{5, 0, 0, 0, 0, 0, EntityKind::Player, 9}};
        const auto cabuf = carrying.encode();
        Entities   caout;
        ByteReader car(cabuf);
        check(Entities::decode(car, caout) && caout.players[0].item == 9,
              "a person carries what is in their hand (protocol 48)");

        Entities armed_mob;
        armed_mob.players = {{7, 0, 0, 0, 0, 0, EntityKind::Mob, 9}};
        armed_mob.players[0].clip = "fish";
        const auto ambuf = armed_mob.encode();
        Entities   amout;
        ByteReader amr(ambuf);
        check(Entities::decode(amr, amout) && amout.players[0].item == 9 &&
                  amout.players[0].clip == "fish" && !amout.players[0].crouching,
              "a worker creature holds a thing and plays its mod's clip (protocol 81)");

        Entities armed_thing;
        armed_thing.players = {{8, 0, 0, 0, 0, 0, EntityKind::Thing, 9}};
        const auto atbuf = armed_thing.encode();
        Entities   atout;
        ByteReader atr(atbuf);
        check(!Entities::decode(atr, atout), "but a thing that is also an item is refused");

        Entities acting_thing;
        acting_thing.players = {{9, 0, 0, 0, 0, 0, EntityKind::Thing, 0}};
        acting_thing.players[0].clip = "death";
        const auto acbuf = acting_thing.encode();
        Entities   acout;
        ByteReader acr(acbuf);
        check(Entities::decode(acr, acout) && acout.players[0].clip == "death" && !acout.players[0].crouching,
              "a mod's thing plays a clip its mod named (core.ecs.clip, protocol 88)");

        // A player's livery (93): on the hello and on their body, flag 32 as a
        // drop's falling, and a number past the extras refused.
        Hello dressed_hello;
        dressed_hello.livery = 5u | (2u << kLiveryHeadShift) | (3u << kLiveryNeckShift) |
                               (3u << kLiverySpeciesShift) | (2u << kLiveryStyleShift);   // 94
        const auto dhbuf = dressed_hello.encode();
        Hello      dhout;
        ByteReader dhr(dhbuf);
        check(Hello::decode(dhr, dhout) && dhout.livery == dressed_hello.livery,
              "a hello carries the livery the player picked (protocol 93)");
        Entities liveried;
        liveried.players = {{10, 0, 0, 0, 0, 0, EntityKind::Player, 0}};
        liveried.players[0].livery = dressed_hello.livery;
        liveried.players[0].clip   = "cards";
        const auto lvbuf = liveried.encode();
        Entities   lvout;
        ByteReader lvr(lvbuf);
        check(Entities::decode(lvr, lvout) && lvout.players[0].livery == dressed_hello.livery &&
                  lvout.players[0].clip == "cards" && !lvout.players[0].falling,
              "and so does their body, beside a clip");
        Dress      dress{dressed_hello.livery};
        const auto dsbuf = dress.encode();
        Dress      dsout;
        ByteReader dsr(dsbuf);
        check(Dress::decode(dsr, dsout) && dsout.livery == dress.livery && dsbuf.size() == 5,
              "and a livery changed mid-game, in five bytes");
        dressed_hello.livery = kLiveryMax;
        const auto badbuf = dressed_hello.encode();
        ByteReader badr(badbuf);
        check(!Hello::decode(badr, dhout), "and a livery past its bits is refused");

        Hold hold;
        hold.slot    = 4;
        hold.working = true;
        const auto hold_buf = hold.encode();
        Hold       hold_out;
        ByteReader hold_r(hold_buf);
        check(Hold::decode(hold_r, hold_out) && hold_out.slot == 4 && hold_out.working &&
                  hold_buf.size() == 3,
              "a hand changes square, or starts or stops working, in three bytes");
        Hold far_hand;
        far_hand.slot = 9;
        const auto far_buf = far_hand.encode();
        ByteReader far_r(far_buf);
        check(!Hold::decode(far_r, hold_out), "and a square past the hotbar is refused");

        Hold low;
        low.crouching = true;
        const auto low_buf = low.encode();
        ByteReader low_r(low_buf);
        check(Hold::decode(low_r, hold_out) && hold_out.crouching && !hold_out.working,
              "and a body crouches in the same byte (protocol 51)");

        Entities crouched;
        crouched.players = {{8, 0, 0, 0, 0, 0, EntityKind::Player, 0}};
        crouched.players[0].crouching = true;
        const auto crbuf = crouched.encode();
        Entities   crout;
        ByteReader crr(crbuf);
        check(Entities::decode(crr, crout) && crout.players[0].crouching,
              "and everybody else is told it crouches");

        Entities swinging;
        swinging.players = {{9, 0, 0, 0, 0, 0, EntityKind::Mob, 0}};
        swinging.players[0].working = true;
        const auto swbuf = swinging.encode();
        ByteReader swr(swbuf);
        Entities   swout;
        check(Entities::decode(swr, swout) && swout.players[0].working,
              "a creature swings or draws (protocol 52)");
        swinging.players[0].crouching = true;
        const auto sw2 = swinging.encode();
        ByteReader sw2r(sw2);
        check(!Entities::decode(sw2r, swout), "but only a person crouches");

        // Protocol 58: what a person wears, and how used a stack in a bag is.
        Entities wearing;
        wearing.players = {{10, 0, 0, 0, 0, 0, EntityKind::Player, 0}};
        wearing.players[0].worn[0] = 41;
        wearing.players[0].worn[3] = 44;
        const auto drbuf = wearing.encode();
        ByteReader drr(drbuf);
        Entities   drout;
        check(Entities::decode(drr, drout) && drout.players[0].worn[0] == 41 &&
                  drout.players[0].worn[1] == 0 && drout.players[0].worn[3] == 44,
              "everybody is told what a person wears (protocol 58)");

        InventoryState worn_bag;
        worn_bag.slots = {{7, 1, 30}, {0, 0, 0}};
        const auto wbuf = worn_bag.encode();
        ByteReader wr(wbuf);
        InventoryState wout;
        check(InventoryState::decode(wr, wout) && wout.slots[0].used == 30,
              "and how much of a piece is used travels with it");

        ByteWriter odd;
        odd.u8((uint8_t)MsgType::Entities);
        odd.u16(1);
        odd.u32(6);
        odd.f32(0); odd.f32(0); odd.f32(0);
        odd.f32(0); odd.f32(0);
        odd.u8(9);      // a kind nothing here draws
        odd.u16(0);
        Entities   oddout;
        ByteReader oddr(odd.data());
        check(!Entities::decode(oddr, oddout),
              "and a body of a kind this build has never heard of is refused rather than drawn as a person");

        // ---------------------------------------- and what each body LOOKS like
        //
        // Until now every body on the wire arrived as a box, because there was
        // no field in which to say otherwise: a mob somebody modelled was a
        // character on the machine that spawned it and a crate everywhere else.
        //
        // An id and not a name, for the same reason `item` is one. 0 is
        // "nothing said", so a build with no models sends zeroes and gets the
        // world it has today.
        Entities dressed;
        dressed.players = {{1, 0, 0, 0, 0, 0, EntityKind::Player, 0, 3},
                           {2, 0, 0, 0, 0, 0, EntityKind::Mob, 0, 1},
                           {3, 0, 0, 0, 0, 0, EntityKind::Drop, 77, 0}};
        const auto dressbuf = dressed.encode();
        Entities   worn;
        ByteReader wear(dressbuf);
        check(Entities::decode(wear, worn) && worn.players.size() == 3,
              "three bodies go out, two of them wearing something");
        check(worn.players[0].model == 3 && worn.players[1].model == 1 &&
                  worn.players[2].model == 0,
              "and each one arrives wearing what it went out in");
        std::printf("        a body costs %zu bytes with a model in it\n",
                    dressbuf.size() / worn.players.size());

        // A stack on the ground is drawn as the thing it is, which is what
        // `item` is for. One wearing a model is two answers to one question --
        // the same rule `kind` and `item` already live by, and a decoder that
        // picked its favourite would pick differently on different clients.
        Entities dressed_drop;
        dressed_drop.players = {{6, 0, 0, 0, 0, 0, EntityKind::Drop, 77, 2}};
        const auto dropbuf = dressed_drop.encode();
        Entities   dropout;
        ByteReader dropr(dropbuf);
        check(!Entities::decode(dropr, dropout),
              "a stack on the ground wearing a costume is refused");
    }

    std::printf("\nstriking something\n");
    {
        Hit h;
        h.target = 4242;
        h.slot   = 3;
        h.use    = true;
        const std::vector<uint8_t> hw = h.encode();
        check(peek_type(hw.data(), hw.size()) == MsgType::Hit, "a hit goes out as a hit");

        ByteReader hr(hw);
        Hit        hout;
        check(Hit::decode(hr, hout) && hout.target == 4242 && hout.slot == 3 && hout.use,
              "and it names the thing that was struck, the hand it was struck with and "
              "whether it was a use");
        std::printf("        a hit is %zu bytes\n", hw.size());

        // No damage on the wire, deliberately. A client that could say what a
        // blow was worth is a client that could say twenty, and the check would
        // have to be a second copy of the rule that decides it.
        //
        // Ten bytes since protocol 47: the whole id (protocol 31, TODO.md
        // OPEN-48) and the hotbar slot, whose item the SERVER looks the damage
        // up for. Still nowhere to put a number; and since 60 whether it is a use.
        check(hw.size() == 11,
              "eleven bytes: a type, a whole id, a slot and a use, with nowhere to put a number of its own");

        Hit                        nobody;
        const std::vector<uint8_t> nw = nobody.encode();
        ByteReader                 nr2(nw);
        Hit                        nout2;
        check(!Hit::decode(nr2, nout2), "a hit at entity zero is refused: zero is not a body");
    }


    std::printf("\nthe redirect\n");
    {
        Redirect rd;
        rd.host      = "far.example";
        rd.port      = 25301;
        rd.portal_id = 1;
        rd.reason    = "you walked into an opening onto far.example:25301";

        const auto rbuf = rd.encode();
        Redirect   rout;
        ByteReader rr(rbuf);
        check(Redirect::decode(rr, rout) && rout.host == rd.host && rout.port == 25301 &&
                  rout.portal_id == 1 && rout.reason == rd.reason && !rout.has_ticket,
              "a redirect survives a round trip");
        std::printf("        a redirect with a sentence in it is %zu bytes\n", rbuf.size());

        // Somewhere nobody can connect to. A redirect a client cannot act on is
        // worse than none: it would close a working session to go nowhere.
        Redirect hostless = rd;
        hostless.host.clear();
        const auto  hbuf4 = hostless.encode();
        Redirect    hout4;
        ByteReader  hr4(hbuf4);
        check(!Redirect::decode(hr4, hout4), "a redirect to no host is refused");

        Redirect portless = rd;
        portless.port     = 0;
        const auto  pbuf4 = portless.encode();
        Redirect    pout4;
        ByteReader  pr4(pbuf4);
        check(!Redirect::decode(pr4, pout4), "and one to no port");

        Redirect far_hole = rd;
        far_hole.portal_id = (uint32_t)kMaxOpenings;
        const auto  fbuf4 = far_hole.encode();
        Redirect    fout4;
        ByteReader  fr4(fbuf4);
        check(!Redirect::decode(fr4, fout4), "and one naming an opening this protocol lacks");
    }

    std::printf("\nthe media messages\n");
    {
        // The offer.
        MediaOffer offer;
        for (int i = 0; i < 3; ++i) {
            MediaEntry e;
            e.name = "core_stone" + std::to_string(i) + ".png";
            e.hash = hash_content(&i, sizeof(i));
            e.size = 4128;
            offer.items.push_back(e);
        }

        const auto obuf = offer.encode();
        MediaOffer oout;
        ByteReader orr(obuf);
        check(MediaOffer::decode(orr, oout) && oout.items.size() == 3 &&
                  oout.items[1].name == offer.items[1].name &&
                  oout.items[1].hash == offer.items[1].hash && oout.items[2].size == 4128,
              "an offer of three files survives a round trip");
        std::printf("        an offer naming %zu file(s) is %zu bytes, %zu per file\n",
                    offer.items.size(), obuf.size(), obuf.size() / offer.items.size());

        // A name that is a path. It never reaches a filesystem -- the cache is
        // keyed by the hash and never by this -- and it is refused anyway,
        // because a name is what a log line prints and what a block looks
        // itself up by.
        MediaOffer escaped = offer;
        escaped.items[0].name = "../../.ssh/id_ed25519";
        const auto ebuf = escaped.encode();
        MediaOffer eout;
        ByteReader err(ebuf);
        check(!MediaOffer::decode(err, eout), "a media name that is a path is refused");

        MediaOffer huge = offer;
        huge.items[0].size = (uint32_t)kMaxMediaBytes + 1;
        const auto hbuf = huge.encode();
        MediaOffer hout;
        ByteReader hrr(hbuf);
        check(!MediaOffer::decode(hrr, hout), "and one announcing a file bigger than the cap");

        // The request.
        MediaNeed need;
        need.hashes.push_back(offer.items[0].hash);
        need.hashes.push_back(offer.items[2].hash);
        const auto nbuf = need.encode();
        MediaNeed  nout;
        ByteReader nrr(nbuf);
        check(MediaNeed::decode(nrr, nout) && nout.hashes.size() == 2 &&
                  nout.hashes[1] == offer.items[2].hash,
              "a request for two files survives a round trip");
        std::printf("        a request for %zu file(s) is %zu bytes\n", need.hashes.size(),
                    nbuf.size());

        // The file itself.
        std::vector<uint8_t> payload(4128);
        for (size_t i = 0; i < payload.size(); ++i) payload[i] = (uint8_t)(i * 7 + 3);

        MediaData blob;
        blob.bytes = payload;
        blob.hash  = hash_content(payload);
        const auto bbuf = blob.encode();
        MediaData  bout;
        ByteReader brr(bbuf);
        check(MediaData::decode(brr, bout) && bout.bytes == payload && bout.hash == blob.hash,
              "a file survives a round trip");
        std::printf("        a %zu byte file costs %zu bytes on the wire\n", payload.size(),
                    bbuf.size());

        // What content addressing is FOR, at the parser: bytes that are not
        // what the message says they are do not get out of the decoder. One
        // bit, in the middle, which is the corruption a length check and a
        // checksum-free format would both wave through.
        auto tampered = bbuf;
        tampered[tampered.size() / 2] ^= 0x01;
        MediaData  tout;
        ByteReader trr(tampered);
        check(!MediaData::decode(trr, tout),
              "and one bit changed anywhere in it does not");

        // And the same message with the hash corrected to match the new bytes
        // parses -- which is the point that makes the check above worth having
        // rather than a length test in disguise: it is the CONTENT that is
        // being named, and a consistent lie is a different file, not a broken
        // one.
        MediaData relabelled;
        relabelled.bytes = bout.bytes.empty() ? payload : bout.bytes;
        relabelled.bytes[relabelled.bytes.size() / 2] ^= 0x01;
        relabelled.hash = hash_content(relabelled.bytes);
        const auto rlbuf = relabelled.encode();
        MediaData  rlout;
        ByteReader rlrr(rlbuf);
        check(MediaData::decode(rlrr, rlout) && rlout.hash != blob.hash,
              "while the same bytes under their own hash are a different file, and parse");
    }

    std::printf("\nthe heartbeat\n");
    {
        Heartbeat h;
        h.seq           = 42;
        h.players       = 3;
        h.max_players   = 8;
        h.observers     = 5;
        h.max_observers = 8;
        h.flags         = kWorldWatchable | kWorldEnterable;

        const auto hbuf = h.encode();
        Heartbeat  hout;
        ByteReader hr(hbuf);
        check(Heartbeat::decode(hr, hout) && hout.seq == 42 && hout.players == 3 &&
                  hout.watchable() && hout.enterable(),
              "a heartbeat survives a round trip");
        std::printf("        one beat is %zu bytes; at %.0f Hz that is %.0f bytes a minute a "
                    "player\n", hbuf.size(), 1.0 / kHeartbeatInterval,
                    (double)hbuf.size() * 60.0 / kHeartbeatInterval);

        // Counts that cannot both be true. Not a security matter -- this comes
        // from a server the client already decided to talk to -- but a bar drawn
        // from 9 out of 8 is a rendering bug traced back to here.
        Heartbeat overfull = h;
        overfull.players   = 9;
        const auto  obuf = overfull.encode();
        Heartbeat   oout;
        ByteReader  orr(obuf);
        check(!Heartbeat::decode(orr, oout), "more players than seats is refused");

        Heartbeat overwatched = h;
        overwatched.observers = 9;
        const auto  wbuf = overwatched.encode();
        Heartbeat   wout;
        ByteReader  wr(wbuf);
        check(!Heartbeat::decode(wr, wout), "and more windows than the world allows");

        // A flag this build has no name for. Refused rather than masked off: a
        // heartbeat is small and frequent, and quietly ignoring half of one is
        // how a client acts on a world state that was never sent.
        ByteWriter future;
        future.u8((uint8_t)MsgType::Heartbeat);
        future.u32(1);
        future.u16(0); future.u16(8); future.u16(0); future.u16(8);
        future.u8(0x40);
        Heartbeat  fout;
        ByteReader fr(future.data());
        check(!Heartbeat::decode(fr, fout),
              "and a flag from something newer than this parser is refused, not ignored");
    }

    std::printf("\ntickets\n");
    {
        // DESIGN.md 4.4. The policy -- who is trusted, which audience, whether
        // it has expired -- lives with the server that has one and is measured
        // over a socket in net_loopback. What is measured here is the thing
        // under it: the bytes, and what the signature covers.
        SignKeyPair issuer, other, player;
        check(SignKeyPair::generate(issuer) && SignKeyPair::generate(other) &&
                  SignKeyPair::generate(player),
              "an issuer, another issuer, and a player");

        PortalTicket t;
        std::memcpy(t.issuer, issuer.public_key, kSignPublicBytes);
        std::memcpy(t.subject, player.public_key, kSignPublicBytes);
        std::memcpy(t.audience, other.public_key, kSignPublicBytes);
        t.portal_id = 1;
        t.scopes    = kTicketObserve | kTicketEnter;
        t.issued    = 1000;
        t.expires   = 1000 + kTicketLifetime;
        {
            const auto msg = ticket_challenge(t);
            sign_message(issuer, msg.data(), msg.size(), t.signature);
        }

        const auto  tbuf = t.encode();
        PortalTicket tout;
        ByteReader   tr(tbuf);
        check(PortalTicket::decode(tr, tout) && tr.done(), "a ticket survives a round trip");
        check(std::memcmp(tout.subject, player.public_key, kSignPublicBytes) == 0 &&
                  tout.portal_id == 1 && tout.expires == t.expires,
              "with every field it was given");
        check(ticket_signed_by_issuer(tout), "and the signature on it is the issuer's");
        std::printf("        a ticket is %zu bytes on the wire\n", tbuf.size());

        // Every field is under the signature, which is what stops a ticket from
        // being edited into a better one. Checked one field at a time rather
        // than trusted: a signature over four of the seven fields verifies just
        // as happily, and the three left out are the three worth changing.
        int caught = 0;
        for (int field = 0; field < 6; ++field) {
            PortalTicket b = t;
            switch (field) {
            case 0: b.subject[0]  ^= 0x01; break;
            case 1: b.audience[0] ^= 0x01; break;
            case 2: b.portal_id    = 0;    break;
            case 3: b.scopes       = kTicketObserve; break;
            case 4: b.expires     += 100000; break;
            case 5: b.issuer[0]   ^= 0x01; break;
            }
            if (!ticket_signed_by_issuer(b)) ++caught;
        }
        check(caught == 6, "and changing ANY field under it breaks that signature");
        std::printf("        6 of 6 single-field edits are caught by the signature\n");

        // What the decoder refuses before policy ever sees it.
        PortalTicket empty_scope = t;
        empty_scope.scopes       = 0;
        const auto   ebuf2 = empty_scope.encode();
        PortalTicket eout2;
        ByteReader   er2(ebuf2);
        check(!PortalTicket::decode(er2, eout2), "a ticket that permits nothing is refused");

        PortalTicket unknown_scope = t;
        unknown_scope.scopes       = 0x80;
        const auto   ubuf2 = unknown_scope.encode();
        PortalTicket uout2;
        ByteReader   ur2(ubuf2);
        check(!PortalTicket::decode(ur2, uout2),
              "and one for a permission this build has no name for");

        PortalTicket far_opening = t;
        far_opening.portal_id    = (uint32_t)kMaxOpenings;
        const auto   fbuf2 = far_opening.encode();
        PortalTicket fout2;
        ByteReader   fr2(fbuf2);
        check(!PortalTicket::decode(fr2, fout2), "and one for an opening this protocol lacks");

        PortalTicket backwards = t;
        backwards.expires      = backwards.issued;
        const auto   bbuf3 = backwards.encode();
        PortalTicket bout3;
        ByteReader   br3(bbuf3);
        check(!PortalTicket::decode(br3, bout3), "and one whose window is not one");

        // The request and the answer.
        TicketRequest req;
        req.scopes = kTicketEnter;
        const auto    rqbuf = req.encode();
        TicketRequest rqout;
        ByteReader    rqr(rqbuf);
        check(TicketRequest::decode(rqr, rqout) && rqout.scopes == kTicketEnter,
              "a ticket request survives a round trip");

        ByteWriter nothing_asked;
        nothing_asked.u8((uint8_t)MsgType::TicketRequest);
        nothing_asked.u8(0);
        TicketRequest naout;
        ByteReader    nar(nothing_asked.data());
        check(!TicketRequest::decode(nar, naout), "and asking for nothing is refused");

        Ticket granted;
        granted.granted = true;
        granted.ticket  = t;
        const auto gbuf = granted.encode();
        Ticket     gout;
        ByteReader gr(gbuf);
        check(Ticket::decode(gr, gout) && gout.granted && gout.ticket.portal_id == 1,
              "a granted ticket survives a round trip");

        Ticket refused_ticket;
        refused_ticket.reason = RefuseReason::BadTicket;
        refused_ticket.detail = "this world does not know its neighbour's key";
        const auto rfbuf = refused_ticket.encode();
        Ticket     rfout;
        ByteReader rfr(rfbuf);
        check(Ticket::decode(rfr, rfout) && !rfout.granted &&
                  rfout.reason == RefuseReason::BadTicket && rfout.detail == refused_ticket.detail,
              "and so does a refusal, with the reason on it");

        // And the two messages that carry one.
        Hello with_ticket    = make_hello();
        with_ticket.scope      = Scope::Observe;
        with_ticket.has_ticket = true;
        with_ticket.ticket     = t;
        const auto hbuf3 = with_ticket.encode();
        Hello      hout3;
        ByteReader hr3(hbuf3);
        check(Hello::decode(hr3, hout3) && hout3.has_ticket &&
                  std::memcmp(hout3.ticket.signature, t.signature, kSignatureBytes) == 0,
              "a hello can present a ticket");

        Hello without = make_hello();
        const auto wbuf3 = without.encode();
        Hello      wout3;
        ByteReader wr3(wbuf3);
        check(Hello::decode(wr3, wout3) && !wout3.has_ticket,
              "and a hello without one says so, rather than carrying 181 zero bytes");

        ChangeScope entering;
        entering.scope      = (uint8_t)Scope::Enter;
        entering.portal_id  = 1;
        entering.has_ticket = true;
        entering.ticket     = t;
        entering.from.cx = 10.5f; entering.from.cy = 20.0f; entering.from.cz = 30.5f;
        entering.from.half_w = 0.5f; entering.from.half_h = 1.0f;
        entering.x = 10.5f; entering.y = 19.8f; entering.z = 30.5f;
        entering.look_z = -1.0f;
        const auto  cbuf3 = entering.encode();
        ChangeScope cout3;
        ByteReader  cr3(cbuf3);
        check(ChangeScope::decode(cr3, cout3) && cout3.has_ticket && cout3.portal_id == 1,
              "and so can a crossing");
    }

    std::printf("\nsessions\n");
    {
        ServerSessions s(make_server(), SpawnPoint{0.0f, 0.0f, 0.0f});

        s.on_connect(1, 0.0);
        check(s.find(1) != nullptr && s.find(1)->state == SessionState::Pending,
              "a peer that has connected is pending, not a player");

        const auto hello_buf = make_hello().encode();
        SessionAction a = s.on_message(1, (uint8_t)Channel::Handshake, hello_buf.data(),
                                       hello_buf.size(), 0.1);
        check(!a.close && !a.reply.empty(), "a valid hello is answered, not closed");
        check(s.find(1)->state == SessionState::Ready && s.find(1)->name == "giacomo",
              "and the peer becomes a named player");

        // A second hello is a request to become somebody else, mid-session.
        SessionAction again = s.on_message(1, (uint8_t)Channel::Handshake, hello_buf.data(),
                                           hello_buf.size(), 0.2);
        check(again.close, "a second hello closes the connection");

        // Position claims move the interest centre and nothing else.
        {
            Move far;
            far.seq = 5;
            far.x = 300.0f; far.y = 40.0f; far.z = 300.0f;
            const auto fb = far.encode();
            s.on_message(1, (uint8_t)Channel::Entity, fb.data(), fb.size(), 0.3);
            const ChunkPos moved_to = s.find(1)->interest.centre();
            check(moved_to.x == 9 && moved_to.z == 9, "a move puts the interest where it says");

            // An older one arriving late. Sequenced delivery makes this rare
            // and not impossible, and applying it would drag the centre
            // backwards and resend chunks the player already has.
            Move stale;
            stale.seq = 3;
            stale.x = 0.0f; stale.y = 0.0f; stale.z = 0.0f;
            const auto sb = stale.encode();
            s.on_message(1, (uint8_t)Channel::Entity, sb.data(), sb.size(), 0.4);
            check(s.find(1)->interest.centre() == moved_to,
                  "and an older one arriving late is ignored");

            // Garbage on the entity channel is a packet to drop, not a player
            // to disconnect: that channel is unreliable and carries whatever
            // turns up.
            const SessionAction junk =
                s.on_message(1, (uint8_t)Channel::Entity, "\x05nonsense", 9, 0.5);
            check(!junk.close, "a malformed position does not disconnect anybody");
            check(s.find(1)->interest.centre() == moved_to, "and does not move them either");
        }

        // Traffic on a subsystem channel before admission.
        s.on_connect(2, 1.0);
        SessionAction early = s.on_message(2, (uint8_t)Channel::Chunk, "\x01\x02", 2, 1.1);
        check(early.close, "chunk traffic before the handshake closes the connection");

        // The pending slot has a deadline. `now` is a parameter precisely so
        // this is three lines instead of a ten-second sleep.
        ServerSessions t(make_server(), SpawnPoint{0.0f, 0.0f, 0.0f});
        t.on_connect(7, 100.0);
        check(t.expired(105.0).empty(), "a peer mid-handshake is left alone");
        check(t.expired(100.0 + kHandshakeTimeout + 0.1).size() == 1,
              "a peer that connects and says nothing is dropped after the timeout");

        // Pending peers must not count towards the player limit, or filling a
        // server is a matter of opening eight sockets and going quiet.
        ServerConfig small = make_server();
        small.max_players  = 2;
        ServerSessions u(small, SpawnPoint{0.0f, 0.0f, 0.0f});
        for (PeerId p = 1; p <= 8; ++p) u.on_connect(p, 0.0);
        int admitted = 0;
        for (PeerId p = 1; p <= 8; ++p) {
            // A name each: a server refuses a name somebody connected is
            // already using, and what is being counted here is the limit.
            Hello mine       = make_hello();
            mine.player_name = "player" + std::to_string((int)p);
            const auto buf   = mine.encode();

            const SessionAction act =
                u.on_message(p, (uint8_t)Channel::Handshake, buf.data(), buf.size(), 0.5);
            if (!act.close) ++admitted;
        }
        check(admitted == 2, "the player limit counts admitted players, not open sockets");
        check(u.ready_count() == 2, "and the server holds exactly that many");
    }

    std::printf("\nnames\n");
    {
        check(valid_player_name("giacomo"), "an ordinary name is fine");
        check(!valid_player_name(""), "an empty name is not");
        check(!valid_player_name(std::string(33, 'a')), "nor one over 32 characters");
        check(!valid_player_name("gia como"), "nor one with a space");
        check(!valid_player_name("gia\ncomo"),
              "and a newline in a name cannot turn one log line into two");

        Hello h        = make_hello();
        h.player_name  = "bad name";
        const HandshakeResult r = negotiate(h, make_server());
        check(!r.accepted && r.reason == RefuseReason::BadName,
              "the handshake refuses it rather than sanitising it into something else");

        // And a name is one player's while they are here: the label is what
        // everybody else reads, so somebody else wearing it is somebody else
        // being taken for them.
        ServerSessions n(make_server(), SpawnPoint{0.0f, 0.0f, 0.0f});
        const auto     same = make_hello().encode();
        n.on_connect(1, 0.0);
        n.on_connect(2, 0.0);
        check(!n.on_message(1, (uint8_t)Channel::Handshake, same.data(), same.size(), 0.1).close,
              "the first player with a name is admitted");
        check(n.on_message(2, (uint8_t)Channel::Handshake, same.data(), same.size(), 0.2).close,
              "and a second connection under that name is refused");
    }

    // -----------------------------------------------------------------------
    // Everything below is hostile input. None of it should crash, and none of
    // it should be accepted.
    // -----------------------------------------------------------------------

    std::printf("\ntruncation\n");
    {
        const auto hello_buf = make_hello().encode();
        Welcome    w;
        w.features = {"portals"};
        w.channels = {{"yaportal:link", 4}};
        const auto welcome_buf = w.encode();

        int accepted_short = 0;
        for (size_t n = 0; n < hello_buf.size(); ++n) {
            Hello      out;
            ByteReader r(hello_buf.data(), n);
            if (Hello::decode(r, out)) ++accepted_short;
        }
        for (size_t n = 0; n < welcome_buf.size(); ++n) {
            Welcome    out;
            ByteReader r(welcome_buf.data(), n);
            if (Welcome::decode(r, out)) ++accepted_short;
        }
        check(accepted_short == 0,
              "no prefix of a valid message parses as that message");

        // The mirror: bytes on the end are as wrong as bytes missing.
        std::vector<uint8_t> trailing = hello_buf;
        trailing.push_back(0x41);
        Hello      out;
        ByteReader r(trailing);
        check(!Hello::decode(r, out), "and nor does a valid message with a byte glued on");

        // A length field that claims more than the buffer holds. Written by
        // hand because encode() cannot produce it.
        ByteWriter lie;
        lie.u8((uint8_t)MsgType::Hello);
        lie.u32(kMagic);
        lie.u16(1);
        lie.u16(1);
        lie.u16(4000);            // a 4000-byte name in a 20-byte packet
        lie.bytes("abc", 3);
        Hello      lied;
        ByteReader lr(lie.data());
        check(!Hello::decode(lr, lied),
              "a length field larger than the packet is refused before anything is allocated");
    }

    std::printf("\nwell formed and hostile\n");
    {
        // Everything above is malformed in some way, so the bounds check catches
        // it whether or not the caps exist -- which sabotaging the caps proved:
        // deleting them left the whole suite green. The caps defend against the
        // other shape of attack, a packet that is entirely well formed and
        // simply enormous. That needs its own input, so here it is.

        // A name of 60000 bytes, with all 60000 bytes actually present.
        ByteWriter big_name;
        big_name.u8((uint8_t)MsgType::Hello);
        big_name.u32(kMagic);
        big_name.u16(1);
        big_name.u16(1);
        big_name.u16(60000);
        for (int i = 0; i < 60000; ++i) big_name.u8('a');
        big_name.u16(0);

        Hello      out;
        ByteReader r(big_name.data());
        check(!Hello::decode(r, out),
              "a 60000-byte name that is all there is still refused, by the cap");

        // 20000 features, each one real. Without the count cap this parses and
        // the server holds 20000 strings for one unauthenticated packet.
        ByteWriter many;
        many.u8((uint8_t)MsgType::Hello);
        many.u32(kMagic);
        many.u16(1);
        many.u16(1);
        many.str("giacomo");
        many.u16(20000);
        for (int i = 0; i < 20000; ++i) many.str("feature");

        Hello      out2;
        ByteReader r2(many.data());
        check(!Hello::decode(r2, out2),
              "and 20000 real features are refused by the count cap, not stored");

        // The bound this puts on memory, stated as a number rather than a
        // feeling: whatever a peer sends, one parsed handshake cannot hold more
        // than this many strings of this many bytes.
        const size_t worst = kMaxFeatures * kMaxFeatureName + kMaxNameLength;
        std::printf("        the most one accepted hello can hold: %zu bytes of strings\n", worst);
        check(worst < 8192, "so an unauthenticated packet has a small, stated ceiling");
    }

    std::printf("\ncorruption\n");
    {
        const auto original = make_hello().encode();

        // The first version of this section asserted that no flipped bit
        // produces a message with an invalid name, and it went red: flip a bit
        // in "giacomo" and the parser hands back a name with a control
        // character in it. The parser is right and the assertion was wrong.
        // Decoding says "these bytes are a well-formed Hello"; it does not say
        // "this is a player we will admit". Validation belongs one layer up,
        // where the refusal has somewhere to go.
        //
        // So the property tested is the one that actually protects anything:
        // run every corrupted message through the whole door -- decode, then
        // negotiate -- and require that nothing accepted carries a name that
        // did not pass validation.
        int parsed       = 0;
        int admitted     = 0;
        int bad_admitted = 0;

        for (size_t i = 0; i < original.size(); ++i) {
            for (int bit = 0; bit < 8; ++bit) {
                std::vector<uint8_t> buf = original;
                buf[i] ^= (uint8_t)(1u << bit);

                Hello      out;
                ByteReader r(buf);
                if (!Hello::decode(r, out)) continue;
                ++parsed;

                const HandshakeResult res = negotiate(out, make_server());
                if (!res.accepted) continue;
                ++admitted;
                if (!valid_player_name(out.player_name)) ++bad_admitted;
            }
        }

        std::printf("        %zu single-bit flips: %d parsed, %d got past the handshake\n",
                    original.size() * 8, parsed, admitted);
        check(bad_admitted == 0,
              "no corrupted hello reaches the server with a name that failed validation");
        check(parsed > 0 && admitted < parsed,
              "and the flips are doing work: some parse, and fewer are admitted");
    }

    std::printf("\nrandom input\n");
    {
        Rng rng(0x5EED1234u);
        const auto base   = make_hello().encode();
        int        parsed = 0;

        for (int iter = 0; iter < 20000; ++iter) {
            const size_t         n = rng.next() % 128;
            std::vector<uint8_t> buf(n);
            for (size_t i = 0; i < n; ++i) buf[i] = (uint8_t)(rng.next() & 0xFF);

            // Through the same door a real packet comes in: peek the type,
            // then hand it to the matching parser.
            switch (peek_type(buf.data(), buf.size())) {
            case MsgType::Hello: {
                Hello      out;
                ByteReader r(buf);
                if (Hello::decode(r, out)) ++parsed;
                break;
            }
            case MsgType::Welcome: {
                Welcome    out;
                ByteReader r(buf);
                if (Welcome::decode(r, out)) ++parsed;
                break;
            }
            case MsgType::Refuse: {
                Refuse     out;
                ByteReader r(buf);
                if (Refuse::decode(r, out)) ++parsed;
                break;
            }
            case MsgType::Teleport: {
                Teleport   out;
                ByteReader r(buf);
                if (Teleport::decode(r, out)) ++parsed;
                break;
            }
            case MsgType::Push: {
                Push       out;
                ByteReader r(buf);
                if (Push::decode(r, out)) ++parsed;
                break;
            }
            case MsgType::PlacePortal: {
                PlacePortal out;
                ByteReader  r(buf);
                if (PlacePortal::decode(r, out)) ++parsed;
                break;
            }
            case MsgType::Portals: {
                Portals    out;
                ByteReader r(buf);
                if (Portals::decode(r, out)) ++parsed;
                break;
            }
            case MsgType::Edit: {
                Edit       out;
                ByteReader r(buf);
                if (Edit::decode(r, out)) ++parsed;
                break;
            }
            case MsgType::ModData: {
                ModData    out;
                ByteReader r(buf);
                if (ModData::decode(r, out)) ++parsed;
                break;
            }
            case MsgType::BlockChange: {
                BlockChange out;
                ByteReader  r(buf);
                if (BlockChange::decode(r, out)) ++parsed;
                break;
            }
            case MsgType::CellOpen: {
                CellOpen   out;
                ByteReader r(buf);
                if (CellOpen::decode(r, out)) ++parsed;
                break;
            }
            case MsgType::ChunksGone: {
                ChunksGone out;
                ByteReader r(buf);
                if (ChunksGone::decode(r, out)) ++parsed;
                break;
            }
            case MsgType::BodyMovement: {
                BodyMovement out;
                ByteReader   r(buf);
                if (BodyMovement::decode(r, out)) ++parsed;
                break;
            }
            case MsgType::Notice: {
                Notice     out;
                ByteReader r(buf);
                if (Notice::decode(r, out)) ++parsed;
                break;
            }
            case MsgType::Chat: {
                Chat       out;
                ByteReader r(buf);
                if (Chat::decode(r, out)) ++parsed;
                break;
            }
            case MsgType::Hold: {
                Hold       out;
                ByteReader r(buf);
                if (Hold::decode(r, out)) ++parsed;
                break;
            }
            case MsgType::Inventory: {
                InventoryState out;
                ByteReader     r(buf);
                if (InventoryState::decode(r, out)) ++parsed;
                break;
            }
            case MsgType::MoveItem: {
                MoveItem   out;
                ByteReader r(buf);
                if (MoveItem::decode(r, out)) ++parsed;
                break;
            }
            case MsgType::QuickMove: {
                QuickMove  out;
                ByteReader r(buf);
                if (QuickMove::decode(r, out)) ++parsed;
                break;
            }
            case MsgType::Vitals: {
                Vitals     out;
                ByteReader r(buf);
                if (Vitals::decode(r, out)) ++parsed;
                break;
            }
            case MsgType::Time: {
                Time       out;
                ByteReader r(buf);
                if (Time::decode(r, out)) ++parsed;
                break;
            }
            case MsgType::Hit: {
                Hit        out;
                ByteReader r(buf);
                if (Hit::decode(r, out)) ++parsed;
                break;
            }
            case MsgType::DropItem: {
                DropItem   out;
                ByteReader r(buf);
                if (DropItem::decode(r, out)) ++parsed;
                break;
            }
            case MsgType::PanelShow: {
                PanelShow  out;
                ByteReader r(buf);
                if (PanelShow::decode(r, out)) ++parsed;
                break;
            }
            case MsgType::UiClick: {
                UiClick    out;
                ByteReader r(buf);
                if (UiClick::decode(r, out)) ++parsed;
                break;
            }
            case MsgType::Container: {
                ContainerState out;
                ByteReader     r(buf);
                if (ContainerState::decode(r, out)) ++parsed;
                break;
            }
            case MsgType::Craft: {
                CraftState out;
                ByteReader r(buf);
                if (CraftState::decode(r, out)) ++parsed;
                break;
            }
            case MsgType::CloseContainer: {
                CloseContainer out;
                ByteReader     r(buf);
                if (CloseContainer::decode(r, out)) ++parsed;
                break;
            }
            case MsgType::ChangeScope: {
                ChangeScope out;
                ByteReader  r(buf);
                if (ChangeScope::decode(r, out)) ++parsed;
                break;
            }
            case MsgType::ScopeDenied: {
                ScopeDenied out;
                ByteReader  r(buf);
                if (ScopeDenied::decode(r, out)) ++parsed;
                break;
            }
            case MsgType::MediaOffer: {
                MediaOffer out;
                ByteReader r(buf);
                if (MediaOffer::decode(r, out)) ++parsed;
                break;
            }
            case MsgType::MediaNeed: {
                MediaNeed  out;
                ByteReader r(buf);
                if (MediaNeed::decode(r, out)) ++parsed;
                break;
            }
            case MsgType::MediaData: {
                MediaData  out;
                ByteReader r(buf);
                if (MediaData::decode(r, out)) ++parsed;
                break;
            }
            case MsgType::MediaMissing: {
                MediaMissing out;
                ByteReader   r(buf);
                if (MediaMissing::decode(r, out)) ++parsed;
                break;
            }
            case MsgType::TicketRequest: {
                TicketRequest out;
                ByteReader    r(buf);
                if (TicketRequest::decode(r, out)) ++parsed;
                break;
            }
            case MsgType::Ticket: {
                Ticket     out;
                ByteReader r(buf);
                if (Ticket::decode(r, out)) ++parsed;
                break;
            }
            case MsgType::Heartbeat: {
                Heartbeat  out;
                ByteReader r(buf);
                if (Heartbeat::decode(r, out)) ++parsed;
                break;
            }
            case MsgType::Redirect: {
                Redirect   out;
                ByteReader r(buf);
                if (Redirect::decode(r, out)) ++parsed;
                break;
            }
            case MsgType::Chunk:
            case MsgType::Move:
            case MsgType::Correct:
            case MsgType::Entities:
            case MsgType::Invalid:
                // Chunk bodies belong to the chunk codec and have their own
                // hostile-input gate; Move is exercised below. Nothing here
                // parses them, and the compiler is the thing that will say so
                // when the next message type is added.
                break;
            }
        }

        // 0 parsed, every time, and that number is the point of printing it:
        // pure noise almost never gets past the type byte and the magic, so a
        // fuzz section that stopped here would be decoration. It stays because
        // it is the case of a stray packet arriving on the port, but the real
        // work is the two loops below, which start from a valid header.
        std::printf("        20000 random buffers, %d parsed as a message\n", parsed);
        check(true, "random input does not crash the parsers");

        // Valid type byte, valid magic, everything after it noise. This is the
        // input that gets past the front door and into the field parsers -- the
        // length prefixes, the counts and the loops they drive.
        int deep = 0;
        for (int iter = 0; iter < 20000; ++iter) {
            ByteWriter w;
            const MsgType types[3] = {MsgType::Hello, MsgType::Welcome, MsgType::Refuse};
            const MsgType type     = types[rng.next() % 3];
            w.u8((uint8_t)type);
            w.u32(kMagic);

            const size_t n = rng.next() % 96;
            for (size_t i = 0; i < n; ++i) w.u8((uint8_t)(rng.next() & 0xFF));

            const auto &buf = w.data();
            switch (type) {
            case MsgType::Hello: {
                Hello      out;
                ByteReader r(buf);
                if (Hello::decode(r, out)) {
                    ++deep;
                    // Anything that parses is then run through negotiate, the
                    // way a real one would be: the parser is not the last line.
                    (void)negotiate(out, make_server());
                }
                break;
            }
            case MsgType::Welcome: {
                Welcome    out;
                ByteReader r(buf);
                if (Welcome::decode(r, out)) ++deep;
                break;
            }
            default: {
                Refuse     out;
                ByteReader r(buf);
                if (Refuse::decode(r, out)) ++deep;
                break;
            }
            }
        }
        // 0 again, and this one took a second look. A random body does reach
        // the field parsers -- it is the length prefixes and counts that stop
        // it, since a random u16 name length is over the 32-byte cap 99.9% of
        // the time, and `done()` requires the body to end exactly where the
        // fields do. So this loop exercises the refusal paths and almost never
        // the acceptance path. Both are worth running; only the first is what
        // this loop is good at, and saying otherwise would be a green tick for
        // work not done.
        std::printf("        20000 with a valid header and a random body, %d parsed\n", deep);
        check(true, "a valid header followed by noise is refused rather than crashed on");

        // The mutation that does get deep: take a valid message and corrupt a
        // handful of bytes. Enough structure survives that the parser walks the
        // whole message, which is where a bad length or a bad count would do
        // its damage.
        int mutated_parsed = 0;
        for (int iter = 0; iter < 20000; ++iter) {
            std::vector<uint8_t> buf = base;
            const int            k   = 1 + (int)(rng.next() % 8);
            for (int j = 0; j < k; ++j)
                buf[rng.next() % buf.size()] = (uint8_t)(rng.next() & 0xFF);

            Hello      out;
            ByteReader r(buf);
            if (Hello::decode(r, out)) {
                ++mutated_parsed;
                (void)negotiate(out, make_server());
            }
        }
        std::printf("        20000 hellos with 1-8 bytes corrupted, %d parsed\n", mutated_parsed);
        check(mutated_parsed > 0,
              "the mutation fuzz does reach the field parsers, so it is testing them");

        // And the same fuzz aimed properly: a valid message with a random tail,
        // which is the shape that gets past a type check and into the fields.
        int parsed2 = 0;
        for (int iter = 0; iter < 20000; ++iter) {
            std::vector<uint8_t> buf = base;
            const size_t         cut = rng.next() % buf.size();
            buf.resize(cut);
            const size_t extra = rng.next() % 32;
            for (size_t i = 0; i < extra; ++i) buf.push_back((uint8_t)(rng.next() & 0xFF));

            Hello      out;
            ByteReader r(buf);
            if (Hello::decode(r, out)) ++parsed2;
        }
        std::printf("        20000 truncated-and-extended hellos, %d parsed\n", parsed2);
        check(true, "and neither does a valid message with its tail replaced by noise");
    }

    std::printf("\na grid to craft in, on the wire\n");
    {
        // The SIZE is the state. There is no "a table is open" flag, because a
        // client told three knows -- and two fields could disagree.
        CraftState cs;
        cs.w = 3; cs.h = 3;
        cs.x = -4; cs.y = 21; cs.z = 300;
        for (int i = 0; i < 9; ++i)
            cs.slots.push_back(InventoryState::Slot{(uint16_t)(i == 4 ? 7 : 0),
                                                    (uint16_t)(i == 4 ? 3 : 0)});
        cs.result = InventoryState::Slot{11, 4};

        const std::vector<uint8_t> cw = cs.encode();
        ByteReader                 cr(cw);
        CraftState                 back;
        check(CraftState::decode(cr, back), "a three-by-three parses");
        check(back.w == 3 && back.h == 3, "at the size it was sent");
        check(back.x == -4 && back.z == 300, "with the cell the table is at");
        check(back.slots.size() == 9 && back.slots[4].item == 7 && back.slots[4].count == 3,
              "and every square, the empty ones included");
        check(back.result.item == 11 && back.result.count == 4,
              "and what it makes, which travels WITH it: a client working its own answer out "
              "would be a second recipe table that has to agree");

        // The grid everybody carries.
        CraftState small;
        small.w = 2; small.h = 2;
        small.slots.resize(4);
        const std::vector<uint8_t> sw = small.encode();
        ByteReader                 sr(sw);
        CraftState                 sback;
        check(CraftState::decode(sr, sback) && sback.w == 2 && sback.slots.size() == 4,
              "and a two-by-two parses as four squares");
        check(sback.result.item == 0 && sback.result.count == 0,
              "making nothing, which is a stack of nothing and not a missing field");

        // A size no grid has, in a message that is otherwise WELL FORMED: the
        // right number of squares for the size it claims, so nothing runs out
        // of bytes and nothing is left over. Mutating one byte of a good 3x3
        // message would be refused by the arithmetic alone and would say
        // nothing about the size check -- which is exactly what sabotaging it
        // showed.
        const auto sized = [](int gw, int gh) {
            ByteWriter bw;
            bw.u8((uint8_t)MsgType::Craft);
            bw.u8((uint8_t)gw);
            bw.u8((uint8_t)gh);
            bw.i32(0); bw.i32(0); bw.i32(0);
            for (int i = 0; i < gw * gh; ++i) { bw.u16(0); bw.u16(0); bw.u32(0); }
            bw.u16(0); bw.u16(0); bw.u32(0);
            return bw.data();
        };

        for (const int bad : {0, 1, 4, 9}) {
            const std::vector<uint8_t> bytes = sized(bad, bad);
            ByteReader                 br(bytes);
            CraftState                 out;
            check(!CraftState::decode(br, out),
                  bad == 0   ? "a grid of no width is refused, even with the bytes to match"
                  : bad == 1 ? "and one square is not a grid"
                  : bad == 4 ? "nor four by four"
                             : "nor nine by nine, which is eighty-one squares nothing draws");
        }
        {
            const std::vector<uint8_t> ok2 = sized(2, 2);
            ByteReader                 br(ok2);
            CraftState                 out;
            check(CraftState::decode(br, out),
                  "while the two sizes there are still parse, so the check is a size check and "
                  "not a refusal of everything");
        }

        // A grid whose two sides disagree, which is neither of the two shapes.
        {
            const std::vector<uint8_t> bytes = sized(3, 2);
            ByteReader                 br(bytes);
            CraftState                 out;
            check(!CraftState::decode(br, out), "and three by two is not a grid either");
        }

        {
            std::vector<uint8_t> bytes = cw;
            bytes.push_back(0);
            ByteReader br(bytes);
            CraftState out;
            check(!CraftState::decode(br, out), "a byte too many is refused");
        }
        {
            std::vector<uint8_t> bytes(cw.begin(), cw.end() - 1);
            ByteReader br(bytes);
            CraftState out;
            check(!CraftState::decode(br, out), "and so is a byte too few");
        }

        // Taking the result rides on MoveItem, and the square it names is
        // outside every inventory there is -- so a bug cannot silently mean a
        // real slot.
        check(kCraftResultSlot >= kMaxInventorySlots,
              "the result square is not a slot number any bag can have");

        MoveItem take;
        take.from_side = (uint8_t)Side::Craft;
        take.from      = kCraftResultSlot;
        take.to_side   = (uint8_t)Side::Bag;
        take.to        = 0;
        const std::vector<uint8_t> tw = take.encode();
        ByteReader                 tr(tw);
        MoveItem                   tback;
        check(MoveItem::decode(tr, tback) && tback.from == kCraftResultSlot,
              "a move OUT of the result square parses");

        MoveItem into;
        into.from_side = (uint8_t)Side::Bag;
        into.from      = 0;
        into.to_side   = (uint8_t)Side::Craft;
        into.to        = kCraftResultSlot;
        const std::vector<uint8_t> iw = into.encode();
        ByteReader                 ir(iw);
        MoveItem                   iback;
        check(!MoveItem::decode(ir, iback),
              "while a move INTO it is refused: it is an answer, not a slot");

        MoveItem grid;
        grid.from_side = (uint8_t)Side::Bag;
        grid.from      = 4;
        grid.to_side   = (uint8_t)Side::Craft;
        grid.to        = 2;
        const std::vector<uint8_t> gw = grid.encode();
        ByteReader                 gr(gw);
        MoveItem                   gback;
        check(MoveItem::decode(gr, gback) && gback.to_side == (uint8_t)Side::Craft,
              "and an ordinary move into the grid is not");
    }

    std::printf("\na chest on the wire\n");
    {
        // Which inventory a slot number is a slot OF. A handle and not a place:
        // "the one I have open" is something the server already knows, so a
        // modified client cannot name a chest across the map.
        MoveItem mv;
        mv.from_side = (uint8_t)Side::Bag;
        mv.to_side   = (uint8_t)Side::Container;
        mv.from      = 3;
        mv.to        = 11;
        mv.count     = 5;

        const std::vector<uint8_t> mw = mv.encode();
        ByteReader                 mr(mw);
        MoveItem                   back;
        check(MoveItem::decode(mr, back), "a move across two inventories parses");
        check(back.from_side == (uint8_t)Side::Bag &&
                  back.to_side == (uint8_t)Side::Container && back.from == 3 &&
                  back.to == 11 && back.count == 5,
              "with both sides, both slots and the count");

        {
            MoveItem bad = mv;
            bad.from_side = 9;
            const std::vector<uint8_t> bw = bad.encode();
            ByteReader                 br(bw);
            MoveItem                   out;
            check(!MoveItem::decode(br, out),
                  "a side this build has no inventory for is refused, not read as the bag");
        }
        {
            MoveItem bad = mv;
            bad.to_side = 200;
            const std::vector<uint8_t> bw = bad.encode();
            ByteReader                 br(bw);
            MoveItem                   out;
            check(!MoveItem::decode(br, out), "at either end");
        }

        // ---- and a shift-click, which names one square and no destination
        //
        // Its own message rather than a MoveItem with a made-up `to`. The gate
        // for that is the shape of the thing: there is nowhere in it to put a
        // destination even if a client wanted to send one.
        {
            QuickMove q;
            q.side = (uint8_t)Side::Container;
            q.slot = 17;

            const std::vector<uint8_t> qw = q.encode();
            check(peek_type(qw.data(), qw.size()) == MsgType::QuickMove,
                  "a shift-click goes out as a shift-click and not as a move");
            check(qw.size() == 4, "four bytes: the type, the side, and the square");

            ByteReader qr(qw);
            QuickMove  qb;
            check(QuickMove::decode(qr, qb) && qb.side == (uint8_t)Side::Container &&
                      qb.slot == 17,
                  "and comes back as the square it named");

            // The answer square, which is not a square any bag has.
            QuickMove all;
            all.side = (uint8_t)Side::Craft;
            all.slot = kCraftResultSlot;
            const std::vector<uint8_t> aw = all.encode();
            ByteReader                 ar(aw);
            QuickMove                  ab;
            check(QuickMove::decode(ar, ab) && ab.slot == kCraftResultSlot,
                  "the answer square survives the trip: it is the one slot number that is not "
                  "a slot");

            // The same number on any other side is nothing at all. One
            // comparison decides both, so a build that let it through here
            // would let a bag be indexed one past its end.
            QuickMove wrong;
            wrong.side = (uint8_t)Side::Bag;
            wrong.slot = kCraftResultSlot;
            const std::vector<uint8_t> ww = wrong.encode();
            ByteReader                 wr(ww);
            QuickMove                  wb;
            check(!QuickMove::decode(wr, wb),
                  "while the same number out of the bag is a square no bag has");

            QuickMove side;
            side.side = 200;
            const std::vector<uint8_t> sw = side.encode();
            ByteReader                 sr(sw);
            QuickMove                  sb;
            check(!QuickMove::decode(sr, sb),
                  "a side this build has no inventory for is refused, not read as the bag");

            // A byte too many is a different message that happens to start the
            // same way.
            std::vector<uint8_t> longer = qw;
            longer.push_back(0);
            ByteReader lr(longer);
            QuickMove  lb;
            check(!QuickMove::decode(lr, lb), "and a trailing byte is not a shift-click");
        }

        // ---- what time it is
        {
            Time t;
            t.tod  = clock_to_wire(kDusk);
            t.wind = 123456;

            const std::vector<uint8_t> tw = t.encode();
            check(peek_type(tw.data(), tw.size()) == MsgType::Time, "the time goes out as the time");
            check(tw.size() == 7, "and it is seven bytes, type and wind included");

            ByteReader tr(tw);
            Time       tb;
            check(Time::decode(tr, tb) && tb.tod == t.tod && tb.wind == t.wind,
                  "and comes back the same code and the same wind");
            check(std::fabs(clock_gap(clock_from_wire(tb.tod), kDusk)) < 1.0 / 65536.0,
                  "which is dusk to within one code");

            // Every sixteen-bit value is a real time, so there is no such thing
            // as a time this decoder should refuse -- only a message the wrong
            // length.
            std::vector<uint8_t> longer = tw;
            longer.push_back(0);
            ByteReader lr(longer);
            Time       lb;
            check(!Time::decode(lr, lb), "and a trailing byte is not a time");

            bool all_real = true;
            for (int i = 0; i < 65536; i += 7) {
                std::vector<uint8_t> raw = Time{(uint16_t)i}.encode();
                ByteReader           rr(raw);
                Time                 rb;
                if (!Time::decode(rr, rb) || rb.tod != (uint16_t)i) all_real = false;
            }
            check(all_real, "and every code on the wire is a time this decoder accepts");
        }

        // ---- and how much of a body is left
        {
            Vitals v;
            v.health = 13;
            v.max    = 20;

            const std::vector<uint8_t> vw = v.encode();
            check(peek_type(vw.data(), vw.size()) == MsgType::Vitals, "vitals go out as vitals");

            ByteReader vr(vw);
            Vitals     vb;
            check(Vitals::decode(vr, vb) && vb.health == 13 && vb.max == 20,
                  "and come back with both numbers");

            // The maximum travels rather than being a constant both ends agree
            // on: a bar drawn against a maximum the client made up is a bar
            // that is wrong the day a mod changes one.
            Vitals dead;
            dead.health = 0;
            dead.max    = 20;
            const std::vector<uint8_t> dw = dead.encode();
            ByteReader                 dr(dw);
            Vitals                     db;
            check(Vitals::decode(dr, db) && db.health == 0,
                  "a body at zero is a message and not an absence");

            Vitals over;
            over.health = 21;
            over.max    = 20;
            const std::vector<uint8_t> ow = over.encode();
            ByteReader                 orr(ow);
            Vitals                     ob;
            check(!Vitals::decode(orr, ob),
                  "while more health than the body can hold is a bar past the end of itself");

            Vitals none;
            none.health = 0;
            none.max    = 0;
            const std::vector<uint8_t> nw = none.encode();
            ByteReader                 nr(nw);
            Vitals                     nb;
            check(!Vitals::decode(nr, nb),
                  "and a maximum of nothing is a body that cannot be drawn or hurt");
        }

        // And the chest itself.
        ContainerState cs;
        cs.open = true;
        cs.x = -12; cs.y = 40; cs.z = 305;
        cs.slots.push_back(InventoryState::Slot{7, 20});
        cs.slots.push_back(InventoryState::Slot{0, 0});
        cs.slots.push_back(InventoryState::Slot{3, 1});

        const std::vector<uint8_t> cw = cs.encode();
        check(peek_type(cw.data(), cw.size()) == MsgType::Container, "a chest goes out as a chest");

        ByteReader     cr(cw);
        ContainerState cb;
        check(ContainerState::decode(cr, cb), "and comes back");
        check(cb.open && cb.x == -12 && cb.y == 40 && cb.z == 305,
              "with the cell it is at, negative coordinates and all -- a chest is a place");
        check(cb.slots.size() == 3 && cb.slots[0].item == 7 && cb.slots[0].count == 20 &&
                  cb.slots[2].item == 3,
              "and every slot in it");

        {
            // An item with no count and a count with no item are one empty
            // slot. A client that drew them differently would show a phantom.
            ContainerState odd;
            odd.open = true;
            odd.slots.push_back(InventoryState::Slot{9, 0});
            const std::vector<uint8_t> ow = odd.encode();
            ByteReader                 orr(ow);
            ContainerState             out;
            check(ContainerState::decode(orr, out) && out.slots.size() == 1 &&
                      out.slots[0].item == 0 && out.slots[0].count == 0,
                  "and a slot with a count of nothing comes back empty rather than as a phantom");
        }
        {
            ContainerState shut;
            const std::vector<uint8_t> sw = shut.encode();
            ByteReader                 sr(sw);
            ContainerState             out;
            check(ContainerState::decode(sr, out) && !out.open,
                  "a chest that closed is a message too, saying nothing is open");
        }
        {
            ContainerState empty_open;
            empty_open.open = true;
            const std::vector<uint8_t> ew = empty_open.encode();
            ByteReader                 er(ew);
            ContainerState             out;
            check(!ContainerState::decode(er, out),
                  "while one that is open with no slots at all is not a chest");
        }
        {
            // The count off the wire, refused BEFORE it is used as a reserve.
            // Built by hand, because encode() truncates and so can never
            // produce this.
            ByteWriter bw;
            bw.u8((uint8_t)MsgType::Container);
            bw.u8(1);
            bw.i32(0); bw.i32(0); bw.i32(0);
            bw.u16(60000);   // and then nothing at all

            const std::vector<uint8_t> lie = bw.data();
            ByteReader                 lr(lie);
            ContainerState             out;
            check(!ContainerState::decode(lr, out),
                  "a chest claiming 60000 slots in 20 bytes is refused");
            check(out.slots.capacity() <= kMaxInventorySlots,
                  "and refused by the COUNT, so nothing was reserved for a number that was "
                  "never checked");
        }

        // And shutting it, which carries nothing at all -- not even which
        // chest. A session has one open and the server holds which; a cell here
        // would be a cell to check against the one already known.
        const std::vector<uint8_t> xw = CloseContainer{}.encode();
        check(peek_type(xw.data(), xw.size()) == MsgType::CloseContainer,
              "a chest close goes out as a chest close");
        {
            ByteReader     xr(xw);
            CloseContainer out;
            check(CloseContainer::decode(xr, out), "and comes back");
        }
        {
            // A byte on the end is a message this build did not understand.
            std::vector<uint8_t> extra = xw;
            extra.push_back(0);
            ByteReader     xr(extra);
            CloseContainer out;
            check(!CloseContainer::decode(xr, out),
                  "while one with anything after it is refused, because it is not this message");
        }

        std::printf("        a chest of 3 slots is %zu bytes, a move is %zu, a close is %zu\n",
                    cw.size(), mw.size(), xw.size());
    }

    std::printf("\na panel on the wire\n");
    {
        PanelShow p;
        p.open  = true;
        p.name  = "console:menu";
        p.owner = "console";
        p.w     = 9.5f;
        p.h     = 6.25f;
        p.elements.push_back(PanelShow::Elem{(uint8_t)ui::ElemKind::Box, "", "",
                                             0.25f, 0.25f, 9.0f, 5.75f, 40, 42, 50});
        p.elements.push_back(PanelShow::Elem{(uint8_t)ui::ElemKind::Label, "", "the console",
                                             0.75f, 0.75f, 0.0f, 0.0f, 0, 0, 0});
        p.elements.push_back(PanelShow::Elem{(uint8_t)ui::ElemKind::Button, "build", "Build",
                                             0.75f, 2.0f, 3.5f, 1.0f, 0, 0, 0});

        const std::vector<uint8_t> wire = p.encode();
        check(peek_type(wire.data(), wire.size()) == MsgType::PanelShow,
              "a panel goes out as a panel");

        ByteReader r(wire);
        PanelShow  back;
        check(PanelShow::decode(r, back), "and comes back");
        check(back.open && back.name == p.name && back.owner == p.owner,
              "with the name and the mod that owns it");
        check(back.w == p.w && back.h == p.h,
              "and the size in cells, exactly -- a cell that arrived rounded is a layout "
              "that drifts every hop");
        check(back.elements.size() == 3, "and all three elements");
        check(back.elements.size() == 3 && back.elements[0].r == 40 &&
                  back.elements[0].g == 42 && back.elements[0].b == 50 &&
                  back.elements[2].id == "build" && back.elements[2].text == "Build" &&
                  back.elements[2].x == 0.75f && back.elements[2].w == 3.5f,
              "with their colours, ids, text and geometry");

        // Nothing up. A message and not an absence: a client cannot tell a
        // panel that closed from a tick in which nothing arrived.
        {
            PanelShow                  shut;
            const std::vector<uint8_t> sw = shut.encode();
            ByteReader                 sr(sw);
            PanelShow                  out;
            check(PanelShow::decode(sr, out) && !out.open,
                  "and a panel that closed is a message too, saying nothing is up");
        }

        // What a decoder refuses. Every one of these is a byte string a peer
        // can send; none of them is a panel this build knows how to draw.
        {
            PanelShow bad = p;
            bad.elements[0].kind = 200;
            const std::vector<uint8_t> bw = bad.encode();
            ByteReader                 br(bw);
            PanelShow                  out;
            check(!PanelShow::decode(br, out),
                  "an element of a kind this build cannot draw is refused, not drawn as kind 0");
        }
        {
            PanelShow bad = p;
            bad.w = 500.0f;
            const std::vector<uint8_t> bw = bad.encode();
            ByteReader                 br(bw);
            PanelShow                  out;
            check(!PanelShow::decode(br, out), "a panel wider than any screen is refused");
        }
        {
            PanelShow bad = p;
            bad.name.clear();
            const std::vector<uint8_t> bw = bad.encode();
            ByteReader                 br(bw);
            PanelShow                  out;
            check(!PanelShow::decode(br, out),
                  "and one that is up with no name, because a click on it could name nothing");
        }
        {
            PanelShow many = p;
            many.elements.clear();
            for (size_t i = 0; i < kMaxWirePanelElements + 40; ++i)
                many.elements.push_back(PanelShow::Elem{(uint8_t)ui::ElemKind::Label, "", "x",
                                                        0.0f, 0.0f, 0.0f, 0.0f, 0, 0, 0});
            const std::vector<uint8_t> mw = many.encode();
            ByteReader                 br(mw);
            PanelShow                  out;
            check(PanelShow::decode(br, out) && out.elements.size() == kMaxWirePanelElements,
                  "a panel with more elements than the cap goes out carrying exactly the cap");
        }

        // And the other half of that, which the encoder cannot be asked about:
        // a count off the wire. Built by hand, because encode() truncates and
        // so can never produce this -- a peer that is not this code can. What
        // the check is really about is that the number is refused BEFORE it is
        // used as a loop bound or a reserve.
        {
            ByteWriter bw;
            bw.u8((uint8_t)MsgType::PanelShow);
            bw.u8(1);
            bw.str("console:menu");
            bw.str("console");
            bw.f32(6.0f);
            bw.f32(5.0f);
            bw.u16(60000);   // and then nothing at all

            const std::vector<uint8_t> lie = bw.data();
            ByteReader                 lr(lie);
            PanelShow                  out;
            check(!PanelShow::decode(lr, out),
                  "and a message claiming 60000 elements in 30 bytes is refused");

            // BEFORE anything is allocated for it, which is the half that
            // matters and the half a "did it refuse" check cannot see: a cap
            // raised past 60000 still refuses -- on the first field of the
            // first element, after reserving room for sixty thousand. That is
            // a two-byte lie costing megabytes, and bytes.h promises it cannot
            // happen. Capacity is how the promise is measured.
            check(out.elements.capacity() <= kMaxWirePanelElements,
                  "and refused by the COUNT, so nothing was reserved for a number "
                  "that was never checked");
        }

        UiClick c;
        c.panel = "console:menu";
        c.id    = "build";
        const std::vector<uint8_t> cw = c.encode();
        check(peek_type(cw.data(), cw.size()) == MsgType::UiClick, "a click goes out as a click");
        {
            ByteReader cr(cw);
            UiClick    out;
            check(UiClick::decode(cr, out) && out.panel == c.panel && out.id == c.id,
                  "and names the panel and the button, and nothing else");
        }
        {
            UiClick                    empty;
            const std::vector<uint8_t> ew = empty.encode();
            ByteReader                 cr(ew);
            UiClick                    out;
            check(!UiClick::decode(cr, out),
                  "while a click that names neither is refused before it costs a lookup");
        }

        std::printf("        a panel of 3 elements is %zu bytes, a click is %zu\n",
                    wire.size(), cw.size());
    }

    std::printf("\nhow long a session may say nothing\n");
    {
        // TODO.md OPEN-12. The server gave every Ready session a flat twelve
        // seconds and the client gives a join kHandshakeRtts round trips, so
        // past a 300 ms link the server always gave up first -- it dropped a
        // player for not answering a welcome that was still being retransmitted
        // to them, and the client's own window never got to mean anything.
        Session player;
        player.spoke_since_welcome = true;
        player.joined_after        = 0.377;   // a 400 ms link, measured
        check(join_patience(player) == kSessionSilence,
              "a session that has spoken since its welcome gets the player's rule");

        Session joining = player;
        joining.spoke_since_welcome = false;
        check(join_patience(joining) > kSessionSilence,
              "and one that has not gets longer, because it has nothing to say yet");

        Session local;
        local.spoke_since_welcome = false;
        local.joined_after        = 0.0003;   // loopback
        check(join_patience(local) == kSessionSilence,
              "on a link with no latency in it nothing changes");

        // The property, across every link rather than at one point on it: the
        // server outlasts the client's own join window. Ties are not enough --
        // the client is the side that can retry, and it cannot retry a server
        // that has already forgotten it.
        bool outlasts = true;
        double worst_ratio = 1e9;
        for (int i = 1; i <= 200; ++i) {
            const double rtt = (double)i * 0.005;   // 5 ms to a second
            Session waiting;
            waiting.spoke_since_welcome = false;
            waiting.joined_after        = rtt;   // a handshake is about one round trip

            const double client_gives = rtt * kHandshakeRtts;
            const double server_gives = join_patience(waiting);
            if (server_gives <= client_gives) outlasts = false;
            const double ratio = server_gives / (client_gives > 0.0 ? client_gives : 1.0);
            if (ratio < worst_ratio) worst_ratio = ratio;
        }
        check(outlasts, "and at every link the server outlasts the client's own join window");
        std::printf("        the server's patience is never less than %.2fx the client's\n",
                    worst_ratio);
    }

    std::printf("\nsilence the server itself caused\n");
    {
        // TODO.md OPEN-13, seen once on `bad_link`: a player stepped through a
        // portal, the far server admitted them -- "came out of this world's
        // opening 1 at 61 20 61" -- and dropped them twelve seconds later for
        // saying nothing, with the world still being retransmitted to them.
        // join_patience does not reach this: the handshake there was fast, so
        // it gave the floor, and the floor ran out while the server was talking.
        Session player;
        player.spoke_since_welcome = true;
        player.last_heard_at       = 100.0;

        check(quiet_for(player, 105.0) == 5.0,
              "a session nothing is owed to has been quiet since the last thing it said");

        // The same session, with the server still sending at 110.
        Session sending      = player;
        sending.last_owed_at = 110.0;
        check(quiet_for(sending, 110.0) == 0.0,
              "one the server is still answering is not quiet at all, whatever it last said");
        check(quiet_for(sending, 118.0) == 8.0,
              "and the clock starts when the answer stops, not when the client last spoke");

        // Which is the whole point: at 118 the old subtraction says eighteen
        // seconds and drops it, and the client has had eight.
        check(118.0 - sending.last_heard_at > join_patience(sending),
              "the old rule would have let this session go");
        check(quiet_for(sending, 118.0) < join_patience(sending),
              "and this one keeps it, because most of that was the server talking");

        // But not for ever, and this is the half that stops it being a rule
        // that never lets go: once the sending stops, the full window runs from
        // there and no longer.
        check(quiet_for(sending, 110.0 + join_patience(sending) + 0.001)
                  > join_patience(sending),
              "a seat is still freed, one patience after the last thing was owed");

        // An owing in the PAST must not shorten anything. Later of the two, not
        // the owing on its own.
        Session stale      = player;
        stale.last_owed_at = 40.0;   // long before it last spoke
        check(quiet_for(stale, 105.0) == quiet_for(player, 105.0),
              "an answer finished before the client last spoke changes nothing");

        // And a session that has never been owed anything is exactly what it
        // was before this field existed.
        Session untouched;
        untouched.last_heard_at = 7.0;
        check(untouched.last_owed_at == 0.0 && quiet_for(untouched, 20.0) == 13.0,
              "a session nobody has told anything about is judged as it always was");
    }

    std::printf("\n%s\n", g_failures == 0 ? "all protocol checks passed"
                                          : "PROTOCOL CHECKS FAILED");
    return g_failures == 0 ? 0 : 1;
}
