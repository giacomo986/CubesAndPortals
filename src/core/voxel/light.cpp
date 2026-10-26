#include "core/voxel/light.h"

#include <algorithm>
#include <climits>
#include <deque>
#include <vector>

namespace cnp {
namespace {

struct Node {
    int x, y, z;
};

struct Glow {
    int     x, y, z;
    uint8_t level;
};

} // namespace

namespace {

void light_box(World &world, const NodeRegistry &reg, int min_x, int min_y, int min_z,
               int max_x, int max_y, int max_z, int keep_min_x, int keep_min_z, int keep_max_x,
               int keep_max_z);

} // namespace

void compute_lighting(World &world, const NodeRegistry &reg,
                      int min_x, int min_y, int min_z,
                      int max_x, int max_y, int max_z)
{
    light_box(world, reg, min_x, min_y, min_z, max_x, max_y, max_z, min_x, min_z, max_x, max_z);
}

void compute_column_lighting(World &world, const NodeRegistry &reg, int cx, int cz, int height)
{
    const int x0 = cx * kChunkSize, z0 = cz * kChunkSize;
    const int x1 = x0 + kChunkSize - 1, z1 = z0 + kChunkSize - 1;
    const int r  = kMaxLight;
    light_box(world, reg, x0 - r, 0, z0 - r, x1 + r, height - 1, z1 + r, x0, z0, x1, z1);
}

namespace {

void light_box(World &world, const NodeRegistry &reg, int min_x, int min_y, int min_z,
               int max_x, int max_y, int max_z, int keep_min_x, int keep_min_z, int keep_max_x,
               int keep_max_z)
{
    const int size_x = max_x - min_x + 1;
    const int size_y = max_y - min_y + 1;
    const int size_z = max_z - min_z + 1;
    if (size_x <= 0 || size_y <= 0 || size_z <= 0) return;

    // A local scratch grid rather than writing through the world for every
    // step: the flood fill touches each cell several times, and every one of
    // those would otherwise be a chunk lookup.
    //
    // Retained across calls when the region is small, freed immediately when it
    // is not.
    //
    // Relighting after an edit is a 33-cubed box and happens constantly, so
    // holding its scratch is worth it. The bulk pass at generation covers the
    // whole world -- 16 MB per grid at 512 wide -- and holding THAT for the rest
    // of the session costs 50 MB of steady state to save one allocation that
    // happens once. Retaining it unconditionally was measured doing exactly
    // that: resident memory went from 138 MB to 186 MB and stayed there.
    constexpr size_t kRetainLimit = 4u * 1024u * 1024u;   // cells, not bytes

    const size_t cells = (size_t)size_x * size_y * size_z;
    static thread_local std::vector<uint8_t> retained_sun, retained_blk, retained_solid;
    std::vector<uint8_t> local_sun, local_blk, local_solid;

    const bool retain = cells <= kRetainLimit;
    std::vector<uint8_t> &sun   = retain ? retained_sun   : local_sun;
    std::vector<uint8_t> &blk   = retain ? retained_blk   : local_blk;
    std::vector<uint8_t> &solid = retain ? retained_solid : local_solid;

    sun.assign(cells, 0);
    blk.assign(cells, 0);
    solid.assign(cells, 0);

    auto idx = [&](int x, int y, int z) {
        return (size_t)((x - min_x) + size_x * ((y - min_y) + (size_y * (z - min_z))));
    };
    auto inside = [&](int x, int y, int z) {
        return x >= min_x && y >= min_y && z >= min_z && x <= max_x && y <= max_y && z <= max_z;
    };

    // What each block does to light, once per id rather than once per cell.
    const size_t kinds = reg.size();
    static thread_local std::vector<uint8_t> opaque_of, emit_of;
    opaque_of.assign(kinds, 0);
    emit_of.assign(kinds, 0);
    for (size_t id = 0; id < kinds; ++id) {
        opaque_of[id] = reg.is_opaque((BlockId)id) ? 1 : 0;
        emit_of[id]   = reg.light_source((BlockId)id);
    }

    // One chunk lookup per chunk crossed and not one per cell: the walk goes
    // x inside y inside z, so the next cell is nearly always in the chunk the
    // last one was in. A column is lit over a box of 238 thousand cells, and
    // the lookup per cell was most of what lighting it cost (TODO.md OPEN-35).
    // The cells that glow are gathered in the same walk, which used to be a
    // second pass over every cell of the box for the handful that do.
    static thread_local std::vector<Glow> glowing;
    glowing.clear();
    {
        ChunkPos     at{INT_MIN, INT_MIN, INT_MIN};
        const Chunk *chunk = nullptr;
        for (int z = min_z; z <= max_z; ++z) {
            for (int y = min_y; y <= max_y; ++y) {
                for (int x = min_x; x <= max_x; ++x) {
                    const ChunkPos p = World::chunk_of(x, y, z);
                    if (!(p == at)) {
                        at    = p;
                        chunk = world.find_chunk(p);
                    }
                    BlockId id = kAir;
                    if (chunk != nullptr) {
                        int lx, ly, lz;
                        World::local_of(x, y, z, lx, ly, lz);
                        id = chunk->get(lx, ly, lz);
                    }

                    const bool known = id < kinds;
                    solid[idx(x, y, z)] =
                        known ? opaque_of[id] : (reg.is_opaque(id) ? 1 : 0);
                    const uint8_t emit = known ? emit_of[id] : reg.light_source(id);
                    if (emit != 0) glowing.push_back(Glow{x, y, z, emit});
                }
            }
        }
    }

    // Same reasoning for the frontier queue: keep it between the small,
    // frequent relights, but do not let one huge pass leave its capacity
    // parked for the rest of the session.
    static thread_local std::deque<Node> retained_queue;
    std::deque<Node> local_queue;
    std::deque<Node> &queue = retain ? retained_queue : local_queue;
    queue.clear();

    // Sunlight: straight down from the top of the region at full strength,
    // stopping at the first solid block in the column.
    for (int z = min_z; z <= max_z; ++z) {
        for (int x = min_x; x <= max_x; ++x) {
            for (int y = max_y; y >= min_y; --y) {
                if (solid[idx(x, y, z)]) break;
                sun[idx(x, y, z)] = kMaxLight;
            }
        }
    }

    // Seed the spread from the frontier only: a fully lit cell that has a
    // neighbour still in shadow. Enqueueing every lit cell instead is the
    // obvious version and it is what ran a 512 by 512 world out of memory --
    // eleven million nodes, hundreds of megabytes of queue, for a frontier
    // that is only ever the surface of the terrain.
    for (int z = min_z; z <= max_z; ++z) {
        for (int y = min_y; y <= max_y; ++y) {
            for (int x = min_x; x <= max_x; ++x) {
                if (sun[idx(x, y, z)] != kMaxLight) continue;

                static const int kNeighbours[6][3] = {
                    {1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1}};
                for (const auto &o : kNeighbours) {
                    const int nx = x + o[0], ny = y + o[1], nz = z + o[2];
                    if (!inside(nx, ny, nz)) continue;
                    if (solid[idx(nx, ny, nz)]) continue;
                    if (sun[idx(nx, ny, nz)] >= kMaxLight - 1) continue;
                    queue.push_back(Node{x, y, z});
                    break;
                }
            }
        }
    }

    // Spread sideways and into overhangs, losing one level per step.
    while (!queue.empty()) {
        const Node n = queue.front();
        queue.pop_front();
        const uint8_t level = sun[idx(n.x, n.y, n.z)];
        if (level <= 1) continue;

        static const int kOffsets[6][3] = {
            {1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1}};

        for (const auto &o : kOffsets) {
            const int nx = n.x + o[0], ny = n.y + o[1], nz = n.z + o[2];
            if (!inside(nx, ny, nz)) continue;
            if (solid[idx(nx, ny, nz)]) continue;

            const uint8_t next = (uint8_t)(level - 1);
            if (sun[idx(nx, ny, nz)] >= next) continue;
            sun[idx(nx, ny, nz)] = next;
            queue.push_back(Node{nx, ny, nz});
        }
    }

    // Block light: same flood fill, seeded from whatever the registry says
    // emits. Two passes rather than one shared queue because the channels
    // attenuate independently -- sunlight falls vertically for free, block
    // light does not, and merging them makes a torch light a shaft downward.
    queue.clear();
    for (const Glow &g : glowing) {
        blk[idx(g.x, g.y, g.z)] = g.level;
        queue.push_back(Node{g.x, g.y, g.z});
    }

    while (!queue.empty()) {
        const Node n = queue.front();
        queue.pop_front();
        const uint8_t level = blk[idx(n.x, n.y, n.z)];
        if (level <= 1) continue;

        static const int kSpread[6][3] = {
            {1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1}};

        for (const auto &o : kSpread) {
            const int nx = n.x + o[0], ny = n.y + o[1], nz = n.z + o[2];
            if (!inside(nx, ny, nz)) continue;
            if (solid[idx(nx, ny, nz)]) continue;

            const uint8_t next = (uint8_t)(level - 1);
            if (blk[idx(nx, ny, nz)] >= next) continue;
            blk[idx(nx, ny, nz)] = next;
            queue.push_back(Node{nx, ny, nz});
        }
    }


    {
        // Back into the world with the same one lookup per chunk crossed.
        ChunkPos at{INT_MIN, INT_MIN, INT_MIN};
        Chunk   *dst = nullptr;
        for (int z = keep_min_z; z <= keep_max_z; ++z) {
            for (int y = min_y; y <= max_y; ++y) {
                for (int x = keep_min_x; x <= keep_max_x; ++x) {
                    const ChunkPos p = World::chunk_of(x, y, z);
                    if (!(p == at)) {
                        at  = p;
                        dst = &world.ensure_chunk(p);
                    }
                    int lx, ly, lz;
                    World::local_of(x, y, z, lx, ly, lz);
                    dst->set_light(lx, ly, lz, pack_light(sun[idx(x, y, z)], blk[idx(x, y, z)]));
                }
            }
        }
    }

    // Every chunk the region touched needs a new mesh: light lives in the
    // vertices.
    const ChunkPos lo = World::chunk_of(keep_min_x, min_y, keep_min_z);
    const ChunkPos hi = World::chunk_of(keep_max_x, max_y, keep_max_z);
    for (int cz = lo.z; cz <= hi.z; ++cz)
        for (int cy = lo.y; cy <= hi.y; ++cy)
            for (int cx = lo.x; cx <= hi.x; ++cx)
                world.mark_dirty(ChunkPos{cx, cy, cz});
}

} // namespace

bool light_unchanged(const NodeRegistry &reg, BlockId from, BlockId to)
{
    if (from == to) return true;
    return reg.is_opaque(from) == reg.is_opaque(to) &&
           reg.light_source(from) == reg.light_source(to);
}

LightBatch::Box LightBatch::joined(const Box &a, const Box &b)
{
    Box out{};
    for (int i = 0; i < 3; ++i) {
        out.min[i] = a.min[i] < b.min[i] ? a.min[i] : b.min[i];
        out.max[i] = a.max[i] > b.max[i] ? a.max[i] : b.max[i];
    }
    return out;
}

// What a box will actually COST when the batch is flushed: its volume grown by
// the light radius on every side. Deciding on the bare volume is what the first
// version of this did, and it joined a cell five hundred blocks away to a box
// because a line of 501 cells looks cheap -- while the pass over it, expanded,
// is 580000 cells against the 36000 of a box of its own.
long long LightBatch::growth(const Box &a, const Box &b)
{
    return expanded_volume(joined(a, b)) - expanded_volume(a);
}

long long LightBatch::expanded_volume(const Box &b)
{
    const long long r = kMaxLight + 1;
    return (long long)(b.max[0] - b.min[0] + 1 + 2 * r) *
           (long long)(b.max[1] - b.min[1] + 1 + 2 * r) *
           (long long)(b.max[2] - b.min[2] + 1 + 2 * r);
}

void LightBatch::add(int x, int y, int z)
{
    const Box cell{{x, y, z}, {x, y, z}};

    // The box this cell costs least to join, in the cells a flush would have to
    // light. A cell already inside one costs nothing, which is what a mod
    // filling a room produces over and over.
    size_t    best      = m_boxes.size();
    long long best_cost = 0;
    for (size_t i = 0; i < m_boxes.size(); ++i) {
        const long long cost = growth(m_boxes[i], cell);
        if (best == m_boxes.size() || cost < best_cost) {
            best      = i;
            best_cost = cost;
        }
    }

    // A box of its own costs exactly one pass around one cell. So the decision
    // is a comparison and not a threshold somebody chose: whichever adds fewer
    // cells to the flush wins, and a cell far from everything adds fewer by
    // starting its own box.
    const long long alone = expanded_volume(cell);

    // A cell that would cost more to join than it costs alone gets its own box,
    // AND IT GETS ONE EVEN AT THE CAP: the cap is then restored by merging the
    // two boxes that are cheapest to merge, which is a better answer than
    // stretching whichever box happened to be nearest across the gap.
    //
    // The first version of this refused to push at the cap, which made the
    // merge below unreachable -- dead code that a sabotage of it could not turn
    // red, which is how it was found.
    if (best == m_boxes.size() || best_cost > alone) {
        m_boxes.push_back(cell);
    } else {
        m_boxes[best] = joined(m_boxes[best], cell);
    }

    // Over-lighting, never under-lighting: a box that covers more than it had
    // to costs time, and one that covers less leaves a seam of stale light
    // nothing will come back for.
    while (m_boxes.size() > (size_t)kLightBatchBoxes) {
        size_t    ai = 0, bi = 1;
        long long cheapest = -1;
        for (size_t i = 0; i < m_boxes.size(); ++i) {
            for (size_t j = i + 1; j < m_boxes.size(); ++j) {
                const long long cost = expanded_volume(joined(m_boxes[i], m_boxes[j])) -
                                       expanded_volume(m_boxes[i]) - expanded_volume(m_boxes[j]);
                if (cheapest < 0 || cost < cheapest) {
                    cheapest = cost;
                    ai       = i;
                    bi       = j;
                }
            }
        }
        m_boxes[ai] = joined(m_boxes[ai], m_boxes[bi]);
        m_boxes.erase(m_boxes.begin() + (ptrdiff_t)bi);
    }
}

std::string LightBatch::describe() const
{
    std::string out;
    for (const Box &b : m_boxes) {
        if (!out.empty()) out += "  ";
        out += std::to_string(b.min[0]) + "," + std::to_string(b.min[1]) + "," +
               std::to_string(b.min[2]) + ".." + std::to_string(b.max[0]) + "," +
               std::to_string(b.max[1]) + "," + std::to_string(b.max[2]);
    }
    return out;
}

size_t LightBatch::flush(World &world, const NodeRegistry &reg)
{
    size_t cells = 0;

    for (const Box &b : m_boxes) {
        // The same radius relight_around uses, and for the same reason: light
        // travels fifteen blocks, so anything closer re-lights partially and
        // leaves a seam.
        const int r = kMaxLight + 1;
        cells += (size_t)expanded_volume(b);

        compute_lighting(world, reg, b.min[0] - r, b.min[1] - r, b.min[2] - r, b.max[0] + r,
                         b.max[1] + r, b.max[2] + r);
    }

    m_boxes.clear();
    return cells;
}

void relight_around(World &world, const NodeRegistry &reg, int x, int y, int z, int radius)
{
    compute_lighting(world, reg,
                     x - radius, y - radius, z - radius,
                     x + radius, y + radius, z + radius);
}


} // namespace cnp
