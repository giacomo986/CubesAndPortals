#include "core/portal/portal_traverse.h"

#include "core/physics/aabb.h"
#include "core/physics/movement.h"

#include <algorithm>
#include <cmath>

namespace cnp {
namespace {

// One quarter turn of the in-plane pair about the normal: r' = u, u' = -r.
void apply_rot(Vec3 &r, Vec3 &u, int rot)
{
    for (int i = 0; i < (((rot % 4) + 4) % 4); ++i) {
        const Vec3 nr = u;
        const Vec3 nu = Vec3{-r.x, -r.y, -r.z};
        r = nr;
        u = nu;
    }
}

// The unrotated triad. Left-handed by construction (r x u = -n), which is what
// makes the transform of portal_traverse.h a rotation rather than a mirror.
void base_triad(const Portal &p, Vec3 &n, Vec3 &r, Vec3 &u)
{
    const float s = (float)((p.normal_sign >= 0) ? 1 : -1);

    switch (p.axis) {
    case PortalAxis::XY:
        n = Vec3{0.0f, 0.0f, s};
        r = Vec3{-s, 0.0f, 0.0f};
        u = Vec3{0.0f, 1.0f, 0.0f};
        break;
    case PortalAxis::YZ:
        n = Vec3{s, 0.0f, 0.0f};
        r = Vec3{0.0f, 0.0f, s};
        u = Vec3{0.0f, 1.0f, 0.0f};
        break;
    case PortalAxis::XZ:
        // The fork writes r = (1,0,0) here, fixed, while n flips with the sign.
        // That makes a ceiling portal's triad RIGHT-handed while every other
        // portal's is left-handed, and the consequence is not cosmetic: the
        // determinant of the body map is det(B_d)/det(B_s), so with the fork's
        // table a floor portal reflects the player and a ceiling portal rotates
        // them. Two horizontal portals that differ only in which way they face
        // would treat the same crossing differently.
        //
        // Flipping r with the sign, as the other two axes already do, makes
        // every triad left-handed for every facing, so every map is a rotation.
        // tests/portal_traverse_test.cpp checks the handedness of all six, which
        // is the check that found this.
        n = Vec3{0.0f, s, 0.0f};
        r = Vec3{s, 0.0f, 0.0f};
        u = Vec3{0.0f, 0.0f, 1.0f};
        break;
    }
    apply_rot(r, u, p.rot);
}

void triad(const Portal &p, Vec3 &n, Vec3 &r, Vec3 &u)
{
    base_triad(p, n, r, u);
}

Vec3 apply_map(const Portal &src, const Portal &dst, Vec3 v)
{
    Vec3 n_s, r_s, u_s, n_d, r_d, u_d;
    triad(src, n_s, r_s, u_s);
    triad(dst, n_d, r_d, u_d);

    const float lr = dot(v, r_s);
    const float lu = dot(v, u_s);
    const float ln = dot(v, n_s);

    return r_d * (-lr) + u_d * lu + n_d * (-ln);
}

float sign_of(float v) { return v < 0.0f ? -1.0f : 1.0f; }

float clampf(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }

} // namespace

Vec3 Portal::normal() const
{
    Vec3 n, r, u;
    base_triad(*this, n, r, u);
    return n;
}

Vec3 Portal::right() const
{
    Vec3 n, r, u;
    base_triad(*this, n, r, u);
    return r;
}

Vec3 Portal::up() const
{
    Vec3 n, r, u;
    base_triad(*this, n, r, u);
    return u;
}

Vec3 portal_transform_dir(const Portal &src, const Portal &dst, Vec3 dir)
{
    return apply_map(src, dst, dir);
}

Vec3 portal_transform_pos(const Portal &src, const Portal &dst, Vec3 pos)
{
    Vec3 n_d, r_d, u_d;
    triad(dst, n_d, r_d, u_d);

    Vec3 off = apply_map(src, dst, pos - src.center);

    // The depth clamp. A point that maps exactly onto the destination plane is
    // simultaneously inside and outside the destination portal, and the trigger
    // test then fires again for ever.
    const float ln = dot(off, n_d);
    if (std::fabs(ln) < kDepthClamp) off = off + n_d * (sign_of(ln) * kDepthClamp - ln);

    return dst.center + off;
}

Vec3 Portal::plane_point() const
{
    return center + normal() * portal_tuning().plane_depth();
}


// Signed distance from the CENTRE of the portal's node, positive on the
// approach side. Not from the image plane: the block is what the tuning
// numbers are fractions of, so the block's own middle is the coordinate they
// convert into. plane_point() is where the image is.
float portal_depth(const Portal &p, Vec3 pos)
{
    return dot(pos - p.center, p.normal());
}

bool portal_swallows(const Portal &p, Vec3 feet, float eye_height, Vec3 vel)
{
    Vec3 n, r, u;
    triad(p, n, r, u);
    if (dot(vel, n) > 0.1f) return false;
    // The eye alone, as the trigger: a crossing taken at the feet throws the
    // view, the eye still a body's height from where the opening showed it.
    const float in  = 0.4f * kNodeSize;   // a tenth in from each face of the block
    const Vec3  rel = feet + Vec3{0.0f, eye_height, 0.0f} - p.center;
    return std::fabs(dot(rel, n)) <= in && std::fabs(dot(rel, r)) <= p.half_w &&
           std::fabs(dot(rel, u)) <= p.half_h;
}

bool in_portal_bounds(const Portal &p, Vec3 pos)
{
    Vec3 n, r, u;
    triad(p, n, r, u);

    const Vec3  rel = pos - p.center;
    const float lr  = std::fabs(dot(rel, r));
    const float lu  = std::fabs(dot(rel, u));
    const float ln  = std::fabs(dot(rel, n));

    if (p.kind == PortalKind::Frame) {
        // Loose: the wall around the opening blocks a sideways approach anyway,
        // and the slack helps a player who clips a corner.
        return lr <= p.half_w + 0.3f && lu <= p.half_h + 0.3f && ln <= 1.0f;
    }

    // Tight: nothing else stops a player who merely walks past a free-standing
    // portal, so alignment is what gates the trigger.
    //
    // The vertical tolerance is 2.0 for a horizontal portal rather than 1.0.
    // The eye sits about 1.6 above the feet, so at half penetration the FEET are
    // already 1.1 from the plane; with 1.0 the player fell out of bounds
    // mid-fall, which cleared the camera hint and made the portal visibly
    // vanish under them.
    const float depth_tol = p.horizontal() ? 2.0f : 1.0f;
    return lr <= p.half_w + 0.1f && lu <= p.half_h + 0.6f && ln <= depth_tol;
}

bool past_trigger(const Portal &p, Vec3 eye)
{
    return portal_depth(p, eye) < portal_tuning().trigger_depth();
}

bool portal_step_trigger(const Portal &p, PortalTriggerState &state, Vec3 feet, Vec3 eye,
                         Vec3 vel)
{
    const bool inside = in_portal_bounds(p, feet) || in_portal_bounds(p, eye);

    if (!inside) {
        // Leaving resets everything, which is what lets the same portal be used
        // twice without a cooldown.
        state.in_bounds          = false;
        state.entered_from_front = false;
        state.triggered          = false;
        return false;
    }

    // The same point the trigger below uses, and for the same reason: a floor
    // portal is crossed by the FEET, so "did they come at it from the front"
    // has to be asked about the feet too. Asking the eye while triggering on
    // the feet is an asymmetry 1.62 nodes wide -- somebody standing under a
    // ceiling portal with their head poking up through it counts as having
    // approached from the front, and gets pulled up through a portal they were
    // merely walking under. The two thresholds used to mask each other; moving
    // one of them is what made it visible.
    const float depth = portal_depth(p, p.horizontal() ? feet : eye);

    if (!state.in_bounds) {
        state.in_bounds = true;

        // Front entry in the ordinary sense: still on the front side.
        state.entered_from_front = depth >= portal_tuning().trigger_depth();

        // Border entry. A fast faller is already past the plane on the very
        // first sample where they are in bounds -- the frame in which they were
        // in front of it never happened. Descending, over a horizontal opening,
        // counts as front entry anyway; without this a fall through a floor
        // portal never fires at all.
        if (!state.entered_from_front && p.horizontal() && vel.y < -0.1f) {
            Vec3 n, r, u;
            triad(p, n, r, u);
            const Vec3  rel = feet - p.center;
            const float lr  = std::fabs(dot(rel, r));
            const float lu  = std::fabs(dot(rel, u));
            if (lr <= p.half_w - 0.15f && lu <= p.half_h - 0.15f)
                state.entered_from_front = true;
        }
    }

    if (state.triggered || !state.entered_from_front) return false;

    // The eye for a wall, the FEET for a floor, and the difference is not a
    // preference -- the eye version cannot be made to work.
    //
    // What has to be continuous through a wall portal is what you SEE, so the
    // eye is what crosses. Through a floor portal it is where you STAND: you go
    // through feet first, and the moment the feet are past the plane you are
    // through. Measuring the eye there means falling 1.6 nodes INTO the block
    // before anything fires, and with the trigger a quarter node inside the top
    // face that is 1.87 nodes of sinking -- more than the collision box is tall,
    // so the box override of PORTAL_SYSTEM 2.3 runs out of room and the player
    // hangs a fifth of a node short of a portal they are standing in. Measured:
    // feet arrested at 22.306 needing 22.13.
    if (!past_trigger(p, p.horizontal() ? feet : eye)) return false;

    state.triggered = true;
    return true;
}

// The trigger, tested along the whole step rather than at the end of it.
//
// A position claim arrives 20 times a second and a sprinting player covers
// about 0.4 m between two of them -- wider than the slab the trigger looks at.
// Sampling only where the claim says they ARE steps straight over a portal, and
// the client never notices because it samples every frame with its own copy of
// the same rule.
//
// The client walks it too, from last frame's feet to this frame's (a kart at
// 13 frames a second covers 1.8 m a frame, past the whole front of a frame's
// slab: it drove into the tunnel behind a mouth and nothing happened).
//
// It became visible the moment the server was the only thing that could notice:
// a client that opens no window behind a mouth walks through the opening, ends
// up a node and a half past it, and nothing happens. Measured that way -- feet
// at 40.61 43.09 with a plane at 44.50, eight rounds of the trigger, no
// crossing.
//
// Sub-sampling and not a swept test on purpose: the rule this walks is the same
// portal_step_trigger the client uses, and two different rules for the same
// event is the thing worth avoiding here. The step is smaller than the slab,
// and the count is capped so a claim that jumps a long way costs a bounded
// amount of work rather than one sample per centimetre.
bool trigger_along(const Portal &p, PortalTriggerState &state, Vec3 from, Vec3 to, Vec3 vel,
                   float eye_height)
{
    const Vec3  step = to - from;
    const float d    = length(step);
    int         n    = d > 0.0f ? (int)(d / 0.15f) + 1 : 1;
    if (n > 64) n = 64;

    for (int i = 1; i <= n; ++i) {
        const float t    = (float)i / (float)n;
        const Vec3  feet = from + step * t;
        const Vec3  eye{feet.x, feet.y + eye_height, feet.z};
        if (portal_step_trigger(p, state, feet, eye, vel)) return true;
    }
    return false;
}

void portal_seed_arrival(const Portal &dst, PortalTriggerState &state, Vec3 feet)
{
    state.in_bounds          = in_portal_bounds(dst, feet);
    state.entered_from_front = false;
    state.triggered          = true;
}

WallShift portal_wall_shift(const Portal &p, Vec3 feet, float half_width, float height, float eye,
                            float margin)
{
    WallShift w;
    if (p.horizontal()) return w;
    const Vec3  n   = p.normal();
    const Vec3  r   = p.right();
    const Vec3  rel = feet - p.center;
    // In front of it, up to a stride away, or already in the hole short of
    // the plane (past it the crossing has happened).
    const float depth = dot(rel, n);
    if (depth > 1.2f || depth < -0.6f) return w;
    // Across it: room for a box a tenth wide at least.
    const float side = p.half_w - margin - std::fabs(dot(rel, r));
    if (side < 0.1f) return w;
    // The eye inside its frame, as the crossing measures it.
    const float lo = p.center.y - p.half_h + margin, hi = p.center.y + p.half_h - margin;
    const float eye_y = feet.y + eye;
    if (eye_y < lo || eye_y > hi) return w;
    // A body standing at the hole's foot steps onto what lines it; one in the
    // air over it has its bottom raised into the hole, but never by much: a
    // body lifted off the ground it stands on would fall.
    float bottom = 0.0f;
    if (feet.y > p.center.y - p.half_h + 0.01f) bottom = std::max(0.0f, lo - feet.y);
    if (bottom > 0.6f) return w;
    const float top = std::max(0.0f, feet.y + height - hi);
    if (height - bottom - top < 0.6f) return w;
    w.on     = true;
    w.bottom = bottom;
    w.top    = top;
    w.half   = std::min(half_width, side);
    return w;
}

float portal_floor_shift(const Portal &p, Vec3 feet, Vec3 velocity, float dt, float half_width)
{
    if (!p.horizontal()) return 0.0f;

    // The whole box over the opening, not only the feet. The shift lowers the
    // body into whatever holds it up, and only the opening's own cells are
    // exempt on the server: a player whose middle was over the hole and whose
    // side rested on the frame beside it sank into the frame, was put back,
    // sank again -- stuck, the view shaking, 30 corrections in two seconds
    // walking down a floor frame's edge (reported from play). Now they stand on
    // the frame, and step in to fall through. A millimetre of slack so a box
    // exactly flush with the edge still counts.
    const Vec3  r    = p.right();
    const Vec3  u    = p.up();
    const Vec3  rel  = feet - p.center;
    const float edge = half_width - 1e-3f;
    if (std::fabs(dot(rel, r)) + edge > p.half_w) return 0.0f;
    if (std::fabs(dot(rel, u)) + edge > p.half_h) return 0.0f;

    // Where the feet will be next tick if they keep falling. Only the falling
    // half: a player jumping UP through a floor portal from below is not
    // stuck on anything and needs no override.
    const float look_y = feet.y + std::min(0.0f, velocity.y) * dt;

    // The node's top face, plus a tenth so the box clears it rather than
    // resting exactly on it -- a box whose bottom is exactly on a face is
    // colliding or not depending on the sign of a rounding error.
    const float top   = p.center.y + 0.5f;   // half a node, the box's own units
    const float shift = top - look_y + 0.1f;

    if (shift <= 0.0f) return 0.0f;

    // Past the cap the player is not sinking THROUGH the opening any more, they
    // are already below it, and the override has to stop -- refused rather than
    // clamped.
    //
    // It used to clamp to kMaxFloorShift, and the difference is not a detail.
    // Nothing here bounds how far below the opening the feet may be: only the
    // LATERAL position is tested. So a player who came out of a floor portal
    // underneath it -- which is where the exit is, the map putting them behind
    // the destination plane -- kept a box raised by 1.7 out of 1.8 for as long
    // as they stayed lined up with the opening, and fell through the world.
    // Measured: they ended at y 29.30 with the floor at y 30.
    //
    // Refusing is also the right answer for the case the cap was protecting. A
    // fast faller whose shift is overrun collides, is arrested on the block with
    // the feet near its top, and the next tick asks again with a small shift --
    // which is the re-engagement the predicted position was added for.
    if (shift > kMaxFloorShift) return 0.0f;
    return shift;
}

Vec3 portal_funnel(const Portal &p, Vec3 feet, Vec3 velocity, float gravity, float dt)
{
    if (!p.horizontal() || p.normal().y <= 0.0f || velocity.y >= 0.0f) return Vec3{};

    // How long until the feet reach the top of the node the opening is in:
    // drop + vy t + g t^2 / 2 = 0, the root in the future.
    const float drop = feet.y - (p.center.y + 0.5f);
    if (drop <= 0.0f) return Vec3{};
    const float t = (gravity < 0.0f)
                        ? (-velocity.y - std::sqrt(velocity.y * velocity.y - 2.0f * gravity * drop)) /
                              gravity
                        : drop / -velocity.y;
    if (!(t > 1e-3f)) return Vec3{};

    // Where it lands as it is going, and whether that is near enough to mean it.
    const float land_x = feet.x + velocity.x * t - p.center.x;
    const float land_z = feet.z + velocity.z * t - p.center.z;
    const float reach  = std::max(p.half_w, p.half_h) + 1.0f;
    if (land_x * land_x + land_z * land_z > reach * reach) return Vec3{};

    Vec3 dv{(p.center.x - feet.x) / t - velocity.x, 0.0f, (p.center.z - feet.z) / t - velocity.z};
    const float most = kFunnelAccel * dt;
    const float len  = length(dv);
    if (len > most) dv = dv * (most / len);
    return dv;
}

Traversal portal_traverse(const Portal &src, const Portal &dst, Vec3 feet, Vec3 look, Vec3 vel,
                          const World *world, const NodeRegistry *registry, float eye_height,
                          float half_width)
{
    Traversal out;

    Vec3 n_d, r_d, u_d;
    triad(dst, n_d, r_d, u_d);

    // ------------------------------------------------------------ direction
    out.look = normalize(apply_map(src, dst, look));

    // This engine's angles, not the original engine's: dir_from_angles is
    // (cos(pitch) sin(yaw), sin(pitch), -cos(pitch) cos(yaw)).
    out.pitch = std::asin(clampf(out.look.y, -1.0f, 1.0f));

    if (std::fabs(out.look.y) > 0.9999f) {
        // Gimbal lock: a look that is within a hundredth of vertical has a
        // horizontal projection too short to carry any yaw. Take the yaw off a
        // transformed horizontal RIGHT vector instead, derived from the look the
        // player had before the crossing.
        const float len = std::sqrt(look.x * look.x + look.z * look.z);
        Vec3        right_before;
        if (len > 1e-4f) {
            // right = normalize(cross(forward, world up)), flattened
            right_before = normalize(Vec3{-look.z, 0.0f, look.x});
        } else {
            // Looking exactly along the vertical: any horizontal direction will
            // do, and +X is as good as another.
            right_before = Vec3{1.0f, 0.0f, 0.0f};
        }
        const Vec3 rt = apply_map(src, dst, right_before);
        out.yaw       = std::atan2(rt.z, rt.x);
    } else {
        out.yaw = std::atan2(out.look.x, -out.look.z);
    }

    // ------------------------------------------------------------------ roll
    //
    // How far the horizon has tipped: world-up carried through the portal,
    // against world-up as it is, both measured in the plane the player is now
    // looking along.
    {
        const Vec3 nwu = normalize(apply_map(src, dst, Vec3{0.0f, 1.0f, 0.0f}));
        const Vec3 wu  = Vec3{0.0f, 1.0f, 0.0f};

        const Vec3 cu0 = wu - out.look * dot(wu, out.look);
        const Vec3 sup = nwu - out.look * dot(nwu, out.look);

        const float l0 = length(cu0), l1 = length(sup);
        if (l0 > 1e-4f && l1 > 1e-4f) {
            const Vec3  a = cu0 * (1.0f / l0);
            const Vec3  b = sup * (1.0f / l1);
            const float c = clampf(dot(a, b), -1.0f, 1.0f);
            out.roll      = std::acos(c) * sign_of(dot(cross(a, b), out.look));
            if (std::fabs(c - 1.0f) < 1e-6f) out.roll = 0.0f;
        }
    }

    // -------------------------------------------------------------- velocity
    out.velocity_delta = apply_map(src, dst, vel) - vel;

    // --------------------------------------------------------------- position
    //
    // Computed for the EYE and not the feet, so that what the player SEES is
    // continuous -- and then LEFT ALONE, which is the whole point and was not
    // what this did.
    //
    // What the player is looking at through the opening is drawn from
    // portal_virtual_camera, which is this same map and nothing else. Put the
    // eye anywhere but where the map sends it and the frame of the crossing is
    // a CUT: the world they were watching jumps by the difference. So the only
    // exit position that can be continuous is the map's own, and any correction
    // applied on top of it is the size of the seam.
    //
    // There used to be one. The eye was pushed along the destination normal to
    // exactly 0.5 + kTriggerDepth past the centre, on the reasoning that the
    // trigger fires that far past the source's face and the two should match.
    // They do not, and the arithmetic says so. past_trigger fires while the eye
    // is still 0.25 IN FRONT of the source plane; the map negates the normal
    // component, so it arrives 0.25 BEHIND the destination plane; and the push
    // then moved it to 0.75 in front. Those distances do not cancel, they add.
    // Walked and measured: the eye was landing 0.917 nodes from where the
    // opening had been showing it, straight along the exit normal, with the
    // look direction agreeing to three decimals. A player sees the roll come
    // out right and the world lurch backwards by most of a node.
    //
    // What the push was really protecting against is that the map leaves the
    // eye BEHIND the destination plane -- mid-traversal, which is exactly what
    // it is. The body stands in the destination's own node, which is the hole,
    // and walks out of it on the frames after. kDepthClamp below is what stops
    // it landing exactly ON the plane, which is the case that ping-pongs.
    const Vec3 eye = feet + Vec3{0.0f, eye_height, 0.0f};

    Vec3 off = apply_map(src, dst, eye - src.center);

    // The normal component is NOT the map's. config/portal.cfg names where the
    // player lands inside the destination block, and that is a position rather
    // than an offset, so it is written over whatever the mirror produced. The
    // two in-plane components stay the map's: which way along the opening the
    // player comes out is geometry, not taste.
    //
    // What this costs is stated where the setting is: the mirror is the only
    // exit that makes the view before a crossing and the view after it the same
    // picture. Anything else is a jump the size of the difference, and the
    // dual-session gate measures it rather than leaving it to be noticed.
    const float want = portal_tuning().exit_depth();
    off = off + n_d * (want - dot(off, n_d));

    // A destination smaller than the source -- an 8x8 frame into a 1x2 one --
    // would have the map put the eye beside the hole, inside the wall round it.
    // Kept within the hole instead: a jump in the view, but only across two
    // openings of different sizes, and the same-size crossing is untouched.
    if (src.half_w > dst.half_w) {
        const float lr  = dot(off, r_d);
        const float lim = std::max(0.0f, dst.half_w - half_width);
        off             = off + r_d * (clampf(lr, -lim, lim) - lr);
    }
    if (src.half_h > dst.half_h) {
        // The eye no lower than its height above the bottom of the hole, when
        // the hole stands up.
        const float lu = dot(off, u_d);
        const float lo = std::min(-dst.half_h + eye_height * std::max(0.0f, u_d.y), dst.half_h);
        off            = off + u_d * (clampf(lu, lo, dst.half_h) - lu);
    }

    // Out of a floor (or a ceiling) the body comes out standing in the hole,
    // its feet in what is under it: the client's box is let into the floor
    // there (portal_floor_shift), but only a box wholly over the opening. A
    // wall opening's eye height, mapped onto a floor's 1x1, landed on the
    // hole's edge, half the box over the floor beside it, and the body stuck
    // in the floor (reported from play, 2026-10-05). The whole box inside it.
    if (dst.horizontal()) {
        const float lr = dot(off, r_d), lu = dot(off, u_d);
        const float wr = std::max(0.0f, dst.half_w - half_width), wu = std::max(0.0f, dst.half_h - half_width);
        off = off + r_d * (clampf(lr, -wr, wr) - lr) + u_d * (clampf(lu, -wu, wu) - lu);
    }

    // Landing exactly on the node's middle plane is the case that ping-pongs:
    // both inside and outside at once. Only reachable when the setting asks for
    // it, and nudged rather than refused.
    const float ln = dot(off, n_d);
    if (std::fabs(ln) < kDepthClamp) off = off + n_d * (sign_of(ln) * kDepthClamp - ln);

    const Vec3 new_eye = dst.center + off;
    out.feet           = new_eye - Vec3{0.0f, eye_height, 0.0f};

    // ------------------------------------------------- an arrival with something in it
    //
    // The exit the settings name is where the player belongs. When a block is
    // standing there, the crossing has to put them somewhere else or refuse,
    // and BOTH of those are things a player notices, so the order is written
    // down rather than left to whichever test came first.
    //
    // Out of the doorway before up. `exit` puts the eye a tenth of a node
    // outside the destination block and the box is three tenths wide, so two
    // tenths of it are always inside that block; the only reason that is legal
    // is that the opening is there at their height. Raise them a node to clear
    // an obstacle and those two tenths are against the WALL beside the hole
    // instead, and the step up is refused for a reason that has nothing to do
    // with the obstacle. Measured: one block on the arrival, and the crossing
    // refused outright -- the player walks through the hole, nothing happens,
    // and nothing tells them why. Reported from play as the teleport failing.
    //
    // So the candidates, in the order a player would accept them:
    //
    //   as computed              nothing was in the way
    //   pushed clear             far enough forward that the box is out of the
    //                            destination's own block, keeping the height
    //                            and the lateral position the image showed
    //   either, up a node        a step, or terrain a node higher on the other
    //                            side, up to three -- past that it is a wall
    //                            and the destination should refuse rather than
    //                            swallow whoever tried it
    // Not out of a floor: there the feet are under it on purpose, the eye
    // where the opening showed it, and the box is let through the floor
    // until the body is up out of the hole (the crossing gives it the speed).
    const bool from_floor = n_d.y > 0.5f;
    if (!from_floor && world && registry && inside_solid(*world, *registry, out.feet.x, out.feet.y,
                                          out.feet.z, nullptr, 0, half_width)) {
        // How far forward the box has to go to leave the destination's block:
        // its near face sits at exit_depth - half a box from the node centre,
        // and the block's own face is at half a node. A hair more, so the
        // faces do not land on each other.
        const float clear = std::max(0.0f, 0.5f * kNodeSize + half_width -
                                               portal_tuning().exit_depth()) + 1e-3f;

        const Vec3  start = out.feet;
        bool        found = false;

        for (int step = 0; step <= 3 && !found; ++step) {
            const float y = (step == 0) ? start.y : std::floor(start.y) + (float)step;
            for (int push = 0; push <= 1 && !found; ++push) {
                const Vec3 cand = Vec3{start.x + n_d.x * clear * (float)push, y,
                                       start.z + n_d.z * clear * (float)push};
                if (inside_solid(*world, *registry, cand.x, cand.y, cand.z, nullptr, 0, half_width))
                    continue;
                out.feet = cand;
                found    = true;
            }
        }

        if (!found) {
            out.blocked = true;
        } else {
            // The camera stays where the image put it and catches up, which is
            // what the warp is for. Both components of it: a sideways shove
            // that snapped the view would be as visible as a vertical one.
            out.stepped_up  = true;
            out.warp_offset = Vec3{start.x - out.feet.x, start.y - out.feet.y,
                                   start.z - out.feet.z};
        }
    }

    return out;
}

} // namespace cnp
