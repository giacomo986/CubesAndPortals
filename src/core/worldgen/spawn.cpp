#include "core/worldgen/spawn.h"

#include <cstdio>

#include "core/physics/movement.h"
#include "core/worldgen/worldgen.h"

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

} // namespace cnp
