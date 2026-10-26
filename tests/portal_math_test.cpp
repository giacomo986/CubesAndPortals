// Headless proof that the portal mathematics is right, before any GPU is
// involved. DESIGN.md 5.7 wants the fork's throwaway "write a mod and grep the
// log" habit promoted to a real framework; this is where that starts.
//
// The interesting assertion is the oblique near plane. Lengyel's construction
// claims that after the substitution, the clip plane becomes the near plane --
// so a point lying exactly on it must land at ndc.z == -1. That is checkable
// with arithmetic alone, and it is the single thing most likely to be subtly
// wrong when porting a left-handed derivation to a right-handed one.

#include "core/math/mat4.h"
#include "core/portal/portal_math.h"

#include "core/portal/portal_tuning.h"

#include <string>

#include <cmath>
#include <cstdio>

using namespace cnp;

namespace {

int g_failures = 0;

void check(bool cond, const char *what)
{
    if (cond) {
        std::printf("  ok    %s\n", what);
    } else {
        std::printf("  FAIL  %s\n", what);
        ++g_failures;
    }
}

void check_near(float got, float want, float tol, const char *what)
{
    const bool ok = std::fabs(got - want) <= tol;
    if (ok) std::printf("  ok    %s (%.5f)\n", what, got);
    else {
        std::printf("  FAIL  %s: got %.6f want %.6f\n", what, got, want);
        ++g_failures;
    }
}

PortalSurface make_dst()
{
    PortalSurface d;
    d.center = Vec3{0.0f, 0.0f, 0.0f};
    d.normal = Vec3{0.0f, 0.0f, 1.0f};
    d.up     = Vec3{0.0f, 1.0f, 0.0f};
    d.half_w = 0.5f;
    d.half_h = 1.0f;
    return d;
}

// Depth of a world point in normalised device coordinates.
float ndc_z(const Mat4 &proj, const Mat4 &view, Vec3 p)
{
    const Vec4 eye  = mat4_transform_vec4(view, Vec4{p.x, p.y, p.z, 1.0f});
    const Vec4 clip = mat4_transform_vec4(proj, eye);
    return clip.z / clip.w;
}

void test_matrix_convention()
{
    std::printf("projection convention\n");
    const Mat4 p = mat4_perspective(1.2217f, 16.0f / 9.0f, 0.05f, 500.0f);

    // Right-handed with depth in [-1, 1]: the fourth row is (0, 0, -1, 0).
    // Lengyel's substitution below assumes exactly this.
    check_near(p.at(3, 2), -1.0f, 1e-6f, "m11 == -1 (right handed)");
    check_near(p.at(3, 3), 0.0f, 1e-6f, "m15 == 0");

    // A point on the near plane maps to -1, one on the far plane to +1.
    const Mat4 identity_view = mat4_identity();
    check_near(ndc_z(p, identity_view, Vec3{0.0f, 0.0f, -0.05f}), -1.0f, 1e-4f,
               "near plane maps to ndc.z == -1");
    check_near(ndc_z(p, identity_view, Vec3{0.0f, 0.0f, -500.0f}), 1.0f, 1e-3f,
               "far plane maps to ndc.z == +1");
}

void test_portal_transform()
{
    std::printf("portal transform\n");

    PortalSurface src;
    src.center = Vec3{10.0f, 0.0f, 0.0f};
    src.normal = Vec3{0.0f, 0.0f, 1.0f};
    src.up     = Vec3{0.0f, 1.0f, 0.0f};

    const PortalSurface dst = make_dst();

    // Standing two units in front of the source's PLANE must place the virtual
    // camera two units BEHIND the destination's: that is what makes it look out
    // through the destination rather than into it.
    //
    // In front of the plane, not of the node centre. The two were the same
    // point while the image sat in the middle of the block; config/portal.cfg
    // moves it, and a test written in node coordinates would then be measuring
    // the offset instead of the map.
    const Vec3 in_front = portal_plane_in(src) + src.normal * 2.0f;
    const Mat4 t   = portal_transform(src, dst);
    const Vec3 out = mat4_transform_point(t, in_front);

    // Behind the destination's emerging face by two, LESS the viewpoint offset:
    // config/portal.cfg's `view` pushes the destination end of the map that far
    // further out, which is the whole point of it. Written as the setting and
    // not as a number, so a different config still measures the map.
    const float out_depth = dot(out - portal_plane_out(dst), dst.normal);
    check_near(out_depth, -2.0f + portal_tuning().view_depth(), 1e-4f,
               "two in front of src -> two behind dst, less the viewpoint offset");
    check_near(out.x, 0.0f, 1e-4f, "lateral offset preserved through the pair");

    // Looking into the source means looking out of the destination.
    const VirtualCamera v = portal_virtual_camera(src, dst, in_front,
                                                  Vec3{0.0f, 0.0f, -1.0f},
                                                  Vec3{0.0f, 1.0f, 0.0f});
    check_near(v.forward.z, 1.0f, 1e-4f, "forward flips through the pair");
    check_near(v.up.y, 1.0f, 1e-4f, "up survives the 180 degree turn");
}

void test_oblique_near()
{
    std::printf("oblique near plane (Lengyel)\n");

    const PortalSurface dst = make_dst();
    const Vec3 vcam{0.0f, 0.0f, -3.0f};          // behind the destination
    const Mat4 view = mat4_look_to(vcam, Vec3{0.0f, 0.0f, 1.0f}, Vec3{0.0f, 1.0f, 0.0f});
    const Mat4 base = mat4_perspective(1.2217f, 16.0f / 9.0f, 0.05f, 500.0f);

    Mat4 oblique;
    const bool applied = portal_oblique_near(base, view, dst, vcam, &oblique);
    check(applied, "applies when the camera is behind the destination");
    if (!applied) return;

    // The clip plane is the far end of the DOORWAY, not the face the player
    // emerges from: see portal_clip_point. It was the node's outer FACE for a
    // while, and half a node of the destination's image went missing behind it
    // -- the floor of its own opening, and the half of a body that portal_clip
    // puts at depth zero and up. Clipping at the emerging face made the same
    // mistake a whole block bigger, and the continuity gate read it at 1.5
    // blocks: 1.15% of the aperture differing, 0.00% once it moved back.
    const float kPlaneZ = portal_clip_point(dst).z;

    // THE assertion: a point on the clip plane is now exactly on the near
    // plane. Off-axis too, not just down the view axis -- an error in the
    // sgn() terms of Q shows up here and nowhere else.
    check_near(ndc_z(oblique, view, Vec3{0.0f, 0.0f, kPlaneZ}), -1.0f, 1e-3f,
               "point on the clip plane lands on the near plane");
    check_near(ndc_z(oblique, view, Vec3{0.9f, 0.6f, kPlaneZ}), -1.0f, 1e-3f,
               "off-axis point on the clip plane too");

    // Geometry beyond the plane stays visible; geometry between the camera and
    // the plane is now behind the near plane and gets clipped away. That pair
    // is exit criterion 1 stated arithmetically.
    check(ndc_z(oblique, view, Vec3{0.0f, 0.0f, 2.0f}) > -1.0f,
          "geometry past the portal remains in front of the near plane");
    check(ndc_z(oblique, view, Vec3{0.0f, 0.0f, -1.0f}) < -1.0f,
          "geometry between camera and portal falls behind the near plane");
}

void test_oblique_guards()
{
    std::printf("oblique guards\n");

    const PortalSurface dst = make_dst();
    const Mat4 base = mat4_perspective(1.2217f, 16.0f / 9.0f, 0.05f, 500.0f);
    Mat4 out;

    // Camera on the approach side: this is the player standing inside the
    // frame. Applying Lengyel here puts the near plane behind the camera and
    // the portal renders solid black, so refusing is the correct answer.
    const Vec3 wrong_side{0.0f, 0.0f, 2.0f};
    const Mat4 view_wrong =
        mat4_look_to(wrong_side, Vec3{0.0f, 0.0f, -1.0f}, Vec3{0.0f, 1.0f, 0.0f});
    check(!portal_oblique_near(base, view_wrong, dst, wrong_side, &out),
          "declines when the camera is on the approach side");

    // Grazing: looking along the portal surface rather than through it.
    const Vec3 grazing{0.0f, 0.0f, -3.0f};
    const Mat4 view_grazing =
        mat4_look_to(grazing, Vec3{1.0f, 0.0f, 0.0f}, Vec3{0.0f, 1.0f, 0.0f});
    check(!portal_oblique_near(base, view_grazing, dst, grazing, &out),
          "declines at a grazing angle");
}

void test_clip_planes()
{
    std::printf("lateral clip planes\n");

    const PortalSurface dst = make_dst();
    const Vec3 vcam{0.0f, 0.0f, -3.0f};

    // The four lateral planes only exist when the doorway's side faces are
    // WALL. As a window they come off on purpose -- they hide exactly what a
    // window is for -- so this section asks for the shape that has them, and
    // the window case gets its own check at the end.
    PortalTuning wall = portal_tuning();
    wall.sides        = 0.0f;
    std::string terr;
    check(portal_tuning_set(wall, terr), "the wall-sided shape is a legal one");

    PortalClipPlanes cp;
    const bool applied = portal_clip_planes(dst, vcam, &cp);
    check(applied, "installs planes for a camera outside the opening");
    if (!applied) return;
    check(cp.count == 5, "five planes: four lateral plus the face");

    // A point straight ahead through the opening must be kept by all five.
    auto kept_by_all = [&](Vec3 p) {
        for (int i = 0; i < cp.count; ++i)
            if (plane_distance(cp.planes[i], p) < 0.0f) return false;
        return true;
    };

    check(kept_by_all(Vec3{0.0f, 0.0f, 5.0f}), "a point seen through the opening is kept");

    // The destination's own frame is what leaks in without these planes. The
    // frame lives in the same node layer as the portal -- `center` is the node
    // CENTRE, so that layer is z in [-0.5, 0.5] -- and sits beside the hole.
    // Taken at the plane's depth rather than the node's middle, because the
    // face plane is anchored there and a point in front of it is kept for a
    // reason that has nothing to do with the lateral planes under test.
    // Two different depths, and mixing them up is the whole reason this section
    // needed rewriting. `az` is the APERTURE the four lateral planes are drawn
    // through -- the face the player emerges from. `cz` is the DEPTH plane, a
    // doorway further back. They were the same point while the image sat at the
    // node centre.
    const float az = portal_plane_out(dst).z;
    const float cz = portal_clip_point(dst).z;

    // And where that depth plane is, from the settings rather than from itself.
    // It sits `clip` blocks behind the plane the map glues to, which is the
    // VIEWPOINT -- portal_view_origin -- so
    //
    //     depth = (plane - 0.5) + view - clip
    //
    // At clip's rule value, plane - tunnel_start, that is view + tunnel_start -
    // 0.5 and `plane` cancels: the doorway subtracted is the same doorway the
    // image sits at the end of. Both checked, because the second is the
    // property and the first is what the code may do to it.
    //
    // Short of the rule value the destination's ground is cut away with its
    // wall; past it, the wall is left standing in front of the opening. Both
    // measured in dual_session's third section.
    {
        const PortalTuning &t = portal_tuning();
        check_near(cz - dst.center.z, (t.plane - 0.5f + t.view - t.clip) * kNodeSize, 1e-4f,
                   "the depth plane is `clip` blocks behind the viewpoint");
        check_near(t.clip, t.plane - t.tunnel_start, 1e-4f,
                   "and config/portal.cfg's clip is the doorway, so it is one doorway behind it");
    }
    check(!kept_by_all(Vec3{3.0f, 0.0f, az}), "frame beside the opening is clipped");

    // A lateral plane doing its own work, with the face plane not deciding it:
    // this point is past the plane, but the line from the apex to it misses the
    // opening, so it cannot be seen through the hole.
    check(!kept_by_all(Vec3{10.0f, 0.0f, az + 0.05f}),
          "point past the face but off the line of sight is clipped");

    // The lateral frustum is deliberately WIDER than what the virtual camera
    // really sees: the apex is pulled to the portal rather than left at the
    // camera, because a truthful narrow frustum would clip away most of the
    // destination interior. So a point just past the face and well off to the
    // side is still kept -- from the apex it genuinely is visible through the
    // opening. Clipping it is the job of the oblique near plane, not of these.
    check(kept_by_all(Vec3{3.0f, 0.0f, az + 0.6f}),
          "wide-by-design: point past the face on a valid sight line is kept");

    // Anything behind the face plane goes.
    check(!kept_by_all(Vec3{0.0f, 0.0f, cz - 0.5f}), "geometry behind the face is clipped");

    // Standing fully inside the opening switches the whole thing off.
    PortalClipPlanes unused;
    check(!portal_clip_planes(dst, Vec3{0.0f, 0.0f, az + 0.2f}, &unused),
          "declines while the player is inside the opening");

    // And with the side faces set to be a window, only the depth plane stays.
    // The four lateral ones are what hides whatever stands beside the
    // destination's opening, which is the one thing a window has to show.
    {
        PortalTuning win = portal_tuning();
        win.sides        = 1.0f;
        std::string werr;
        check(portal_tuning_set(win, werr), "the window-sided shape is a legal one");

        PortalClipPlanes wcp;
        const bool wapplied = portal_clip_planes(dst, vcam, &wcp);
        check(wapplied && wcp.count == 1,
              "a window keeps the depth plane and drops the four lateral ones");

        // The point the lateral planes used to remove is now kept, which is the
        // property in one line: what is beside the destination's opening
        // reaches the target.
        bool kept = true;
        for (int i = 0; i < wcp.count; ++i)
            if (plane_distance(wcp.planes[i], Vec3{3.0f, 0.0f, az + 0.6f}) < 0.0f) kept = false;
        check(kept, "and the frame beside the opening now reaches the image");
    }

    // But being at frame depth and laterally OUTSIDE the hole still needs them:
    // loosening this condition to depth alone is a bug that lets the frame leak.
    check(portal_clip_planes(dst, Vec3{2.0f, 0.0f, 0.2f}, &unused),
          "still clips at frame depth but outside the opening");
}

} // namespace

int main()
{
    test_matrix_convention();
    test_portal_transform();
    test_oblique_near();
    test_oblique_guards();
    test_clip_planes();

    std::printf("\n%s\n", g_failures == 0 ? "all portal math checks passed"
                                          : "PORTAL MATH CHECKS FAILED");
    return g_failures == 0 ? 0 : 1;
}
