#include "core/portal/portal_clip.h"

#include "core/portal/portal_tuning.h"

#include <algorithm>
#include <cmath>

namespace cnp {
namespace {

// How far past the edge of the opening a solid may reach and still be treated
// as going through it. A crate wider than the hole is a case this engine has no
// answer for -- there is no collision against the frame yet -- so the choice is
// between cutting it anyway and not cutting it at all, and cutting it anyway is
// the one that keeps a body that is mostly through the hole looking right.
constexpr float kLateralSlack = 0.5f;

float plane_eval(const Plane &pl, Vec3 p) { return pl.x * p.x + pl.y * p.y + pl.z * p.z + pl.w; }

} // namespace

// The plane a body is cut on at the END IT GOES INTO: where the doorway hands
// over, config/portal.cfg's `plane` deep into the block.
Plane portal_plane(const PortalSurface &p)
{
    return plane_from_point_normal(portal_plane_in(p), p.normal);
}

// And the plane the copy is kept against at the end it COMES OUT OF, which is
// the same doorway from the other side -- see portal_plane_out. Using the entry
// plane at both ends leaves the two halves overlapping by twice the offset: 36
// of 36 orientation pairs cut wrong, which is how this was noticed.
//
// portal_view_origin and not portal_plane_out, because the copy is PLACED by
// portal_transform, whose destination end is that same viewpoint. The plane has
// to travel with the map it cuts: leave it on the emerging face while the map
// carries the copy a block past it and the whole copy is on the kept side, so
// a body halfway through the hole is drawn twice, whole, at both ends.
Plane portal_exit_plane(const PortalSurface &p)
{
    return plane_from_point_normal(portal_view_origin(p), p.normal);
}

PortalCut portal_cut(const PortalSurface &src, const PortalSurface &dst, const AABB &box)
{
    PortalCut out;

    const Vec3 n = src.normal;
    const Vec3 r = src.right();
    const Vec3 u = src.up;

    // The box's extent along each of the portal's own axes. An axis-aligned box
    // projected onto a rotated axis has half-extent |h . a| summed per
    // component, which is the standard separating-axis projection and not the
    // same as the box's own half-extents.
    const Vec3 c = Vec3{(box.min.x + box.max.x) * 0.5f, (box.min.y + box.max.y) * 0.5f,
                        (box.min.z + box.max.z) * 0.5f};
    const Vec3 h = Vec3{(box.max.x - box.min.x) * 0.5f, (box.max.y - box.min.y) * 0.5f,
                        (box.max.z - box.min.z) * 0.5f};

    auto extent_along = [&](Vec3 a) {
        return std::fabs(h.x * a.x) + std::fabs(h.y * a.y) + std::fabs(h.z * a.z);
    };

    const Vec3  rel     = c - portal_plane_in(src);
    const float depth   = dot(rel, n);
    const float depth_h = extent_along(n);

    // Entirely on one side: nothing to cut.
    if (std::fabs(depth) >= depth_h) return out;

    // Level with the opening but off to one side: also nothing to cut, and this
    // is the condition that matters. Without it every portal is an infinite
    // blade across its own plane, and a crate on the far side of the room gets
    // sliced because it happens to be at the same height.
    const float lat_r = std::fabs(dot(rel, r));
    const float lat_u = std::fabs(dot(rel, u));
    if (lat_r > src.half_w + extent_along(r) + kLateralSlack) return out;
    if (lat_u > src.half_h + extent_along(u) + kLateralSlack) return out;

    out.cut             = true;
    out.keep_here       = portal_plane(src);
    out.keep_there      = portal_exit_plane(dst);
    out.clone_transform = portal_transform(src, dst);
    return out;
}

AABB portal_clone_bounds(const Mat4 &clone_transform, const AABB &box)
{
    // Every corner through the transform, because the map is a rotation: the
    // image of an axis-aligned box is not axis aligned, and transforming only
    // the two extreme corners gives a box that does not contain the solid.
    AABB out;
    bool first = true;

    for (int i = 0; i < 8; ++i) {
        const Vec3 corner{(i & 1) ? box.max.x : box.min.x, (i & 2) ? box.max.y : box.min.y,
                          (i & 4) ? box.max.z : box.min.z};
        const Vec3 p = mat4_transform_point(clone_transform, corner);

        if (first) {
            out.min = out.max = p;
            first             = false;
            continue;
        }
        out.min.x = std::min(out.min.x, p.x);
        out.min.y = std::min(out.min.y, p.y);
        out.min.z = std::min(out.min.z, p.z);
        out.max.x = std::max(out.max.x, p.x);
        out.max.y = std::max(out.max.y, p.y);
        out.max.z = std::max(out.max.z, p.z);
    }
    return out;
}

bool cut_keeps_here(const PortalCut &cut, Vec3 p)
{
    if (!cut.cut) return true;
    return plane_eval(cut.keep_here, p) >= 0.0f;
}

bool cut_keeps_there(const PortalCut &cut, Vec3 p)
{
    if (!cut.cut) return false;
    return plane_eval(cut.keep_there, mat4_transform_point(cut.clone_transform, p)) >= 0.0f;
}

} // namespace cnp
