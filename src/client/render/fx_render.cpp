#include "client/render/fx_render.h"

#include <cstdio>
#include <cstring>

namespace cnp {
namespace {

// The quad: a corner in -1..1 each way. Where it goes is the vertex shader's:
// facing the eye, flat on the ground, or drawn out between two points.
const float kQuad[12] = {-1, -1, 1, -1, 1, 1, -1, -1, 1, 1, -1, 1};

const char *const kFxVS = R"(#version 330 core
layout(location = 0) in vec2 aCorner;

uniform mat4 uProjection;
uniform mat4 uView;
uniform mat4 uModel;
uniform vec4 uClipPlanes[5];
uniform int  uClipCount;

uniform int   uKind;
uniform vec3  uCenter;
uniform vec3  uEnd;
uniform float uSize;

out vec2 vUV;
out float gl_ClipDistance[5];

void main()
{
    vec3 p;
    if (uKind == 2 || uKind == 3) {
        // On the ground, a little over it so the floor does not cut it.
        p = uCenter + vec3(aCorner.x, 0.02, aCorner.y) * vec3(uSize, 1.0, uSize);
    } else if (uKind == 4 || uKind == 8) {
        // A ribbon from the centre to the end, turned to the eye about its length.
        vec3 eye  = (inverse(uView) * vec4(0.0, 0.0, 0.0, 1.0)).xyz;
        vec3 mid  = mix(uCenter, uEnd, aCorner.y * 0.5 + 0.5);
        vec3 axis = uEnd - uCenter;
        vec3 side = normalize(cross(axis, eye - mid) + vec3(1e-5));
        p = mid + side * aCorner.x * uSize;
    } else {
        vec3 right = vec3(uView[0][0], uView[1][0], uView[2][0]);
        vec3 up    = vec3(uView[0][1], uView[1][1], uView[2][1]);
        p = uCenter + (right * aCorner.x + up * aCorner.y) * uSize;
    }
    vUV = aCorner;
    for (int k = 0; k < 5; ++k)
        gl_ClipDistance[k] = (k < uClipCount) ? dot(vec4(p, 1.0), uClipPlanes[k]) : 1.0;
    gl_Position = uProjection * uView * vec4(p, 1.0);
}
)";

// Every kind is a brightness over the quad; the colour is the mod's, the
// hottest part of it whiter. Output alpha is the brightness: GFX_BLEND_ADD
// adds colour times it.
const char *const kFxFS = R"(#version 330 core
in vec2 vUV;
out vec4 fragColor;

uniform int   uKind;
uniform vec4  uColor;
uniform float uT;      // 0 just made .. 1 gone
uniform float uTime;   // seconds, for what flickers

const float PI = 3.14159265;

float hash(vec2 p) { return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453); }
float noise(vec2 p)
{
    vec2 i = floor(p), f = fract(p);
    vec2 u = f * f * (3.0 - 2.0 * f);
    return mix(mix(hash(i), hash(i + vec2(1, 0)), u.x), mix(hash(i + vec2(0, 1)), hash(i + vec2(1, 1)), u.x), u.y);
}
float fbm(vec2 p)
{
    float v = 0.0, a = 0.5;
    for (int i = 0; i < 4; ++i) { v += a * noise(p); p *= 2.03; a *= 0.5; }
    return v;
}

void main()
{
    vec2  uv   = vUV;
    float r    = length(uv);
    float ang  = atan(uv.y, uv.x);
    float fade = 1.0 - uT;
    float b    = 0.0;      // brightness
    float hot  = 0.0;      // how much of it is white

    if (uKind == 0) {            // glow
        b   = exp(-r * r * 5.0) * fade;
        hot = exp(-r * r * 22.0);
    } else if (uKind == 1) {     // flame: noise climbing up a teardrop
        vec2  q = vec2(uv.x * 1.3, uv.y + 0.25);
        float n = fbm(vec2(q.x * 3.0, q.y * 2.5 - uTime * 3.5));
        float shape = 1.0 - length(vec2(q.x, max(q.y, 0.0) * 0.55 + min(q.y, 0.0) * 1.4));
        b   = clamp((shape + n * 0.55 - 0.45) * 2.4, 0.0, 1.0) * fade;
        hot = clamp((shape - 0.45) * 2.5, 0.0, 1.0);
    } else if (uKind == 2) {     // ring: a shockwave out to the rim
        float rad  = uT;
        float n    = fbm(vec2(ang * 3.0, uTime * 2.0));
        float band = exp(-pow((r - rad) * (9.0 + n * 6.0), 2.0));
        float fill = smoothstep(rad, 0.0, r) * 0.18;
        b   = (band + fill) * fade * step(r, 1.0);
        hot = band * 0.5;
    } else if (uKind == 3) {     // frost: shards out from the middle and a ring
        float rad    = min(1.0, uT * 1.6);
        float spikes = pow(abs(cos(ang * 6.0 + noise(vec2(ang * 4.0, 1.0)) * 2.0)), 18.0);
        float inside = smoothstep(rad, rad * 0.6, r);
        float band   = exp(-pow((r - rad) * 10.0, 2.0));
        b   = (spikes * inside * 0.9 + band * 0.8 + inside * 0.15) * fade * step(r, 1.0);
        hot = spikes * inside * 0.6;
    } else if (uKind == 4) {     // bolt: a jagged line down the ribbon, flickering
        float y   = uv.y * 6.0;
        float off = (noise(vec2(y * 2.0, floor(uTime * 24.0))) - 0.5) * 0.9
                  + (noise(vec2(y * 7.0, floor(uTime * 24.0) + 3.0)) - 0.5) * 0.35;
        float d   = abs(uv.x - off);
        b   = (exp(-d * d * 220.0) + exp(-d * d * 12.0) * 0.35) * fade;
        b  *= 0.75 + 0.25 * noise(vec2(uTime * 40.0, 0.0));
        hot = exp(-d * d * 600.0);
    } else if (uKind == 5) {     // slash: a crescent swept across, thin at its ends
        float sweep = mix(-PI * 0.75, PI * 0.75, smoothstep(0.0, 0.55, uT));
        float a     = ang;
        float along = step(-PI * 0.75, a) * step(a, sweep);
        float trail = clamp(1.0 - (sweep - a) / 2.2, 0.0, 1.0);
        float band  = exp(-pow((r - 0.70) * (6.0 + 6.0 * (1.0 - trail)), 2.0));
        b   = band * along * (0.35 + 0.65 * trail) * fade * 1.8;
        hot = band * trail * trail;
    } else if (uKind == 6) {     // sparks: streaks flying out
        float cells = 18.0;
        float id    = floor((ang + PI) / (2.0 * PI) * cells);
        float c     = fract((ang + PI) / (2.0 * PI) * cells) - 0.5;
        float len   = 0.4 + 0.6 * hash(vec2(id, 7.0));
        float head  = uT * len * 1.3;
        float on    = step(hash(vec2(id, 3.0)), 0.7);
        float line  = exp(-c * c * 90.0) * smoothstep(head - 0.35, head, r) * step(r, head);
        b   = line * on * fade + exp(-r * r * 30.0) * (1.0 - uT) * (1.0 - uT);
        hot = line * 0.6;
    } else if (uKind == 8) {     // jet: from the nozzle (bottom) out to the tip, narrowing and licking
        float along = uv.y * 0.5 + 0.5;
        float n     = fbm(vec2(uv.x * 2.5, along * 5.0 - uTime * 16.0));
        float width = (1.0 - along * 0.85) * (0.55 + 0.45 * n);
        float d     = abs(uv.x) / max(width, 1e-3);
        b   = clamp(1.0 - d, 0.0, 1.0) * (1.0 - smoothstep(0.45 + 0.4 * n, 1.0, along)) * 1.6 * fade;
        hot = clamp(1.0 - d * 1.8, 0.0, 1.0) * (1.0 - along * 1.6);
    } else {                     // vortex: arms wound about the middle
        float arms = 0.5 + 0.5 * sin(ang * 3.0 + r * 9.0 - uTime * 6.0);
        float n    = fbm(vec2(ang * 2.0 + uTime, r * 4.0));
        float edge = smoothstep(1.0, 0.55, r);
        b   = pow(arms, 3.0) * edge * (0.6 + n * 0.8) * sin(uT * PI) + exp(-r * r * 12.0) * 0.5 * sin(uT * PI);
        hot = exp(-r * r * 30.0) * 0.6;
    }

    b *= uColor.a;
    if (b < 0.003) discard;
    vec3 col = mix(uColor.rgb, vec3(1.0), clamp(hot, 0.0, 1.0) * 0.8);
    fragColor = vec4(col, clamp(b, 0.0, 1.0));
}
)";

const char *const kMarkVS = R"(#version 330 core
layout(location = 0) in vec4 aCorner;   // x y z, and when it was left

uniform mat4 uProjection;
uniform mat4 uView;
uniform mat4 uModel;
uniform vec4 uClipPlanes[5];
uniform int  uClipCount;
uniform float uTime;
uniform float uLife;

out float vFade;
out float gl_ClipDistance[5];

void main()
{
    vFade = clamp(1.0 - (uTime - aCorner.w) / uLife, 0.0, 1.0);
    for (int k = 0; k < 5; ++k)
        gl_ClipDistance[k] = (k < uClipCount) ? dot(vec4(aCorner.xyz, 1.0), uClipPlanes[k]) : 1.0;
    gl_Position = uProjection * uView * vec4(aCorner.xyz, 1.0);
}
)";

const char *const kMarkFS = R"(#version 330 core
in float vFade;
out vec4 fragColor;
void main()
{
    if (vFade <= 0.0) discard;
    fragColor = vec4(0.02, 0.02, 0.025, 0.75 * vFade);
}
)";

} // namespace

int fx_kind_named(const char *name)
{
    static const char *const kNames[] = {"glow", "flame", "ring", "frost", "bolt", "slash", "sparks", "vortex", "jet"};
    for (int k = 0; k < (int)FxKind::Count; ++k)
        if (name && std::strcmp(name, kNames[k]) == 0) return k;
    return -1;
}

bool FxRenderer::init()
{
    m_shader = gfx_shader_create(kFxVS, kFxFS);
    if (!m_shader.id) {
        std::fprintf(stderr, "fx renderer: shader compilation failed\n");
        return false;
    }
    m_loc_kind   = gfx_uniform_location(m_shader, "uKind");
    m_loc_center = gfx_uniform_location(m_shader, "uCenter");
    m_loc_end    = gfx_uniform_location(m_shader, "uEnd");
    m_loc_size   = gfx_uniform_location(m_shader, "uSize");
    m_loc_color  = gfx_uniform_location(m_shader, "uColor");
    m_loc_t      = gfx_uniform_location(m_shader, "uT");
    m_loc_time   = gfx_uniform_location(m_shader, "uTime");

    m_vb = gfx_buffer_create(kQuad, (int)sizeof(kQuad), false, false);
    gfx_vertexlayout layout{};
    layout.stride     = 2 * (int)sizeof(float);
    layout.attr_count = 1;
    layout.attrs[0]   = gfx_attr{0, 2, GFX_ATTR_F32, false, 0};
    m_mesh            = gfx_mesh_create(&layout, m_vb, gfx_buffer{0});

    m_mark_shader = gfx_shader_create(kMarkVS, kMarkFS);
    if (!m_mark_shader.id) {
        std::fprintf(stderr, "fx renderer: mark shader compilation failed\n");
        return false;
    }
    m_loc_mark_time = gfx_uniform_location(m_mark_shader, "uTime");
    m_loc_mark_life = gfx_uniform_location(m_mark_shader, "uLife");
    return m_mesh.id != 0;
}

void FxRenderer::shutdown()
{
    if (m_mark_mesh.id)   gfx_mesh_destroy(m_mark_mesh);
    if (m_mark_vb.id)     gfx_buffer_destroy(m_mark_vb);
    if (m_mark_shader.id) gfx_shader_destroy(m_mark_shader);
    if (m_mesh.id)   gfx_mesh_destroy(m_mesh);
    if (m_vb.id)     gfx_buffer_destroy(m_vb);
    if (m_shader.id) gfx_shader_destroy(m_shader);
}

void FxRenderer::draw(FxKind kind, Vec3 at, Vec3 to, float size, const float rgba[4], float t, float time,
                      const Mat4 &projection, const Mat4 &view)
{
    if (m_mesh.id == 0) return;
    gfx_shader_bind(m_shader);
    gfx_projection_set(projection.m);
    gfx_view_set(view.m);
    gfx_model_set(Mat4{}.m);
    gfx_uniform_set_int(m_loc_kind, (int)kind);
    const float c[3] = {at.x, at.y, at.z}, e[3] = {to.x, to.y, to.z};
    gfx_uniform_set_v3(m_loc_center, c);
    gfx_uniform_set_v3(m_loc_end, e);
    gfx_uniform_set_f(m_loc_size, size);
    gfx_uniform_set_v4(m_loc_color, rgba, 1);
    gfx_uniform_set_f(m_loc_t, t);
    gfx_uniform_set_f(m_loc_time, time);

    gfx_state state   = gfx_state_default();
    state.cull        = GFX_CULL_NONE;
    state.depth_write = false;
    state.blend       = GFX_BLEND_ADD;
    gfx_state_set(&state);
    gfx_draw(m_mesh, 6, 0);
    state = gfx_state_default();
    gfx_state_set(&state);
}

void FxRenderer::draw_marks(const std::vector<float> &corners, bool changed, float time, float life,
                            const Mat4 &projection, const Mat4 &view)
{
    if (m_mark_shader.id == 0 || corners.empty()) return;
    const int count = (int)(corners.size() / 4);
    if (count > m_mark_capacity) {   // grown to the ring once, then only updated
        if (m_mark_mesh.id) gfx_mesh_destroy(m_mark_mesh);
        if (m_mark_vb.id)   gfx_buffer_destroy(m_mark_vb);
        m_mark_vb = gfx_buffer_create(corners.data(), (int)(corners.size() * sizeof(float)), true, false);
        gfx_vertexlayout layout{};
        layout.stride     = 4 * (int)sizeof(float);
        layout.attr_count = 1;
        layout.attrs[0]   = gfx_attr{0, 4, GFX_ATTR_F32, false, 0};
        m_mark_mesh       = gfx_mesh_create(&layout, m_mark_vb, gfx_buffer{0});
        m_mark_capacity   = count;
    } else if (changed) {
        gfx_buffer_update(m_mark_vb, corners.data(), (int)(corners.size() * sizeof(float)), 0);
    }
    m_mark_count = count;
    if (m_mark_mesh.id == 0) return;

    gfx_shader_bind(m_mark_shader);
    gfx_projection_set(projection.m);
    gfx_view_set(view.m);
    gfx_model_set(Mat4{}.m);
    gfx_uniform_set_f(m_loc_mark_time, time);
    gfx_uniform_set_f(m_loc_mark_life, life);
    gfx_state state   = gfx_state_default();
    state.cull        = GFX_CULL_NONE;
    state.depth_write = false;
    state.blend       = GFX_BLEND_ALPHA;
    gfx_state_set(&state);
    gfx_draw(m_mark_mesh, m_mark_count, 0);
    state = gfx_state_default();
    gfx_state_set(&state);
}

} // namespace cnp
