#include "core/worldgen/spawn.h"

#include <cstdio>

#include "core/physics/movement.h"
#include "core/worldgen/noise.h"
#include "core/worldgen/worldgen.h"

#include <cmath>

namespace cnp {

namespace {

// A point, or the nearest free one beside it.
//
// The search steps SIDEWAYS, half a node at a time, alternating. Sideways keeps
// whatever the base point was derived FROM -- the distance to an opening, the
// middle of the plaza -- which is the whole reason these points are derived
// rather than written down; nudging a constant until one seed looked right
// would have been fixing one world and leaving the next to be discovered the
// same way.
//
// One function and not one per spawn, for the reason this milestone has kept
// finding: two copies of a search are two searches, and they agree until the
// day one of them is changed.
Vec3 free_beside(const World &world, const NodeRegistry &reg, Vec3 base, Vec3 side)
{
    if (!inside_solid(world, reg, base.x, base.y, base.z)) return base;

    for (int i = 1; i <= 24; ++i) {
        const float step = 0.5f * (float)((i + 1) / 2);
        const float off  = (i % 2) ? step : -step;
        const Vec3  c{base.x + side.x * off, base.y, base.z + side.z * off};
        if (!inside_solid(world, reg, c.x, c.y, c.z)) return c;
    }

    // Twelve nodes each way and every one of them occupied. The derived point
    // is returned rather than a wrong one invented: standing in a block is
    // visible and diggable, and a spawn silently moved somewhere else is not.
    std::printf("spawn: nothing free within 12 nodes either side of the derived point\n");
    return base;
}

} // namespace

// Where a player stands to look at an opening: `stand_off` nodes in front of
// it, on the ground, and out of whatever happens to be standing there.
//
// The last part is not decoration. The plaza used to have stone columns two
// to five tall scattered on it (core:scatter, gone since 2026-10-10), and on
// seed 1, the one --dual runs, the derived point was inside one -- reported
// from play. Mods still build on the plaza, so the point is still checked.
//
// The search steps SIDEWAYS, half a node at a time, alternating. Sideways
// keeps both the distance to the opening and the line of sight to it, which is
// the whole reason this point is derived from the portal instead of written
// down; nudging the constant until seed 1 looked right would have been fixing
// one world and leaving the next one to be discovered the same way.
Vec3 spawn_in_front(const World &world, const NodeRegistry &reg, const Portal &p,
                    float stand_off)
{
    const Vec3 n = p.normal();
    // At the foot of the opening, wherever a world stood it: kGroundTop was the
    // engine's plaza, and a world with ground of its own is somewhere else.
    const Vec3 base{p.center.x + n.x * stand_off, p.center.y - p.half_h,
                    p.center.z + n.z * stand_off};

    // Sideways to the opening keeps both the distance to it and the line of
    // sight to it.
    return free_beside(world, reg, base, normalize(cross(Vec3{0.0f, 1.0f, 0.0f}, n)));
}

Vec3 spawn_in_plaza(const World &world, const NodeRegistry &reg, int extent)
{
    // The middle of the world, which is the middle of the plaza: the flat part
    // mapgen builds and the point every other thing in this engine measures
    // from. Half a node in, so the point is the centre of a cell rather than
    // its corner.
    const float mid = (float)extent * 0.5f + 0.5f;
    return free_beside(world, reg, Vec3{mid, (float)kGroundTop, mid},
                       Vec3{1.0f, 0.0f, 0.0f});
}

Vec3 spawn_on_land(const MapgenContext &ctx)
{
    // A point the seed picks, up to 2500 from the middle of the world, and
    // from there out in rings 16 apart until a column is dry, over the sea by
    // two, not a shore, and level with the ground 3 either side of it. A
    // function of the seed and the ground alone, so nothing is stored: the
    // same world finds the same place every start.
    const uint32_t h  = noise_hash(0x5EA, 0x5EED, ctx.seed);
    const float    a  = (float)(h % 3600u) * (6.2831853f / 3600.0f);
    const float    r  = (float)((h >> 12) % 2500u);
    const int      x0 = (int)std::lround(std::cos(a) * r), z0 = (int)std::lround(std::sin(a) * r);
    const size_t   beach = ctx.biomes->find("core:beach");
    const auto dry = [&](int x, int z) {
        const int top = mapgen_terrain_height(ctx, x, z);
        return mapgen_water_level(ctx, x, z) == top && top >= 20 &&
               mapgen_biome_at(*ctx.biomes, ctx.seed, x, z) != beach;
    };
    for (int ring = 0; ring <= 40; ++ring) {
        const int steps = ring == 0 ? 1 : ring * 8;
        for (int k = 0; k < steps; ++k) {
            const float t = 6.2831853f * (float)k / (float)steps;
            const int   x = x0 + (int)std::lround(std::cos(t) * 16.0f * (float)ring);
            const int   z = z0 + (int)std::lround(std::sin(t) * 16.0f * (float)ring);
            if (!dry(x, z) || !dry(x + 3, z) || !dry(x - 3, z) || !dry(x, z + 3) || !dry(x, z - 3)) continue;
            const int top = mapgen_terrain_height(ctx, x, z);
            if (std::abs(mapgen_terrain_height(ctx, x + 3, z) - top) > 2 ||
                std::abs(mapgen_terrain_height(ctx, x, z + 3) - top) > 2)
                continue;
            return Vec3{(float)x + 0.5f, (float)top, (float)z + 0.5f};
        }
    }
    std::printf("spawn: no dry level ground within 640 of the seed's point; spawning there anyway\n");
    return Vec3{(float)x0 + 0.5f, (float)mapgen_terrain_height(ctx, x0, z0), (float)z0 + 0.5f};
}

Vec3 spawn_free(const World &world, const NodeRegistry &reg, Vec3 at)
{
    return free_beside(world, reg, at, Vec3{1.0f, 0.0f, 0.0f});
}

} // namespace cnp
