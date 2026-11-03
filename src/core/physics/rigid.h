#pragma once

#include "core/math/vec.h"
#include "core/physics/movement.h"

#include <cstdint>
#include <memory>

// Boxes that tumble (core.mob_tumble): a creature's body handed to Jolt as a
// rigid box, so that it slides, tips over an edge, rolls, bounces and is
// stacked as a box is -- a storage cube thrown, knocked, dropped or piled on
// another. All of one world's boxes are in one Jolt world (a RigidSpace), so
// they meet each other; round each, the blocks within a few nodes of it, read
// again as it moves. The server alone runs them, and what comes out is where
// each is and how it is turned.
//
// A turn is three axes, the model's own x, y and z in the world -- what a
// portal maps (portal_transform_dir) as it maps a direction -- and is drawn as
// yaw, pitch and roll (rigid_angles), the order a body's model is placed in
// (the client's body_place), turned about the box's middle.

namespace cnp {

class World;
class NodeRegistry;

struct RigidSpec {
    Vec3  half{0.35f, 0.35f, 0.35f};   // the box, nodes
    float mass     = 25.0f;             // kg
    float friction = 0.7f;
    float bounce   = 0.15f;             // restitution
};

struct RigidState {
    Vec3 centre;
    Vec3 axis[3] = {Vec3{1, 0, 0}, Vec3{0, 1, 0}, Vec3{0, 0, 1}};   // the model's x, y, z
    Vec3 velocity;
    Vec3 spin;            // angular velocity, rad/s about the world's axes
    bool resting = false; // Jolt put it to sleep
};

struct RigidSpace;
using RigidId = uint32_t;

// One world's boxes, falling at `gravity` (a creature's, so a cube falls as
// it did).
std::shared_ptr<RigidSpace> rigid_space(float gravity = kGravity);
RigidId rigid_add(RigidSpace &sp, const RigidSpec &spec, const RigidState &state, const World &world,
                  const NodeRegistry &reg);
void    rigid_remove(RigidSpace &sp, RigidId id);
// Every box on by dt in fixed steps, together.
void rigid_step(RigidSpace &sp, const World &world, const NodeRegistry &reg, float dt);

RigidState rigid_state(const RigidSpace &sp, RigidId id);
// Put there, turned so and moving so: carried, placed by a mod, through a portal.
void rigid_set(RigidSpace &sp, RigidId id, const RigidState &s);
// A change of velocity given at a point of the world: off its middle it turns
// it too -- a box shoved high tips, one shoved low slides.
void rigid_impulse(RigidSpace &sp, RigidId id, Vec3 dv, Vec3 at);
// The hardest knock since last asked, m/s (a sound's).
float rigid_impact(RigidSpace &sp, RigidId id);
RigidSpec rigid_spec(const RigidSpace &sp, RigidId id);
// How far the box reaches from its middle along the world's x, y and z, as it
// is turned now.
Vec3 rigid_extent(const RigidSpec &spec, const RigidState &s);

// The axes as the yaw, pitch and roll a body's model is placed by: the model
// turned by Ry(-yaw) Rx(pitch) Rz(-roll).
void rigid_angles(const Vec3 axis[3], float &yaw, float &pitch, float &roll);

} // namespace cnp
