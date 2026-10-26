#pragma once

#include "core/registry/node_registry.h"
#include "core/voxel/chunk.h"
#include "core/worldgen/biome.h"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

// World generation as a list of stages, and the hash that proves two runs of it
// agree (DESIGN.md 8, M3: "mapgen deterministico con stage registrabili", and
// the exit test's headless half -- a server running a thousand ticks and
// checking a hash of the world).
//
// Two properties are the whole point:
//
//   - **Deterministic.** Same seed, same stages, same world, on any machine.
//     A server and a client that disagree about the ground are a desync before
//     a single packet has been sent.
//   - **Registrable.** A stage is a name and a function. The core registers
//     the terrain; the client adds the portal walls it needs for its
//     scene; a mod will add its own later. Nothing in here knows what a portal
//     is, and nothing in the client duplicates the terrain.

namespace cnp {

// Vertical extent of a generated world, in blocks. Here rather than in the
// client because the server generates the same world and the two agreeing is
// not optional.
inline constexpr int kWorldHeight = 64;

// Height of the flat plaza the portals stand on, and the level players spawn
// at. Shared for the same reason: the client places its scene on it and the
// server generates it.
inline constexpr int kGroundTop = 20;

// A column of chunks: every chunk at one x and z, whatever its y. The unit the
// world is generated, lit, stored and let go in (TODO.md OPEN-35, OPEN-38).
struct ColumnPos {
    int x = 0, z = 0;

    bool operator==(const ColumnPos &o) const { return x == o.x && z == o.z; }
};

struct ColumnPosHash {
    size_t operator()(const ColumnPos &p) const
    {
        uint64_t h = (uint64_t)(uint32_t)p.x * 0x9E3779B97F4A7C15ull;
        h ^= (uint64_t)(uint32_t)p.z * 0x165667B19E3779F9ull;
        return (size_t)(h ^ (h >> 31));
    }
};

inline ColumnPos column_of(ChunkPos p) { return ColumnPos{p.x, p.z}; }

inline ColumnPos column_of_cell(int x, int z)
{
    const ChunkPos c = World::chunk_of(x, 0, z);
    return ColumnPos{c.x, c.z};
}

// How many columns a side of `extent` blocks takes, from 0.
inline int columns_across(int extent) { return (extent + kChunkSize - 1) / kChunkSize; }

struct MapgenContext {
    World              &world;
    const NodeRegistry &registry;

    uint32_t seed   = 0;
    int      height = kWorldHeight;

    // The column being generated, in blocks, inclusive. A stage runs once per
    // column and writes only inside it: the world has no edge, so a stage that
    // walked a whole world would never finish, and one that wrote into a
    // neighbour would be writing into a column that may already have been
    // lit, stored and sent -- or one whose own terrain has yet to be laid over
    // what it wrote.
    int min_x = 0, min_z = 0;
    int max_x = kChunkSize - 1, max_z = kChunkSize - 1;

    bool owns(int x, int z) const { return x >= min_x && x <= max_x && z >= min_z && z <= max_z; }

    // A write, kept to the column. Anything outside it is dropped, not an
    // error: a structure that straddles two columns is written by both, each
    // keeping its own half, and that is the only way it comes out whole
    // whichever of the two is generated first.
    void set_block(int x, int y, int z, BlockId id) const
    {
        if (owns(x, z)) world.set_block(x, y, z, id);
    }

    // Resolved once, so a stage does not look up "core:stone" per block.
    BlockId stone   = kAir;
    BlockId grass   = kAir;
    BlockId bedrock = kAir;

    // The climates this world has, and their blocks resolved the same way --
    // one entry per biome, in the set's own order. Never null once generate()
    // has built the context.
    const BiomeSet      *biomes = nullptr;
    std::vector<BlockId> biome_top;
    std::vector<BlockId> biome_filler;
};

using MapgenStageFn = std::function<void(MapgenContext &)>;

struct MapgenStage {
    std::string   name;   // namespaced, like a node: "core:terrain"
    MapgenStageFn fn;
};

class Mapgen {
public:
    // Registers core:terrain. Pass false to start empty.
    explicit Mapgen(bool with_core_stages = true);

    void add_stage(const std::string &name, MapgenStageFn fn);

    // No stages at all: a world whose mods make all of it (core.mapgen_core).
    void clear_stages() { m_stages.clear(); }

    // Runs every stage in registration order over one column. Order is the
    // contract: a stage sees what the ones before it wrote.
    void generate_column(World &world, const NodeRegistry &registry, uint32_t seed,
                         ColumnPos column, int height) const;

    // Every column that covers x and z in [0, extent). The same world as
    // generating those columns one at a time in any order, because nothing a
    // stage writes leaves its column.
    void generate(World &world, const NodeRegistry &registry, uint32_t seed,
                  int extent, int height) const;

    const std::vector<MapgenStage> &stages() const { return m_stages; }

    // The climates. Handed out so a caller can add to them, the same way it can
    // add a stage -- except that the order biomes are added in does not change
    // the world, and the order stages are added in is the whole contract.
    BiomeSet       &biomes()       { return m_biomes; }
    const BiomeSet &biomes() const { return m_biomes; }

private:
    std::vector<MapgenStage> m_stages;
    BiomeSet                 m_biomes;
};

// The context the stages see, built without running any of them.
//
// Exposed because a stage a MOD registers cannot run inside generate(): a mod
// declares it while it loads, and a mod loads after the world it is going to
// decorate exists. That pass needs exactly what a core stage was handed --
// the same seed, the same extent, the same blocks resolved once -- and building
// a second context by hand beside this one is how the two come apart.
MapgenContext mapgen_make_context(World &world, const NodeRegistry &registry,
                                  const BiomeSet &biomes, uint32_t seed, int height);

// Points a context at one column.
void mapgen_set_column(MapgenContext &ctx, ColumnPos column);

// Terrain height at one column, exposed because the client needs it to place
// things on the ground without re-deriving the noise.
int mapgen_terrain_height(const MapgenContext &ctx, int x, int z);

// Where the water stands over a column: the y of the first cell above it --
// kSea (18) over the sea, a river or a lake down low, a lake's own level up in
// the hills -- or the terrain height where the column is dry. mods/lakes fills
// below it.
int mapgen_water_level(const MapgenContext &ctx, int x, int z);

// Which biome owns a column: an index into ctx.biomes, and into the resolved
// block arrays beside it. A step function, on purpose -- the surface block is
// allowed to change from one column to the next.
size_t mapgen_biome_at(const BiomeSet &biomes, uint32_t seed, int x, int z);

// The blended height parameters at a column. Continuous, on purpose -- the
// ground is not allowed to change from one column to the next by more than the
// terrain noise already changes it.
BiomeShape mapgen_shape_at(const BiomeSet &biomes, uint32_t seed, int x, int z);

// FNV-1a over every block in the region. Two worlds with the same hash are the
// same world; two with different hashes differ somewhere, and the point of a
// number this cheap is that a CI run can compare it without shipping a world.
uint64_t world_hash(const World &world, int extent, int height);

} // namespace cnp
