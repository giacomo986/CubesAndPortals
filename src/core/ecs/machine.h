#pragma once

#include "flecs.h"

#include "core/physics/craft.h"

#include <memory>

// What makes a mod's thing a machine (core.craft): a jeep, a tank, a
// helicopter, a plane, and what rides on one -- a turret on a hull, a gun on
// the turret. The simulation is core/physics/craft.h; this is where it meets
// the entities, once a tick on the server (step_machines), before the riders
// are put on their seats.

namespace cnp {
class World;
class NodeRegistry;
} // namespace cnp

namespace cnp::ecs {

// A Thing that is a machine: its simulation, what its driver asks this tick,
// and what it did last.
struct Machine {
    std::shared_ptr<cnp::Craft> sim;
    CraftKeys                   keys;
    CraftPose                   pose;
    float                       impact = 0.0f;   // the hardest knock since a mod last asked
};

// A Thing carried on another (core.craft.mount): at x, y, z in the other's
// frame, turned by yaw from it and pitched nose up by pitch -- a turret on a
// hull, a barrel on the turret.
struct Mounted {
    flecs::entity_t on = 0;
    Vec3            at;
    float           yaw = 0.0f, pitch = 0.0f;
};

// How far a Thing is rolled, right side down (protocol 98).
struct Roll {
    float roll = 0.0f;
};

// Every machine on by dt, two that overlap pushed apart (each is a Jolt
// world of its own, so nothing else keeps them out of each other), then
// everything mounted on one put where it rides.
void step_machines(flecs::world &w, const World &world, const NodeRegistry &reg, float dt);

// A body on foot, feet at `feet`, inside a machine's hull: which way across
// the ground it gets out soonest (a unit vector), and how fast the machine is
// going. False when it is in none. The hull is taken as turned by its yaw
// alone.
bool in_machine(flecs::world &w, Vec3 feet, float half_width, float height, Vec3 &out, Vec3 &machine_vel);

// Whether feet there may be standing on a machine's roof, as a client a
// moment behind the server draws it: over its top or a little under, and
// as far off it across as it goes in a third of a second.
bool on_machine(flecs::world &w, Vec3 feet, float half_width);

} // namespace cnp::ecs
