#include "core/voxel/raycast.h"

#include "core/physics/aabb.h"
#include "core/registry/node_shape.h"

#include <cmath>

namespace cnp {
namespace {

// Whether the ray meets the block in this cell, and where.
//
// A cube is met the moment the ray is in its cell, which is what the traversal
// already established -- so `t` and the face come from the boundary it crossed,
// and this says yes without arithmetic. A block with a SHAPE is a different
// question: its boxes are somewhere inside the cell, and the ray may cross the
// cell without touching any of them. Aiming at the sky over a slab used to
// select the slab, because the cell was the block.
//
// The boxes come from registry/node_shape.h, which is where the collision
// resolver gets them too: what a body is stopped by and what an aim finds are
// the same boxes, or a player is selecting a block they can walk through.
bool ray_meets_cell(const World &world, const NodeRegistry &reg, int bx, int by, int bz,
                    Vec3 origin, Vec3 dir, float max_distance, bool liquids, float &t,
                    int normal[3])
{
    const BlockId id = world.get_block(bx, by, bz);
    if (liquids && reg.is_liquid(id)) return true;   // the whole cell, like a cube

    // A block that is not solid but has boxes -- a crop -- is walked through
    // and still aimed at by them: what would a hoe or a hand be dug with else.
    const NodeDef &def = reg.get(id);
    if (!reg.is_solid(id) && !node_has_shape(def)) return false;
    if (!node_has_shape(def)) return true;   // the whole cell: the caller already knows where

    const uint8_t facing = world.get_facing(bx, by, bz);
    const int     count  = node_shape_count(def);

    bool  found = false;
    float best  = max_distance;
    for (int i = 0; i < count; ++i) {
        const NodeBox nb = node_shape_box(def, facing, i, bx, by, bz);
        const AABB    part{Vec3{nb.min[0], nb.min[1], nb.min[2]},
                        Vec3{nb.max[0], nb.max[1], nb.max[2]}};

        float hit_t = 0.0f;
        int   hit_n[3] = {0, 0, 0};
        if (!ray_hits_box(part, origin, dir, max_distance, hit_t, hit_n)) continue;
        if (found && hit_t >= best) continue;

        found     = true;
        best      = hit_t;
        t         = hit_t;
        normal[0] = hit_n[0];
        normal[1] = hit_n[1];
        normal[2] = hit_n[2];
    }
    return found;
}

} // namespace

RaycastHit raycast(const World &world, const NodeRegistry &reg, Vec3 origin, Vec3 direction,
                   float max_distance, bool liquids)
{
    RaycastHit result;

    const Vec3 dir = normalize(direction);
    if (length_sq(dir) < 0.5f) return result;   // degenerate direction

    int voxel[3] = {(int)std::floor(origin.x), (int)std::floor(origin.y), (int)std::floor(origin.z)};
    const float o[3] = {origin.x, origin.y, origin.z};
    const float d[3] = {dir.x, dir.y, dir.z};

    int   step[3];
    float t_max[3];
    float t_delta[3];

    for (int a = 0; a < 3; ++a) {
        if (d[a] > 1e-8f) {
            step[a]    = 1;
            t_delta[a] = 1.0f / d[a];
            t_max[a]   = ((float)(voxel[a] + 1) - o[a]) / d[a];
        } else if (d[a] < -1e-8f) {
            step[a]    = -1;
            t_delta[a] = -1.0f / d[a];
            t_max[a]   = ((float)voxel[a] - o[a]) / d[a];
        } else {
            // Exactly parallel to this axis: it never crosses a boundary here.
            step[a]    = 0;
            t_delta[a] = 1e30f;
            t_max[a]   = 1e30f;
        }
    }

    // Standing inside a solid block counts as an immediate hit, with no face:
    // the alternative is the ray escaping the block it started in and selecting
    // something across the room.
    //
    // With a SHAPE in the cell it is not the cell that decides that any more.
    // Half of a slab's cell is air, and a player standing in that half is
    // standing in the cell of a solid block without being inside it -- so the
    // boxes are asked, and a ray that misses them all carries on. It can still
    // meet one further along: looking down at the slab you are standing on is a
    // ray that starts in its cell and ends on its top.
    {
        float t = 0.0f;
        int   n[3] = {0, 0, 0};
        if (ray_meets_cell(world, reg, voxel[0], voxel[1], voxel[2], origin, dir, max_distance, liquids, t,
                           n)) {
            result.hit = true;
            result.x = voxel[0]; result.y = voxel[1]; result.z = voxel[2];
            result.distance = t;
            result.nx = n[0]; result.ny = n[1]; result.nz = n[2];
            return result;
        }
    }

    float travelled = 0.0f;
    while (travelled <= max_distance) {
        // Cross whichever boundary is nearest.
        int axis = 0;
        if (t_max[1] < t_max[axis]) axis = 1;
        if (t_max[2] < t_max[axis]) axis = 2;

        travelled = t_max[axis];
        if (travelled > max_distance) break;

        voxel[axis] += step[axis];
        t_max[axis] += t_delta[axis];

        float box_t = travelled;
        int   box_n[3] = {0, 0, 0};
        if (!ray_meets_cell(world, reg, voxel[0], voxel[1], voxel[2], origin, dir, max_distance,
                            liquids, box_t, box_n))
            continue;

        result.hit      = true;
        result.x        = voxel[0];
        result.y        = voxel[1];
        result.z        = voxel[2];
        result.distance = box_t;

        // The face entered through is the one on the axis just crossed, facing
        // back the way the ray came -- for a cube, where the cell boundary IS
        // the block's face. A shape has faces of its own inside the cell, and
        // the top of a slab is not the top of its cell: there the box says
        // which face, because that is the face a block gets built on.
        if (box_n[0] == 0 && box_n[1] == 0 && box_n[2] == 0) {
            if (axis == 0) result.nx = -step[0];
            if (axis == 1) result.ny = -step[1];
            if (axis == 2) result.nz = -step[2];
        } else {
            result.nx = box_n[0];
            result.ny = box_n[1];
            result.nz = box_n[2];
        }
        return result;
    }

    return result;
}

} // namespace cnp
