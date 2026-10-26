#include "core/portal/portal_math.h"

#include "core/portal/portal_tuning.h"

#include <cmath>

namespace cnp {
namespace {

// Standoff that keeps the frustum apex just outside the destination face.
// Leaving the apex at the virtual camera instead is the trap: when the camera
// sits deep behind the destination, the four lateral planes converge to a
// half-angle of about atan(half_w / depth) and clip away most of the world you
// were trying to show. Pulling the apex to the face gives a near-90-degree
// frustum that still passes through the opening's edges, so it clips the frame
// and nothing else.
constexpr float kApexStandoff = 0.05f * kNodeSize;

// Where the destination's image starts: the portal plane, through the middle of
// the node.
//
// It used to be 0.49, the node's outer face, and that was a half-node of image
// thrown away. Everything else in this engine already agrees that the portal is
// the CENTRE plane -- portal_clip cuts a solid there, portal_traverse fires
// there, and portal_render's back quad sits at the image of exactly this plane
// -- so anchoring the near plane half a node further out meant the render
// target held nothing for the half node the player is actually walking through.
// Two things that needs: the inside of the hole, which is the other place from
// the portal plane back and cannot be drawn without an image for it; and the
// half of a body that has gone through, which portal_clip puts at destination
// depth 0 and up, where 0.49 clipped it away.
//
// Zero rather than a hair positive: a hair positive is the value that has to be
// justified, and the thing the old comment worried about -- z-fighting with a
// block built flush against the portal -- is half a node away from here.
constexpr float kNearPlaneInset = 0.0f * kNodeSize;

float sgnf(float v) { return (v >= 0.0f) ? 1.0f : -1.0f; }

// Builds the basis of a portal: columns are right, up, normal, and the origin.
//
// `going_in` picks WHICH plane is the origin. The transform glues the face the
// player leaves the source through to the face they emerge from at the
// destination, and with the image inside the block those are opposite faces of
// their blocks -- see portal_plane_in / portal_plane_out.
//
// The destination end is portal_view_origin and not portal_plane_out: the
// emerging face pushed `view` blocks further out, so that what the opening
// shows at walking distance is the area outside the destination rather than the
// inside of its doorway. That offset is what makes this a viewpoint rather than
// a hole, and portal_tuning.h states what it costs.
Mat4 portal_basis(const PortalSurface &p, bool going_in)
{
    const Vec3 r = p.right();
    Mat4 m;
    const Vec3 o = going_in ? portal_plane_in(p) : portal_view_origin(p);
    m.at(0, 0) = r.x;        m.at(0, 1) = p.up.x; m.at(0, 2) = p.normal.x; m.at(0, 3) = o.x;
    m.at(1, 0) = r.y;        m.at(1, 1) = p.up.y; m.at(1, 2) = p.normal.y; m.at(1, 3) = o.y;
    m.at(2, 0) = r.z;        m.at(2, 1) = p.up.z; m.at(2, 2) = p.normal.z; m.at(2, 3) = o.z;
    m.at(3, 0) = 0.0f;       m.at(3, 1) = 0.0f;   m.at(3, 2) = 0.0f;       m.at(3, 3) = 1.0f;
    return m;
}

} // namespace

Plane plane_transform(const Mat4 &m, Plane p)
{
    // A plane's coefficients are a covector: they transform by the inverse
    // transpose. Applying the matrix directly is the classic silent bug --
    // correct for pure rotation, wrong the moment there is translation.
    const Mat4 it = mat4_transpose(mat4_invert(m));
    const Vec4 r  = mat4_transform_vec4(it, Vec4{p.x, p.y, p.z, p.w});
    return Plane{r.x, r.y, r.z, r.w};
}

Mat4 portal_transform(const PortalSurface &src, const PortalSurface &dst)
{
    // 180 degrees about the local up axis: right and normal flip, up stays.
    Mat4 flip;
    flip.at(0, 0) = -1.0f;
    flip.at(2, 2) = -1.0f;

    return mat4_multiply(portal_basis(dst, false),
                         mat4_multiply(flip, mat4_invert(portal_basis(src, true))));
}

VirtualCamera portal_virtual_camera(const PortalSurface &src, const PortalSurface &dst,
                                    Vec3 cam_pos, Vec3 cam_forward, Vec3 cam_up)
{
    const Mat4 t = portal_transform(src, dst);
    VirtualCamera v;
    v.pos     = mat4_transform_point(t, cam_pos);
    v.forward = normalize(mat4_transform_dir(t, cam_forward));
    v.up      = normalize(mat4_transform_dir(t, cam_up));
    return v;
}

bool portal_oblique_near(const Mat4 &proj, const Mat4 &view, const PortalSurface &dst,
                         Vec3 vcam_pos, Mat4 *out)
{
    // Guard 1: only clip when the virtual camera is genuinely behind the
    // destination. See the header for why the black-portal case lives here.
    const float side = dot(vcam_pos - portal_clip_point(dst), dst.normal);
    if (side >= -0.001f * kNodeSize) return false;

    const Vec3  clip_origin = portal_clip_point(dst) + dst.normal * kNearPlaneInset;
    const Plane world_plane = plane_from_point_normal(clip_origin, dst.normal);
    const Plane c = plane_normalize(plane_transform(view, world_plane));

    // Guard 2a: grazing view. The destination normal points away from the
    // virtual camera and the camera looks down -z in eye space, so c.z is
    // negative in the healthy case; near zero means the plane is edge-on and
    // the technique degenerates.
    if (c.z > -1e-4f) return false;

    const Mat4 &p = proj;
    const Vec4 q{(sgnf(c.x) + p.m[8]) / p.m[0],
                 (sgnf(c.y) + p.m[9]) / p.m[5],
                 -1.0f,
                 (1.0f + p.m[10]) / p.m[14]};

    const float denom = dot(Vec4{c.x, c.y, c.z, c.w}, q);
    // Guard 2b: degenerate system.
    if (std::fabs(denom) <= 1e-6f) return false;

    const float s = 2.0f / denom;

    // Replace the third row of the projection with the scaled clip plane. The
    // +1 on m10 absorbs the -1 sitting in the fourth row of a right-handed
    // perspective matrix; drop it and the far plane inverts.
    Mat4 r = proj;
    r.m[2]  = c.x * s;
    r.m[6]  = c.y * s;
    r.m[10] = c.z * s + 1.0f;
    r.m[14] = c.w * s;

    *out = r;
    return true;
}

bool portal_clip_planes(const PortalSurface &dst, Vec3 vcam_pos, PortalClipPlanes *out)
{
    const Vec3  pr = dst.right();
    const Vec3  pu = dst.up;
    const Vec3  pc = portal_plane_out(dst);
    const Vec3  bn = dst.normal;
    const float hw = dst.half_w;
    const float hh = dst.half_h;

    const Vec3  delta     = vcam_pos - pc;
    const float apex_side = dot(delta, bn);
    const float lx        = dot(delta, pr);
    const float ly        = dot(delta, pu);

    // Standing fully inside the opening: no clipping, or we would carve up the
    // world around the player. Depth alone is not enough -- at frame level but
    // laterally outside the hole, the frame still leaks and the planes must
    // stay on.
    const bool inside_frame =
        apex_side > 0.0f && std::fabs(lx) < hw && std::fabs(ly) < hh;
    if (inside_frame) return false;

    // With the doorway's side faces set to be a window, the four lateral planes
    // come off and only the depth plane stays.
    //
    // Those four exist to hide whatever stands BESIDE the destination's
    // opening, and that is precisely what a window is asked to show. Dropping
    // them is the whole mechanism -- there is no second render, no wider cone.
    // Widening the cone was tried first, by anchoring the four at the aperture
    // of the doorway's mouth instead of at the image plane, and it fails for a
    // reason worth writing down: kApexStandoff pulls the frustum apex to within
    // 0.05 of the face, so a mouth a whole node further back sits BEHIND the
    // apex, every plane flips, and the image loses half its pixels. Measured at
    // 49.38% of the aperture differing from the reference at 6.5 blocks.
    //
    // What makes dropping them safe here is the plane's new home. The depth
    // plane sits on the face the player emerges from, so the destination's own
    // block is entirely behind it and cannot leak through the aperture at any
    // angle. That was NOT true while the image sat at the node centre, where
    // half the destination's block stood in front of the clip plane -- which is
    // why these four were written in the first place.
    if (portal_tuning().sides_are_window()) {
        out->planes[0] = plane_from_point_normal(portal_clip_point(dst), bn);
        out->count     = 1;
        return true;
    }

    // Move the apex to a fixed standoff behind the face, keeping the camera's
    // lateral offset. See kApexStandoff.
    const Vec3 apex = vcam_pos - bn * (apex_side + kApexStandoff);

    // Plane through `apex` and the edge (edge_center, edge_dir), oriented so
    // that `interior` is on the keep side.
    auto make_plane = [&](Vec3 edge_center, Vec3 edge_dir, Vec3 interior) -> Plane {
        const Vec3 v = edge_center - apex;
        Vec3 n = cross(v, edge_dir);
        float len = length(n);
        if (len < 1e-6f) {
            // Degenerate edge: a plane that keeps everything, so a numerical
            // accident can never blank the portal.
            return Plane{0.0f, 0.0f, 1.0f, 1e15f};
        }
        n = n * (1.0f / len);
        if (dot(n, interior - apex) < 0.0f) n = -n;

        const float nbn = dot(n, bn);
        if (nbn < 0.0f) {
            // The plane tilts backward, which happens once the apex is pushed
            // laterally past the opening's edge. Left alone it would eat the
            // destination interior. Remove the component along the portal
            // normal to get a corridor plane perpendicular to the face, and
            // re-anchor it on the edge rather than on the apex.
            n = n - bn * nbn;
            const float nlen = length(n);
            if (nlen < 1e-6f) return Plane{0.0f, 0.0f, 1.0f, 1e15f};
            n = n * (1.0f / nlen);
            return Plane{n.x, n.y, n.z, -dot(n, edge_center)};
        }
        return Plane{n.x, n.y, n.z, -dot(n, apex)};
    };

    out->planes[0] = make_plane(pc + pr * hw, pu, pc - pr * hw);   // right
    out->planes[1] = make_plane(pc - pr * hw, pu, pc + pr * hw);   // left
    out->planes[2] = make_plane(pc + pu * hh, pr, pc - pu * hh);   // top
    out->planes[3] = make_plane(pc - pu * hh, pr, pc + pu * hh);   // bottom

    // Fifth plane: the depth clip that survives when the oblique near plane
    // switches itself off at grazing angles (guard 2a). Same plane as that one,
    // for the same reason -- the portal is the centre of the node.
    //
    // It used to sit on the node's OUTER FACE, to cut away the front half of an
    // exit node that is itself a solid block. That is a real case, but paying
    // for it with half a node of every destination is not: the price was the
    // floor of the exit's own opening, which is what a ray leaving through the
    // bottom of a portal grazes along, and the half of a body that has gone
    // through, which portal_clip puts at destination depth zero and up. A solid
    // exit node has to be excluded by not drawing that node, not by blanking
    // the half node around it.
    out->planes[4] = plane_from_point_normal(portal_clip_point(dst), bn);

    out->count = 5;
    return true;
}

} // namespace cnp
