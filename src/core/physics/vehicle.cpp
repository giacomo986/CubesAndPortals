#include "core/physics/vehicle.h"

#include "core/registry/node_shape.h"
#include "core/track/piece.h"

#include <Jolt/Jolt.h>

#include <Jolt/Core/Factory.h>
#include <Jolt/Core/JobSystemSingleThreaded.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Body/BodyFilter.h>
#include <Jolt/Physics/Collision/BroadPhase/BroadPhaseLayerInterfaceTable.h>
#include <Jolt/Physics/Collision/CastResult.h>
#include <Jolt/Physics/Collision/NarrowPhaseQuery.h>
#include <Jolt/Physics/Collision/RayCast.h>
#include <Jolt/Physics/Collision/BroadPhase/ObjectVsBroadPhaseLayerFilterTable.h>
#include <Jolt/Physics/Collision/ObjectLayerPairFilterTable.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/MeshShape.h>
#include <Jolt/Physics/Collision/Shape/OffsetCenterOfMassShape.h>
#include <Jolt/Physics/Collision/Shape/StaticCompoundShape.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/Physics/Vehicle/VehicleCollisionTester.h>
#include <Jolt/Physics/Vehicle/VehicleConstraint.h>
#include <Jolt/Physics/Vehicle/WheeledVehicleController.h>
#include <Jolt/RegisterTypes.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <memory>
#include <map>
#include <mutex>

namespace cnp {
namespace {

// The shape of a kart, in nodes and kilograms. Chosen and then tuned by
// driving, not derived: the world's numbers (core.drive) say how fast it goes,
// brakes and turns, and these say what it is -- a low box on four small wheels.
constexpr float kMass        = 200.0f;
constexpr float kHalfHeight  = 0.25f;   // of the chassis box
constexpr float kWheelRadius = 0.3f;
constexpr float kWheelWidth  = 0.2f;
constexpr float kWheelInset  = 0.35f;   // from the nose and the tail to an axle
constexpr float kSuspMin     = 0.05f;   // from the chassis's bottom, fully up
constexpr float kSuspMax     = 0.3f;    // and fully down
constexpr float kSuspHz      = 3.0f;
constexpr float kSuspDamping = 0.5f;
// How far from upright a chassis may be tipped, in radians (Jolt keeps its up
// inside a cone this wide). It was 1.0, and at 57 degrees a kart that touched a
// wall at speed rolled onto that side until its wheels found the wall's face as
// ground and DROVE UP IT -- "mentre corro e tocco il muro di lato, il kart
// prima si inclina e poi scala il muro", reported from play with a frame of it
// on the wall (2026-09-20). A kart leans a few degrees in a bend and pitches a
// few under braking; 26 is far past that, and far short of what lies a chassis
// against a wall. It also caps how far the server's upright box and the tilted
// chassis can disagree about what the body is touching.
constexpr float kMaxTilt     = 0.45f;   // rad of pitch or roll, about 26 degrees

// And the steepest surface a WHEEL will take as ground. A wall is 90 degrees
// and is now no more ground to a kart than the sky is: without this a wheel
// cast into a wall came back with a contact the vehicle happily drove along.
constexpr float kMaxWheelSlope = 0.87f;   // rad, about 50 degrees

// How a wall ride holds a kart (the step): not at all below kWallSlow m/s,
// with kWallGrip of its weight from kWallFast.
constexpr float kWallSlow = 10.0f;
constexpr float kWallFast = 16.0f;
constexpr float kWallGrip = 1.5f;
constexpr float kWallFrom = 0.6f;   // rad, about 34 degrees: where it starts to hold
// And how much of its weight's pull along the road is taken away: not all of
// it. With none left a kart that came onto a wall nose up drove on up it and
// over the top; what is left bends its line back down.
constexpr float kWallLift = 0.75f;
// A step a kart hops onto by itself, driving into it: its top more than
// kStepLow and at most kStepHigh above the feet, with kStepRoom free over it.
// Under kStepLow the wheels roll up it; a track's walls are two high and stay
// walls.
constexpr float kStepLow  = 0.2f;
constexpr float kStepHigh = 1.05f;
constexpr float kStepRoom = 1.5f;
// Seen this far ahead in time: about how long a hop takes to rise a node, so
// a kart that comes at a step fast is over it as it gets there, not against it.
constexpr float kStepLead = 0.2f;
constexpr float kMaxRPM      = 6000.0f;
constexpr float kEngineInertia = 0.02f;   // kg m^2
constexpr float kWheelInertia  = 0.2f;    // kg m^2, a 5 kg wheel of radius 0.3 is 0.225

// Where the chassis's centre sits above the feet with the kart at rest on flat
// ground: the chassis's half height, the suspension a little compressed, and a
// wheel. What a chassis put back on the ground is placed at.
constexpr float kRideHeight = kHalfHeight + kSuspMax - 0.05f + kWheelRadius;

// Which is what the server allows a jump beyond its top (movement.h): the feet
// go from the wheels to the chassis's bottom when the wheels leave the ground.
static_assert(kRideHeight - kHalfHeight - kChassisLift < 1e-5f &&
                  kChassisLift - (kRideHeight - kHalfHeight) < 1e-5f,
              "kChassisLift is the chassis's bottom above its wheels at rest");

// What is left of an upward speed that meets the ceiling (MovementLimits::rise):
// it comes back down as off something solid, not stuck to it.
constexpr float kCeilingBounce = 0.5f;

// The simulation runs in fixed steps, whatever the frame rate: a spring and a
// tyre are only the same spring and tyre at the same step, and the server that
// will one day run this for itself has to take the same steps as the client.
constexpr float kStep     = 1.0f / 60.0f;

// The drift: ctrl held at more than kDriftMin m/s, and the rear tyres hold
// sideways only kDriftGrip of what they did, while the wheel turns kDriftTurn
// times as far as it does at that speed -- the tail swings out and the kart
// slides round the bend, leaving its marks (kSlideSeen).
constexpr float kDriftMin  = 5.0f;
constexpr float kDriftGrip = 0.4f;
constexpr float kDriftTurn = 1.6f;
// And it turns about its up at most kDriftSpin rad/s while drifting: with the
// tail loose and the throttle down a kart with no countersteer went on round
// (3.6 rad in two seconds at half the wheel, physics_box_test), a spin and not
// a drift.
constexpr float kDriftSpin = 1.4f;
// The nitro: a full gauge burns in kNitroBurn seconds of shift, pushing with
// kNitroPush times the engine's acceleration, and fills again in kNitroFill
// seconds of not pressing it. Past the top speed it lets the kart go up to
// kNitroTop times it (movement.h); let go, the cap comes back down at
// kNitroFade m/s^2, so the kart is not stopped dead at the old top.
constexpr float kNitroBurn = 3.0f;
constexpr float kNitroFill = 20.0f;
constexpr float kNitroPush = 1.5f;
constexpr float kNitroFade = 6.0f;
// The most steps one frame catches up: a quarter of a second, so a kart keeps
// to real time down to 4 frames a second; a slower frame loses the rest. At
// 4 (66 ms), with the frame cut to 50 ms before it (player.cpp), a kart in a
// slow frame lost time: on the loop's west straight, where the client draws
// the Inferno through two openings at 50-85 ms a frame in Xephyr, 11 s instead
// of 5, the lap 28 instead of 21 (since 649f9f0, which made those openings).
constexpr int   kMaxStepsPerUpdate = 15;

// The blocks around the chassis, as boxes in a compound shape of their own.
// Built again once the chassis has gone kRebuildAfter nodes from where the last
// one was centred, and every kRebuildEvery seconds for the blocks that changed.
// ponytail: the whole neighbourhood rebuilt, about a thousand cells; a voxel
// Shape of Jolt's own that reads the world directly if F6 ever shows it.
constexpr int   kReach        = 8;   // nodes out from the centre, and ...
constexpr int   kReachDown    = 4;   // ... below it, and ...
constexpr int   kReachUp      = 4;   // ... above it
constexpr float kRebuildAfter = 2.0f;
constexpr float kRebuildEvery = 0.25f;

namespace Layers {
constexpr JPH::ObjectLayer kStill  = 0;
constexpr JPH::ObjectLayer kMoving = 1;
} // namespace Layers

JPH::Vec3 jv(Vec3 v) { return JPH::Vec3(v.x, v.y, v.z); }
Vec3      cv(JPH::Vec3Arg v) { return Vec3{v.GetX(), v.GetY(), v.GetZ()}; }

// This engine's yaw is 0 looking down -Z; Jolt's vehicle drives along its own
// +Z. A turn of pi - yaw about up takes the one onto the other.
JPH::Quat upright(float yaw) { return JPH::Quat::sRotation(JPH::Vec3::sAxisY(), 3.14159265f - yaw); }

void jolt_once()
{
    static std::once_flag once;
    std::call_once(once, [] {
        JPH::RegisterDefaultAllocator();
        JPH::Factory::sInstance = new JPH::Factory();
        JPH::RegisterTypes();
    });
}

// Which layers meet: a moving body meets the blocks and another moving body.
// One table for every vehicle, built in this order because the broad phase
// filter reads the other two when it is constructed, not when it is asked.
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

// What a step leaves behind, for the frame to be drawn between two of them.
struct Pose {
    Vec3  centre;
    Vec3  feet;
    Vec3  base;   // under the centre along the chassis's up, by the ride height
    float yaw = 0.0f, pitch = 0.0f, roll = 0.0f;
};

} // namespace

struct Vehicle {
    MovementRules rules;   // what the chassis below was built from

    JPH::TempAllocatorImpl                 temp{1 << 20};
    JPH::JobSystemSingleThreaded           jobs{JPH::cMaxPhysicsJobs};
    JPH::PhysicsSystem                     physics;

    JPH::BodyID                   chassis;
    JPH::Ref<JPH::VehicleConstraint> constraint;
    JPH::BodyID                   ground;   // the blocks around it; invalid when there are none
    JPH::BodyID                   road;     // the track pieces' road around it, a body of its own

    const World *ground_world = nullptr;
    Vec3         ground_centre;
    float        since_ground = 0.0f;

    // Replaying a client's record: the chunks it built without, and which it
    // may have. And what the last build left out, for the record.
    const DriveEvent *replay   = nullptr;
    const MayMiss    *may_miss = nullptr;
    bool              built    = false;
    uint32_t          built_missing = 0;

    float accumulator = 0.0f;
    Pose  was, now;          // the last two steps
    Vec3  wrote_feet;        // what the last update handed the player
    float wrote_yaw = 0.0f;
    float stood     = 0.0f;  // the feet the last time a wheel touched, as the server's ground_y

    // What the road under the wheels lets the chassis do: tip as far from
    // upright as the road is, and leave it as fast upwards as it was climbing.
    // kMaxTilt and a jump on the flat; kept while in the air.
    float tilt = kMaxTilt;
    float lift = 0.0f;
    float cap  = 0.0f;   // the top speed forwards now: past `sprint` after a nitro
    // What the last step looked like (vehicle_look): the nitro burning, and
    // how fast the chassis went sideways with a wheel down.
    bool  boosting = false;
    float slip     = 0.0f;
    bool  drifting = false;   // this step's rear tyres let go (kDriftGrip)

    ~Vehicle()
    {
        JPH::BodyInterface &bodies = physics.GetBodyInterface();
        physics.RemoveStepListener(constraint);
        physics.RemoveConstraint(constraint);
        constraint = nullptr;   // before the chassis it holds on to
        for (JPH::BodyID id : {chassis, ground, road}) {
            if (id.IsInvalid()) continue;
            bodies.RemoveBody(id);
            bodies.DestroyBody(id);
        }
    }
};

namespace {

// What a wheel takes as ground: a block within kMaxWheelSlope of UP, which
// is what keeps a kart off a wall of blocks -- and a track piece's road
// within it of the chassis's own up, which is how a wall ride is driven at
// all; the nearer of the two. The road was asked against UP first: where it
// passed 50 degrees under a wheel, in a loop, that cast skipped the steep
// facet for a flatter one behind it, the spring stretched for a frame and the
// kart stumbled going up and coming down (reported from play, 2026-10-13).
class WheelTester : public JPH::VehicleCollisionTester {
public:
    WheelTester(const JPH::BodyID *road)
        : m_road(road),
          m_up(new JPH::VehicleCollisionTesterCastSphere(Layers::kMoving, kWheelRadius,
                                                         JPH::Vec3::sAxisY(), kMaxWheelSlope))
    {
    }

    bool Collide(JPH::PhysicsSystem &physics, const JPH::VehicleConstraint &constraint,
                 JPH::uint wheel, JPH::RVec3Arg origin, JPH::Vec3Arg direction,
                 const JPH::BodyID &self, JPH::Body *&body, JPH::SubShapeID &sub,
                 JPH::RVec3 &at, JPH::Vec3 &normal, float &length) const override
    {
        struct Filter : JPH::BodyFilter {
            JPH::BodyID self, road;
            bool        only_road;
            bool ShouldCollide(const JPH::BodyID &b) const override
            {
                return only_road ? b == road : b != self && b != road;
            }
        } blocks, road;
        blocks.self = road.self = self;
        blocks.road = road.road = *m_road;
        blocks.only_road = false;
        road.only_road   = true;

        JPH::VehicleCollisionTesterCastSphere up(Layers::kMoving, kWheelRadius, JPH::Vec3::sAxisY(),
                                                 kMaxWheelSlope);
        up.SetBodyFilter(&blocks);
        const bool on_block = up.Collide(physics, constraint, wheel, origin, direction, self, body, sub,
                                         at, normal, length);
        if (m_road->IsInvalid()) return on_block;

        JPH::Body          *rbody;
        JPH::SubShapeID     rsub;
        JPH::RVec3          rat;
        JPH::Vec3           rnormal;
        float               rlength;
        JPH::VehicleCollisionTesterCastSphere along(Layers::kMoving, kWheelRadius, -direction,
                                                    kMaxWheelSlope);
        along.SetBodyFilter(&road);
        if (!along.Collide(physics, constraint, wheel, origin, direction, self, rbody, rsub, rat,
                           rnormal, rlength) ||
            (on_block && length <= rlength))
            return on_block;
        body = rbody, sub = rsub, at = rat, normal = rnormal, length = rlength;
        return true;
    }

    void PredictContactProperties(JPH::PhysicsSystem &physics,
                                  const JPH::VehicleConstraint &constraint, JPH::uint wheel,
                                  JPH::RVec3Arg origin, JPH::Vec3Arg direction,
                                  const JPH::BodyID &self, JPH::Body *&body, JPH::SubShapeID &sub,
                                  JPH::RVec3 &at, JPH::Vec3 &normal, float &length) const override
    {
        m_up->PredictContactProperties(physics, constraint, wheel, origin, direction, self, body,
                                       sub, at, normal, length);
    }

private:
    const JPH::BodyID                            *m_road;
    JPH::Ref<JPH::VehicleCollisionTesterCastSphere> m_up;
};

// How far behind the reach a piece's corner may stand and its road still
// reach into it: the largest footprint, less a node.
constexpr int kPieceReach = kPieceMaxUnits * kPieceUnit - 1;

// A piece's road and curbs, made once for each shape and turn and shared by
// every chassis after: the same triangles on both ends, from the same numbers.
const std::vector<PieceTri> &piece_road(const PieceShape &p, uint8_t facing)
{
    static std::mutex                                           lock;
    static std::map<std::array<int, 11>, std::vector<PieceTri>> made;
    const std::array<int, 11> key{p.plan, p.size, p.climb, p.grade0, p.grade1,
                                  p.bank0, p.bank1, p.curbs, p.wall, p.width, facing};
    std::lock_guard<std::mutex> hold(lock);
    auto                        it = made.find(key);
    if (it == made.end()) {
        std::vector<PieceTri> tris;
        piece_triangles(p, facing, tris);
        tris.erase(std::remove_if(tris.begin(), tris.end(),
                                  [](const PieceTri &t) { return t.role == kPieceSide; }),
                   tris.end());   // drawn, not driven on
        it = made.emplace(key, std::move(tris)).first;
    }
    return it->second;   // a map's element stays where it is
}

// Every solid block within reach of `centre`, as boxes: a row of whole cubes
// along x as one box, and a block with a shape as its own boxes -- the ones
// node_shape_box gives the resolver and the renderer, so what a wheel rolls on
// is what a foot stands on.
void build_ground(Vehicle &v, const World &world, const NodeRegistry &reg, Vec3 centre)
{
    JPH::BodyInterface &bodies = v.physics.GetBodyInterface();
    if (!v.road.IsInvalid()) {
        bodies.RemoveBody(v.road);
        bodies.DestroyBody(v.road);
        v.road = JPH::BodyID();
    }
    if (!v.ground.IsInvalid()) {
        bodies.RemoveBody(v.ground);
        bodies.DestroyBody(v.ground);
        v.ground = JPH::BodyID();
    }
    v.ground_world  = &world;
    v.since_ground  = 0.0f;

    const int cx = (int)std::floor(centre.x), cy = (int)std::floor(centre.y),
              cz = (int)std::floor(centre.z);
    v.ground_centre = Vec3{(float)cx, (float)cy, (float)cz};

    // Which chunks are left out: on a client the ones it has not been sent,
    // which its world reads as air anyway; replaying one, the ones it said,
    // where it may not have had them (DriveEvent::missing). The reach, and the
    // corners of the track pieces whose road may reach into it (kPieceReach),
    // span three chunks a side at most, two up: a bit each.
    static_assert(2 * kReach + kPieceReach + 1 <= 2 * kChunkSize &&
                      kReachDown + kPieceMaxHeight + kReachUp + kPieceBelow + 1 <= kChunkSize,
                  "the blocks around a chassis span at most three chunks across, two up");
    const ChunkPos base = World::chunk_of(cx - kReach - kPieceReach, cy - kReachDown - kPieceMaxHeight,
                                          cz - kReach - kPieceReach);
    const ChunkPos last = World::chunk_of(cx + kReach, cy + kReachUp + kPieceBelow, cz + kReach);
    const auto     bit  = [&](ChunkPos c) {
        return 1u << ((c.x - base.x) + 3 * (c.y - base.y) + 6 * (c.z - base.z));
    };
    uint32_t missing = 0;
    for (int z = base.z; z <= last.z; ++z)
        for (int y = base.y; y <= last.y; ++y)
            for (int x = base.x; x <= last.x; ++x) {
                const ChunkPos c{x, y, z};
                const bool     gone = v.replay ? (v.replay->missing & bit(c)) &&
                                                     (!v.may_miss || (*v.may_miss)(c))
                                               : world.find_chunk(c) == nullptr;
                if (gone) missing |= bit(c);
            }
    v.built         = true;
    v.built_missing = missing;

    JPH::StaticCompoundShapeSettings compound;
    int                              boxes = 0;
    const auto add = [&](float x0, float y0, float z0, float x1, float y1, float z1) {
        const JPH::Vec3 half(0.5f * (x1 - x0), 0.5f * (y1 - y0), 0.5f * (z1 - z0));
        const float     thin = std::min({half.GetX(), half.GetY(), half.GetZ()});
        if (thin <= 0.0f) return;
        // Positions relative to the centre, where a float is still exact.
        const JPH::Vec3 mid(0.5f * (x0 + x1) - cx, 0.5f * (y0 + y1) - cy, 0.5f * (z0 + z1) - cz);
        compound.AddShape(mid, JPH::Quat::sIdentity(),
                          new JPH::BoxShape(half, std::min(JPH::cDefaultConvexRadius, thin)));
        ++boxes;
    };

    for (int y = cy - kReachDown; y <= cy + kReachUp; ++y)
        for (int z = cz - kReach; z <= cz + kReach; ++z) {
            int run = 0;   // whole cubes waiting to be one box, ending at x - 1
            for (int x = cx - kReach; x <= cx + kReach + 1; ++x) {
                const bool     inside = x <= cx + kReach &&
                                        !(missing & bit(World::chunk_of(x, y, z)));
                const BlockId  id     = inside ? world.get_block(x, y, z) : kAir;
                const NodeDef *def    = inside && reg.is_solid(id) ? &reg.get(id) : nullptr;
                // A track piece's cells are what a foot stands on; a wheel
                // rolls on its road, added below.
                const bool     solid  = def && def->turn != NodeTurn::Height && def->piece.plan == 0;
                if (!solid) def = nullptr;
                const bool     cube   = solid && !node_has_shape(*def);

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

    // The track pieces whose road reaches into the reach, by the corner they
    // stand on: on the grid, never more than kPieceReach behind it. All of
    // them ONE mesh, their corners snapped to a 1024th of a node: where two
    // pieces meet the edge is then shared by triangles of both, and Jolt sees
    // a road going on rather than the end of one and a step onto the next --
    // as separate meshes a kart crossing the join was thrown up by it
    // (reported from play: "compenetra la pista e scatta").
    JPH::TriangleList road;
    const auto        snap = [](float f) { return std::round(f * 1024.0f) / 1024.0f; };
    const auto grid = [](int n) { return (int)std::floor((float)n / (float)kPieceUnit) * kPieceUnit; };
    for (int y = cy - kReachDown - kPieceMaxHeight; y <= cy + kReachUp + kPieceBelow; ++y)
        for (int z = grid(cz - kReach - kPieceReach); z <= cz + kReach; z += kPieceUnit)
            for (int x = grid(cx - kReach - kPieceReach); x <= cx + kReach; x += kPieceUnit) {
                if (missing & bit(World::chunk_of(x, y, z))) continue;
                const NodeDef &def = reg.get(world.get_block(x, y, z));
                if (def.piece.plan == 0) continue;
                const uint8_t facing = world.get_facing(x, y, z);
                int           nx, nz;
                piece_size(def.piece, facing, nx, nz);
                if (x + nx < cx - kReach || z + nz < cz - kReach) continue;
                const Vec3 at{(float)(x - cx), (float)(y - cy), (float)(z - cz)};
                for (const PieceTri &t : piece_road(def.piece, facing)) {
                    JPH::Float3 c[3];
                    for (int i = 0; i < 3; ++i)
                        c[i] = JPH::Float3(snap(t.v[i].x + at.x), snap(t.v[i].y + at.y),
                                           snap(t.v[i].z + at.z));
                    road.push_back(JPH::Triangle(c[0], c[1], c[2]));
                }
            }
    // A body of its own and not a part of the blocks': a wheel takes a road
    // as steep as a wall ride's as ground, and a block's wall never
    // (WheelTester).
    if (!road.empty()) {
        JPH::ShapeSettings::ShapeResult r = JPH::MeshShapeSettings(road).Create();
        if (!r.HasError()) {
            JPH::BodyCreationSettings settings(r.Get(), JPH::RVec3(cx, cy, cz), JPH::Quat::sIdentity(),
                                               JPH::EMotionType::Static, Layers::kStill);
            settings.mFriction = 1.0f;   // the blocks' road, below
            v.road = bodies.CreateAndAddBody(settings, JPH::EActivation::DontActivate);
        }
    }

    if (boxes == 0) return;   // nothing to drive on, and a compound of nothing is refused

    JPH::ShapeSettings::ShapeResult shape = compound.Create();
    if (shape.HasError()) return;
    JPH::BodyCreationSettings settings(shape.Get(), JPH::RVec3(cx, cy, cz), JPH::Quat::sIdentity(),
                                       JPH::EMotionType::Static, Layers::kStill);
    // A road, not ice: Jolt's default of 0.2 combined with the tyre's own
    // friction (sqrt of the two) left a kart spinning its wheels at 4.8 m/s
    // after a second of throttle that should have been 12.
    settings.mFriction = 1.0f;
    v.ground = bodies.CreateAndAddBody(settings, JPH::EActivation::DontActivate);
}

// The kart the world's numbers describe (core.drive, core.body_width), put
// down at the player: upright, facing where they face, at their velocity.
std::shared_ptr<Vehicle> make_vehicle(const Player &player, const World &world,
                                      const NodeRegistry &reg, const MovementRules &rules,
                                      const DriveEvent *record, const MayMiss *may_miss)
{
    jolt_once();

    auto v   = std::make_shared<Vehicle>();
    v->rules = rules;

    const LayerTables &layers = layer_tables();
    v->physics.Init(64, 0, 1024, 1024, layers.bp_layers, *layers.bp_filter, layers.pairs);
    v->physics.SetGravity(JPH::Vec3(0.0f, rules.gravity, 0.0f));

    const float half_w = 0.5f * rules.width;
    const float half_l = 0.5f * (rules.length > 0.0f ? rules.length : rules.width);

    // The chassis, its weight low so a bend leans it rather than rolls it.
    JPH::RefConst<JPH::Shape> box =
        JPH::OffsetCenterOfMassShapeSettings(JPH::Vec3(0.0f, -kHalfHeight, 0.0f),
                                             new JPH::BoxShape(JPH::Vec3(half_w, kHalfHeight, half_l)))
            .Create()
            .Get();
    const Vec3 centre = player.pos + Vec3{0.0f, kRideHeight, 0.0f};
    JPH::BodyCreationSettings body(box, JPH::RVec3(centre.x, centre.y, centre.z), upright(player.yaw),
                                   JPH::EMotionType::Dynamic, Layers::kMoving);
    body.mOverrideMassProperties       = JPH::EOverrideMassProperties::CalculateInertia;
    body.mMassPropertiesOverride.mMass = kMass;
    body.mAllowSleeping                = false;
    // A bumper, not a tyre: the chassis slides on whatever it touches. With
    // Jolt's 0.2 a kart that met a wall anywhere within 50 degrees of head on
    // squared up against it and stayed, the throttle pressing its corner into
    // the wall whatever the wheel said -- "girando mi sono buggato in un muro".
    // At 0 it scrapes along the wall from 40 degrees on (a flat floor and a
    // wall, 2 s of throttle into it then 4 s of throttle and full lock: 24 m/s
    // along it, or 14 to 19 steering away). Nearer head on than that the nose
    // is flat on the wall and turning presses the other corner in, as it would
    // any car: reverse.
    body.mFriction                     = 0.0f;
    body.mLinearVelocity               = jv(player.velocity);
    v->chassis = v->physics.GetBodyInterface().CreateAndAddBody(body, JPH::EActivation::Activate);

    // Four wheels at the corners, the front two steering. Jolt's vehicle has
    // its left at +x.
    JPH::VehicleConstraintSettings car;
    car.mMaxPitchRollAngle = kMaxTilt;

    // The wheel's angle at full lock: the one that turns the body at `turn`
    // at 4 m/s, where drive_turn_rate first gives all of it.
    const float wheelbase = 2.0f * (half_l - kWheelInset);
    const float lock      = std::atan(rules.turn * std::max(wheelbase, 0.1f) / 4.0f);

    // Brakes that stop it at `brake` if the tyres can take it.
    const float brake_torque = kMass * rules.brake * kWheelRadius / 4.0f;

    const float xs[4] = {half_w - kWheelWidth * 0.5f, -(half_w - kWheelWidth * 0.5f),
                         half_w - kWheelWidth * 0.5f, -(half_w - kWheelWidth * 0.5f)};
    const float zs[4] = {half_l - kWheelInset, half_l - kWheelInset, -(half_l - kWheelInset),
                         -(half_l - kWheelInset)};
    for (int i = 0; i < 4; ++i) {
        auto *w                          = new JPH::WheelSettingsWV;
        w->mPosition                     = JPH::Vec3(xs[i], -kHalfHeight, zs[i]);
        w->mSuspensionMinLength          = kSuspMin;
        w->mSuspensionMaxLength          = kSuspMax;
        w->mSuspensionSpring.mFrequency  = kSuspHz;
        w->mSuspensionSpring.mDamping    = kSuspDamping;
        w->mRadius                       = kWheelRadius;
        w->mWidth                        = kWheelWidth;
        w->mMaxSteerAngle                = i < 2 ? lock : 0.0f;
        w->mMaxBrakeTorque               = brake_torque;
        w->mMaxHandBrakeTorque           = 0.0f;
        w->mInertia                      = kWheelInertia;
        car.mWheels.push_back(w);
    }

    // One gear, its ratio making the engine's top revs the top speed: the
    // engine cannot turn past them, so the kart cannot go past `sprint`. And
    // the torque that pushes the kart at `accel`. Reverse is the same gear
    // backwards; its top speed, `walk`, is held by the clamp in the step.
    auto *drive                          = new JPH::WheeledVehicleControllerSettings;
    const float top                      = std::max(rules.sprint, 1.0f);
    const float ratio                    = (kMaxRPM * 2.0f * 3.14159265f / 60.0f) * kWheelRadius / top;
    drive->mEngine.mMaxRPM               = kMaxRPM;
    // A kart's engine and wheels are light. Jolt's defaults are a car's, and
    // through a ratio this long the engine's half a kg m^2 weighed as much as
    // 340 kg more kart: 2.4 m/s^2 of the 12 asked for.
    drive->mEngine.mInertia              = kEngineInertia;
    drive->mEngine.mMaxTorque            = kMass * rules.accel * kWheelRadius / ratio;
    drive->mTransmission.mGearRatios     = {1.0f};
    drive->mTransmission.mReverseGearRatios = {-1.0f};
    drive->mDifferentials.resize(1);
    drive->mDifferentials[0].mLeftWheel         = 2;   // rear wheel drive
    drive->mDifferentials[0].mRightWheel        = 3;
    drive->mDifferentials[0].mDifferentialRatio = ratio;
    car.mController = drive;

    v->constraint = new JPH::VehicleConstraint(
        *v->physics.GetBodyLockInterfaceNoLock().TryGetBody(v->chassis), car);
    // A sphere the size of a wheel rather than a cylinder: the cylinder tester
    // takes any surface as ground, and this one refuses anything steeper than
    // kMaxWheelSlope, which is what stops a kart driving up a wall.
    v->constraint->SetVehicleCollisionTester(new WheelTester(&v->road));
    v->physics.AddConstraint(v->constraint);
    v->physics.AddStepListener(v->constraint);
    // The tyres' grip, Jolt's own, the rear ones' sideways cut while drifting.
    Vehicle *self = v.get();
    static_cast<JPH::WheeledVehicleController *>(v->constraint->GetController())
        ->SetTireMaxImpulseCallback([self](JPH::uint wheel, float &along, float &across, float suspension,
                                           float grip_along, float grip_across, float, float, float) {
            along  = grip_along * suspension;
            across = grip_across * suspension * (self->drifting && wheel >= 2 ? kDriftGrip : 1.0f);
        });

    v->lift     = rules.jump;
    v->replay   = record;
    v->may_miss = may_miss;
    build_ground(*v, world, reg, player.pos);
    v->replay   = nullptr;
    v->may_miss = nullptr;
    return v;
}

// Where things are after a step, as the rest of the engine reads them.
Pose pose_of(const Vehicle &v)
{
    const JPH::BodyInterface &bodies = v.physics.GetBodyInterface();
    const JPH::RVec3          c      = bodies.GetPosition(v.chassis);
    const JPH::Quat           q      = bodies.GetRotation(v.chassis);
    const JPH::Vec3           fwd    = q * JPH::Vec3::sAxisZ();
    const JPH::Vec3           left   = q * JPH::Vec3::sAxisX();

    Pose p;
    p.centre = Vec3{(float)c.GetX(), (float)c.GetY(), (float)c.GetZ()};
    p.base   = p.centre - cv(q * JPH::Vec3::sAxisY()) * kRideHeight;
    p.yaw    = std::atan2(fwd.GetX(), -fwd.GetZ());
    p.pitch  = std::asin(std::clamp(fwd.GetY(), -1.0f, 1.0f));
    // All the way round and not only to a side: upside down at the top of a
    // loop, a roll read off the left alone came back level.
    p.roll   = std::atan2(left.GetY(), (q * JPH::Vec3::sAxisY()).GetY());

    // The feet are where the server's box for this body stands: on the highest
    // point a wheel touches, so the box never starts inside the road and the
    // server finds the road under it; and with no wheel down, at the bottom of
    // the chassis, which nothing solid is inside.
    float feet_y = -1e30f;
    for (const JPH::Wheel *w : v.constraint->GetWheels())
        if (w->HasContact()) feet_y = std::max(feet_y, (float)w->GetContactPosition().GetY());
    if (feet_y < -1e29f) feet_y = p.centre.y - kHalfHeight;
    p.feet = Vec3{p.centre.x, feet_y + kBoxSkin, p.centre.z};
    return p;
}

bool any_wheel_down(const Vehicle &v)
{
    for (const JPH::Wheel *w : v.constraint->GetWheels())
        if (w->HasContact()) return true;
    return false;
}

float lerp_angle(float a, float b, float t)
{
    float d = b - a;
    while (d > 3.14159265f) d -= 6.2831853f;
    while (d < -3.14159265f) d += 6.2831853f;
    return a + d * t;
}

// How high the step in front of the kart is, 0 for none: rays against what
// the wheels drive on, so the server's copy finds the same step. Forward at
// the wheels' height from the middle and both sides, down onto what they hit
// from kStepRoom over the highest step taken; a top too high, or something
// in the way above it, is a wall and no step.
float step_ahead(const Vehicle &v, float half_w, float half_l, float reach)
{
    const JPH::BodyInterface &bodies = v.physics.GetBodyInterface();
    const JPH::Quat           q      = bodies.GetRotation(v.chassis);
    JPH::Vec3                 fwd    = q * JPH::Vec3::sAxisZ();
    fwd.SetY(0.0f);
    if (fwd.LengthSq() < 1e-4f) return 0.0f;
    fwd = fwd.Normalized();
    const JPH::Vec3 side(fwd.GetZ(), 0.0f, -fwd.GetX());
    const Pose      p = pose_of(v);
    const float     feet = p.feet.y - kBoxSkin;

    // The blocks only: a piece's road climbs by itself, and a ramp is no step.
    struct Blocks : JPH::BodyFilter {
        JPH::BodyID ground;
        bool ShouldCollide(const JPH::BodyID &id) const override { return id == ground; }
    } not_me;
    not_me.ground = v.ground;
    if (v.ground.IsInvalid()) return 0.0f;
    const JPH::NarrowPhaseQuery &query = v.physics.GetNarrowPhaseQuery();
    float step = 0.0f;
    for (const float s : {-(half_w - 0.1f), 0.0f, half_w - 0.1f}) {
        const JPH::RVec3 from(p.centre.x + side.GetX() * s, feet + 0.1f, p.centre.z + side.GetZ() * s);
        JPH::RayCastResult hit;
        if (!query.CastRay(JPH::RRayCast{from, fwd * (half_l + reach)}, hit, {}, {}, not_me))
            continue;
        const JPH::RVec3 at   = from + fwd * ((half_l + reach) * hit.mFraction + 0.05f);
        const float      over = feet + kStepHigh + kStepRoom;
        JPH::RayCastResult top;
        const JPH::RVec3   high(at.GetX(), over, at.GetZ());
        if (!query.CastRay(JPH::RRayCast{high, JPH::Vec3(0.0f, -(kStepHigh + kStepRoom), 0.0f)}, top,
                           {}, {}, not_me))
            continue;
        const float rise = (kStepHigh + kStepRoom) * (1.0f - top.mFraction);
        if (rise > kStepHigh) return 0.0f;
        step = std::max(step, rise);
    }
    return step > kStepLow ? step : 0.0f;
}

void step(Vehicle &v, const World &world, const NodeRegistry &reg, const MovementRules &rules,
          const DriveKeys &input, float &nitro)
{
    JPH::BodyInterface &bodies = v.physics.GetBodyInterface();
    const JPH::Quat     q      = bodies.GetRotation(v.chassis);
    const JPH::Vec3     fwd    = q * JPH::Vec3::sAxisZ();
    JPH::Vec3           vel    = bodies.GetLinearVelocity(v.chassis);
    const float         along  = vel.Dot(fwd);
    const bool          down   = any_wheel_down(v);

    // The keys. Forward against the way it is rolling is the brake until it
    // has stopped, and only then a gear the other way -- which is what S did
    // before the kart had wheels.
    float forward = input.forward, brake = 0.0f;
    if ((forward < 0.0f && along > 0.5f) || (forward > 0.0f && along < -0.5f)) {
        brake   = std::fabs(forward);
        forward = 0.0f;
    }

    // The wheel asks for the turn drive_turn_rate allows at this speed, and the
    // tyres decide whether the kart follows it or slides: the angle that turns
    // a body rolling at v at that rate on a wheelbase L is atan(rate L / v).
    const float half_l    = 0.5f * (rules.length > 0.0f ? rules.length : rules.width);
    const float wheelbase = std::max(2.0f * (half_l - kWheelInset), 0.1f);
    const float lock      = std::atan(rules.turn * wheelbase / 4.0f);
    const float rolling   = std::max(std::fabs(along), 4.0f);
    const float angle     = std::atan(drive_turn_rate(rules, rolling) * wheelbase / rolling);
    v.drifting            = input.drift && down && along > kDriftMin;
    const float right     = lock > 0.0f ? input.right * std::min(1.0f, (v.drifting ? kDriftTurn : 1.0f) * angle / lock) : 0.0f;

    auto *drive = static_cast<JPH::WheeledVehicleController *>(v.constraint->GetController());
    drive->SetDriverInput(forward, right, brake, 0.0f);
    if (v.drifting) {
        const JPH::Vec3 up   = q * JPH::Vec3::sAxisY();
        const JPH::Vec3 spin = bodies.GetAngularVelocity(v.chassis);
        const float     yaw  = spin.Dot(up);
        if (std::fabs(yaw) > kDriftSpin)
            bodies.SetAngularVelocity(v.chassis, spin - up * (yaw - std::copysign(kDriftSpin, yaw)));
    }

    // Coasting on the road: the speed fades at `drag` with nothing pressed.
    if (down && input.forward == 0.0f && rules.drag > 0.0f)
        bodies.AddForce(v.chassis, fwd * (-kMass * rules.drag * along));

    if (input.jump && down) vel.SetY(std::max(vel.GetY(), rules.jump));

    // Driving into a step: a hop just over it, as the jump key would, so a
    // kart goes up the country's one-block terraces (from the paddock it
    // stopped against the first). Only near level (kMaxTilt): coming down a
    // loop the rays went out flat past the road into the hill beside the
    // track, found its top a "step", and the hop put the kart's fall of 23
    // m/s at 4.5 up -- it lost all its way (reported from play, 2026-10-13).
    if (down && input.forward > 0.0f && along > -0.5f && vel.GetY() < 1.0f && rules.jump > 0.0f &&
        (q * JPH::Vec3::sAxisY()).GetY() > std::cos(kMaxTilt)) {
        const float h = step_ahead(v, 0.5f * rules.width, half_l, 0.15f + std::max(along, 0.0f) * kStepLead);
        if (h > 0.0f)
            vel.SetY(std::max(vel.GetY(), std::min(rules.jump, std::sqrt(-2.0f * rules.gravity * (h + 0.15f)))));
    }
    bodies.SetLinearVelocity(v.chassis, vel);

    // The nitro: shift with the throttle down, while there is some left and
    // the kart may move at all. It pushes only with a wheel down, and burns
    // either way; the cap goes up with it and comes back down after.
    const bool boost = input.boost && input.forward > 0.0f && nitro > 0.0f && rules.sprint > 0.0f;
    if (boost) {
        nitro = std::max(0.0f, nitro - kStep / kNitroBurn);
        if (down) bodies.AddForce(v.chassis, fwd * (kMass * rules.accel * kNitroPush));
        v.cap = rules.sprint * kNitroTop;
    } else {
        nitro = std::min(1.0f, nitro + kStep / kNitroFill);
        v.cap = std::max(rules.sprint, v.cap - kNitroFade * kStep);
    }
    v.boosting = boost;
    v.slip     = down ? std::fabs(vel.Dot(q * JPH::Vec3::sAxisX())) : 0.0f;

    // The blocks around it, again when it has left them behind or they may
    // have changed.
    v.since_ground += kStep;
    const Vec3 at = cv(JPH::Vec3(bodies.GetPosition(v.chassis)));
    const Vec3 off{at.x - v.ground_centre.x, at.y - v.ground_centre.y, at.z - v.ground_centre.z};
    if (v.ground_world != &world || v.since_ground >= kRebuildEvery ||
        std::max({std::fabs(off.x), std::fabs(off.y), std::fabs(off.z)}) > kRebuildAfter)
        build_ground(v, world, reg, at);

    // How steep the road under the wheels is. On a banked turn or a slope the
    // chassis may tip that much further from upright, and leave the road as
    // fast upwards as the road was lifting it -- off a ramp, a jump. Off the
    // road the cone closes back to kMaxTilt, never faster than a radian in a
    // second and a bit; and a wall is never road (kMaxWheelSlope), so it never
    // opens the cone for a chassis against one.
    //
    // The cone opens to the STEEPEST wheel's road: into a bank the rear wheels
    // are still flatter than the front, and a cone held to them kept the
    // chassis more upright than the road it was on -- its corner went into the
    // road and was thrown out of it, 0.39 deep. The lift is the LEAST steep
    // wheel's: one wheel on the rounded edge between two blocks of a flat road
    // is not a ramp, four on one are. Flat is exactly the numbers there were
    // before any of this.
    {
        float     least = 1e30f, most = 0.0f;
        int       got   = 0;
        JPH::Vec3 facing = JPH::Vec3::sZero();
        for (const JPH::Wheel *w : v.constraint->GetWheels())
            if (w->HasContact()) {
                const float a = std::acos(std::clamp(w->GetContactNormal().GetY(), -1.0f, 1.0f));
                least = std::min(least, a);
                most  = std::max(most, a);
                facing += w->GetContactNormal();
                ++got;
            }

        // A wall ride: on a road steeper than a wheel takes from a block, the
        // kart is held to it by how fast it goes -- none under kWallSlow, all
        // of it from kWallFast. Held is pressed into the road by kWallGrip of
        // its weight, and most of the pull of its weight ALONG the road taken
        // away (kWallLift): pressed alone it slid down a wall a node in three
        // squares, and off the low edge coming out of it. Coming in from kWallFrom, all of it
        // from kMaxWheelSlope, so a bank of 27 degrees is what it was. Slow
        // down on a wall and you come off it.
        if (got > 0 && most > kWallFrom && facing.Length() > 0.0f) {
            const float speed = std::sqrt(vel.GetX() * vel.GetX() + vel.GetY() * vel.GetY() +
                                          vel.GetZ() * vel.GetZ());
            const float held  = std::clamp((speed - kWallSlow) / (kWallFast - kWallSlow), 0.0f, 1.0f) *
                                std::clamp((most - kWallFrom) / (kMaxWheelSlope - kWallFrom), 0.0f, 1.0f);
            const JPH::Vec3 n     = facing.Normalized();
            const JPH::Vec3 g     = JPH::Vec3(0.0f, rules.gravity, 0.0f);
            const JPH::Vec3 along = g - n * g.Dot(n);
            bodies.AddForce(v.chassis,
                            (n * (rules.gravity * kWallGrip) - along * kWallLift) * (kMass * held));
        }
        if (got == 0 || least < 0.01f) least = 0.0f;
        if (got == 0 || most < 0.01f) most = 0.0f;

        const float was    = v.tilt;
        const float closes = kMaxTilt + (got > 0 ? most : 0.0f);
        // Up to upside down, which is no cone at all: in a loop the road is
        // over the kart (Jolt's pi turns it off; past it the cosine comes back).
        v.tilt = std::min(std::max(closes, v.tilt - 2.0f * kStep * (kMaxWheelSlope - kMaxTilt)), JPH::JPH_PI);
        if (got > 0)
            v.lift = rules.jump + std::sqrt(vel.GetX() * vel.GetX() + vel.GetZ() * vel.GetZ()) *
                                      std::tan(std::min(least, kMaxWheelSlope));
        // On a road no block is as steep as -- a wall ride, a loop -- the kart
        // may go up as fast as it goes: climbing a loop's side straight up it
        // was held to a jump's 8 m/s and fell off it.
        if (got > 0 && most > kMaxWheelSlope)
            v.lift = std::max(v.lift, rules.jump + vel.Length());
        if (v.tilt != was) v.constraint->SetMaxPitchRollAngle(v.tilt);
    }

    const JPH::RVec3 before = bodies.GetPosition(v.chassis);
    v.physics.Update(kStep, 1, &v.temp, &v.jobs);

    // Held (a countdown took every speed away with core.body_movement): where
    // it was, exactly. Springs settling moved a held kart by tenths of a
    // millimetre, which a server allowing nothing corrected 28 times a start.
    if (rules.sprint <= 0.0f && rules.walk <= 0.0f) {
        JPH::RVec3 at = bodies.GetPosition(v.chassis);
        at.SetX(before.GetX());
        at.SetZ(before.GetZ());
        bodies.SetPosition(v.chassis, at, JPH::EActivation::Activate);
        bodies.SetAngularVelocity(v.chassis, JPH::Vec3::sZero());
    }

    // Within the numbers whatever happened: the top speed forwards and back,
    // which is what the server allows -- and a body whose speeds were taken
    // away (a countdown holding it with core.body_movement) stops. And never
    // falling faster than anything else in the engine.
    vel               = bodies.GetLinearVelocity(v.chassis);
    const float now   = vel.Dot(bodies.GetRotation(v.chassis) * JPH::Vec3::sAxisZ());
    const float cap   = now < 0.0f ? rules.walk : std::min(v.cap, rules.sprint * kNitroTop);
    const float flat  = std::sqrt(vel.GetX() * vel.GetX() + vel.GetZ() * vel.GetZ());
    if (flat > cap) {
        const float k = cap > 0.0f ? cap / flat : 0.0f;
        vel.SetX(vel.GetX() * k);
        vel.SetZ(vel.GetZ() * k);
    }
    if (vel.GetY() < -kTerminalFallSpeed) vel.SetY(-kTerminalFallSpeed);

    // And never thrown UP harder than a jump, which is the same sentence: a
    // body that cannot fly may be a jump above where it last stood (v^2 / 2g,
    // 1.28 on the track) and the server refuses anything higher. A crash into a
    // wall put a kart 1.87 up with nothing under it, and from there every claim
    // it sent was refused -- 200 corrections, the kart stuck in the wall
    // (reported from play, 2026-09-20). A jump the keys asked for is set to
    // exactly this before the step, so it is not what gets clipped; what does
    // is a solver throwing a chassis off what it hit.
    if (vel.GetY() > v.lift) vel.SetY(v.lift);

    // And never higher than the server lets a body that cannot fly be above
    // where it last stood: off the ground, the chassis meets that height as a
    // ceiling and bounces off it. Past it every claim was refused and the kart
    // was put back where it had been.
    float rise = limits_for(rules).rise;
    if (rise >= 0.0f && v.lift > rules.jump)
        rise += (v.lift * v.lift - rules.jump * rules.jump) / (-2.0f * rules.gravity);
    if (any_wheel_down(v)) {
        v.stood = pose_of(v).feet.y;
    } else if (rise >= 0.0f) {
        JPH::RVec3  at   = bodies.GetPosition(v.chassis);
        const float over = (float)at.GetY() - kHalfHeight - (v.stood + rise);
        if (over > 0.0f) {
            at.SetY(at.GetY() - over);
            bodies.SetPosition(v.chassis, at, JPH::EActivation::Activate);
            if (vel.GetY() > 0.0f) vel.SetY(-kCeilingBounce * vel.GetY());
        }
    }

    bodies.SetLinearVelocity(v.chassis, vel);
}

} // namespace

DriveKeys drive_keys(const PlayerInput &input)
{
    const auto q = [](float x) { return std::round(std::clamp(x, -1.0f, 1.0f) * 127.0f) / 127.0f; };
    return DriveKeys{q(input.move_forward), q(input.move_right), input.jump, input.sprint, input.descend};
}

float vehicle_step_seconds() { return kStep; }

DriveLook vehicle_look(const Player &player)
{
    DriveLook out;
    if (player.vehicle) {
        out.boosting = player.vehicle->boosting;
        out.sliding  = player.vehicle->slip > kSlideSeen;
    }
    return out;
}

void jolt_init() { jolt_once(); }

namespace {

// A put, from the player as it stands or from a record of one.
void put(Player &player, const World &world, const NodeRegistry &reg, const MovementRules &rules,
         const DriveEvent *record, const MayMiss *may_miss)
{
    player.vehicle.reset();   // the old chassis's Jolt world goes before the new one is made
    player.vehicle = make_vehicle(player, world, reg, rules, record, may_miss);
    Vehicle &v     = *player.vehicle;
    v.was = v.now = pose_of(v);
    // No wheel has touched anything before the first step, and the feet
    // would be the chassis's bottom, half a node up for one frame.
    v.was.feet = v.now.feet = player.pos;
    v.wrote_feet = player.pos;
    v.wrote_yaw  = player.yaw;
    v.stood      = player.pos.y;   // where it was put, as the server counts it
}

} // namespace

void vehicle_put(Player &player, const World &world, const NodeRegistry &reg,
                 const MovementRules &rules, const DriveEvent &record, const MayMiss &may_miss)
{
    put(player, world, reg, rules, &record, &may_miss);
}

void vehicle_step(Player &player, const World &world, const NodeRegistry &reg,
                  const MovementRules &rules, const DriveEvent &record, const MayMiss &may_miss)
{
    if (!player.vehicle) return;
    Vehicle &v = *player.vehicle;
    v.was      = v.now;
    // A rebuild the client did not make is made of every chunk here.
    const DriveEvent none;
    v.replay   = record.built ? &record : &none;
    v.may_miss = &may_miss;
    step(v, world, reg, rules, record.keys, player.nitro);
    v.replay   = nullptr;
    v.may_miss = nullptr;
    v.now = pose_of(v);

    const JPH::BodyInterface &bodies = v.physics.GetBodyInterface();
    const JPH::Vec3           vel    = bodies.GetLinearVelocity(v.chassis);
    player.pos         = v.now.feet;
    player.yaw         = v.now.yaw;
    player.body_pitch  = v.now.pitch;
    player.body_roll   = v.now.roll;
    player.body_base   = v.now.base;
    player.velocity    = cv(vel);
    player.drive_speed = vel.Dot(bodies.GetRotation(v.chassis) * JPH::Vec3::sAxisZ());
    player.on_ground   = any_wheel_down(v);
    v.wrote_feet       = player.pos;
    v.wrote_yaw        = player.yaw;
}

bool vehicle_feet(const Player &player, Vec3 &was, Vec3 &now)
{
    if (!player.vehicle) return false;
    was = player.vehicle->was.feet;
    now = player.vehicle->now.feet;
    return true;
}

void vehicle_update(Player &player, const World &world, const NodeRegistry &reg,
                    const MovementRules &rules, const PlayerInput &input, float dt,
                    std::vector<DriveEvent> *log)
{
    player.flying = false;

    const auto same = [](const MovementRules &a, const MovementRules &b) {
        return a.walk == b.walk && a.sprint == b.sprint && a.jump == b.jump &&
               a.gravity == b.gravity && a.accel == b.accel && a.brake == b.brake &&
               a.turn == b.turn && a.drag == b.drag && a.width == b.width && a.length == b.length;
    };

    // Made afresh, and not moved, when somebody else moved the player since the
    // last update: a chassis moved in place keeps its engine's revs and its
    // wheels' spin, which a server replaying the log never saw and cannot copy.
    // And afresh when the blocks it stood on were read out of another world.
    const bool moved = player.vehicle &&
                       (length(player.pos - player.vehicle->wrote_feet) > 1e-3f ||
                        std::fabs(player.yaw - player.vehicle->wrote_yaw) > 1e-4f);
    if (!player.vehicle || !same(player.vehicle->rules, rules) || moved ||
        player.vehicle->ground_world != &world) {
        DriveEvent e;
        e.put = true;
        e.pos = player.pos;
        e.vel = player.velocity;
        e.yaw = player.yaw;
        put(player, world, reg, rules, nullptr, nullptr);
        e.built   = true;
        e.missing = player.vehicle->built_missing;
        if (log) log->push_back(e);
    }
    Vehicle            &v      = *player.vehicle;
    JPH::BodyInterface &bodies = v.physics.GetBodyInterface();
    const Vec3          held_at = player.pos;
    const DriveKeys     keys    = drive_keys(input);

    v.accumulator += dt;
    for (int n = 0; v.accumulator >= kStep && n < kMaxStepsPerUpdate; ++n) {
        v.accumulator -= kStep;
        v.was   = v.now;
        v.built = false;
        step(v, world, reg, rules, keys, player.nitro);
        v.now = pose_of(v);
        if (log) {
            DriveEvent e;
            e.keys    = keys;
            e.built   = v.built;
            e.missing = v.built ? v.built_missing : 0;
            log->push_back(e);
        }
    }
    v.accumulator = std::min(v.accumulator, kStep);

    // Drawn between the last two steps, or the view steps at 60 Hz on a
    // screen that does not.
    const float t = v.accumulator / kStep;
    player.pos        = v.was.feet + (v.now.feet - v.was.feet) * t;
    player.yaw        = lerp_angle(v.was.yaw, v.now.yaw, t);
    player.body_pitch = v.was.pitch + (v.now.pitch - v.was.pitch) * t;
    player.body_roll  = lerp_angle(v.was.roll, v.now.roll, t);
    player.body_base  = v.was.base + (v.now.base - v.was.base) * t;

    // Held: exactly where it was. step() pins the chassis, but through Jolt's
    // centre of mass, and the way back out of it is a float off now and then
    // -- 0.000001 of a node, which a server allowing nothing refused for the
    // whole countdown (30 corrections, one start in ten).
    if (rules.sprint <= 0.0f && rules.walk <= 0.0f) {
        player.pos.x = held_at.x;
        player.pos.z = held_at.z;
    }

    // And never inside what it stands on. A wheel pressed into the road -- the
    // chassis put down on its springs, a hard landing -- touches it up to 0.13
    // below its top, and feet there are inside the road to the server: one
    // start in five and a landing now and then refused (0.06 and 0.13 deep,
    // measured). Onto the top of that cell, when that is clear and less than a
    // wheel away; deeper is a chassis in a wall, and that is the server's.
    if (inside_solid(world, reg, player.pos.x, player.pos.y, player.pos.z, nullptr, 0, kBoxSkin)) {
        const float top = std::floor(player.pos.y + 0.05f) + 1.0f;
        if (top - player.pos.y < kWheelRadius &&
            !inside_solid(world, reg, player.pos.x, top, player.pos.z, nullptr, 0, kBoxSkin))
            player.pos.y = top;
    }

    const JPH::Vec3 vel = bodies.GetLinearVelocity(v.chassis);
    player.velocity     = cv(vel);
    player.drive_speed  = vel.Dot(bodies.GetRotation(v.chassis) * JPH::Vec3::sAxisZ());
    player.on_ground    = any_wheel_down(v);

    v.wrote_feet = player.pos;
    v.wrote_yaw  = player.yaw;
}

} // namespace cnp
