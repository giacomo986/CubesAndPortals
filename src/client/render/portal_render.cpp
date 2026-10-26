#include "client/render/portal_render.h"

#include "client/render/shaders.h"

#include "core/math/frustum.h"
#include "core/portal/portal_tuning.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>

namespace cnp {
namespace {

// Five quads: the four sides of the hole from the viewer's face of the node as
// far as the portal plane, plus the quad that carries the image. 30 vertices,
// rewritten every frame because the geometry depends on which side of the
// portal the camera is on.
//
// The tunnel stops at the MIDDLE of the node, and the image quad is there
// rather than at the far face. That is where the portal is: it is where
// portal_clip cuts a solid, where portal_traverse fires, and now where the
// render target's near plane is anchored. In front of it the hole is wall
// material; behind it there is no wall to draw, because that half of the node
// is already the other place -- and the other place's own hole, with its own
// sides and floor, is in the render target, so the image quad shows it in
// perspective for free.
//
// Building geometry for the far half instead is the thing that does not work,
// and it was tried: quads spanning the far half do sample the target correctly,
// but they also WRITE DEPTH across half a node of the other world, where they
// hide whatever is legitimately there. The measurement that caught it was the
// cut gate, which lost 1039 pixels of a crate's far half behind them.
constexpr int kPortalVertices = 5 * 6;

// The inside of the hole, in front of the portal plane. Matches core:frame
// closely enough that the tunnel reads as the wall it is cut through, which is
// what it is. Lives in the header so the recursion gate can ask for it rather
// than carry a second copy of the same colour.
constexpr const float *kTunnelColour = kPortalWallColour;

struct PortalVertex {
    float x, y, z;
};

// Pulls the walls a hair inside the opening so they never z-fight with the
// frame blocks around it.
constexpr float kEdgeInset = 0.005f * kNodeSize;

// And pulls the far END of the side faces a hair back down the doorway, so they
// stop short of the plane instead of landing exactly on it.
//
// With the image at the middle of the block that plane was in mid-air and this
// was not needed. With `plane = 1.00` it is the block's back face, which is
// also the front face of whatever node stands behind the portal -- two coplanar
// surfaces, one node boundary, and the flicker that comes with them. Reported
// from play against the exit wall.
constexpr float kExitSeamInset = 0.001f * kNodeSize;

void push_quad(PortalVertex *out, int &n, Vec3 a, Vec3 b, Vec3 c, Vec3 d)
{
    auto put = [&](Vec3 v) { out[n++] = PortalVertex{v.x, v.y, v.z}; };
    put(a); put(b); put(c);
    put(a); put(c); put(d);
}

// The four sides of the hole between two rectangles of the same size, one at
// `a` and one at `b`.
void push_ring(PortalVertex *out, int &n, Vec3 r, Vec3 u, Vec3 a, Vec3 b, float hw, float hh)
{
    push_quad(out, n, a - r * hw - u * hh, a - r * hw + u * hh,
                      b - r * hw + u * hh, b - r * hw - u * hh);   // left
    push_quad(out, n, a + r * hw - u * hh, a + r * hw + u * hh,
                      b + r * hw + u * hh, b + r * hw - u * hh);   // right
    push_quad(out, n, a - r * hw - u * hh, a + r * hw - u * hh,
                      b + r * hw - u * hh, b - r * hw - u * hh);   // bottom
    push_quad(out, n, a - r * hw + u * hh, a + r * hw + u * hh,
                      b + r * hw + u * hh, b - r * hw + u * hh);   // top
}

// How far in front of the centre, toward `side_sign`, the doorway starts: the
// tunnel depth config/portal.cfg names, but never out past the face of the
// node the opening stands in. A portal gun's opening sits 0.1 off its node's
// centre, toward the room (chambers_game, lab), and a tunnel measured from the
// centre stood 0.1 out of the wall, through the rim drawn on its face.
float entrance_depth(const PortalSurface &s, float side_sign)
{
    const float n[3] = {s.normal.x, s.normal.y, s.normal.z};
    const float c[3] = {s.center.x, s.center.y, s.center.z};
    int k = 0;
    for (int i = 1; i < 3; ++i)
        if (std::fabs(n[i]) > std::fabs(n[k])) k = i;
    const float a    = c[k] / kNodeSize;
    const float face = (n[k] * side_sign > 0.0f) ? std::ceil(a) - a : a - std::floor(a);
    const float tunnel = portal_tuning().tunnel_depth();
    // On a node boundary there is no node to stand in: the tuning as it was.
    return face > 1e-3f ? std::min(tunnel, face * kNodeSize) : tunnel;
}

// Builds the opening's geometry for a camera on `side_sign` of the surface.
// Back-face culling is off for these quads, so winding is not load-bearing.
//
// `painted` comes back as the number of leading vertices that are wall rather
// than portal image: the near ring, and only it.
int build_opening(const PortalSurface &s, float side_sign, PortalVertex *out, int &painted)
{
    const Vec3 r = s.right();
    const Vec3 u = s.up;
    const Vec3 n = s.normal;

    // The face of the node on the viewer's side, and the plane the image sits
    // on. Both come from config/portal.cfg: with the plane on the far face the
    // ring below spans the whole block and the opening reads as a doorway; with
    // it in the middle the ring covers half and the image is halfway in.
    //
    // The ring never crosses the plane, whatever the plane is, and that is not
    // a detail. Quads BEYOND it write depth across the other place and hide
    // what is legitimately there -- the cut gate caught exactly that, losing
    // 1039 pixels of a crate's far half behind them.
    const Vec3 wf  = s.center + n * (side_sign * entrance_depth(s, side_sign));
    const Vec3 mid = portal_plane_in(s);

    // Where the side faces STOP: a thousandth of a node short of the plane, off
    // the node boundary. See kExitSeamInset.
    const Vec3 wb = mid + n * (side_sign * kExitSeamInset);

    const float hw = s.half_w - kEdgeInset;
    const float hh = s.half_h - kEdgeInset;

    int count = 0;

    // In front of the portal plane: wall. A ray that reaches these quads
    // crossed the portal plane OUTSIDE the aperture, so it never went through,
    // and the render target holds nothing for it -- sampling here is what
    // filled the bottom of a floor-level opening with clear colour.
    push_ring(out, count, r, u, wf, wb, hw, hh);
    painted = count;

    // The image, on the portal plane and at the opening's full size, so it
    // meets the tunnel's inner edge rather than sitting half a node behind it.
    // Full half_w/half_h rather than the inset ones: the tunnel is inset to
    // stay off the frame, and leaving the image inset too would show a hairline
    // of whatever is behind the wall around it.
    //
    // A hair toward the viewer, not exactly on the plane. portal_clip cuts a
    // solid on this same plane, so the cut face of a body halfway through is
    // exactly coplanar with this quad, and which of the two wins the depth test
    // is then down to draw order. Without the offset a crate straddling a
    // portal showed its cut face through the opening from the far side, where
    // the portal surface should be covering it.
    const Vec3 ip = mid + n * (side_sign * kEdgeInset);
    push_quad(out, count, ip - r * s.half_w - u * s.half_h,
                          ip + r * s.half_w - u * s.half_h,
                          ip + r * s.half_w + u * s.half_h,
                          ip - r * s.half_w + u * s.half_h);

    return count;
}

} // namespace

bool PortalRenderer::init()
{
    m_shader = gfx_shader_create(shaders::kPortalVS, shaders::kPortalFS);
    if (!m_shader.id) {
        std::fprintf(stderr, "portal renderer: shader compilation failed\n");
        return false;
    }
    m_loc_tex    = gfx_uniform_location(m_shader, "uPortalTex");
    m_loc_screen = gfx_uniform_location(m_shader, "uScreenSize");
    m_loc_tunnel = gfx_uniform_location(m_shader, "uTunnel");
    m_loc_tunnel_color = gfx_uniform_location(m_shader, "uTunnelColor");
    m_loc_closed_color = gfx_uniform_location(m_shader, "uClosedColor");
    m_loc_centre       = gfx_uniform_location(m_shader, "uCentre");
    m_loc_right        = gfx_uniform_location(m_shader, "uRight");
    m_loc_up           = gfx_uniform_location(m_shader, "uUp");
    m_loc_half         = gfx_uniform_location(m_shader, "uHalf");
    m_loc_time         = gfx_uniform_location(m_shader, "uTime");
    m_loc_open         = gfx_uniform_location(m_shader, "uOpen");
    m_water.locate(m_shader);

    m_vb = gfx_buffer_create(nullptr, kPortalVertices * (int)sizeof(PortalVertex), true, false);

    gfx_vertexlayout layout{};
    layout.stride     = (int)sizeof(PortalVertex);
    layout.attr_count = 1;
    layout.attrs[0]   = gfx_attr{0, 3, GFX_ATTR_F32, false, 0};
    m_mesh = gfx_mesh_create(&layout, m_vb, gfx_buffer{0});

    return m_mesh.id != 0;
}

void PortalRenderer::shutdown()
{
    if (m_mesh.id)   gfx_mesh_destroy(m_mesh);
    if (m_vb.id)     gfx_buffer_destroy(m_vb);
    if (m_shader.id) gfx_shader_destroy(m_shader);
}

void PortalRenderer::render_target(WorldRenderer &world_render, const PortalLink &link,
                                   Vec3 cam_pos, Vec3 cam_forward, Vec3 cam_up,
                                   float fov_y, float aspect, float z_near, float z_far,
                                   bool to_screen)
{
    if (link.rt.id == 0) return;

    const VirtualCamera vcam =
        portal_virtual_camera(link.src, link.dst, cam_pos, cam_forward, cam_up);

    // Built from the transformed direction, never from a look-at target: see
    // mat4_look_to on why a subtraction here loses the cross-world case.
    const Mat4 view = mat4_look_to(vcam.pos, vcam.forward, vcam.up);

    Mat4 projection = mat4_perspective(fov_y, aspect, z_near, z_far);
    Mat4 oblique;
    // This path renders one link and nothing else, so its diagnostics are
    // slot zero's.
    m_oblique_applied[0] = m_use_oblique &&
                           portal_oblique_near(projection, view, link.dst, vcam.pos, &oblique);
    if (m_oblique_applied[0]) projection = oblique;

    PortalClipPlanes clip;
    m_clip_applied[0] = m_use_clip && portal_clip_planes(link.dst, vcam.pos, &clip);

    // The sun's shadows around what this camera sees, not around the eye that
    // looks into the opening: the far end of a link can be anywhere.
    world_render.draw_shadows(vcam.pos, vcam.forward);

    if (!to_screen) gfx_rt_begin(link.rt);
    gfx_clear(m_sky[0], m_sky[1], m_sky[2], m_sky[3], true);

    gfx_state state = gfx_state_default();
    gfx_state_set(&state);

    if (m_clip_applied[0])
        gfx_clip_planes_set(&clip.planes[0].x, clip.count);
    else
        gfx_clip_planes_set(nullptr, 0);

    world_render.set_water(true, vcam.pos, 1);
    world_render.draw(projection, view);
    world_render.draw_translucent(projection, view);
    world_render.set_water(false);

    // Clearing before leaving matters: a clip distance left enabled would carve
    // up the main pass against a plane it never asked for.
    gfx_clip_planes_set(nullptr, 0);
    if (!to_screen) gfx_rt_end();
}

void PortalRenderer::blit_rt(const PortalLink &link)
{
    if (link.rt.id == 0 || m_mesh.id == 0) return;

    // Vertices straight in clip space, with identity matrices, so the quad
    // covers the screen and every fragment samples the target at its own
    // screen position -- the same lookup draw_opening does.
    PortalVertex verts[6] = {
        {-1.0f, -1.0f, 0.0f}, {1.0f, -1.0f, 0.0f}, {1.0f, 1.0f, 0.0f},
        {-1.0f, -1.0f, 0.0f}, {1.0f,  1.0f, 0.0f}, {-1.0f, 1.0f, 0.0f},
    };
    gfx_buffer_update(m_vb, verts, (int)sizeof(verts), 0);

    const Mat4 id = mat4_identity();
    gfx_shader_bind(m_shader);
    gfx_projection_set(id.m);
    gfx_view_set(id.m);
    gfx_model_set(id.m);
    m_water.off();   // a picture laid on the screen, not a place in the world

    const float screen[2] = {(float)gfx_screen_width(), (float)gfx_screen_height()};
    gfx_texture_bind(gfx_rt_texture(link.rt), 0);
    gfx_uniform_set_int(m_loc_tex, 0);
    gfx_uniform_set_v2(m_loc_screen, screen);

    gfx_state state = gfx_state_default();
    state.cull  = GFX_CULL_NONE;
    state.blend = GFX_BLEND_NONE;
    state.depth_test = false;
    gfx_state_set(&state);

    gfx_draw(m_mesh, 6, 0);
}

void PortalRenderer::draw_opening(const PortalLink &link, Vec3 cam_pos,
                                  const Mat4 &projection, const Mat4 &view)
{
    draw_opening_from(link, link.rt, cam_pos, projection, view);
}

// The same thing with the source named. Which target an opening samples is
// the only difference between drawing a portal in the world and drawing one
// inside another portal, so it is a parameter rather than a second function.
void PortalRenderer::draw_opening_from(const PortalLink &link, gfx_rt source, Vec3 cam_pos,
                                       const Mat4 &projection, const Mat4 &view)
{
    if (source.id == 0 || m_mesh.id == 0) return;

    const float side = dot(cam_pos - link.src.center, link.src.normal);

    // A one-way portal is not there from behind: no image, no doorway sides,
    // nothing drawn at all. What is left is the hole in the wall and whatever
    // stands beyond it. See PortalTuning::one_way for why the picture agreeing
    // with the trigger is the point -- the body could never cross from this
    // side, and the opening was showing the destination anyway.
    if (side < 0.0f && portal_tuning().is_one_way()) return;

    const float side_sign = (side >= 0.0f) ? 1.0f : -1.0f;

    PortalVertex verts[kPortalVertices];
    int          painted = 0;
    const int    count   = build_opening(link.src, side_sign, verts, painted);
    gfx_buffer_update(m_vb, verts, count * (int)sizeof(PortalVertex), 0);

    const Mat4 model = mat4_identity();

    gfx_shader_bind(m_shader);
    gfx_projection_set(projection.m);
    gfx_view_set(view.m);
    gfx_model_set(model.m);
    m_water.set(projection, view);

    const float screen[2] = {(float)gfx_screen_width(), (float)gfx_screen_height()};
    gfx_texture_bind(gfx_rt_texture(source), 0);
    gfx_uniform_set_int(m_loc_tex, 0);
    gfx_uniform_set_v2(m_loc_screen, screen);

    // Culling off, depth writes on: the opening has to occlude what is behind
    // it, and both of its faces are legitimately visible depending on which
    // side the player stands.
    gfx_state state = gfx_state_default();
    state.cull  = GFX_CULL_NONE;
    state.blend = GFX_BLEND_NONE;
    gfx_state_set(&state);

    // Two draws, because the buffer holds two different things: the sides of
    // the hole down to the portal plane, which are wall, then the quad on that
    // plane, which is the other place. See build_opening for where the split
    // falls and kPortalFS for what each half does with a fragment.
    const int tunnel_verts = painted;
    if (tunnel_verts > 0) {
        // Wall or window, from config/portal.cfg. As a window they sample the
        // target exactly as the far quad does -- same shader path, same
        // screen-space lookup -- and what they show is whatever stands beside
        // the destination's opening. The clip planes were widened to the
        // doorway's mouth so that there is something there to show.
        const bool window = portal_tuning().sides_are_window();
        gfx_uniform_set_int(m_loc_tunnel, window ? 0 : 1);
        if (!window) gfx_uniform_set_v4(m_loc_tunnel_color, kTunnelColour, 1);
        gfx_draw(m_mesh, tunnel_verts, 0);
    }
    if (count - tunnel_verts > 0) {
        gfx_uniform_set_int(m_loc_tunnel, 0);
        gfx_draw(m_mesh, count - tunnel_verts, tunnel_verts);
    }

    state.cull = GFX_CULL_BACK;
    gfx_state_set(&state);
}

// ---------------------------------------------------------------- recursion

bool PortalRenderer::init_recursion(int link_count, int max_depth, int width, int height)
{
    shutdown_recursion();
    if (link_count <= 0 || max_depth <= 0) return true;   // depth zero needs no pool

    m_link_count = link_count;
    m_max_depth  = max_depth;
    m_pool.assign((size_t)(link_count * max_depth), gfx_rt{0});

    for (auto &rt : m_pool) {
        rt = gfx_rt_create(width, height, true);
        if (rt.id == 0) {
            // Partial success is worse than none: half a pool means some
            // portals recurse and some show a hole, which looks like a bug in
            // the transform rather than like running out of memory.
            shutdown_recursion();
            return false;
        }
    }
    return true;
}

void PortalRenderer::shutdown_recursion()
{
    for (auto &rt : m_pool)
        if (rt.id) gfx_rt_destroy(rt);
    m_pool.clear();
    m_link_count = 0;
    m_max_depth  = 0;
}

gfx_rt PortalRenderer::rt_at(const PortalLink *links, int index, int depth) const
{
    if (depth == 0) return links[index].rt;
    if (depth > m_max_depth || index >= m_link_count) return gfx_rt{0};
    return m_pool[(size_t)((depth - 1) * m_link_count + index)];
}

bool PortalRenderer::opening_in_view(const PortalSurface &s, const Mat4 &view_projection)
{
    const Vec3 r = s.right(), u = s.up, n = s.normal;
    const Vec3 half{std::fabs(r.x) * s.half_w + std::fabs(u.x) * s.half_h + std::fabs(n.x),
                    std::fabs(r.y) * s.half_w + std::fabs(u.y) * s.half_h + std::fabs(n.y),
                    std::fabs(r.z) * s.half_w + std::fabs(u.z) * s.half_h + std::fabs(n.z)};
    return frustum_from_view_projection(view_projection)
        .intersects_aabb(s.center - half, s.center + half);
}

// The pixels of a target an opening can sample: its box (the hole's sides
// included, as opening_in_view) projected with the camera that looks at it,
// two pixels wider. False when a corner is at or behind the eye, where the
// box's projection says nothing and the whole target is drawn.
static bool opening_rect(const PortalSurface &s, const Mat4 &view_projection, int w, int h,
                         int &x0, int &y0, int &x1, int &y1)
{
    const Vec3 r = s.right(), u = s.up, n = s.normal;
    const Vec3 half{std::fabs(r.x) * s.half_w + std::fabs(u.x) * s.half_h + std::fabs(n.x),
                    std::fabs(r.y) * s.half_w + std::fabs(u.y) * s.half_h + std::fabs(n.y),
                    std::fabs(r.z) * s.half_w + std::fabs(u.z) * s.half_h + std::fabs(n.z)};
    float lo[2] = {1.0f, 1.0f}, hi[2] = {-1.0f, -1.0f};
    for (int c = 0; c < 8; ++c) {
        const Vec3 p{s.center.x + ((c & 1) ? half.x : -half.x),
                     s.center.y + ((c & 2) ? half.y : -half.y),
                     s.center.z + ((c & 4) ? half.z : -half.z)};
        const Vec4 q = mat4_transform_vec4(view_projection, Vec4{p.x, p.y, p.z, 1.0f});
        if (q.w < 1e-3f) return false;
        const float ndc[2] = {q.x / q.w, q.y / q.w};
        for (int a = 0; a < 2; ++a) {
            lo[a] = std::min(lo[a], ndc[a]);
            hi[a] = std::max(hi[a], ndc[a]);
        }
    }
    x0 = std::max(0, (int)std::floor((lo[0] * 0.5f + 0.5f) * (float)w) - 2);
    y0 = std::max(0, (int)std::floor((lo[1] * 0.5f + 0.5f) * (float)h) - 2);
    x1 = std::min(w, (int)std::ceil((hi[0] * 0.5f + 0.5f) * (float)w) + 2);
    y1 = std::min(h, (int)std::ceil((hi[1] * 0.5f + 0.5f) * (float)h) + 2);
    return x1 > x0 && y1 > y0;
}

void PortalRenderer::render_into(WorldRenderer &world_render, const PortalLink *links,
                                 int link_count, int index, Vec3 pos, Vec3 forward, Vec3 up,
                                 float fov_y, float aspect, float z_near, float z_far, int depth)
{
    const PortalLink &link   = links[index];
    const gfx_rt      target = rt_at(links, index, depth);
    if (target.id == 0) return;

    // Nothing is drawn for a one-way portal seen from behind, so nothing needs
    // rendering into its target either -- and that is the whole world pass for
    // this link, which is the most expensive thing in the frame. The
    // diagnostics are cleared rather than left holding the last answer from
    // when the camera was still in front of it.
    //
    // Nor for one out of the camera's sight, nor, below the first level, one
    // the camera is behind: the pass above does not draw it (see the loop at
    // the end of this function). With a pair facing each other that is most
    // of what the recursion used to render for nobody.
    const Mat4 seen_from = mat4_multiply(mat4_perspective(fov_y, aspect, z_near, z_far),
                                         mat4_look_to(pos, forward, up));
    const bool behind    = dot(pos - link.src.center, link.src.normal) < 0.0f;
    if ((portal_tuning().is_one_way() && behind) || (depth > 0 && behind) ||
        !opening_in_view(link.src, seen_from)) {
        if (depth == 0 && index < kMaxDiagnosticLinks) {
            m_oblique_applied[index] = false;
            m_clip_applied[index]    = false;
        }
        return;
    }

    VirtualCamera vcam = portal_virtual_camera(link.src, link.dst, pos, forward, up);

    // The hint replaces where the virtual camera STANDS and not where it looks.
    // Overriding the direction too would let the image point somewhere the
    // opening cannot show: the quad samples the target by screen position, so
    // the target has to be rendered with the same orientation the opening is
    // seen with, or the picture inside the hole is of a different room seen
    // from a different angle. Only at depth zero, because the deeper levels are
    // derived from the level above and a hint applied again at every level
    // would put every one of them in the same place.
    Vec3 hint;
    if (depth == 0 && cam_hint(index, &hint)) vcam.pos = hint;

    // Built from the transformed direction, never from a look-at target: see
    // mat4_look_to on why a subtraction here loses the cross-world case.
    const Mat4 view = mat4_look_to(vcam.pos, vcam.forward, vcam.up);

    Mat4 projection = mat4_perspective(fov_y, aspect, z_near, z_far);
    Mat4 oblique;
    const bool applied_oblique =
        m_use_oblique && portal_oblique_near(projection, view, link.dst, vcam.pos, &oblique);
    if (applied_oblique) projection = oblique;

    // Only the pixels the opening covers: the projection cut down to them and
    // drawn into them alone, so a far opening a few pixels wide costs a few
    // pixels and the chunks in that narrow frustum, not a whole screen of the
    // other world (50-85 ms a frame in software GL for the Inferno's two
    // openings by the loop). The image lands on the same pixels it would have.
    const int tw = gfx_rt_width(target), th = gfx_rt_height(target);
    int       x0 = 0, y0 = 0, x1 = tw, y1 = th;
    const bool cropped = opening_rect(link.src, seen_from, tw, th, x0, y0, x1, y1) &&
                         (x1 - x0 < tw || y1 - y0 < th);
    if (cropped) {
        const float ax = 2.0f * (float)x0 / (float)tw - 1.0f, bx = 2.0f * (float)x1 / (float)tw - 1.0f;
        const float ay = 2.0f * (float)y0 / (float)th - 1.0f, by = 2.0f * (float)y1 / (float)th - 1.0f;
        Mat4 crop;
        crop.at(0, 0) = 2.0f / (bx - ax);
        crop.at(0, 3) = -(ax + bx) / (bx - ax);
        crop.at(1, 1) = 2.0f / (by - ay);
        crop.at(1, 3) = -(ay + by) / (by - ay);
        projection = mat4_multiply(crop, projection);
    }

    PortalClipPlanes clip;
    const bool applied_clip = m_use_clip && portal_clip_planes(link.dst, vcam.pos, &clip);

    // The diagnostics keep meaning depth zero. They answer M0's second exit
    // criterion, which is about the camera being inside a frame in the world --
    // a question about the outermost view, not about level three of a tunnel.
    //
    // Per link, not one flag for the last one rendered. With two portals facing
    // each other the player is usually looking at one and standing with their
    // back to the other, and the one behind them legitimately switches its
    // oblique plane off -- so a single flag reported "off" while the portal
    // being looked at had it on.
    if (depth == 0 && index < kMaxDiagnosticLinks) {
        m_oblique_applied[index] = applied_oblique;
        m_clip_applied[index]    = applied_clip;
    }

    // Deeper first. Two reasons, and either one alone would be enough: the
    // openings this pass draws have to contain an image already, and a render
    // target cannot be bound while another one is.
    const bool deeper = depth < m_max_depth;
    if (deeper)
        for (int j = 0; j < link_count; ++j)
            render_into(world_render, links, link_count, j, vcam.pos, vcam.forward, vcam.up,
                        fov_y, aspect, z_near, z_far, depth + 1);

    // The sun's shadows around what this camera sees (see render_target).
    // ponytail: one shadow pass per opening drawn; reuse the eye's map when
    // the far end lies well inside it, if openings show up in the profile.
    world_render.draw_shadows(vcam.pos, vcam.forward);

    gfx_rt_begin(target);
    if (cropped) {
        gfx_viewport(x0, y0, x1 - x0, y1 - y0);
        gfx_scissor(x0, y0, x1 - x0, y1 - y0);
    }
    gfx_clear(m_sky[0], m_sky[1], m_sky[2], m_sky[3], true);

    gfx_state state = gfx_state_default();
    gfx_state_set(&state);

    if (applied_clip)
        gfx_clip_planes_set(&clip.planes[0].x, clip.count);
    else
        gfx_clip_planes_set(nullptr, 0);

    // The water around the far end, for everything this camera draws: the far
    // side of an opening under a lake is under the lake. It was off for every
    // camera but the player's until 2026-09-19, and an opening above water onto
    // one below it showed the lake bed as if the lake were air.
    world_render.set_water(true, vcam.pos, 1 + (index % (WorldRenderer::kWaterSlots - 1)));
    world_render.draw(projection, view);

    // Bodies inside the portal's image, with the same clip planes the world got:
    // a crate standing in front of the destination is as much a thing that must
    // not leak into the image as the destination's own frame is.
    if (m_extra) m_extra(m_extra_ud, projection, view);
    world_render.draw_translucent(projection, view);   // over them, as the main pass

    // Clearing before the openings, not after the whole pass: the openings are
    // drawn from the virtual camera's point of view and have no business being
    // carved by the destination's clip planes.
    gfx_clip_planes_set(nullptr, 0);

    // Which portals get drawn (PORTAL_SYSTEM.md 3.8). A portal the virtual
    // camera is BEHIND is the one it is looking out through, and painting that
    // opening would cover the whole view with the image of somewhere else --
    // you look through a portal, you do not look at its surface from inside.
    //
    // Not reached by the recursion gate, and that is stated rather than
    // assumed: deleting this line leaves it green, because the oblique near
    // plane is anchored on exactly the face this quad sits on and clips it away
    // first. Two mechanisms covering one case is defence in depth working; it
    // is also a check nobody can break, so it is not counted as one.
    for (int j = 0; j < link_count; ++j) {
        if (dot(vcam.pos - links[j].src.center, links[j].src.normal) < 0.0f) continue;
        if (deeper)
            draw_opening_from(links[j], rt_at(links, j, depth + 1), vcam.pos, projection, view);
        else
            draw_opening_terminal(links[j], vcam.pos, projection, view);
    }
    world_render.set_water(false);

    if (cropped) gfx_scissor_off();
    gfx_rt_end();
    ++m_pass_count;
}

// The last level. Drawn, and drawn FLAT.
//
// Leaving it out is what this renderer did until now, and it does not read as
// "the recursion stopped" -- it reads as a hole. The opening is simply not
// painted, so what shows through it is whatever stands behind the wall it is
// carved into: with a facing pair, a rectangle of open field at the end of a
// corridor, indistinguishable at a glance from one more level. A player
// counting levels counts one too many, which is exactly what happened the first
// time somebody stood in that corridor and counted.
//
// Wall colour, not sky. Both close the tunnel, and sky was the first choice --
// it is what this engine already paints where nothing was rendered, including
// the clear on every one of these targets. Two things against it. It is a
// bright rectangle at the end of a dark corridor, which draws the eye to
// exactly the place where the picture is weakest. And being the clear colour,
// no test can tell it from the sky it stands in front of: a gate for this fill
// would have had nothing to count. Wall says "the hole ends here", which is
// what a portal with nothing behind it is.
void PortalRenderer::draw_opening_terminal(const PortalLink &link, Vec3 cam_pos,
                                           const Mat4 &projection, const Mat4 &view)
{
    if (m_mesh.id == 0) return;

    const float side      = dot(cam_pos - link.src.center, link.src.normal);
    const float side_sign = (side >= 0.0f) ? 1.0f : -1.0f;

    PortalVertex verts[kPortalVertices];
    int          painted = 0;
    const int    count   = build_opening(link.src, side_sign, verts, painted);
    gfx_buffer_update(m_vb, verts, count * (int)sizeof(PortalVertex), 0);

    const Mat4 model = mat4_identity();
    gfx_shader_bind(m_shader);
    gfx_projection_set(projection.m);
    gfx_view_set(view.m);
    gfx_model_set(model.m);
    m_water.set(projection, view);

    gfx_state state = gfx_state_default();
    state.cull  = GFX_CULL_NONE;
    state.blend = GFX_BLEND_NONE;
    gfx_state_set(&state);

    // Every quad flat, tunnel and image alike: one draw instead of two, because
    // at this level there is no image to tell them apart.
    gfx_uniform_set_int(m_loc_tunnel, 1);
    gfx_uniform_set_v4(m_loc_tunnel_color, kTunnelColour, 1);
    gfx_draw(m_mesh, count, 0);
    gfx_uniform_set_int(m_loc_tunnel, 0);

    state.cull = GFX_CULL_BACK;
    gfx_state_set(&state);
}

void PortalRenderer::draw_closed(const PortalSurface &s, const float rgb[3], Vec3 cam_pos,
                                 const Mat4 &projection, const Mat4 &view, float open)
{
    if (m_mesh.id == 0) return;

    // One quad across the doorway's mouth, a hair inside it: nothing behind it
    // shows, so the doorway's sides are not drawn.
    const float side_sign = (dot(cam_pos - s.center, s.normal) >= 0.0f) ? 1.0f : -1.0f;
    const Vec3  r         = s.right();
    const Vec3  m = s.center + s.normal * (side_sign * (entrance_depth(s, side_sign) - 2.0f * kEdgeInset));
    PortalVertex verts[6];
    int          count = 0;
    push_quad(verts, count, m - r * s.half_w - s.up * s.half_h, m + r * s.half_w - s.up * s.half_h,
              m + r * s.half_w + s.up * s.half_h, m - r * s.half_w + s.up * s.half_h);
    gfx_buffer_update(m_vb, verts, count * (int)sizeof(PortalVertex), 0);

    const Mat4 model = mat4_identity();
    gfx_shader_bind(m_shader);
    gfx_projection_set(projection.m);
    gfx_view_set(view.m);
    gfx_model_set(model.m);
    m_water.set(projection, view);

    gfx_state state = gfx_state_default();
    state.cull  = GFX_CULL_NONE;
    state.blend = GFX_BLEND_NONE;
    gfx_state_set(&state);

    static const auto t0 = std::chrono::steady_clock::now();
    const float t = std::chrono::duration<float>(std::chrono::steady_clock::now() - t0).count();
    const float colour[4] = {rgb[0], rgb[1], rgb[2], 1.0f};
    const float centre[4] = {m.x, m.y, m.z, 0.0f};
    const float right[4]  = {r.x, r.y, r.z, 0.0f};
    const float up[4]     = {s.up.x, s.up.y, s.up.z, 0.0f};
    const float half[2]   = {s.half_w, s.half_h};
    gfx_uniform_set_int(m_loc_tunnel, 2);
    gfx_uniform_set_v4(m_loc_closed_color, colour, 1);
    gfx_uniform_set_v4(m_loc_centre, centre, 1);
    gfx_uniform_set_v4(m_loc_right, right, 1);
    gfx_uniform_set_v4(m_loc_up, up, 1);
    gfx_uniform_set_v2(m_loc_half, half);
    gfx_uniform_set_f(m_loc_time, t);
    gfx_uniform_set_f(m_loc_open, open);
    gfx_draw(m_mesh, count, 0);
    gfx_uniform_set_int(m_loc_tunnel, 0);

    state.cull = GFX_CULL_BACK;
    gfx_state_set(&state);
}

void PortalRenderer::render_recursive(WorldRenderer &world_render, const PortalLink *links,
                                      int link_count, Vec3 cam_pos, Vec3 cam_forward,
                                      Vec3 cam_up, float fov_y, float aspect, float z_near,
                                      float z_far)
{
    m_pass_count = 0;
    for (int i = 0; i < link_count; ++i)
        render_into(world_render, links, link_count, i, cam_pos, cam_forward, cam_up, fov_y,
                    aspect, z_near, z_far, 0);
}

} // namespace cnp
