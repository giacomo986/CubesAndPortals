#include "client/render/body_render.h"

#include "client/render/shaders.h"

#include <cstdio>
#include <vector>

namespace cnp {
namespace {

struct BodyVertex {
    float x, y, z;
    float shade;
};

// A unit cube centred on the origin, so the model matrix is a translate and a
// scale and nothing has to know where the corner is.
//
// The shade is per vertex and per face. Its only job is to make the shape read
// as a solid rather than as a silhouette: without it, a cut through a
// single-colour body is invisible against a background of the same colour,
// which is the one thing this renderer exists to show.
std::vector<BodyVertex> build_cube()
{
    const float h = 0.5f;

    struct Face {
        float nx, ny, nz;
        float shade;
    };
    static const Face faces[6] = {
        {0.0f, 1.0f, 0.0f, 1.00f},    // top
        {0.0f, -1.0f, 0.0f, 0.42f},   // bottom
        {1.0f, 0.0f, 0.0f, 0.78f},    // +X
        {-1.0f, 0.0f, 0.0f, 0.70f},   // -X
        {0.0f, 0.0f, 1.0f, 0.62f},    // +Z
        {0.0f, 0.0f, -1.0f, 0.54f},   // -Z
    };

    std::vector<BodyVertex> v;
    v.reserve(36);

    for (const Face &f : faces) {
        // Two in-plane axes for this face, right handed about its normal.
        const Vec3 n{f.nx, f.ny, f.nz};
        Vec3       a = (std::fabs(n.y) > 0.5f) ? Vec3{1.0f, 0.0f, 0.0f} : Vec3{0.0f, 1.0f, 0.0f};
        const Vec3 u = normalize(cross(a, n));
        const Vec3 w = cross(n, u);

        const Vec3 c = n * h;
        const Vec3 p0 = c - u * h - w * h;
        const Vec3 p1 = c + u * h - w * h;
        const Vec3 p2 = c + u * h + w * h;
        const Vec3 p3 = c - u * h + w * h;

        const Vec3 quad[6] = {p0, p1, p2, p0, p2, p3};
        for (const Vec3 &p : quad) v.push_back(BodyVertex{p.x, p.y, p.z, f.shade});
    }
    return v;
}

} // namespace

bool BodyRenderer::init()
{
    m_shader = gfx_shader_create(shaders::kBodyVS, shaders::kBodyFS);
    if (!m_shader.id) {
        std::fprintf(stderr, "body renderer: shader compilation failed\n");
        return false;
    }
    m_loc_color = gfx_uniform_location(m_shader, "uColor");
    m_water.locate(m_shader);

    const std::vector<BodyVertex> verts = build_cube();
    m_vertex_count                      = (int)verts.size();
    m_vb = gfx_buffer_create(verts.data(), m_vertex_count * (int)sizeof(BodyVertex), false, false);

    gfx_vertexlayout layout{};
    layout.stride     = (int)sizeof(BodyVertex);
    layout.attr_count = 2;
    layout.attrs[0]   = gfx_attr{0, 3, GFX_ATTR_F32, false, 0};
    layout.attrs[1]   = gfx_attr{1, 1, GFX_ATTR_F32, false, 12};
    m_mesh            = gfx_mesh_create(&layout, m_vb, gfx_buffer{0});

    return m_mesh.id != 0;
}

void BodyRenderer::shutdown()
{
    if (m_mesh.id)   gfx_mesh_destroy(m_mesh);
    if (m_vb.id)     gfx_buffer_destroy(m_vb);
    if (m_shader.id) gfx_shader_destroy(m_shader);
}

void BodyRenderer::draw(const Body &body, const Mat4 &projection, const Mat4 &view)
{
    draw_impl(body, nullptr, mat4_identity(), projection, view);
}

void BodyRenderer::draw_clipped(const Body &body, const Plane &plane, const Mat4 &transform,
                                const Mat4 &projection, const Mat4 &view)
{
    draw_impl(body, &plane, transform, projection, view);
}

void BodyRenderer::draw_impl(const Body &body, const Plane *plane, const Mat4 &transform,
                             const Mat4 &projection, const Mat4 &view)
{
    if (m_mesh.id == 0) return;

    const Mat4 local = mat4_multiply(mat4_translate(body.center),
                                     mat4_scale(Vec3{2.0f * body.half.x, 2.0f * body.half.y,
                                                     2.0f * body.half.z}));
    const Mat4 model = mat4_multiply(transform, local);

    ++m_draws;

    gfx_shader_bind(m_shader);
    gfx_projection_set(projection.m);
    gfx_view_set(view.m);
    gfx_model_set(model.m);
    gfx_uniform_set_v4(m_loc_color, body.colour, 1);
    m_water.set(projection, view);

    // Culling off. A body cut open is seen from the inside through the cut, and
    // back-face culling there would show the hole as a window straight out the
    // far side of the solid.
    gfx_state state = gfx_state_default();
    state.cull      = GFX_CULL_NONE;
    gfx_state_set(&state);

    if (plane) gfx_clip_planes_set(&plane->x, 1);
    gfx_draw(m_mesh, m_vertex_count, 0);
    if (plane) gfx_clip_planes_set(nullptr, 0);

    state.cull = GFX_CULL_BACK;
    gfx_state_set(&state);
}

} // namespace cnp
