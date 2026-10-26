#pragma once

#include "core/math/mat4.h"
#include "core/math/vec.h"

namespace cnp {

// Six planes pulled straight out of a view-projection matrix, each oriented so
// that a point inside the frustum has a non-negative distance to all of them.
//
// Lives in core/ rather than in the renderer because the portal pass needs a
// frustum for a camera that does not exist as a camera -- the virtual one --
// and because it is worth being able to test culling without a GPU.
struct Frustum {
    Plane planes[6];   // left, right, bottom, top, near, far

    bool contains_point(Vec3 p) const;

    // Conservative: a box that only touches the frustum counts as inside. The
    // cost of a false positive is one wasted draw call; the cost of a false
    // negative is a hole in the world.
    bool intersects_aabb(Vec3 min, Vec3 max) const;
};

Frustum frustum_from_view_projection(const Mat4 &view_projection);

} // namespace cnp
