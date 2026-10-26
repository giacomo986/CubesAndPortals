#include "core/physics/craft.h"

#include "core/physics/vehicle.h"
#include "core/registry/node_shape.h"
#include "core/voxel/chunk.h"

#include <Jolt/Jolt.h>

#include <Jolt/Core/JobSystemSingleThreaded.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Body/BodyFilter.h>
#include <Jolt/Physics/Collision/BroadPhase/BroadPhaseLayerInterfaceTable.h>
#include <Jolt/Physics/Collision/BroadPhase/ObjectVsBroadPhaseLayerFilterTable.h>
#include <Jolt/Physics/Collision/CastResult.h>
#include <Jolt/Physics/Collision/NarrowPhaseQuery.h>
#include <Jolt/Physics/Collision/ObjectLayerPairFilterTable.h>
#include <Jolt/Physics/Collision/RayCast.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/ConvexHullShape.h>
#include <Jolt/Physics/Collision/Shape/OffsetCenterOfMassShape.h>
#include <Jolt/Physics/Collision/Shape/StaticCompoundShape.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/Physics/Vehicle/TrackedVehicleController.h>
#include <Jolt/Physics/Vehicle/VehicleCollisionTester.h>
#include <Jolt/Physics/Vehicle/VehicleConstraint.h>
#include <Jolt/Physics/Vehicle/WheeledVehicleController.h>

#include <algorithm>
#include <cmath>

namespace cnp {
namespace {

// Fixed steps, as the kart's: a spring is only the same spring at the same step.
constexpr float kStep     = 1.0f / 60.0f;
constexpr int   kMaxCraftSteps = 15;

constexpr float kMaxRPM        = 6000.0f;
constexpr float kMaxWheelSlope = 0.87f;   // rad: no wheel takes a wall as ground (vehicle.cpp)
constexpr float kMaxTilt       = 0.9f;    // rad: a jeep on a hillside, not on its roof

// The blocks round it, rebuilt as the kart's are (vehicle.cpp).
constexpr float kRebuildAfter = 2.0f;
constexpr float kRebuildEvery = 0.25f;

// How hard a Rotor or a Wing is turned to what its stick asks: the error in
// radians times this is its rate in radians a second.
constexpr float kAttitude = 3.0f;
constexpr float kRotorTip = 0.4f;         // rad of pitch or roll at full stick
constexpr float kRotorClimb = 6.0f;       // m/s up or down at full collective

namespace Layers {
constexpr JPH::ObjectLayer kStill  = 0;
constexpr JPH::ObjectLayer kMoving = 1;
} // namespace Layers

JPH::Vec3 jv(Vec3 v) { return JPH::Vec3(v.x, v.y, v.z); }
Vec3      cv(JPH::Vec3Arg v) { return Vec3{v.GetX(), v.GetY(), v.GetZ()}; }

// Yaw 0 faces -z; Jolt's vehicle drives along its +z (vehicle.cpp).
JPH::Quat upright(float yaw) { return JPH::Quat::sRotation(JPH::Vec3::sAxisY(), 3.14159265f - yaw); }

struct LayerTables {
    JPH::BroadPhaseLayerInterfaceTable bp_layers{2, 2};
    JPH::ObjectLayerPairFilterTable    pairs{2};
    std::unique_ptr<JPH::ObjectVsBroadPhaseLayerFilterTable> bp_filter;

    LayerTables()
    {
        bp_layers.MapObjectToBroadPhaseLayer(Layers::kStill, JPH::BroadPhaseLayer(0));
        bp_layers.MapObjectToBroadPhaseLayer(Layers::kMoving, JPH::BroadPhaseLayer(1));
        pairs.EnableCollision(Layers::kMoving, Layers::kStill);
        pairs.EnableCollision(Layers::kMoving, Layers::kMoving);
        bp_filter = std::make_unique<JPH::ObjectVsBroadPhaseLayerFilterTable>(bp_layers, 2, pairs, 2);
    }
};

const LayerTables &layer_tables()
{
    static const LayerTables tables;
    return tables;
}

} // namespace

struct Craft {
    CraftSpec spec;

    JPH::TempAllocatorImpl       temp{1 << 20};
    JPH::JobSystemSingleThreaded jobs{JPH::cMaxPhysicsJobs};
    JPH::PhysicsSystem           physics;

    JPH::BodyID                      hull;
    JPH::Ref<JPH::VehicleConstraint> constraint;   // Wheeled and Tracked
    JPH::BodyID                      ground;

    const World *ground_world = nullptr;
    Vec3         ground_centre;
    float        since_ground = 0.0f;

    float accumulator = 0.0f;
    float ride        = 0.0f;   // the base under the hull's middle, along its up
    float wheel_r     = 0.0f;
    float impact      = 0.0f;
    float throttle    = 0.0f;

    ~Craft()
    {
        JPH::BodyInterface &bodies = physics.GetBodyInterface();
        if (constraint) {
            physics.RemoveStepListener(constraint);
            physics.RemoveConstraint(constraint);
            constraint = nullptr;
        }
        for (JPH::BodyID id : {hull, ground}) {
            if (id.IsInvalid()) continue;
            bodies.RemoveBody(id);
            bodies.DestroyBody(id);
        }
    }
};

namespace {

// Every solid block within reach of `centre`, as boxes: rows of whole cubes as
// one box, a block with a shape as its own boxes (vehicle.cpp's build_ground
// without the track's roads, and wider: a tank is longer than a kart).
void build_ground(Craft &c, const World &world, const NodeRegistry &reg, Vec3 centre)
{
    JPH::BodyInterface &bodies = c.physics.GetBodyInterface();
    if (!c.ground.IsInvalid()) {
        bodies.RemoveBody(c.ground);
        bodies.DestroyBody(c.ground);
        c.ground = JPH::BodyID();
    }
    c.ground_world = &world;
    c.since_ground = 0.0f;

    const int cx = (int)std::floor(centre.x), cy = (int)std::floor(centre.y),
              cz = (int)std::floor(centre.z);
    c.ground_centre = Vec3{(float)cx, (float)cy, (float)cz};
    const int reach = (int)std::ceil(std::max(c.spec.half_l, c.spec.half_w)) + 6;
    const int down  = 5;
    const int up    = (int)std::ceil(2.0f * c.spec.half_h) + 4;

    JPH::StaticCompoundShapeSettings compound;
    int                              boxes = 0;
    const auto add = [&](float x0, float y0, float z0, float x1, float y1, float z1) {
        const JPH::Vec3 half(0.5f * (x1 - x0), 0.5f * (y1 - y0), 0.5f * (z1 - z0));
        const float     thin = std::min({half.GetX(), half.GetY(), half.GetZ()});
        if (thin <= 0.0f) return;
        const JPH::Vec3 mid(0.5f * (x0 + x1) - cx, 0.5f * (y0 + y1) - cy, 0.5f * (z0 + z1) - cz);
        compound.AddShape(mid, JPH::Quat::sIdentity(),
                          new JPH::BoxShape(half, std::min(JPH::cDefaultConvexRadius, thin)));
        ++boxes;
    };
    for (int y = cy - down; y <= cy + up; ++y)
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
    // A ramp in front of every step one block high with room over it: a
    // wedge from the step's foot to its top over up to three cells, as many
    // as lie level in front of it. Tracks and wheels roll up it, the front
    // rising first and the back after, where a wheel's cast would meet the
    // step's face as a wall; one cell long it was a 45-degree kick that took
    // most of a jeep's speed at every step of a climb. A wall two high has no
    // ramp: its first row has a block over it, not room.
    const auto solid_at = [&](int x, int y, int z) {
        const BlockId id = world.get_block(x, y, z);
        return reg.is_solid(id) && reg.get(id).turn != NodeTurn::Height;
    };
    const int dirs[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
    for (int y = cy - down + 1; y <= cy + up; ++y)
        for (int z = cz - reach; z <= cz + reach; ++z)
            for (int x = cx - reach; x <= cx + reach; ++x) {
                // (x, y, z) is the lower cell: air on solid ground.
                if (solid_at(x, y, z) || !solid_at(x, y - 1, z)) continue;
                for (const auto &d : dirs) {
                    const int ax = x + d[0], az = z + d[1];
                    if (!solid_at(ax, y, az) || solid_at(ax, y + 1, az) || solid_at(ax, y + 2, az)) continue;
                    // How far it runs back from the step: over the level
                    // ground there, up to three cells.
                    int len = 1;
                    while (len < 3) {
                        const int tx = x - d[0] * len, tz = z - d[1] * len;
                        if (solid_at(tx, y, tz) || !solid_at(tx, y - 1, tz)) break;
                        ++len;
                    }
                    const int lx = x - d[0] * (len - 1), lz = z - d[1] * (len - 1);   // the toe's cell
                    // The face against the step is at the step's side of the
                    // first cell, the ramp's toe at the far side of the last.
                    JPH::Array<JPH::Vec3> pts;
                    for (const float w : {0.0f, 1.0f}) {
                        const float along0 = d[0] != 0 ? (d[0] > 0 ? 1.0f : 0.0f) : w;
                        const float side0  = d[1] != 0 ? (d[1] > 0 ? 1.0f : 0.0f) : w;
                        const float toex   = d[0] != 0 ? (d[0] > 0 ? 0.0f : 1.0f) : w;
                        const float toez   = d[1] != 0 ? (d[1] > 0 ? 0.0f : 1.0f) : w;
                        const float fx = x + along0 - cx, fz = z + side0 - cz;
                        pts.push_back(JPH::Vec3(fx, (float)(y - cy), fz));
                        pts.push_back(JPH::Vec3(fx, (float)(y + 1 - cy), fz));
                        pts.push_back(JPH::Vec3(lx + toex - cx, (float)(y - cy), lz + toez - cz));
                    }
                    JPH::ConvexHullShapeSettings wedge(pts, 0.01f);
                    compound.AddShape(JPH::Vec3::sZero(), JPH::Quat::sIdentity(), wedge.Create().Get());
                    ++boxes;
                }
            }

    if (boxes == 0) return;
    JPH::ShapeSettings::ShapeResult shape = compound.Create();
    if (shape.HasError()) return;
    JPH::BodyCreationSettings settings(shape.Get(), JPH::RVec3(cx, cy, cz), JPH::Quat::sIdentity(),
                                       JPH::EMotionType::Static, Layers::kStill);
    settings.mFriction = 1.0f;
    c.ground = bodies.CreateAndAddBody(settings, JPH::EActivation::DontActivate);
}

// One gear whose top revs are the top speed, and the torque that pushes the
// mass at `accel` through it (vehicle.cpp).
float gear_ratio(const CraftSpec &s, float wheel_r)
{
    return (kMaxRPM * 2.0f * 3.14159265f / 60.0f) * wheel_r / std::max(s.top, 1.0f);
}

void make_wheeled(Craft &c)
{
    const CraftSpec &s = c.spec;
    c.wheel_r          = std::clamp(s.half_h * 0.9f, 0.3f, 0.55f);
    const float smin = 0.1f, smax = 0.45f;   // c.ride, set by craft_make, is made of these

    JPH::VehicleConstraintSettings car;
    car.mMaxPitchRollAngle = kMaxTilt;
    const float inset     = c.wheel_r + 0.15f;
    const float wheelbase = std::max(2.0f * (s.half_l - inset), 0.5f);
    const float lock      = std::atan(s.turn * wheelbase / 4.0f);
    for (int i = 0; i < 4; ++i) {
        auto *w                         = new JPH::WheelSettingsWV;
        w->mPosition                    = JPH::Vec3(i % 2 == 0 ? s.half_w - 0.15f : -(s.half_w - 0.15f), -s.half_h,
                                                    i < 2 ? s.half_l - inset : -(s.half_l - inset));
        w->mSuspensionMinLength         = smin;
        w->mSuspensionMaxLength         = smax;
        w->mSuspensionSpring.mFrequency = 2.0f;
        w->mSuspensionSpring.mDamping   = 0.5f;
        w->mRadius                      = c.wheel_r;
        w->mWidth                       = 0.3f;
        w->mMaxSteerAngle               = i < 2 ? lock : 0.0f;
        w->mMaxBrakeTorque              = s.mass * s.accel * 1.5f * c.wheel_r / 4.0f;
        w->mMaxHandBrakeTorque          = 0.0f;
        car.mWheels.push_back(w);
    }
    auto       *drive = new JPH::WheeledVehicleControllerSettings;
    const float ratio = gear_ratio(s, c.wheel_r);
    drive->mEngine.mMaxRPM          = kMaxRPM;
    drive->mEngine.mInertia         = 0.05f;
    drive->mEngine.mMaxTorque       = s.mass * s.accel * c.wheel_r / ratio;
    drive->mTransmission.mGearRatios        = {1.0f};
    drive->mTransmission.mReverseGearRatios = {-1.0f};
    drive->mDifferentials.resize(2);   // all four driven, half the engine to each axle
    for (int a = 0; a < 2; ++a) {
        drive->mDifferentials[a].mLeftWheel         = 2 * a;
        drive->mDifferentials[a].mRightWheel        = 2 * a + 1;
        drive->mDifferentials[a].mDifferentialRatio = ratio;
        drive->mDifferentials[a].mEngineTorqueRatio = 0.5f;
    }
    car.mController = drive;
    c.constraint    = new JPH::VehicleConstraint(*c.physics.GetBodyLockInterfaceNoLock().TryGetBody(c.hull), car);
}

void make_tracked(Craft &c)
{
    const CraftSpec &s = c.spec;
    c.wheel_r          = 0.35f;
    const float smin = 0.15f, smax = 0.35f;   // and these

    JPH::VehicleConstraintSettings tank;
    tank.mMaxPitchRollAngle = kMaxTilt;
    auto       *drive = new JPH::TrackedVehicleControllerSettings;
    const float ratio = gear_ratio(s, c.wheel_r);
    drive->mEngine.mMaxRPM                  = kMaxRPM;
    drive->mEngine.mMaxTorque               = s.mass * s.accel * c.wheel_r / ratio;
    drive->mTransmission.mGearRatios        = {1.0f};
    drive->mTransmission.mReverseGearRatios = {-1.0f};
    constexpr int kWheels = 6;
    for (int t = 0; t < 2; ++t) {   // Jolt's left, +x, first
        JPH::VehicleTrackSettings &track = drive->mTracks[t];
        for (int k = 0; k < kWheels; ++k) {
            auto *w = new JPH::WheelSettingsTV;
            const float z = (s.half_l - c.wheel_r) * (1.0f - 2.0f * (float)k / (kWheels - 1));
            const bool  end = k == 0 || k == kWheels - 1;
            w->mPosition            = JPH::Vec3(t == 0 ? s.half_w - 0.2f : -(s.half_w - 0.2f),
                                                -s.half_h + (end ? 0.15f : 0.0f), z);
            w->mRadius              = c.wheel_r;
            w->mWidth               = 0.4f;
            w->mSuspensionMinLength = smin;
            w->mSuspensionMaxLength = end ? smin : smax;
            w->mSuspensionSpring.mFrequency = 1.5f;
            track.mWheels.push_back((JPH::uint)tank.mWheels.size());
            tank.mWheels.push_back(w);
        }
        track.mDrivenWheel       = track.mWheels.back();
        track.mDifferentialRatio = ratio;
        track.mMaxBrakeTorque    = s.mass * s.accel * 2.0f * c.wheel_r;
    }
    tank.mController = drive;
    c.constraint     = new JPH::VehicleConstraint(*c.physics.GetBodyLockInterfaceNoLock().TryGetBody(c.hull), tank);
}

JPH::Quat rotation(const Craft &c) { return c.physics.GetBodyInterface().GetRotation(c.hull); }

void attitude(const Craft &c, float &yaw, float &pitch, float &roll)
{
    const JPH::Quat q    = rotation(c);
    const JPH::Vec3 fwd  = q * JPH::Vec3::sAxisZ();
    const JPH::Vec3 left = q * JPH::Vec3::sAxisX();
    yaw   = std::atan2(fwd.GetX(), -fwd.GetZ());
    pitch = std::asin(std::clamp(fwd.GetY(), -1.0f, 1.0f));
    roll  = std::atan2(left.GetY(), (q * JPH::Vec3::sAxisY()).GetY());
}

bool hull_down(const Craft &c)
{
    if (c.constraint) {
        for (const JPH::Wheel *w : c.constraint->GetWheels())
            if (w->HasContact()) return true;
        return false;
    }
    if (c.ground.IsInvalid()) return false;
    struct Only : JPH::BodyFilter {
        JPH::BodyID ground;
        bool ShouldCollide(const JPH::BodyID &id) const override { return id == ground; }
    } only;
    only.ground = c.ground;
    const JPH::BodyInterface &bodies = c.physics.GetBodyInterface();
    const JPH::RVec3 from = bodies.GetPosition(c.hull);
    JPH::RayCastResult hit;
    return c.physics.GetNarrowPhaseQuery().CastRay(
        JPH::RRayCast{from, JPH::Vec3(0.0f, -(c.spec.half_h + 0.3f), 0.0f)}, hit, {}, {}, only);
}

// The wheel and the throttle of a machine on wheels or tracks: forward against
// the way it rolls is the brake until it has stopped, and only then a gear the
// other way (vehicle.cpp).
void drive_ground(Craft &c, const CraftKeys &keys, float along)
{
    float forward = keys.forward, brake = 0.0f;
    if ((forward < 0.0f && along > 0.5f) || (forward > 0.0f && along < -0.5f)) {
        brake   = std::fabs(forward);
        forward = 0.0f;
    }
    if (c.spec.kind == CraftKind::Wheeled) {
        auto *drive = static_cast<JPH::WheeledVehicleController *>(c.constraint->GetController());
        drive->SetDriverInput(forward, keys.right, brake, 0.0f);
        return;
    }
    // Tracks: the side to turn to runs slower, and from a standstill the other
    // way, which turns it on the spot.
    float left = 1.0f, right = 1.0f;
    const float r = std::clamp(keys.right, -1.0f, 1.0f);
    if (std::fabs(r) > 0.05f) {
        if (forward == 0.0f && brake == 0.0f && std::fabs(along) < 1.0f) {
            forward = 1.0f;
            (r > 0.0f ? right : left) = -1.0f;
        } else {
            (r > 0.0f ? right : left) = std::max(0.1f, 1.0f - std::fabs(r) * 0.8f);
        }
    }
    auto *drive = static_cast<JPH::TrackedVehicleController *>(c.constraint->GetController());
    drive->SetDriverInput(forward, left, right, brake);
}

void step_once(Craft &c, const World &world, const NodeRegistry &reg, const CraftKeys &keys)
{
    JPH::BodyInterface &bodies = c.physics.GetBodyInterface();
    const CraftSpec    &s      = c.spec;
    const JPH::Quat     q      = bodies.GetRotation(c.hull);
    const JPH::Vec3     fwd    = q * JPH::Vec3::sAxisZ();
    const JPH::Vec3     upv    = q * JPH::Vec3::sAxisY();
    const JPH::Vec3     vel    = bodies.GetLinearVelocity(c.hull);
    const float         along  = vel.Dot(fwd);
    const float         g      = -s.gravity;

    c.since_ground += kStep;
    const Vec3 at  = cv(JPH::Vec3(bodies.GetPosition(c.hull)));
    const Vec3 off = at - c.ground_centre;
    if (c.ground_world != &world || c.since_ground >= kRebuildEvery ||
        std::max({std::fabs(off.x), std::fabs(off.y), std::fabs(off.z)}) > kRebuildAfter)
        build_ground(c, world, reg, at);

    float yaw, pitch, roll;
    attitude(c, yaw, pitch, roll);

    switch (s.kind) {
    case CraftKind::Wheeled:
    case CraftKind::Tracked:
        drive_ground(c, keys, along);
        // Coasting: the speed fades with nothing pressed.
        if (keys.forward == 0.0f) bodies.AddForce(c.hull, fwd * (-s.mass * 1.5f * along));
        // Up a step's ramp (build_ground) the engine alone would stall: the
        // pull of its weight down the slope is taken away while it is driven
        // up it, so it climbs at its own pace, nose first, the tail after.
        if (keys.forward != 0.0f && hull_down(c)) {
            const float uphill = fwd.GetY() * (keys.forward > 0.0f ? 1.0f : -1.0f);
            if (uphill > 0.02f) bodies.AddForce(c.hull, fwd * (s.mass * g * fwd.GetY()));
        }
        break;

    case CraftKind::Rotor: {
        // The collective asks for a rate of climb, kRotorClimb at full, and
        // the rotor pushes along its up for it: hover at 0.
        const float want = kRotorClimb * std::clamp(keys.up, -1.0f, 1.0f);
        const float lift = s.mass * std::clamp(g + (want - vel.GetY()) * 2.0f, 0.0f, 2.0f * g) /
                           std::max(upv.GetY(), 0.5f);
        bodies.AddForce(c.hull, upv * lift);
        // The air: a helicopter left alone comes to rest.
        bodies.AddForce(c.hull, -vel * (s.mass * s.accel / std::max(s.top, 1.0f)));
        // Tipped towards where it should go, turned at the rate asked.
        const float want_pitch = -kRotorTip * std::clamp(keys.forward, -1.0f, 1.0f);
        const float want_roll  = kRotorTip * std::clamp(keys.right, -1.0f, 1.0f);
        const JPH::Vec3 local(-(want_pitch - pitch) * kAttitude, 0.0f, (want_roll - roll) * kAttitude);
        bodies.SetAngularVelocity(c.hull, q * local + JPH::Vec3(0.0f, -keys.yaw * s.turn, 0.0f));
        break;
    }

    case CraftKind::Wing: {
        c.throttle = std::clamp(c.throttle + keys.up * kStep * 0.5f, 0.0f, 1.0f);
        const float stall = std::max(s.top * 0.45f, 1.0f);
        const float k     = std::clamp(along / stall, 0.0f, 1.0f);   // how much the air answers
        bodies.AddForce(c.hull, fwd * (c.throttle * s.accel * s.mass));
        bodies.AddForce(c.hull, upv * (s.mass * g * std::min(1.4f, (along / stall) * (along / stall))));
        // Drag that makes `top` the speed of a full throttle.
        const float speed = vel.Length();
        if (speed > 0.01f) bodies.AddForce(c.hull, -vel * (s.mass * s.accel * speed / (s.top * s.top)));
        // The stick turns it as fast as the air lets it, and the air turns
        // what it is doing towards where its nose is.
        const bool  down = hull_down(c);
        JPH::Vec3   local(std::clamp(keys.forward, -1.0f, 1.0f) * s.turn * k,
                          -(std::clamp(keys.yaw, -1.0f, 1.0f) * s.turn * (down ? 1.0f : 0.4f)),
                          std::clamp(keys.right, -1.0f, 1.0f) * s.turn * 1.5f * k);
        if (down && along < stall) local.SetZ(-roll * kAttitude);   // on its wheels: level
        bodies.SetAngularVelocity(c.hull, q * local);
        if (k > 0.0f && speed > 0.01f) {
            const JPH::Vec3 aligned = fwd * speed;
            bodies.SetLinearVelocity(c.hull, vel + (aligned - vel) * std::min(1.0f, 2.0f * kStep * k));
        }
        break;
    }
    }

    const JPH::Vec3 before = bodies.GetLinearVelocity(c.hull);
    c.physics.Update(kStep, 1, &c.temp, &c.jobs);
    JPH::Vec3 after = bodies.GetLinearVelocity(c.hull);

    // A knock: what changed beyond what gravity did.
    const JPH::Vec3 knock = after - before - JPH::Vec3(0.0f, s.gravity * kStep, 0.0f);
    c.impact              = std::max(c.impact, knock.Length());

    // No faster than its top forwards, and back at half of it; a rotor's top
    // is across the ground whichever way.
    if (s.kind == CraftKind::Rotor) {
        const float flat = std::sqrt(after.GetX() * after.GetX() + after.GetZ() * after.GetZ());
        if (flat > s.top) {
            after.SetX(after.GetX() * s.top / flat);
            after.SetZ(after.GetZ() * s.top / flat);
            bodies.SetLinearVelocity(c.hull, after);
        }
    }
    if (s.kind == CraftKind::Wheeled || s.kind == CraftKind::Tracked) {
        const float now  = after.Dot(bodies.GetRotation(c.hull) * JPH::Vec3::sAxisZ());
        const float cap  = now < 0.0f ? s.top * 0.5f : s.top;
        const float flat = std::sqrt(after.GetX() * after.GetX() + after.GetZ() * after.GetZ());
        if (flat > cap && flat > 0.0f) {
            after.SetX(after.GetX() * cap / flat);
            after.SetZ(after.GetZ() * cap / flat);
            bodies.SetLinearVelocity(c.hull, after);
        }
    }
}

} // namespace

std::shared_ptr<Craft> craft_make(const CraftSpec &spec, Vec3 base, float yaw, const World &world,
                                  const NodeRegistry &reg)
{
    jolt_init();
    auto c  = std::make_shared<Craft>();
    c->spec = spec;
    c->spec.half_w = std::clamp(spec.half_w, 0.2f, 6.0f);
    c->spec.half_h = std::clamp(spec.half_h, 0.2f, 4.0f);
    c->spec.half_l = std::clamp(spec.half_l, 0.2f, 10.0f);
    c->spec.mass   = std::clamp(spec.mass, 10.0f, 50000.0f);
    const CraftSpec &s = c->spec;

    const LayerTables &layers = layer_tables();
    c->physics.Init(64, 0, 1024, 1024, layers.bp_layers, *layers.bp_filter, layers.pairs);
    c->physics.SetGravity(JPH::Vec3(0.0f, s.gravity, 0.0f));

    const bool on_wheels = s.kind == CraftKind::Wheeled || s.kind == CraftKind::Tracked;
    c->ride = s.half_h;   // a hull on its belly; the wheels' makers raise it
    if (s.kind == CraftKind::Wheeled) c->ride = s.half_h + 0.45f - 0.05f + std::clamp(s.half_h * 0.9f, 0.3f, 0.55f);
    if (s.kind == CraftKind::Tracked) c->ride = s.half_h + 0.35f - 0.05f + 0.35f;

    // The hull, its weight low so it leans rather than rolls.
    JPH::RefConst<JPH::Shape> box =
        JPH::OffsetCenterOfMassShapeSettings(JPH::Vec3(0.0f, on_wheels ? -s.half_h : -0.3f * s.half_h, 0.0f),
                                             new JPH::BoxShape(JPH::Vec3(s.half_w, s.half_h, s.half_l)))
            .Create()
            .Get();
    const Vec3 centre = base + Vec3{0.0f, c->ride, 0.0f};
    JPH::BodyCreationSettings body(box, JPH::RVec3(centre.x, centre.y, centre.z), upright(yaw),
                                   JPH::EMotionType::Dynamic, Layers::kMoving);
    body.mOverrideMassProperties       = JPH::EOverrideMassProperties::CalculateInertia;
    body.mMassPropertiesOverride.mMass = s.mass;
    body.mAllowSleeping                = false;
    body.mFriction                     = on_wheels ? 0.1f : (s.kind == CraftKind::Wing ? 0.15f : 0.6f);
    body.mAngularDamping               = on_wheels ? 0.05f : 2.0f;
    c->hull = c->physics.GetBodyInterface().CreateAndAddBody(body, JPH::EActivation::Activate);

    if (s.kind == CraftKind::Wheeled) make_wheeled(*c);
    if (s.kind == CraftKind::Tracked) make_tracked(*c);
    if (c->constraint) {
        c->constraint->SetVehicleCollisionTester(
            new JPH::VehicleCollisionTesterCastSphere(Layers::kMoving, c->wheel_r, JPH::Vec3::sAxisY(), kMaxWheelSlope));
        c->physics.AddConstraint(c->constraint);
        c->physics.AddStepListener(c->constraint);
    }
    build_ground(*c, world, reg, base);
    return c;
}

void craft_step(Craft &c, const World &world, const NodeRegistry &reg, const CraftKeys &keys, float dt)
{
    CraftKeys k = keys;
    for (float *f : {&k.forward, &k.right, &k.up, &k.yaw})
        *f = std::isfinite(*f) ? std::clamp(*f, -1.0f, 1.0f) : 0.0f;
    c.accumulator += std::max(dt, 0.0f);
    for (int n = 0; c.accumulator >= kStep && n < kMaxCraftSteps; ++n) {
        c.accumulator -= kStep;
        step_once(c, world, reg, k);
    }
    c.accumulator = std::min(c.accumulator, kStep);
}

CraftPose craft_pose(Craft &c)
{
    const JPH::BodyInterface &bodies = c.physics.GetBodyInterface();
    const JPH::Quat           q      = bodies.GetRotation(c.hull);
    const JPH::RVec3          at     = bodies.GetPosition(c.hull);
    const JPH::Vec3           vel    = bodies.GetLinearVelocity(c.hull);

    CraftPose p;
    attitude(c, p.yaw, p.pitch, p.roll);
    p.base     = Vec3{(float)at.GetX(), (float)at.GetY(), (float)at.GetZ()} - cv(q * JPH::Vec3::sAxisY()) * c.ride;
    p.velocity = cv(vel);
    p.speed    = vel.Dot(q * JPH::Vec3::sAxisZ());
    p.grounded = hull_down(c);
    p.impact   = c.impact;
    p.throttle = c.throttle;
    c.impact   = 0.0f;
    return p;
}

void craft_push(Craft &c, Vec3 dv)
{
    for (const float f : {dv.x, dv.y, dv.z})
        if (!std::isfinite(f)) return;
    JPH::BodyInterface &bodies = c.physics.GetBodyInterface();
    bodies.SetLinearVelocity(c.hull, bodies.GetLinearVelocity(c.hull) + jv(dv));
}

void craft_place(Craft &c, const CraftPose &p)
{
    for (const float f : {p.base.x, p.base.y, p.base.z, p.yaw, p.pitch, p.roll, p.velocity.x, p.velocity.y,
                          p.velocity.z})
        if (!std::isfinite(f)) return;
    // attitude()'s yaw, pitch nose up and roll right side down, as Jolt turns.
    const JPH::Quat q = upright(p.yaw) * JPH::Quat::sRotation(JPH::Vec3::sAxisX(), -p.pitch) *
                        JPH::Quat::sRotation(JPH::Vec3::sAxisZ(), p.roll);
    const Vec3 centre = p.base + cv(q * JPH::Vec3::sAxisY()) * c.ride;
    JPH::BodyInterface &bodies = c.physics.GetBodyInterface();
    bodies.SetPositionAndRotation(c.hull, JPH::RVec3(centre.x, centre.y, centre.z), q, JPH::EActivation::Activate);
    bodies.SetLinearVelocity(c.hull, jv(p.velocity));
}

const CraftSpec &craft_spec(const Craft &c) { return c.spec; }
float            craft_ride(const Craft &c) { return c.ride; }

} // namespace cnp
