#include "client/render/hurt_render.h"

#include <algorithm>
#include <cstdio>

namespace cnp {
namespace {

const float kQuad[12] = {-1, -1, 1, -1, 1, 1, -1, -1, 1, 1, -1, 1};

const char *const kHurtVS = R"(#version 330 core
layout(location = 0) in vec2 aCorner;
out vec2 vP;
void main()
{
    vP = aCorner;
    gl_Position = vec4(aCorner, 0.0, 1.0);
}
)";

// vP is the point on the screen, -1..1 each way. The red is a band round the
// edge -- nothing in the middle third, deepest in the corners -- and each
// mark lays it on the side it came from, a lobe about 100 degrees wide, or all
// round, fainter, for one that came from no side.
const char *const kHurtFS = R"(#version 330 core
in vec2 vP;
out vec4 fragColor;

uniform float uAspect;
uniform int   uCount;
uniform vec4  uMarks[8];   // x, y: the side it came from; z: strength; w: 1 if from a side

void main()
{
    vec2  q    = vec2(vP.x * min(uAspect, 1.6) / 1.6, vP.y);
    float edge = smoothstep(0.45, 1.25, length(vP * vec2(1.0, 0.92)));
    vec2  dir  = length(q) > 1e-4 ? normalize(q) : vec2(0.0);
    float a    = 0.0;
    for (int i = 0; i < 8; ++i) {
        if (i >= uCount) break;
        vec4  m    = uMarks[i];
        float lobe = m.w > 0.5 ? pow(max(dot(dir, m.xy), 0.0), 2.5) : 0.45;
        a += m.z * lobe;
    }
    a = clamp(a, 0.0, 1.0) * edge * 0.62;
    // Darker red at the very edge, a little brighter where it fades in.
    vec3 col = mix(vec3(0.85, 0.06, 0.04), vec3(0.45, 0.0, 0.0), edge);
    fragColor = vec4(col, a);
}
)";

} // namespace

bool HurtRenderer::init()
{
    m_shader = gfx_shader_create(kHurtVS, kHurtFS);
    if (!m_shader.id) {
        std::fprintf(stderr, "hurt renderer: shader compilation failed\n");
        return false;
    }
    m_loc_aspect = gfx_uniform_location(m_shader, "uAspect");
    m_loc_count  = gfx_uniform_location(m_shader, "uCount");
    m_loc_marks  = gfx_uniform_location(m_shader, "uMarks");

    m_vb = gfx_buffer_create(kQuad, (int)sizeof(kQuad), false, false);
    gfx_vertexlayout layout{};
    layout.stride     = 2 * (int)sizeof(float);
    layout.attr_count = 1;
    layout.attrs[0]   = gfx_attr{0, 2, GFX_ATTR_F32, false, 0};
    m_mesh            = gfx_mesh_create(&layout, m_vb, gfx_buffer{0});
    return m_mesh.id != 0;
}

void HurtRenderer::shutdown()
{
    if (m_mesh.id)   gfx_mesh_destroy(m_mesh);
    if (m_vb.id)     gfx_buffer_destroy(m_vb);
    if (m_shader.id) gfx_shader_destroy(m_shader);
}

void HurtRenderer::draw(const Mark *marks, int count, int width, int height)
{
    if (m_mesh.id == 0 || count <= 0 || width <= 0 || height <= 0) return;
    count = std::min(count, kMarks);
    float v[4 * kMarks] = {};
    for (int i = 0; i < count; ++i) {
        v[i * 4 + 0] = marks[i].x;
        v[i * 4 + 1] = marks[i].y;
        v[i * 4 + 2] = std::clamp(marks[i].strength, 0.0f, 1.0f);
        v[i * 4 + 3] = marks[i].from_side ? 1.0f : 0.0f;
    }
    gfx_shader_bind(m_shader);
    gfx_uniform_set_f(m_loc_aspect, (float)width / (float)height);
    gfx_uniform_set_int(m_loc_count, count);
    gfx_uniform_set_v4(m_loc_marks, v, count);

    gfx_state state   = gfx_state_default();
    state.depth_test  = false;
    state.depth_write = false;
    state.cull        = GFX_CULL_NONE;
    state.blend       = GFX_BLEND_ALPHA;
    gfx_state_set(&state);
    gfx_draw(m_mesh, 6, 0);
    const gfx_state back = gfx_state_default();
    gfx_state_set(&back);
    gfx_shader_bind_default();
}

} // namespace cnp
