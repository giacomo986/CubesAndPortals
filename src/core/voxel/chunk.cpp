#include "core/voxel/chunk.h"

#include <algorithm>

namespace cnp {
namespace {

// Floor division and the matching modulo. Plain / and % truncate toward zero,
// which puts block -1 in chunk 0 and makes everything west or below the origin
// land one chunk off.
int floor_div(int a, int b)
{
    const int q = a / b;
    return (a % b != 0 && ((a < 0) != (b < 0))) ? q - 1 : q;
}

int floor_mod(int a, int b)
{
    const int r = a % b;
    return (r != 0 && ((r < 0) != (b < 0))) ? r + b : r;
}

} // namespace

void Chunk::materialize()
{
    m_blocks = std::make_unique<BlockId[]>(kChunkVolume);
    for (int i = 0; i < kChunkVolume; ++i) m_blocks[i] = m_uniform;
}

void Chunk::materialize_light()
{
    m_light = std::make_unique<uint8_t[]>(kChunkVolume);
    for (int i = 0; i < kChunkVolume; ++i) m_light[i] = m_uniform_light;
}

void Chunk::materialize_facing()
{
    m_facing = std::make_unique<uint8_t[]>(kChunkVolume);
    for (int i = 0; i < kChunkVolume; ++i) m_facing[i] = m_uniform_facing;
}

ChunkPos World::chunk_of(int x, int y, int z)
{
    return ChunkPos{floor_div(x, kChunkSize), floor_div(y, kChunkSize), floor_div(z, kChunkSize)};
}

void World::local_of(int x, int y, int z, int &lx, int &ly, int &lz)
{
    lx = floor_mod(x, kChunkSize);
    ly = floor_mod(y, kChunkSize);
    lz = floor_mod(z, kChunkSize);
}

Chunk *World::find_chunk(ChunkPos p)
{
    auto it = m_chunks.find(p);
    return (it == m_chunks.end()) ? nullptr : it->second.get();
}

const Chunk *World::find_chunk(ChunkPos p) const
{
    auto it = m_chunks.find(p);
    return (it == m_chunks.end()) ? nullptr : it->second.get();
}

Chunk &World::ensure_chunk(ChunkPos p)
{
    auto it = m_chunks.find(p);
    if (it != m_chunks.end()) return *it->second;
    auto [inserted, ok] = m_chunks.emplace(p, std::make_unique<Chunk>(p));
    (void)ok;
    m_lowest_layer  = std::min(m_lowest_layer, p.y);
    m_highest_layer = std::max(m_highest_layer, p.y);
    return *inserted->second;
}

bool World::erase_chunk(ChunkPos p)
{
    if (m_chunks.erase(p) == 0) return false;
    ++m_erased;
    return true;
}

Chunk &World::ensure_uniform_chunk(ChunkPos p, BlockId id)
{
    Chunk &c = ensure_chunk(p);
    c.set_uniform_block(id);
    return c;
}

BlockId World::get_block(int x, int y, int z) const
{
    const Chunk *c = find_chunk(chunk_of(x, y, z));
    if (!c) return kAir;   // ungenerated space reads as air
    int lx, ly, lz;
    local_of(x, y, z, lx, ly, lz);
    return c->get(lx, ly, lz);
}

void World::set_block(int x, int y, int z, BlockId id)
{
    Chunk &c = ensure_chunk(chunk_of(x, y, z));
    int lx, ly, lz;
    local_of(x, y, z, lx, ly, lz);
    if (c.set(lx, ly, lz, id)) mark_dirty_around(x, y, z);
}

uint8_t World::get_light(int x, int y, int z) const
{
    const Chunk *c = find_chunk(chunk_of(x, y, z));
    if (!c) return pack_light(kMaxLight, 0);   // open sky outside the world
    int lx, ly, lz;
    local_of(x, y, z, lx, ly, lz);
    return c->light(lx, ly, lz);
}

void World::set_light(int x, int y, int z, uint8_t packed)
{
    Chunk &c = ensure_chunk(chunk_of(x, y, z));
    int lx, ly, lz;
    local_of(x, y, z, lx, ly, lz);
    c.set_light(lx, ly, lz, packed);
}

uint8_t World::get_facing(int x, int y, int z) const
{
    const Chunk *c = find_chunk(chunk_of(x, y, z));
    if (!c) return 0;   // nothing outside the world is turned
    int lx, ly, lz;
    local_of(x, y, z, lx, ly, lz);
    return c->facing(lx, ly, lz);
}

void World::set_facing(int x, int y, int z, uint8_t f)
{
    Chunk &c = ensure_chunk(chunk_of(x, y, z));
    int lx, ly, lz;
    local_of(x, y, z, lx, ly, lz);
    if (c.set_facing(lx, ly, lz, f)) c.set_dirty(true);
}

void World::mark_dirty(ChunkPos p)
{
    if (Chunk *c = find_chunk(p)) c->set_dirty(true);
}

void World::mark_mesh_dirty(ChunkPos p)
{
    if (Chunk *c = find_chunk(p)) c->set_mesh_dirty();
}

void World::mark_dirty_around(int x, int y, int z)
{
    const ChunkPos base = chunk_of(x, y, z);
    mark_dirty(base);

    // A block on a shared face changes the neighbour's mesh too: the neighbour
    // either hid a face it was drawing or exposed one it was not. And on a
    // shared EDGE or corner, the chunks across it as well: a cube's ambient
    // occlusion and a liquid's corner heights are read off the cells meeting
    // at a corner, so a block at a chunk's corner is in the mesh of every chunk
    // that meets there. Only the faces were marked, and water that drained at
    // one chunk's corner left the chunk diagonally across from it drawing the
    // corner of a surface that was no longer there.
    int lx, ly, lz;
    local_of(x, y, z, lx, ly, lz);
    const int local[3] = {lx, ly, lz};

    int step[3];
    for (int axis = 0; axis < 3; ++axis)
        step[axis] = local[axis] == 0 ? -1 : (local[axis] == kChunkSize - 1 ? 1 : 0);

    for (int dx = 0; dx <= (step[0] != 0 ? 1 : 0); ++dx)
        for (int dy = 0; dy <= (step[1] != 0 ? 1 : 0); ++dy)
            for (int dz = 0; dz <= (step[2] != 0 ? 1 : 0); ++dz) {
                if (dx == 0 && dy == 0 && dz == 0) continue;
                mark_dirty(ChunkPos{base.x + dx * step[0], base.y + dy * step[1],
                                    base.z + dz * step[2]});
            }
}

void World::mark_all_dirty()
{
    for (auto &entry : m_chunks) entry.second->set_dirty(true);
}

std::vector<ChunkPos> World::take_dirty(int max)
{
    std::vector<ChunkPos> out;
    if (max <= 0) return out;
    out.reserve((size_t)max);

    for (auto &entry : m_chunks) {
        if (!entry.second->dirty()) continue;
        entry.second->set_dirty(false);
        out.push_back(entry.first);
        if ((int)out.size() >= max) break;
    }
    return out;
}

std::vector<ChunkPos> World::take_unsaved()
{
    std::vector<ChunkPos> out;
    for (auto &entry : m_chunks) {
        if (!entry.second->unsaved()) continue;
        entry.second->set_saved();
        out.push_back(entry.first);
    }
    return out;
}

size_t World::memory_bytes() const
{
    size_t n = 0;
    for (const auto &entry : m_chunks) n += entry.second->memory_bytes();
    return n;
}

std::vector<ChunkPos> World::chunk_positions_sorted() const
{
    std::vector<ChunkPos> out;
    out.reserve(m_chunks.size());
    for (const auto &entry : m_chunks) out.push_back(entry.first);

    // y first, then z, then x: the order a sweep walks a column of chunks
    // before moving along, which is the order that reads the fewest pages. It
    // matters less than that the order is the SAME on every run.
    std::sort(out.begin(), out.end(), [](const ChunkPos &a, const ChunkPos &b) {
        if (a.y != b.y) return a.y < b.y;
        if (a.z != b.z) return a.z < b.z;
        return a.x < b.x;
    });
    return out;
}

} // namespace cnp