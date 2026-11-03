#include "core/physics/rigid.h"

#include "core/physics/vehicle.h"   // jolt_init
#include "core/registry/node_shape.h"
#include "core/voxel/chunk.h"

#include <Jolt/Jolt.h>

#include <Jolt/Core/JobSystemSingleThreaded.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Collision/BroadPhase/BroadPhaseLayerInterfaceTable.h>
#include <Jolt/Physics/Collision/BroadPhase/ObjectVsBroadPhaseLayerFilterTable.h>
#include <Jolt/Physics/Collision/ObjectLayerPairFilterTable.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/StaticCompoundShape.h>
#include <Jolt/Physics/PhysicsSystem.h>

#include <algorithm>
#include <cmath>
#include <unordered_map>

namespace cnp {
namespace {

constexpr float kStep          = 1.0f / 60.0f;   // fixed, as the machines'
constexpr int   kMaxRigidSteps = 15;
constexpr float kRebuildEvery  = 0.25f;          // seconds: the blocks round a box read again
constexpr float kRebuildAfter  = 1.5f;           // nodes from where they were read
constexpr int   kMaxBodies     = 2048;           // boxes and the ground round each

namespace Layers {
constexpr JPH::ObjectLayer kStill  = 0;
constexpr JPH::ObjectLayer kMoving = 1;
} // namespace Layers

JPH::Vec3 jv(Vec3 v) { return JPH::Vec3(v.x, v.y, v.z); }
Vec3      cv(JPH::Vec3Arg v) { return Vec3{v.GetX(), v.GetY(), v.GetZ()}; }

struct LayerTables {
    JPH::BroadPhaseLayerInterfaceTable bp_layers{2, 2};
    JPH::ObjectLayerPairFilterTable    pairs{2};
    std::unique_ptr<JPH::ObjectVsBroadPhaseLayerFilterTable> bp_filter;

    LayerTables()
    {
        bp_layers.MapObjectToBroadPhaseLayer(Layers::kStill, JPH::BroadPhaseLayer(0));
        bp_layers.MapObjectToBroadPhaseLayer(Layers::kMoving, JPH::BroadPhaseLayer(1));
        pairs.EnableCollision(Layers::kMoving, Layers::kStill);
        pairs.EnableCollision(Layers::kMoving, Layers::kMoving);   // box on box
        bp_filter = std::make_unique<JPH::ObjectVsBroadPhaseLayerFilterTable>(bp_layers, 2, pairs, 2);
    }
};

const LayerTables &layer_tables()
{
    static const LayerTables tables;
    return tables;
}

// Three axes as Jolt's rotation, made orthonormal on the way: a portal's map
// may hand back a mirror (a floor's basis is), which is no rotation at all.
JPH::Quat quat_of(const Vec3 axis[3])
{
    Vec3 x = normalize(axis[0]);
    Vec3 y = axis[1] - x * dot(axis[1], x);
    y      = length_sq(y) > 1e-8f ? normalize(y) : Vec3{0, 1, 0};
    Vec3 z = cross(x, y);   // right-handed, whatever the third was
    JPH::Mat44 m(JPH::Vec4(x.x, x.y, x.z, 0), JPH::Vec4(y.x, y.y, y.z, 0), JPH::Vec4(z.x, z.y, z.z, 0),
                 JPH::Vec4(0, 0, 0, 1));
    return m.GetQuaternion().Normalized();
}

struct Box {
    RigidSpec   spec;
    JPH::BodyID body;
    JPH::BodyID ground;
    Vec3        ground_centre;
    uint64_t    ground_hash  = 0;
    float       since_ground = 1e9f;
    float       impact       = 0.0f;
    JPH::Vec3   before;   // the velocity at the start of a step
};

} // namespace

struct RigidSpace {
    float gravity = kGravity;

    JPH::TempAllocatorImpl       temp{8 << 20};
    JPH::JobSystemSingleThreaded jobs{JPH::cMaxPhysicsJobs};
    JPH::PhysicsSystem           physics;

    std::unordered_map<RigidId, Box> boxes;
    RigidId                          next        = 1;
    float                            accumulator = 0.0f;

    ~RigidSpace()
    {
        JPH::BodyInterface &bodies = physics.GetBodyInterface();
        for (auto &[id, b] : boxes)
            for (JPH::BodyID one : {b.body, b.ground}) {
                if (one.IsInvalid()) continue;
                bodies.RemoveBody(one);
                bodies.DestroyBody(one);
            }
    }
};

namespace {

Vec3 centre_of(const RigidSpace &sp, const Box &b)
{
    const JPH::RVec3 at = sp.physics.GetBodyInterface().GetPosition(b.body);
    return Vec3{(float)at.GetX(), (float)at.GetY(), (float)at.GetZ()};
}

// Every solid block within reach of the box, as boxes: runs of whole cubes as
// one, a block with a shape as its own (a portal's shell, a button, a plate).
// Read again only when what is there changed, so a box asleep on the floor is
// not woken every quarter second for nothing.
void build_ground(RigidSpace &sp, Box &bx, const World &world, const NodeRegistry &reg, Vec3 centre)
{
    bx.since_ground = 0.0f;
    const int cx = (int)std::floor(centre.x), cy = (int)std::floor(centre.y), cz = (int)std::floor(centre.z);
    const int reach = (int)std::ceil(std::max({bx.spec.half.x, bx.spec.half.y, bx.spec.half.z})) + 3;

    uint64_t hash = 1469598103934665603ull ^ ((uint64_t)(uint32_t)cx << 1) ^ ((uint64_t)(uint32_t)cy << 21) ^
                    ((uint64_t)(uint32_t)cz << 41);
    for (int y = cy - reach; y <= cy + reach; ++y)
        for (int z = cz - reach; z <= cz + reach; ++z)
            for (int x = cx - reach; x <= cx + reach; ++x) {
                const BlockId id = world.get_block(x, y, z);
                hash = (hash ^ ((uint64_t)id << 8 | world.get_facing(x, y, z))) * 1099511628211ull;
            }
    if (hash == bx.ground_hash && !bx.ground.IsInvalid()) return;
    bx.ground_hash   = hash;
    bx.ground_centre = Vec3{(float)cx, (float)cy, (float)cz};

    JPH::BodyInterface &bodies = sp.physics.GetBodyInterface();
    if (!bx.ground.IsInvalid()) {
        bodies.RemoveBody(bx.ground);
        bodies.DestroyBody(bx.ground);
        bx.ground = JPH::BodyID();
    }

    JPH::StaticCompoundShapeSettings compound;
    int                              count = 0;
    const auto add = [&](float x0, float y0, float z0, float x1, float y1, float z1) {
        const JPH::Vec3 half(0.5f * (x1 - x0), 0.5f * (y1 - y0), 0.5f * (z1 - z0));
        const float     thin = std::min({half.GetX(), half.GetY(), half.GetZ()});
        if (thin <= 0.0f) return;
        const JPH::Vec3 mid(0.5f * (x0 + x1) - cx, 0.5f * (y0 + y1) - cy, 0.5f * (z0 + z1) - cz);
        compound.AddShape(mid, JPH::Quat::sIdentity(), new JPH::BoxShape(half, std::min(0.02f, thin)));
        ++count;
    };
    for (int y = cy - reach; y <= cy + reach; ++y)
        for (int z = cz - reach; z <= cz + reach; ++z) {
            int run = 0;
            for (int x = cx - reach; x <= cx + reach + 1; ++x) {
                const BlockId  id    = x <= cx + reach ? world.get_block(x, y, z) : kAir;
                const NodeDef *def   = reg.is_solid(id) ? &reg.get(id) : nullptr;
                const bool     solid = def && def->turn != NodeTurn::Height;
                const bool     cube  = solid && !node_has_shape(*def);
                if (cube) {
                    ++run;
                    continue;
                }
                if (run > 0) {
                    add((float)(x - run), (float)y, (float)z, (float)x, (float)(y + 1), (float)(z + 1));
                    run = 0;
                }
                if (solid) {
                    const uint8_t facing = world.get_facing(x, y, z);
                    for (int i = 0; i < node_shape_count(*def); ++i) {
                        const NodeBox b = node_shape_box(*def, facing, i, x, y, z);
                        add(b.min[0], b.min[1], b.min[2], b.max[0], b.max[1], b.max[2]);
                    }
                }
            }
        }
    if (count > 0) {
        JPH::ShapeSettings::ShapeResult shape = compound.Create();
        if (!shape.HasError()) {
            JPH::BodyCreationSettings settings(shape.Get(), JPH::RVec3(cx, cy, cz), JPH::Quat::sIdentity(),
                                               JPH::EMotionType::Static, Layers::kStill);
            settings.mFriction    = 0.8f;
            settings.mRestitution = 0.1f;
            bx.ground = bodies.CreateAndAddBody(settings, JPH::EActivation::DontActivate);
        }
    }
    // What it lay on changed: it may have to fall.
    bodies.ActivateBody(bx.body);
}

Box *find(RigidSpace &sp, RigidId id)
{
    const auto it = sp.boxes.find(id);
    return it == sp.boxes.end() ? nullptr : &it->second;
}
const Box *find(const RigidSpace &sp, RigidId id)
{
    const auto it = sp.boxes.find(id);
    return it == sp.boxes.end() ? nullptr : &it->second;
}

} // namespace

std::shared_ptr<RigidSpace> rigid_space(float gravity)
{
    jolt_init();
    auto sp     = std::make_shared<RigidSpace>();
    sp->gravity = gravity;
    const LayerTables &layers = layer_tables();
    sp->physics.Init(kMaxBodies, 0, 4096, 4096, layers.bp_layers, *layers.bp_filter, layers.pairs);
    sp->physics.SetGravity(JPH::Vec3(0.0f, gravity, 0.0f));
    return sp;
}

RigidId rigid_add(RigidSpace &sp, const RigidSpec &spec, const RigidState &state, const World &world,
                  const NodeRegistry &reg)
{
    if (sp.boxes.size() * 2 + 2 > (size_t)kMaxBodies) return 0;
    Box b;
    b.spec      = spec;
    b.spec.half = Vec3{std::clamp(spec.half.x, 0.05f, 2.0f), std::clamp(spec.half.y, 0.05f, 2.0f),
                       std::clamp(spec.half.z, 0.05f, 2.0f)};
    b.spec.mass = std::clamp(spec.mass, 0.1f, 5000.0f);

    const Vec3 &h     = b.spec.half;
    const float round = std::min({0.03f, h.x * 0.5f, h.y * 0.5f, h.z * 0.5f});
    JPH::BodyCreationSettings body(new JPH::BoxShape(JPH::Vec3(h.x, h.y, h.z), round),
                                   JPH::RVec3(state.centre.x, state.centre.y, state.centre.z), quat_of(state.axis),
                                   JPH::EMotionType::Dynamic, Layers::kMoving);
    body.mOverrideMassProperties       = JPH::EOverrideMassProperties::CalculateInertia;
    body.mMassPropertiesOverride.mMass = b.spec.mass;
    body.mFriction                     = std::clamp(b.spec.friction, 0.0f, 2.0f);
    body.mRestitution                  = std::clamp(b.spec.bounce, 0.0f, 1.0f);
    body.mLinearDamping                = 0.02f;
    body.mAngularDamping               = 0.08f;
    // Thrown at speed it must not pass through a wall between two steps.
    body.mMotionQuality   = JPH::EMotionQuality::LinearCast;
    body.mLinearVelocity  = jv(state.velocity);
    body.mAngularVelocity = jv(state.spin);
    b.body = sp.physics.GetBodyInterface().CreateAndAddBody(body, JPH::EActivation::Activate);
    if (b.body.IsInvalid()) return 0;

    const RigidId id = sp.next++;
    Box          &in = sp.boxes.emplace(id, b).first->second;
    build_ground(sp, in, world, reg, state.centre);
    return id;
}

void rigid_remove(RigidSpace &sp, RigidId id)
{
    Box *b = find(sp, id);
    if (b == nullptr) return;
    JPH::BodyInterface &bodies = sp.physics.GetBodyInterface();
    for (JPH::BodyID one : {b->body, b->ground}) {
        if (one.IsInvalid()) continue;
        bodies.RemoveBody(one);
        bodies.DestroyBody(one);
    }
    sp.boxes.erase(id);
}

void rigid_step(RigidSpace &sp, const World &world, const NodeRegistry &reg, float dt)
{
    if (sp.boxes.empty()) return;
    JPH::BodyInterface &bodies = sp.physics.GetBodyInterface();
    for (auto &[id, b] : sp.boxes) {
        b.since_ground += std::max(dt, 0.0f);
        const Vec3 here = centre_of(sp, b);
        if (b.since_ground >= kRebuildEvery || length(here - b.ground_centre) > kRebuildAfter)
            build_ground(sp, b, world, reg, here);
    }

    sp.accumulator += std::max(dt, 0.0f);
    for (int n = 0; sp.accumulator >= kStep && n < kMaxRigidSteps; ++n) {
        sp.accumulator -= kStep;
        for (auto &[id, b] : sp.boxes) b.before = bodies.GetLinearVelocity(b.body);
        sp.physics.Update(kStep, 1, &sp.temp, &sp.jobs);
        // A knock is what the velocity did that gravity did not.
        const JPH::Vec3 fall(0.0f, sp.gravity * kStep, 0.0f);
        for (auto &[id, b] : sp.boxes)
            b.impact = std::max(b.impact, (bodies.GetLinearVelocity(b.body) - b.before - fall).Length());
    }
    sp.accumulator = std::min(sp.accumulator, kStep);
}

RigidState rigid_state(const RigidSpace &sp, RigidId id)
{
    RigidState s;
    const Box *b = find(sp, id);
    if (b == nullptr) return s;
    const JPH::BodyInterface &bodies = sp.physics.GetBodyInterface();
    s.centre        = centre_of(sp, *b);
    const JPH::Quat q = bodies.GetRotation(b->body);
    s.axis[0]  = cv(q * JPH::Vec3::sAxisX());
    s.axis[1]  = cv(q * JPH::Vec3::sAxisY());
    s.axis[2]  = cv(q * JPH::Vec3::sAxisZ());
    s.velocity = cv(bodies.GetLinearVelocity(b->body));
    s.spin     = cv(bodies.GetAngularVelocity(b->body));
    s.resting  = !bodies.IsActive(b->body);
    return s;
}

void rigid_set(RigidSpace &sp, RigidId id, const RigidState &s)
{
    Box *b = find(sp, id);
    if (b == nullptr) return;
    for (const Vec3 &v : {s.centre, s.velocity, s.spin, s.axis[0], s.axis[1], s.axis[2]})
        for (const float f : {v.x, v.y, v.z})
            if (!std::isfinite(f)) return;
    JPH::BodyInterface &bodies = sp.physics.GetBodyInterface();
    bodies.SetPositionAndRotation(b->body, JPH::RVec3(s.centre.x, s.centre.y, s.centre.z), quat_of(s.axis),
                                  JPH::EActivation::Activate);
    bodies.SetLinearAndAngularVelocity(b->body, jv(s.velocity), jv(s.spin));
    if (length(s.centre - b->ground_centre) > kRebuildAfter) b->since_ground = 1e9f;   // read again next step
}

void rigid_impulse(RigidSpace &sp, RigidId id, Vec3 dv, Vec3 at)
{
    Box *b = find(sp, id);
    if (b == nullptr) return;
    for (const Vec3 &v : {dv, at})
        for (const float f : {v.x, v.y, v.z})
            if (!std::isfinite(f)) return;
    JPH::BodyInterface &bodies = sp.physics.GetBodyInterface();
    bodies.AddImpulse(b->body, jv(dv) * b->spec.mass, JPH::RVec3(at.x, at.y, at.z));
    bodies.ActivateBody(b->body);
}

float rigid_impact(RigidSpace &sp, RigidId id)
{
    Box *b = find(sp, id);
    if (b == nullptr) return 0.0f;
    const float k = b->impact;
    b->impact     = 0.0f;
    return k;
}

RigidSpec rigid_spec(const RigidSpace &sp, RigidId id)
{
    const Box *b = find(sp, id);
    return b ? b->spec : RigidSpec{};
}

Vec3 rigid_extent(const RigidSpec &spec, const RigidState &s)
{
    const float h[3] = {spec.half.x, spec.half.y, spec.half.z};
    Vec3        e{};
    for (int k = 0; k < 3; ++k) {
        e.x += std::fabs(s.axis[k].x) * h[k];
        e.y += std::fabs(s.axis[k].y) * h[k];
        e.z += std::fabs(s.axis[k].z) * h[k];
    }
    return e;
}

void rigid_angles(const Vec3 axis[3], float &yaw, float &pitch, float &roll)
{
    // R = Ry(a) Rx(b) Rz(c), its columns the axes: R[1][2] = -sin b,
    // R[0][2] = sin a cos b, R[2][2] = cos a cos b, R[1][0] = cos b sin c,
    // R[1][1] = cos b cos c. And a = -yaw, b = pitch, c = -roll.
    const float sb = std::clamp(-axis[2].y, -1.0f, 1.0f);
    const float b  = std::asin(sb);
    float       a, c;
    if (std::sqrt(std::max(0.0f, 1.0f - sb * sb)) > 1e-4f) {
        a = std::atan2(axis[2].x, axis[2].z);
        c = std::atan2(axis[0].y, axis[1].y);
    } else {
        // Its z straight up or down: the yaw takes the whole turn.
        c = 0.0f;
        a = std::atan2(-axis[0].z, axis[0].x);
    }
    yaw   = -a;
    pitch = b;
    roll  = -c;
}

} // namespace cnp
