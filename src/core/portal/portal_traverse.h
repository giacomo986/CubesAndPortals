#pragma once

#include "core/math/vec.h"
#include "core/physics/aabb.h"
#include "core/portal/portal_tuning.h"
#include "core/registry/node_registry.h"
#include "core/voxel/chunk.h"

#include <cstdint>

// Going through a portal, as opposed to looking through one.
//
// portal_math.h is the renderer's half: where to put a virtual camera and how
// to clip what it sees. This is the other half -- where the player comes out,
// facing where, tilted how much, moving how fast. Both apply the same map, so
// the view on the frame after a crossing is the picture the opening showed.
//
// Everything here is ported from the yaportal fork's init.lua, where each rule
// is a paid-for bug rather than a preference, and the debt is worth naming:
// this is the second implementation of a transform that already works, so the
// only excuse for it existing is that this one is testable. Every rule below
// has a check in tests/portal_traverse_test.cpp that fails when it is removed.
//
// One thing is deliberately NOT a transcription. The fork reads yaw through
// The original engine's getRadYawDep(), which adds ninety degrees and flips a sign; carrying
// that convention here would import a bug from an engine this one does not
// have. The angles below are this engine's: yaw = atan2(x, -z), pitch =
// asin(y), matching dir_from_angles in the client.

namespace cnp {

// Where a crossing fires, where the player lands, and where the image sits are
// all in config/portal.cfg now -- see portal_tuning.h. They were three
// constants in three files describing one shape, and they have to agree.
//
// The history is worth keeping. The trigger was once a quarter node IN FRONT
// of the plane, on the approach side. The map negates the normal component, so
// somebody still in front of the source came out short of the destination --
// BEHIND the opening they were supposed to have come out of, looking at its far
// face, which is a window onto the world they had just left. Three frames of it
// with the opening covering 94% of the screen, filmed in
// docs/archive/M5_FINDINGS.md 8.2.

// A traversal computed exactly on the destination plane leaves the player at
// depth zero, which is both inside and outside the destination at once. The
// trigger then fires again and the player ping-pongs between the two portals.
inline constexpr float kDepthClamp = 0.02f;

// Which plane the opening lies in, named by the plane rather than by the normal
// so that "horizontal" is a property of the value rather than a special case in
// the reader's head.
enum class PortalAxis : uint8_t {
    XY = 0,   // opening in the XY plane, normal along +/-Z
    YZ = 1,   // opening in the YZ plane, normal along +/-X
    XZ = 2,   // horizontal: opening in the XZ plane, normal along +/-Y
};

// A frame portal is built into a wall that physically blocks a sideways
// approach; a block portal stands free. They get different trigger bounds, and
// that difference is not tuning -- loose bounds on a free-standing portal fire
// while a player merely walks past it.
enum class PortalKind : uint8_t { Frame = 0, Block = 1 };

// A portal as the world holds it.
//
// PortalSurface in portal_math.h is the same opening as the renderer sees it,
// carrying only what a camera needs. This carries what a body needs: which side
// is the front, how the in-plane basis is turned, and how big the hole is.
struct Portal {
    Vec3 center;                          // the centre of the OPENING, in world units
    PortalAxis axis        = PortalAxis::XY;
    int        normal_sign = 1;           // +1 or -1: which side is the front
    int        rot         = 0;           // 0..3 quarter turns of the in-plane basis

    float half_w = 0.5f;
    float half_h = 1.0f;

    PortalKind kind = PortalKind::Frame;

    // What it shows while it leads nowhere: 0 green, a fixed frame; 1 blue and
    // 2 orange, a portal gun's two ends.
    uint8_t colour = 0;

    Vec3 normal() const;
    Vec3 right() const;
    Vec3 up() const;

    // Where the image is and where the two worlds are glued: inside the block,
    // at the depth config/portal.cfg names. See portal_plane_in.
    Vec3 plane_point() const;

    bool horizontal() const { return axis == PortalAxis::XZ; }
};

// --------------------------------------------------------------- the transform
//
// With B_s and B_d the two frames as column matrices [r u n], the map is
//
//     T = B_d . diag(-1, +1, -1) . B_s^T
//
// The right and normal components flip; up is carried through. Both frames are
// left-handed (r x u = -n), so det T = (-1)(+1)/(-1) = +1: a proper rotation.
// That is a requirement, not a coincidence. A portal that mirrored the world
// would reverse writing and swap the player's hands.
//
// **No horizontal exception.** The fork negated the source's right vector
// whenever either end was horizontal, which made the body map a reflection while
// the virtual camera kept the rotation -- on the reasoning that the difference
// "lands as a different exit yaw that the roll animation absorbs". It does not:
// a roll cannot absorb a yaw. Diving into a floor portal along its right axis
// came out looking 20 to 90 degrees away from what the opening showed, and with
// the image upright the body landed upside down and rolled half a turn back --
// reported from play (2026-09-19). One map for the picture and the body; a
// horizontal end tips the horizon like any other turned end, and the roll
// settles it.
Vec3 portal_transform_dir(const Portal &src, const Portal &dst, Vec3 dir);

// The same map, applied to a point: relative to the source's centre, and put
// back relative to the destination's. The depth clamp above is applied here and
// not in the direction form.
Vec3 portal_transform_pos(const Portal &src, const Portal &dst, Vec3 pos);

// ------------------------------------------------------------------ predicates

// Signed distance from the plane of the opening, along the front normal.
float portal_depth(const Portal &p, Vec3 pos);

// Inside the box around the opening. Frame portals get loose bounds because the
// wall around them blocks a sideways approach and looseness helps a player who
// clips a corner; block portals get tight ones because nothing else stops a
// side-walker from triggering them.
bool in_portal_bounds(const Portal &p, Vec3 pos);

// Far enough past the outer face to have crossed. Measured at the EYE, because
// what decides a crossing is what the player can see.
bool past_trigger(const Portal &p, Vec3 eye);

// ------------------------------------------------------------- the crossing

// One record per player per portal. The triggered flag is what makes a crossing
// fire exactly once; seeding the arrival portal with entered_from_front false
// and triggered true is what stops it bouncing the player straight back.
struct PortalTriggerState {
    bool in_bounds          = false;
    bool entered_from_front = false;
    bool triggered          = false;
};

// Advances one record by one frame and answers whether the player crosses now.
//
// `vel` is only read for the border-entry case: a player already past the plane
// on their first in-bounds sample -- a fast faller, or somebody who came in from
// the side -- is counted as having entered from the front when they are
// descending over a floor opening. Without that clause a fall through a floor
// portal never fires, because the sample where they were in front of it never
// happened.
bool portal_step_trigger(const Portal &p, PortalTriggerState &state, Vec3 feet, Vec3 eye,
                         Vec3 vel);

// The same, sampled along a step from `from` to `to` (feet), every 0.15 node
// and at most 64 times: what a body moved past in one step still triggers.
bool trigger_along(const Portal &p, PortalTriggerState &state, Vec3 from, Vec3 to, Vec3 vel,
                   float eye_height);

// A body caught inside the opening's own block: its eye between a tenth and
// nine tenths of the block's depth and within the opening's sides, and not on
// its way out (moving along the normal). The eye and not the feet, so the
// view stays where the opening showed it. Such
// a body crosses whether or not the trigger is armed. One that came out of a
// floor opening and fell straight back in arrived seeded, so the trigger stayed
// disarmed, and it stood in the floor, refused by the server at every move --
// reported from play twice (2026-10-05).
bool portal_swallows(const Portal &p, Vec3 feet, float eye_height, Vec3 vel);

// Seeds the destination's record after a crossing, so arriving does not
// immediately trigger the return trip.
void portal_seed_arrival(const Portal &dst, PortalTriggerState &state, Vec3 feet);

// ------------------------------------------------------------ the traversal

// ------------------------------------------------- falling through a floor
//
// A floor portal has a problem no vertical one has: something must hold the
// frame up, and that something is solid ground exactly where the player has to
// fall through. PORTAL_SYSTEM 2.3's answer is a physics override -- raise the
// BOTTOM of the collision box so it stops intersecting the blocking node, and
// the player sinks until the eye passes the trigger depth on its own.
//
// The gate is a PREDICTED position, not a velocity, and that is the whole
// subtlety. A fast faller whose shift is overrun gets arrested on the block
// with vy about zero; a velocity gate would then clear the shift and leave them
// standing there forever, never reaching trigger depth. Predicting where the
// feet will be next tick engages early for a fast fall AND keeps re-engaging
// for somebody already stuck, which is the case that has no other way out.
//
// Returns how far to raise the box bottom, 0 when this portal is not underfoot.
// Capped at kMaxFloorShift: the box is 1.8 tall and a shift that reached the
// top would leave nothing to collide with anywhere, including the walls.
// Underfoot means the whole box, `half_width` either side of the feet, is over
// the opening: see the definition.
inline constexpr float kMaxFloorShift = 1.7f;

float portal_floor_shift(const Portal &p, Vec3 feet, Vec3 velocity, float dt,
                         float half_width = kPlayerHalfWidth);

// ------------------------------------------------- into a wall portal
//
// A wall portal can stand INSIDE the wall (a mod hollows the wall's cells into
// shells round it, games/chambers), and then a body has to go into the hole to
// reach the plane: a body 1.8 tall in a hole 2 tall, so a jump put its head
// against the wall over the hole and it stopped before the plane. Before such
// an opening -- the eye inside its frame, the body across it -- the collision
// box is made to fit the hole: its bottom raised and its top lowered into the
// opening's height, and narrowed into its width, `margin` in from each edge
// for what lines the hole. The server exempts the wall round the opening
// (ServerSessions::floor_exemptions) for the same reason.
struct WallShift {
    bool  on     = false;
    float bottom = 0.0f;   // raise the box's bottom by this
    float top    = 0.0f;   // and lower its top by this
    float half   = 0.0f;   // and make it this wide each way
};
WallShift portal_wall_shift(const Portal &p, Vec3 feet, float half_width, float height, float eye,
                            float margin = 0.07f);

// ------------------------------------------------- funnelling into a floor
//
// The classic portal-puzzle funnel: a body falling towards a floor
// opening is steered into it. Aiming a fall at a hole one node wide from ten
// nodes up is aiming at a coin, and a fall that lands on the frame beside it is
// the most common way a floor portal fails a player who did nothing wrong.
//
// Only while falling, onto an opening that faces up, and only when the fall
// would land within a node of its edge anyway: a body falling somewhere else is
// left alone. The pull is towards the horizontal velocity that lands at the
// centre, at most kFunnelAccel, so it is a nudge a player can still see and
// steer against.
inline constexpr float kFunnelAccel = 15.0f;   // m/s^2

// The change to the horizontal velocity this frame; zero when this opening is
// not where the body is falling.
Vec3 portal_funnel(const Portal &p, Vec3 feet, Vec3 velocity, float gravity, float dt);

struct Traversal {
    Vec3  feet;                 // where the player is put
    Vec3  look;                 // unit, transformed
    float yaw   = 0.0f;         // this engine's convention, see the note above
    float pitch = 0.0f;

    // The horizon comes out tilted whenever the two bases differ. The client
    // starts rolled by this and animates it back to zero, which is the
    // Portal-puzzle landing rather than a snap or a permanently tilted view.
    float roll = 0.0f;

    // A DELTA, not an absolute. Both the server's speed override and the
    // client's own physics work in deltas, and an absolute velocity would be
    // overwritten by whatever the client computed for the same frame.
    Vec3 velocity_delta;

    // The feet landed inside something and were lifted onto it. The player
    // stands at `feet`; the camera is told to start `warp_offset` lower -- at
    // the seamless exit -- and ease up over a fraction of a second. Without it a
    // portal that opens at knee height throws the view a metre up in one frame.
    Vec3 warp_offset;
    bool stepped_up = false;

    // Nothing could be found to stand in. The caller must not teleport: a
    // destination that is walled in should refuse the crossing rather than
    // swallow the player.
    bool blocked = false;
};

// Computes a crossing. `world` and `registry` may both be null, in which case
// no step-up is attempted and `blocked` is never set -- which is what the pure
// geometry checks want, and what a caller with no world to consult gets.
// `half_width` is the arriving body's (MovementRules::width): a wide body is
// pushed further clear of the wall it comes out beside.
Traversal portal_traverse(const Portal &src, const Portal &dst, Vec3 feet, Vec3 look, Vec3 vel,
                          const World *world, const NodeRegistry *registry,
                          float eye_height = kPlayerEyeHeight,
                          float half_width = kPlayerHalfWidth);

} // namespace cnp
