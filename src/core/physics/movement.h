#pragma once

#include "core/physics/aabb.h"
#include "core/registry/node_registry.h"
#include "core/voxel/chunk.h"

#include <string>

// How fast a player may move, and the server's opinion about whether they did.
//
// The constants were in the client, which is where they get used every frame
// and the wrong place for them to live: the server has to know the same numbers
// to have any opinion at all, and two copies of "how fast is a sprint" is a
// disagreement waiting for its first cheater. Same argument as one block table
// and one mapgen, and it is the third time it has come up.
//
// What this is not: a server-side simulation of movement. DESIGN.md 3.3 asks
// for "anti-cheat leggero (speed cap + distanza di interazione + inventari
// server-side)", and that is what this is -- the server checks that a claimed
// position could have been reached, and that it is not inside a wall. A player
// who plays normally is never corrected; a player who claims to be 400 blocks
// away is put back. Rewinding and replaying their input properly is a much
// larger thing and it is not pretended to be here.

namespace cnp {

inline constexpr float kGravity     = -25.0f;
inline constexpr float kJumpSpeed   = 8.0f;
inline constexpr float kWalkSpeed   = 4.8f;
inline constexpr float kSprintSpeed = 8.0f;
inline constexpr float kFlySpeed    = 14.0f;

// How much faster flying is while sprint is held. Here and not in the client,
// for the reason every number in this file is here: the validator's cap is
// "the fastest any state allows", and a state the validator had not heard of is
// how an unmodified client got corrected for holding two keys (see
// MovementLimits::max_horizontal).
inline constexpr float kFlySprintFactor = 2.0f;

// The fastest a world may declare any of its speeds, in m/s.
//
// A number chosen, not derived, and what it bounds is WORK: the client moves a
// body in steps of at most half a node (physics/aabb.h, move_body), so a speed
// is a count of resolver calls per update, and 256 m/s over the client's longest
// update is 26 of them. It is about 920 km/h, which is past anything a race or
// an arena asks for.
inline constexpr float kMaxDeclaredSpeed = 256.0f;

// And the most a world may declare for driving (MovementRules::accel and the
// rest). An acceleration or a braking of 256 m/s^2 reaches the top speed a
// world may declare in a second; two full turns a second is past any wheel; a
// drag of 20 stops a body in a twentieth of a second, which is a wall.
inline constexpr float kMaxDeclaredAccel = 256.0f;
inline constexpr float kMaxDeclaredTurn  = 12.566f;
inline constexpr float kMaxDeclaredDrag  = 20.0f;

// And the highest a world may let a body be over where it last stood
// (MovementRules::rise): two chunks, which the ground under it is streamed for.
inline constexpr float kMaxDeclaredRise = 64.0f;

// How wide a world may declare its bodies (core.body_width), in nodes. Under a
// fifth of a node a body slips between shapes a mod draws a sixteenth thick;
// over four it no longer fits the portal an arrival is pushed clear of.
inline constexpr float kMinBodyWidth = 0.2f;
inline constexpr float kMaxBodyWidth = 4.0f;

// How fast a body's horizontal velocity closes on what the keys ask for, in
// 1/s: v = want + (v - want) * exp(-k dt), which is the same curve at any frame
// rate. The body has weight -- 95% of the pace in 3/k seconds, and a stop from
// a sprint that slides 8/k nodes.
//
// On the ground five: 0.6 s to get going or to stop, 1.6 nodes of slide from a
// sprint, asked for by Giacomo (2026-09-19) over a lighter 0.25 s. It is also
// what spends the speed a portal hands over, which the keys used to replace on
// the first tick after landing: 60 m/s out of a wall comes under a world's
// walking cap in a third of a second, about five nodes past the cap, inside the
// second of travel a claim may have in hand (MovementLimits::max_burst).
//
// Water drags less than the ground grips, and flying pushes as water does.
inline constexpr float kGroundGrip = 5.0f;
inline constexpr float kSwimGrip   = 2.0f;
inline constexpr float kFlyGrip    = 2.0f;
// Flying with nothing pressed on an axis, the body coasts: a stop from the
// flying speed drifts 14 nodes over about three seconds. A key the other way
// brakes at kFlyGrip, so stopping where you want is a tap backwards.
inline constexpr float kFlyCoast   = 1.0f;

// How the keys steer a body that is not standing on anything: the classic
// FPS air acceleration, the movement portal puzzle games are played with.
//
// The keys may add along the direction asked for, and only until the velocity
// ALONG that direction reaches kAirSpeedCap -- 30 units a second there, scaled
// here by the player's height, 72 units to 1.8 m. So a jump from standing goes
// nearly straight up, a running jump keeps its speed, a key held sideways bends
// the path a little (and turning the mouse with it bends it more), and a key
// held against the motion brakes at kAirAccelerate times the pace, 48 m/s^2 at
// a walk. Nothing pressed changes nothing: the speed a fall into a floor portal
// turns into forward speed out of a wall is kept, which is the one thing a
// portal does that nothing else in a voxel game does.
//
// One thing Source allows and this does not: the keys never take the
// horizontal speed past the larger of the sprint and what it already was. In
// Source that bend adds speed every tick a strafe and a turn agree, which is
// bunny hopping; here it would be a body faster than its world's cap, and a
// body the server corrects.
inline constexpr float kAirAccelerate = 10.0f;   // 1/s, times the pace
inline constexpr float kAirSpeedCap   = 0.75f;   // m/s along the keys' direction

// Falling is the fastest a player legitimately moves, and it is not capped by
// the walk speed. Terminal velocity here is a number this engine chooses rather
// than derives: nothing in the client accelerates past it.
inline constexpr float kTerminalFallSpeed = 60.0f;

// On a ladder (NodeDef::climbable): how fast a body climbs with jump held, how
// fast it slides down with nothing held, and how much of its walking pace it
// keeps sideways. Up is well under a jump's speed, so the vertical budget
// already covers it; the height check is told a ladder holds a body up.
inline constexpr float kClimbUp    = 3.0f;
inline constexpr float kClimbSlide = 1.5f;
inline constexpr float kClimbPace  = 0.6f;

// How much higher than its jump a body that drives may be (MovementLimits::rise).
// Its feet are where its wheels touch, and once they leave the ground the
// bottom of the chassis (core/physics/vehicle.cpp), kChassisLift higher: a kart
// whose jump tops out at 1.28 put its feet 1.76 up, over the 1.58 the server
// allowed, and every jump was corrected (reported from play, 2026-09-21). Then
// kDriveHeadroom over that, so a jump stays clear of the ceiling a kart bounces
// off -- also one taken with jump held, from springs the last landing still
// has pressed: 1.86 for one jump, 2.23 held at 24 m/s, against a ceiling of 2.43.
inline constexpr float kChassisLift   = 0.55f;
inline constexpr float kDriveHeadroom = 0.6f;

// Crouched -- Ctrl held on foot: the walk at this pace, no sprint, and the
// eye at this share of its height. Slower is never something the server's
// speed check has to be told about; the eye is, and a crouch goes to it with
// the hand (net::Hold).
inline constexpr float kCrouchPace = 0.35f;
inline constexpr float kCrouchEye  = 0.8f;

// Swimming (core.node_liquid): the keys push a body at this share of its pace,
// gravity pulls at this share of its strength and never faster than the sink
// speed, and jump held lifts it at the rise speed. All four under what a body on
// land may do, so the validator's budgets hold them without a state of their own.
inline constexpr float kSwimPace    = 0.5f;
inline constexpr float kSwimGravity = 0.2f;
inline constexpr float kSwimSink    = 2.5f;
inline constexpr float kSwimRise    = 3.5f;

// A body in a boat (MovementRules::boat), on a liquid: held with its feet this
// far under the top of it. W and S row it along its heading -- ahead up to its
// sprint, astern up to half its walk -- gathering and losing way at the grip,
// A and D turn it (and the view with it), and the keel takes a sideways drift
// off at kBoatKeel. Up to its sprint, so the validator's budgets hold it as
// they hold a run.
inline constexpr float kBoatDraft = 0.35f;
inline constexpr float kBoatGrip  = 1.2f;
inline constexpr float kBoatKeel  = 4.0f;    // 1/s
inline constexpr float kBoatTurn  = 1.8f;    // rad/s at full rudder

// How a body moves in ONE world: what its mods declared, or the numbers above
// when they declared nothing (TODO.md OPEN-44, DESIGN.md M13).
//
// Per world and not per engine, because the worlds behind a portal are other
// games: a race wants a body faster than anything here flies, and an arena may
// want a jump that is not a sandbox's. Sent to every client in the welcome, and
// the client moves by the world's numbers rather than its own -- a client that
// ran at its own speed would be a client the server corrects, or a client the
// server should have.
//
// The terminal fall speed is NOT in here. It is what bounds a fall however
// strong the gravity, and so what bounds the work a fall costs the resolver.
// A body that drives has a nitro (core/physics/vehicle.cpp): shift held with
// the throttle down pushes it past its top speed, up to this many times it, and
// burns a gauge that fills again slowly. Here because the server's allowance
// has to know how fast that is.
inline constexpr float kNitroTop = 1.35f;

struct MovementRules {
    float walk    = kWalkSpeed;
    float sprint  = kSprintSpeed;
    float fly     = kFlySpeed;      // doubled while sprint is held: kFlySprintFactor
    float jump    = kJumpSpeed;
    float gravity = kGravity;       // m/s^2, and never upwards

    // Driving (core.drive), when `accel` is above zero: every body
    // drives. The keys are a throttle and a wheel instead of a velocity, the
    // speed is kept from one update to the next, and nobody flies. `sprint` is
    // the top speed and `walk` the top speed in reverse.
    float accel = 0.0f;   // m/s^2 with the throttle down; 0 is a body on foot
    float brake = 0.0f;   // m/s^2 braking, before the body reverses
    float turn  = 0.0f;   // rad/s at full lock, from walking pace up
    float drag  = 0.0f;   // 1/s: how fast the speed fades with nothing pressed

    // How wide a body is, in nodes (core.body_width): the side of the square
    // box it collides with, whichever way it faces. The height is the engine's.
    float width = kPlayerHalfWidth * 2.0f;

    // How long a body that drives is, nose to tail, in nodes (core.body_width's
    // second number): the chassis Jolt moves (core/physics/vehicle.h). 0 is as
    // long as it is wide. A body on foot has no use for it.
    float length = 0.0f;

    // How far above where it last stood a body that cannot fly may be, in
    // nodes (core.body_movement's `rise`): MovementLimits::rise. Negative is
    // the top of its jump, and for a body that drives that plus the chassis's
    // lift and headroom (kChassisLift). A kart on a track with no walls to keep
    // it in is 10: whatever throws it up bounces off 10 over the road, and
    // leaving the road is the track's to deal with.
    float rise = -1.0f;

    // How high the eye is over the feet, in nodes (core.eye_height): where the
    // camera is in first person and what reach and every ray a hand casts are
    // measured from, on both ends. The body's box stays kPlayerHeight tall.
    float eye = kPlayerEyeHeight;

    // Parkour (core.parkour): a body on foot here runs along walls, kicks off
    // them, slides under a block and flips backwards, and may hang on a rope
    // (core.rope) -- core/physics/player.cpp. The server then allows what
    // those do: the speed a swing builds, any height, a body slid under one
    // block (limits_for).
    bool parkour = false;

    // Whether hands grab a ledge in the air and hang from it (core.ledges):
    // a world seen from above, where a body hanging off a wall is a body
    // stuck halfway up one, says no.
    bool ledges = true;

    // In a boat (core.body_boat): on a liquid the body floats on top of it
    // and goes at its sprint instead of swimming (core/physics/player.cpp).
    // One body's, never a world's: only BodyMovement carries it.
    bool boat = false;

    bool drives() const { return accel > 0.0f; }

    // Whether a body here may leave the ground for longer than a jump. Nobody
    // flies in a body that drives, and nobody in a world that says fly 0.
    bool flies() const { return !drives() && fly > 0.0f; }
};

// What `from` said beyond the five numbers of core.movement: the driving
// numbers and the width. core.movement and core.body_movement say five, and
// saying them must not take away what core.drive or core.body_width said.
inline void keep_body(MovementRules &to, const MovementRules &from)
{
    to.accel = from.accel;
    to.brake = from.brake;
    to.turn  = from.turn;
    to.drag   = from.drag;
    to.width  = from.width;
    to.length = from.length;
    to.eye    = from.eye;
    to.parkour = from.parkour;
    to.ledges  = from.ledges;
    to.boat    = from.boat;
}

// What a body in a parkour world may do beyond walking (MovementRules::parkour),
// which the server's limits allow for.
inline constexpr float kRopeTop     = 20.0f;   // m/s: the fastest a swing goes
inline constexpr float kSlideHeight = 0.8f;    // the box sliding: under one block

struct MovementLimits {
    // The fastest horizontal speed any state allows: flying at a sprint, because
    // a player who is flying is allowed to and the server does not track that
    // state yet.
    //
    // It was the flying speed alone, and the client doubles that while sprint is
    // held. Budgeted, so it took a while to show: 28 m/s against 22.4 spends a
    // full budget in four seconds, and an unmodified client holding two keys was
    // corrected -- 17 claims of 160 in eight seconds, the first after 3.80 s
    // (tests/chunk_codec_test.cpp). A world with its own speeds gets this from
    // limits_for, by the same rule.
    float max_horizontal = kFlySpeed * kFlySprintFactor;
    float max_vertical   = kTerminalFallSpeed;

    // Latency, jitter and a client integrating at a different step all make an
    // honest player look slightly too fast over one interval. The tolerance is
    // generous on purpose: a false correction is felt by everyone who plays
    // properly, and a cheat that fits inside 60% extra speed is not the cheat
    // anyone builds.
    float tolerance = 1.6f;

    // The longest interval a single claim may refill the budget from. Without
    // it, a client that goes quiet for a minute earns a minute's worth of
    // travel in one step.
    double max_interval = 1.0;

    // The most travel a player may have saved up, in seconds of it.
    //
    // This is what a client banks by going quiet, and therefore the size of the
    // jump a cheat can make in one claim.
    //
    // Deliberately the same as max_interval, so that this change alters exactly
    // ONE thing: what the allowance is measured over. The ceiling on a single
    // claim is what it always was -- a full interval's worth of travel -- and
    // the difference is that an interval the network shortened no longer
    // shortens the allowance with it. Tightening the anti-cheat is a separate
    // decision that would need its own evidence, and half a second was tried
    // here first: it refuses the soak probe, which claims in twelve-block steps
    // at an average speed that is perfectly legal.
    //
    // It is far more than the jitter it has to absorb. At 400 ms round trip
    // with 60 ms of jitter two claims' arrival times move by at most 120 ms
    // relative to each other.
    double max_burst = 1.0;

    // Whether a position inside a solid block is refused. On by default; the
    // one honest reason to turn it off is a test.
    bool refuse_inside_solid = true;

    // The half width the "inside a solid block" test uses, which is the body's
    // on foot and a POINT for a body that drives (limits_for).
    //
    // Why a point: the server checks an upright square and the client simulates
    // a chassis that turns, tips and has wheels outside its box, and the two
    // disagree about touching. When they did, the refusal put the body back to
    // a place where the SERVER's box was clear and the CHASSIS was a third of a
    // node inside the block -- from which the client's solver needed several
    // steps to climb out, and every claim it sent meanwhile was refused again.
    // Measured as a standoff that no tolerance fixed: 5 mm over the line at a
    // slop of 5 cm, and 5 mm over it again at 15 cm, hundreds of corrections a
    // second either way (2026-09-20). A point cannot start that loop, and it
    // still refuses the cheat that matters -- a kart whose middle is inside the
    // wall it is cutting the corner through.
    float solid_half = kPlayerHalfWidth;

    // How long a teleport is allowed to look like cheating.
    //
    // DESIGN.md 3.4: the swap packet carries a grace token that suppresses the
    // speed check for N ticks on the destination server. This is that N, in
    // seconds, because everything else here is measured against a clock rather
    // than a tick count.
    //
    // A portal breaks this validator twice over. The crossing itself moves the
    // player twenty blocks in one tick, which is four hundred times the
    // allowance. And what the player carries out of it is worse: a fall into a
    // floor portal and out of a wall turns downward speed into forward speed,
    // so terminal velocity -- 60 blocks a second -- lands on the horizontal
    // axis, against a cap of 14 times 1.6, or 22.4. See docs/archive/M4_FINDINGS.md
    // 20.1, where a 11.67 m/s fall came out as 11.67 m/s forward.
    //
    // A second, because the carried speed is not decayed by anything in the
    // air unless the player brakes (kAirAccelerate), and on the ground it is
    // spent by the grip (kGroundGrip) in about a third of a second, a few nodes
    // past the cap that the budget's reserve covers. So the excess lasts until
    // the player touches ground, which from an ordinary exit is a fraction of a
    // second. The limitation this leaves is stated rather than hidden: a player
    // still airborne a second after a crossing is corrected like anybody else.
    //
    // Raising max_horizontal instead would be the wrong fix. It is the number
    // that stops a speed cheat, and a portal is not a reason to stop checking.
    double portal_grace = 1.0;

    // How far a player may reach to put something in the world.
    //
    // DESIGN.md 3.4 asks for placement validated by distance, and this is that
    // distance: the client's own reach is 6 nodes, so a claim beyond this is a
    // client that has been modified rather than a client that lagged. The extra
    // node is the gap between the position the server last accepted and the one
    // the player was standing at when they clicked -- one tick of running is
    // most of a node, and refusing that would refuse honest placements at a
    // dead sprint.
    float place_reach = 7.0f;

    // Half the width of the box a claim is checked with: the world's body
    // (MovementRules::width).
    float half_width = kPlayerHalfWidth;

    // And how far out of the destination's wall a portal puts an arrival: the
    // same half width on foot, and half the chassis's length for a body that
    // drives. A kart put 0.6 out of the wall with 1.3 of it behind its centre
    // came out with its tail in the wall, was pushed out by its own physics,
    // and was corrected for it by a countdown that allowed it nothing.
    float arrival_half = kPlayerHalfWidth;

    // How far above where it last stood a body may be, in nodes, when it
    // cannot fly: the top of a jump. Negative when it can, and then height is
    // not checked. Without it nothing refuses a modified client in a world that
    // drives which claims a climb at jump speed over the track's two-node walls:
    // the vertical budget is a speed, and a climb at that speed fits it.
    float rise = -1.0f;

    // How tall the box is that may not be inside a solid block, from the feet:
    // the body's, or less where a body slides under a block (kSlideHeight).
    float height = kPlayerHeight;
};

// Why a world's rules are not ones this engine can run, or empty when they are.
//
// Asked where rules come IN -- from a mod, and from the wire -- because a NaN
// speed is a body the resolver moves to nowhere and a speed of a million is a
// loop the resolver runs a million times. Every speed finite, not negative and
// at most kMaxDeclaredSpeed; the gravity finite and not upwards.
std::string movement_rules_problem(const MovementRules &rules);

// The validator's limits for a world whose bodies move by `rules`: the fastest
// the keys can push a body, by the same rule MovementLimits' defaults follow --
// which are therefore exactly limits_for(MovementRules{}). Everything that is
// not a speed (the tolerance, the burst, the grace, the reach) is left as it is.
MovementLimits limits_for(const MovementRules &rules);

struct MovementState {
    float  x = 0.0f, y = 0.0f, z = 0.0f;
    double when = 0.0;   // when this was accepted, in server seconds
    bool   valid = false;

    // How much travel this player has in hand, horizontally and vertically.
    //
    // A budget and not a per-claim rate, and the difference is a defect a real
    // link found. The check used to divide the distance in a claim by the time
    // since the LAST CLAIM ARRIVED -- a number the network decides, not the
    // player. On a jittery link two claims sent 50 ms apart arrive 7 ms apart
    // when the first was held longer than the second, and an honest player at
    // walking pace was corrected for "0.42 blocks in 0.007s". Ten corrections
    // in forty seconds at 400 ms round trip and 10% loss, standing still.
    //
    // Filled at the allowed speed over real time and capped, so what is bounded
    // is distance over time -- which is the property the check exists for --
    // instead of distance over an interval the client cannot control. Jitter
    // now spends a reserve that the previous late claim built up.
    float  budget_h = 0.0f, budget_v = 0.0f;

    // Whether the SERVER put the body here and no claim has been accepted since
    // (TODO.md OPEN-46).
    //
    // A path is walked from where the body has been, and a body the server has
    // just placed -- a spawn, a respawn, a teleport -- has been nowhere: its
    // client may not even know about the spot yet, and its first claim is
    // wherever that client thinks it is. four_clients found it. Every walker's
    // first claim was on its circle, twelve nodes from the spawn at the middle
    // of the plaza, and walking from the spawn met a dirt column and refused a
    // player who had done nothing but join. The first claim after a placement
    // is judged by its end and its budget, which is every claim's rule before
    // the path was, and the walking starts from the next one. The price, said:
    // in that one claim a modified client can still cross a wall inside the
    // budget.
    bool   placed = false;

    // Until when the speed check is suppressed, in the same server seconds.
    //
    // Granted by the server when the server MOVES the player -- a portal
    // crossing today, anything else that teleports later -- and never by
    // anything a client says. A token a client could ask for is a token every
    // cheat asks for; this one is a consequence of the server having decided to
    // move somebody, which is the only reason it exists.
    //
    // It suppresses the speed checks and nothing else: a graced claim inside a
    // solid block is still refused. A teleport that has to break the world's
    // own rule is not a teleport this server performs.
    double grace_until = 0.0;

    // The height of the feet the last time an accepted claim stood on
    // something, or the server put the body somewhere. What
    // MovementLimits::rise is measured from.
    float ground_y = 0.0f;
};

// Where a claim inside a solid node is NOT a cheat.
//
// A floor portal has to be held up by something, and that something is a solid
// node exactly where the player has to pass through. PORTAL_SYSTEM 2.3's answer
// is a physics override: the client raises the bottom of the collision box so
// the player sinks INTO that node until the trigger fires. The server knows
// nothing about that box -- it sees a position inside stone and refuses it, and
// the player is corrected once per claim for a portal they placed themselves.
// Measured at 97 corrections in five seconds; see docs/archive/M6_FINDINGS.md 4.
//
// A box and not a flag, because the rule being relaxed is about geometry: in
// THIS node, under THIS opening, solid is passable. Everywhere else the rule is
// untouched, and a graced teleport into a wall is still refused -- which is the
// difference between an exemption and a licence.
//
// A node is exempt only when it lies ENTIRELY inside the box. Half a node is
// not a passable node: the test has to be all-or-nothing, or a box that clips
// the corner of a wall would open that wall.
struct SolidExemption {
    float min_x = 0.0f, min_y = 0.0f, min_z = 0.0f;
    float max_x = 0.0f, max_y = 0.0f, max_z = 0.0f;
};

// One node per opening, and a world has at most net::kMaxOpenings of them
// (32; this header does not reach the wire's).
inline constexpr size_t kMaxSolidExemptions = 32;

struct MovementVerdict {
    bool accepted = false;

    // Where the player is now as far as the server is concerned. On a refusal
    // this is where they were, which is what gets sent back to them.
    float x = 0.0f, y = 0.0f, z = 0.0f;

    // What is left in this player's travel budget. The caller writes it back
    // into the MovementState it keeps -- on a refusal too, where nothing was
    // spent, so a refused claim does not also cost the player the allowance it
    // never used.
    float budget_h = 0.0f, budget_v = 0.0f;

    // And where the body last stood, written back the same way.
    float ground_y = 0.0f;

    std::string reason;   // empty when accepted
};

// Decides whether a claimed position is one the player could have reached.
// `world` and `registry` are consulted only to ask whether a block is solid.
MovementVerdict check_move(const MovementState &last, float x, float y, float z, double now,
                           const World &world, const NodeRegistry &registry,
                           const MovementLimits &limits = {});

// The same, with the nodes that hold floor portals up. Separate rather than a
// defaulted parameter so that every caller which does NOT pass exemptions is
// visibly a caller with no portals, rather than one that forgot.
MovementVerdict check_move(const MovementState &last, float x, float y, float z, double now,
                           const World &world, const NodeRegistry &registry,
                           const MovementLimits &limits, const SolidExemption *exempt,
                           size_t exempt_count);

// Whether a player standing with their feet at this position is inside
// something solid. The player is two blocks tall, which is the same shape the
// client's collision uses.
bool inside_solid(const World &world, const NodeRegistry &registry, float x, float y, float z);

// The same question, with a list of nodes that do not count (see SolidExemption),
// for a body of this half width.
bool inside_solid(const World &world, const NodeRegistry &registry, float x, float y, float z,
                  const SolidExemption *exempt, size_t exempt_count,
                  float half_width = kPlayerHalfWidth, float height = kPlayerHeight);

} // namespace cnp
