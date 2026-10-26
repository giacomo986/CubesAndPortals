#pragma once

#include "core/math/mat4.h"
#include "core/math/vec.h"
#include "core/physics/aabb.h"
#include "core/portal/portal_math.h"

// Cutting a solid at a portal.
//
// A body halfway through a portal is in two places. What the previous project
// does with that is nothing: the object is drawn whole on one side and pops to
// the other when it crosses, so a crate going through a portal is a crate that
// vanishes and reappears. It is the single most visible thing missing from it,
// and this file is the answer.
//
// The rule is one sentence: **the part still in front of the entry plane is
// drawn where it is, and everything else is drawn as a copy in front of the
// exit plane**. Those two regions are exactly complementary under the portal
// transform, so the two halves make one solid with no gap and no overlap --
// which is a property a test can check on points rather than on pixels, and
// tests/portal_clip_test.cpp does.
//
// **Which plane.** The cut is the plane through the opening's centre, not
// through its outer face. Not a detail: the transform maps A's centre plane
// exactly onto B's centre plane, so cutting there makes the two halves meet.
// Cutting at A's face instead maps to B's BACK face, and the copy would start
// emerging from half a node inside the wall.
//
// **Which transform.** The renderer's rotation form (portal_transform), not the
// body's. They differ when either end is horizontal -- see portal_traverse.h --
// and the copy has to agree with what is seen THROUGH the portal, because the
// two are next to each other on screen and any disagreement is visible as a
// kink at the opening. The consequence is that for a horizontal pair the copy
// and the place the body actually arrives disagree. The fork has the same
// disagreement for the same reason; resolving it is on M4's list, and it is
// written down rather than left to be discovered.

namespace cnp {

// What to draw for one solid at one portal.
struct PortalCut {
    // False means the solid is nowhere near this portal's opening: draw it
    // whole, draw no copy, and do not clip. Anything else would cut a crate in
    // half because it happened to be level with a portal on the other side of
    // the room.
    bool cut = false;

    // Both are "keep where the plane evaluates to >= 0". `here` keeps the part
    // that has not gone through; `there` keeps the part that has.
    //
    // Both keep the plane ITSELF, so the two halves meet with a hair of overlap
    // rather than a hair of gap. That is a choice and it goes the way it does
    // for a reason: an overlap on the seam is two coplanar surfaces at the same
    // depth in the same colour, which is invisible, while a gap of the same
    // size is the background showing through the middle of a body. A GPU keeps
    // a fragment whose clip distance is exactly zero, so this is also the
    // behaviour that comes for free -- what would cost effort is the wrong one.
    Plane keep_here;
    Plane keep_there;

    // World to world. Applied to the solid to get the copy.
    Mat4 clone_transform;
};

// The plane of an opening, oriented so that the front of the portal is the
// positive side.
Plane portal_plane(const PortalSurface &p);

// The same for the end a body comes OUT of. See portal_plane_out: a doorway is
// entered through one face and left through the other, so the two ends are not
// the same plane once the image sits anywhere but the middle of the block.
Plane portal_exit_plane(const PortalSurface &p);

// Decides the cut for a solid whose world-space bounds are `box`.
//
// Cut when the box crosses the plane AND overlaps the opening laterally. The
// second condition is what stops a portal slicing everything that happens to be
// coplanar with it: a portal is a hole of a particular size, not an infinite
// blade.
PortalCut portal_cut(const PortalSurface &src, const PortalSurface &dst, const AABB &box);

// Where the copy ends up, for culling. The transform is a rotation, so this is
// the bounds of the rotated box and not of the box's own axes.
AABB portal_clone_bounds(const Mat4 &clone_transform, const AABB &box);

// True when `p` should be drawn by the original half. The copy draws exactly
// the points for which this is false -- that is the property, and it is what
// the test checks a few hundred thousand times.
bool cut_keeps_here(const PortalCut &cut, Vec3 p);
bool cut_keeps_there(const PortalCut &cut, Vec3 p);

} // namespace cnp
