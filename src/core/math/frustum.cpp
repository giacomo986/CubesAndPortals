#include "core/math/frustum.h"

namespace cnp {

Frustum frustum_from_view_projection(const Mat4 &m)
{
    // Gribb-Hartmann: each plane is a row of the matrix added to or subtracted
    // from the w row. With m[row + 4*col] indexing, "row i" is m.at(i, c).
    auto row = [&](int i) {
        return Vec4{m.at(i, 0), m.at(i, 1), m.at(i, 2), m.at(i, 3)};
    };
    const Vec4 r0 = row(0), r1 = row(1), r2 = row(2), r3 = row(3);

    auto add = [](Vec4 a, Vec4 b) { return Vec4{a.x + b.x, a.y + b.y, a.z + b.z, a.w + b.w}; };
    auto sub = [](Vec4 a, Vec4 b) { return Vec4{a.x - b.x, a.y - b.y, a.z - b.z, a.w - b.w}; };

    Frustum f;
    f.planes[0] = plane_normalize(add(r3, r0));   // left
    f.planes[1] = plane_normalize(sub(r3, r0));   // right
    f.planes[2] = plane_normalize(add(r3, r1));   // bottom
    f.planes[3] = plane_normalize(sub(r3, r1));   // top
    f.planes[4] = plane_normalize(add(r3, r2));   // near
    f.planes[5] = plane_normalize(sub(r3, r2));   // far
    return f;
}

bool Frustum::contains_point(Vec3 p) const
{
    for (const Plane &pl : planes)
        if (plane_distance(pl, p) < 0.0f) return false;
    return true;
}

bool Frustum::intersects_aabb(Vec3 min, Vec3 max) const
{
    for (const Plane &pl : planes) {
        // The corner furthest along the plane normal. If even that one is
        // behind the plane, every corner is, and the box is out.
        const Vec3 far_corner{
            pl.x >= 0.0f ? max.x : min.x,
            pl.y >= 0.0f ? max.y : min.y,
            pl.z >= 0.0f ? max.z : min.z};
        if (plane_distance(pl, far_corner) < 0.0f) return false;
    }
    return true;
}

} // namespace cnp
