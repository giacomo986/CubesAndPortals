#pragma once

#include "client/gfx/gfx.h"
#include "client/render/world_render.h"
#include "core/math/mat4.h"
#include "core/portal/portal_math.h"
#include "core/world/daylight.h"

#include <vector>

namespace cnp {

// The inside of a hole through a wall, and what the deepest opening in a
// recursion is filled with. Exposed because a gate that checks the fill has to
// ask for the colour rather than carry its own copy: two constants for one
// colour is exactly the pair that drifts, and the gate would go green against
// the value it remembered.
inline constexpr float kPortalWallColour[4] = {0.50f, 0.41f, 0.30f, 1.0f};

// The sky, and the one place it is written down.
//
// DESIGN.md 8 asks for "sky through the portal", and in an engine with one flat
// sky that is a duplication problem rather than a rendering one: the main pass
// and every render target clear to it, and until now that was the same three
// literals typed in three files. Change one and the sky through a portal stops
// matching the sky around it, on a seam a player is looking straight at.
//
// Fog is NOT here and is not implemented anywhere: there is no distance fog in
// this engine to carry through a portal. Said plainly rather than left to be
// discovered behind a constant that looks like it covers it.
// The same three literals again would be the same duplication one paragraph up,
// one layer out: the sky moves with the clock now, and core/world/daylight.h is
// where the three skies it moves between are written down. This is the one it
// happens to be at noon, kept because a world that has not been told the time
// yet has to clear to SOMETHING and noon is what it starts at.
inline constexpr float kSkyColour[4] = {kDaySky[0], kDaySky[1], kDaySky[2], 1.0f};

struct PortalLink {
    PortalSurface src;      // the opening you look into
    PortalSurface dst;      // the opening the image comes from
    gfx_rt        rt{0};
};

// Renders a portal in two passes, in the order DESIGN.md 2.2 fixes.
//
// The order is not a preference. The render-target pass must run before the
// main pass so the driver returns to framebuffer 0 and the main pass binds a
// clean target. And the opening has to be drawn INSIDE the opaque pass, not
// after it: transparent geometry writes depth, so an opening drawn afterwards
// gets z-culled by any pane of glass standing in front of the portal.
class PortalRenderer {
public:
    bool init();
    void shutdown();

    // Pass 1: what the destination sees, into link.rt.
    //
    // `to_screen` draws the same pass to the current framebuffer instead, which
    // is the only way to see what is actually going into the target. When a
    // portal looks wrong, the first question is whether the target is wrong or
    // the opening is sampling it wrong, and guessing between the two wastes
    // more time than the flag costs.
    void render_target(WorldRenderer &world_render, const PortalLink &link,
                       Vec3 cam_pos, Vec3 cam_forward, Vec3 cam_up,
                       float fov_y, float aspect, float z_near, float z_far,
                       bool to_screen = false);

    // Pass 2: the opening itself, sampling link.rt at screen position.
    void draw_opening(const PortalLink &link, Vec3 cam_pos,
                      const Mat4 &projection, const Mat4 &view);

    // An opening that leads nowhere: across its doorway's mouth an animated
    // membrane in `rgb` -- blue or orange for a portal gun's end, green for a
    // fixed portal -- so a player sees a portal waiting for its other end.
    // `open` from 0 to 1 is the moment it is linked: the membrane opens from
    // the middle onto the image already drawn behind it.
    void draw_closed(const PortalSurface &s, const float rgb[3], Vec3 cam_pos,
                     const Mat4 &projection, const Mat4 &view, float open = 0.0f);

    // ------------------------------------------------------------- recursion
    //
    // A portal that shows another portal has to show something INSIDE that
    // second one, or the world has a hole in it exactly where the interesting
    // part is. Doing that needs a target per portal per depth, which is what
    // the pool is: link.rt stays depth zero, and this allocates the rest.
    //
    // `max_depth` is how many portals deep the image goes. Two is what
    // DESIGN.md 8 asks M4 to prove -- two portals facing each other, seen
    // through each other -- and each level costs one full-size target per
    // portal, so it is a number to raise deliberately rather than by default.
    //
    // Full size at every level on purpose: the opening shader samples its
    // target by SCREEN position, so a half-size target for the deep levels
    // would be sampled at the wrong place. Making them cheaper means changing
    // that shader, not the allocation.
    bool init_recursion(int link_count, int max_depth, int width, int height);
    void shutdown_recursion();
    int  max_depth() const { return m_max_depth; }
    int  pool_links() const { return m_link_count; }

    // Whether any of an opening, its tunnel included, is inside the frustum of
    // `view_projection`. Conservative, like the chunk culling: an opening out of
    // sight has nothing to show, and its target pass is a whole world drawn for
    // no pixel -- with more than a pair, most of the frame.
    static bool opening_in_view(const PortalSurface &s, const Mat4 &view_projection);

    // Renders every link's target, deepest first. Deepest first is the whole
    // trick: the openings a pass draws have to contain an image already, and a
    // render target cannot be bound while another one is.
    //
    // How the recursion ENDS: at the deepest level the openings are still
    // drawn, but flat -- the tunnel in wall colour and the image quad in the
    // clear colour. Leaving them out, which is what this did until the flat
    // path existed, turns the innermost portal into a hole through to whatever
    // stands behind the wall, and somebody counting levels in a corridor counts
    // that hole as one more. See draw_opening_terminal.
    void render_recursive(WorldRenderer &world_render, const PortalLink *links, int link_count,
                          Vec3 cam_pos, Vec3 cam_forward, Vec3 cam_up, float fov_y, float aspect,
                          float z_near, float z_far);

    // ------------------------------------------------------- the camera hint
    //
    // An override for where a link's virtual camera stands, and PORTAL_SYSTEM
    // 2.5's answer to a problem that has exactly one frame in it: on the frame
    // a player goes through, the arrival view has to be rendered BEFORE the
    // live camera has moved there. The fork sets the hint, teleports, clears
    // it.
    //
    // This engine does not have that frame -- the traversal is applied before
    // anything is drawn, and portal_cases measures the first frame after a five
    // node jump as already the settled frame, 0 pixels out of 2073600. The hint
    // is here for the other use the fork puts it to: driving a portal's image
    // from somewhere other than the player, which is what an arrival preview,
    // a camera block or a second session rendered into a portal all need.
    //
    // Non-consuming, like the fork's: it holds until cleared, rather than
    // applying to one frame. A hint that expired on its own would have to be
    // set again every frame by whoever wanted it, which is the same bug as a
    // per-frame teleport trigger.
    void set_cam_hint(int link_index, Vec3 pos)
    {
        if (link_index >= 0 && link_index < kMaxDiagnosticLinks) {
            m_cam_hint[link_index]     = pos;
            m_cam_hint_set[link_index] = true;
        }
    }
    void clear_cam_hint(int link_index)
    {
        if (link_index >= 0 && link_index < kMaxDiagnosticLinks)
            m_cam_hint_set[link_index] = false;
    }
    bool cam_hint(int link_index, Vec3 *out) const
    {
        if (link_index < 0 || link_index >= kMaxDiagnosticLinks) return false;
        if (!m_cam_hint_set[link_index]) return false;
        if (out) *out = m_cam_hint[link_index];
        return true;
    }

    // The sky the NEXT pass clears to.
    //
    // A constant would be the same singleton DESIGN.md 4.1 spends a section on:
    // once a portal can look into another world, the sky inside the opening is
    // that world's, and clearing to a value compiled into the renderer makes the
    // seam between the two skies invisible for the wrong reason -- they are the
    // same colour because nobody can choose.
    //
    // Set per call rather than per level: with two sessions every level of a
    // portal's image belongs to the same other world, so one value covers them.
    // Three worlds deep would need it per level, and that is stated rather than
    // pretended.
    void set_sky(const float rgba[4])
    {
        for (int i = 0; i < 4; ++i) m_sky[i] = rgba[i];
    }

    // What else belongs in a portal's image besides the world.
    //
    // The world renderer draws chunks. A crate halfway through a portal is not
    // a chunk, and without this it would be cut correctly in the world and
    // simply absent from the view through the opening -- which is worse than
    // not cutting it at all, because the half you can see would have nothing on
    // the other side of it.
    using ExtraDraw = void (*)(void *ud, const Mat4 &projection, const Mat4 &view);
    void set_extra_draw(ExtraDraw fn, void *ud)
    {
        m_extra    = fn;
        m_extra_ud = ud;
    }

    // How many target passes the last render_recursive did. One per portal per
    // depth if nothing was skipped, and the number that says whether the cost
    // is what it looks like.
    int last_pass_count() const { return m_pass_count; }

    // Diagnostics for the M0 exit criteria: which techniques actually engaged
    // on the last frame. Criterion 2 is precisely the case where the oblique
    // near plane is expected to switch itself OFF, so "did it apply" has to be
    // observable rather than assumed.
    // Debug switches for bisecting a portal image that looks wrong: turn one of
    // the two mechanisms off and see which one was carving up the picture.
    // DEBUG: paints the link's render target across the whole screen, sampled
    // exactly the way the opening samples it. If this is correct and the
    // opening is not, the fault is in the opening, not in the target.
    void blit_rt(const PortalLink &link);

    void set_debug(bool use_oblique, bool use_clip)
    {
        m_use_oblique = use_oblique;
        m_use_clip    = use_clip;
    }

    // Per link. Index 0 is the first link handed to render_recursive, which is
    // the one M0's exit criteria are written about.
    static constexpr int kMaxDiagnosticLinks = 4;
    bool oblique_applied(int index = 0) const
    {
        return (index >= 0 && index < kMaxDiagnosticLinks) && m_oblique_applied[index];
    }
    bool clip_applied(int index = 0) const
    {
        return (index >= 0 && index < kMaxDiagnosticLinks) && m_clip_applied[index];
    }

private:
    // One target per link per depth from 1 upwards; depth zero is link.rt.
    gfx_rt rt_at(const PortalLink *links, int index, int depth) const;

    void render_into(WorldRenderer &world_render, const PortalLink *links, int link_count,
                     int index, Vec3 pos, Vec3 forward, Vec3 up, float fov_y, float aspect,
                     float z_near, float z_far, int depth);

    void draw_opening_from(const PortalLink &link, gfx_rt source, Vec3 cam_pos,
                           const Mat4 &projection, const Mat4 &view);

    // The deepest level, with no target left to sample. See the definition.
    void draw_opening_terminal(const PortalLink &link, Vec3 cam_pos, const Mat4 &projection,
                               const Mat4 &view);

    gfx_shader m_shader{0};
    gfx_buffer m_vb{0};
    gfx_mesh   m_mesh{0};
    int        m_loc_tex    = -1;
    int        m_loc_screen = -1;
    int        m_loc_tunnel = -1;
    int        m_loc_tunnel_color = -1;
    int        m_loc_closed_color = -1;
    int        m_loc_centre = -1, m_loc_right = -1, m_loc_up = -1, m_loc_half = -1;
    int        m_loc_time   = -1;
    int        m_loc_open   = -1;
    WaterUniforms m_water;

    ExtraDraw m_extra    = nullptr;
    void     *m_extra_ud = nullptr;

    std::vector<gfx_rt> m_pool;         // link_count * max_depth, depths 1..max
    int                 m_link_count = 0;
    int                 m_max_depth  = 0;
    int                 m_pass_count = 0;

    float m_sky[4] = {kSkyColour[0], kSkyColour[1], kSkyColour[2], kSkyColour[3]};

    bool m_use_oblique = true;
    bool m_use_clip    = true;
    Vec3 m_cam_hint[kMaxDiagnosticLinks]     = {};
    bool m_cam_hint_set[kMaxDiagnosticLinks]  = {};
    bool m_oblique_applied[kMaxDiagnosticLinks] = {};
    bool m_clip_applied[kMaxDiagnosticLinks]    = {};
};

} // namespace cnp
