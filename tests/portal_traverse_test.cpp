// Going through a portal, decided on numbers.
//
// M0's gate answered "does the image inside the portal match what is really
// there". This answers the other question, the one that decides whether a
// crossing is playable rather than merely pretty: where does the player come
// out, facing where, tilted how much, moving how fast.
//
// It is all arithmetic, so it runs with no GPU, no window and no world. That is
// the reason for porting the fork's logic into core/ at all: over there it is
// Lua inside a running game, and the only way to find out whether a rule still
// holds is to walk into a portal and look.
//
// The cases come from docs/PORTAL_SYSTEM.md in the fork -- vertical to
// vertical, horizontal to vertical, half offsets, the player inside the frame,
// the gimbal-locked look -- plus the two rules that are easiest to write and
// hardest to notice missing: the determinant, and the depth clamp.

#include "core/portal/portal_traverse.h"
#include "core/portal/portal_math.h"

#include "core/portal/portal_tuning.h"
#include "core/registry/core_nodes.h"
#include "core/voxel/chunk.h"

#include <cmath>
#include <cstdio>

using namespace cnp;

namespace {

int g_failures = 0;

void check(bool cond, const char *what)
{
    std::printf(cond ? "  ok    %s\n" : "  FAIL  %s\n", what);
    if (!cond) ++g_failures;
}

bool near(float a, float b, float eps = 1e-3f) { return std::fabs(a - b) <= eps; }

bool near_vec(Vec3 a, Vec3 b, float eps = 1e-3f)
{
    return near(a.x, b.x, eps) && near(a.y, b.y, eps) && near(a.z, b.z, eps);
}

// Four buffers in rotation, because one static buffer makes every argument in a
// printf print the last vector formatted -- which had two different vectors
// below reading as identical and would have hidden a disagreement rather than
// showing it.
const char *vs(Vec3 v)
{
    static char buf[4][96];
    static int  next = 0;
    char       *b    = buf[next];
    next             = (next + 1) % 4;
    std::snprintf(b, 96, "(%.3f %.3f %.3f)", v.x, v.y, v.z);
    return b;
}

Portal make(PortalAxis axis, int ns, Vec3 centre, int rot = 0,
            PortalKind kind = PortalKind::Frame)
{
    Portal p;
    p.axis        = axis;
    p.normal_sign = ns;
    p.center      = centre;
    p.rot         = rot;
    p.kind        = kind;
    return p;
}

// The determinant of the map, read off by transforming the three axes. Cheaper
// than building a matrix, and it is the number the whole "a portal is a
// rotation, not a mirror" argument rests on.
float map_determinant(const Portal &src, const Portal &dst)
{
    const Vec3 x = portal_transform_dir(src, dst, Vec3{1.0f, 0.0f, 0.0f});
    const Vec3 y = portal_transform_dir(src, dst, Vec3{0.0f, 1.0f, 0.0f});
    const Vec3 z = portal_transform_dir(src, dst, Vec3{0.0f, 0.0f, 1.0f});
    return dot(x, cross(y, z));
}

} // namespace

int main()
{
    // The same shape the client runs, not the built-in defaults. A unit test
    // that measures one shape while config/portal.cfg names another is a test
    // that agrees with itself.
    {
        std::string err;
        if (!portal_tuning_load_default(err)) {
            std::printf("  FAIL  %s\n", err.c_str());
            return 1;
        }
    }

    std::printf("going through a portal\n");

    // ------------------------------------------------------------ the basis
    {
        const Portal p = make(PortalAxis::XY, 1, Vec3{0.0f, 20.0f, 0.0f});
        check(near_vec(p.normal(), Vec3{0.0f, 0.0f, 1.0f}), "an XY portal faces along +Z");
        check(near(dot(p.normal(), p.right()), 0.0f) && near(dot(p.normal(), p.up()), 0.0f) &&
                  near(dot(p.right(), p.up()), 0.0f),
              "and its triad is orthonormal");

        // Left-handed by construction: r x u = -n. Everything about the
        // determinant below depends on this being true of BOTH ends -- and of
        // every facing, which is where the fork's table does not hold. There
        // r = (1,0,0) is fixed for a horizontal portal while the normal flips
        // with the sign, so a ceiling portal comes out right-handed, and the
        // body map then rotates for a ceiling and reflects for a floor. Two
        // portals differing only in which way they face would treat the same
        // crossing differently.
        int handed = 0;
        for (const PortalAxis axis : {PortalAxis::XY, PortalAxis::YZ, PortalAxis::XZ})
            for (const int ns : {1, -1})
                for (int rot = 0; rot < 4; ++rot) {
                    const Portal q = make(axis, ns, Vec3{}, rot);
                    if (near_vec(cross(q.right(), q.up()),
                                 Vec3{-q.normal().x, -q.normal().y, -q.normal().z}))
                        ++handed;
                }
        check(handed == 24, "every axis, facing and rotation gives a left-handed triad");

        const Portal turned = make(PortalAxis::XY, 1, Vec3{0.0f, 20.0f, 0.0f}, 1);
        check(near_vec(turned.right(), p.up()), "one quarter turn puts up where right was");
        check(near_vec(turned.up(), Vec3{-p.right().x, -p.right().y, -p.right().z}),
              "and minus right where up was");
        check(near_vec(make(PortalAxis::XY, 1, Vec3{}, 4).right(), p.right()),
              "and four turns is no turn at all");
    }

    // ------------------------------------------------------- a rotation, not a mirror
    {
        // Two vertical portals facing each other across a room.
        const Portal a = make(PortalAxis::XY, 1, Vec3{0.0f, 20.0f, 0.0f});
        const Portal b = make(PortalAxis::YZ, 1, Vec3{40.0f, 20.0f, 10.0f});

        check(near(map_determinant(a, b), 1.0f),
              "between two vertical portals the map is a proper rotation");

        // Which is the whole point: a mirror would reverse writing and swap the
        // player's hands. Checked as a fact about handedness rather than as a
        // number: a right-handed triad must come out right-handed.
        const Vec3 x = portal_transform_dir(a, b, Vec3{1.0f, 0.0f, 0.0f});
        const Vec3 y = portal_transform_dir(a, b, Vec3{0.0f, 1.0f, 0.0f});
        const Vec3 z = portal_transform_dir(a, b, Vec3{0.0f, 0.0f, 1.0f});
        check(near_vec(cross(x, y), z), "so a right-handed triad stays right-handed");

        // Enter the front of A heading -n_s, leave the front of B heading +n_d.
        const Vec3 in  = Vec3{-a.normal().x, -a.normal().y, -a.normal().z};
        const Vec3 out = portal_transform_dir(a, b, in);
        check(near_vec(out, b.normal()),
              "walking into the front of one comes out of the front of the other");

        // Up is carried through, unchanged, for two upright portals.
        check(near_vec(portal_transform_dir(a, b, Vec3{0.0f, 1.0f, 0.0f}),
                       Vec3{0.0f, 1.0f, 0.0f}),
              "and up stays up");
    }

    // ------------------------------------------- one map for the picture and the body
    {
        // The fork reflected the body whenever an end was horizontal and drew
        // the opening with the rotation, and the two disagreed: a player diving
        // into a floor portal with the picture upright came out upside down and
        // rolled half a turn back (reported from play, 2026-09-19). So every
        // pair of the 24 orientations is checked against the renderer's own
        // matrix, and each map must be a rotation.
        const Portal all_axes[3] = {make(PortalAxis::XY, 1, Vec3{}), make(PortalAxis::YZ, 1, Vec3{}),
                                    make(PortalAxis::XZ, 1, Vec3{})};
        auto surface = [](const Portal &p) {
            PortalSurface s;
            s.center = p.center;
            s.normal = p.normal();
            s.up     = p.up();
            return s;
        };
        int same = 0, rotations = 0, pairs = 0;
        for (int a = 0; a < 24; ++a) {
            for (int b = 0; b < 24; ++b) {
                Portal src = all_axes[a / 8], dst = all_axes[b / 8];
                src.normal_sign = (a / 4) % 2 ? -1 : 1;
                dst.normal_sign = (b / 4) % 2 ? -1 : 1;
                src.rot = a % 4;
                dst.rot = b % 4;
                src.center = Vec3{1.0f, 20.0f, 3.0f};
                dst.center = Vec3{40.0f, 25.0f, -7.0f};

                const Mat4 render = portal_transform(surface(src), surface(dst));
                bool agree = true;
                for (const Vec3 v : {Vec3{1, 0, 0}, Vec3{0, 1, 0}, Vec3{0, 0, 1}})
                    agree = agree && near_vec(portal_transform_dir(src, dst, v),
                                              mat4_transform_dir(render, v));
                same += agree ? 1 : 0;
                rotations += near(map_determinant(src, dst), 1.0f) ? 1 : 0;
                ++pairs;
            }
        }
        std::printf("        %d of %d pairs map the body as the picture, %d are rotations\n",
                    same, pairs, rotations);
        check(same == pairs, "the body goes where the opening's picture goes, for every pair");
        check(rotations == pairs, "and every map is a rotation, a horizontal end included");

        const Portal floor_p = make(PortalAxis::XZ, 1, Vec3{0.0f, 20.0f, 0.0f});
        const Portal wall    = make(PortalAxis::XY, 1, Vec3{40.0f, 20.0f, 10.0f});

        // And it is applied consistently: position and look must agree, which
        // is what deriving the rule per quantity got wrong in the fork.
        const Vec3 look = normalize(Vec3{0.3f, -0.9f, 0.2f});
        const Traversal t =
            portal_traverse(floor_p, wall, Vec3{0.2f, 19.0f, -0.1f}, look, Vec3{}, nullptr,
                            nullptr);
        check(near_vec(t.look, portal_transform_dir(floor_p, wall, look), 1e-3f),
              "the look a crossing produces is the look the direction map produces");
    }

    // --------------------------------------------------------------- positions
    {
        const Portal a = make(PortalAxis::XY, 1, Vec3{0.0f, 20.0f, 0.0f});
        const Portal b = make(PortalAxis::YZ, 1, Vec3{40.0f, 20.0f, 10.0f});

        // Positions are relative to the centres; directions are not. Moving both
        // portals must move the mapped point with them and leave a mapped
        // direction alone.
        const Vec3 p    = Vec3{0.4f, 21.0f, 2.0f};
        const Vec3 got  = portal_transform_pos(a, b, p);
        const Vec3 dir  = normalize(Vec3{0.0f, 0.0f, -1.0f});
        const Vec3 gotd = portal_transform_dir(a, b, dir);

        Portal a2 = a, b2 = b;
        a2.center = a.center + Vec3{100.0f, 0.0f, 0.0f};
        b2.center = b.center + Vec3{0.0f, 0.0f, 250.0f};
        const Vec3 got2  = portal_transform_pos(a2, b2, p + Vec3{100.0f, 0.0f, 0.0f});
        const Vec3 gotd2 = portal_transform_dir(a2, b2, dir);

        check(near_vec(got2, got + Vec3{0.0f, 0.0f, 250.0f}),
              "a position is measured from the centres and lands relative to them");
        check(near_vec(gotd2, gotd), "a direction is not");

        // The depth clamp: a point exactly on the source plane must not land
        // exactly on the destination plane, or it is inside and outside the
        // destination at once and the crossing fires for ever.
        const Vec3  on_plane = a.center + a.right() * 0.2f + a.up() * 0.1f;
        const Vec3  landed   = portal_transform_pos(a, b, on_plane);
        const float depth    = portal_depth(b, landed);
        check(std::fabs(depth) >= kDepthClamp - 1e-5f,
              "a point on the source plane lands clear of the destination plane");
        check(depth > 0.0f, "and on the front side of it, not behind");
        std::printf("        landed at depth %.4f, the clamp is %.4f\n", depth, kDepthClamp);
    }

    // --------------------------------------------------------- exit placement
    {
        const Portal a = make(PortalAxis::XY, 1, Vec3{0.0f, 20.0f, 0.0f});
        const Portal b = make(PortalAxis::YZ, 1, Vec3{40.0f, 20.0f, 10.0f});

        // A player standing at the trigger depth on the way in.
        const Vec3 feet = Vec3{0.0f, 20.0f - kPlayerEyeHeight + 0.1f,
                               portal_tuning().trigger_depth() - 0.01f};
        const Traversal t =
            portal_traverse(a, b, feet, Vec3{0.0f, 0.0f, -1.0f}, Vec3{}, nullptr, nullptr);

        // The exit depth is the SETTING, and nothing else decides it.
        //
        // It used to be the map's own: the depth negated and nothing happened
        // to it, which is the whole statement of a continuous crossing in one
        // number -- what the player is watching through the opening is drawn by
        // that same map, so any other exit makes the frame of the crossing a
        // cut the size of the difference. config/portal.cfg names a position
        // instead, and this is where that decision is paid for.
        //
        // So the seam is not asserted away, it is PRINTED. `exit` = 1 - trigger
        // read from the other end is the mirror; every hundredth away from it
        // is a hundredth of a node of jump, and the number below is what a
        // player walks into.
        //
        // The older mistake is still worth its warning: the eye was once pushed
        // to 0.5 + kTriggerDepth past the destination's face, on the reasoning
        // that the trigger fires that far past the source's. Those distances
        // added instead of cancelling -- 0.917 nodes of lurch, measured by
        // walking through one.
        const Vec3  eye_in    = feet + Vec3{0.0f, kPlayerEyeHeight, 0.0f};
        const float in_depth  = portal_depth(a, eye_in);
        const Vec3  eye_out   = t.feet + Vec3{0.0f, kPlayerEyeHeight, 0.0f};
        const float out_depth = portal_depth(b, eye_out);
        check(near(out_depth, portal_tuning().exit_depth(), 1e-3f),
              "the eye comes out at the exit depth the config names");

        // The reference is the RENDERER's map. It glues the face the player
        // leaves the source through to the face they emerge from at the
        // destination, and those two offsets cancel, so a point at depth d comes
        // out at -d, plus config/portal.cfg's viewpoint offset, whatever `plane`
        // is -- and that is where the image the player was watching says they
        // are.
        const float seamless = portal_shown_depth(in_depth);
        std::printf("        eye at %s, depth %.4f in, %.4f out; the image puts it at %.4f, "
                    "so the crossing jumps %.4f nodes\n", vs(eye_out), in_depth, out_depth,
                    seamless, std::fabs(out_depth - seamless));

        check(!t.blocked && !t.stepped_up, "with nothing in the way there is no step up");
    }

    // ---------------------------------------------- which side the exit lands on
    //
    // The negation above says the exit mirrors the entry. Which SIDE that puts
    // the player on is then decided entirely by where the trigger fires, and
    // the two together are what the player sees.
    //
    // A crossing fires once the eye is past the source plane, so the mirrored
    // exit is in FRONT of the destination -- out of the doorway, with it
    // behind. When the trigger sat a quarter node out on the approach side the
    // sign came out the other way and the player spent three frames standing
    // behind the destination opening, whose far face is a window onto the world
    // they had just left. Measured from play and filmed in the dual-session
    // gate: the opening covering 94% of the screen, painting the departed
    // world.
    {
        const Portal a = make(PortalAxis::XY, 1, Vec3{0.0f, 20.0f, 0.0f});
        const Portal b = make(PortalAxis::YZ, 1, Vec3{40.0f, 20.0f, 10.0f});

        // Walked in, not posed. The pose the exit is computed from is whichever
        // one the trigger fires on, so a test that picks its own pose measures
        // the map and leaves the composition -- trigger, then map -- unmeasured.
        // The composition is where the defect was.
        //
        // Four step sizes, covering a frame of movement from a crawl to a
        // sprint at the physics step cap.
        for (const float step : {0.005f, 0.04f, 0.08f, 0.30f}) {
            PortalTriggerState st;
            Vec3               eye_in{};
            bool               fired = false;

            for (float d = 1.0f; d > -1.0f && !fired; d -= step) {
                eye_in = a.center + a.normal() * d;
                const Vec3 feet = eye_in - Vec3{0.0f, kPlayerEyeHeight, 0.0f};
                fired = portal_step_trigger(a, st, feet, eye_in, Vec3{});
            }
            check(fired, "walking into the plane fires the crossing");

            const Vec3      feet = eye_in - Vec3{0.0f, kPlayerEyeHeight, 0.0f};
            const Traversal t =
                portal_traverse(a, b, feet, Vec3{0.0f, 0.0f, -1.0f}, Vec3{}, nullptr, nullptr);
            const float out_depth =
                portal_depth(b, t.feet + Vec3{0.0f, kPlayerEyeHeight, 0.0f});
            std::printf("        step %.3f: fired at %+.3f, comes out at %+.3f\n", (double)step,
                        (double)portal_depth(a, eye_in), (double)out_depth);
            check(out_depth > 0.0f,
                  "and comes out IN FRONT of the destination plane, not behind its opening");
        }
    }

    // ------------------------------------------------------------ yaw and pitch
    {
        const Portal a = make(PortalAxis::XY, 1, Vec3{0.0f, 20.0f, 0.0f});
        const Portal b = make(PortalAxis::YZ, 1, Vec3{40.0f, 20.0f, 10.0f});

        // The angles must reproduce the look direction under this engine's own
        // convention. Transcribing the original engine's would put a ninety-degree error here
        // that nothing else in the file would notice.
        for (const Vec3 look : {normalize(Vec3{0.0f, 0.0f, -1.0f}),
                                normalize(Vec3{1.0f, 0.2f, -0.3f}),
                                normalize(Vec3{-0.7f, -0.5f, 0.4f})}) {
            const Traversal t =
                portal_traverse(a, b, Vec3{0.0f, 19.0f, 0.2f}, look, Vec3{}, nullptr, nullptr);
            const float cp   = std::cos(t.pitch);
            const Vec3  from = Vec3{cp * std::sin(t.yaw), std::sin(t.pitch), -cp * std::cos(t.yaw)};
            check(near_vec(from, t.look, 2e-3f),
                  "the yaw and pitch reported rebuild the look reported");
        }

        // Gimbal lock. A look transformed to straight down has no horizontal
        // projection at all, so the yaw has to come off a transformed right
        // vector instead of off a zero-length shadow.
        //
        // The first version of this check asked whether the yaw was finite, and
        // it was worse than no check: atan2(0, 0) is zero, not NaN, so deleting
        // the fallback left it green. What the fallback actually buys is that
        // the player keeps FACING the way they were facing -- their shoulders
        // stay put while their head points down -- so that is what gets checked:
        // the right vector the reported yaw implies must be the transform of the
        // right vector they had before.
        const Portal floor_p = make(PortalAxis::XZ, -1, Vec3{0.0f, 30.0f, 0.0f});
        const Vec3   before  = normalize(Vec3{0.0f, 0.0f, -1.0f});
        const Traversal t = portal_traverse(a, floor_p, Vec3{0.0f, 19.0f, 0.2f}, before, Vec3{},
                                            nullptr, nullptr);
        check(std::fabs(t.look.y) > 0.99f, "looking into a wall portal and out of a floor one");

        const Vec3 right_before = normalize(Vec3{-before.z, 0.0f, before.x});
        Vec3       want         = portal_transform_dir(a, floor_p, right_before);
        want.y                  = 0.0f;
        want                    = normalize(want);
        const Vec3 got = Vec3{std::cos(t.yaw), 0.0f, std::sin(t.yaw)};
        check(near_vec(got, want, 2e-3f),
              "and leaves the player facing where their shoulders were carried to");
        std::printf("        straight down: look %s, yaw %.3f (facing %s, wanted %s)\n",
                    vs(t.look), t.yaw, vs(got), vs(want));
    }

    // ------------------------------------------------------------------- roll
    {
        const Portal a = make(PortalAxis::XY, 1, Vec3{0.0f, 20.0f, 0.0f});

        // Same basis at both ends: nothing tips, so the horizon must not either.
        const Portal same = make(PortalAxis::XY, -1, Vec3{0.0f, 20.0f, 30.0f});
        const Traversal flat =
            portal_traverse(a, same, Vec3{0.0f, 19.0f, 0.2f}, normalize(Vec3{0.1f, 0.0f, -1.0f}),
                            Vec3{}, nullptr, nullptr);
        check(near(flat.roll, 0.0f, 1e-3f), "between two portals with the same basis, no roll");

        // A destination turned a quarter turn about its normal tips the horizon
        // by exactly that much.
        const Portal turned = make(PortalAxis::XY, -1, Vec3{0.0f, 20.0f, 30.0f}, 1);
        const Traversal tilted =
            portal_traverse(a, turned, Vec3{0.0f, 19.0f, 0.2f},
                            normalize(Vec3{0.0f, 0.0f, -1.0f}), Vec3{}, nullptr, nullptr);
        check(near(std::fabs(tilted.roll), 3.14159265f / 2.0f, 1e-2f),
              "a destination turned a quarter turn tips the horizon a quarter turn");
        std::printf("        roll %.4f rad (%.1f degrees)\n", tilted.roll,
                    tilted.roll * 180.0f / 3.14159265f);
    }

    // --------------------------------------------------------------- velocity
    {
        const Portal a = make(PortalAxis::XY, 1, Vec3{0.0f, 20.0f, 0.0f});
        const Portal b = make(PortalAxis::YZ, 1, Vec3{40.0f, 20.0f, 10.0f});

        const Vec3      vel = Vec3{0.0f, -3.0f, -8.0f};
        const Traversal t =
            portal_traverse(a, b, Vec3{0.0f, 19.0f, 0.2f}, normalize(vel), vel, nullptr, nullptr);

        // A delta, not an absolute: adding it to what the player had must give
        // what the transform says they should have.
        const Vec3 expect = portal_transform_dir(a, b, vel);
        check(near_vec(vel + t.velocity_delta, expect, 1e-3f),
              "the velocity is a delta that adds up to the transformed velocity");
        check(near(length(vel + t.velocity_delta), length(vel), 1e-2f),
              "and a crossing does not change how fast the player is going");
    }

    // ---------------------------------------------------------------- step up
    {
        // A destination whose floor is one node higher than the exit computes.
        World        world;
        NodeRegistry reg;
        const CoreNodes ids = register_core_nodes(reg);

        const Portal a = make(PortalAxis::XY, 1, Vec3{0.0f, 20.0f, 0.0f});
        const Portal b = make(PortalAxis::YZ, 1, Vec3{40.0f, 20.0f, 10.0f});

        const Traversal clear =
            portal_traverse(a, b, Vec3{0.0f, 19.0f, 0.2f}, Vec3{0.0f, 0.0f, -1.0f}, Vec3{},
                            &world, &reg, kPlayerEyeHeight);
        check(!clear.stepped_up && !clear.blocked, "an empty destination needs no step up");

        // Fill the node the feet would land in.
        for (int dx = -1; dx <= 1; ++dx)
            for (int dz = -1; dz <= 1; ++dz)
                world.set_block((int)std::floor(clear.feet.x) + dx, (int)std::floor(clear.feet.y),
                                (int)std::floor(clear.feet.z) + dz, ids.stone);

        const Traversal stepped =
            portal_traverse(a, b, Vec3{0.0f, 19.0f, 0.2f}, Vec3{0.0f, 0.0f, -1.0f}, Vec3{},
                            &world, &reg, kPlayerEyeHeight);
        check(stepped.stepped_up && !stepped.blocked, "a blocked landing steps up onto it");
        check(stepped.feet.y > clear.feet.y, "and comes out higher than it would have");
        check(near(stepped.warp_offset.y, -(stepped.feet.y - clear.feet.y), 1e-3f),
              "and records how much, so the camera can ease up instead of jumping");
        std::printf("        stepped from %.3f to %.3f, warp %s\n", clear.feet.y,
                    stepped.feet.y, vs(stepped.warp_offset));

        // Walled in: three nodes of solid above the landing. Refusing is the
        // right answer -- a destination that is buried should not swallow
        // whoever tried to use it.
        for (int step = 0; step <= 4; ++step)
            for (int dx = -1; dx <= 1; ++dx)
                for (int dz = -1; dz <= 1; ++dz)
                    world.set_block((int)std::floor(clear.feet.x) + dx,
                                    (int)std::floor(clear.feet.y) + step,
                                    (int)std::floor(clear.feet.z) + dz, ids.stone);

        const Traversal walled =
            portal_traverse(a, b, Vec3{0.0f, 19.0f, 0.2f}, Vec3{0.0f, 0.0f, -1.0f}, Vec3{},
                            &world, &reg, kPlayerEyeHeight);
        check(walled.blocked, "a destination that is walled in refuses the crossing");
    }

    // ------------------------------------------------ a big opening into a small one
    {
        // An 8x8 frame (the infernoportal mod's largest) into a 1x2 one: walked
        // through at its bottom corner, the player comes out inside the small
        // hole, feet on its floor, not four nodes beside it in the wall.
        Portal big = make(PortalAxis::XY, 1, Vec3{0.0f, 24.0f, 0.0f});
        big.half_w = big.half_h = 4.0f;
        const Portal small = make(PortalAxis::XY, -1, Vec3{50.0f, 11.0f, 0.0f});
        const Traversal t  = portal_traverse(big, small, Vec3{3.5f, 20.0f, 0.2f},
                                             Vec3{0.0f, 0.0f, -1.0f}, Vec3{}, nullptr, nullptr);
        check(std::fabs(t.feet.x - small.center.x) <= small.half_w &&
                  near(t.feet.y, small.center.y - small.half_h),
              "a big opening into a small one lands inside the small one");
    }

    // ------------------------------------------------------------- the trigger
    {
        const Portal a = make(PortalAxis::XY, 1, Vec3{0.0f, 20.0f, 0.0f});

        PortalTriggerState st;

        // Approaching from the front, still outside.
        Vec3 eye  = a.center + a.normal() * 2.0f;
        Vec3 feet = eye - Vec3{0.0f, kPlayerEyeHeight, 0.0f};
        check(!portal_step_trigger(a, st, feet, eye, Vec3{}), "far away, nothing happens");

        // In bounds, in front.
        eye  = a.center + a.normal() * 0.6f;
        feet = eye - Vec3{0.0f, kPlayerEyeHeight, 0.0f};
        check(!portal_step_trigger(a, st, feet, eye, Vec3{}), "in front of it, nothing yet");
        check(st.in_bounds && st.entered_from_front, "but it is noted as entered from the front");

        // A hundredth of a node short of the trigger depth: still nothing.
        // Written against the setting rather than a number, because the setting
        // is what moves -- and it has moved twice, each time for a reason a
        // player reported.
        const float td = portal_tuning().trigger_depth();
        eye  = a.center + a.normal() * (td + 0.01f);
        feet = eye - Vec3{0.0f, kPlayerEyeHeight, 0.0f};
        check(!portal_step_trigger(a, st, feet, eye, Vec3{}),
              "a hundredth of a node short of the trigger depth, it still does not fire");

        // Past it, by however far one frame of walking carried the eye.
        eye  = a.center + a.normal() * (td - 0.04f);
        feet = eye - Vec3{0.0f, kPlayerEyeHeight, 0.0f};
        check(portal_step_trigger(a, st, feet, eye, Vec3{}), "past the trigger depth, it fires");

        // And exactly once. A second sample deeper in must not fire again, or a
        // player walking through gets teleported every frame they are inside.
        eye  = a.center + a.normal() * (td - 0.1f);
        feet = eye - Vec3{0.0f, kPlayerEyeHeight, 0.0f};
        check(!portal_step_trigger(a, st, feet, eye, Vec3{}), "and only once");

        // Leaving and coming back arms it again.
        eye  = a.center + a.normal() * 5.0f;
        feet = eye - Vec3{0.0f, kPlayerEyeHeight, 0.0f};
        portal_step_trigger(a, st, feet, eye, Vec3{});
        check(!st.in_bounds && !st.triggered, "walking away rearms it");
    }

    // Entering from the back must never fire: that is what makes a portal
    // one-directional, and it is the difference between arriving somewhere and
    // being bounced straight back.
    {
        const Portal a = make(PortalAxis::XY, 1, Vec3{0.0f, 20.0f, 0.0f});
        PortalTriggerState st;

        Vec3 eye  = a.center - a.normal() * 0.6f;
        Vec3 feet = eye - Vec3{0.0f, kPlayerEyeHeight, 0.0f};
        portal_step_trigger(a, st, feet, eye, Vec3{});
        check(!st.entered_from_front, "coming at it from behind is not front entry");

        eye  = a.center - a.normal() * 0.1f;
        feet = eye - Vec3{0.0f, kPlayerEyeHeight, 0.0f};
        check(!portal_step_trigger(a, st, feet, eye, Vec3{}),
              "and walking on through it does not fire");
    }

    // The arrival portal is seeded so that landing in front of it does not send
    // the player straight back where they came from.
    {
        const Portal b = make(PortalAxis::YZ, 1, Vec3{40.0f, 20.0f, 10.0f});
        PortalTriggerState st;

        const Vec3 feet = b.center + b.normal() * 0.75f - Vec3{0.0f, kPlayerEyeHeight, 0.0f};
        portal_seed_arrival(b, st, feet);
        check(st.triggered && !st.entered_from_front, "the arrival is seeded as already used");

        const Vec3 eye = feet + Vec3{0.0f, kPlayerEyeHeight, 0.0f};
        check(!portal_step_trigger(b, st, feet, eye, Vec3{}),
              "so standing in the destination does not bounce the player back");
    }

    // A fast faller through a floor portal. Their first in-bounds sample is
    // already past the plane -- the frame in which they were above it never
    // happened -- so without the border-entry clause this crossing never fires
    // at all, which is the bug that makes floor portals feel broken.
    {
        const Portal floor_p = make(PortalAxis::XZ, 1, Vec3{0.0f, 20.0f, 0.0f}, 0,
                                    PortalKind::Block);
        PortalTriggerState st;

        const Vec3 eye  = floor_p.center + floor_p.normal() * 0.05f;
        const Vec3 feet = eye - Vec3{0.0f, kPlayerEyeHeight, 0.0f};
        const bool fired =
            portal_step_trigger(floor_p, st, feet, eye, Vec3{0.0f, -14.0f, 0.0f});
        check(st.entered_from_front,
              "a fast faller already past the plane still counts as entering from the front");
        check(fired, "and falls through");

        // But only while descending: standing still under a ceiling portal must
        // not count, or a player walking about below one gets pulled up.
        PortalTriggerState still;
        portal_step_trigger(floor_p, still, feet, eye, Vec3{});
        check(!still.entered_from_front, "standing still under it does not");
    }

    // ------------------------------------------------------------------ bounds
    {
        // A free-standing block portal must not fire on somebody walking past
        // it; a frame portal, which is set in a wall, may be generous.
        const Portal block = make(PortalAxis::XY, 1, Vec3{0.0f, 20.0f, 0.0f}, 0,
                                  PortalKind::Block);
        const Portal frame = make(PortalAxis::XY, 1, Vec3{0.0f, 20.0f, 0.0f}, 0,
                                  PortalKind::Frame);

        const Vec3 beside = Vec3{0.0f, 20.0f, 0.0f} + block.right() * (block.half_w + 0.25f);
        check(!in_portal_bounds(block, beside), "walking past a block portal stays out of bounds");
        check(in_portal_bounds(frame, beside), "the same spot is in bounds for a frame portal");

        // A horizontal portal gets more vertical tolerance, because the eye is
        // 1.6 above the feet and one node of it left a falling player out of
        // bounds halfway through -- which cleared the camera hint and made the
        // portal visibly vanish under them.
        const Portal floor_p = make(PortalAxis::XZ, 1, Vec3{0.0f, 20.0f, 0.0f}, 0,
                                    PortalKind::Block);
        const Portal wall_p  = make(PortalAxis::XY, 1, Vec3{0.0f, 20.0f, 0.0f}, 0,
                                    PortalKind::Block);
        check(in_portal_bounds(floor_p, Vec3{0.0f, 20.0f, 0.0f} + floor_p.normal() * 1.5f),
              "a falling player stays in a horizontal portal's bounds at 1.5 nodes");
        check(!in_portal_bounds(wall_p, Vec3{0.0f, 20.0f, 0.0f} + wall_p.normal() * 1.5f),
              "where a vertical one would already have let go");
    }

    // ------------------------------------ falling through a solid floor portal
    //
    // PORTAL_SYSTEM 2.3. The block holding the frame up is exactly where the
    // player has to fall through, so the bottom of the collision box is raised
    // until it clears the node. The three cases below are the three the fork
    // documents, and the third is the one a velocity gate gets wrong.
    {
        std::printf("floor passthrough\n");

        const Portal floor_p = make(PortalAxis::XZ, 1, Vec3{0.5f, 20.5f, 0.5f}, 0,
                                    PortalKind::Block);
        const float dt = 1.0f / 60.0f;

        // Standing on top of it: the node's top is at 21.0, the feet are there,
        // so the box has to come up by a tenth to stop resting on it.
        const float standing = portal_floor_shift(floor_p, Vec3{0.5f, 21.0f, 0.5f},
                                                  Vec3{0.0f, 0.0f, 0.0f}, dt);
        check(standing > 0.0f, "a player standing on a floor portal gets a shift");
        check(standing < 0.2f, "and it is only as much as the block they are resting on");

        // Falling fast. The predicted feet are below the node's top, so the
        // shift is bigger and arrives BEFORE they are on the block -- which is
        // the point: a shift that waited for contact would arrive a frame late,
        // every frame, for as long as they keep falling.
        const float falling = portal_floor_shift(floor_p, Vec3{0.5f, 21.4f, 0.5f},
                                                 Vec3{0.0f, -30.0f, 0.0f}, dt);
        check(falling > standing, "a fast faller gets a bigger shift, before touching down");
        check(falling <= kMaxFloorShift, "and never more than the box can give");

        // Arrested on the block with no velocity at all. This is the case the
        // fork calls out: a velocity gate reports "not falling" and clears the
        // shift, and the player stands on the portal forever. The predicted
        // position gate cannot, because it does not ask about velocity when
        // the feet are already low enough.
        const float stuck = portal_floor_shift(floor_p, Vec3{0.5f, 20.9f, 0.5f},
                                               Vec3{0.0f, 0.0f, 0.0f}, dt);
        check(stuck > 0.0f, "and a player arrested on the block still gets one");

        // Off to the side, and rising through from below: neither is stuck.
        check(portal_floor_shift(floor_p, Vec3{3.5f, 21.0f, 0.5f}, Vec3{}, dt) == 0.0f,
              "a player walking past it keeps their collision box");
        check(portal_floor_shift(floor_p, Vec3{0.5f, 22.0f, 0.5f}, Vec3{0.0f, 8.0f, 0.0f}, dt) ==
                  0.0f,
              "and one jumping up through it from below needs no override");

        // Feet over the opening, side over what is beside it: sinking would put
        // the box into a block the server does not exempt, and it puts them back,
        // every claim (reported from play, walking down a floor frame's edge).
        check(portal_floor_shift(floor_p, Vec3{0.85f, 21.0f, 0.5f}, Vec3{}, dt) == 0.0f,
              "a player whose box is partly over the edge keeps their collision box");

        // A wall portal never shifts anything: there is nothing underfoot.
        const Portal wall_p = make(PortalAxis::XY, 1, Vec3{0.5f, 20.5f, 0.5f}, 0,
                                   PortalKind::Block);
        check(portal_floor_shift(wall_p, Vec3{0.5f, 21.0f, 0.5f}, Vec3{}, dt) == 0.0f,
              "a vertical portal shifts nothing");
    }

    std::printf(g_failures == 0 ? "portal traversal passed\n" : "PORTAL TRAVERSAL FAILED\n");
    return g_failures == 0 ? 0 : 1;
}
