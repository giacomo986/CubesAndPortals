#pragma once

#include "core/math/vec.h"
#include "core/physics/movement.h"
#include "core/registry/node_registry.h"
#include "core/voxel/chunk.h"

#include <memory>
#include <vector>

namespace cnp {

struct PlayerInput {
    float move_forward = 0.0f;   // -1..1
    float move_right   = 0.0f;   // -1..1
    float look_dx      = 0.0f;   // pixels
    float look_dy      = 0.0f;
    bool  jump         = false;
    bool  sprint       = false;
    bool  toggle_fly   = false;
    bool  descend      = false;
};

// How fast the mouse turns the view, and how far up it may be turned.
//
// In the header rather than in player.cpp because a second reader appeared:
// the view trace in the client asks "how much of this frame's turn did the
// mouse ask for", and answering it with its own copy of 0.0025 would make the
// measurement agree with a constant instead of with the code.
constexpr float kLookSpeed = 0.0025f;   // radians per pixel

// Just under a right angle. Letting pitch reach exactly +-90 degrees makes the
// forward vector parallel to up, and the view basis collapses.
constexpr float kMaxPitch = 1.5533f;

// Parkour, in a world that says so (MovementRules::parkour).
inline constexpr float kWallRunMost = 5.0f;    // nodes along a wall at most
inline constexpr float kWallRunMin  = 4.0f;    // m/s along it to start
inline constexpr float kKickOut     = 5.5f;    // m/s away from a wall kicked off
inline constexpr float kKickTime    = 0.4f;    // seconds of the wall_jump clip
inline constexpr float kKickReach   = 0.45f;   // how far off a wall a kick still finds it
inline constexpr float kSlideTime   = 0.9f;    // seconds of a slide
inline constexpr float kSlideBoost  = 1.15f;   // a slide starts this much over the sprint
inline constexpr float kSlideEye    = 0.6f;    // the eye over the feet, sliding
inline constexpr float kFlipRise    = 10.5f;   // m/s up: 2.2 nodes, onto a block two high
inline constexpr float kFlipBack    = 3.0f;    // m/s backwards
inline constexpr float kFlipTime    = 0.6f;    // seconds of the backflip clip
inline constexpr float kRopeReach   = 40.0f;   // nodes a rope flies
inline constexpr float kRopeHands   = 1.9f;    // the hands over the feet, on a rope
inline constexpr float kRopePump    = 6.0f;    // m/s^2 the keys swing it
inline constexpr float kRopeReel    = 3.0f;    // m/s the rope is let out or taken in
inline constexpr float kRopeShortest = 1.5f;
inline constexpr float kChuteStart  = 2.0f;    // m/s a ride down a slide starts at, at least
inline constexpr float kChuteAccel  = 6.0f;    // m/s^2 it gathers, up to the slide's speed
inline constexpr float kChutePull   = 6.0f;    // 1/m: how fast it comes back to the middle
inline constexpr float kChuteEye    = 0.6f;    // of the eye's height, sat in a slide

struct Vehicle;      // core/physics/vehicle.cpp
struct DriveEvent;   // core/physics/vehicle.h

struct Player {
    Vec3  pos;                   // feet, not eye
    Vec3  velocity;
    float yaw       = 0.0f;      // radians; 0 looks down -Z
    float pitch     = 0.0f;
    bool  on_ground = false;
    bool  flying    = false;

    // How fast a body that drives is going along its heading, in m/s; negative
    // in reverse. Read off the chassis (core/physics/vehicle.h).
    float drive_speed = 0.0f;

    // Its nitro, 0 empty to 1 full: burnt with shift held, filled again
    // slowly (core/physics/vehicle.cpp). On the Player and not the chassis, so
    // a chassis put down afresh does not fill it -- the server's copy steps the
    // same keys and keeps its own.
    float nitro = 1.0f;

    // The chassis of a body that drives, and how it sits: nose up and right
    // side down, in radians, for drawing it. Shared because a Player is copied,
    // and a copy that took the chassis with it would leave the original none.
    std::shared_ptr<Vehicle> vehicle;
    float body_pitch = 0.0f;
    float body_roll  = 0.0f;

    // Seconds afloat in a boat (MovementRules::boat): the clock its swell
    // rocks and lifts it by.
    float swell = 0.0f;

    // And where the bottom of its model goes: under the chassis along the
    // chassis's own up, where its wheels are at rest. Not `pos`, the feet the
    // server checks, which are the highest point a wheel touches -- they jump
    // from one wheel to another over a join, and a kart drawn from them was
    // tipped about the wrong point, its rear wheels in the road and the view
    // jolting (reported from play). Valid while `vehicle` is.
    Vec3 body_base;

    // How far the bottom of the collision box is raised this frame. Set by
    // whoever owns the portals, from portal_floor_shift, and read by
    // player_update -- the physics has no opinion about portals and must not
    // grow one. Zero everywhere else.
    float cb_shift  = 0.0f;

    // And, before a wall portal it is going into, how far the box's top is
    // lowered and how narrow it is made (0: as wide as the body), so it goes
    // into the hole in the wall the portal is in without meeting the wall
    // round it (portal_wall_shift). Set by the same owner; zero elsewhere.
    float cb_top  = 0.0f;
    float cb_half = 0.0f;

    // How high the eye is over the feet: the world's (MovementRules::eye),
    // copied in by whoever applies its rules.
    float eye_height = kPlayerEyeHeight;

    // Ctrl held on foot, as the last update left it: slower, and the eye lower.
    bool crouching = false;

    // Lying on its back (core.player_lie): the middle of the body at lie_at
    // (y the surface it lies on) and its head along lie_head, a heading. The
    // eye is then at the head, on the pillow. Walking off gets it up.
    bool  lying    = false;
    Vec3  lie_at   = {};
    float lie_head = 0.0f;

    // Hanging from a ledge by the hands (find_ledge), facing it along one
    // axis, or pulling itself up over it: `mantle` is how far into the pull
    // it is, 0..1, and 0 when it is not pulling.
    bool  hanging = false;
    int   hang_dx = 0, hang_dz = 0;   // towards the wall
    float mantle  = 0.0f;
    Vec3  mantle_from, mantle_to;
    bool  stepping = false;           // the pull is a step up one block from the ground (kStepUpTime)
    float regrab  = 0.0f;             // seconds before it may grab again, after letting go
    bool  jump_held = false;          // jump still down from before the grab: not a pull yet

    // Parkour (MovementRules::parkour). Running along a wall beside it, with
    // nothing under the feet: `wall_side` +1 for a wall on the right, -1 on
    // the left, 0 when not, and how far along it so far (kWallRunMost).
    int   wall_side = 0;
    int   wall_dx = 0, wall_dz = 0;   // towards that wall
    float wall_run  = 0.0f;
    // The side of the wall last run along or kicked off, which is not used
    // again before the feet touch the ground.
    int   used_dx = 0, used_dz = 0;
    float kick  = 0.0f;   // seconds left of a kick off a wall (kKickTime), for the clip
    float slide = 0.0f;   // seconds left of a slide (kSlideTime)
    bool  low   = false;  // the box kSlideHeight tall: sliding, or under a block after one
    float flip  = 0.0f;   // seconds left of a backflip (kFlipTime)
    bool  jump_was = false, descend_was = false;   // the keys at the last update

    // Riding a slide (chute_at, player_update): sat in it, the eye low.
    bool  riding = false;

    // On a rope (core.rope): the hands kept within rope_len of rope_at, which
    // they swing about.
    bool  roped    = false;
    Vec3  rope_at  = {};
    float rope_len = 0.0f;
    Vec3  hands() const { return Vec3{pos.x, pos.y + kRopeHands, pos.z}; }

    Vec3 eye() const;
    Vec3 forward() const;
    Vec3 up() const { return Vec3{0.0f, 1.0f, 0.0f}; }
};

// How fast the wheel of a body that drives asks it to turn at full lock going
// at `speed` (m/s, negative in reverse), in rad/s: nothing standing still,
// `turn` once it rolls, and less again past half the top speed. What it gets is
// the tyres' to decide (core/physics/vehicle.cpp): past their grip it slides.
float drive_turn_rate(const MovementRules &rules, float speed);

inline constexpr float kMantleTime = 0.6f;   // seconds to pull up and over
inline constexpr float kShimmy     = 1.2f;   // m/s along a ledge
// Walking into a block one high, standing and without crouch held: the same
// pull, up and over, from the ground and quicker -- a body climbing onto it
// instead of stopping against it. Lower than kStepHeight is a step and walked.
inline constexpr float kStepUpTime = 0.35f;

// One update of a body under the player's keys.
//
// `rules` are the speeds and the gravity of the world the body is IN, which is
// a thing a world declares (core/physics/movement.h, MovementRules) and a
// client is told in the welcome. Passed in and never read from a global,
// because this process can hold two worlds and a portal between them.
//
// A body that drives writes down on `log`, when there is one, what its chassis
// did (core/physics/vehicle.h): what a client sends its server to replay.
void player_update(Player &player, const World &world, const NodeRegistry &reg,
                   const MovementRules &rules, const PlayerInput &input, float dt,
                   std::vector<DriveEvent> *log = nullptr);

// A rope shot from the eye toward what the camera's ray (cam_eye, cam_look:
// the crosshair) is on, into the first solid block within kRopeReach
// (core.rope): the body hangs on it from there. False, and no rope, when
// nothing is hit. In first person the camera's ray is the eye's look.
bool rope_fire(Player &player, const World &world, const NodeRegistry &reg, Vec3 cam_eye, Vec3 cam_look);

// A step up, let into the VIEW over a moment instead of in one frame.
//
// move_box lifts a body onto a slab or a stair in the frame it walks into it,
// half a node at once, and an eye fixed to the body went with it: a jolt on
// every slab, and a stair a row of them. The body stays where the physics puts
// it -- the server checks that -- and the eye lags: drawn `lag` under the
// body's and pulled up by a critically damped spring, which starts and stops
// without a kick. Only a rise on the ground both sides of the move and no more
// than a step counts: a jump leaves the ground, a teleport is more than a step.
struct StepSmooth {
    float lag   = 0.0f;   // how far under the body's the eye is drawn (over it, sitting down)
    float speed = 0.0f;
    bool  riding = false;  // Player::riding at the last note

    void note(const Player &p, bool was_on_ground, float y_before);
    void settle(float dt);
};

// Turns raw mouse deltas into the deltas the ROLLED camera needs, so that
// "mouse right" keeps moving the view right on the screen while the horizon is
// still tipped from a crossing.
//
// The roll lives on the camera and not in the player's yaw and pitch, which is
// what keeps the animation from fighting the mouse. The cost of that choice is
// exactly this function: yaw turns about world up and pitch about the flat
// right axis, and once the view is rolled neither of those is a screen axis any
// more. Without it, half a second of settling after every cross-axis crossing
// is half a second where the controls are diagonal, and at ninety degrees they
// are swapped outright.
//
// Derived in this engine's conventions rather than transcribed from the fork,
// which stores yaw with ninety degrees added and pitch with the opposite sign.
// Wanting the view to turn about the rolled camera's own up and right axes
// gives, for roll r,
//
//     dx' = dx cos r - dy sin r
//     dy' = dx sin r + dy cos r
//
// -- a plain rotation of the mouse vector by the roll. At r = 0 it is the
// identity, so the unrolled path is untouched by construction.
void look_apply_roll(PlayerInput &input, float roll);

// Drops the pointer warp that grabbing the cursor produces.
//
// The platform reports the change since the previous frame's pointer position.
// On the first frame there is no previous position, so the delta is wherever
// the pointer happened to be, in pixels; and grabbing warps the pointer to the
// centre of the window, which lands as another delta on the frame after. Both
// of them spin the view before the player has touched anything.
//
// Two frames, not one, because the warp arrives after the grab. And on EVERY
// grab, not only the first: dropping it once at startup left every ESC out and
// back in throwing the view by however far the pointer had wandered while it
// was free. Reported from play as the view snapping on its own, and the reason
// this is a type with a name rather than an int in a loop -- an int in a loop
// is what got re-armed in one place and not the other.
struct LookSettle {
    int left = 2;

    void grabbed() { left = 2; }

    void apply(PlayerInput &input)
    {
        if (left <= 0) return;
        --left;
        input.look_dx = 0.0f;
        input.look_dy = 0.0f;
    }
};

} // namespace cnp
