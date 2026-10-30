#include "core/worldgen/worldgen.h"

#include "core/worldgen/noise.h"

#include <algorithm>
#include <cmath>
#include <string>
#include <unordered_map>

namespace cnp {
namespace {

// The portals and the walls around them need level ground, and the continuity
// gate depends on that scene staying put. So the plaza stays flat and the
// terrain only varies outside it.
constexpr int kPlazaX0 = 20, kPlazaX1 = 100;
constexpr int kPlazaZ0 = 20, kPlazaZ1 = 100;

void stage_terrain(MapgenContext &ctx)
{
    for (int z = ctx.min_z; z <= ctx.max_z; ++z) {
        for (int x = ctx.min_x; x <= ctx.max_x; ++x) {
            const int    top    = mapgen_terrain_height(ctx, x, z);
            const size_t biome  = mapgen_biome_at(*ctx.biomes, ctx.seed, x, z);
            const BlockId top_id    = ctx.biome_top[biome];
            const BlockId filler_id = ctx.biome_filler[biome];
            const int     depth     = ctx.biomes->at(biome).filler_depth;

            for (int y = 0; y < top; ++y) {
                // The bottom layer does not break. Without it the world has no
                // floor, which is not a hard mode, it is a hole.
                const BlockId id = (y == 0)                  ? ctx.bedrock
                                 : (y == top - 1)            ? top_id
                                 : (y >= top - 1 - depth)    ? filler_id
                                                             : ctx.stone;
                ctx.world.set_block(x, y, z, id);
            }
        }
    }
}

// The plaza had stone pillars scattered on it (core:scatter), one column in
// about eighty, as something to judge a portal's image against. Taken out on
// 2026-10-10: not wanted on the plaza. Worlds already made keep theirs.

} // namespace

namespace {

bool in_plaza(const BiomeSet &biomes, int x, int z)
{
    return biomes.plaza && x >= kPlazaX0 && x <= kPlazaX1 && z >= kPlazaZ0 && z <= kPlazaZ1;
}

// mods/lakes fills every cell below this with water. The engine does not put
// water anywhere itself; it decides where the ground lies against this line.
constexpr int kSea = 18;

float smooth01(float e0, float e1, float v)
{
    float t = (v - e0) / (e1 - e0);
    t = t < 0.0f ? 0.0f : (t > 1.0f ? 1.0f : t);
    return t * t * (3.0f - 2.0f * t);
}

// Three octaves of value noise, each a third of the one before, weighted
// 0.6, 0.3, 0.1: big shapes with a ragged edge.
float fbm(int x, int z, int period, uint32_t seed)
{
    return 0.6f * value_noise(x, z, period, seed) + 0.3f * value_noise(x, z, period / 3, seed ^ 0x2545F491u) +
           0.1f * value_noise(x, z, period / 9, seed ^ 0x9E3779B9u);
}

// Where a column's ground lies, and what put it there.
struct Ground {
    int   h     = kGroundTop;
    float ocean = 0.0f;   // 0 inland .. 1 open sea
    float deep  = 0.0f;   // 0 on the shelf .. 1 the ocean's floor
    float river = 0.0f;   // 0 dry .. 1 the river's bed
    float gorge = 0.0f;   // 0 .. 1 a gorge's floor
    float plaza = 0.0f;   // 1 on the plaza .. 0 from 16 out
    int   water = kSea;   // the y of the first cell over the water, where it is under any
};

// The ground, as the layers made it:
//
//   * the land: the terrain noise scaled by the climate's blended base and
//     relief, as it always was;
//   * the sea: a continental field some 600 blocks across. Below 0.40 the
//     land sinks to a shelf a few blocks under the water (a sea), below 0.28
//     to an ocean's floor a dozen down; the plaza's surroundings are pushed
//     up so the spawn is always on land;
//   * the rivers: where a third field crosses its middle, a channel a few
//     blocks wide is cut down to three under the sea level, its banks rising
//     back to the land over a dozen blocks; not on the plaza, faded out under
//     the hills it would have to cut a gorge through, and gone into the sea;
//   * the plaza, flat, and blended into for 16 blocks round it.
Ground ground_base(const BiomeSet &biomes, uint32_t seed, int x, int z, int height)
{
    Ground g;
    if (in_plaza(biomes, x, z)) {
        g.plaza = 1.0f;
        return g;
    }

    const float      n = 0.65f * value_noise(x, z, 48, seed) + 0.35f * value_noise(x, z, 13, seed);
    const BiomeShape s = mapgen_shape_at(biomes, seed, x, z);
    // Three above the plaza on average, so the plains do not stand in puddles:
    // a hollow is a lake where the noise is deep, not wherever it dips.
    float land = (float)kGroundTop + 3.0f + s.base + (n - 0.5f) * 28.0f * s.relief;

    const int   dx = (x < kPlazaX0) ? kPlazaX0 - x : (x > kPlazaX1 ? x - kPlazaX1 : 0);
    const int   dz = (z < kPlazaZ0) ? kPlazaZ0 - z : (z > kPlazaZ1 ? z - kPlazaZ1 : 0);
    // No plaza: as far from one as anywhere can be.
    const int   d  = !biomes.plaza ? 1 << 20 : (dx > dz) ? dx : dz;
    const float near_plaza = 1.0f - smooth01(0.0f, 420.0f, (float)d);

    float c = fbm(x, z, 640, seed ^ 0xC0A57u);
    c = 0.5f + (c - 0.5f) * 1.8f + 0.45f * near_plaza;
    g.ocean = 1.0f - smooth01(0.28f, 0.40f, c);
    g.deep  = 1.0f - smooth01(0.14f, 0.28f, c);
    const float floor = (float)kSea - 3.0f - 9.0f * g.deep;
    float h = land * (1.0f - g.ocean) + floor * g.ocean;

    const float r    = fbm(x, z, 360, seed ^ 0x51BE5u);
    const float v    = std::fabs(r - 0.5f);
    constexpr float kWidth = 0.011f;
    const float hills = 1.0f - smooth01((float)kSea + 10.0f, (float)kSea + 18.0f, land);
    const float away  = smooth01(24.0f, 56.0f, (float)d);
    g.river = (1.0f - smooth01(kWidth, kWidth * 3.5f, v)) * hills * away * (1.0f - g.ocean);
    const float bed = (float)kSea - 1.0f - 2.0f * (1.0f - smooth01(0.0f, kWidth, v));
    if (h > bed) h -= (h - bed) * g.river;

    // Gorges: where a fourth field crosses its middle, in the stretches a
    // slow fifth one marks (a third of the world or so), a cut three or four
    // blocks wide down to two under the sea level -- a torrent at the bottom --
    // with walls of rock that fall in four or five blocks however high the
    // land. Not near the plaza, not into the sea.
    const float where = smooth01(0.56f, 0.66f, value_noise(x, z, 720, seed ^ 0x6A29Eu));
    const float gv    = std::fabs(fbm(x, z, 420, seed ^ 0x60E6Eu) - 0.5f);
    constexpr float kGorge = 0.006f;
    g.gorge = (1.0f - smooth01(kGorge, kGorge * 2.4f, gv)) * where * away * (1.0f - g.ocean);
    const float gfloor = (float)kSea - 2.0f;
    if (h > gfloor) h -= (h - gfloor) * g.gorge;

    int hi = (int)std::floor(h);
    if (d < 16) {
        const float t = (float)d / 16.0f;
        hi = kGroundTop + (int)((float)(hi - kGroundTop) * t);
        g.plaza = 1.0f - t;
    }
    if (hi < 1) hi = 1;
    if (hi > height - 1) hi = height - 1;
    g.h = hi;
    return g;
}

// Lakes up in the hills and the mountains. The world is cut into cells of 176
// blocks and one in two has a site: a disc 7 to 15 across the middle, put
// where the ground round it is at least 6 over the sea and nowhere near the
// sea, a river or the plaza. Its water stands one under the lowest of the
// ground on two rings round it, so it is held in all round; the disc is dug
// into a bowl 2 to 5 deep under that, and for 14 blocks out the ground is cut
// down to a slope rising from one over the water, so a lake on a hillside sits
// in a hollow instead of behind a wall. Its middle is 32 from the cell's edge,
// so a lake and its slope stay in one cell.
constexpr int kTarnCell = 176;
constexpr int kTarnSlope = 14;

struct Tarn {
    bool  any = false;
    int   cx = 0, cz = 0, r = 0, depth = 0, level = 0;
};

Tarn tarn_in(const BiomeSet &biomes, uint32_t seed, int gx, int gz, int height)
{
    // Asked for every column round it; worked out once a cell, per thread.
    struct Key {
        const BiomeSet *b; uint32_t seed; int gx, gz;
        bool operator==(const Key &o) const { return b == o.b && seed == o.seed && gx == o.gx && gz == o.gz; }
    };
    struct KeyHash {
        size_t operator()(const Key &k) const
        {
            return (size_t)noise_hash(k.gx, k.gz, k.seed) ^ ((size_t)(uintptr_t)k.b >> 4);
        }
    };
    thread_local std::unordered_map<Key, Tarn, KeyHash> known;
    const Key key{&biomes, seed, gx, gz};
    if (auto it = known.find(key); it != known.end()) return it->second;
    if (known.size() > 4096) known.clear();

    Tarn t;
    const uint32_t h = noise_hash(gx, gz, seed ^ 0x7A4E1u);
    if (h % 2 == 0) {
        t.cx    = gx * kTarnCell + 32 + (int)((h >> 4) % (kTarnCell - 64));
        t.cz    = gz * kTarnCell + 32 + (int)((h >> 12) % (kTarnCell - 64));
        t.r     = 7 + (int)((h >> 20) % 9);
        t.depth = 2 + (int)((h >> 26) % 4);
        const Ground mid = ground_base(biomes, seed, t.cx, t.cz, height);
        int lowest = height;
        for (int ring : {t.r, t.r + 3}) {
            for (int k = 0; k < 24; ++k) {
                const float a  = 6.2831853f * (float)k / 24.0f;
                const Ground e = ground_base(biomes, seed, t.cx + (int)std::lround(std::cos(a) * ring),
                                             t.cz + (int)std::lround(std::sin(a) * ring), height);
                if (e.ocean > 0.02f || e.river > 0.02f || e.gorge > 0.02f || e.plaza > 0.0f) lowest = 0;
                lowest = std::min(lowest, e.h);
            }
        }
        t.level = lowest - 1;
        t.any   = t.level >= kSea + 5 && mid.ocean < 0.02f && mid.river < 0.02f && mid.plaza == 0.0f &&
                !in_plaza(biomes, t.cx, t.cz);
    }
    known.emplace(key, t);
    return t;
}

int floor_div(int a, int b) { return (a >= 0) ? a / b : -((-a + b - 1) / b); }

Ground ground_at(const BiomeSet &biomes, uint32_t seed, int x, int z, int height)
{
    Ground g = ground_base(biomes, seed, x, z, height);
    if (g.plaza > 0.0f) return g;
    const Tarn t = tarn_in(biomes, seed, floor_div(x, kTarnCell), floor_div(z, kTarnCell), height);
    if (!t.any) return g;
    const float d = std::hypot((float)(x - t.cx), (float)(z - t.cz));
    if (d < (float)t.r) {
        const float u      = d / (float)t.r;
        const int   bottom = t.level - (int)std::lround((float)t.depth * (1.0f - u * u));
        g.h     = std::min(g.h, std::min(bottom, t.level - 1));
        g.water = t.level;
    } else if (d < (float)(t.r + kTarnSlope)) {
        g.h = std::min(g.h, t.level + 1 + (int)((d - (float)t.r) * 1.6f));
    }
    return g;
}

size_t find_or(const BiomeSet &biomes, const char *name, size_t fallback)
{
    const size_t i = biomes.find(name);
    return (i < biomes.size()) ? i : fallback;
}

} // namespace

size_t mapgen_biome_at(const BiomeSet &biomes, uint32_t seed, int x, int z)
{
    // The plaza is plains whatever the noise says. The portal gates stand on
    // it, and a scene that moves because this seed happened to roll a desert is
    // a scene that fails a gate for a reason nobody can read.
    if (in_plaza(biomes, x, z)) {
        const size_t plains = biomes.find("core:plains");
        return (plains < biomes.size()) ? plains : 0;
    }
    const size_t climate = biomes.pick(biome_temperature(x, z, seed), biome_humidity(x, z, seed));

    // Under water, which water; at its edge, a beach -- of sand, unless the
    // climate is one that would not have one (snow and rock come down to the
    // water as they are).
    const Ground g = ground_at(biomes, seed, x, z, kWorldHeight);
    if (g.water > kSea && g.h < g.water) return find_or(biomes, "core:lake", climate);   // up in the hills
    if (g.gorge > 0.3f && g.ocean < 0.5f) return find_or(biomes, "core:gorge", climate);
    if (g.h < kSea) {
        if (g.ocean > 0.5f) return find_or(biomes, g.deep > 0.4f ? "core:ocean" : "core:sea", climate);
        return find_or(biomes, g.river > 0.3f ? "core:river" : "core:lake", climate);
    }
    const std::string &cname = biomes.at(climate).name;
    if (g.h <= kSea + 1 && g.plaza == 0.0f && (g.ocean > 0.02f || g.river > 0.12f) && cname != "core:tundra" &&
        cname != "core:peaks")
        return find_or(biomes, "core:beach", climate);
    return climate;
}

BiomeShape mapgen_shape_at(const BiomeSet &biomes, uint32_t seed, int x, int z)
{
    return biomes.shape(biome_temperature(x, z, seed), biome_humidity(x, z, seed));
}

bool mapgen_plaza(const BiomeSet &biomes, int *lo, int *hi)
{
    static_assert(kPlazaX0 == kPlazaZ0 && kPlazaX1 == kPlazaZ1, "the plaza is square");
    if (!biomes.plaza) return false;
    *lo = kPlazaX0;
    *hi = kPlazaX1;
    return true;
}

int mapgen_terrain_height(const MapgenContext &ctx, int x, int z)
{
    return ground_at(*ctx.biomes, ctx.seed, x, z, ctx.height).h;
}

int mapgen_water_level(const MapgenContext &ctx, int x, int z)
{
    const Ground g = ground_at(*ctx.biomes, ctx.seed, x, z, ctx.height);
    if (g.h < g.water) return g.water;
    return g.h;
}

Mapgen::Mapgen(bool with_core_stages)
{
    if (!with_core_stages) return;
    add_stage("core:terrain", stage_terrain);
}

void Mapgen::add_stage(const std::string &name, MapgenStageFn fn)
{
    m_stages.push_back(MapgenStage{name, std::move(fn)});
}

MapgenContext mapgen_make_context(World &world, const NodeRegistry &registry,
                                  const BiomeSet &biomes, uint32_t seed, int height)
{
    MapgenContext ctx{world, registry, seed, height, 0, 0, kChunkSize - 1, kChunkSize - 1,
                      kAir, kAir, kAir, nullptr, {}, {}};

    // Looked up by name: the generator does not know any block's id either.
    ctx.stone   = registry.find("core:stone");
    ctx.grass   = registry.find("core:grass");
    ctx.bedrock = registry.find("core:bedrock");
    if (ctx.bedrock == kAir) ctx.bedrock = ctx.stone;   // a world without bedrock still gets a floor

    // Same lookup, once per biome instead of once per block. A biome naming a
    // block this build does not have falls back to stone rather than to air:
    // a surface made of nothing is a hole, and a hole is harder to notice than
    // the wrong colour.
    ctx.biomes = &biomes;
    for (size_t i = 0; i < biomes.size(); ++i) {
        const BiomeDef &b = biomes.at(i);
        const BlockId   t = registry.find(b.top);
        const BlockId   f = registry.find(b.filler);
        ctx.biome_top.push_back(t == kAir ? ctx.stone : t);
        ctx.biome_filler.push_back(f == kAir ? ctx.stone : f);
    }

    return ctx;
}

void mapgen_set_column(MapgenContext &ctx, ColumnPos column)
{
    ctx.min_x = column.x * kChunkSize;
    ctx.min_z = column.z * kChunkSize;
    ctx.max_x = ctx.min_x + kChunkSize - 1;
    ctx.max_z = ctx.min_z + kChunkSize - 1;
}

void Mapgen::generate_column(World &world, const NodeRegistry &registry, uint32_t seed,
                             ColumnPos column, int height) const
{
    MapgenContext ctx = mapgen_make_context(world, registry, m_biomes, seed, height);
    mapgen_set_column(ctx, column);
    for (const MapgenStage &stage : m_stages) stage.fn(ctx);
}

void Mapgen::generate(World &world, const NodeRegistry &registry, uint32_t seed,
                      int extent, int height) const
{
    const int n = columns_across(extent);
    for (int cz = 0; cz < n; ++cz)
        for (int cx = 0; cx < n; ++cx)
            generate_column(world, registry, seed, ColumnPos{cx, cz}, height);
}

uint64_t world_hash(const World &world, int extent, int height)
{
    // FNV-1a, 64 bit. Not a cryptographic claim -- a cheap way for two machines
    // to agree that they generated the same world without shipping it.
    uint64_t h = 1469598103934665603ull;

    for (int z = 0; z < extent; ++z) {
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < extent; ++x) {
                const uint16_t id = (uint16_t)world.get_block(x, y, z);
                h ^= (uint64_t)(id & 0xFFu);
                h *= 1099511628211ull;
                h ^= (uint64_t)((id >> 8) & 0xFFu);
                h *= 1099511628211ull;
            }
        }
    }
    return h;
}

} // namespace cnp
