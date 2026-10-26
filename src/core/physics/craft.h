#pragma once

#include "core/math/vec.h"

#include <cstdint>

#include <memory>

// A machine a mod puts in the world and players get into (core.craft): a jeep,
// a tank, a helicopter, a plane. Not a player's body, as a kart is (vehicle.h):
// an entity of its own, which stays where it was left, carries several riders
// and is driven by whichever of them a mod hands the keys.
//
// The same Jolt as the kart, and like it each machine is a Jolt world of its
// own holding it and the blocks around it -- so two machines meet only as
// step_machines parts them (core/ecs/machine.h), and the server alone runs them: a rider's client is sent where its seat is, as
// a passenger on a body is (core.ride). What comes out is a pose.

namespace cnp {

class World;
class NodeRegistry;

enum class CraftKind : uint8_t {
    Wheeled,   // four wheels, the front two steering, all four driven
    Tracked,   // two tracks, steered by running one slower than the other
    Rotor,     // lifted along its own up, tipped the way it should go
    Wing,      // pushed along its nose and lifted by how fast it goes
};

struct CraftSpec {
    CraftKind kind = CraftKind::Wheeled;
    float mass   = 600.0f;               // kg
    float half_w = 1.0f, half_h = 0.5f, half_l = 2.0f;   // the hull, nodes
    float accel  = 6.0f;                 // m/s^2 at full throttle
    float top    = 14.0f;                // m/s, forward
    float turn   = 1.0f;                 // rad/s at full lock or stick
    float gravity = -25.0f;
};

// What a driver asks of it this step, each -1 to 1. Wheeled and Tracked: the
// throttle and the wheel. Rotor: forward and right tip it, up is the
// collective, yaw turns it. Wing: up is the throttle (it holds what it was
// given), forward the stick's pitch (nose down), right its roll, yaw the
// rudder.
struct CraftKeys {
    float forward = 0.0f, right = 0.0f, up = 0.0f, yaw = 0.0f;
};

struct CraftPose {
    Vec3  base;                          // the middle of the bottom of its model
    float yaw = 0.0f, pitch = 0.0f, roll = 0.0f;   // as a player's: yaw 0 faces -z
    Vec3  velocity;
    float speed = 0.0f;                  // along its nose
    bool  grounded = false;              // a wheel, a track or the hull on something
    float impact = 0.0f;                 // the hardest knock since the last pose, m/s
    float throttle = 0.0f;               // a Wing's
};

struct Craft;

std::shared_ptr<Craft> craft_make(const CraftSpec &spec, Vec3 base, float yaw, const World &world,
                                  const NodeRegistry &reg);

// Runs it on by dt in fixed steps, the blocks round it read from `world`.
void craft_step(Craft &craft, const World &world, const NodeRegistry &reg, const CraftKeys &keys,
                float dt);

// Where it is now. Reading it clears the impact.
CraftPose craft_pose(Craft &craft);

// Adds to its velocity at once: a blast, a shell.
void craft_push(Craft &craft, Vec3 dv);

// Puts it where a pose says, turned as it says and moving at its velocity --
// its spin kept: a client's copy of a machine pulled to the server's.
void craft_place(Craft &craft, const CraftPose &pose);

// What it was made as, its sizes clamped; and how far its hull's middle stands
// over the pose's base, along its up.
const CraftSpec &craft_spec(const Craft &craft);
float            craft_ride(const Craft &craft);

// A point given in a machine's own frame -- x to its right, y up, z to its
// front -- in the world's, the machine at `base` turned by a player's yaw, a
// pitch nose up and a roll right side down: where a seat or a turret is.
inline Vec3 craft_point(Vec3 base, float yaw, float pitch, float roll, Vec3 local)
{
    const float sy = std::sin(yaw), cy = std::cos(yaw), sp = std::sin(pitch), cp = std::cos(pitch);
    const float sr = std::sin(roll), cr = std::cos(roll);
    const Vec3  front{sy * cp, sp, -cy * cp};
    const Vec3  right0{cy, 0.0f, sy};
    const Vec3  up0{-sy * sp, cp, cy * sp};
    const Vec3  right = right0 * cr - up0 * sr;
    const Vec3  up    = up0 * cr + right0 * sr;
    return base + right * local.x + up * local.y + front * local.z;
}

// And back: a point in the world's frame in a machine's own.
inline Vec3 craft_local(Vec3 base, float yaw, float pitch, float roll, Vec3 world)
{
    const Vec3 o = craft_point(base, yaw, pitch, roll, Vec3{});
    const Vec3 r = craft_point(base, yaw, pitch, roll, Vec3{1.0f, 0.0f, 0.0f}) - o;
    const Vec3 u = craft_point(base, yaw, pitch, roll, Vec3{0.0f, 1.0f, 0.0f}) - o;
    const Vec3 f = craft_point(base, yaw, pitch, roll, Vec3{0.0f, 0.0f, 1.0f}) - o;
    const Vec3 d = world - o;
    return Vec3{dot(d, r), dot(d, u), dot(d, f)};
}

} // namespace cnp
