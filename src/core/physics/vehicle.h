#pragma once

#include "core/physics/player.h"

#include <functional>
#include <vector>

// A body that drives (core.drive) as a rigid body on four wheels.
//
// The player on foot is a box the engine's own resolver moves (aabb.h), and
// that is the right tool for a person: it is the rule the server validates
// with, it steps up slabs, it sinks through a floor portal. A kart is not a
// person. It wants tyres that grip and then slide, springs that let it pitch
// under braking and roll in a bend, and a weight that carries it wide when it
// arrives too fast -- which is a vehicle simulation, and Jolt has a good one
// (WheeledVehicleController). So a body that drives is a Jolt body, in a Jolt
// world of its own that holds nothing but it and the blocks around it.
//
// Nothing outside vehicle.cpp sees a Jolt type. What comes out is what the rest
// of the engine already reads off a player: the feet, the heading, a velocity,
// whether it stands on something -- and the chassis's attitude, for drawing it.
//
// And the server runs the same chassis (TODO.md OPEN-47). A client's update
// writes down what it did -- where the chassis was put down, and the keys of
// every fixed step after -- and a server that does the same things in the same
// order with the same blocks gets the same kart, bit for bit: Jolt is built
// deterministic, and both ends link this file.

namespace cnp {

// The keys one fixed step reads: a throttle and a wheel, -1 to 1, in 127ths
// because that is what the wire carries and the client has to drive by the
// same numbers the server replays; and jump.
struct DriveKeys {
    float forward = 0.0f;
    float right   = 0.0f;
    bool  jump    = false;
    bool  boost   = false;   // shift: the nitro
    bool  drift   = false;   // ctrl: the rear tyres let go, a drift (kDriftGrip)
};

DriveKeys drive_keys(const PlayerInput &input);

// One thing an update did to the chassis: put it down afresh at the player's
// feet, velocity and heading, or one fixed step under these keys.
//
// And when it built the blocks around the chassis again (a put always does),
// which of the chunks they reach into it built them without, a bit each: a
// client drives over what it has been sent, and a copy has to be built from the
// same blocks to the last one -- even a block the kart never touches changes
// the order Jolt meets the ones it does, and the last bit of every number
// after it (measured: client and server apart from the first rebuild after a
// chunk had not arrived, 0.38 of a node three seconds later).
struct DriveEvent {
    bool      put = false;
    Vec3      pos, vel;
    float     yaw = 0.0f;
    DriveKeys keys;
    bool      built   = false;
    uint32_t  missing = 0;
};

// One update of a body that drives: the keys are a throttle and a wheel (see
// MovementRules). The chassis is made afresh on the first call, when the
// world's numbers change, and when anything else moved the player since the
// last call -- a correction, a teleport, a portal: upright, heading the way the
// player faces, at the player's velocity. So no call site has to tell the
// vehicle it moved. What it did goes on `log` when there is one.
void vehicle_update(Player &player, const World &world, const NodeRegistry &reg,
                    const MovementRules &rules, const PlayerInput &input, float dt,
                    std::vector<DriveEvent> *log = nullptr);

// The server's half: the same two things, one at a time, from a client's log.
// A put makes the chassis afresh at player.pos, player.velocity and player.yaw;
// a step moves it one fixed step and leaves the player at its feet. The blocks
// are built without the chunks the record says were missing, where `may_miss`
// agrees one could have been.
using MayMiss = std::function<bool(ChunkPos)>;
void vehicle_put(Player &player, const World &world, const NodeRegistry &reg,
                 const MovementRules &rules, const DriveEvent &record, const MayMiss &may_miss);
void vehicle_step(Player &player, const World &world, const NodeRegistry &reg,
                  const MovementRules &rules, const DriveEvent &record, const MayMiss &may_miss);

// Where the feet were at the last two steps. A client draws, and claims, a
// point between them. False when there is no chassis.
bool vehicle_feet(const Player &player, Vec3 &was, Vec3 &now);

// What a chassis is seen doing, for drawing it: the nitro burning, and its
// tyres sliding sideways faster than kSlideSeen m/s (a drift: they leave
// marks). Off the last fixed step; nothing for a body with no chassis. The
// server reads its own copy's and sends it on (net::EntityState).
inline constexpr float kSlideSeen = 2.0f;
struct DriveLook {
    bool boosting = false;
    bool sliding  = false;
};
DriveLook vehicle_look(const Player &player);

// Jolt's types and allocator, once a process: the kart's and core.craft's.
void jolt_init();

// How long one fixed step is, in seconds: what a server charges a step against
// the time that has really passed.
float vehicle_step_seconds();

} // namespace cnp
