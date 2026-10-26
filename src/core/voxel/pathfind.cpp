#include "core/voxel/pathfind.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>

#include "core/registry/node_registry.h"
#include "core/registry/node_shape.h"
#include "core/voxel/chunk.h"

namespace cnp {

namespace {

static_assert(kChunkSize == 32, "the reader below shifts by 5");

// What a block is to a body, read from its shape TURNED the way its cell is
// (node_shape_box, the same boxes a body collides with), once a search for
// each block and orientation met.
//
// The floor of a cell is read by QUARTER: in each of its four quarters, how
// high the boxes stacked from the bottom reach (a slab 0.5 all over; a stair
// 0.5 at the front and 1 at the back), and where the lowest box that does not
// stand on that stack begins (an upside-down slab: a ceiling at 0.5). Coming
// in over one side, the feet meet that side's quarters first -- which is what
// makes a stair two half steps from the front and a wall of one from the side.
enum class Kind : uint8_t { Open, Shaped, Full, Post, Water, DoorShut, DoorOpen };

struct Info {
    Kind  kind  = Kind::Open;
    float q[4]  = {0, 0, 0, 0};   // floor per quarter: x<.5 z<.5, x>.5 z<.5, x<.5 z>.5, x>.5 z>.5
    float top   = 0.0f;           // the highest of the four
    float ceil  = 1.0f;           // the lowest box not on the floor's stack
    float u[4]  = {0, 0, 0, 0};   // what is stood on from the cell above: the top of any box
    float utop  = 0.0f;
    bool  climb = false;
    uint8_t sides = 0;            // a panel along a side: 1 -x, 2 +x, 4 -z, 8 +z
};

Info classify(const NodeDef &d, uint8_t facing)
{
    Info i;
    i.climb = d.climbable;
    if (d.door == 1) { i.kind = Kind::DoorShut; return i; }
    if (d.door == 2) { i.kind = Kind::DoorOpen; return i; }
    if (!d.solid) { i.kind = d.liquid ? Kind::Water : Kind::Open; return i; }
    if (d.parts.empty()) {
        i.kind = Kind::Full;
        for (float &v : i.q) v = 1.0f;
        for (float &v : i.u) v = 1.0f;
        i.top = i.utop = 1.0f, i.ceil = 0.0f;
        return i;
    }

    std::vector<NodeBox> boxes;
    for (size_t b = 0; b < d.parts.size(); ++b)
        boxes.push_back(node_shape_box(d, facing, (int)b, 0, 0, 0));
    std::sort(boxes.begin(), boxes.end(),
              [](const NodeBox &a, const NodeBox &b) { return a.min[1] < b.min[1]; });

    // A box counts in a quarter when it covers at least half of it: a
    // ladder's panel or a door, an eighth of a cell deep, covers none, and a
    // body fits beside it.
    for (int qi = 0; qi < 4; ++qi) {
        const float x0 = (qi & 1) ? 0.5f : 0.0f, z0 = (qi & 2) ? 0.5f : 0.0f;
        float floor = 0.0f;
        for (const NodeBox &bx : boxes) {
            const float ox = std::min(bx.max[0], x0 + 0.5f) - std::max(bx.min[0], x0);
            const float oz = std::min(bx.max[2], z0 + 0.5f) - std::max(bx.min[2], z0);
            if (ox <= 0.0f || oz <= 0.0f || ox * oz < 0.125f) continue;
            if (bx.min[1] <= floor + 0.05f) floor = std::max(floor, bx.max[1]);
            else i.ceil = std::min(i.ceil, bx.min[1]);
            i.u[qi] = std::max(i.u[qi], std::min(bx.max[1], 1.0f));
        }
        i.q[qi] = std::min(floor, 1.0f);
        i.top   = std::max(i.top, i.q[qi]);
        i.utop  = std::max(i.utop, i.u[qi]);
    }
    // A panel along a side, across all of it and taller than a step: a
    // ladder's, an open trapdoor's. It covers no quarter, but a body does not
    // cross that side through it.
    for (const NodeBox &bx : boxes) {
        if (bx.max[1] - bx.min[1] < 0.5f) continue;
        if (bx.max[2] - bx.min[2] > 0.8f && bx.max[0] - bx.min[0] < 0.25f) {
            if (bx.min[0] < 0.05f) i.sides |= 1;
            if (bx.max[0] > 0.95f) i.sides |= 2;
        }
        if (bx.max[0] - bx.min[0] > 0.8f && bx.max[2] - bx.min[2] < 0.25f) {
            if (bx.min[2] < 0.05f) i.sides |= 4;
            if (bx.max[2] > 0.95f) i.sides |= 8;
        }
    }
    const bool cube = i.q[0] > 0.999f && i.q[1] > 0.999f && i.q[2] > 0.999f && i.q[3] > 0.999f;
    i.kind = cube ? Kind::Full : (i.top > 0.0f || i.ceil < 1.0f) ? Kind::Shaped : Kind::Open;
    // Thin, but across the middle and above a step: a fence post, a pane. A
    // body in the cell would stand in it, and one going by cuts its corner
    // and is held there against it -- a wall, as the classic block game's mobs read one.
    if (i.kind == Kind::Open && !d.climbable)
        for (const NodeBox &bx : boxes)
            if (bx.max[1] > 0.6f && bx.min[1] < 1.0f && bx.min[0] < 0.8f && bx.max[0] > 0.2f &&
                bx.min[2] < 0.8f && bx.max[2] > 0.2f) {
                i.kind = Kind::Post;
                break;
            }
    if (cube) i.ceil = 0.0f;
    return i;
}

// Where a standing body's feet are and what they stand on: the floor of each
// quarter under it, absolute heights, and the highest of them.
struct Floor {
    float q[4] = {0, 0, 0, 0};
    float f    = 0.0f;
    bool  ground = false;   // held by that floor, not by water or a ladder

    // The height the feet meet first coming in from (dx, dz): the higher of
    // the quarters on the side facing where they come from.
    float entry(int dx, int dz) const
    {
        float e = -1e30f;
        for (int qi = 0; qi < 4; ++qi) {
            if (dx > 0 && (qi & 1)) continue;   // from -x: the quarters at x < .5
            if (dx < 0 && !(qi & 1)) continue;
            if (dz > 0 && (qi & 2)) continue;
            if (dz < 0 && !(qi & 2)) continue;
            e = std::max(e, q[qi]);
        }
        return e;
    }
};

// The blocks, read for one search. A search reads the same few chunks tens of
// thousands of times, and World::get_block finds the chunk in a hash map every
// time: a handful of chunk pointers kept here by position is most of the speed
// of this file (measured with the node table below: 1.03 to 0.35 us a cell
// looked at, 4.0 to 1.5 ms for a way 140 cells long through a field of pillars).
class Reader {
public:
    Reader(const World &world, const NodeRegistry &reg, const PathBody &body)
        : m_world(world), m_reg(reg), m_body(body)
    {
        // Every block's unturned shape now, and whether its turn matters: a
        // world has a few hundred kinds and most of them are cubes.
        m_infos.resize(reg.size() * 24);
        m_known.assign(reg.size() * 24, 0);
        m_turns.resize(reg.size());
        for (size_t b = 0; b < reg.size(); ++b) {
            const NodeDef &d = reg.get((BlockId)b);
            m_turns[b] = d.turn != NodeTurn::None && !d.parts.empty() && d.door == 0;
            m_infos[b * 24] = classify(d, 0);
            m_known[b * 24] = 1;
        }
    }

    const Info &at(int x, int y, int z)
    {
        static const Info kNothing{};
        const Chunk *c = chunk(x, y, z);
        const BlockId b = c ? c->get(x & 31, y & 31, z & 31) : kAir;
        if (b >= m_turns.size()) return kNothing;
        if (!m_turns[b]) return m_infos[(size_t)b * 24];
        const uint8_t facing = c->facing(x & 31, y & 31, z & 31);
        const int     o      = node_cell_orientation(m_reg.get(b).turn, facing);
        const size_t  k      = (size_t)b * 24 + (size_t)(o < 0 || o >= 24 ? 0 : o);
        if (!m_known[k]) {
            m_infos[k] = classify(m_reg.get(b), facing);
            m_known[k] = 1;
        }
        return m_infos[k];
    }

    // Whether the body, standing in column x z, meets nothing between heights
    // lo and hi (world units): no wall, no floor of a shaped block above lo,
    // no ceiling of one below hi, no water for a body that does not swim, no
    // shut door it cannot open.
    bool clear(int x, int z, float lo, float hi)
    {
        const int k0 = (int)std::floor(lo + 1e-4f), k1 = (int)std::ceil(hi - 1e-4f) - 1;
        for (int k = k0; k <= k1; ++k) {
            const Info &i = at(x, k, z);
            switch (i.kind) {
            case Kind::Full:
            case Kind::Post:     return false;
            case Kind::DoorShut: if (!m_body.opens_doors) return false; break;
            case Kind::Water:    if (!m_body.swim) return false; break;
            case Kind::Shaped:
                if (lo < (float)k + i.top - 1e-4f) return false;
                if (i.ceil < 1.0f && hi > (float)k + i.ceil + 1e-4f) return false;
                break;
            default:             break;
            }
        }
        return true;
    }

    // Whether the body can stand with its feet in this cell, and on what: a
    // shaped block in it lower than the cell (a slab), or the top of what is
    // under it -- a cube, or a shaped block that reaches the top (a stair,
    // whose front is lower than its back); or water, or a ladder.
    bool feet(int x, int y, int z, Floor &out)
    {
        const Info &here = at(x, y, z);
        bool held = false;
        if (here.kind == Kind::Full || here.kind == Kind::Post) return false;
        if (here.kind == Kind::DoorShut && !m_body.opens_doors) return false;
        if (here.kind == Kind::Shaped && here.top > 0.0f) {
            if (here.top > 0.999f) return false;   // stood on from the cell above
            for (int k = 0; k < 4; ++k) out.q[k] = (float)y + here.q[k];
            out.f = (float)y + here.top;
            held  = true;
        } else {
            const Info &below = at(x, y - 1, z);
            out.f = (float)y;
            // Stood on from above: a cube, or a shaped block whose boxes
            // reach the top -- a stair, a slab turned upside down.
            if (below.kind == Kind::Full || below.kind == Kind::DoorShut ||
                (below.kind == Kind::Shaped && below.utop > 0.999f)) {
                for (int k = 0; k < 4; ++k) out.q[k] = (float)(y - 1) + below.u[k];
                held = true;
            } else {
                for (float &v : out.q) v = (float)y;
            }
        }
        const bool wet = here.kind == Kind::Water && m_body.swim;
        // A ladder only when nothing else holds it: the two lookups are most
        // of what a cell on open ground would otherwise pay for nothing.
        if (!held && !wet &&
            !(m_body.climbs && (here.climb || at(x, y + 1, z).climb || at(x, y - 1, z).climb)))
            return false;
        out.ground = held && !wet;
        return clear(x, z, out.f, out.f + (float)m_body.height);
    }

    bool feet(int x, int y, int z, float &f, bool *ground = nullptr)
    {
        Floor fl;
        if (!feet(x, y, z, fl)) return false;
        f = fl.f;
        if (ground) *ground = fl.ground;
        return true;
    }

    // Whether a panel on a side of either cell stands between a body going
    // from (x, y, z) to the column (dx, dz) along, its feet at ny there.
    bool fenced(int x, int y, int z, int dx, int dz, int ny)
    {
        const int out = (dx > 0 ? 2 : dx < 0 ? 1 : 0) | (dz > 0 ? 8 : dz < 0 ? 4 : 0);
        const int in  = (dx > 0 ? 1 : dx < 0 ? 2 : 0) | (dz > 0 ? 4 : dz < 0 ? 8 : 0);
        for (int k = 0; k < m_body.height; ++k)
            if ((at(x, y + k, z).sides & out) || (at(x + dx, ny + k, z + dz).sides & in)) return true;
        return false;
    }

    bool ladder(int x, int y, int z)
    {
        return m_body.climbs && (at(x, y, z).climb || at(x, y + 1, z).climb);
    }

private:
    const Chunk *chunk(int x, int y, int z)
    {
        const ChunkPos cp{x >> 5, y >> 5, z >> 5};   // arithmetic shift: floor division
        const unsigned slot = ((unsigned)cp.x * 7u + (unsigned)cp.y * 3u + (unsigned)cp.z) & 15u;
        Cached &c = m_cache[slot];
        if (!c.valid || !(c.pos == cp)) {
            c.pos   = cp;
            c.chunk = m_world.find_chunk(cp);   // null: ungenerated space, read as air
            c.valid = true;
        }
        return c.chunk;
    }

    struct Cached {
        ChunkPos     pos;
        const Chunk *chunk = nullptr;
        bool         valid = false;
    };

    const World          &m_world;
    const NodeRegistry   &m_reg;
    const PathBody       &m_body;
    Cached                m_cache[16];
    std::vector<Info>     m_infos;   // by block * 24 + orientation
    std::vector<uint8_t>  m_known;
    std::vector<uint8_t>  m_turns;
};

uint64_t key(int x, int y, int z)
{
    // 21 bits each, offset so a negative coordinate is a positive field.
    return ((uint64_t)(uint32_t)(x + (1 << 20)) & 0x1FFFFF) << 42 |
           ((uint64_t)(uint32_t)(y + (1 << 20)) & 0x1FFFFF) << 21 |
           ((uint64_t)(uint32_t)(z + (1 << 20)) & 0x1FFFFF);
}

// Octile distance on the ground plus a little for every level: never more than
// the cost of getting there, which is what keeps A* shortest.
float estimate(const PathCell &a, const PathCell &b)
{
    const float dx = (float)std::abs(a.x - b.x), dz = (float)std::abs(a.z - b.z);
    const float dy = (float)std::abs(a.y - b.y);
    return std::max(dx, dz) + 0.41421356f * std::min(dx, dz) + 0.2f * dy;
}

// The cells a search has touched: the nodes in a dense list, and a table of
// where each one is by cell, open addressing, doubled when it is half full.
// Both kept between searches on this thread and emptied by a stamp rather
// than by clearing: a creature asks for a way several times a second, and a
// search is mostly the few hundred cells round it.
struct Node {
    uint64_t key    = 0;
    PathCell at;
    float    g      = 0.0f;
    uint32_t parent = 0;       // index in the node list
    bool     closed = false;
};

struct Table {
    struct Slot {
        uint64_t key   = 0;
        uint32_t node  = 0;
        uint32_t stamp = 0;    // which search wrote it; any other is an empty slot
    };
    std::vector<Node> nodes;
    std::vector<Slot> slots = std::vector<Slot>(4096);
    uint32_t          stamp = 0;

    void begin()
    {
        nodes.clear();
        if (++stamp == 0) {   // wrapped: every old stamp could collide, so start clean
            for (Slot &sl : slots) sl.stamp = 0;
            stamp = 1;
        }
    }

    uint32_t mask() const { return (uint32_t)slots.size() - 1; }

    // The slot for this key: the one holding it, or the empty one it would go in.
    uint32_t find(uint64_t k) const
    {
        uint32_t i = (uint32_t)((k * 0x9E3779B97F4A7C15ull) >> 40) & mask();
        while (slots[i].stamp == stamp && slots[i].key != k) i = (i + 1) & mask();
        return i;
    }

    // The node for this cell, or UINT32_MAX.
    uint32_t node_of(uint64_t k) const
    {
        const uint32_t i = find(k);
        return slots[i].stamp == stamp ? slots[i].node : UINT32_MAX;
    }

    uint32_t insert(const Node &n)
    {
        if ((nodes.size() + 1) * 2 > slots.size()) {   // half full: twice the room
            slots.assign(slots.size() * 2, Slot{});
            ++stamp;
            if (stamp == 0) stamp = 1;
            for (uint32_t j = 0; j < nodes.size(); ++j) slots[find(nodes[j].key)] = {nodes[j].key, j, stamp};
        }
        nodes.push_back(n);
        slots[find(n.key)] = {n.key, (uint32_t)nodes.size() - 1, stamp};
        return (uint32_t)nodes.size() - 1;
    }
};

thread_local Table g_table;

} // namespace

bool path_standable(const World &world, const NodeRegistry &reg, int x, int y, int z,
                    const PathBody &body)
{
    Reader r(world, reg, body);
    float  f = 0.0f;
    return r.feet(x, y, z, f);
}

bool path_ground(const World &world, const NodeRegistry &reg, int &x, int &y, int &z,
                 const PathBody &body)
{
    Reader r(world, reg, body);
    float  f = 0.0f;
    for (int k = 0; k <= body.drop + 2; ++k)
        if (r.feet(x, y - k, z, f)) {
            y -= k;
            return true;
        }
    return false;
}

PathResult find_path(const World &world, const NodeRegistry &reg, PathCell from, PathCell to,
                     const PathBody &body)
{
    PathResult out;
    if (!path_ground(world, reg, from.x, from.y, from.z, body)) return out;
    path_ground(world, reg, to.x, to.y, to.z, body);   // a goal in the air is aimed at from below

    Reader  rd(world, reg, body);
    Table  &t = g_table;
    t.begin();
    const float hurry = std::max(1.0f, body.hurry);

    // How far the feet may rise in one move: a jump reaches 1.28 over a
    // node, so `step_up` cells and a little more; and what rise is a step
    // (move_box's kStepHeight) rather than a jump.
    constexpr float kStep = 0.55f;
    // A body that does not jump still climbs what it can walk: a stair, two
    // slabs.
    const float most_rise = body.step_up > 0 ? (float)body.step_up + 0.3f : 2.0f * kStep;
    const int   up_cells  = std::max(body.step_up, 1);   // a slab's step can be into the cell above

    // f, then the node. Ties go to the larger g -- the node further along --
    // so that of two equally good cells the one nearer the goal is looked at
    // first.
    struct Open {
        float    f, g;
        uint32_t i;
    };
    // "a after b" for a min-heap on f, larger g first on a tie.
    const auto later = [](const Open &a, const Open &b) {
        return a.f > b.f || (a.f == b.f && a.g < b.g);
    };
    thread_local std::vector<Open> open;   // kept, like the table, between searches
    open.clear();
    const auto push = [&](const Open &o) {
        open.push_back(o);
        std::push_heap(open.begin(), open.end(), later);
    };

    auto add = [&](const PathCell &at, float g, uint32_t parent) -> uint32_t {
        const uint64_t k = key(at.x, at.y, at.z);
        uint32_t       i = t.node_of(k);
        if (i != UINT32_MAX) {
            Node &n = t.nodes[i];
            if (n.closed || n.g <= g) return UINT32_MAX;
            n.g      = g;       // a shorter way to a cell already seen
            n.parent = parent;
        } else {
            i = t.insert(Node{k, at, g, parent, false});
        }
        push({g + hurry * estimate(at, to), g, i});
        return i;
    };

    const uint32_t start = add(from, 0.0f, 0);
    t.nodes[start].parent = start;

    uint32_t best      = start;               // closest so far, if the goal is never reached
    float    best_left = estimate(from, to);
    uint32_t goal      = start;
    bool     found     = false;

    static const int kDirs[8][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1},
                                    {1, 1}, {1, -1}, {-1, 1}, {-1, -1}};
    const float h = (float)body.height;

    // What passing through a cell costs on top of the distance: water is a
    // swim, and a shut door has to be opened.
    auto extra = [&](int x, int y, int z) {
        float e = 0.0f;
        if (rd.at(x, y, z).kind == Kind::Water) e += 1.0f;
        if (body.opens_doors)   // otherwise a shut door was never a way through
            for (int k = 0; k < body.height; ++k)
                if (rd.at(x, y + k, z).kind == Kind::DoorShut) {
                    e += 2.0f;
                    break;
                }
        return e;
    };

    while (!open.empty() && out.expanded < body.max_nodes) {
        std::pop_heap(open.begin(), open.end(), later);
        const uint32_t i = open.back().i;
        open.pop_back();
        if (t.nodes[i].closed) continue;
        t.nodes[i].closed = true;
        ++out.expanded;

        const PathCell c  = t.nodes[i].at;
        const float    g0 = t.nodes[i].g;
        if (c.x == to.x && c.y == to.y && c.z == to.z) {
            goal  = i;
            found = true;
            break;
        }
        const float left = estimate(c, to);
        if (left < best_left) {
            best_left = left;
            best      = i;
        }

        float fc = 0.0f;
        if (!rd.feet(c.x, c.y, c.z, fc)) continue;   // the world changed under a cell

        for (const auto &d : kDirs) {
            const int  nx = c.x + d[0], nz = c.z + d[1];
            const bool diagonal = d[0] != 0 && d[1] != 0;

            // The nearest height it can stand at over there: level first, then
            // up, then down.
            bool  any = false;
            int   ny  = 0;
            float fn  = 0.0f;
            bool  jumps = false;
            auto take = [&](int y) {
                Floor fl;
                if (!rd.feet(nx, y, nz, fl)) return false;
                const float f = fl.f, rise = f - fc;
                if (rise > most_rise || -rise > (float)body.drop) return false;
                // Walked, when the feet go up in steps: to the edge of the
                // floor over there on the side they come in by, then to the
                // rest of it -- a stair from its front. A jump otherwise.
                const float edge = fl.entry(d[0], d[1]);
                const bool  walk = rise <= 0.0f || (edge - fc <= kStep && f - edge <= kStep);
                if (!walk && !(body.step_up > 0 || rise <= kStep)) return false;
                // Room to rise in this column first, or to fall in that one.
                if (rise > 0.0f && !rd.clear(c.x, c.z, fc, f + h)) return false;
                if (rise < 0.0f && !rd.clear(nx, nz, f, fc + h)) return false;
                any = true, ny = y, fn = f, jumps = !walk;
                return true;
            };
            // Level; then up, while there is room over the head in this column
            // (a slab's step can be into the cell above); then down -- a low
            // block's top in the cell under is a drop of less than a cell --
            // only when the body fits over there at the height it is at.
            if (!take(c.y)) {
                for (int dy = 1; dy <= up_cells && !any; ++dy) {
                    if (!rd.clear(c.x, c.z, fc, (float)(c.y + dy) + h)) break;
                    take(c.y + dy);
                }
                if (!any && rd.clear(nx, nz, fc, fc + h))
                    for (int dy = 1; dy <= body.drop + 1 && !any; ++dy) take(c.y - dy);
            }
            if (!any || rd.fenced(c.x, c.y, c.z, d[0], d[1], ny)) continue;

            // Not through a corner: both cells a diagonal cuts past must hold
            // the body over the whole of the move, so a diagonal jump or drop
            // does not clip the edge of a wall on the way.
            if (diagonal) {
                const float lo = std::min(fc, fn), hi = std::max(fc, fn) + h;
                if (!rd.clear(c.x + d[0], c.z, lo, hi) || !rd.clear(c.x, c.z + d[1], lo, hi))
                    continue;
            }

            float step = diagonal ? 1.41421356f : 1.0f;
            if (jumps) step += 0.5f * (fn - fc);             // a jump costs
            if (fn < fc) step += 0.2f * (fc - fn);           // a fall a little
            step += extra(nx, ny, nz);

            if (path_avoids(body, nx, ny, nz) && !(nx == to.x && ny == to.y && nz == to.z)) continue;
            add(PathCell{nx, ny, nz}, g0 + step, i);
        }

        // On a ladder, straight up and straight down as well.
        if (rd.ladder(c.x, c.y, c.z) || rd.ladder(c.x, c.y - 1, c.z)) {
            float f = 0.0f;
            if (rd.feet(c.x, c.y + 1, c.z, f) && rd.clear(c.x, c.z, fc, f + h))
                add(PathCell{c.x, c.y + 1, c.z}, g0 + 1.3f, i);
            if (rd.feet(c.x, c.y - 1, c.z, f))
                add(PathCell{c.x, c.y - 1, c.z}, g0 + 1.0f, i);
        }
    }

    out.reached = found;
    uint32_t k  = found ? goal : best;
    while (k != start) {
        out.cells.push_back(t.nodes[k].at);
        k = t.nodes[k].parent;
    }
    std::reverse(out.cells.begin(), out.cells.end());
    return out;
}

std::vector<PathCell> path_smooth(const World &world, const NodeRegistry &reg, PathCell from,
                                  const std::vector<PathCell> &cells, const PathBody &body)
{
    std::vector<PathCell> out;
    if (cells.empty()) return out;
    Reader rd(world, reg, body);

    // The cells a segment on the ground crosses, both at a corner it passes
    // exactly through; each must hold the feet on its floor at `level`, with
    // no door in the way -- a door is a corner, where a body stops to open
    // it, or to shut one behind it that somebody left open.
    auto crossed = [&](float x0, float z0, float x1, float z1, int y, float level) {
        int       x = (int)std::floor(x0), z = (int)std::floor(z0);
        const int ex = (int)std::floor(x1), ez = (int)std::floor(z1);
        const float dx = x1 - x0, dz = z1 - z0;
        const int   sx = dx > 0 ? 1 : -1, sz = dz > 0 ? 1 : -1;
        const float tdx = dx != 0 ? std::fabs(1.0f / dx) : 1e30f;
        const float tdz = dz != 0 ? std::fabs(1.0f / dz) : 1e30f;
        float tx = dx != 0 ? (sx > 0 ? (float)(x + 1) - x0 : x0 - (float)x) * tdx : 1e30f;
        float tz = dz != 0 ? (sz > 0 ? (float)(z + 1) - z0 : z0 - (float)z) * tdz : 1e30f;
        auto ok = [&](int cx, int cz) {
            float f = 0.0f;
            bool  ground = false;
            if (!rd.feet(cx, y, cz, f, &ground) || !ground || std::fabs(f - level) > 1e-3f) return false;
            if (path_avoids(body, cx, y, cz)) return false;
            for (int k = 0; k < body.height; ++k)
                if (rd.at(cx, y + k, cz).kind == Kind::DoorShut || rd.at(cx, y + k, cz).kind == Kind::DoorOpen)
                    return false;
            return true;
        };
        for (int guard = 0; guard < 512; ++guard) {
            if (!ok(x, z)) return false;
            if (x == ex && z == ez) return true;
            if (tx > 1.0f && tz > 1.0f) return true;   // the segment ends inside this cell
            if (std::fabs(tx - tz) < 1e-5f) {           // exactly through a corner: both sides
                if (!ok(x + sx, z) || !ok(x, z + sz) || rd.fenced(x, y, z, sx, sz, y)) return false;
                x += sx, z += sz, tx += tdx, tz += tdz;
            } else if (tx < tz) {
                if (rd.fenced(x, y, z, sx, 0, y)) return false;
                x += sx, tx += tdx;
            } else {
                if (rd.fenced(x, y, z, 0, sz, y)) return false;
                z += sz, tz += tdz;
            }
        }
        return false;
    };

    // Whether a body walks straight from the middle of `a` to the middle of
    // `b` on one floor: every cell its middle crosses, and every cell its
    // sides cross -- the same line moved the body's half width either way, so
    // it does not brush the corner of a pillar beside the line -- with its
    // feet on the floor at the same height all the way (a slab is a step, and
    // a step is where a way turns).
    // ponytail: the half width is a player's 0.3; take it from the body when
    // a body much wider than a cell needs a way.
    auto straight = [&](const PathCell &a, const PathCell &b) {
        if (a.y != b.y) return false;
        float fa = 0.0f;
        bool  ground = false;
        if (!rd.feet(a.x, a.y, a.z, fa, &ground) || !ground) return false;
        const float x0 = (float)a.x + 0.5f, z0 = (float)a.z + 0.5f;
        const float x1 = (float)b.x + 0.5f, z1 = (float)b.z + 0.5f;
        const float dx = x1 - x0, dz = z1 - z0, len = std::sqrt(dx * dx + dz * dz);
        if (len < 1e-3f) return true;
        const float px = -dz / len * 0.3f, pz = dx / len * 0.3f;   // half a body sideways
        return crossed(x0, z0, x1, z1, a.y, fa) &&
               crossed(x0 + px, z0 + pz, x1 + px, z1 + pz, a.y, fa) &&
               crossed(x0 - px, z0 - pz, x1 - px, z1 - pz, a.y, fa);
    };

    // From each corner, on along the way for as long as a straight walk still
    // reaches: the first cell it does not is where the next corner is. Linear
    // in the length of the way, and a corner kept that a longer look would
    // have cut costs a few steps, never a wall.
    PathCell at = from;
    size_t   i  = 0;
    while (i < cells.size()) {
        size_t far = i;
        while (far + 1 < cells.size() && straight(at, cells[far + 1])) ++far;
        out.push_back(cells[far]);
        at = cells[far];
        i  = far + 1;
    }
    return out;
}

} // namespace cnp
