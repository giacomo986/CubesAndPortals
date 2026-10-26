#pragma once

#include "core/math/mat4.h"
#include "core/math/vec.h"

// Portal geometry and the two clipping techniques that make a portal look like
// a hole instead of a picture. This lives in core/ and touches no GPU API: it
// is pure math, so the whole file is testable headless, which is the point.
//
// Every constant here was lifted from the fork of the original engine this project replaces
// (its src/client/render/portal.cpp), where each one is a paid-for bug
// rather than a tuning preference. That fork worked in the original engine's internal units
// where one node is BS = 10; here one node is 1.0, so 0.05f*BS became 0.05f.
//
// The fork also ran on Irrlicht's LEFT-handed projection with +z_eye pointing
// forward. raylib is RIGHT-handed with -z_eye forward, so the oblique near
// plane below is the right-handed derivation, not a transcription.

namespace cnp {

inline constexpr float kNodeSize = 1.0f;

// The surface of a portal opening.
//
// `center` is the NODE CENTER, not the visible surface: the real surface is
// half a node further out along `normal`. Conflating the two is what made the
// solid front half of the exit node cover the render target at grazing angles
// in the old implementation, so the distinction is kept explicit here.
struct PortalSurface {
    Vec3  center;
    Vec3  normal;               // unit; points out of the opening, toward the approach side
    Vec3  up;                    // unit; lies in the plane of the opening
    float half_w = 0.5f;
    float half_h = 1.0f;

    Vec3 right() const { return normalize(cross(up, normal)); }
    Vec3 face()  const { return center + normal * (0.5f * kNodeSize); }
};

// Where a camera on the approach side of `src` has to be placed, relative to
// `dst`, to show what someone looking through the pair would see. The mapping
// rotates 180 degrees about the destination's up axis, so a camera two nodes in
// front of the source ends up two nodes BEHIND the destination, looking out
// through it.
Mat4 portal_transform(const PortalSurface &src, const PortalSurface &dst);

struct VirtualCamera {
    Vec3 pos;
    Vec3 forward;               // normalized
    Vec3 up;                    // normalized
};

VirtualCamera portal_virtual_camera(const PortalSurface &src, const PortalSurface &dst,
                                    Vec3 cam_pos, Vec3 cam_forward, Vec3 cam_up);

// Oblique near plane (Lengyel), right-handed, depth [-1, 1].
//
// Returns false and leaves `out` untouched when the technique must not be
// applied. That is not an error path: it is the correct answer in two cases the
// old implementation learned the hard way.
//
//   1. The virtual camera is not on the destination side. When the player walks
//      INTO the frame, the eye-space plane's w flips sign, the near plane lands
//      behind the camera, and the portal renders solid black. A standard
//      projection is both correct and cheaper there, because nothing between
//      the camera and the portal needs clipping anyway.
//   2. The view is grazing (the plane is nearly edge-on) or the system is
//      degenerate. Pushing through either produces a projection full of NaN.
//
// The caller must fall back to `proj` unchanged when this returns false.
bool portal_oblique_near(const Mat4 &proj, const Mat4 &view, const PortalSurface &dst,
                         Vec3 vcam_pos, Mat4 *out);

// The four lateral planes plus the destination face plane, in world space,
// oriented so that gl_ClipDistance[k] >= 0 means "keep".
//
// The near plane alone is not enough: at grazing angles the destination's own
// frame leaks in around the edges of the image. These five planes are what stop
// that.
struct PortalClipPlanes {
    Plane planes[5];
    int   count = 0;
};

// Returns false when no clipping should happen at all, which is the case when
// the player is standing fully inside the opening. Note "fully": being at frame
// depth but laterally outside the opening still needs the planes, or the frame
// leaks. Getting that condition too loose was a real bug.
bool portal_clip_planes(const PortalSurface &dst, Vec3 vcam_pos, PortalClipPlanes *out);

// Transforms a plane by a matrix. Planes transform by the inverse transpose,
// not by the matrix itself; this is the routine that carries a world-space clip
// plane into eye space.
Plane plane_transform(const Mat4 &m, Plane p);

} // namespace cnp
