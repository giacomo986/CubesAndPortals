#include "client/render/selection.h"

#include "client/render/shaders.h"

#include <cstdio>
#include <vector>

namespace cnp {
namespace {

// Thickness of the outline bars, in block units.
constexpr float kThick = 0.028f;

struct P3 { float x, y, z; };

void push_box(std::vector<P3> &out, float x0, float y0, float z0, float x1, float y1, float z1)
{
    const P3 c[8] = {
        {x0, y0, z0}, {x1, y0, z0}, {x1, y1, z0}, {x0, y1, z0},
        {x0, y0, z1}, {x1, y0, z1}, {x1, y1, z1}, {x0, y1, z1},
    };
    static const int faces[6][4] = {
        {0, 3, 2, 1},   // -Z
        {4, 5, 6, 7},   // +Z
        {0, 4, 7, 3},   // -X
        {1, 2, 6, 5},   // +X
        {0, 1, 5, 4},   // -Y
        {3, 7, 6, 2},   // +Y
    };
    for (const auto &f : faces) {
        out.push_back(c[f[0]]); out.push_back(c[f[1]]); out.push_back(c[f[2]]);
        out.push_back(c[f[0]]); out.push_back(c[f[2]]); out.push_back(c[f[3]]);
    }
}

// Twelve bars along the edges of the unit cube, each sitting entirely OUTSIDE
// the block rather than straddling its surface.
//
// That placement is the whole trick. An outline that expands into the block's
// neighbours is invisible for any block flush with another -- which is most of
// them, and certainly every block of flat ground. Sitting just outside means
// the bars along the top edges clear the neighbouring block's top surface and
// show, while the bars along the bottom edges are correctly buried in the
// ground below.
std::vector<P3> build_outline()
{
    std::vector<P3> v;
    v.reserve(12 * 36);

    const float t = kThick;
    const float lo = -t, hi = 1.0f + t;   // bars overrun the corners so they join

    // Four bars along X, at each combination of the Y and Z edges.
    for (int iy = 0; iy < 2; ++iy) {
        for (int iz = 0; iz < 2; ++iz) {
            const float y0 = iy ? 1.0f : -t, y1 = iy ? 1.0f + t : 0.0f;
            const float z0 = iz ? 1.0f : -t, z1 = iz ? 1.0f + t : 0.0f;
            push_box(v, lo, y0, z0, hi, y1, z1);
        }
    }
    // Four along Y.
    for (int ix = 0; ix < 2; ++ix) {
        for (int iz = 0; iz < 2; ++iz) {
            const float x0 = ix ? 1.0f : -t, x1 = ix ? 1.0f + t : 0.0f;
            const float z0 = iz ? 1.0f : -t, z1 = iz ? 1.0f + t : 0.0f;
            push_box(v, x0, lo, z0, x1, hi, z1);
        }
    }
    // Four along Z.
    for (int ix = 0; ix < 2; ++ix) {
        for (int iy = 0; iy < 2; ++iy) {
            const float x0 = ix ? 1.0f : -t, x1 = ix ? 1.0f + t : 0.0f;
            const float y0 = iy ? 1.0f : -t, y1 = iy ? 1.0f + t : 0.0f;
            push_box(v, x0, y0, lo, x1, y1, hi);
        }
    }
    return v;
}

} // namespace

bool SelectionRenderer::init()
{
    m_shader = gfx_shader_create(shaders::kSolidVS, shaders::kSolidFS);
    if (!m_shader.id) {
        std::fprintf(stderr, "selection renderer: shader compilation failed\n");
        return false;
    }
    m_loc_color = gfx_uniform_location(m_shader, "uColor");

    const std::vector<P3> verts = build_outline();
    m_vertex_count = (int)verts.size();
    m_vb = gfx_buffer_create(verts.data(), m_vertex_count * (int)sizeof(P3), false, false);

    gfx_vertexlayout layout{};
    layout.stride     = (int)sizeof(P3);
    layout.attr_count = 1;
    layout.attrs[0]   = gfx_attr{0, 3, GFX_ATTR_F32, false, 0};
    m_mesh = gfx_mesh_create(&layout, m_vb, gfx_buffer{0});

    return m_mesh.id != 0;
}

void SelectionRenderer::shutdown()
{
    if (m_tri_mesh.id) gfx_mesh_destroy(m_tri_mesh);
    if (m_tri_vb.id)   gfx_buffer_destroy(m_tri_vb);
    if (m_mesh.id)   gfx_mesh_destroy(m_mesh);
    if (m_vb.id)     gfx_buffer_destroy(m_vb);
    if (m_shader.id) gfx_shader_destroy(m_shader);
}

void SelectionRenderer::draw(Vec3 block_min, const Mat4 &projection, const Mat4 &view)
{
    const float color[4] = {0.03f, 0.03f, 0.05f, 1.0f};
    draw_box(block_min, Vec3{1.0f, 1.0f, 1.0f}, color, projection, view);
}

void SelectionRenderer::draw_box(Vec3 min, Vec3 size, const float colour[4],
                                 const Mat4 &projection, const Mat4 &view)
{
    if (m_mesh.id == 0) return;

    // Scale then translate: the mesh is built around the unit cube at the
    // origin, so scaling first is what keeps the minimum corner where the
    // caller put it. The bars get scaled too, which is why a very flat box
    // shows thicker bars on its long axis -- for a player-sized box that is
    // not worth a second mesh.
    const Mat4 model = mat4_multiply(mat4_translate(min), mat4_scale(size));

    gfx_shader_bind(m_shader);
    gfx_projection_set(projection.m);
    gfx_view_set(view.m);
    gfx_model_set(model.m);

    gfx_uniform_set_v4(m_loc_color, colour, 1);

    // Culling off: the bars are thin, and which side of one is facing the
    // camera is not worth reasoning about for 432 vertices.
    gfx_state state = gfx_state_default();
    state.cull = GFX_CULL_NONE;
    gfx_state_set(&state);

    gfx_draw(m_mesh, m_vertex_count, 0);

    state.cull = GFX_CULL_BACK;
    gfx_state_set(&state);
}

void SelectionRenderer::draw_tris(const float *xyz, int count, Vec3 at, const float colour[4],
                                  const Mat4 &projection, const Mat4 &view)
{
    if (m_shader.id == 0 || count <= 0) return;

    // Grown when a bigger piece comes along, never shrunk: the largest piece
    // is a few thousand vertices.
    if (count > m_tri_cap) {
        if (m_tri_mesh.id) gfx_mesh_destroy(m_tri_mesh);
        if (m_tri_vb.id)   gfx_buffer_destroy(m_tri_vb);
        m_tri_cap = count;
        m_tri_vb  = gfx_buffer_create(nullptr, m_tri_cap * (int)sizeof(P3), true, false);
        gfx_vertexlayout layout{};
        layout.stride     = (int)sizeof(P3);
        layout.attr_count = 1;
        layout.attrs[0]   = gfx_attr{0, 3, GFX_ATTR_F32, false, 0};
        m_tri_mesh = gfx_mesh_create(&layout, m_tri_vb, gfx_buffer{0});
    }
    if (m_tri_mesh.id == 0) return;
    gfx_buffer_update(m_tri_vb, xyz, count * (int)sizeof(P3), 0);

    gfx_shader_bind(m_shader);
    gfx_projection_set(projection.m);
    gfx_view_set(view.m);
    const Mat4 model = mat4_translate(at);
    gfx_model_set(model.m);
    gfx_uniform_set_v4(m_loc_color, colour, 1);

    gfx_state state   = gfx_state_default();
    state.cull        = GFX_CULL_NONE;
    state.blend       = GFX_BLEND_ALPHA;
    state.depth_write = false;
    gfx_state_set(&state);

    gfx_draw(m_tri_mesh, count, 0);

    state = gfx_state_default();
    gfx_state_set(&state);
}

} // namespace cnp
