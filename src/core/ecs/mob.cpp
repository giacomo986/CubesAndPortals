#include "core/ecs/mob.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <unordered_map>
#include <vector>

#include "core/ecs/components.h"
#include "core/ecs/drop.h"
#include "core/item/item_registry.h"
#include "core/media/gltf.h"
#include "core/physics/movement.h"
#include "core/registry/node_registry.h"
#include "core/voxel/chunk.h"
#include "core/voxel/raycast.h"

namespace cnp::ecs {

// The first body that can be hurt whose box a ray enters within `max`
// (shoot_bodies), without hurting it.
bool first_body(flecs::world &w, Vec3 from, Vec3 dir, float max, flecs::entity &out, float &at);

namespace {

// How far a creature notices a player, and how far an archer backs off to.
constexpr float kArcherNear  = 6.0f;

// An arrow: how fast it leaves the string, how it falls, how long it lasts.
constexpr float  kArrowSpeed   = 22.0f;
constexpr float  kArrowGravity = -12.0f;
constexpr double kArrowStuck   = 8.0;    // seconds in a block before it goes
constexpr double kArrowFlight  = 6.0;    // seconds in the air at most
// A player's: The classic block game's 0.05 node/tick^2 and 0.99 a tick, at 20 ticks.
constexpr float  kPlayerArrowGravity = -20.0f;
constexpr float  kPlayerArrowDrag    = 0.201f;   // -ln(0.99^20)
constexpr double kPlayerArrowStuck   = 60.0;
constexpr double kLooseAfter   = 0.6;    // the shoot clip lets go 0.6 s in
constexpr double kSwing        = 0.8;    // the attack clip, played while Working
constexpr double kDraw         = 1.0;    // the shoot clip: draw, loose, nock

constexpr float kJump = 8.0f;            // clears a node: 8^2 / 50 = 1.28

// How a creature's body moves, for the way to a player: two cells tall, a node
// up, three down; it swims and climbs ladders, and opens doors if its kind
// does. In a hurry (1.5): a way a few percent longer than the shortest for a
// twelfth of the cells looked at, so 600 of them -- a few tenths of a
// millisecond -- reach a player round most of what is in the way.
PathBody body_of(const MobKind &k)
{
    PathBody b;
    b.height      = k.height > 1.0f ? 2 : 1;
    b.max_nodes   = 600;
    b.hurry       = 1.5f;
    b.opens_doors = k.opens_doors;
    return b;
}
constexpr double kReplan = 0.75;       // seconds between plans, per creature
constexpr double kStuck    = 3.0;      // seconds no nearer the next cell: stuck
constexpr double kAvoidFor = 20.0;     // and how long a cell it was stuck at is avoided
constexpr size_t kAvoided  = 8;

// What the way says to do this tick.
struct Step {
    float    x = 0.0f, z = 0.0f;   // which way to walk, unit or zero
    bool     jump     = false;     // the next cell is a step up, and it is close
    int      vertical = 0;         // on a ladder: +1 up, -1 down
    bool     door     = false;     // a shut door in the next cell, within reach
    bool     open_door = false;    // or one somebody left open, to shut behind it
    PathCell door_at;
};

// Which way to walk to reach a player: along the planned way, planned again
// when it is stale or they have moved off its end. False when there is no way
// worth taking -- then the caller walks straight at them.
bool walk_the_way(const World &world, const NodeRegistry &reg, const MobKind &k, Brain &b,
                  uint64_t who, const Position &p, const Target &goal, double now, Step &step)
{
    PathBody body = body_of(k);
    if (now < b.avoid_until) body.avoid = &b.avoid;
    const PathCell there{(int)std::floor(goal.x), (int)std::floor(goal.y + 0.01f),
                         (int)std::floor(goal.z)};
    const bool moved = std::abs(there.x - b.planned_for.x) + std::abs(there.y - b.planned_for.y) +
                           std::abs(there.z - b.planned_for.z) > 1;
    if (now >= b.replan_at || (moved && now >= b.replan_at - kReplan * 0.5) ||
        b.next_cell >= b.path.size()) {
        const PathCell here{(int)std::floor(p.x), (int)std::floor(p.y + 0.01f), (int)std::floor(p.z)};
        PathResult r = find_path(world, reg, here, there, body);
        // Corners only: a creature steering at the next point walks straight
        // across open ground instead of in the eight directions of the cells.
        b.path        = path_smooth(world, reg, here, r.cells, body);
        b.next_cell   = 0;
        b.planned_for = there;
        b.way_reached = r.reached;
        // Spread over ticks by who it is, so a crowd does not plan at once.
        b.replan_at = now + kReplan + (double)(who % 7) * 0.05;
    }

    // Past the cells already stood in: over the middle of one, with the feet
    // at its floor -- which for a cell up a ladder means having climbed to it.
    while (b.next_cell < b.path.size()) {
        const PathCell &c = b.path[b.next_cell];
        const float dx = (float)c.x + 0.5f - p.x, dz = (float)c.z + 0.5f - p.z;
        if (dx * dx + dz * dz < 0.35f * 0.35f && p.y > (float)c.y - 0.3f && p.y < (float)c.y + 0.8f)
            ++b.next_cell;
        else
            break;
    }
    if (b.next_cell >= b.path.size()) return false;

    const PathCell &c = b.path[b.next_cell];
    const float dx = (float)c.x + 0.5f - p.x, dz = (float)c.z + 0.5f - p.z;
    const float d  = std::sqrt(dx * dx + dz * dz);

    // No nearer the cell it walks at for kStuck seconds -- a panel the way
    // went through, a corner it cannot round, a crowd in a doorway: the cell
    // is avoided for a while and the way planned again now, round it. Again
    // and again if need be: up to kAvoided cells, the oldest let go first.
    const float dy   = (float)c.y - p.y;
    const float near = std::sqrt(d * d + dy * dy);
    if (c.x != b.stuck_on.x || c.y != b.stuck_on.y || c.z != b.stuck_on.z || near < b.stuck_best - 0.3f ||
        now - b.stuck_seen > 0.5) {
        b.stuck_on   = c;
        b.stuck_best = near;
        b.stuck_at   = now;
    } else if (now - b.stuck_at > kStuck) {
        // The cell just ahead of it, what it is pushing at; the corner it
        // walks to may be far beyond, or the goal, which is never avoided.
        PathCell ahead = c;
        if (d > 1e-3f) {
            const PathCell at{(int)std::floor(p.x + dx / d * 0.8f), (int)std::floor(p.y + 0.01f),
                              (int)std::floor(p.z + dz / d * 0.8f)};
            if (at.x != (int)std::floor(p.x) || at.z != (int)std::floor(p.z)) ahead = at;
        }
        if (now >= b.avoid_until) b.avoid.clear();
        bool known = false;
        for (const PathCell &a : b.avoid) known = known || (a.x == ahead.x && a.y == ahead.y && a.z == ahead.z);
        if (!known) b.avoid.push_back(ahead);
        if (b.avoid.size() > kAvoided) b.avoid.erase(b.avoid.begin());
        b.avoid_until = now + kAvoidFor;
        b.stuck_at    = now;
        b.replan_at   = now;   // the next tick plans round it
    }
    b.stuck_seen = now;

    if (d > 1e-3f) {
        step.x = dx / d;
        step.z = dz / d;
    }
    step.jump = (float)c.y > p.y + 0.6f && d < 1.3f && d > 0.2f;   // a step, when it is next
    // On a ladder: up while the next cell is higher -- also the ledge beside
    // its top, which it climbs until its feet are over it and it can walk
    // onto it -- and down only when the next cell is under it. Up to the
    // ledge's height and no further, a body slid back below its edge and
    // hung there: a hall's keepers stopped at its ladder's top all night.
    if ((float)c.y + 0.1f > p.y && d < 1.6f) step.vertical = 1;
    else if ((float)c.y < p.y - 0.3f && d < 0.6f) step.vertical = -1;

    // A shut door in the way: in the next cell, at the feet or the head.
    if (k.opens_doors && d < 1.6f)
        for (int up = 0; up < body.height; ++up) {
            const BlockId there_b = world.get_block(c.x, c.y + up, c.z);
            const uint8_t door = reg.get(there_b).door;
            if (door == 1 || door == 2) {
                step.door      = door == 1;
                step.open_door = door == 2;
                step.door_at   = PathCell{c.x, c.y + up, c.z};
                break;
            }
        }
    return true;
}

float yaw_to(float dx, float dz) { return std::atan2(dx, -dz); }   // Player::forward's inverse

// A creature in love (Brain::love_until), for another to find.
struct Lover {
    flecs::entity_t id;
    uint16_t        kind;
    float           x, y, z;
};

// The nearest other one of that kind in love, within kLoveSeek of p, or null.
const Lover *nearest_lover(const std::vector<Lover> &lovers, flecs::entity_t self, uint16_t kind,
                           const Position &p)
{
    const Lover *best = nullptr;
    float        near = kLoveSeek * kLoveSeek;
    for (const Lover &l : lovers) {
        if (l.id == self || l.kind != kind) continue;
        const float dx = l.x - p.x, dy = l.y - p.y, dz = l.z - p.z;
        const float d  = dx * dx + dy * dy + dz * dz;
        if (d < near) {
            near = d;
            best = &l;
        }
    }
    return best;
}

// Where a young one's parent is, when it is alive and 3 to 24 nodes off
// (further, and it has lost it); null otherwise.
const Position *parent_away(flecs::world &w, const Brain &b, const Position &p)
{
    if (b.grown_at <= 0.0 || b.parent == 0 || !w.entity(b.parent).is_alive()) return nullptr;
    const Position *mum = w.entity(b.parent).try_get<Position>();
    if (mum == nullptr) return nullptr;
    const float d = std::fabs(mum->x - p.x) + std::fabs(mum->z - p.z);
    return d > 3.0f && d < 24.0f ? mum : nullptr;
}

// The nearest player within `within` of p, or null.
const Target *nearest(const std::vector<Target> &targets, const Position &p, float within)
{
    const Target *best = nullptr;
    for (const Target &t : targets) {
        const float dx = t.x - p.x, dy = t.y - p.y, dz = t.z - p.z;
        const float d  = std::sqrt(dx * dx + dy * dy + dz * dz);
        if (d < within) {
            within = d;
            best   = &t;
        }
    }
    return best;
}

// A small deterministic hash, so a spawn is repeatable from a seed and nothing
// here reaches for a global generator.
uint32_t mix(uint32_t h)
{
    h ^= h >> 16; h *= 0x7feb352dU; h ^= h >> 15; h *= 0x846ca68bU; h ^= h >> 16;
    return h;
}
float unit(uint32_t h) { return (float)(mix(h) & 0xFFFFFF) / (float)0x1000000; }

bool solid(const World &world, const NodeRegistry &reg, int x, int y, int z)
{
    return reg.is_solid(world.get_block(x, y, z));
}

bool open(const World &world, const NodeRegistry &reg, int x, int y, int z)
{
    const BlockId b = world.get_block(x, y, z);
    return !reg.is_solid(b) && !reg.is_liquid(b);
}

// Whether a creature's eye sees a player's chest: no solid block between.
bool sees(const World &world, const NodeRegistry &reg, Vec3 eye, Vec3 at)
{
    const Vec3  d   = at - eye;
    const float len = std::sqrt(d.x * d.x + d.y * d.y + d.z * d.z);
    if (len < 1e-3f) return true;
    const RaycastHit hit = raycast(world, reg, eye, d * (1.0f / len), len);
    return !hit.hit;
}

// The nearest cell within 8 nodes, 3 up or down, that a body could stand in
// with its head out of the sun. False when there is none.
bool find_shade(const World &world, const NodeRegistry &reg, const Position &p, PathCell &out)
{
    const int fx = (int)std::floor(p.x), fy = (int)std::floor(p.y + 0.01f), fz = (int)std::floor(p.z);
    int best = 1 << 30;
    for (int dy = -3; dy <= 3; ++dy)
        for (int dz = -8; dz <= 8; ++dz)
            for (int dx = -8; dx <= 8; ++dx) {
                const int d2 = dx * dx + dy * dy + dz * dz;
                if (d2 >= best) continue;
                const int x = fx + dx, y = fy + dy, z = fz + dz;
                if (!solid(world, reg, x, y - 1, z) || !open(world, reg, x, y, z) ||
                    !open(world, reg, x, y + 1, z))
                    continue;
                if (sun_light(world.get_light(x, y + 1, z)) == kMaxLight) continue;
                best = d2;
                out  = PathCell{x, y, z};
            }
    return best < (1 << 30);
}

void loose_arrow(flecs::world &w, Vec3 from, Vec3 at, uint16_t damage, uint16_t model,
                 double now, bool fireball, uint16_t bare = 0, uint64_t shooter = 0)
{
    // Aimed at where the player is, lifted for the drop over the distance: the
    // arrow falls kArrowGravity * t^2 / 2 on the way there. A fireball does
    // not fall, and is slow enough to step out of the way of.
    const float gravity = fireball ? 0.0f : kArrowGravity;
    const float speed   = fireball ? 12.0f : kArrowSpeed;
    const Vec3  d    = at - from;
    const float flat = std::sqrt(d.x * d.x + d.z * d.z);
    const float t    = std::sqrt(d.x * d.x + d.y * d.y + d.z * d.z) / speed;
    Vec3        v{d.x / t, d.y / t - 0.5f * gravity * t, d.z / t};
    const float s = std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
    v = v * (speed / s);

    Position p{};
    p.x = from.x; p.y = from.y; p.z = from.z;
    p.valid = true;
    flecs::entity e = w.entity()
                          .set<Position>(p)
                          .set<Look>({yaw_to(v.x, v.z), std::atan2(v.y, flat > 1e-3f ? flat : 1e-3f)})
                          .set<Arrow>({v.x, v.y, v.z, damage, -1.0, now, gravity, shooter, 0, 0.0f, bare})
                          .add<Thing>()
                          .add<Body>();
    if (model != 0) e.set<Wears>({model});
}

// A creature a player's arrow struck, hurt after the arrows are stepped.
struct ArrowStrike {
    flecs::entity_t target;
    uint16_t        damage;
    Vec3            from;
};

void step_arrows(flecs::world &w, const World &world, const NodeRegistry &reg,
                 const std::vector<Target> &targets, float dt, double now,
                 const std::function<void(uint32_t, uint16_t, uint16_t, Vec3)> &hurt,
                 std::vector<ArrowStrike> &strikes)
{
    std::vector<flecs::entity_t> gone;
    w.each([&](flecs::entity e, Position &p, Look &look, Arrow &a) {
        if (a.landed_at >= 0.0) {
            if (now - a.landed_at > (a.shooter != 0 ? kPlayerArrowStuck : kArrowStuck))
                gone.push_back(e.id());
            return;
        }
        if (now - a.shot_at > kArrowFlight) {
            gone.push_back(e.id());
            return;
        }

        a.vy += a.gravity * dt;
        if (a.drag > 0.0f) {
            const float keep = std::exp(-a.drag * dt);
            a.vx *= keep, a.vy *= keep, a.vz *= keep;
        }
        const Vec3  from{p.x, p.y, p.z};
        const Vec3  v{a.vx, a.vy, a.vz};
        const float len = std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z) * dt;
        if (len < 1e-5f) return;
        const Vec3 dir = v * (dt / len);

        // A player first, then a block, whichever the segment meets sooner.
        float    best = len;
        uint32_t hit_peer = 0;
        for (const Target &t : targets) {
            if (a.shooter != 0 && t.entity == a.shooter) continue;   // not the archer
            const AABB box{Vec3{t.x - 0.3f, t.y, t.z - 0.3f}, Vec3{t.x + 0.3f, t.y + 1.8f, t.z + 0.3f}};
            float      at = 0.0f;
            if (ray_hits_box(box, from, dir, best, at) && at < best) {
                best     = at;
                hit_peer = t.peer;
            }
        }
        const RaycastHit wall = raycast(world, reg, from, dir, len);
        // A player's arrow strikes a creature it meets before the wall and
        // before a player, and is gone into it.
        if (a.shooter != 0) {
            flecs::entity body;
            float         at = 0.0f;
            if (first_body(w, from, dir, wall.hit ? std::min(best, wall.distance) : best, body, at)) {
                strikes.push_back({body.id(), a.damage, from});
                gone.push_back(e.id());
                return;
            }
        }
        if (wall.hit && wall.distance <= best) {
            p.x = from.x + dir.x * wall.distance;
            p.y = from.y + dir.y * wall.distance;
            p.z = from.z + dir.z * wall.distance;
            a.landed_at = now;
            if (a.gravity == 0.0f) gone.push_back(e.id());   // a fireball bursts
            e.modified<Position>();
            return;
        }
        if (hit_peer != 0) {
            gone.push_back(e.id());
            if (a.damage == 0) return;   // nothing, and no knock either
            // Knocked the way the arrow was going, less than a blow.
            const float flat = std::sqrt(v.x * v.x + v.z * v.z);
            const Vec3  push = flat > 1e-3f ? Vec3{v.x / flat * kKnock * 0.5f, kKnockUp * 0.5f,
                                                   v.z / flat * kKnock * 0.5f}
                                            : Vec3{0.0f, 0.0f, 0.0f};
            hurt(hit_peer, a.damage, a.bare, push);
            return;
        }

        p.x += v.x * dt;
        p.y += v.y * dt;
        p.z += v.z * dt;
        const float flat = std::sqrt(v.x * v.x + v.z * v.z);
        look.yaw   = yaw_to(v.x, v.z);
        look.pitch = std::atan2(v.y, flat > 1e-3f ? flat : 1e-3f);
        e.modified<Position>();
    });
    for (const flecs::entity_t id : gone) w.entity(id).destruct();
}

} // namespace

void loose_player_arrow(flecs::world &w, Vec3 from, Vec3 v, uint16_t damage, uint16_t model,
                        uint64_t shooter, uint16_t item, double now)
{
    Position p{};
    p.x = from.x; p.y = from.y; p.z = from.z;
    p.valid = true;
    const float   flat = std::sqrt(v.x * v.x + v.z * v.z);
    flecs::entity e    = w.entity()
                          .set<Position>(p)
                          .set<Look>({yaw_to(v.x, v.z), std::atan2(v.y, flat > 1e-3f ? flat : 1e-3f)})
                          .set<Arrow>({v.x, v.y, v.z, damage, -1.0, now, kPlayerArrowGravity, shooter,
                                       item, kPlayerArrowDrag})
                          .add<Thing>()
                          .add<Body>();
    if (model != 0) e.set<Wears>({model});
}

bool make_mob_kind(MobKind &out, std::string &why, const char *name, int model, int health,
                   double speed, const char *fight, int damage, double reach, double every,
                   const char *drop, int drop_count, int shot_model, int opens_doors,
                   double hover, int fireball, const char *spawn_on, size_t models)
{
    const std::string how = fight ? fight : "";
    if (how != "melee" && how != "archer" && how != "pet" && how != "worker") {
        why = "fight is \"melee\", \"archer\", \"pet\" or \"worker\", not \"" + how + "\"";
        return false;
    }
    const auto clamp = [](double v, double lo, double hi) { return v < lo ? lo : v > hi ? hi : v; };

    MobKind k;
    k.name        = name ? name : "";
    k.model       = (uint16_t)(model < 0 || (size_t)model > models ? 0 : model);
    k.health      = (uint16_t)clamp(health, 1, 1000);
    k.speed       = (float)clamp(speed, 0.0, 12.0);
    k.fight       = how == "archer" ? MobKind::Fight::Archer
                  : how == "pet"    ? MobKind::Fight::Pet
                  : how == "worker" ? MobKind::Fight::Worker
                                    : MobKind::Fight::Melee;
    k.damage      = (uint16_t)clamp(damage, 0, 100);
    k.reach       = (float)clamp(reach, 0.5, 32.0);
    k.every       = (float)clamp(every, 0.2, 30.0);
    k.drop_name   = drop ? drop : "";   // found once every mod has loaded
    k.drop_count  = (uint16_t)clamp(drop_count, 1, 64);
    k.shot_model  = (uint16_t)(shot_model < 0 || (size_t)shot_model > models ? 0 : shot_model);
    k.opens_doors = opens_doors != 0;
    k.hover       = (float)clamp(hover, 0.0, 24.0);
    k.fireball    = fireball != 0;
    k.spawn_on    = spawn_on ? spawn_on : "";
    out = k;
    return true;
}

int add_mob_kind(std::vector<MobKind> &kinds, const MobKind &k)
{
    for (size_t i = 0; i < kinds.size(); ++i)
        if (kinds[i].name == k.name) {
            const MobKind was = kinds[i];
            kinds[i] = k;
            // Registered again: what tames, shears and lays it stays said.
            if (k.tame_name.empty()) {
                kinds[i].tame_name  = was.tame_name;
                kinds[i].tame       = was.tame;
                kinds[i].tame_model = was.tame_model;
            }
            if (k.shear_name.empty()) {
                kinds[i].shear_name   = was.shear_name;
                kinds[i].fleece_name  = was.fleece_name;
                kinds[i].shear        = was.shear;
                kinds[i].fleece       = was.fleece;
                kinds[i].fleece_count = was.fleece_count;
                kinds[i].shorn_model  = was.shorn_model;
                kinds[i].regrow       = was.regrow;
            }
            if (k.breed_name.empty()) {
                kinds[i].breed_name = was.breed_name;
                kinds[i].breed      = was.breed;
                kinds[i].baby_model = was.baby_model;
                kinds[i].grow       = was.grow;
                kinds[i].hearts_model = was.hearts_model;
            }
            if (k.lay_name.empty()) {
                kinds[i].lay_name = was.lay_name;
                kinds[i].lay      = was.lay;
                kinds[i].lay_min  = was.lay_min;
                kinds[i].lay_max  = was.lay_max;
            }
            return (int)i + 1;
        }
    kinds.push_back(k);
    return (int)kinds.size();
}

void find_mob_items(std::vector<MobKind> &kinds, const ItemRegistry &items)
{
    const auto find = [&](ItemId &id, const std::string &name) {
        if (id == kNoItem && !name.empty()) id = items.find(name);
    };
    for (MobKind &k : kinds) {
        find(k.drop, k.drop_name);
        find(k.tame, k.tame_name);
        find(k.shear, k.shear_name);
        find(k.fleece, k.fleece_name);
        find(k.lay, k.lay_name);
        find(k.extra, k.extra_name);
        find(k.breed, k.breed_name);
    }
}

int mob_shear(std::vector<MobKind> &kinds, const char *name, const char *shears,
              const char *fleece, int count, int shorn_model, double regrow)
{
    for (MobKind &k : kinds)
        if (k.name == (name ? name : "")) {
            k.shear_name   = shears ? shears : "";
            k.fleece_name  = fleece ? fleece : "";
            k.shear = k.fleece = kNoItem;   // found again by name
            k.fleece_count = (uint16_t)std::clamp(count, 1, 64);
            k.shorn_model  = (uint16_t)std::max(shorn_model, 0);
            k.regrow       = (float)std::clamp(regrow, 1.0, 36000.0);
            return 1;
        }
    return 0;
}

int mob_lays(std::vector<MobKind> &kinds, const char *name, const char *item, double min_s,
             double max_s)
{
    for (MobKind &k : kinds)
        if (k.name == (name ? name : "")) {
            k.lay_name = item ? item : "";
            k.lay      = kNoItem;
            k.lay_min  = (float)std::clamp(min_s, 1.0, 36000.0);
            k.lay_max  = (float)std::clamp(max_s, (double)k.lay_min, 36000.0);
            return 1;
        }
    return 0;
}

int mob_drop(std::vector<MobKind> &kinds, const char *name, const char *item, int max)
{
    for (MobKind &k : kinds)
        if (k.name == (name ? name : "")) {
            k.extra_name = item ? item : "";
            k.extra      = kNoItem;
            k.extra_max  = (uint16_t)std::clamp(max, 0, 64);
            return 1;
        }
    return 0;
}

int push_mobs(flecs::world &w, int x, int y, int z, int dx, int dy, int dz)
{
    int moved = 0;
    w.each([&](flecs::entity e, Position &p, Mob) {
        if ((int)std::floor(p.x) != x || (int)std::floor(p.y + 0.01f) != y ||
            (int)std::floor(p.z) != z)
            return;
        p.x += (float)dx;
        p.y += (float)dy;
        p.z += (float)dz;
        e.modified<Position>();
        ++moved;
    });
    return moved;
}

int monsters_near(flecs::world &w, const std::vector<MobKind> &kinds, float x, float y, float z,
                  float r)
{
    int n = 0;
    w.each([&](const Position &p, const Brain &b, Mob) {
        if (b.kind >= kinds.size() || !fights(kinds[b.kind])) return;
        const float dx = p.x - x, dy = p.y - y, dz = p.z - z;
        if (dx * dx + dy * dy + dz * dz <= r * r) ++n;
    });
    return n;
}

int mobs_near(flecs::world &w, const std::vector<MobKind> &kinds, float x, float y, float z,
              float r, float *out, int max)
{
    std::vector<std::pair<float, Position>> found;
    w.each([&](const Position &p, const Brain &b, Mob) {
        if (b.kind >= kinds.size() || !fights(kinds[b.kind])) return;
        const float dx = p.x - x, dy = p.y - y, dz = p.z - z;
        const float d2 = dx * dx + dy * dy + dz * dz;
        if (d2 <= r * r) found.push_back({d2, p});
    });
    std::sort(found.begin(), found.end(), [](const auto &a, const auto &b) { return a.first < b.first; });
    int n = 0;
    for (const auto &[d2, p] : found) {
        if (n + 3 > max) break;
        out[n++] = p.x;
        out[n++] = p.y;
        out[n++] = p.z;
    }
    return n;
}

int mob_ids(flecs::world &w, const std::vector<MobKind> &kinds, float x, float y, float z, float r,
            int *out, int max)
{
    std::vector<std::pair<float, uint32_t>> found;
    w.each([&](flecs::entity e, const Position &p, const Brain &b, Mob) {
        if (b.kind >= kinds.size() || !fights(kinds[b.kind])) return;
        const float dx = p.x - x, dy = p.y - y, dz = p.z - z;
        const float d2 = dx * dx + dy * dy + dz * dz;
        if (d2 <= r * r) found.push_back({d2, (uint32_t)e.id()});
    });
    std::sort(found.begin(), found.end());
    int n = 0;
    for (const auto &f : found) {
        if (n >= max) break;
        out[n++] = (int)f.second;
    }
    return n;
}

// ponytail: the index alone; a creature that died and whose index was given to
// a new one answers for the new one. Ids last seconds in a mod, so it does not.
bool mob_pos(flecs::world &w, uint32_t id, float *x, float *y, float *z)
{
    const flecs::entity e = w.get_alive(id);
    if (!e.is_valid() || !e.has<Mob>()) return false;
    const Position *p = e.try_get<Position>();
    if (p == nullptr) return false;
    *x = p->x, *y = p->y, *z = p->z;
    return true;
}

namespace {
// A live worker by the low bits of its id, and its Brain; null otherwise.
Brain *worker(flecs::world &w, uint32_t id, flecs::entity &e)
{
    e = w.get_alive(id);
    if (!e.is_valid() || !e.has<Act>()) return nullptr;
    return e.try_get_mut<Brain>();
}
} // namespace

int mob_goto(flecs::world &w, const std::vector<MobKind> &, uint32_t id, int x, int y, int z)
{
    flecs::entity e;
    Brain        *b = worker(w, id, e);
    if (b == nullptr) return 0;
    b->going    = true;
    b->going_to = PathCell{x, y, z};
    b->path.clear();        // planned afresh for the new place
    b->replan_at = 0.0;
    return 1;
}

int mob_hunt(flecs::world &w, uint32_t id, uint32_t peer)
{
    flecs::entity e;
    Brain        *b = worker(w, id, e);
    if (b == nullptr) return 0;
    b->hunt  = peer;
    b->going = false;
    b->path.clear();
    return 1;
}

int mob_attack(flecs::world &w, uint32_t id, uint32_t prey)
{
    flecs::entity e;
    Brain        *b = worker(w, id, e);
    if (b == nullptr) return 0;
    const flecs::entity t = prey != 0 ? w.get_alive(prey) : flecs::entity();
    b->prey  = t.is_valid() && t.has<Mob>() && t.id() != e.id() ? t.id() : 0;
    b->going = false;
    b->path.clear();
    return 1;
}

int mob_going(flecs::world &w, uint32_t id)
{
    flecs::entity e;
    const Brain  *b = worker(w, id, e);
    return b != nullptr && b->going ? 1 : 0;
}

int mob_act(flecs::world &w, const std::vector<MobKind> &, uint32_t id, const char *clip, ItemId held)
{
    flecs::entity e;
    if (worker(w, id, e) == nullptr) return 0;
    Act a;
    a.clip = clip ? std::string(clip).substr(0, 32) : std::string();
    a.held = held;
    e.set<Act>(a);
    return 1;
}

int mob_face(flecs::world &w, uint32_t id, float x, float z)
{
    flecs::entity e;
    if (worker(w, id, e) == nullptr) return 0;
    const Position *p = e.try_get<Position>();
    Look           *l = e.try_get_mut<Look>();
    if (p == nullptr || l == nullptr) return 0;
    if (std::fabs(x - p->x) + std::fabs(z - p->z) > 1e-3f) l->yaw = yaw_to(x - p->x, z - p->z);
    e.modified<Look>();
    return 1;
}

int mob_place(flecs::world &w, uint32_t id, float x, float y, float z)
{
    flecs::entity e;
    Brain        *b = worker(w, id, e);
    Position     *p = b != nullptr ? e.try_get_mut<Position>() : nullptr;
    if (p == nullptr) return 0;
    p->x = x, p->y = y, p->z = z;
    b->vy = b->kx = b->kz = 0.0f;
    b->going = false;
    b->path.clear();
    e.modified<Position>();
    return 1;
}

int mob_velocity(flecs::world &w, uint32_t id, float vx, float vy, float vz)
{
    flecs::entity e;
    Brain        *b = worker(w, id, e);
    if (b == nullptr) return 0;
    b->kx = vx, b->vy = vy, b->kz = vz;
    b->going = false;
    b->path.clear();
    return 1;
}

int mob_unseen(flecs::world &w, uint32_t id, uint32_t peer)
{
    flecs::entity e;
    if (worker(w, id, e) == nullptr) return 0;
    if (peer == 0)
        e.remove<UnseenBy>();
    else
        e.set<UnseenBy>({peer});
    return 1;
}

int mob_remove(flecs::world &w, uint32_t id)
{
    flecs::entity e;
    if (worker(w, id, e) == nullptr) return 0;
    e.destruct();
    return 1;
}

int mob_blow_every(flecs::world &w, uint32_t id, float factor)
{
    const flecs::entity e = w.get_alive(id);
    if (!e.is_valid() || !e.has<Mob>()) return 0;
    Brain *b = e.try_get_mut<Brain>();
    if (b == nullptr) return 0;
    b->blow_every = std::clamp(factor, 0.25f, 4.0f);
    return 1;
}

int slow_mobs(flecs::world &w, const std::vector<MobKind> &kinds, float x, float y, float z, float r,
              float factor, double seconds, double now)
{
    int n = 0;
    w.each([&](const Position &p, Brain &b, Mob) {
        if (b.kind >= kinds.size() || !fights(kinds[b.kind])) return;
        const float dx = p.x - x, dy = p.y - y, dz = p.z - z;
        if (dx * dx + dy * dy + dz * dz > r * r) return;
        b.slow       = std::clamp(factor, 0.05f, 1.0f);
        b.slow_until = now + std::clamp(seconds, 0.0, 60.0);
        ++n;
    });
    return n;
}

int mob_breed(std::vector<MobKind> &kinds, const char *name, const char *item, int baby_model,
              double grow, int hearts_model)
{
    for (MobKind &k : kinds)
        if (k.name == (name ? name : "")) {
            k.breed_name = item ? item : "";
            k.breed      = kNoItem;   // found again by name
            k.baby_model = (uint16_t)std::max(baby_model, 0);
            k.grow       = (float)std::clamp(grow, 1.0, 36000.0);
            k.hearts_model = (uint16_t)std::max(hearts_model, 0);
            return 1;
        }
    return 0;
}

void swap_door(const World &world, const NodeRegistry &reg, int x, int y, int z,
               const std::function<void(int, int, int, BlockId, uint8_t)> &write)
{
    const auto swap = [&](int cy) {
        const NodeDef &d = reg.get(world.get_block(x, cy, z));
        if (d.door == 0 || d.door_other == kAir) return false;
        write(x, cy, z, d.door_other, world.get_facing(x, cy, z));
        return true;
    };
    if (!swap(y)) return;
    const uint8_t was = reg.get(world.get_block(x, y, z)).door;   // after the swap
    for (const int cy : {y - 1, y + 1})
        if (reg.get(world.get_block(x, cy, z)).door != 0 &&
            reg.get(world.get_block(x, cy, z)).door != was)
            swap(cy);
}

int mob_pierce(std::vector<MobKind> &kinds, const char *name, double part)
{
    for (MobKind &k : kinds)
        if (k.name == (name ? name : "")) {
            k.pierce = (float)std::clamp(part, 0.0, 1.0);
            return 1;
        }
    return 0;
}

int mob_sight(std::vector<MobKind> &kinds, const char *name, double nodes)
{
    for (MobKind &k : kinds)
        if (k.name == (name ? name : "")) {
            k.notice = (float)std::clamp(nodes, 1.0, 128.0);
            return 1;
        }
    return 0;
}

namespace {

// The box a gun or a burst aims at: a creature's own body, or half a node
// round a mod's entity, feet at its y as the client draws it.
bool struck_box(flecs::entity e, const Position &p, AABB &box)
{
    if (e.has<DrivenBy>() || !(e.has<Mob>() || e.has<Thing>())) return false;
    const Health *h = e.try_get<Health>();
    if (h == nullptr || !h->alive()) return false;
    float half = 0.3f, height = 0.6f;
    if (const Brain *b = e.try_get<Brain>()) {
        half   = b->half;
        height = b->height;
    } else if (const ThingBox *t = e.try_get<ThingBox>()) {   // core.ecs.box
        half   = t->half;
        height = t->height;
    }
    box = AABB{Vec3{p.x - half, p.y, p.z - half}, Vec3{p.x + half, p.y + height, p.z + half}};
    return true;
}

void voice(flecs::world &w, uint16_t kind, const char *what, const Position &p)
{
    if (const MobVoice *v = w.try_get<MobVoice>(); v != nullptr && v->say)
        v->say(kind, what, Vec3{p.x, p.y + 1.0f, p.z});
}

bool struck(flecs::entity e, uint16_t damage, float from_x, float from_z, const Position &p)
{
    Health &h = e.get_mut<Health>();
    hurt(h, damage);
    e.modified<Health>();
    if (Brain *b = e.try_get_mut<Brain>()) knock_back(*b, from_x, from_z, p.x, p.z);
    return !h.alive();
}

} // namespace

bool first_body(flecs::world &w, Vec3 from, Vec3 dir, float max, flecs::entity &best, float &at)
{
    best = flecs::entity();
    at   = max;
    w.each([&](flecs::entity e, const Position &p, Body) {
        AABB box;
        if (!struck_box(e, p, box)) return;
        // Slabs: where the ray is inside all three.
        float near = 0.0f, far = at;
        for (int a = 0; a < 3; ++a) {
            const float o = a == 0 ? from.x : a == 1 ? from.y : from.z;
            const float d = a == 0 ? dir.x : a == 1 ? dir.y : dir.z;
            const float lo = a == 0 ? box.min.x : a == 1 ? box.min.y : box.min.z;
            const float hi = a == 0 ? box.max.x : a == 1 ? box.max.y : box.max.z;
            if (std::fabs(d) < 1e-9f) {
                if (o < lo || o > hi) return;
                continue;
            }
            float t1 = (lo - o) / d, t2 = (hi - o) / d;
            if (t1 > t2) std::swap(t1, t2);
            near = std::max(near, t1);
            far  = std::min(far, t2);
            if (near > far) return;
        }
        if (near < at) {
            at   = near;
            best = e;
        }
    });
    return best.is_valid();
}

bool shoot_bodies(flecs::world &w, Vec3 from, Vec3 dir, float max, uint16_t damage, ShotHit &out)
{
    flecs::entity best;
    float         at = 0.0f;
    if (!first_body(w, from, dir, max, best, at)) return false;
    const Position p = best.get<Position>();
    out.target   = best.id();
    out.distance = at;
    out.killed   = struck(best, damage, from.x, from.z, p);
    return true;
}

int mob_hurt(flecs::world &w, uint32_t id, uint16_t damage, float from_x, float from_z)
{
    flecs::entity e = w.get_alive(id);
    if (!e.is_valid() || !e.has<Mob>()) return 0;
    const Health   *h = e.try_get<Health>();
    const Position *p = e.try_get<Position>();
    if (h == nullptr || p == nullptr || !h->alive()) return 0;
    return struck(e, damage, from_x, from_z, *p) ? 2 : 1;
}

int blast_bodies(flecs::world &w, Vec3 centre, float r, uint16_t damage)
{
    std::vector<std::pair<flecs::entity_t, uint16_t>> hit;
    w.each([&](flecs::entity e, const Position &p, Body) {
        AABB box;
        if (!struck_box(e, p, box)) return;
        const Vec3  mid = (box.min + box.max) * 0.5f;
        const float d   = length(mid - centre);
        if (d > r) return;
        hit.push_back({e.id(), (uint16_t)std::max(1.0f, damage * (1.0f - 0.5f * d / r))});
    });
    int killed = 0;
    for (const auto &[id, dmg] : hit) {
        flecs::entity  e = w.entity(id);
        const Position p = e.get<Position>();
        if (struck(e, dmg, centre.x, centre.z, p)) ++killed;
    }
    return killed;
}

void knock_back(Brain &b, float from_x, float from_z, float x, float z)
{
    const float dx = x - from_x, dz = z - from_z;
    const float d  = std::sqrt(dx * dx + dz * dz);
    if (d < 1e-3f) return;
    b.kx = dx / d * kKnock;
    b.kz = dz / d * kKnock;
    b.vy = std::max(b.vy, kKnockUp);
    b.scared   = true;   // step_mobs: a wild pet runs from here
    b.scared_x = from_x;
    b.scared_z = from_z;
}

bool size_mob_kind(MobKind &k, const std::string &model_file)
{
    SkinnedModel m;
    std::string  why;
    if (model_file.empty() || !load_gltf(model_file, m, why)) return true;
    if (m.max.y <= m.min.y) return m.clip("death") >= 0;
    k.height = std::clamp(m.max.y, 0.3f, kMobHeight);
    k.half   = std::clamp(std::max(std::fabs(m.min.x), std::fabs(m.max.x)), 0.1f, kMobHalfWidth);
    k.spread = std::clamp(std::max(std::fabs(m.min.x), std::fabs(m.max.x)), 0.1f, 1.0f);
    return m.clip("death") >= 0;
}

AABB mob_box(float x, float y, float z)
{
    return AABB{Vec3{x - kMobHalfWidth, y, z - kMobHalfWidth},
                Vec3{x + kMobHalfWidth, y + kMobHeight, z + kMobHalfWidth}};
}

flecs::entity spawn_mob(flecs::world &w, const std::vector<MobKind> &kinds, uint16_t kind,
                        float x, float y, float z, double now)
{
    if (kind >= kinds.size()) return flecs::entity{};
    const MobKind &k = kinds[kind];

    Position p{};
    p.x = x; p.y = y; p.z = z;
    p.valid = true;

    Brain b;
    b.kind       = kind;
    b.height     = k.height;
    b.half       = k.half;
    b.spread     = k.spread;
    b.heading    = unit((uint32_t)(x * 73.0f) ^ (uint32_t)(z * 131.0f)) * 6.2831853f;
    b.rethink_at = now;

    flecs::entity e = w.entity()
                          .set<Position>(p)
                          .set<Look>({b.heading, 0.0f})
                          .set<Brain>(b)
                          .set<Health>({k.health, k.health})
                          .set<Drops>({k.drop, k.drop_count, k.lay, k.extra, k.extra_max})
                          .add<Mob>()
                          .add<Body>();

    // What it looks like, put on here and not added later: the streaming query
    // reads it off the entity, so a creature dressed a tick after it exists is
    // a creature that went out as a box first.
    if (k.model != 0) e.set<Wears>({k.model});
    if (k.fight == MobKind::Fight::Pet) e.set<Tame>({k.tame, k.tame_model, std::string(), false});
    if (k.fight == MobKind::Fight::Worker) e.set<Act>({});
    if (k.breed != kNoItem) e.set<Breed>({k.breed});
    if (k.shear != kNoItem) e.set<Fleece>({k.shear, k.fleece, k.fleece_count, k.shorn_model, k.regrow});
    return e;
}

std::vector<std::string> pets_to_lines(flecs::world &w, const std::vector<MobKind> &kinds)
{
    std::vector<std::string> out;
    w.each([&](const Position &p, const Brain &b, const Tame &t) {
        if (t.owner.empty() || b.kind >= kinds.size()) return;
        char buf[96];
        std::snprintf(buf, sizeof(buf), " %.2f %.2f %.2f %d ", (double)p.x, (double)p.y,
                      (double)p.z, t.sitting ? 1 : 0);
        out.push_back(kinds[b.kind].name + buf + t.owner);
    });
    return out;
}

int pets_from_lines(flecs::world &w, const std::vector<MobKind> &kinds,
                    const std::vector<std::string> &lines, double now)
{
    int made = 0;
    for (const std::string &line : lines) {
        char  kind[128] = {};
        float x = 0.0f, y = 0.0f, z = 0.0f;
        int   sitting = 0, used = 0;
        if (std::sscanf(line.c_str(), "%127s %f %f %f %d %n", kind, &x, &y, &z, &sitting, &used) < 5 ||
            used <= 0 || (size_t)used >= line.size())
            continue;
        for (size_t i = 0; i < kinds.size(); ++i) {
            if (kinds[i].name != kind || kinds[i].fight != MobKind::Fight::Pet) continue;
            flecs::entity e = spawn_mob(w, kinds, (uint16_t)i, x, y, z, now);
            Tame *t = e.try_get_mut<Tame>();
            t->owner   = line.substr((size_t)used);
            t->sitting = sitting != 0;
            if (t->model != 0) e.set<Wears>({t->model});
            ++made;
            break;
        }
    }
    return made;
}

bool use_pet(flecs::entity e, const std::string &who, ItemId held, bool &took, std::string &why)
{
    took       = false;
    Tame *tame = e.try_get_mut<Tame>();
    if (tame == nullptr) {
        why = "that is not a pet";
        return false;
    }
    if (tame->owner.empty()) {
        if (tame->with == kNoItem || held != tame->with) {
            why = "that is not what it wants";
            return false;
        }
        tame->owner = who;
        took        = true;
        if (tame->model != 0) e.set<Wears>({tame->model});   // its collar
        return true;
    }
    if (tame->owner != who) {
        why = "it is somebody else's";
        return false;
    }
    tame->sitting = !tame->sitting;
    return true;
}

bool shear_mob(flecs::world &w, flecs::entity e, ItemId held, double now)
{
    Brain          *b = e.try_get_mut<Brain>();
    const Position *p = e.try_get<Position>();
    const Fleece   *f = e.try_get<Fleece>();
    if (b == nullptr || p == nullptr || f == nullptr) return false;
    if (f->shears == kNoItem || held != f->shears || b->shorn_until > 0.0 || b->grown_at > 0.0)
        return false;

    b->shorn_until = now + f->regrow;
    if (f->shorn_model != 0) e.set<Wears>({f->shorn_model});
    // Startled from where it stands, a little behind itself: off it goes.
    b->scared   = true;
    b->scared_x = p->x - std::sin(b->heading);
    b->scared_z = p->z + std::cos(b->heading);
    if (f->wool != kNoItem) {
        ItemStack stack;
        stack.item  = f->wool;
        stack.count = f->count;
        spawn_drop(w, stack, p->x, p->y + b->height * 0.5f, p->z, now);
    }
    return true;
}

bool feed_mob(flecs::entity e, ItemId held, double now)
{
    Brain       *b = e.try_get_mut<Brain>();
    const Breed *f = e.try_get<Breed>();
    const Tame  *t = e.try_get<Tame>();
    if (b == nullptr || f == nullptr || f->food == kNoItem || held != f->food) return false;
    if ((t != nullptr && !t->owner.empty()) || b->grown_at > 0.0) return false;
    if (now < b->love_until || now < b->breed_again) return false;
    b->love_until = now + kLove;
    return true;
}

namespace {

// A young one of that kind: half the size, in its own model, leaving nothing
// when it dies and laying and shorn by nobody until it has grown up.
void spawn_young(flecs::world &w, const std::vector<MobKind> &kinds, uint16_t kind, float x,
                 float y, float z, double now, flecs::entity_t parent)
{
    flecs::entity e = spawn_mob(w, kinds, kind, x, y, z, now);
    if (!e.is_alive()) return;
    const MobKind &k = kinds[kind];
    Brain &b = e.get_mut<Brain>();
    b.grown_at    = now + k.grow;
    b.parent      = parent;
    b.breed_again = b.grown_at;
    b.height      = std::max(0.3f, k.height * 0.5f);
    b.half        = std::max(0.1f, k.half * 0.5f);
    b.spread      = std::max(0.1f, k.spread * 0.5f);
    e.set<Drops>({kNoItem, 1, kNoItem});
    if (k.baby_model != 0) e.set<Wears>({k.baby_model});
}

} // namespace

void step_mobs(flecs::world &w, const World &world, const NodeRegistry &reg,
               const std::vector<MobKind> &kinds, const std::vector<Target> &targets,
               float dt, double now, float daylight,
               const std::function<void(uint32_t peer, uint16_t amount, uint16_t bare, Vec3 push)> &hurt,
               const std::function<void(int x, int y, int z)> &swap_door)
{
    std::vector<PathCell> doors;   // opened after the walk, like the arrows
    struct Shot {
        Vec3     from, at;
        uint16_t damage, model;
        bool     fireball;
        uint16_t bare;
        uint64_t shooter;   // a worker shooting a creature: its arrow strikes creatures
    };
    // A worker's blows at the creature it was sent after, dealt after the walk.
    struct Blow {
        flecs::entity_t target;
        uint16_t        damage;
        float           x, z;
    };
    std::vector<Blow> blows;
    std::vector<Shot> shots;   // made after the walk, which must not create entities
    struct Laid {
        ItemId item;
        float  x, y, z;
    };
    std::vector<Laid> laid;    // the same

    // Who is in love, and where, to find each other; and the pairs that met.
    std::vector<Lover> lovers;
    // And every creature, to keep out of each other, filed by the column of
    // kCrowdCell it stands in: two can touch only from the same column or the
    // next, since neither spreads more than half a cell. So each is weighed
    // against the few round it and not against every creature in the world.
    struct Near {
        flecs::entity_t id;
        float           x, y, z, spread, height;
    };
    std::vector<Near> crowd;
    std::unordered_map<uint64_t, std::vector<uint32_t>> cells;
    const auto cell_of = [](float v) { return (int32_t)std::floor(v / kCrowdCell); };
    const auto key     = [](int32_t cx, int32_t cz) { return (uint64_t)(uint32_t)cx << 32 | (uint32_t)cz; };
    w.each([&](flecs::entity e, const Position &p, const Brain &b) {
        if (now < b.love_until) lovers.push_back({e.id(), b.kind, p.x, p.y, p.z});
        cells[key(cell_of(p.x), cell_of(p.z))].push_back((uint32_t)crowd.size());
        crowd.push_back({e.id(), p.x, p.y, p.z, b.spread, b.height});
    });
    struct Birth {
        flecs::entity_t a, b;
        uint16_t        kind;
        float           x, y, z;
    };
    std::vector<Birth> births;
    std::vector<std::pair<flecs::entity_t, uint16_t>> new_hearts;   // made after the walk

    // Deferred: the Working tag goes on and off inside the walk, and a tag is a
    // table move that must not happen under the iterator.
    w.defer_begin();
    w.each([&](flecs::entity e, Position &p, Look &look, Brain &b, Health &health) {
        if (b.kind >= kinds.size()) return;
        const MobKind &k = kinds[b.kind];

        // Heard: a blow since the last tick, from wherever it came, and now
        // and then a sound of its own. A killing blow is reap_dead's "death".
        if (health.alive() && health.hp < b.heard_hp) voice(w, b.kind, "hurt", p);
        b.heard_hp = health.hp;
        if (b.say_at == 0.0 || now >= b.say_at) {
            if (b.say_at != 0.0) voice(w, b.kind, "say", p);
            b.say_at = now + 6.0 + std::fmod((double)(e.id() % 997) * 0.618 + now * 0.37, 1.0) * 14.0;
        }

        // Where the world is not in memory there is nothing to stand on, and a
        // creature stepped there falls for ever: it waits, as a furnace does.
        const int fx = (int)std::floor(p.x), fy = (int)std::floor(p.y), fz = (int)std::floor(p.z);
        if (world.find_chunk(World::chunk_of(fx, fy, fz)) == nullptr ||
            world.find_chunk(World::chunk_of(fx, fy - 1, fz)) == nullptr)
            return;

        // A pet goes for nobody; its owner, when it has one and they are here.
        Tame         *tame  = k.fight == MobKind::Fight::Pet ? e.try_get_mut<Tame>() : nullptr;
        const Target *owner = nullptr;
        if (tame != nullptr && !tame->owner.empty())
            for (const Target &t : targets)
                if (t.name == tame->owner) owner = &t;

        // The nearest player it can notice; a worker, the one it was sent after.
        const Target *goal = nullptr;
        float         dist = fights(k) ? k.notice : 0.0f;
        const bool    worker_kind = k.fight == MobKind::Fight::Worker;
        for (const Target &t : targets) {
            const float dx = t.x - p.x, dy = t.y - p.y, dz = t.z - p.z;
            const float d  = std::sqrt(dx * dx + dy * dy + dz * dz);
            if (worker_kind ? b.hunt != 0 && t.peer == b.hunt : d < dist) {
                dist = d;
                goal = &t;
            }
        }
        // A worker with no player to go for and a creature it was sent after:
        // that creature, as if it were a player (peer 0).
        Target prey;
        if (worker_kind && goal == nullptr && b.prey != 0) {
            const flecs::entity t = w.entity(b.prey);
            const Position     *tp = t.is_alive() ? t.try_get<Position>() : nullptr;
            const Health       *th = t.is_alive() ? t.try_get<Health>() : nullptr;
            if (tp == nullptr || th == nullptr || !th->alive()) {
                b.prey = 0;
            } else {
                prey = Target{b.prey, 0, tp->x, tp->y, tp->z, std::string()};
                goal = &prey;
                dist = std::sqrt((tp->x - p.x) * (tp->x - p.x) + (tp->y - p.y) * (tp->y - p.y) +
                                 (tp->z - p.z) * (tp->z - p.z));
            }
        }
        // How it fights once it has somebody: a worker with a shot shoots.
        const bool melee = k.fight == MobKind::Fight::Melee || (worker_kind && k.shot_model == 0);
        const uint16_t bare = (uint16_t)std::lround(k.damage * k.pierce);

        const Vec3 eye{p.x, p.y + 1.55f, p.z};
        float      want_x = 0.0f, want_z = 0.0f;   // where it walks this tick, unit or zero
        const float pace  = now < b.slow_until ? b.slow : 1.0f;   // core.slow_mobs
        float      speed  = k.speed * pace;
        Step       way;                            // what its way says, when it has one

        // Struck or shorn: a wild pet runs (a tame one knows who it is with).
        const bool wild = tame != nullptr && tame->owner.empty();
        if (b.scared) {
            b.scared = false;
            if (wild) {
                b.panic_until = now + kPanic;
                b.rethink_at  = now;
            }
        }

        // Its fleece back, and now and then an egg.
        if (b.shorn_until > 0.0 && now >= b.shorn_until) {
            b.shorn_until = 0.0;
            if (k.model != 0) e.set<Wears>({k.model});
        }
        if (b.grown_at > 0.0 && now >= b.grown_at) {
            b.grown_at = 0.0;
            b.height   = k.height;
            b.half     = k.half;
            b.spread   = k.spread;
            e.set<Wears>({k.model});
            e.set<Drops>({k.drop, k.drop_count, k.lay, k.extra, k.extra_max});
        }
        if (k.lay != kNoItem && k.lay_max > 0.0f && b.grown_at <= 0.0 && now >= b.lay_at) {
            if (b.lay_at > 0.0) laid.push_back({k.lay, p.x, p.y + 0.3f, p.z});
            const uint32_t h = (uint32_t)e.id() * 2246822519U ^ (uint32_t)(now * 10.0);
            b.lay_at = now + k.lay_min + (k.lay_max - k.lay_min) * unit(h);
        }

        if (tame != nullptr && tame->sitting) {
            // Sat down where it was told to, whoever goes where.
        } else if (wild && now < b.panic_until) {
            // Away from what scared it, a new way every so often, running.
            if (now >= b.rethink_at) {
                const uint32_t h = (uint32_t)e.id() * 2654435761U ^ (uint32_t)(now * 10.0);
                const float    dx = p.x - b.scared_x, dz = p.z - b.scared_z;
                const float    away = std::fabs(dx) + std::fabs(dz) > 1e-3f ? yaw_to(dx, dz) : b.heading;
                b.heading    = away + (unit(h) - 0.5f) * 1.6f;
                b.rethink_at = now + 0.7 + 0.6 * unit(h + 1);
                b.strolling  = false;
            }
            want_x   = std::sin(b.heading);
            want_z   = -std::cos(b.heading);
            look.yaw = b.heading;
            speed *= kPanicSpeed;
        } else if (const Lover *mate = wild && now < b.love_until ? nearest_lover(lovers, e.id(), b.kind, p)
                                                                  : nullptr) {
            // In love, and another of its kind is too: to it, and a young one
            // when they meet (made after the walk, by the one of the two that
            // comes first).
            const float dx = mate->x - p.x, dz = mate->z - p.z;
            const float flat = std::sqrt(dx * dx + dz * dz);
            if (flat > 1e-3f) look.yaw = yaw_to(dx, dz);
            // Near enough, as the classic block game's: the crowd keeps them from ever
            // touching, and one of their kind may stand between them.
            if (flat > 2.0f * b.spread + 1.5f) {
                want_x = dx / flat;
                want_z = dz / flat;
            } else if (e.id() < mate->id) {
                births.push_back({e.id(), mate->id, b.kind, (p.x + mate->x) * 0.5f, p.y,
                                  (p.z + mate->z) * 0.5f});
            }
        } else if (const Position *mum = parent_away(w, b, p)) {
            // A young one wandered off: back to the one it came from, trotting.
            const float dx = mum->x - p.x, dz = mum->z - p.z;
            const float flat = std::sqrt(dx * dx + dz * dz);
            look.yaw = yaw_to(dx, dz);
            want_x   = dx / flat;
            want_z   = dz / flat;
            speed *= 1.3f;
        } else if (owner != nullptr) {
            // At heel: after them when they are more than a couple of nodes
            // off, quicker the further, and put down where they stand when
            // they are out of its sight's reach -- through a portal, up a cliff.
            const float dx = owner->x - p.x, dy = owner->y - p.y, dz = owner->z - p.z;
            const float flat = std::sqrt(dx * dx + dz * dz);
            if (flat > 1e-3f) look.yaw = yaw_to(dx, dz);
            if (flat > 14.0f || std::fabs(dy) > 6.0f) {
                p.x = owner->x;
                p.y = owner->y;
                p.z = owner->z;
                b.vy = 0.0f;
                b.path.clear();
            } else if (flat > 2.5f) {
                if (walk_the_way(world, reg, k, b, e.id(), p, *owner, now, way))
                    want_x = way.x, want_z = way.z;
                else
                    want_x = dx / flat, want_z = dz / flat;
                if (flat > 5.0f) speed *= 1.6f;
            }
        } else if (worker_kind && goal == nullptr) {
            // On its mod's errand, or standing where it was left.
            if (b.going) {
                const PathCell &c = b.going_to;
                const float dx = (float)c.x + 0.5f - p.x, dz = (float)c.z + 0.5f - p.z;
                const float flat = std::sqrt(dx * dx + dz * dz);
                if (flat < 0.4f && std::fabs((float)c.y - p.y) < 1.0f) {
                    b.going = false;
                    b.path.clear();
                } else {
                    const Target there{0, 0, (float)c.x + 0.5f, (float)c.y, (float)c.z + 0.5f};
                    const bool on_way = walk_the_way(world, reg, k, b, e.id(), p, there, now, way);
                    if (!b.way_reached && flat > 1.5f && flat < 48.0f) {
                        // No way there -- into a block, behind a wall, up
                        // where nothing climbs: it stops and says so
                        // (core.mob_going goes to 0) instead of walking as
                        // near as it can and on straight at it. A builder sent
                        // at a chest's own cell stood in a corner of the
                        // mine's hut jumping at it. Further than a way is
                        // looked for, it goes as before.
                        b.going = false;
                        b.path.clear();
                    } else if (on_way)
                        want_x = way.x, want_z = way.z;
                    else if (flat > 1e-3f)
                        want_x = dx / flat, want_z = dz / flat;
                    if (want_x != 0.0f || want_z != 0.0f) look.yaw = yaw_to(want_x, want_z);
                    if (flat < 1.0f) speed *= 0.6f;   // not past it
                }
            }
        } else if (goal != nullptr) {
            const float dx = goal->x - p.x, dz = goal->z - p.z;
            const float flat = std::sqrt(dx * dx + dz * dz);
            look.yaw = yaw_to(dx, dz);
            const Vec3 chest{goal->x, goal->y + 1.3f, goal->z};

            if (melee) {
                if (flat > k.reach * 0.8f && flat > 1e-3f) {
                    if (k.hover <= 0.0f && walk_the_way(world, reg, k, b, e.id(), p, *goal, now, way)) {
                        want_x = way.x, want_z = way.z;
                    } else {
                        want_x = dx / flat;
                        want_z = dz / flat;
                    }
                }
                if (k.damage > 0 && dist <= k.reach && now >= b.next_blow &&
                    sees(world, reg, eye, chest)) {
                    const Vec3 push = flat > 1e-3f ? Vec3{dx / flat * kKnock, kKnockUp, dz / flat * kKnock}
                                                   : Vec3{0.0f, kKnockUp, 0.0f};
                    if (goal->peer != 0) hurt(goal->peer, k.damage, bare, push);
                    else blows.push_back({goal->entity, k.damage, p.x, p.z});
                    voice(w, b.kind, "attack", p);
                    b.next_blow  = now + k.every * b.blow_every / pace;
                    b.busy_until = now + kSwing;
                }
            } else {
                const bool in_sight = sees(world, reg, eye, chest);
                if (flat > 1e-3f) {
                    if (!in_sight || flat > k.reach * 0.8f) {
                        if (k.hover <= 0.0f && walk_the_way(world, reg, k, b, e.id(), p, *goal, now, way))
                            want_x = way.x, want_z = way.z;
                        else
                            want_x = dx / flat, want_z = dz / flat;
                    } else if (flat < kArcherNear) {
                        want_x = -dx / flat, want_z = -dz / flat;
                        speed *= 0.7f;
                    }
                }
                // Draw while it sees them and is in range; the arrow leaves
                // the string kLooseAfter into the draw, if they are still there.
                if (in_sight && dist <= k.reach && now >= b.next_blow) {
                    b.next_blow  = now + std::max((double)k.every * b.blow_every, kDraw) / pace;
                    b.busy_until = now + kDraw;
                    b.loose_at   = now + kLooseAfter;
                }
                if (b.loose_at >= 0.0 && now >= b.loose_at) {
                    b.loose_at = -1.0;
                    if (in_sight) {
                        const Vec3 hand{p.x + std::sin(look.yaw) * 0.6f, p.y + 1.25f,
                                        p.z - std::cos(look.yaw) * 0.6f};
                        shots.push_back({hand, chest, k.damage, k.shot_model, k.fireball, bare,
                                         goal->peer == 0 ? e.id() : 0});
                        voice(w, b.kind, "shoot", p);
                    }
                }
            }
        } else if (b.burn_at > 0.0) {
            // Nobody about and burning: to the nearest shade, looked for again
            // every two seconds, and where there is none it burns where it is.
            if (now >= b.shade_at) {
                b.has_shade = find_shade(world, reg, p, b.shade);
                b.shade_at  = now + 2.0;
            }
            const Target cool{0, 0, (float)b.shade.x + 0.5f, (float)b.shade.y,
                              (float)b.shade.z + 0.5f};
            if (b.has_shade && walk_the_way(world, reg, k, b, e.id(), p, cool, now, way)) {
                want_x   = way.x;
                want_z   = way.z;
                look.yaw = yaw_to(way.x, way.z);
            } else {
                b.has_shade = false;   // got as near as it can: not planned every tick
            }
        } else if (const Target *near = nearest(targets, p, 4.0f); tame != nullptr && near) {
            // A pet with somebody close by stops and looks at them, which is
            // what lets them feed it.
            if (std::fabs(near->x - p.x) + std::fabs(near->z - p.z) > 1e-3f)
                look.yaw = yaw_to(near->x - p.x, near->z - p.z);
        } else {
            // Nobody about: now and then it strolls a little way and stops.
            if (now >= b.rethink_at) {
                const uint32_t h = (uint32_t)e.id() * 2654435761U ^ (uint32_t)(now * 10.0);
                b.strolling  = unit(h) < 0.4f;
                b.heading    = unit(h + 1) * 6.2831853f;
                b.rethink_at = now + 2.0 + 4.0 * unit(h + 2);
            }
            if (b.strolling) {
                want_x = std::sin(b.heading);
                want_z = -std::cos(b.heading);
                look.yaw = b.heading;
                speed *= 0.4f;
            }
        }

        if (wild && now < b.love_until && k.hearts_model != 0 && b.hearts == 0)
            new_hearts.push_back({e.id(), k.hearts_model});

        // Going round something it walked into: sideways for a moment, then
        // straight at them again. Not a path, and it does not need to be one
        // for a pillar or a tree; a maze keeps a zombie out.
        // ponytail: no pathfinding; A* over the cells when creatures need to
        // find their way through a cave.
        if (now < b.detour_until && (want_x != 0.0f || want_z != 0.0f)) {
            want_x = b.detour_x;
            want_z = b.detour_z;
        }

        // A shut door where its way goes, for a kind that opens them; and the
        // one it opened, shut behind it once it has stood in its cell and is
        // two nodes past it -- or six from it, having gone elsewhere -- and
        // nobody, player or creature, stands in it. Not before: a body pushed
        // sideways at the doorway and two nodes off shut it in its own face,
        // opened it again, and stood there doing that. A door somebody left
        // open on its way it shuts behind it the same, as the classic block game's
        // villagers do.
        if (way.door || (way.open_door && !b.door_opened)) {
            if (way.door) doors.push_back(way.door_at);
            b.door_opened = true;
            b.door_passed = false;
            b.door_at     = way.door_at;
        } else if (b.door_opened) {
            const PathCell &d  = b.door_at;
            const auto     in = [&](float x, float y, float z) {
                return (int)std::floor(x) == d.x && (int)std::floor(z) == d.z &&
                       std::abs(std::floor(y) - (float)d.y) <= 1.0f;
            };
            if (in(p.x, p.y, p.z)) b.door_passed = true;
            const float dx = (float)d.x + 0.5f - p.x, dz = (float)d.z + 0.5f - p.z;
            const float d2 = dx * dx + dz * dz;
            if ((b.door_passed && d2 > 4.0f) || d2 > 36.0f) {
                bool someone = false;
                for (const Target &t : targets) someone = someone || in(t.x, t.y, t.z);
                for (const Near &n : crowd) someone = someone || (n.id != e.id() && in(n.x, n.y, n.z));
                if (!someone) {
                    if (reg.get(world.get_block(d.x, d.y, d.z)).door == 2) doors.push_back(d);
                    b.door_opened = false;
                }
            }
        }

        // Falling, or swimming up out of water, or on a ladder: up or down it
        // as its way says, and holding on otherwise.
        const auto box    = [&](float y) {
            return AABB{Vec3{p.x - b.half, y, p.z - b.half}, Vec3{p.x + b.half, y + b.height, p.z + b.half}};
        };
        const bool wet    = box_in_liquid(world, reg, box(p.y + 0.2f));
        const bool ladder = box_on_climbable(world, reg, box(p.y));
        if (k.hover > 0.0f) {
            // Flying: up or down towards its height over whoever it is after,
            // and holding where it is with nobody about.
            const float want_y = goal ? goal->y + k.hover : p.y;
            b.vy = std::clamp((want_y - p.y) * 1.5f, -3.0f, 3.0f);
        } else if (ladder) {
            b.vy = way.vertical > 0 ? kClimbUp : way.vertical < 0 ? -kClimbUp : -kClimbSlide;
        } else {
            b.vy += (wet ? -4.0f : kGravity) * dt;
            if (wet && b.vy < 2.0f && (want_x != 0.0f || want_z != 0.0f || goal)) b.vy = 2.0f;
            if (b.vy < -kTerminalFallSpeed) b.vy = -kTerminalFallSpeed;
        }

        // Under the open sky by day: a point a second until it is gone, as it
        // came out of the dark. Not in water.
        if (fights(k) && daylight > 0.5f && !wet &&
            sun_light(world.get_light(fx, (int)std::floor(p.y + 1.5f), fz)) == kMaxLight) {
            if (now >= b.burn_at) {
                if (b.burn_at > 0.0) {
                    ecs::hurt(health, 1);
                    e.modified<Health>();
                }
                b.burn_at = now + 1.0;
            }
        } else {
            b.burn_at = 0.0;
        }

        // Knocked back: that speed on top of its own, spent on the ground.
        // Out of each other: pushed away from every creature it stands in,
        // harder the deeper, the way the classic block game's crowd themselves apart.
        float push_x = 0.0f, push_z = 0.0f;
        const int32_t mx = cell_of(p.x), mz = cell_of(p.z);
        for (int32_t cx = mx - 1; cx <= mx + 1; ++cx) {
            for (int32_t cz = mz - 1; cz <= mz + 1; ++cz) {
                const auto in = cells.find(key(cx, cz));
                if (in == cells.end()) continue;
                for (const uint32_t i : in->second) {
                    const Near &n = crowd[i];
                    if (n.id == e.id() || p.y >= n.y + n.height || n.y >= p.y + b.height) continue;
                    const float dx = p.x - n.x, dz = p.z - n.z, reach = b.spread + n.spread;
                    const float d2 = dx * dx + dz * dz;
                    if (d2 >= reach * reach) continue;
                    const float d = std::sqrt(d2);
                    // Standing on the same spot: apart by who is who.
                    const float ux = d > 1e-3f ? dx / d : (e.id() < n.id ? 1.0f : -1.0f);
                    const float uz = d > 1e-3f ? dz / d : 0.0f;
                    push_x += ux * (reach - d) * kCrowdPush;
                    push_z += uz * (reach - d) * kCrowdPush;
                }
            }
        }
        // On a belt, carried as a player is.
        const Belt belt = b.vy <= 0.0f ? belt_under(world, reg, p.x, p.y, p.z) : Belt{};
        const Vec3 delta{(want_x * speed + b.kx + push_x + belt.dx * belt.speed) * dt, b.vy * dt,
                         (want_z * speed + b.kz + push_z + belt.dz * belt.speed) * dt};
        const MoveResult r = move_body(world, reg, Vec3{p.x, p.y, p.z}, delta, 0.0f,
                                       kStepHeight, b.half, b.height);

        // Blocked sideways on the ground: jump the node in the way.
        const float moved = std::fabs(r.pos.x - p.x) + std::fabs(r.pos.z - p.z);
        const float meant = std::fabs(delta.x) + std::fabs(delta.z);
        if (r.on_ground) {
            b.vy = 0.0f;
            const float grip = std::max(0.0f, 1.0f - 8.0f * dt);
            b.kx *= grip;
            b.kz *= grip;
        }
        if (r.hit_ceiling && b.vy > 0.0f) b.vy = 0.0f;
        const bool blocked = meant > 1e-4f && moved < meant * 0.3f;
        if (r.on_ground && !ladder && k.hover <= 0.0f && (blocked || way.jump)) b.vy = kJump;
        // Swimming into a bank: up over its edge, as a player climbs out.
        else if (wet && !ladder && k.hover <= 0.0f && (blocked || way.jump)) b.vy = std::max(b.vy, kJump);

        // Still blocked half a second on, jumping or not: a wall it cannot
        // climb. Off the middle of its cell -- a corner of its way taken a
        // little wide, so its side is against the corner of the wall -- back
        // to the middle first: sent sideways from there, in a corridor, it
        // went back the way it came, and round again, for good. In the
        // middle already, round it, to one side or the other by who it is.
        if (!blocked) {
            b.stuck_since = -1.0;
        } else if (b.stuck_since < 0.0) {
            b.stuck_since = now;
        } else if (now - b.stuck_since > 0.5 && now >= b.detour_until) {
            const float mx = std::floor(p.x) + 0.5f - p.x, mz = std::floor(p.z) + 0.5f - p.z;
            const float off = std::sqrt(mx * mx + mz * mz);
            if (off > 0.12f) {
                b.detour_x     = mx / off;
                b.detour_z     = mz / off;
                b.detour_until = now + std::min(0.6, (double)off / std::max(speed, 0.5f) + 0.05);
            } else {
                const float side = (e.id() & 1) ? 1.0f : -1.0f;
                b.detour_x     = -want_z * side;
                b.detour_z     = want_x * side;
                b.detour_until = now + 1.2;
            }
            b.stuck_since  = -1.0;
        }

        p.x = r.pos.x;
        p.y = r.pos.y;
        p.z = r.pos.z;
        p.valid = true;

        // Swinging or drawing: that is what the Working tag says to a client.
        const bool busy = now < b.busy_until || (tame != nullptr && tame->sitting);
        if (busy != e.has<Working>()) {
            if (busy) e.add<Working>();
            else      e.remove<Working>();
        }
        const bool burning = b.burn_at > 0.0;
        if (burning != e.has<Burning>()) {
            if (burning) e.add<Burning>();
            else         e.remove<Burning>();
        }
        e.modified<Position>();
    });

    w.defer_end();

    for (const Shot &s : shots)
        loose_arrow(w, s.from, s.at, s.damage, s.model, now, s.fireball, s.bare, s.shooter);
    for (const Blow &bl : blows) {
        const flecs::entity t = w.entity(bl.target);
        if (t.is_alive() && t.has<Health>() && t.has<Position>())
            struck(t, bl.damage, bl.x, bl.z, t.get<Position>());
    }
    for (const Birth &n : births) {
        for (const flecs::entity_t id : {n.a, n.b}) {
            flecs::entity e = w.entity(id);
            if (!e.is_alive()) continue;
            Brain &b      = e.get_mut<Brain>();
            b.love_until  = 0.0;
            b.breed_again = now + kBreedAgain;
        }
        spawn_young(w, kinds, n.kind, n.x, n.y, n.z, now, n.a);
        std::printf("        creatures: a young %s born at %.1f %.1f %.1f\n",
                    kinds[n.kind].name.c_str(), (double)n.x, (double)n.y, (double)n.z);
    }
    // Hearts over whoever came to be in love, and every heart kept over its
    // one, a little up and down and turning; gone with the love, or with it.
    for (const auto &[id, model] : new_hearts) {
        Position hp{};
        hp.valid = true;
        const flecs::entity h = w.entity().set<Position>(hp).set<Look>({0.0f, 0.0f})
                                    .set<Hearts>({id}).set<Wears>({model}).add<Thing>().add<Body>();
        w.entity(id).get_mut<Brain>().hearts = h.id();
    }
    std::vector<flecs::entity_t> heartbroken;
    w.each([&](flecs::entity h, Position &hp, Look &look, const Hearts &of) {
        const flecs::entity e = w.entity(of.of);
        const Position     *p = e.is_alive() ? e.try_get<Position>() : nullptr;
        const Brain        *b = e.is_alive() ? e.try_get<Brain>() : nullptr;
        if (p == nullptr || b == nullptr || now >= b->love_until || b->hearts != h.id()) {
            heartbroken.push_back(h.id());
            return;
        }
        hp.x = p->x;
        hp.y = p->y + b->height + 0.15f + 0.08f * (float)std::sin(now * 3.0);
        hp.z = p->z;
        look.yaw = (float)std::fmod(now * 2.0, 6.2831853);
        h.modified<Position>();
    });
    for (const flecs::entity_t id : heartbroken) {
        const flecs::entity h  = w.entity(id);
        const flecs::entity_t of = h.get<Hearts>().of;
        if (w.entity(of).is_alive())
            if (Brain *b = w.entity(of).try_get_mut<Brain>(); b && b->hearts == id) b->hearts = 0;
        h.destruct();
    }
    for (const Laid &l : laid) {
        ItemStack stack;
        stack.item  = l.item;
        stack.count = 1;
        spawn_drop(w, stack, l.x, l.y, l.z, now);
    }
    if (swap_door)
        for (const PathCell &d : doors) swap_door(d.x, d.y, d.z);
    std::vector<ArrowStrike> strikes;
    step_arrows(w, world, reg, targets, dt, now, hurt, strikes);
    for (const ArrowStrike &k : strikes) {
        const flecs::entity e = w.entity(k.target);
        if (e.is_alive() && e.has<Health>()) struck(e, k.damage, k.from.x, k.from.z, e.get<Position>());
    }
}

SpawnResult spawn_and_despawn(flecs::world &w, const World &world, const NodeRegistry &reg,
                              const std::vector<MobKind> &kinds,
                              const std::vector<Target> &targets, float daylight, int cap,
                              uint32_t seed, double now)
{
    SpawnResult out;

    // Gone first: any creature no player is near.
    std::vector<flecs::entity_t> far;
    w.each([&](flecs::entity e, const Position &p, const Brain &) {
        // A pet with an owner is theirs, near or not.
        // ponytail: kept for as long as the server runs, and not saved with
        // the world; a Tame in the save when pets must outlive a restart.
        if (const Tame *tame = e.try_get<Tame>(); tame && !tame->owner.empty()) return;
        if (e.has<Act>()) return;   // a worker is its mod's to remove
        for (const Target &t : targets) {
            const float dx = t.x - p.x, dy = t.y - p.y, dz = t.z - p.z;
            if (dx * dx + dy * dy + dz * dz < 72.0f * 72.0f) return;
        }
        far.push_back(e.id());
    });
    for (const flecs::entity_t id : far) w.entity(id).destruct();
    out.went = (int)far.size();

    if (kinds.empty()) return out;

    for (size_t ti = 0; ti < targets.size(); ++ti) {
        const Target &t = targets[ti];

        // Counted apart: the animals and pets that came by day do not keep
        // the night's creatures away, nor the night's them.
        int near = 0, pets = 0;
        w.each([&](flecs::entity e, const Position &p, const Brain &) {
            const float dx = t.x - p.x, dy = t.y - p.y, dz = t.z - p.z;
            if (dx * dx + dy * dy + dz * dz >= 48.0f * 48.0f) return;
            if (e.has<Act>()) return;   // a worker is neither
            if (e.has<Tame>()) ++pets;
            else               ++near;
        });

        for (uint32_t tries = 0; tries < 4; ++tries) {
            const uint32_t h     = seed * 7919U + (uint32_t)ti * 104729U + tries * 31U;
            const float    angle = unit(h) * 6.2831853f;
            const float    r     = 18.0f + 22.0f * unit(h + 1);
            const int      x     = (int)std::floor(t.x + std::cos(angle) * r);
            const int      z     = (int)std::floor(t.z + std::sin(angle) * r);

            // Down a column from above the player to below: the first floor
            // with two open cells over it.
            for (int y = (int)t.y + 12; y > (int)t.y - 16; --y) {
                if (!solid(world, reg, x, y - 1, z)) continue;
                if (!open(world, reg, x, y, z) || !open(world, reg, x, y + 1, z)) continue;

                const uint8_t light = world.get_light(x, y, z);
                const float   lit   = std::max((float)sun_light(light) * daylight,
                                                (float)block_light(light));
                // The dark's creatures in the dark, pets in the sun: one of the
                // kinds that fit the light, and none in the dusk between.
                const bool pet = (float)sun_light(light) * daylight >= 12.0f;
                if (pet ? pets >= cap : lit > 7.0f || near >= cap) break;
                std::vector<uint16_t> fit;
                for (size_t i = 0; i < kinds.size(); ++i)
                    if (pet ? kinds[i].fight == MobKind::Fight::Pet : fights(kinds[i]))
                        fit.push_back((uint16_t)i);
                if (fit.empty()) break;
                const uint16_t kind = fit[mix(h + 2) % fit.size()];

                // A kind that comes out only on one block waits for it.
                if (!kinds[kind].spawn_on.empty() &&
                    reg.get(world.get_block(x, y - 1, z)).name != kinds[kind].spawn_on)
                    break;
                spawn_mob(w, kinds, kind, (float)x + 0.5f, (float)y, (float)z + 0.5f, now);
                ++out.came;
                tries = 4;
                break;
            }
        }
    }
    return out;
}

size_t reap_dead(flecs::world &w, double now)
{
    // Collected first and destroyed after, rather than destroyed inside the
    // walk: spawning a drop CREATES an entity, and creating one during an
    // each() is how a table gets moved out from under the iterator.
    std::vector<flecs::entity_t> dead;
    w.each([&](flecs::entity e, const Health &h, Mob) {
        if (!h.alive()) dead.push_back(e.id());
    });

    for (const flecs::entity_t id : dead) {
        flecs::entity        e    = w.entity(id);
        const Position      *p    = e.try_get<Position>();
        const Drops         *left = e.try_get<Drops>();

        if (const Brain *b = e.try_get<Brain>(); b != nullptr && p != nullptr)
            voice(w, b->kind, "death", *p);

        if (p != nullptr && left != nullptr && left->item != kNoItem && left->count > 0) {
            ItemStack stack;
            stack.item  = left->item;
            stack.count = left->count;

            // From the middle of the body and not from its feet, so the stack
            // falls the last half metre the way any other drop does instead of
            // starting already resting on whatever the mob was standing on.
            spawn_drop(w, stack, p->x, p->y + kMobHeight * 0.5f, p->z, now);
        }
        if (p != nullptr && left != nullptr && left->also != kNoItem) {
            ItemStack stack;
            stack.item  = left->also;
            stack.count = 1;
            spawn_drop(w, stack, p->x, p->y + kMobHeight * 0.5f, p->z, now);
        }
        if (p != nullptr && left != nullptr && left->extra != kNoItem && left->extra_max > 0) {
            // 0 to extra_max, from the entity and the clock and not a
            // generator: nothing else here draws a number for a death.
            const uint64_t h = (uint64_t)id * 0x9E3779B97F4A7C15ull ^ (uint64_t)(now * 1000.0);
            ItemStack stack;
            stack.item  = left->extra;
            stack.count = (uint16_t)((h >> 33) % (uint64_t)(left->extra_max + 1));
            if (stack.count > 0)
                spawn_drop(w, stack, p->x, p->y + kMobHeight * 0.5f, p->z, now);
        }

        e.destruct();
    }

    return dead.size();
}

} // namespace cnp::ecs
