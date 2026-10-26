#include "core/ecs/drop.h"

#include <algorithm>
#include <cmath>

#include "core/ecs/components.h"
#include "core/ecs/mob.h"
#include "core/physics/aabb.h"
#include "core/physics/movement.h"
#include "core/registry/node_shape.h"

namespace cnp::ecs {

namespace {

// Whether the node at this point stops a falling stack. Asked of the node
// registry rather than of World::is_solid, which answers "not air": a mod can
// register something a player walks through, and a stack should fall through it
// too rather than resting on a pane of glass the player is standing inside.
//
// A block with a shape (core.node_box) stops it only inside one of its boxes,
// and `top` is where it then rests: on a slab's top half, not on the cell.
bool solid_at(const World &world, const NodeRegistry &nodes, float x, float y, float z, float &top)
{
    const int     bx = (int)std::floor(x);
    const int     by = (int)std::floor(y);
    const int     bz = (int)std::floor(z);
    const BlockId id = world.get_block(bx, by, bz);
    if (!nodes.is_solid(id)) return false;

    const NodeDef &def = nodes.get(id);
    if (!node_has_shape(def)) {
        top = (float)by + 1.0f;
        return true;
    }
    const uint8_t facing = world.get_facing(bx, by, bz);
    bool          hit    = false;
    for (int i = 0; i < node_shape_count(def); ++i) {
        const NodeBox nb = node_shape_box(def, facing, i, bx, by, bz);
        if (x >= nb.min[0] && x <= nb.max[0] && z >= nb.min[2] && z <= nb.max[2] &&
            y >= nb.min[1] && y < nb.max[1]) {
            top = hit ? std::max(top, nb.max[1]) : nb.max[1];
            hit = true;
        }
    }
    return hit;
}

} // namespace

AABB drop_box(float x, float y, float z)
{
    const float h = kDropSize * 0.5f;
    return AABB{Vec3{x - h, y - h, z - h}, Vec3{x + h, y + h, z + h}};
}

flecs::entity spawn_drop(flecs::world &w, const ItemStack &what, float x, float y, float z,
                         double now)
{
    Position p;
    p.x     = x;
    p.y     = y;
    p.z     = z;
    p.valid = true;

    Dropped d;
    d.stack     = what;
    d.arm_at    = now + kDropArming;
    d.expire_at = now + kDropLifetime;

    // Body from the start, like a wanderer and unlike a player: there is no
    // handshake to wait for and nowhere else for it to be. Look too, because
    // the streaming query asks for one -- a drop that had no Look would be a
    // body nobody could see, which is a strange way to find out that the query
    // is really three components and not one tag.
    return w.entity().set<Position>(p).set<Look>({}).set<Dropped>(d).add<Body>();
}

flecs::entity spawn_falling(flecs::world &w, BlockId block, uint8_t facing, int x, int y, int z)
{
    Position p;
    p.x     = (float)x + 0.5f;
    p.y     = (float)y + 0.5f;
    p.z     = (float)z + 0.5f;
    p.valid = true;
    return w.entity().set<Position>(p).set<Look>({}).set<Falling>({block, facing, 0.0f, p.y}).add<Body>();
}

void step_falling(flecs::world &w, const World &world, const NodeRegistry &nodes, float dt,
                  const std::function<void(int, int, int, BlockId, uint8_t, int)> &land)
{
    if (dt <= 0.0f) return;
    struct Landing {
        flecs::entity_t e;
        int             x, y, z;
        BlockId         block;
        uint8_t         facing;
        int             fell;
    };
    std::vector<Landing> landed;
    w.each([&](flecs::entity e, Position &p, Falling &f) {
        f.vy += kGravity * dt;
        if (f.vy < -kTerminalFallSpeed) f.vy = -kTerminalFallSpeed;
        const float want = p.y + f.vy * dt;
        // The bottom face against the floor: a whole cell's box, so it stops on
        // the top of whatever its bottom reaches, as a drop does.
        float top = 0.0f;
        if (solid_at(world, nodes, p.x, want - 0.5f, p.z, top) || want - 0.5f < 0.0f) {
            const int cy = (int)std::floor(std::max(top, 0.0f) + 0.01f);
            landed.push_back({e.id(), (int)std::floor(p.x), cy, (int)std::floor(p.z), f.block, f.facing,
                              (int)std::floor(f.from_y)});
            return;
        }
        p.y = want;
        e.modified<Position>();
    });
    // The lowest first, and each on top of what landed before it: two of a
    // column that came down in the same step do not share a cell.
    std::sort(landed.begin(), landed.end(), [](const Landing &a, const Landing &b) { return a.y < b.y; });
    for (Landing &l : landed) {
        const int from = l.fell;   // the cell it started in, carried here
        while (l.y < from && nodes.is_solid(world.get_block(l.x, l.y, l.z))) ++l.y;
        w.entity(l.e).destruct();
        land(l.x, l.y, l.z, l.block, l.facing, from - l.y);
    }
}

void step_drops(flecs::world &w, const World &world, const NodeRegistry &nodes, float dt)
{
    if (dt <= 0.0f) return;

    w.each([&](flecs::entity e, Position &p, Dropped &d) {
        const float half = kDropSize * 0.5f;

        if (d.resting) {
            // On a belt (NodeDef::conveyor): carried the way its cell runs and
            // drawn to its middle across, which is what takes a stack round a
            // corner -- the next cell runs the other way and the pull does the
            // rest -- and held on its top, which is what takes it up a ramp.
            const Belt b = belt_under(world, nodes, p.x, p.y - half, p.z);
            // On a belt, or lying on a block that keeps what is on it (a
            // sorter), a stack does not grow older: it is on its way somewhere.
            if (b.on || nodes.get(world.get_block((int)std::floor(p.x), (int)std::floor(p.y - half - 0.05f),
                                                  (int)std::floor(p.z))).keeps_drops)
                d.expire_at += (double)dt;
            if (!b.on) return;
            const float step = b.speed * dt;
            float       nx   = p.x + b.dx * step;
            float       nz   = p.z + b.dz * step;
            if (b.dx == 0.0f) nx = p.x + std::clamp((float)b.x + 0.5f - p.x, -step, step);
            else              nz = p.z + std::clamp((float)b.z + 0.5f - p.z, -step, step);

            // A wall ahead, in the next column (its own is the ramp it is on),
            // asked near the stack's top: up a ramp, the belt it climbs onto is
            // above the middle of a stack still on the slope.
            float       top  = 0.0f;
            const float lx   = nx + b.dx * half, lz = nz + b.dz * half;
            const bool  away = (int)std::floor(lx) != b.x || (int)std::floor(lz) != b.z;
            if (away && solid_at(world, nodes, lx, p.y + half * 0.9f, lz, top)) return;
            p.x = nx;
            p.z = nz;
            e.modified<Position>();
            const Belt next = belt_under(world, nodes, p.x, p.y - half, p.z);
            if (next.on) p.y = next.surface + half;
            // Off the end, or onto a lower floor: it falls from here.
            else if (!solid_at(world, nodes, p.x, p.y - half - 0.01f, p.z, top)) d.resting = false;
            return;
        }

        d.vy += kGravity * dt;
        if (d.vy < -kTerminalFallSpeed) d.vy = -kTerminalFallSpeed;

        const float want = p.y + d.vy * dt;

        // Only downward, and only against the node the bottom face would end
        // up in. A stack does not need a collision resolver: it falls, and it
        // stops on the first thing under it. Sideways it never moves, so there
        // is nothing to push it into a wall.
        float top = 0.0f;
        if (d.vy < 0.0f && solid_at(world, nodes, p.x, want - half, p.z, top)) {
            // Resting ON the block, not inside it: the top of what the bottom
            // face landed in, plus the half-height.
            p.y       = top + half;
            d.vy      = 0.0f;
            d.resting = true;
            return;
        }

        p.y = want;

        // Out of the bottom of the world. Nothing catches it and nothing should
        // simulate it forever, so it stops falling where the world ends. A drop
        // that fell into a hole is a drop the player lost, and it is visible
        // sitting there rather than gone.
        if (p.y < 0.0f) {
            p.y       = 0.0f;
            d.vy      = 0.0f;
            d.resting = true;
        }
    });
}

std::vector<flecs::entity_t> collect_drops(flecs::world &w, const ItemRegistry &items, double now)
{
    std::vector<flecs::entity_t> changed;

    // Who can carry something: an entity with a bag. That is the query and not
    // "the players", because a chest that walks would satisfy it too and this
    // has no business knowing which is which.
    std::vector<flecs::entity> carriers;
    w.each([&](flecs::entity e, const Position &, Inventory &) { carriers.push_back(e); });
    if (carriers.empty()) return changed;

    std::vector<flecs::entity> spent;

    w.each([&](flecs::entity drop, const Position &p, Dropped &d) {
        if (now < d.arm_at || d.stack.empty()) return;

        const AABB box = drop_box(p.x, p.y, p.z);

        for (flecs::entity who : carriers) {
            const Position &cp = who.get<Position>();
            if (!cp.valid) continue;
            if (!boxes_overlap(box, player_box(Vec3{cp.x, cp.y, cp.z}))) continue;

            Inventory &bag = who.get_mut<Inventory>();

            const uint16_t left = bag.add(items, d.stack.item, d.stack.count, -1, d.stack.used, d.stack.ench,
                                            d.stack.tag);
            if (left == d.stack.count) continue;   // no room; leave it where it is

            d.stack.count = left;
            changed.push_back(who.id());

            if (d.stack.count == 0) {
                spent.push_back(drop);
                break;
            }
        }
    });

    // A player's arrow that has landed is picked up the same way, one at a time.
    w.each([&](flecs::entity arrow, const Position &p, const Arrow &a) {
        if (a.landed_at < 0.0 || a.item == 0) return;
        AABB box = drop_box(p.x, p.y, p.z);   // grown: it is reached for, not walked into
        box.min = box.min - Vec3{0.5f, 0.5f, 0.5f};
        box.max = box.max + Vec3{0.5f, 0.5f, 0.5f};
        for (flecs::entity who : carriers) {
            const Position &cp = who.get<Position>();
            if (!cp.valid || !boxes_overlap(box, player_box(Vec3{cp.x, cp.y, cp.z}))) continue;
            if (who.get_mut<Inventory>().add(items, a.item, 1) != 0) continue;   // no room
            changed.push_back(who.id());
            spent.push_back(arrow);
            break;
        }
    });

    // Destroyed after the iteration and not during it: deleting an entity out
    // of the query that is walking over it is how a table gets rearranged
    // underneath a loop.
    for (flecs::entity e : spent) e.destruct();

    // One avatar can empty two drops in a tick, and its owner does not need to
    // be told twice.
    std::sort(changed.begin(), changed.end());
    changed.erase(std::unique(changed.begin(), changed.end()), changed.end());
    return changed;
}

namespace {

flecs::entity first_in_cell(flecs::world &w, int x, int y, int z)
{
    flecs::entity found;
    w.each([&](flecs::entity e, const Position &p, const Dropped &d) {
        if (found || d.stack.empty()) return;
        if ((int)std::floor(p.x) == x && (int)std::floor(p.y) == y && (int)std::floor(p.z) == z) found = e;
    });
    return found;
}

} // namespace

ItemId drop_in_cell(flecs::world &w, int x, int y, int z)
{
    const flecs::entity e = first_in_cell(w, x, y, z);
    return e ? e.get<Dropped>().stack.item : kNoItem;
}

ItemId send_drop(flecs::world &w, int x, int y, int z, float tx, float ty, float tz)
{
    flecs::entity e = first_in_cell(w, x, y, z);
    if (!e) return kNoItem;
    Position &p = e.get_mut<Position>();
    p.x = tx;
    p.y = ty;
    p.z = tz;
    e.modified<Position>();
    Dropped &d = e.get_mut<Dropped>();
    d.vy      = 0.0f;
    d.resting = false;   // it falls onto whatever is there
    return d.stack.item;
}

int drop_from(flecs::world &w, Inventory &bag, int count, float tx, float ty, float tz, double now)
{
    if (count <= 0) return 0;
    for (int i = 0; i < bag.size(); ++i) {
        if (bag.at(i).empty() || !((kRolesOut >> bag.role(i)) & 1u)) continue;   // what may come out
        const ItemStack out = bag.take_from(i, (uint16_t)std::min(count, 65535));
        if (out.empty()) return 0;
        spawn_drop(w, out, tx, ty, tz, now);
        return (int)out.count;
    }
    return 0;
}

size_t expire_drops(flecs::world &w, double now)
{
    // Collected first and destroyed after, because destroying an entity from
    // inside the iteration over the query that found it is the one thing flecs
    // asks callers not to do. collect_drops takes the same care for the same
    // reason.
    std::vector<flecs::entity_t> done;
    w.each([&](flecs::entity e, const Dropped &d) {
        if (now >= d.expire_at) done.push_back(e.id());
    });

    for (const flecs::entity_t id : done) w.entity(id).destruct();
    return done.size();
}

} // namespace cnp::ecs
