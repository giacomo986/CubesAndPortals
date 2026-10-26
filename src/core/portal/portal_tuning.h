#pragma once

#include "core/portal/portal_math.h"

#include <string>

// The shape of a portal, as six numbers a person can change without a
// compiler.
//
// Everything here used to be a constant spread across three files: the tunnel's
// near end in portal_render.cpp, the crossing depth in portal_traverse.h, the
// image plane implied by whichever file was being read. They are one shape and
// they have to agree, so they live together and load from one file.
//
// ---------------------------------------------------------------- the convention
//
// A portal is a BLOCK, one node thick, and every number below is a fraction of
// that block measured ALONG THE DIRECTION OF TRAVEL from the face the player
// enters it through. Zero is the face they come in at, one is the face they
// would come out of.
//
// That convention is why the two depth conversions below are not the same
// formula. A player enters the source block through its FRONT face, at +0.5
// from the node centre along the normal, and walks inward; they enter the
// destination block through its BACK face, at -0.5, and walk outward. Same
// sentence, opposite ends.
namespace cnp {

struct PortalTuning {
    // Where the image sits inside the block, and with it the plane the crossing
    // is measured against, the plane a body is cut on, and the plane the two
    // worlds are glued along. 1.0 puts it on the far face, so the whole block
    // is a doorway seen from the approach side; 0.5 puts it in the middle,
    // which is where it used to be.
    float plane = 1.00f;

    // Where the doorway's side faces begin. 0.0 is the face of the block.
    float tunnel_start = 0.00f;

    // How deep into the block the eye has to be for the crossing to fire.
    float trigger = 0.10f;

    // How deep into the destination block the player is put. 1.0 is the far
    // face; past it they land in the open in front of the destination, which is
    // where the seamless exit is whenever `view` is not zero.
    //
    // This is a POSITION, not an offset, and that is a decision with a cost.
    // The alternative is the mirror -- exit depth = -entry depth -- which is
    // what makes the view before a crossing and the view after it the same
    // picture, measured at 0.0000 in M4. A fixed exit gives that up unless it is
    // set to the value the map itself lands on -- `exit = trigger + view` --
    // and every hundredth away from that is a hundredth of a node of jump at
    // the moment of crossing. The gate prints the number.
    float exit = 1.10f;

    // How far PAST the face the player emerges from the destination's viewpoint
    // sits, in blocks. 0 is the face itself, which is where it was.
    //
    // This is the one number here that is not a property of the doorway but of
    // what is SHOWN through it, and it is the only one that gives up an
    // invariant, so the cost is written down rather than discovered.
    //
    // With 0 the image is a true portal view: the map glues the face the player
    // leaves the source through to the face they emerge from at the
    // destination, and every ray through the opening is the ray a real hole
    // would carry. It also means the viewpoint is one full block INSIDE the
    // destination's doorway whenever the player is a block in front of the
    // source's, so what the opening shows at walking distance is the inside of
    // that doorway -- the destination's own wall, from behind. Reported from
    // play in exactly those terms, twice.
    //
    // With 1 the viewpoint is one block further out at every distance, so the
    // doorway seen through the opening is one block shallower and the last
    // frames before a crossing show the area OUTSIDE the destination rather
    // than the tunnel into it. What it costs: the image is no longer the
    // picture a real hole in the wall would carry -- it is that picture taken
    // one block ahead -- so it no longer lines up with the world at the
    // opening's edges, and the crossing jumps by the distance between where the
    // image put the eye and where `exit` puts the body. Both are measured:
    // run_verify compares the image against a real camera AT THIS VIEWPOINT
    // (0 differing pixels, which says the offset is applied consistently, not
    // that the illusion is intact), and portal_crossing prints the jump.
    //
    // The seamless exit with a viewpoint offset is exit = trigger + view.
    float view = 1.00f;

    // Where the oblique near plane goes: how far BEHIND the plane the map glues
    // to -- the viewpoint, portal_view_origin -- it sits, in blocks.
    //
    // The plane's job is to take the destination's own wall out from between
    // the virtual camera and the opening, and it has a failure on each side.
    // Too SMALL and the plane sits near the viewpoint and cuts too much: the
    // destination's ground goes with its wall, the side faces are asked what is
    // beside the opening, the render target holds nothing there, and they come
    // back CLEAR COLOUR. Too LARGE and the wall is left standing: it gets
    // drawn against the virtual camera, the side faces show it, and it is close
    // enough to the geometry it is painted on to fight it for depth.
    //
    // Both were found by playing. Neither is visible to the continuity gate,
    // which compares the middle of the aperture and reads 0.00% for every value
    // tried, including one that paints a quarter of the opening in clear
    // colour. dual_session's third section is the one that measures them.
    //
    // The value that keeps the doorway rule is `plane - tunnel_start` -- one
    // doorway behind the glued plane, which is what makes the tunnel the player
    // walks down and the destination's own block the SAME volume rather than
    // two. Anything else is a deliberate experiment, and the engine says so at
    // startup rather than leaving it to be discovered.
    float clip = 1.00f;

    // What the doorway's side faces are made of.
    //
    // 1 = a window: they sample the destination like the far quad does, so a
    // player looking into the doorway at an angle sees whatever is beside the
    // destination's opening -- its own wall if it has one, open sky if it does
    // not. Asked for in exactly those terms: "nella destinazione potrebbe non
    // esserci un muro".
    //
    // 0 = wall: they are painted kPortalWallColour, flat, which is what they
    // were. Honest about being this world's wall and silent about the other's.
    //
    // The window costs something real and it is named here rather than
    // discovered: the four lateral clip planes have to be widened from the
    // aperture at the image plane to the aperture at the doorway's MOUTH, or
    // the render target holds nothing where these fragments sample it -- which
    // is how the first attempt at this filled the bottom of a floor-level
    // opening with clear colour. Widening lets the destination's own frame into
    // the target exactly where these faces look, and nowhere else: every ray
    // through the far aperture was already inside the wider cone, so what the
    // opening itself shows does not change.
    float sides = 1.0f;

    // ------------------------------------------------------------- one way
    //
    // Whether a portal exists at all from behind.
    //
    // 1 = one way: from the back of the opening nothing is drawn and nothing is
    // rendered for it -- no image, no doorway sides, and the target pass is
    // skipped. What is left where the portal was is the hole in the wall it is
    // carved into and whatever stands on the other side of that wall, which is
    // what a doorway with nothing special about it looks like. Crossing from
    // behind was never possible: `entered_from_front` is set from the depth on
    // the frame the player enters the bounds, so somebody arriving from the
    // back never arms the trigger. This makes the picture agree with that.
    //
    // 0 = both ways: the opening is painted from either side, which is what it
    // did. It shows the destination from behind as well -- a portal you can see
    // through but cannot walk through, which is the picture and the body
    // disagreeing about which way the thing works.
    //
    // The cost is stated rather than discovered: with this on, a pair seen from
    // the back is a hole into the room behind the wall. In the spike scenes the
    // slab is one node thick and the far side is open field, so what shows
    // through is field -- correct, and less interesting than an image.
    float one_way = 1.0f;

    // ------------------------------------------------ depths from the node centre
    //
    // What the geometry actually works in: signed distance along the portal's
    // normal, positive on the approach side.
    float plane_depth()   const { return (0.5f - plane) * kNodeSize; }
    float tunnel_depth()  const { return (0.5f - tunnel_start) * kNodeSize; }
    float trigger_depth() const { return (0.5f - trigger) * kNodeSize; }
    float exit_depth()    const { return (-0.5f + exit) * kNodeSize; }
    float view_depth()    const { return view * kNodeSize; }
    float clip_depth()    const { return clip * kNodeSize; }

    // How deep the doorway is: from where its side faces begin to where the
    // image sits. Zero when the two coincide, and then there are no side faces
    // to make a window out of.
    float doorway_depth() const { return (plane - tunnel_start) * kNodeSize; }

    bool sides_are_window() const { return sides >= 0.5f; }
    bool is_one_way()       const { return one_way >= 0.5f; }
};

// The one the engine reads. Every consumer calls this rather than holding a
// copy, so a reload changes the shape everywhere at once.
const PortalTuning &portal_tuning();

// Replaces it. Returns false and fills `err` when a value is outside its range;
// nothing is applied in that case, because half a shape is worse than the old
// one.
bool portal_tuning_set(const PortalTuning &t, std::string &err);

// Reads `key = value` lines, `#` to end of line is a comment. An unknown key is
// an error rather than a shrug: a typo in a config file that silently does
// nothing is the kind of thing that costs an afternoon.
//
// Returns false and fills `err` on a bad file. A MISSING file is not an error
// -- `found` comes back false and the built-in defaults stand.
bool portal_tuning_load(const char *path, bool *found, std::string &err);

// Searches the usual places for portal.cfg and loads the first one that exists:
// $CNP_PORTAL_CONFIG, ./config/portal.cfg, ../config/portal.cfg. Prints what it
// used. The two relative paths are for running from the source root and from
// the build directory, which are both normal here.
bool portal_tuning_load_default(std::string &err);

// The plane of an opening in world space. `center` stays the NODE centre --
// every scene, mod and gate places portals by their node -- and the plane is
// derived from it, so moving the plane moves one number and not a hundred
// coordinates.
//
// There are TWO of them, and that is the whole geometry of a doorway.
//
// An opening you are looking INTO, or walking into, is glued along the face
// you would come out the far side of: with `plane` at 1.0 that is its back
// face, and everything between it and the block's front face is this world's
// wall -- the tunnel you see and walk down.
//
// The opening you come OUT of is the same doorway seen from the other end, so
// it is glued along the face you EMERGE from: the mirror image, `-plane_depth`.
//
// Using the in-plane at both ends is what a first version did, and it means the
// two blocks are glued back to back: the image then contains the destination's
// entire wall thickness as a second doorway behind the first. Reported from
// play in exactly those words -- "non deve mostrare il muro dell'altro mondo".
// With the two planes the doorway is traversed ONCE.
//
// A pleasant consequence: the two offsets cancel in the map, so the virtual
// camera lands exactly where it did when the image sat at the node centre, and
// every hand-derived reference viewpoint in the gates keeps its old value.
inline Vec3 portal_plane_in(const PortalSurface &s)
{
    return s.center + s.normal * portal_tuning().plane_depth();
}

inline Vec3 portal_plane_out(const PortalSurface &s)
{
    return s.center - s.normal * portal_tuning().plane_depth();
}

// Where the destination is LOOKED FROM, and where a body's copy is cut against:
// the emerging face, pushed `view` blocks further out.
//
// Separate from portal_plane_out on purpose. That plane is the doorway's
// geometry -- the aperture the lateral planes pass through, the far end of the
// tunnel -- and it belongs to the block. This one is the viewpoint, and moving
// it is a decision about what the opening SHOWS, not about where the opening
// is. With view = 0 the two coincide and the image is a true portal view.
//
// The body's copy follows the viewpoint rather than the plane, because the copy
// is placed by this same map: cut it against the plane while the map has moved
// it a block, and the half that is kept is the wrong half.
inline Vec3 portal_view_origin(const PortalSurface &s)
{
    return portal_plane_out(s) + s.normal * portal_tuning().view_depth();
}

// Where the image puts the eye, as a depth at the DESTINATION, for an eye
// `in_depth` in front of the source's centre.
//
// The map glues the face the player leaves the source through to the face they
// emerge from at the destination, so the two block offsets cancel and the depth
// is simply negated; the viewpoint offset then adds to it. One line, in one
// place, because half the gates in this engine predict a seam from it and a
// prediction that disagrees with the map is worse than no prediction.
//
// The seam of a crossing is the distance from this to exit_depth(), which is
// zero exactly when exit = trigger + view.
inline float portal_shown_depth(float in_depth)
{
    return portal_tuning().view_depth() - in_depth;
}

// Where the destination's geometry starts being drawn: `clip` blocks behind
// the plane the map glues to.
//
// That plane is portal_view_origin -- the VIEWPOINT -- and not
// portal_plane_out, and once `view` is non-zero those are a block apart. With
// clip at its rule value, plane - tunnel_start, the depth of this at the
// destination works out to
//
//     view + tunnel_start - 0.5      (nodes from the node centre)
//
// because `plane` cancels: the doorway subtracted is the same doorway the
// image sits at the far end of. tests/portal_math_test.cpp checks the general
// form against the settings.
//
// config/portal.cfg's `clip` says what each direction costs and both were
// found by playing: too little and the destination's ground is cut away with
// its wall, leaving clear colour; too much and the wall is left standing in
// front of the opening, fighting the side faces for depth. dual_session's third section
// measures both from one viewpoint with two aims.
//
// With `view` at zero and clip at the doorway this is the far side of the
// destination's own block, which is where it has always been.
inline Vec3 portal_clip_point(const PortalSurface &s)
{
    return portal_view_origin(s) - s.normal * portal_tuning().clip_depth();
}

} // namespace cnp
