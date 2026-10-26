#pragma once

#include "core/math/vec.h"
#include "core/registry/node_registry.h"
#include "core/registry/node_shape.h"
#include "core/voxel/chunk.h"

namespace cnp {

struct AABB {
    Vec3 min;
    Vec3 max;
};

// Player box: 0.6 wide, 1.8 tall, origin at the feet.
inline constexpr float kPlayerHalfWidth = 0.3f;
inline constexpr float kPlayerHeight    = 1.8f;
inline constexpr float kPlayerEyeHeight = 1.62f;

// How far a box may touch a solid before it counts as overlapping it. A box
// whose far face lands exactly on a node boundary touches that node and does
// not enter it, so the boundary belongs to the node behind, and the collision
// resolver leaves the box a hair short of whatever it lands on for the same
// reason: resolving exactly onto the plane leaves the next frame's overlap
// test ambiguous and the player alternates between standing and falling.
//
// Public because more than one place has to agree about it. `box_hits_world`
// decides where a player can WALK, `inside_solid` decides which positions are
// accepted -- by the server's validator and by a portal's arrival. Two rules
// for one question is how a player ends up refused from a place they legally
// stood: measured as a crossing turned away with "the destination world is
// blocked" for an entry a fifth of a node off centre.
inline constexpr float kBoxSkin = 1e-4f;

// `bottom_shift` raises the BOTTOM of the box without moving its top. Zero for
// everything except a player standing over a solid floor portal, where it is
// what lets them sink through the block holding the frame up -- see
// portal_floor_shift in core/portal/portal_traverse.h. It is a parameter rather
// than a field because the box has no state: whoever asks for it that frame
// knows why.
AABB player_box(Vec3 feet_pos, float bottom_shift = 0.0f);

// The same box for a body of another width (core.body_width). Square, whichever
// way the body faces: a box that turned with it would be a resolver of its own,
// and a square the width of a kart is what a voxel game's boat has always been.
// `height` is a player's unless said: a creature smaller than one (a pet)
// fits under what a player does not.
AABB body_box(Vec3 feet_pos, float half_width, float bottom_shift = 0.0f,
              float height = kPlayerHeight);

// Whether any cell a box overlaps is a liquid (core.node_liquid). A body in one
// swims, and a swimming body is held up by what it swims in -- which is a
// question the client's movement and the server's validator both ask, so it is
// asked here, once.
bool box_in_liquid(const World &world, const NodeRegistry &reg, const AABB &box);

// Whether any cell the box is in holds something climbable (a ladder): the
// body clings instead of falling.
bool box_on_climbable(const World &world, const NodeRegistry &reg, const AABB &box);

// The belt (NodeDef::conveyor) something whose bottom is at (x, y, z) stands on:
// the one in its column, or the one under that for a ramp's top half, whose
// surface is within 0.3 of y. `dx, dz` is the way it runs and `surface` its
// height at x, z. Stacks, players and creatures are all carried by this.
struct Belt {
    bool  on = false;
    int   x = 0, y = 0, z = 0;
    float dx = 0.0f, dz = 0.0f, speed = 0.0f, surface = 0.0f;
};
Belt belt_under(const World &world, const NodeRegistry &reg, float x, float y, float z);

// A slide: a belt that runs downhill fast (conveyor at least kChuteSpeed, rise
// under 0) is a chute a body rides, not a ramp it walks (player_update). Its
// bed falls `rise` from `height` over the cell along its middle line: straight
// across the cell, or a quarter circle about the corner of the side it comes in
// by and the one it leaves by -- in by the side a slide one cell up runs into
// it from, straight on when none does. chute_at is the chute whose bed is
// within kChuteReach of feet at (x, y, z), with its middle line nearest to
// them, the way along it there (horizontal, length 1), and the bed's height.
inline constexpr float kChuteSpeed = 3.0f;
inline constexpr float kChuteReach = 0.35f;
struct Chute {
    bool  on = false;
    float mx = 0.0f, mz = 0.0f;   // the nearest point of the middle line
    float tx = 0.0f, tz = 0.0f;   // the way along it
    float surface = 0.0f, speed = 0.0f;
};
Chute chute_at(const World &world, const NodeRegistry &reg, float x, float y, float z);

// A ledge a body can hang from: in the column next to it along (dx, dz), a
// block whose top is `edge`, with two open cells over it -- room to climb up
// and stand. `edge` is absolute; the caller says how far above the feet it
// may be. `face` is where the wall's face is along that axis.
struct Ledge {
    bool  found = false;
    float edge  = 0.0f;
    float face  = 0.0f;
};
Ledge find_ledge(const World &world, const NodeRegistry &reg, Vec3 feet, float half_width,
                 int dx, int dz, float lowest, float highest);

// Whether a body is at a ledge in any of the four directions, with its hands
// on it or anywhere in the pull up over it: the server's validator counts
// such a body as held, as it counts one on a ladder.
// `dx`, `dz`, when given, say which way the wall is.
bool body_at_ledge(const World &world, const NodeRegistry &reg, Vec3 feet, float half_width,
                   int *dx = nullptr, int *dz = nullptr);

// How far under a ledge the feet hang: the hands, over the robot's head, on
// its edge.
inline constexpr float kHangDrop = 1.85f;


// Do two boxes overlap? Touching is not overlapping: a box whose face lands
// exactly on another's face shares a plane and no volume, which is the same
// bargain kBoxSkin strikes with the world.
//
// Public because two places ask it about the same pair and must agree: the
// client, deciding whether the block it is about to place is where the player
// is standing, and the server, deciding whether to allow that placement at all.
// Written twice they drift, and the drift shows up as a player who can seal
// somebody into a wall on one build and not on another.
bool boxes_overlap(const AABB &a, const AABB &b);

// Where a ray enters a box, if it enters it at all.
//
// `dir` must be unit length; `t` comes back as the distance along it at which
// the ray crosses into the box, and is zero when the origin is already inside.
// False when the ray misses, when the box is behind it, or when the crossing is
// further away than `max_distance`.
//
// It lives here rather than in the client because it decides what a player is
// AIMING AT, and what a player is aiming at has to be the same question on both
// ends of a wire eventually. Today only the client asks it -- the server checks
// a distance and not a ray, and that difference is stated in ServerSessions::
// strike rather than hidden.
bool ray_hits_box(const AABB &b, Vec3 origin, Vec3 dir, float max_distance, float &t);

// The same, and WHICH FACE it came in by, as a unit normal pointing out of the
// box. All three zero when the ray started inside it, which is a crossing that
// happened before the ray did and has no face.
//
// The face matters to whoever is aiming: it is the side a block gets built on.
// A cell-shaped raycast could take it from the boundary it crossed last; a box
// inside the cell has faces of its own, and the top of a slab is not the top of
// its cell.
bool ray_hits_box(const AABB &b, Vec3 origin, Vec3 dir, float max_distance, float &t,
                  int normal[3]);

// Does the block in ONE cell stop this box?
//
// The cell is named rather than worked out from the box, because both callers
// are already walking the cells the box covers and would otherwise walk them
// twice. It answers for a box that is known to reach into the cell: a solid
// cube therefore stops it without further arithmetic, and only a block with a
// SHAPE (registry/node_shape.h) has its boxes tested.
//
// Public because the collision resolver and the server's movement validator
// both have to use it. They asked `is_solid` separately before this, which was
// one rule in two copies for as long as a cell was all or nothing, and would be
// two different shapes for the same block the moment one of them learned about
// boxes and the other did not.
//
// `was`, when given, is where the body was before this move: a door's panel
// it already overlaps -- a door shut on whoever stood in it -- does not stop
// it, so it walks out instead of being held in the panel for good.
bool node_blocks_box(const World &world, const NodeRegistry &reg, int bx, int by, int bz,
                     const AABB &b, const AABB *was = nullptr);

// Does this box overlap any solid node? The rule the collision resolver uses,
// and the one `inside_solid` has to agree with -- it is public so that
// agreement can be checked rather than assumed. See kBoxSkin.
bool box_hits_world(const World &world, const NodeRegistry &reg, const AABB &b,
                    const AABB *was = nullptr);

// Whether this box overlaps one machine's hull (World::movers).
bool box_meets_mover(const World::Mover &m, const AABB &b);

struct MoveResult {
    Vec3 pos;
    bool on_ground   = false;
    bool hit_ceiling = false;
};

// How high a body walks over without jumping.
//
// A shape half a node tall is below what a jump clears, so without this a slab
// is a wall to be hopped -- and a stair, which is a shape whose whole purpose is
// to be walked up, is a row of them. The original engine calls it `stepheight`.
//
// Less than a node, and that is the number's whole job: a cube must stay a wall.
// 0.55 clears the half-node shapes and the little more that a mod is likely to
// put under a door, and refuses everything a body should have to jump.
inline constexpr float kStepHeight = 0.55f;

// Moves an axis-aligned box through the voxel world, resolving one axis at a
// time. Resolving all three at once is what produces the classic bug of
// sticking on the seam between two flush blocks: the box is briefly inside
// both, and the smallest-penetration heuristic picks the wrong normal.
//
// A horizontal step that is blocked is tried again `step_height` higher, and the
// body is set back down onto whatever is under it: that is what walks a body up
// a slab. Only while it is standing on something -- a body climbing half a node
// at a time in mid air is a body climbing a wall -- and never into a ceiling,
// where the whole step is refused rather than half taken.
MoveResult move_box(const World &world, const NodeRegistry &reg, Vec3 feet_pos,
                    Vec3 delta, float bottom_shift = 0.0f,
                    float step_height = kStepHeight, float half_width = kPlayerHalfWidth,
                    float height = kPlayerHeight);

// The longest step move_body hands the resolver, in nodes.
//
// move_box tests where a step ENDS and not what it crosses, so a step longer
// than the body's narrowest extent can start wholly on one side of a box and
// end wholly on the other: measured, a step of three nodes from x = 9.5 ended
// at 12.5, on the far side of a wall one node thick (tests/physics_box_test).
// The body is six tenths wide, and a step of at most half a node cannot clear
// ANY box that way, however thin -- which is the point, since a mod's shape can
// be a sixteenth of a node.
inline constexpr float kMaxStep = 0.5f;

// And the most steps one call takes. A delta longer than this many half-nodes
// is walked in longer steps rather than in more of them: a body is never asked
// to move that far by anything this engine produces -- the fastest a world may
// declare, doubled for flying at a sprint, is 26 nodes over the longest update
// -- and a bound on the loop is a bound on what a wrong number costs.
inline constexpr int kMaxSteps = 64;

// move_box, in steps no longer than kMaxStep. What a body moving under its own
// speed calls: a world may declare that speed (movement.h, MovementRules), and
// a fall is 60 m/s already, which is a node and a half over a slow update.
//
// `on_ground` is the LAST step's -- a body that walked off a ledge halfway
// through is falling -- and `hit_ceiling` is any step's, because a head that
// met a ceiling once has had its upward speed spent.
MoveResult move_body(const World &world, const NodeRegistry &reg, Vec3 feet_pos,
                     Vec3 delta, float bottom_shift = 0.0f,
                     float step_height = kStepHeight, float half_width = kPlayerHalfWidth,
                     float height = kPlayerHeight);

} // namespace cnp
