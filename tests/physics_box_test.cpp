// One rule for "is this position inside a block".
//
// There are two functions that answer it and they answer for different people.
// `box_hits_world` decides where a player can WALK: the collision resolver
// refuses any step that ends in a solid, so it is what puts the player where
// they are. `inside_solid` decides which positions are ACCEPTED: the server's
// movement validator refuses a player inside stone, and a portal's arrival
// refuses to put one there.
//
// They have to agree. When they do not, the player walks somewhere legal and is
// then told they are inside a wall -- and the failure is invisible, because
// nothing crashes and nothing looks wrong. It happened: the two rounded the far
// face of the box differently, `box_hits_world` treating a box edge that lands
// on a node boundary as touching the node behind it and `inside_solid` treating
// it as entering the node in front. One position in ten has a coordinate on a
// boundary, and one of those refused a portal crossing outright -- the player
// walked through the opening and came out the far side of the same wall, told
// nothing. Reported from play as "ogni tanto il teleport fallisce".
//
// The comment at the top of movement.cpp had predicted the shape of it, for two
// constants rather than two rules: "when they disagree the server refuses
// positions the player reached legally, which is the worst failure this code
// has". This is the check that was missing under it.

#include "core/physics/aabb.h"

#include "core/net/protocol.h"
#include "core/physics/movement.h"
#include "core/physics/player.h"
#include "core/physics/vehicle.h"
#include "core/registry/core_nodes.h"
#include "core/track/piece.h"
#include "core/voxel/chunk.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

using namespace cnp;

namespace {

int g_failures = 0;

void check(bool ok, const char *what)
{
    std::printf(ok ? "  ok    %s\n" : "  FAIL  %s\n", what);
    if (!ok) ++g_failures;
}

} // namespace

int main()
{
    std::printf("the player box against the world\n");

    NodeRegistry reg;
    register_core_nodes(reg);
    const BlockId stone = reg.find("core:stone");
    const BlockId air   = reg.find("core:air");

    World world;

    // A floor to stand on, and one solid node with air all around it at head
    // height: every face, edge and corner of it is a boundary the two rules
    // have to round the same way.
    for (int x = 0; x < 16; ++x)
        for (int z = 0; z < 16; ++z)
            world.set_block(x, 3, z, stone);
    world.set_block(8, 5, 8, stone);
    world.set_block(8, 4, 8, stone);

    // A wall with a one-node doorway in it, which is the case that found this:
    // a hole exactly one node wide, a box six tenths wide, and the two tenths
    // of play on each side ending exactly on a boundary.
    for (int y = 4; y <= 7; ++y)
        for (int z = 0; z < 16; ++z)
            if (z != 12) world.set_block(11, y, z, stone);
    world.set_block(11, 4, 12, air);
    world.set_block(11, 5, 12, air);

    // ------------------------------------------------- the two rules agree
    //
    // Swept a fortieth of a node at a time through the doorway, past the
    // standing block and along the floor, which is thousands of positions and
    // every one of them a place a player could be.
    {
        long   total = 0, disagree = 0;
        double first_x = 0.0, first_y = 0.0, first_z = 0.0;

        for (int ix = 380; ix <= 500; ++ix) {
            for (int iz = 440; iz <= 520; ++iz) {
                for (int iy = 160; iy <= 260; ++iy) {
                    const float x = 0.025f * (float)ix;
                    const float y = 0.025f * (float)iy;
                    const float z = 0.025f * (float)iz;
                    ++total;

                    const bool walk = box_hits_world(world, reg, player_box(Vec3{x, y, z}));
                    const bool live = inside_solid(world, reg, x, y, z);
                    if (walk == live) continue;

                    // The vertical is allowed to differ, and only in the
                    // forgiving direction: inside_solid keeps a twentieth of a
                    // node of slack at the feet and at the scalp on purpose, so
                    // a position that has settled a hair into the floor is
                    // still accepted. A box that hits nothing must never be
                    // called solid; that direction is the failure.
                    if (walk && !live) continue;

                    if (disagree == 0) { first_x = x; first_y = y; first_z = z; }
                    ++disagree;
                }
            }
        }

        std::printf("        %ld positions, %ld where inside_solid says solid and the "
                    "collision box hits nothing\n", total, disagree);
        if (disagree > 0)
            std::printf("        first at %.3f %.3f %.3f\n", first_x, first_y, first_z);
        check(total > 500000, "the sweep covers enough positions to be worth reading");
        check(disagree == 0,
              "no position the collision resolver allows is called inside a block");
    }

    // ---------------------------------------------- the boundary, by hand
    //
    // The case in one line, derived rather than swept: standing IN the one-node
    // doorway -- x = 11.5, so the box is inside the wall's own node layer and
    // the only thing keeping it legal is the hole -- and pushed sideways until
    // its edge reaches the wall.
    //
    // The doorway is node z = 12, spanning 12.0 to 13.0. A box three tenths
    // wide centred at 12.7 has its far face at exactly 13.0: on the boundary,
    // touching node 13 without entering it, and both rules must call it free.
    // At 12.8 the face is at 13.1, inside node 13, and both must call it solid.
    // Then the same on the other side, because a rule can be wrong at one end
    // and right at the other.
    {
        const float y = 4.0f;
        const float x = 11.5f;

        check(!box_hits_world(world, reg, player_box(Vec3{x, y, 12.7f})) &&
                  !inside_solid(world, reg, x, y, 12.7f),
              "a box edge exactly on a node boundary is not inside that node");
        check(box_hits_world(world, reg, player_box(Vec3{x, y, 12.8f})) &&
                  inside_solid(world, reg, x, y, 12.8f),
              "and a tenth past it is");
        check(!box_hits_world(world, reg, player_box(Vec3{x, y, 12.3f})) &&
                  !inside_solid(world, reg, x, y, 12.3f),
              "the same at the other edge of the doorway");
        check(box_hits_world(world, reg, player_box(Vec3{x, y, 12.2f})) &&
                  inside_solid(world, reg, x, y, 12.2f),
              "and a tenth past that one too");
    }

    // -------------------------------------------------- a step past a wall
    //
    // The resolver tests where a step ENDS, not what it crosses. A box six
    // tenths wide misses a wall one node thick entirely once a single step is
    // longer than 1.6 nodes: before the step it is all on one side, after it
    // all on the other. At the client's longest step, a twentieth of a second,
    // that is 32 m/s.
    //
    // move_box keeps that property, because it is one step and says so; what a
    // body moving under its own speed calls is move_body, which walks the same
    // delta in steps of at most kMaxStep. The first line below is the defect
    // measured, and it is checked to STILL be there: a gate that stopped seeing
    // it would be a gate whose second line proves nothing.
    {
        const Vec3 from{9.5f, 4.0f, 5.0f};   // on the floor, facing the wall at x = 11
        const Vec3 step{3.0f, 0.0f, 0.0f};   // 60 m/s over a twentieth of a second

        const MoveResult one = move_box(world, reg, from, step);
        std::printf("        one step of 3 nodes from x = %.2f ends at x = %.2f\n",
                    (double)from.x, (double)one.pos.x);
        check(one.pos.x > 12.0f, "one resolver step of three nodes ends past a wall (the defect)");

        const MoveResult walked = move_body(world, reg, from, step);
        std::printf("        the same delta walked by move_body ends at x = %.4f\n",
                    (double)walked.pos.x);
        // Short of it, and by at most one step. The resolver refuses a blocked
        // step whole rather than moving the body up to the obstacle -- which is
        // what it always did, at a walking step of a tenth of a node -- so a
        // body at speed stops up to half a node away. Measured: 10.50 against
        // a face at 10.70. Said here rather than found in play.
        check(walked.pos.x < 11.0f - kPlayerHalfWidth + 0.001f &&
                  walked.pos.x >= 11.0f - kPlayerHalfWidth - kMaxStep - 0.001f,
              "and a body moved by move_body stops in front of it, within one step");

        // And a box much thinner than a node, which is what a mod's shape can
        // be: a pane a sixteenth thick, standing where nothing else is.
        NodeRegistry shaped;
        register_core_nodes(shaped);
        NodeDef pane_def;
        pane_def.name  = "test:pane";
        pane_def.solid = true;
        NodeDef::Box pane_box;
        pane_box.min[0] = 0.5f - 1.0f / 32.0f;
        pane_box.max[0] = 0.5f + 1.0f / 32.0f;
        pane_def.parts.push_back(pane_box);
        const BlockId pane = shaped.register_node(pane_def);

        World thin;
        for (int x = 0; x < 16; ++x)
            for (int z = 0; z < 16; ++z)
                thin.set_block(x, 3, z, shaped.find("core:stone"));
        for (int y = 4; y <= 6; ++y)
            for (int z = 0; z < 16; ++z)
                thin.set_block(8, y, z, pane);

        const Vec3 at{8.0f, 4.0f, 5.0f};   // two tenths short of the pane's near face
        const MoveResult through = move_box(thin, shaped, at, Vec3{1.0f, 0.0f, 0.0f});
        const MoveResult stopped = move_body(thin, shaped, at, Vec3{1.0f, 0.0f, 0.0f} * 26.0f);
        std::printf("        a pane a sixteenth thick at x = 8.5: one step of 1 node ends at "
                    "%.2f, 26 nodes walked end at %.4f\n",
                    (double)through.pos.x, (double)stopped.pos.x);
        check(through.pos.x > 8.5f, "one step of a node ends past a pane (the same defect, thinner)");
        check(stopped.pos.x < 8.5f - 1.0f / 32.0f - kPlayerHalfWidth + 0.001f,
              "and the longest step a world may declare, walked, stops at it");
    }

    // ------------------------------------ the server's copy of a kart
    //
    // The server drives a kart with the keys its client's claims carry
    // (TODO.md OPEN-47) and refuses a claim that is not where its own copy
    // went. That is only fair if the copy IS the client's kart: the same
    // records, through the wire, into the same chassis, must come out at the
    // same place to the last bit. A difference of one float here is a
    // correction in play that nothing on screen explains.
    std::printf("\nthe server's copy of a kart\n");
    {
        World road;
        for (int x = -8; x < 72; ++x)
            for (int z = -40; z < 40; ++z) road.set_block(x, 3, z, stone);
        for (int y = 4; y <= 5; ++y)   // a wall to hit, at an angle to the way it drives
            for (int z = -40; z < 40; ++z) road.set_block(40 + z / 4, y, z, stone);

        MovementRules rules;
        rules.walk = 8.0f; rules.sprint = 24.0f; rules.jump = 8.0f; rules.gravity = -25.0f;
        rules.accel = 12.0f; rules.brake = 30.0f; rules.turn = 2.6f; rules.drag = 0.6f;
        rules.width = 1.4f; rules.length = 2.6f; rules.rise = 10.0f;

        // The client: frames of two lengths, throttle and the wheel, a coast, a
        // jump, and a correction half way that puts the chassis down afresh.
        Player                  client;
        client.pos = Vec3{8.0f, 4.0f, 0.5f};
        client.yaw = 1.5708f;   // along +x, towards the wall
        std::vector<DriveEvent> log;
        float                   t = 0.0f;
        for (int frame = 0; t < 4.0f; ++frame) {
            const float dt = (frame % 3 == 0) ? 1.0f / 50.0f : 1.0f / 144.0f;
            PlayerInput in;
            in.move_forward = (t > 1.5f && t < 2.0f) ? 0.0f : 1.0f;
            in.move_right   = t < 2.0f ? 0.4f : -0.6f;
            in.jump         = t > 1.0f && t < 1.05f;
            in.sprint       = t > 2.5f && t < 3.5f;   // the nitro, which the copy burns too
            in.descend      = t > 0.6f && t < 1.4f;   // and the drift, the rear tyres let go
            if (frame == 200) {
                client.velocity = client.velocity * 0.5f;
                client.vehicle.reset();
            }
            player_update(client, road, reg, rules, in, dt, &log);
            t += dt;
        }

        // Through the wire, as the claims carry them.
        std::vector<net::DriveRecord> sent;
        for (size_t at = 0; at < log.size(); at += net::kMaxDriveRecords) {
            net::Move m;
            for (size_t i = at; i < log.size() && i < at + net::kMaxDriveRecords; ++i)
                m.drive.push_back(net::DriveRecord{log[i], 0});
            const std::vector<uint8_t> buf = m.encode();
            ByteReader                 r(buf);
            net::Move                  back;
            check(net::Move::decode(r, back) && back.drive.size() == m.drive.size(),
                  "a claim's records survive the wire");
            sent.insert(sent.end(), back.drive.begin(), back.drive.end());
        }

        const MayMiss any = [](ChunkPos) { return true; };
        const auto replay = [&](const std::vector<net::DriveRecord> &records) {
            Player copy;
            for (const net::DriveRecord &r : records) {
                if (r.event.put) {
                    copy.pos      = r.event.pos;
                    copy.velocity = r.event.vel;
                    copy.yaw      = r.event.yaw;
                    vehicle_put(copy, road, reg, rules, r.event, any);
                } else {
                    vehicle_step(copy, road, reg, rules, r.event, any);
                }
            }
            return copy;
        };

        size_t puts = 0, jumps = 0, boosts = 0;
        for (const DriveEvent &e : log)
            puts += e.put, jumps += (!e.put && e.keys.jump), boosts += (!e.put && e.keys.boost);
        const Player copy = replay(sent);
        Vec3 cw, cn, sw, sn;
        vehicle_feet(client, cw, cn);
        const bool has = vehicle_feet(copy, sw, sn);
        std::printf("        %zu records, %zu puts, %zu steps with jump; the client's kart at "
                    "%.4f %.4f %.4f, going %.2f\n",
                    log.size(), puts, jumps, (double)cn.x, (double)cn.y, (double)cn.z,
                    (double)length(client.velocity));
        check(puts == 2 && jumps > 0 && cn.x > 15.0f, "the kart went somewhere, and was put down twice");
        check(has && sn.x == cn.x && sn.y == cn.y && sn.z == cn.z && sw.x == cw.x &&
                  sw.z == cw.z && copy.velocity.x == client.velocity.x &&
                  copy.velocity.z == client.velocity.z && boosts > 0 && copy.nitro == client.nitro &&
                  client.nitro < 1.0f,
              "the server's copy ends where the client's kart is, to the last bit, nitro and all");

        // The drift: flat out, then half the wheel for two seconds, with
        // ctrl and without. With it the tail slides out (DriveLook::sliding)
        // and the kart turns further.
        {
            World pad;
            for (int x = -80; x < 400; ++x)
                for (int z = -120; z < 120; ++z) pad.set_block(x, 3, z, stone);
            const auto bend = [&](bool drift, int &slid) {
                Player kart;
                kart.pos = Vec3{0.0f, 4.0f, 0.5f};
                kart.yaw = 1.5708f;
                slid     = 0;
                float turned = 0.0f;
                for (int i = 0; i < 60 * 5; ++i) {
                    PlayerInput in;
                    in.move_forward = 1.0f;
                    in.move_right   = i >= 60 * 3 ? 0.5f : 0.0f;
                    in.descend      = drift && i >= 60 * 3;
                    const float was = kart.yaw;
                    player_update(kart, pad, reg, rules, in, 1.0f / 60.0f);
                    turned += std::remainder(kart.yaw - was, 6.2831853f);
                    slid += vehicle_look(kart).sliding;
                }
                return turned;
            };
            int        grip_slid = 0, drift_slid = 0;
            const float grip  = bend(false, grip_slid);
            const float drift = bend(true, drift_slid);
            std::printf("        a bend at half the wheel: turned %.2f rad sliding %d steps; with ctrl %.2f "
                        "rad sliding %d steps\n",
                        (double)grip, grip_slid, (double)drift, drift_slid);
            check(drift_slid > grip_slid + 30 && std::fabs(drift) > std::fabs(grip),
                  "ctrl lets the tail go: the kart slides and turns further");
        }

        // The nitro on a long straight: flat out to the top speed, then shift
        // until the gauge is empty. Past the top, never past kNitroTop times
        // it, and back down after.
        {
            World strip;
            for (int x = -8; x < 400; ++x)
                for (int z = -24; z < 24; ++z) strip.set_block(x, 3, z, stone);
            Player kart;
            kart.pos = Vec3{0.0f, 4.0f, 0.5f};
            kart.yaw = 1.5708f;
            float top = 0.0f, boosted = 0.0f, after = 0.0f, empty_at = -1.0f;
            for (int i = 0; i < 60 * 12; ++i) {
                const float at = (float)i / 60.0f;
                PlayerInput in;
                in.move_forward = 1.0f;
                in.sprint       = at >= 5.0f && at < 9.0f;
                player_update(kart, strip, reg, rules, in, 1.0f / 60.0f);
                if (at < 5.0f) top = std::max(top, kart.drive_speed);
                else if (at < 9.0f) boosted = std::max(boosted, kart.drive_speed);
                else after = kart.drive_speed;
                if (empty_at < 0.0f && kart.nitro <= 0.0f) empty_at = at;
            }
            std::printf("        flat out %.2f m/s, with the nitro %.2f, 3 s after it %.2f; "
                        "empty at %.2f s, %.2f back after 3 s, at %.1f %.1f %.1f\n",
                        (double)top, (double)boosted, (double)after, (double)empty_at,
                        (double)kart.nitro, (double)kart.pos.x, (double)kart.pos.y, (double)kart.pos.z);
            check(top <= rules.sprint + 0.01f && boosted > rules.sprint + 3.0f &&
                      boosted <= rules.sprint * kNitroTop + 0.01f && after <= rules.sprint + 0.5f &&
                      empty_at > 7.5f && empty_at < 8.5f && kart.nitro > 0.0f && kart.nitro < 0.3f,
                  "the nitro takes a kart past its top speed, runs out, and comes back slowly");
        }

        // Up two terraces one high, then against a wall two high: a kart
        // hops onto a step and not over a track's wall.
        {
            World hills;
            for (int x = -8; x < 60; ++x)
                for (int z = -40; z < 40; ++z) {
                    const int top = x < 20 ? 3 : x < 30 ? 4 : x < 40 ? 5 : 7;
                    for (int y = 3; y <= top; ++y) hills.set_block(x, y, z, stone);
                }
            Player kart;
            kart.pos = Vec3{0.0f, 4.0f, 0.5f};
            kart.yaw = 1.5708f;
            for (int i = 0; i < 60 * 10; ++i) {
                PlayerInput in;
                in.move_forward = 1.0f;
                player_update(kart, hills, reg, rules, in, 1.0f / 60.0f);
            }
            std::printf("        up the terraces to %.2f %.2f %.2f\n", (double)kart.pos.x,
                        (double)kart.pos.y, (double)kart.pos.z);
            check(kart.pos.x > 30.0f && kart.pos.x < 40.0f && std::fabs(kart.pos.y - 6.0f) < 0.1f,
                  "a kart hops up a step one high, and stops at a wall two high");
        }

        // A client that has not been sent everything: the same drive over a
        // world without the chunk behind x 0, which its ground reaches into at
        // the start and the server's has. Built without what the records say
        // it lacked, the copy is still the client's kart; built with it, even
        // road the kart never reaches changes the order Jolt meets the rest in.
        {
            World partial;
            for (int x = 0; x < 72; ++x)
                for (int z = -40; z < 40; ++z) partial.set_block(x, 3, z, stone);
            for (int y = 4; y <= 5; ++y)
                for (int z = -40; z < 40; ++z) partial.set_block(40 + z / 4, y, z, stone);
            Player                  lacking;
            lacking.pos = Vec3{3.0f, 4.0f, 0.5f};
            lacking.yaw = 1.5708f;
            std::vector<DriveEvent> plog;
            float                   pt = 0.0f;
            for (int frame = 0; pt < 2.5f; ++frame) {
                const float dt = (frame % 3 == 0) ? 1.0f / 50.0f : 1.0f / 144.0f;
                PlayerInput in;
                in.move_forward = 1.0f;
                in.move_right   = 0.3f;
                player_update(lacking, partial, reg, rules, in, dt, &plog);
                pt += dt;
            }
            size_t lacked = 0;
            for (const DriveEvent &e : plog) lacked += e.built && e.missing != 0;

            const auto copy_of = [&](const MayMiss &may) {
                Player built;
                for (const DriveEvent &e : plog) {
                    if (e.put) {
                        built.pos = e.pos; built.velocity = e.vel; built.yaw = e.yaw;
                        vehicle_put(built, road, reg, rules, e, may);
                    } else {
                        vehicle_step(built, road, reg, rules, e, may);
                    }
                }
                Vec3 w, n;
                vehicle_feet(built, w, n);
                return n;
            };
            Vec3 lw, ln;
            vehicle_feet(lacking, lw, ln);
            const Vec3 with    = copy_of(any);
            const Vec3 without = copy_of([](ChunkPos) { return false; });
            std::printf("        %zu builds without a chunk; the copy built without it %s, with it "
                        "%.6f off\n",
                        lacked, (with.x == ln.x && with.y == ln.y && with.z == ln.z) ? "exact" : "OFF",
                        (double)length(without - ln));
            check(lacked > 0 && with.x == ln.x && with.y == ln.y && with.z == ln.z,
                  "a client short of a chunk is copied from what it had");
        }

        // And the keys are what decide it: the wheel turned the other way after
        // the last put ends further off than a claim may be.
        std::vector<net::DriveRecord> other = sent;
        size_t last_put = 0;
        for (size_t i = 0; i < other.size(); ++i)
            if (other[i].event.put) last_put = i;
        for (size_t i = last_put; i < other.size(); ++i) other[i].event.keys.right *= -1.0f;
        Vec3 hw, hn;
        const Player pushed = replay(other);
        vehicle_feet(pushed, hw, hn);
        std::printf("        with the wheel the other way after the last put: %.2f from it\n",
                    (double)length(hn - cn));
        check(length(hn - cn) > 0.35f, "and other keys drive it somewhere a claim cannot be");
    }

    // A track piece is two things that must agree: the road a kart drives on
    // and draws, and the boxes under it that a foot, a ray and the server's
    // "inside a block" see. A box standing above the road is a kart the server
    // calls inside a block; a cell piece_find cannot trace is a piece nobody
    // can break. And the road is driven by both ends, so it is the same road.
    std::printf("\ntrack pieces\n");
    {
        const MayMiss any_chunk = [](ChunkPos) { return true; };
        NodeRegistry preg;
        register_core_nodes(preg);
        NodeDef fill_def;
        fill_def.name = "test:fill";
        const BlockId fill = preg.register_node(fill_def);
        const auto piece = [&](const char *name, int plan, int size, int climb, int g0, int g1,
                               int b0, int b1) {
            NodeDef d;
            d.name           = name;
            const BlockId id = preg.register_node(d);
            PieceShape    p;
            p.plan = (uint8_t)plan, p.size = (uint8_t)size, p.climb = (int8_t)climb;
            p.grade0 = (int8_t)g0, p.grade1 = (int8_t)g1, p.bank0 = (int8_t)b0, p.bank1 = (int8_t)b1;
            check(piece_declare(preg, id, fill, p), name);
            return id;
        };
        const BlockId tilt_in = piece("test:tilt_in", 1, 2, 0, 0, 0, 0, 4);
        const BlockId bank    = piece("test:bank_turn2", 2, 2, 0, 0, 0, 4, 4);
        const BlockId kicker  = piece("test:kicker", 1, 1, 2, 0, 4, 0, 0);
        const BlockId turn3   = piece("test:turn3", 2, 3, 0, 0, 0, 0, 0);
        const BlockId run     = piece("test:straight2", 1, 2, 0, 0, 0, 0, 0);
        const BlockId right   = piece("test:chicane_right", 3, 2, 0, 0, 0, 0, 0);
        const BlockId left    = piece("test:chicane_left", 4, 3, 2, 0, 0, 0, 0);
        // Wide: a road two squares across, a banked turn of it, a tilt, and
        // turns to the left.
        const auto wide = [&](const char *name, int plan, int size, int b0, int b1) {
            NodeDef d;
            d.name           = name;
            const BlockId id = preg.register_node(d);
            PieceShape    p;
            p.plan = (uint8_t)plan, p.size = (uint8_t)size, p.bank0 = (int8_t)b0, p.bank1 = (int8_t)b1;
            p.width = 2;
            check(piece_declare(preg, id, fill, p), name);
            return id;
        };
        const BlockId wide_bank = wide("test:wide_bank_turn3", 2, 3, 8, 8);
        const BlockId wide_tilt = wide("test:wide_tilt_in", 1, 3, 0, 8);
        const BlockId wide_left = wide("test:wide_turn2_left", 5, 2, 0, 0);
        const BlockId left_one  = piece("test:turn2_left", 5, 2, 0, 0, 0, 0, 0);
        // Loops out to either side, on one lane and two, and the adapters
        // from two lanes down to one.
        const BlockId loop_l      = piece("test:loop_left", 6, 2, 0, 0, 0, 0, 0);
        const BlockId loop_r      = piece("test:loop_right", 7, 3, 0, 0, 0, 0, 0);
        const BlockId wide_loop_l = wide("test:wide_loop_left", 6, 2, 0, 0);
        const BlockId wide_loop_r = wide("test:wide_loop_right", 7, 2, 0, 0);
        const BlockId to_right    = wide("test:wide_to_right", 8, 2, 0, 0);
        const BlockId to_left     = wide("test:wide_to_left", 9, 1, 0, 0);

        float  worst = -1.0f;
        size_t lost  = 0, cells = 0;
        for (const BlockId id : {tilt_in, bank, kicker, turn3, right, left, wide_bank, wide_tilt,
                                 wide_left, left_one, loop_l, loop_r, wide_loop_l, wide_loop_r,
                                 to_right, to_left})
            for (uint8_t f = 0; f < 4; ++f) {
                const PieceShape &p = preg.get(id).piece;
                int               nx, nz;
                piece_size(p, f, nx, nz);
                for (int cz = 0; cz < nz; ++cz)
                    for (int cx = 0; cx < nx; ++cx) {
                        int top32 = 0;
                        if (!piece_column(p, f, cx, cz, top32)) continue;
                        const float top = (float)top32 / 32.0f;
                        for (int i = 0; i <= 16; ++i)
                            for (int j = 0; j <= 16; ++j) {
                                float y;
                                if (piece_surface(p, f, (float)cx + (float)i / 16.0f,
                                                  (float)cz + (float)j / 16.0f, y))
                                    worst = std::max(worst, top - y);
                            }
                    }
                World                  w;
                std::vector<PieceCell> put;
                piece_cells(p, id, f, 16, 10, -24, put);
                for (const PieceCell &c : put) {
                    w.set_block(c.x, c.y, c.z, c.id);
                    w.set_facing(c.x, c.y, c.z, c.facing);
                }
                for (const PieceCell &c : put) {
                    int ox, oy, oz;
                    ++cells;
                    if (!piece_find(w, preg, c.x, c.y, c.z, ox, oy, oz) || ox != 16 || oy != 10 || oz != -24)
                        ++lost;
                }
            }
        std::printf("        the boxes stand at most %.4f above the road; %zu of %zu cells lead "
                    "nowhere\n", (double)worst, lost, cells);
        check(worst <= 0.0f, "no box under a piece stands above its road");
        check(lost == 0, "every cell of a piece leads back to its corner");

        // A kart along a tilt and into a banked turn: flat stone below, the
        // pieces four nodes up, driven by a client and copied by a server. The
        // driver holds the middle of the road at 18 m/s. A bank tipped about
        // its low edge had the middle climb two nodes into it, and a kart flew
        // off the top and landed into the road (reported from play: "compenetra
        // la pista e scatta di nuovo in posizione"): 0.39 under the road and a
        // vertical speed changing by 2 to 3 m/s in one step.
        World ring;
        for (int x = -40; x < 40; ++x)
            for (int z = -32; z < 40; ++z) ring.set_block(x, 0, z, stone);
        std::vector<PieceCell> put;
        const auto lay = [&](BlockId id, uint8_t f, int x, int z) {
            int ox, oz;
            piece_origin(preg.get(id).piece, f, x, z, ox, oz);
            piece_cells(preg.get(id).piece, id, f, ox, 4, oz, put);
            for (const PieceCell &c : put) {
                ring.set_block(c.x, c.y, c.z, c.id);
                ring.set_facing(c.x, c.y, c.z, c.facing);
            }
        };
        lay(run, 2, 0, -24);     // a run up, entered from -z, heading +z
        lay(tilt_in, 2, 0, -8);
        lay(bank, 2, 0, 8);      // and on into a turn to the right, about (-8, 8)
        lay(run, 1, -9, 16);     // and out of it along -x

        MovementRules rules;
        rules.walk = 8.0f; rules.sprint = 24.0f; rules.jump = 8.0f; rules.gravity = -25.0f;
        rules.accel = 12.0f; rules.brake = 30.0f; rules.turn = 2.6f; rules.drag = 0.6f;
        rules.width = 1.4f; rules.length = 2.6f; rules.rise = 10.0f;

        // Where the middle of the road is a little ahead of p.
        const auto ahead = [](Vec3 p) {
            if (p.z < 8.0f && p.x > 0.0f) return Vec3{4.0f, 0.0f, p.z + 6.0f};
            if (p.x > -8.0f) {
                const float a = std::atan2(p.z - 8.0f, p.x + 8.0f) + 0.5f;
                return Vec3{-8.0f + 12.0f * std::cos(a), 0.0f, 8.0f + 12.0f * std::sin(a)};
            }
            return Vec3{p.x - 6.0f, 0.0f, 20.0f};
        };

        Player client;
        client.pos = Vec3{4.0f, 5.0f, -22.0f};
        client.yaw = 3.14159265f;   // along +z
        std::vector<DriveEvent> log;
        float under = 0.0f, jolt = 0.0f, last_vy = 0.0f;
        for (int frame = 0; frame < 300; ++frame) {
            Vec3        was, now;
            PlayerInput in;
            in.move_forward = length(client.velocity) > 18.0f ? 0.0f : 1.0f;
            if (vehicle_feet(client, was, now)) {
                const Vec3  to = ahead(now) - now;
                const float fx = std::sin(client.yaw), fz = -std::cos(client.yaw);
                const float e  = std::atan2(fx * to.z - fz * to.x, fx * to.x + fz * to.z);
                in.move_right  = std::clamp(2.0f * e, -1.0f, 1.0f);
            }
            player_update(client, ring, preg, rules, in, 1.0f / 60.0f, &log);
            if (!vehicle_feet(client, was, now)) continue;

            int ox, oy, oz;
            for (int cy = 0; cy < 10; ++cy)
                if (piece_find(ring, preg, (int)std::floor(now.x), cy, (int)std::floor(now.z), ox, oy, oz)) {
                    float y;
                    if (piece_surface(preg.get(ring.get_block(ox, oy, oz)).piece,
                                      ring.get_facing(ox, oy, oz), now.x - (float)ox,
                                      now.z - (float)oz, y))
                        under = std::max(under, (float)oy + y - now.y);
                    break;
                }
            if (frame > 60) jolt = std::max(jolt, std::fabs(client.velocity.y - last_vy));
            last_vy = client.velocity.y;
        }
        Vec3 cw, cn;
        vehicle_feet(client, cw, cn);

        Player copy;
        for (const DriveEvent &e : log) {
            if (e.put) {
                copy.pos = e.pos; copy.velocity = e.vel; copy.yaw = e.yaw;
                vehicle_put(copy, ring, preg, rules, e, any_chunk);
            } else {
                vehicle_step(copy, ring, preg, rules, e, any_chunk);
            }
        }
        Vec3 sw, sn;
        vehicle_feet(copy, sw, sn);
        std::printf("        along a tilt, round a banked turn and out: at most %.3f under the "
                    "road, the vertical speed changing by at most %.2f in a step; ends at "
                    "%.2f %.2f %.2f\n",
                    (double)under, (double)jolt, (double)cn.x, (double)cn.y, (double)cn.z);
        check(under < 0.1f, "a kart on a banked turn rides on its road, not through it");
        check(jolt < 1.0f, "and over the joins between pieces without being thrown");
        check(cn.x < -12.0f && std::fabs(cn.z - 20.0f) < 3.0f, "and comes out of the turn on the road");
        check(sn.x == cn.x && sn.y == cn.y && sn.z == cn.z,
              "and the server's copy drives the same road to the last bit");

        // A wall ride: the road rolled up into a wall and back, a driver who
        // keeps to the middle of it. Fast, the kart goes up, along and down
        // again onto the middle of the road; slow, it comes off the wall.
        const auto walled = [&](const char *name, int size, int from, int to) {
            NodeDef d;
            d.name           = name;
            const BlockId id = preg.register_node(d);
            PieceShape    p;
            p.plan = 1, p.size = (uint8_t)size, p.bank0 = (int8_t)from, p.bank1 = (int8_t)to;
            p.wall = 1, p.curbs = 0;
            check(piece_declare(preg, id, fill, p), name);
            return id;
        };
        const BlockId wall_in  = walled("test:wall_in", 2, 0, 90);
        const BlockId wall     = walled("test:wall", 1, 90, 90);
        const BlockId wall_out = walled("test:wall_out", 2, 90, 0);
        World side;
        for (int x = -24; x < 32; ++x)
            for (int z = -8; z < 120; ++z) side.set_block(x, 0, z, stone);
        int at_z = 0;
        for (const BlockId id : {run, wall_in, wall, wall_out, run}) {
            const PieceShape &p = preg.get(id).piece;
            int               ox, oz;
            piece_origin(p, 2, 0, at_z, ox, oz);
            piece_cells(p, id, 2, ox, 8, oz, put);
            for (const PieceCell &c : put) {
                side.set_block(c.x, c.y, c.z, c.id);
                side.set_facing(c.x, c.y, c.z, c.facing);
            }
            at_z += p.size * kPieceUnit;
        }
        const auto ride = [&](float top, std::vector<DriveEvent> *log, float &roll) {
            Player k;
            k.pos = Vec3{4.0f, 9.0f, 1.0f};
            k.yaw = 3.14159265f;
            roll  = 0.0f;
            float was_r = 0.0f;
            for (int frame = 0; frame < 600; ++frame) {
                Vec3        w0, n0;
                PlayerInput in;
                in.move_forward = length(k.velocity) > top ? 0.0f : 1.0f;
                if (vehicle_feet(k, w0, n0)) {
                    // How far right of the road's middle, across its face.
                    const float r = (4.0f - n0.x) * std::cos(k.body_roll) -
                                    (n0.y - 8.25f) * std::sin(k.body_roll);
                    in.move_right = std::clamp(-(0.35f * r + 7.2f * (r - was_r)), -1.0f, 1.0f);
                    was_r         = r;
                    if (n0.z > (float)at_z - 4.0f) break;
                }
                player_update(k, side, preg, rules, in, 1.0f / 60.0f, log);
                roll = std::max(roll, std::fabs(k.body_roll));
            }
            return k;
        };
        std::vector<DriveEvent> wlog;
        float                   fast_roll, slow_roll;
        const Player            fast = ride(18.0f, &wlog, fast_roll);
        const Player            slow = ride(8.0f, nullptr, slow_roll);
        Vec3 fw, fn, lw, ln;
        vehicle_feet(fast, fw, fn);
        vehicle_feet(slow, lw, ln);
        Player wcopy;
        for (const DriveEvent &e : wlog) {
            if (e.put) {
                wcopy.pos = e.pos; wcopy.velocity = e.vel; wcopy.yaw = e.yaw;
                vehicle_put(wcopy, side, preg, rules, e, any_chunk);
            } else {
                vehicle_step(wcopy, side, preg, rules, e, any_chunk);
            }
        }
        Vec3 ww, wn;
        vehicle_feet(wcopy, ww, wn);
        std::printf("        a wall ride at 18 m/s: rolled %.0f degrees, ends at %.2f %.2f %.2f; "
                    "at 8 m/s ends at %.2f %.2f %.2f\n",
                    (double)(fast_roll * 57.3f), (double)fn.x, (double)fn.y, (double)fn.z,
                    (double)ln.x, (double)ln.y, (double)ln.z);
        check(fast_roll > 1.4f && std::fabs(fn.x - 4.0f) < 1.0f && std::fabs(fn.y - 8.25f) < 0.3f,
              "a kart rides a wall at speed and comes down onto the middle of the road");
        check(ln.y < 8.0f, "and falls off it slow");
        check(wn.x == fn.x && wn.y == fn.y && wn.z == fn.z,
              "and the server's copy rides the same wall to the last bit");

        // A loop out to the left: a run up, the loop, and a run on from the
        // lane beside. Fast, the kart goes up, over upside down and down onto
        // the lane 8 to the left (+x), the driver steering the loop's S as a
        // chicane (unrolled, that is what it is); slow, it falls off it.
        World looped;
        for (int x = -16; x < 40; ++x)
            for (int z = -8; z < 72; ++z) looped.set_block(x, 0, z, stone);
        const auto put_at = [&](BlockId id, int x, int z) {
            const PieceShape &p = preg.get(id).piece;
            int               ox, oz;
            piece_origin(p, 2, x, z, ox, oz);
            piece_cells(p, id, 2, ox, 8, oz, put);
            for (const PieceCell &c : put) {
                looped.set_block(c.x, c.y, c.z, c.id);
                looped.set_facing(c.x, c.y, c.z, c.facing);
            }
        };
        put_at(run, 0, 0);
        put_at(loop_l, 0, 16);
        put_at(run, 8, 32);
        put_at(run, 8, 48);
        const auto loop_ride = [&](float top, std::vector<DriveEvent> *log, float &roll) {
            Player k;
            k.pos = Vec3{4.0f, 9.0f, 1.0f};
            k.yaw = 3.14159265f;
            roll  = 0.0f;
            float was_e = 0.0f, gone = 0.0f;
            for (int frame = 0; frame < 900; ++frame) {
                Vec3        w0, n0;
                PlayerInput in;
                in.move_forward = length(k.velocity) > top ? 0.0f : 1.0f;
                if (vehicle_feet(k, w0, n0)) {
                    // Along the loop's line (32 long, 3.13 flat before it),
                    // where its middle is across: 4, and 12 once over.
                    const float s  = std::clamp((gone - 3.13f) / 32.0f, 0.0f, 1.0f);
                    const float t  = std::clamp((s - 0.2f) / 0.6f, 0.0f, 1.0f);
                    const float cx = n0.z < 16.0f ? 4.0f : 4.0f + 8.0f * (0.5f - 0.5f * std::cos(t * 3.14159265f));
                    // Right is -x all the way round: a loop turns about x.
                    const float e = n0.x - cx;
                    in.move_right = std::clamp(0.35f * e + 7.2f * (e - was_e), -1.0f, 1.0f);
                    was_e         = e;
                    if (n0.z > 60.0f) break;
                }
                player_update(k, looped, preg, rules, in, 1.0f / 60.0f, log);
                if (k.pos.z >= 16.0f) gone += length(k.velocity) / 60.0f;
                roll = std::max(roll, std::fabs(k.body_roll));
            }
            return k;
        };
        std::vector<DriveEvent> llog;
        float                   over, slow_over;
        const Player            lfast = loop_ride(24.0f, &llog, over);
        const Player            lslow = loop_ride(7.0f, nullptr, slow_over);
        Vec3 lfw, lfn;
        vehicle_feet(lfast, lfw, lfn);
        Player lcopy;
        for (const DriveEvent &e : llog) {
            if (e.put) {
                lcopy.pos = e.pos; lcopy.velocity = e.vel; lcopy.yaw = e.yaw;
                vehicle_put(lcopy, looped, preg, rules, e, any_chunk);
            } else {
                vehicle_step(lcopy, looped, preg, rules, e, any_chunk);
            }
        }
        Vec3 lcw, ln2;
        vehicle_feet(lcopy, lcw, ln2);
        std::printf("        a loop flat out: rolled over to %.0f degrees, ends at %.2f %.2f %.2f; "
                    "at 7 m/s rolled to %.0f\n",
                    (double)(over * 57.3f), (double)lfn.x, (double)lfn.y, (double)lfn.z,
                    (double)(slow_over * 57.3f));
        check(over > 3.0f && lfn.z > 50.0f && std::fabs(lfn.x - 12.0f) < 2.0f &&
                  std::fabs(lfn.y - 8.25f) < 0.3f,
              "a kart goes round a loop upside down and comes out on the lane beside");
        check(slow_over < 1.6f, "and does not get over it slow");
        check(ln2.x == lfn.x && ln2.y == lfn.y && ln2.z == lfn.z,
              "and the server's copy goes round the same loop to the last bit");
    }

    std::printf("\na stair of slabs, walked up at 60 Hz\n");
    {
        // Slab, block, slab on a block, block on a block: four steps of half a
        // node. The body goes up each in one frame -- that is the physics, and
        // the server's -- while the eye (StepSmooth) goes up without a jump.
        NodeRegistry sreg;
        register_core_nodes(sreg);
        NodeDef slab_def;
        slab_def.name  = "test:slab";
        slab_def.solid = true;
        NodeDef::Box half;
        half.max[1] = 0.5f;
        slab_def.parts.push_back(half);
        const BlockId slab  = sreg.register_node(slab_def);
        const BlockId stone = sreg.find("core:stone");

        World stairs;
        for (int x = 0; x < 24; ++x)
            for (int z = 0; z < 8; ++z) stairs.set_block(x, 3, z, stone);
        for (int z = 0; z < 8; ++z) {
            stairs.set_block(8, 4, z, slab);
            stairs.set_block(9, 4, z, stone);
            stairs.set_block(10, 4, z, stone);
            stairs.set_block(10, 5, z, slab);
            for (int x = 11; x < 24; ++x) {
                stairs.set_block(x, 4, z, stone);
                stairs.set_block(x, 5, z, stone);
            }
        }

        Player p;
        p.pos       = Vec3{5.5f, 4.0f, 4.5f};
        p.yaw       = 1.5707963f;   // along +x
        p.on_ground = true;
        PlayerInput in;
        in.move_forward = 1.0f;
        StepSmooth smooth;

        float body_jump = 0.0f, eye_jump = 0.0f, eye_before = p.pos.y + smooth.lag;
        const float dt = 1.0f / 60.0f;
        for (int f = 0; f < 150; ++f) {
            const float y_before = p.pos.y;
            const bool  grounded = p.on_ground;
            player_update(p, stairs, sreg, MovementRules{}, in, dt);
            smooth.note(p, grounded, y_before);
            smooth.settle(dt);
            const float eye = p.pos.y + smooth.lag;
            body_jump = std::max(body_jump, p.pos.y - y_before);
            eye_jump  = std::max(eye_jump, eye - eye_before);
            eye_before = eye;
        }
        for (int f = 0; f < 30; ++f) smooth.settle(dt);
        std::printf("        up to y %.2f: the body rose at most %.3f in a frame, the eye %.3f\n",
                    (double)p.pos.y, (double)body_jump, (double)eye_jump);
        check(p.pos.y >= 5.99f, "the body walks up four half-node steps without a jump");
        check(body_jump >= 0.45f, "and takes each in one frame, as the server does");
        check(eye_jump <= 0.12f, "while the eye never rises more than 0.12 in a frame");
        check(smooth.lag == 0.0f, "and settles back on the body");

        // A stair, its high half towards +x: walked up from its front, climbed
        // onto (player.stepping) as a block one high only from its back.
        NodeDef stair_def;
        stair_def.name  = "test:stair";
        stair_def.solid = true;
        NodeDef::Box back;
        back.min[0] = 0.5f;
        back.min[1] = 0.5f;
        stair_def.parts.push_back(half);
        stair_def.parts.push_back(back);
        const BlockId stair = sreg.register_node(stair_def);
        World one;
        for (int x = 0; x < 16; ++x)
            for (int z = 0; z < 8; ++z) one.set_block(x, 3, z, stone);
        for (int z = 0; z < 8; ++z) one.set_block(8, 4, z, stair);
        auto walk = [&](float x, float yaw, float &top) {
            Player w;
            w.pos       = Vec3{x, 4.0f, 4.5f};
            w.yaw       = yaw;
            w.on_ground = true;
            bool climbed = false;
            top = 0.0f;
            for (int f = 0; f < 90; ++f) {
                player_update(w, one, sreg, MovementRules{}, in, dt);
                climbed = climbed || w.stepping;
                top     = std::max(top, w.pos.y);
            }
            return climbed;
        };
        float front_top = 0.0f, back_top = 0.0f;
        const bool front = walk(5.5f, 1.5707963f, front_top);
        const bool from_back = walk(11.5f, -1.5707963f, back_top);
        std::printf("        a stair from its front: up to y %.2f%s; from its back: %s\n",
                    (double)front_top, front ? ", climbed" : ", walked",
                    from_back ? "climbed" : "not climbed");
        check(!front && front_top >= 4.99f, "a stair is walked up from its front, not climbed");
        check(from_back && back_top >= 4.99f, "and climbed onto from its back, as a block one high");
    }

    std::printf(g_failures == 0 ? "the box rules agree\n" : "THE BOX RULES DISAGREE\n");
    return g_failures == 0 ? 0 : 1;
}
