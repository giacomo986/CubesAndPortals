#include "client/render/screen_render.h"

#include <cstdio>

namespace cnp {
namespace {

const float kQuad[12] = {-1, -1, 1, -1, 1, 1, -1, -1, 1, 1, -1, 1};

const char *const kScreenVS = R"(#version 330 core
layout(location = 0) in vec2 aCorner;

uniform mat4 uProjection;
uniform mat4 uView;
uniform mat4 uModel;
uniform vec4 uClipPlanes[5];
uniform int  uClipCount;
uniform vec3 uCenter;
uniform vec3 uRight;
uniform vec3 uUp;

out vec2 vUV;
out float gl_ClipDistance[5];

void main()
{
    vec3 p = uCenter + uRight * aCorner.x + uUp * aCorner.y;
    vUV = aCorner * 0.5 + 0.5;
    for (int k = 0; k < 5; ++k)
        gl_ClipDistance[k] = (k < uClipCount) ? dot(vec4(p, 1.0), uClipPlanes[k]) : 1.0;
    gl_Position = uProjection * uView * vec4(p, 1.0);
}
)";

const char *const kScreenFS = R"(#version 330 core
in vec2 vUV;
out vec4 fragColor;

uniform sampler2D uTex;
uniform int   uHas;
uniform float uTime;

float hash(vec2 p) { return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453); }

void main()
{
    vec3 col;
    if (uHas != 0) {
        // A render target is upright as GL keeps it: its first row at the bottom.
        col = texture(uTex, vUV).rgb;
        col *= 0.92 + 0.08 * sin(vUV.y * 128.0 * 3.14159);   // scan lines
    } else {
        col = vec3(hash(floor(vUV * 64.0) + floor(uTime * 24.0)) * 0.7);
    }
    // The edge of the glass a little darker.
    vec2  e    = min(vUV, 1.0 - vUV);
    float edge = smoothstep(0.0, 0.04, min(e.x, e.y));
    fragColor  = vec4(col * (0.75 + 0.25 * edge), 1.0);
}
)";

} // namespace

bool ScreenRenderer::init()
{
    m_shader = gfx_shader_create(kScreenVS, kScreenFS);
    if (!m_shader.id) {
        std::fprintf(stderr, "screen renderer: shader compilation failed\n");
        return false;
    }
    m_loc_center = gfx_uniform_location(m_shader, "uCenter");
    m_loc_right  = gfx_uniform_location(m_shader, "uRight");
    m_loc_up     = gfx_uniform_location(m_shader, "uUp");
    m_loc_tex    = gfx_uniform_location(m_shader, "uTex");
    m_loc_has    = gfx_uniform_location(m_shader, "uHas");
    m_loc_time   = gfx_uniform_location(m_shader, "uTime");

    m_vb = gfx_buffer_create(kQuad, (int)sizeof(kQuad), false, false);
    gfx_vertexlayout layout{};
    layout.stride     = 2 * (int)sizeof(float);
    layout.attr_count = 1;
    layout.attrs[0]   = gfx_attr{0, 2, GFX_ATTR_F32, false, 0};
    m_mesh            = gfx_mesh_create(&layout, m_vb, gfx_buffer{0});
    return m_mesh.id != 0;
}

void ScreenRenderer::shutdown()
{
    if (m_mesh.id)   gfx_mesh_destroy(m_mesh);
    if (m_vb.id)     gfx_buffer_destroy(m_vb);
    if (m_shader.id) gfx_shader_destroy(m_shader);
}

void ScreenRenderer::draw(gfx_texture tex, Vec3 center, Vec3 right, Vec3 up, float time,
                          const Mat4 &projection, const Mat4 &view)
{
    if (m_mesh.id == 0) return;
    gfx_shader_bind(m_shader);
    gfx_projection_set(projection.m);
    gfx_view_set(view.m);
    gfx_model_set(Mat4{}.m);
    const float c[3] = {center.x, center.y, center.z}, r[3] = {right.x, right.y, right.z},
                u[3] = {up.x, up.y, up.z};
    gfx_uniform_set_v3(m_loc_center, c);
    gfx_uniform_set_v3(m_loc_right, r);
    gfx_uniform_set_v3(m_loc_up, u);
    gfx_uniform_set_f(m_loc_time, time);
    gfx_uniform_set_int(m_loc_has, tex.id != 0 ? 1 : 0);
    gfx_texture_bind(tex, 0);
    gfx_uniform_set_int(m_loc_tex, 0);

    gfx_state state = gfx_state_default();
    state.cull      = GFX_CULL_NONE;
    gfx_state_set(&state);
    gfx_draw(m_mesh, 6, 0);
    gfx_texture_bind(gfx_texture{0}, 0);
}

} // namespace cnp
