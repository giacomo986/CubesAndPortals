#pragma once

#include <cstdint>
#include <memory>
#include <unordered_map>
#include <vector>

namespace cnp {

// 32 on a side, against the original engine's 16. The compat layer of DESIGN.md 11 will have
// to present 16-cubed mapblocks to old mods, but that is a translation in the
// compat mod, not a constraint on the core: fewer, larger chunks mean fewer
// draw calls and fewer neighbour lookups per meshed face.
inline constexpr int kChunkSize   = 32;
inline constexpr int kChunkVolume = kChunkSize * kChunkSize * kChunkSize;

using BlockId = uint16_t;
inline constexpr BlockId kAir = 0;

// Light is two 4-bit channels in one byte: sunlight in the high nibble, block
// light in the low one. The same split the classic block game and the original engine use, and the reason
// both fit in a vertex attribute without a second buffer.
inline constexpr uint8_t kMaxLight = 15;

inline uint8_t pack_light(uint8_t sun, uint8_t block)
{
    return (uint8_t)(((sun & 0x0F) << 4) | (block & 0x0F));
}
inline uint8_t sun_light(uint8_t packed)   { return (uint8_t)(packed >> 4); }
inline uint8_t block_light(uint8_t packed) { return (uint8_t)(packed & 0x0F); }

struct ChunkPos {
    int x = 0, y = 0, z = 0;

    bool operator==(const ChunkPos &o) const { return x == o.x && y == o.y && z == o.z; }
};

struct ChunkPosHash {
    size_t operator()(const ChunkPos &p) const
    {
        // Three signed ints into one size_t. Chunk coordinates stay small, so
        // a cheap mix beats anything principled here.
        uint64_t h = (uint64_t)(uint32_t)p.x * 0x9E3779B97F4A7C15ull;
        h ^= (uint64_t)(uint32_t)p.y * 0xC2B2AE3D27D4EB4Full;
        h ^= (uint64_t)(uint32_t)p.z * 0x165667B19E3779F9ull;
        return (size_t)(h ^ (h >> 31));
    }
};

// A chunk that is all one block -- air above the terrain, stone below it --
// stores nothing but that block id. It is the difference between a 512-cubed
// world costing hundreds of megabytes and costing tens: most of a voxel world
// is uniform, and the storage should know that before any compression does.
class Chunk {
public:
    explicit Chunk(ChunkPos p) : m_pos(p) {}

    ChunkPos pos() const { return m_pos; }

    bool    is_uniform() const { return m_blocks == nullptr; }
    BlockId uniform_block() const { return m_uniform; }

    // Whether the *light* is one value, which is a different question from
    // whether the blocks are. A chunk of open air has uniform blocks and a full
    // light array the moment the sun is computed into it, and code that reads
    // is_uniform() as covering both throws that array away. It did: chunks of
    // sunlit air were being stored and streamed as "air, light 0".
    bool    is_light_uniform() const { return m_light == nullptr; }

    static int index(int x, int y, int z) { return x + kChunkSize * (y + kChunkSize * z); }

    static bool in_bounds(int x, int y, int z)
    {
        return x >= 0 && y >= 0 && z >= 0 &&
               x < kChunkSize && y < kChunkSize && z < kChunkSize;
    }

    BlockId get(int x, int y, int z) const
    {
        if (!in_bounds(x, y, z)) return kAir;
        return m_blocks ? m_blocks[index(x, y, z)] : m_uniform;
    }

    // Returns whether anything actually changed. Callers use that to decide
    // whether to invalidate a mesh: writing a block its own value is common --
    // a script that sets state every tick, a generator that overwrites -- and
    // treating it as an edit means remeshing a 32-cubed chunk for nothing.
    bool set(int x, int y, int z, BlockId id)
    {
        if (!in_bounds(x, y, z)) return false;
        if (!m_blocks) {
            if (id == m_uniform) return false;   // still uniform, nothing to store
            materialize();
        }
        BlockId &slot = m_blocks[index(x, y, z)];
        if (slot == id) return false;
        slot = id;
        m_ids.add(id);
        return true;
    }

    uint8_t light(int x, int y, int z) const
    {
        if (!in_bounds(x, y, z) || !m_light) return m_uniform_light;
        return m_light[index(x, y, z)];
    }

    void set_light(int x, int y, int z, uint8_t packed)
    {
        if (!in_bounds(x, y, z)) return;
        if (!m_light) {
            if (packed == m_uniform_light) return;
            materialize_light();
        }
        m_light[index(x, y, z)] = packed;
    }

    // Uniform chunks still need a light value: a chunk of open air is fully
    // sunlit, a chunk of solid stone is dark, and neither should allocate.
    void set_uniform_light(uint8_t packed) { m_uniform_light = packed; }
    uint8_t uniform_light() const { return m_uniform_light; }

    // Which way the block in this cell is turned.
    //
    // A BYTE PER CELL, which is the thing this engine spent M2 to M8 avoiding.
    // Turning used to be extra block ids -- a rotatable block was four of them,
    // one per quarter turn about the vertical -- and that is cheap exactly
    // while there are four. Twenty-four orientations is twenty-four ids per
    // block, a block table mostly made of rotations, and an atlas sized from
    // it. This is where the byte wins (TODO.md OPEN-27).
    //
    // 0 is "not turned", and it is the only value anything writes today: the
    // plane exists, travels and is stored, and nothing reads it yet. That is
    // deliberate -- the storage and the wire are the part that cannot be
    // changed quietly later.
    //
    // A chunk whose facings are all 0 allocates nothing and costs nothing on
    // the wire, which is every chunk of every world this engine makes today.
    bool    is_facing_uniform() const { return m_facing == nullptr; }
    uint8_t uniform_facing() const { return m_uniform_facing; }
    void    set_uniform_facing(uint8_t f) { if (!m_facing) m_uniform_facing = f; }

    uint8_t facing(int x, int y, int z) const
    {
        if (!in_bounds(x, y, z) || !m_facing) return m_uniform_facing;
        return m_facing[index(x, y, z)];
    }

    // Returns whether anything changed, for the same reason set() does: a mesh
    // rebuilt because a cell was written its own facing is a rebuild for
    // nothing.
    bool set_facing(int x, int y, int z, uint8_t f)
    {
        if (!in_bounds(x, y, z)) return false;
        if (!m_facing) {
            if (f == m_uniform_facing) return false;
            materialize_facing();
        }
        uint8_t &slot = m_facing[index(x, y, z)];
        if (slot == f) return false;
        slot = f;
        return true;
    }

    void set_uniform_block(BlockId id)
    {
        if (!m_blocks) {
            m_uniform = id;
            m_ids.add(id);
        }
    }

    // ------------------------------------------------- which ids might be here
    //
    // A bit per block id MODULO 1024, so the answer to "could this chunk hold
    // core:dirt" is a handful of ANDs. It exists for the modifiers of
    // world/modifiers.h: an ABM over one block would otherwise read every cell
    // of every loaded chunk to find out that almost none of them hold it --
    // 16.7 million cells for a world of 512, every interval.
    //
    // CONSERVATIVE in two directions, and both are deliberate. Ids collide
    // modulo 1024, so a set bit means "maybe"; and a bit is never cleared, so a
    // block that was here and is gone leaves its bit behind. Both cost a scan
    // that finds nothing, which is the cheap failure -- the expensive one would
    // be a chunk skipped that did hold the block, and nothing here can produce
    // that. 128 bytes a chunk and one OR per write; the alternative, a list
    // of the distinct ids, is a scan of that list on every write and the
    // mapgen writes sixteen million of them.
    //
    // It was 64 bits until 2026-09-19. With 65 blocks registered, furnace:lit
    // got id 64 -- the bit of air, which every chunk has -- and furnace:burn
    // read every cell of every loaded chunk twice a second without ever
    // firing: 4.9 billion cells, 17.7 s in a 27-minute session.
    struct IdMask {
        static constexpr unsigned kWords = 16;
        uint64_t w[kWords] = {};

        void add(BlockId id) { w[(id >> 6) % kWords] |= 1ull << (id & 63u); }
        bool meets(const IdMask &o) const
        {
            for (unsigned i = 0; i < kWords; ++i)
                if (w[i] & o.w[i]) return true;
            return false;
        }
    };

    const IdMask &ids() const { return m_ids; }

    bool dirty() const { return m_dirty; }

    // A chunk that needs a new mesh is a chunk whose cells, light or facings
    // changed -- which is also what makes the copy in the world file old. So
    // setting one sets the other, and only the mesh flag is cleared by meshing:
    // the client takes `dirty` every frame and the server never does, and a
    // shared flag would be one of them clearing what the other still owes.
    void set_dirty(bool d)
    {
        m_dirty = d;
        if (d) m_unsaved = true;
    }

    // Changed since the world file last had it (see World::take_unsaved).
    bool unsaved() const { return m_unsaved; }
    void set_saved() { m_unsaved = false; }

    size_t memory_bytes() const
    {
        size_t n = sizeof(Chunk);
        if (m_blocks) n += kChunkVolume * sizeof(BlockId);
        if (m_light)  n += kChunkVolume * sizeof(uint8_t);
        if (m_facing) n += kChunkVolume * sizeof(uint8_t);
        return n;
    }

private:
    void materialize();
    void materialize_light();
    void materialize_facing();

    ChunkPos m_pos;
    BlockId  m_uniform        = kAir;
    IdMask   m_ids{{1ull}};   // the bit of kAir, which is 0
    uint8_t  m_uniform_light  = 0;
    uint8_t  m_uniform_facing = 0;
    std::unique_ptr<BlockId[]> m_blocks;
    std::unique_ptr<uint8_t[]> m_light;
    std::unique_ptr<uint8_t[]> m_facing;
    bool     m_dirty = true;
    bool     m_unsaved = true;
};

// Sparse, unbounded, addressed in world block coordinates. Chunks appear when
// something writes to them; nothing outside what has been generated exists.
class World {
public:
    static ChunkPos chunk_of(int x, int y, int z);
    static void     local_of(int x, int y, int z, int &lx, int &ly, int &lz);

    BlockId get_block(int x, int y, int z) const;
    void    set_block(int x, int y, int z, BlockId id);
    bool    is_solid(int x, int y, int z) const { return get_block(x, y, z) != kAir; }

    uint8_t get_light(int x, int y, int z) const;
    void    set_light(int x, int y, int z, uint8_t packed);

    // How the block in a cell is turned. See Chunk::facing.
    uint8_t get_facing(int x, int y, int z) const;
    void    set_facing(int x, int y, int z, uint8_t f);

    Chunk       *find_chunk(ChunkPos p);
    const Chunk *find_chunk(ChunkPos p) const;
    Chunk       &ensure_chunk(ChunkPos p);

    // Takes a chunk out of the world (TODO.md OPEN-38). What was in it reads as
    // ungenerated space again: air, and open sky. Returns whether there was one.
    bool erase_chunk(ChunkPos p);

    // How many chunks have ever been erased. A number and not a list, because
    // the two things that care -- a renderer holding meshes, a sweep holding
    // positions -- only need to know that their copy may be stale, and a list
    // nobody drains is a leak on a server that never draws.
    uint64_t erased() const { return m_erased; }

    // The lowest and highest chunk layer anything has ever been put in, so a
    // column can be walked without walking the whole map. Never shrinks.
    int lowest_layer() const { return m_lowest_layer; }
    int highest_layer() const { return m_highest_layer; }

    // Bulk generation path: fills a chunk without going through set_block's
    // per-write chunk lookup, which dominates the cost of building a world.
    Chunk &ensure_uniform_chunk(ChunkPos p, BlockId id);

    void mark_dirty(ChunkPos p);
    void mark_dirty_around(int x, int y, int z);
    void mark_all_dirty();

    // Hands back up to `max` chunks needing a mesh rebuild, clearing their
    // flags. Bounded so a rebuild storm costs frames of latency, never a stall.
    std::vector<ChunkPos> take_dirty(int max);

    // Every chunk changed since the last call, clearing their flags. Not
    // bounded like take_dirty: a save that left some behind would be a file
    // that is neither the world before nor the world after.
    std::vector<ChunkPos> take_unsaved();

    size_t chunk_count() const { return m_chunks.size(); }
    size_t memory_bytes() const;

    const std::unordered_map<ChunkPos, std::unique_ptr<Chunk>, ChunkPosHash> &chunks() const
    {
        return m_chunks;
    }

private:
    std::unordered_map<ChunkPos, std::unique_ptr<Chunk>, ChunkPosHash> m_chunks;
    uint64_t m_erased        = 0;
    int      m_lowest_layer  = 0;
    int      m_highest_layer = 0;

public:
    // Every loaded chunk's position, sorted. The map's own order is whatever
    // the hashing and the insertion sequence made it, and a sweep that follows
    // it is a sweep whose order is not a property of the world -- which
    // matters, because what the modifiers do to a world has to be the same on
    // two runs of the same seed.
    std::vector<ChunkPos> chunk_positions_sorted() const;

    // Machines' hulls a body is kept out of and stands on (protocol 100), as
    // boxes turned by yaw alone: the middle of the bottom (x, z), the yaw (0
    // faces -z), half the width and length, and the heights between which the
    // hull is. box_hits_world counts them. The client fills them every frame
    // from what it draws; a server leaves them empty -- its machines are a
    // tick ahead of what any client sees, and it shoves bodies out instead
    // (ecs::in_machine).
    struct Mover {
        uint64_t id = 0;
        float    x = 0.0f, z = 0.0f, yaw = 0.0f;
        float    half_w = 0.0f, half_l = 0.0f, bottom = 0.0f, top = 0.0f;
    };
    std::vector<Mover> movers;
};

} // namespace cnp
