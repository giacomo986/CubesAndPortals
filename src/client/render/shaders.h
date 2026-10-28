#pragma once

// GLSL 3.30 core sources, inline so the spike has no asset paths to get wrong.
//
// The uniform names uProjection / uView / uModel / uClipPlanes / uClipCount are
// the contract gfx_raylib.cpp resolves at shader creation; renaming one here
// silently stops it being uploaded.

namespace cnp::shaders {

// Lateral clip planes live in the vertex shader as gl_ClipDistance, the desktop
// GL 3.3 path of DESIGN.md 2.4. The portable fallback is the same five planes
// tested per fragment with discard; it belongs behind an #ifdef in this file
// rather than in a second renderer.
//
// Every attribute arrives as an UNSIGNED_BYTE that was NOT normalised, so the
// fetch hands the shader the integer value as a float and the packed fields are
// unpacked here. That is what lets the 8-byte vertex of mesher.h work without
// glVertexAttribIPointer, which rlgl does not expose.
inline constexpr const char *kWorldVS = R"(#version 330 core
layout(location = 0) in vec3  aPos;      // chunk-local, 0..32
layout(location = 1) in float aFaceAo;   // face in bits 0..2, AO in bits 3..4
layout(location = 2) in vec2  aUV;       // tiles, wraps once per block
layout(location = 3) in float aSky;      // brightness of the sky seen here, 0..255
layout(location = 4) in float aTex;      // texture layer
layout(location = 5) in float aFlow;     // a liquid's picture runs: 0 still, 1..8, 9 down
layout(location = 6) in float aLamp;     // brightness of the lamps here, already turned to the face, 0..255
layout(location = 7) in float aClassic;  // the classic look's light: sun in the high nibble, block in the low
layout(location = 8) in float aFoliage;  // 1: takes the season's colour (core.foliage)

uniform mat4 uProjection;
uniform mat4 uView;
uniform mat4 uModel;
uniform vec4 uClipPlanes[5];
uniform int  uClipCount;
uniform float uTime;                     // seconds, for what moves

out vec2  vUV;
out float vClassic;                      // the whole shade in the classic look
out float vSky;                          // how much sky this point sees, 0..1
out float vLamp;                         // the lamps' light on it, 0..1
out float vAo;
flat out float vTex;
flat out int   vFace;
flat out float vFoliage;
out vec3  vWorld;                        // for the water the eye looks through

out float gl_ClipDistance[5];

// The classic look (WorldRenderer::set_classic): on, and its daylight.
uniform float uClassic;
uniform float uClassicDay;

// Its flat per-direction shading, so faces read as faces without a light model.
const float kFaceShade[6] = float[6](0.80, 0.80, 1.00, 0.55, 0.68, 0.68);

// Ambient occlusion levels, darkest corner first.
const float kAoShade[4] = float[4](0.45, 0.66, 0.84, 1.00);

void main()
{
    vec4 world = uModel * vec4(aPos, 1.0);

    int bits   = int(aFaceAo + 0.5);
    int face   = bits & 7;
    int ao     = (bits >> 3) & 3;

    int   bitsC = int(aClassic + 0.5);
    float litC  = max(float(bitsC >> 4) / 15.0 * uClassicDay, float(bitsC & 15) / 15.0);
    vClassic    = kFaceShade[face] * kAoShade[ao] * (0.15 + 0.85 * litC);

    vSky  = aSky / 255.0;
    vLamp = aLamp / 255.0;
    vAo   = kAoShade[ao];
    vFace = face;
    vFoliage = aFoliage;

    vUV    = aUV;
    vTex   = aTex + 256.0 * float(bits >> 5);   // the tile's high bits ride in face_ao

    // A running liquid's picture slides the way it runs. A cube has no fifth
    // attribute and reads the default 0, so nothing else moves.
    int flow = int(aFlow + 0.5);
    if (flow > 0) {
        vec2 d = (flow == 9) ? vec2(0.0, -1.0)
                             : vec2(cos(float(flow - 1) * 0.785398), sin(float(flow - 1) * 0.785398));
        vUV -= d * uTime * ((flow == 9) ? 1.4 : 0.7);
    }
    vWorld = world.xyz;

    for (int k = 0; k < 5; ++k)
        gl_ClipDistance[k] = (k < uClipCount) ? dot(vec4(world.xyz, 1.0), uClipPlanes[k]) : 1.0;

    gl_Position = uProjection * uView * world;
}
)";

// The water in sight, shared by the world and by the sky behind it so the two
// cannot disagree about which part of the view is under water.
//
// Not a surface at one height: the water is the cells of it around the eye, in
// a box the renderer keeps (WorldRenderer::refresh_water), each with its
// surface's four corner heights exactly as the mesher worked them out -- so a
// waterfall is a wall of water, a stream's top slopes, and both are where they
// are drawn. A line of sight is walked through the box cell by cell, and what
// counts is how much of it runs under the surface of each cell of water.
//
// The line starts at the near plane, not at the eye. With the near plane
// half in a waterfall, half the screen is in the water and the other half is
// not, and the eye being wet does not make the dry half wet.
#define CNP_WATER_GLSL R"(
uniform vec4      uWater;        // xyz the box's lowest corner, w 1 when any of it is water
uniform vec4      uWaterLo;      // the cells of it that hold any water: from xyz,
uniform vec4      uWaterHi;      // to xyz, far side excluded, in the box's own frame
uniform vec4      uWaterColor;   // vec4 because the setter the renderer has uploads four
uniform sampler2D uWaterCells;   // the box, 64 x 32 x 64 cells, its 32 layers in 8 x 4 tiles

const ivec3 kWaterBox = ivec3(64, 32, 64);

// Cell c's surface at its four corners, 0..1, x + 2 z as the mesher has them;
// all four 0 where the cell is not water.
vec4 water_corners(ivec3 c)
{
    return texelFetch(uWaterCells, ivec2((c.y % 8) * 64 + c.x, (c.y / 8) * 64 + c.z), 0) *
           (255.0 / 16.0);
}

// How much of p + d t, t from ta to tb, is under the plane y = k + a x + b z,
// given as (k, a, b). In t, not in nodes.
float water_below(vec3 plane, vec3 p, vec3 d, float ta, float tb)
{
    vec3  qa = p + d * ta, qb = p + d * tb;
    float fa = qa.y - dot(plane, vec3(1.0, qa.x, qa.z));
    float fb = qb.y - dot(plane, vec3(1.0, qb.x, qb.z));
    if (fa >= 0.0 && fb >= 0.0) return 0.0;
    if (fa <  0.0 && fb <  0.0) return tb - ta;
    float tr = ta + (tb - ta) * fa / (fa - fb);
    return fa < 0.0 ? tr - ta : tb - tr;
}

// The same under the surface of one cell of water with corners h, p in the
// cell's own frame. The top is the mesher's two triangles, folded along the
// diagonal whose ends differ least (emit_liquid_cell): along x == z, or along
// x + z == 1. Each triangle's plane is used on its own side of the fold.
float water_in_cell(vec4 h, vec3 p, vec3 d, float ta, float tb)
{
    bool same = abs(h.w - h.x) < abs(h.z - h.y);   // folded along x == z
    vec3 side = same ? vec3(0.0, 1.0, -1.0) : vec3(-1.0, 1.0, 1.0);   // >= 0 on hi's side
    vec3 hi   = same ? vec3(h.x, h.y - h.x, h.w - h.y) : vec3(h.z + h.y - h.w, h.w - h.z, h.w - h.y);
    vec3 lo   = same ? vec3(h.x, h.w - h.z, h.z - h.x) : vec3(h.x, h.y - h.x, h.z - h.x);

    float sa = dot(side, vec3(1.0, p.x + d.x * ta, p.z + d.z * ta));
    float sb = dot(side, vec3(1.0, p.x + d.x * tb, p.z + d.z * tb));
    if (sa * sb >= 0.0) return water_below(sa + sb >= 0.0 ? hi : lo, p, d, ta, tb);
    float tf = ta + (tb - ta) * sa / (sa - sb);   // where the line crosses the fold
    return water_below(sa > 0.0 ? hi : lo, p, d, ta, tf) +
           water_below(sa > 0.0 ? lo : hi, p, d, tf, tb);
}

// How much of the straight line from a to b runs under water, in nodes. Only
// the part of it that crosses the cells holding water is walked. Past the edge
// of the box the water is taken to go on if the line leaves the box through a
// cell of it, and not to be there otherwise.
//
// ponytail: one texel fetch per cell crossed inside the span of the water, so
// a line skimming a lake end to end costs up to ~130; skip empty bricks with a
// coarser box if a frame over a big lake measures slow.
float water_between(vec3 a, vec3 b)
{
    if (uWater.w < 0.5) return 0.0;
    vec3 p  = a - uWater.xyz;
    vec3 d  = b - a;
    vec3 ds = mix(d, vec3(1e-9), lessThan(abs(d), vec3(1e-9)));

    // The part of the line inside the span of the water.
    vec3  t0 = (uWaterLo.xyz - p) / ds, t1 = (uWaterHi.xyz - p) / ds;
    vec3  tl = min(t0, t1), th = max(t0, t1);
    float t    = max(0.0, max(tl.x, max(tl.y, tl.z)));
    float tend = min(1.0, min(th.x, min(th.y, th.z)));
    if (t >= tend) return 0.0;

    // Where the line leaves the box itself, for the water past its edge.
    vec3  tb   = max(-p / ds, (vec3(kWaterBox) - p) / ds);
    float tbox = min(tb.x, min(tb.y, tb.z));

    ivec3 lo   = ivec3(uWaterLo.xyz), hi = ivec3(uWaterHi.xyz);
    ivec3 cell = clamp(ivec3(floor(p + d * t)), lo, hi - 1);
    ivec3 go   = ivec3(sign(d));
    vec3  adv  = 1.0 / abs(ds);   // t across one cell, per axis
    vec3  next = mix((vec3(cell) + vec3(greaterThan(d, vec3(0.0))) - p) / ds, vec3(2.0),
                     equal(go, ivec3(0)));

    float len   = length(d);
    float under = 0.0;
    for (int i = 0; i < 160; ++i) {
        if (any(lessThan(cell, lo)) || any(greaterThanEqual(cell, hi))) break;
        float out_t = min(min(next.x, next.y), min(next.z, tend));
        vec4  h     = water_corners(cell);
        bool  wet   = h.x > 0.0;
        if (wet) under += water_in_cell(h, p - vec3(cell), d, t, out_t);
        if (out_t >= tend) {
            if (wet && tend >= tbox - 1e-6) under += 1.0 - tend;
            break;
        }
        if (under * len > 20.0) break;   // as murky as it gets
        t = out_t;
        if (next.x <= next.y && next.x <= next.z) { cell.x += go.x; next.x += adv.x; }
        else if (next.y <= next.z)                { cell.y += go.y; next.y += adv.y; }
        else                                      { cell.z += go.z; next.z += adv.z; }
    }
    return under * len;
}

// Ten nodes of it take half of what is behind, and never more than three
// quarters: a lake is murky, not a wall.
float water_murk(float under) { return min(0.75, 1.0 - exp(-0.07 * under)); }

// What `rgb` looks like with `under` nodes of water in front of it. Tinted as
// soon as there is water in the way, over a quarter of a node rather than at
// once: the surface itself lies ON the line between wet and dry, and a step
// there speckles it.
vec3 water_shade(vec3 rgb, float under)
{
    vec3 tint = mix(vec3(1.0), vec3(0.80, 0.92, 1.0), min(1.0, under * 4.0));
    return mix(rgb * tint, uWaterColor.rgb, water_murk(under));
}

// The camera: xyz the eye; the near plane in the world, >= 0 in front of it;
// and, for the shaders that have a fragment's clip position and not its place
// in the world -- a body, a model, an opening -- the camera's axes, scaled so
// that the point a pixel sees at depth d along the view is
// eye + (X ndc.x + Y ndc.y + Z) d (WaterUniforms::set).
uniform vec4 uWaterEye;
uniform vec4 uWaterNear;
uniform vec4 uWaterRayX;
uniform vec4 uWaterRayY;
uniform vec4 uWaterRayZ;

// How far along the line from the eye to `p` it leaves the near plane, 0..1.
// A plane and not a distance, because a portal's near plane is the far
// opening, at a slant.
float water_lens(vec3 p)
{
    float fe = dot(uWaterNear.xyz, uWaterEye.xyz) + uWaterNear.w;
    float fp = dot(uWaterNear.xyz, p) + uWaterNear.w;
    return fp > fe ? clamp(fe / (fe - fp), 0.0, 1.0) : 0.0;
}

// `rgb`, seen through the water between the near plane and the fragment whose
// clip position is `clip`: the same line, walked the same way, as a block's.
vec3 water_fade(vec3 rgb, vec4 clip)
{
    if (uWater.w < 0.5) return rgb;
    vec2 ndc = clip.xy / clip.w;
    vec3 ray = uWaterRayX.xyz * ndc.x + uWaterRayY.xyz * ndc.y + uWaterRayZ.xyz;
    vec3 far = uWaterEye.xyz + ray * clip.w;
    return water_shade(rgb, water_between(mix(uWaterEye.xyz, far, water_lens(far)), far));
}
)"

inline constexpr const char *kWorldFS = R"(#version 330 core
in vec2  vUV;
in float vClassic;
in float vSky;
in float vLamp;
in float vAo;
flat in float vTex;
flat in int   vFace;
flat in float vFoliage;
in vec3  vWorld;

uniform sampler2D uTexture;
uniform vec4      uFoliage;      // core.foliage: the season's colour, and how much of it
uniform float     uNightSight;   // core.night_sight: nothing darker than this
uniform float     uAtlasTiles;   // tiles per row in the fallback atlas

// The sun or the moon (core/world/daylight.h SkyLight), and its shadow map.
uniform vec3            uSunDir;
uniform vec3            uSunColor;
uniform vec3            uAmbient;
uniform mat4            uShadowVP;
uniform sampler2DShadow uShadow;
uniform float           uShadowOn;
uniform float           uShadowTexel;   // one texel of the map, in nodes
uniform float           uClassic;

out vec4 fragColor;
)" CNP_WATER_GLSL R"(
// The faces in the mesher's order, +X -X +Y -Y +Z -Z.
const vec3 kNormal[6] = vec3[6](vec3(1, 0, 0), vec3(-1, 0, 0), vec3(0, 1, 0),
                                vec3(0, -1, 0), vec3(0, 0, 1), vec3(0, 0, -1));

// How much of the sky's own light each face gets: the top sees all of it,
// the bottom what the ground throws back.
const float kFaceAmbient[6] = float[6](0.80, 0.80, 1.00, 0.55, 0.68, 0.68);

// The darkest anything gets: mesher.h kLightFloor.
const float kLightFloor = 0.02;

// Fire is not white.
const vec3 kLampTint = vec3(1.0, 0.9, 0.76);

// How much of the sun reaches this point, 0..1. Pushed out along the normal by
// a texel and a half before looking it up, so a face does not shadow itself;
// four filtered taps for a soft edge; and fading to lit towards the map's
// rim, so the edge of the shadowed world is not a line on the ground.
float sun_shadow(vec3 p, vec3 n)
{
    if (uShadowOn < 0.5) return 1.0;
    vec4 c = uShadowVP * vec4(p + n * (1.5 * uShadowTexel), 1.0);
    vec3 s = c.xyz / c.w * 0.5 + 0.5;
    if (s.z > 1.0) return 1.0;
    vec2 rim = abs(s.xy - 0.5) * 2.0;
    float fade = smoothstep(0.85, 1.0, max(rim.x, rim.y));
    if (fade >= 1.0) return 1.0;
    float t = 1.0 / float(textureSize(uShadow, 0).x);
    float lit = 0.0;
    lit += texture(uShadow, vec3(s.xy + vec2(-0.5, -0.5) * t, s.z));
    lit += texture(uShadow, vec3(s.xy + vec2( 0.5, -0.5) * t, s.z));
    lit += texture(uShadow, vec3(s.xy + vec2(-0.5,  0.5) * t, s.z));
    lit += texture(uShadow, vec3(s.xy + vec2( 0.5,  0.5) * t, s.z));
    return mix(lit * 0.25, 1.0, fade);
}

void main()
{
    // Greedy meshing hands over UVs that run 0..w and 0..h, so the tile has to
    // repeat across the quad rather than stretch. fract() does the wrap; with
    // the texture array of M1 this becomes a layer index and the wrap is free.
    vec2 tile = fract(vUV);

    float layer = floor(vTex + 0.5);
    float col   = mod(layer, uAtlasTiles);
    float row   = floor(layer / uAtlasTiles);

    // Inset by half a texel so the wrap never samples the neighbouring tile.
    vec2 uv = (vec2(col, row) + clamp(tile, 0.002, 0.998)) / uAtlasTiles;

    vec4 texel = texture(uTexture, uv);
    if (vFoliage > 0.5) {
        float lum = dot(texel.rgb, vec3(0.3, 0.55, 0.15));
        texel.rgb = mix(texel.rgb, min(lum * uFoliage.rgb, vec3(1.0)), uFoliage.a);
    }

    // The sky's light: what this point sees of the sky, lit by the sky itself
    // and by the sun where the sun reaches. Block light does not dim at night;
    // taking the brighter rather than the sum keeps a torch from blowing out a
    // surface the sun already lights.
    vec3  n   = kNormal[vFace];
    float ndl = max(dot(n, uSunDir), 0.0);
    float sun = (ndl > 0.0 && vSky > 0.0) ? sun_shadow(vWorld, n) : 0.0;
    vec3  sky = vSky * (uAmbient * kFaceAmbient[vFace] + uSunColor * (ndl * sun));
    vec3  lit = max(max(sky, vLamp * kLampTint), vec3(uNightSight * 0.55));
    vec3  rgb = texel.rgb * vAo * (kLightFloor + (1.0 - kLightFloor) * min(lit, vec3(1.0)));
    if (uClassic > 0.5) rgb = texel.rgb * vClassic;

    if (uWater.w > 0.5) {
        // Where this pixel's line leaves the near plane.
        vec3 lens = mix(uWaterEye.xyz, vWorld, water_lens(vWorld));
        rgb       = water_shade(rgb, water_between(lens, vWorld));
    }
    fragColor = vec4(rgb, texel.a);
}
)";

// The sky seen through the water: what the clear colour shows where no block
// is, fogged by the same rule as the blocks. Without it a tree on the shore,
// seen from under the lake, is murky against a sky that is not.
//
// One triangle over the whole screen at the far plane, where the depth test
// lets it through only on pixels still at the depth they were cleared to, and
// blended over the clear rather than handed the sky's colour -- so it does not
// tint the way a block is, only fades. The ends of each pixel's line are read
// back through the camera: on the near plane and on the far one, both of which
// are flat, so interpolating them across the screen is exact.
inline constexpr const char *kWaterSkyVS = R"(#version 330 core
layout(location = 0) in vec3 aPos;   // already in clip space

uniform mat4 uProjection;
uniform mat4 uView;

// Undivided: the line's ends are linear in the screen position only before the
// divide, and a portal's oblique projection gives each corner its own w -- so
// interpolated after it, the ends left the pixel's line and the clouds smeared.
out vec4 vNearH;
out vec4 vFarH;
out float gl_ClipDistance[5];

void main()
{
    // Written, and kept: a portal pass enables five clip distances, and one a
    // shader leaves unwritten is undefined -- Mesa dropped the whole triangle,
    // and with it the clouds seen through a portal.
    for (int k = 0; k < 5; ++k) gl_ClipDistance[k] = 1.0;
    mat4 back = inverse(uProjection * uView);
    vNearH = back * vec4(aPos.xy, -1.0, 1.0);
    vFarH  = back * vec4(aPos.xy,  1.0, 1.0);
    gl_Position = vec4(aPos.xy, 1.0, 1.0);   // on the far plane: LEQUAL passes the cleared
}
)";

inline constexpr const char *kWaterSkyFS = R"(#version 330 core
in vec4 vNearH;
in vec4 vFarH;

out vec4 fragColor;
)" CNP_WATER_GLSL R"(
void main()
{
    vec3 vNear = vNearH.xyz / vNearH.w, vFar = vFarH.xyz / vFarH.w;
    fragColor = vec4(uWaterColor.rgb, water_murk(water_between(vNear, vFar)));
}
)";

// The clouds: a layer of cumulus between kBase and kTop, drawn with kWaterSkyVS's
// triangle (its vNear/vFar are the two ends of each pixel's line) and marched
// through in a few steps. Each column of the layer holds cloud from the flat
// bottom up to a height the noise gives it, which is what makes a flat belly
// and a rounded top. The depth written is the first point of cloud, so a hill
// in front hides it and, from above, it hides the ground below -- and the water
// drawn after it, which is why it is written at all.
inline constexpr const char *kCloudFS = R"(#version 330 core
in vec4 vNearH;
in vec4 vFarH;

uniform mat4  uProjection;
uniform mat4  uView;
uniform float uCloudDrift;   // how far the wind has carried them, in nodes
uniform float uDaylight;
uniform vec4  uSkyColor;
uniform vec3  uSunDir;     // the sun's, or by night the moon's
uniform float uSunShown;   // 0 under a fixed sky
uniform float uMoon;       // 1 when uSunDir is the moon's
uniform vec4  uStorm;      // core.world_storm: its colour, and w 1 when there is one
uniform vec4  uFunnel[4];  // core.storm_funnel: x, ground y, z, and w 1 when there is one
uniform float uRange;      // how far the world is drawn: a funnel past it would stand over nothing
uniform float uOvercast;   // core.overcast: 0..1, the layer thicker and grey

out vec4 fragColor;
)" CNP_WATER_GLSL R"(
const float kBase = 96.0;
const float kTop  = 112.0;
const float kFar  = 420.0;   // gone into the haze by here

float hash(ivec2 p)
{
    uint h = uint(p.x) * 1664525u ^ uint(p.y) * 1013904223u;
    h ^= h >> 16; h *= 0x7feb352du; h ^= h >> 15; h *= 0x846ca68bu; h ^= h >> 16;
    return float(h) * (1.0 / 4294967295.0);
}

float noise(vec2 p)
{
    ivec2 i = ivec2(floor(p));
    vec2  f = fract(p);
    vec2  u = f * f * (3.0 - 2.0 * f);
    return mix(mix(hash(i), hash(i + ivec2(1, 0)), u.x),
               mix(hash(i + ivec2(0, 1)), hash(i + ivec2(1, 1)), u.x), u.y);
}

// Under a storm, whirls: some cells of a 220-node grid hold one, its middle
// somewhere in the cell. Inside its reach the layer is turned whole about the
// middle, round and round with the wind's clock, and wound into two arms --
// blended into the layer outside, so the turning never winds it into mush.
// Some whirls are about to make a twister: a dark wall in the middle and,
// under it, a funnel that comes down towards the ground and draws back up.
const float kCell = 220.0;
const float kTip  = 44.0;    // the lowest a funnel's point comes down to

bool whirl_at(ivec2 c, out vec2 mid, out float reach, out float way)
{
    mid = vec2(0.0); reach = 1.0; way = 1.0;
    if (hash(c + ivec2(91, 7)) < 0.45) return false;
    mid   = (vec2(c) + vec2(hash(c), hash(c + ivec2(13, 57)))) * kCell;
    reach = 70.0 + 60.0 * hash(c + ivec2(5, 31));
    way   = hash(c + ivec2(77, 3)) < 0.5 ? -1.0 : 1.0;
    return true;
}

bool twister(ivec2 c) { return hash(c + ivec2(41, 29)) < 0.4; }

float fbm(vec2 xz)
{
    vec2  p = (xz + vec2(uCloudDrift, uCloudDrift * 0.35)) / 80.0;
    float s = 0.0, a = 0.5;
    for (int k = 0; k < 4; ++k) {
        s += a * noise(p);
        p  = p * 2.07 + vec2(13.7, 7.1);
        a *= 0.5;
    }
    return s;
}

// How tall the cloud standing on this column is, 0..1 of the layer.
float column(vec2 xz)
{
    float s = fbm(xz);
    if (uStorm.w > 0.5) {
        ivec2 cell = ivec2(floor(xz / kCell));
        for (int i = -1; i <= 1; ++i) {
            for (int j = -1; j <= 1; ++j) {
                ivec2 c = cell + ivec2(i, j);
                vec2  mid; float reach, way;
                if (!whirl_at(c, mid, reach, way)) continue;
                vec2  off  = xz - mid;
                float fall = exp(-dot(off, off) / (reach * reach));
                if (fall < 0.02) continue;
                // A turn about every half minute, the middle wound further.
                float turn = way * (uCloudDrift * 0.18 + 2.5 * fall);
                float sn = sin(turn), cs = cos(turn);
                float si = fbm(mid + vec2(cs * off.x - sn * off.y, sn * off.x + cs * off.y));
                float r    = length(off);
                float ang  = atan(off.y, off.x + 1e-4);
                float arms = 0.5 + 0.5 * cos(2.0 * (ang + turn) + way * 3.0 * log(r / reach + 0.05));
                si = si * (0.5 + 0.7 * arms) + 0.1 * fall;
                if (twister(c)) si += 0.3 * fall * fall;               // the wall a funnel hangs from
                else            si *= smoothstep(0.03, 0.14, r / reach);   // a clear eye
                s = mix(s, si, smoothstep(0.03, 0.4, fall));
            }
        }
    }
    return smoothstep(0.44 - 0.3 * uOvercast, 0.72 - 0.2 * uOvercast, s);
}

// A world's own twister, funnel k (uFunnel): from the clouds' belly down to
// the ground under it, narrow at the foot, its streaks turning fast, and a
// skirt of dust whirled up round the foot. How thick at p, 0..1.
float real_funnel(vec3 p, int k)
{
    vec4  f   = uFunnel[k];
    float tip = f.y;
    if (p.y < tip - 0.5) return 0.0;
    float u = clamp((p.y - tip) / (kBase - tip), 0.0, 1.0);
    // The top lags the foot, so it leans and sways.
    vec2  axis = f.xz + u * u * 4.0 * vec2(sin(uCloudDrift * 0.2 + float(k) * 1.7),
                                             cos(uCloudDrift * 0.17 + float(k) * 2.3));
    vec2  off = p.xz - axis;
    float r   = length(off);
    float ang = atan(off.y, off.x + 1e-4);
    float R   = 1.6 + 26.0 * pow(u, 2.2);
    float streak = 0.55 + 0.45 * sin(4.0 * ang + uCloudDrift * 3.0 - p.y * 0.35);
    float body   = smoothstep(R, R * 0.4, r) * streak;
    float h    = p.y - tip;
    float dR   = 6.0 + 2.0 * sin(ang * 3.0 + uCloudDrift * 4.0);
    float dust = smoothstep(dR, dR * 0.3, r) * smoothstep(7.0, 0.0, h)
               * (0.5 + 0.5 * sin(5.0 * ang - uCloudDrift * 5.0 + h));
    return max(body, dust * 0.8);
}

const int kReal = -999999;   // a candidate's cell .y for a world's own twister, .x its k

// Under the layer, a twister's funnel (cell c): how thick at p, 0..1.
float funnel(vec3 p, ivec2 c)
{
    if (c.y == kReal) return real_funnel(p, c.x);
    vec2  mid; float reach, way;
    whirl_at(c, mid, reach, way);
    // Coming down and drawing back up, every minute and a half or so.
    float tip = mix(kTip, kBase - 14.0,
                    0.5 + 0.5 * sin(uCloudDrift * 0.06 + 6.28 * hash(c + ivec2(17, 61))));
    if (p.y < tip) return 0.0;
    float u = (p.y - tip) / (kBase - tip);
    // Its axis swaying, the more the lower down.
    vec2  axis = mid + (1.0 - u) * 7.0 * vec2(sin(uCloudDrift * 0.15 + p.y * 0.08),
                                               cos(uCloudDrift * 0.12 + p.y * 0.07));
    vec2  off = p.xz - axis;
    float r   = length(off);
    float R   = 1.5 + 26.0 * u * u;
    if (r > R) return 0.0;
    float ang    = atan(off.y, off.x + 1e-4);
    float streak = 0.65 + 0.35 * sin(5.0 * ang + way * uCloudDrift * 2.0 - p.y * 0.4);
    return smoothstep(R, R * 0.45, r) * smoothstep(tip, tip + 5.0, p.y) * streak;
}

const float kFunnelR = 36.0;   // no funnel reaches further than this from its middle

// The sun or the moon behind this pixel, if either is: a disc with a soft
// edge, and round the sun a glow.
vec4 disc;

// Where no cloud is: the disc at the far plane, or nothing.
void sky_only()
{
    if (disc.a <= 0.0) discard;
    fragColor    = disc;
    gl_FragDepth = 0.99999;
}

// Where the line from o along d, 0..1 of it, is between heights y0 and y1.
vec2 slab(vec3 o, vec3 d, float y0, float y1)
{
    float ta = (y0 - o.y) / d.y;
    float tb = (y1 - o.y) / d.y;
    if (ta > tb) { float x = ta; ta = tb; tb = x; }
    return vec2(max(ta, 0.0), min(tb, 1.0));
}

float keep;   // light that gets through
vec3  eyeG;   // the eye, for the haze a funnel goes into
float hit;    // where the first cloud was met, -1 none
vec3  rgb;

// Through t.x..t.y of the line in n steps: the layer, or below it twister tw's funnel.
void march(vec3 o, vec3 d, vec2 t, int n, bool below, ivec2 tw)
{
    float len    = length(d);
    float dt     = (t.y - t.x) / float(n);
    // Steps that do not band: each pixel's start moved along by interleaved
    // gradient noise, whose neighbours differ evenly -- a hash per pixel
    // left the edges of the clouds in loose dots.
    float jitter = fract(52.9829189 * fract(dot(gl_FragCoord.xy, vec2(0.06711056, 0.00583715))));
    for (int i = 0; i < n; ++i) {
        float at = t.x + (float(i) + jitter) * dt;
        vec3  p  = o + d * at;
        float dens, lit;
        if (below) {
            // Gone into the haze where the ground and what stands on it are
            // no longer drawn: past there nothing would hide it.
            dens = funnel(p, tw) * (1.0 - smoothstep(0.75 * uRange, uRange, length(p.xz - eyeG.xz)));
            lit  = 0.15;
        } else {
            float h = (p.y - kBase) / (kTop - kBase);
            float c = column(p.xz);
            // Continuous in c, down to nothing: a step here put lone pixels of
            // cloud through an opening that the same view without it had as sky.
            dens = smoothstep(h - 0.16, h + 0.16, c) * smoothstep(0.0, 0.1, c);
            // Tops in the sun, bellies grey: how high in its own column the point is.
            lit  = clamp(0.35 + 0.65 * h / max(c, 0.05), 0.0, 1.0);
        }
        if (dens <= 0.0) continue;
        if (hit < 0.0) hit = at;
        float a = 1.0 - exp(-dens * dt * len * 1.2);
        vec3 low = vec3(0.58, 0.61, 0.68), top = vec3(1.0);
        if (uStorm.w > 0.5) {   // dark bellies of its colour, tops lit like embers
            low = uStorm.rgb * 0.35;
            top = min(uStorm.rgb * 1.25 + vec3(0.25, 0.12, 0.05), vec3(1.0));
        }
        low = mix(low, vec3(0.30, 0.32, 0.35), uOvercast);
        top = mix(top, vec3(0.58, 0.60, 0.63), uOvercast);
        rgb  += keep * a * mix(low, top, lit);
        keep *= 1.0 - a;
        if (keep < 0.03) return;
    }
}

// The candidates main() gathers: cell (or kReal and k), and the stretch of
// the line within kFunnelR of its middle, nearest first.
ivec2 tw[3];
vec2  twt[3];
int   ntw;
vec3  vNearG, dG;

void add_near(ivec2 c, vec2 mid, vec2 band, float dd)
{
    if (dd < 1e-6) return;
    vec2  o  = vNearG.xz - mid;
    float bq = dot(o, dG.xz) / dd;
    float cq = (dot(o, o) - kFunnelR * kFunnelR) / dd;
    float q  = bq * bq - cq;
    if (q <= 0.0) return;
    vec2 t = vec2(max(-bq - sqrt(q), band.x), min(-bq + sqrt(q), band.y));
    if (t.y <= t.x) return;
    int i = ntw < 3 ? ntw : 3;
    while (i > 0 && twt[i - 1].x > t.x) {
        if (i < 3) { tw[i] = tw[i - 1]; twt[i] = twt[i - 1]; }
        --i;
    }
    if (i < 3) { tw[i] = c; twt[i] = t; ntw = min(ntw + 1, 3); }
}

void main()
{
    // Along the eye's line and out to the haze, not to vFar: through a portal
    // the projection's near plane is oblique, which tilts its far plane down
    // onto the opening, and a line ended there never reached the layer.
    vec3 vNear = vNearH.xyz / vNearH.w;
    vec3 eye   = inverse(uView)[3].xyz;
    vec3 dir   = vNear - eye;

    disc = vec4(0.0);
    if (uSunShown > 0.5) {
        float c    = dot(normalize(dir), uSunDir);
        float edge = uMoon > 0.5 ? 0.99965 : 0.9992;
        float body = smoothstep(edge - 0.00004, edge + 0.00004, c);
        float glow = uMoon > 0.5 ? 0.12 * pow(max(c, 0.0), 2000.0) : 0.5 * pow(max(c, 0.0), 300.0);
        vec3  col  = uMoon > 0.5 ? vec3(0.86, 0.89, 1.0) : vec3(1.0, 0.96, 0.84);
        disc = vec4(col, clamp(body + glow, 0.0, 1.0));
    }

    vec3 d = dir * (kFar / max(length(dir.xz), 0.05 * length(dir)));
    if (abs(d.y) < 1e-5) { sky_only(); return; }
    keep = 1.0;
    hit  = -1.0;
    vNearG = vNear;
    dG     = d;
    eyeG   = eye;
    rgb  = vec3(0.0);
    vec2 lay  = slab(vNear, d, kBase, kTop);
    // The twisters the line passes near under the layer, and where: found once
    // here so the steps go only where a funnel can be. Three at most, nearest
    // first: the storm's own, and the world's (uFunnel), down to the ground.
    float low = kTip;
    bool  any = false;
    for (int k = 0; k < 4; ++k)
        if (uFunnel[k].w > 0.5) { low = min(low, uFunnel[k].y - 1.0); any = true; }
    vec2 band = uStorm.w > 0.5 || any ? slab(vNear, d, low, kBase) : vec2(1.0, 0.0);
    ntw = 0;
    if (band.y > band.x) {
        float dd = dot(d.xz, d.xz);
        if (uStorm.w > 0.5) {
            vec2  a = vNear.xz + d.xz * band.x, b = vNear.xz + d.xz * band.y;
            ivec2 lo = ivec2(floor((min(a, b) - kFunnelR) / kCell));
            ivec2 hi = ivec2(floor((max(a, b) + kFunnelR) / kCell));
            for (int i = lo.x; i <= hi.x && i <= lo.x + 4; ++i) {
                for (int j = lo.y; j <= hi.y && j <= lo.y + 4; ++j) {
                    ivec2 c = ivec2(i, j);
                    vec2  mid; float reach, way;
                    if (!whirl_at(c, mid, reach, way) || !twister(c)) continue;
                    add_near(c, mid, band, dd);
                }
            }
        }
        for (int k = 0; k < 4; ++k)
            if (uFunnel[k].w > 0.5) add_near(ivec2(k, kReal), uFunnel[k].xz, band, dd);
    }
    // Front to back: from under the layer the funnels come first.
    bool under = vNear.y < kBase;
    for (int k = 0; under && k < ntw && keep >= 0.03; ++k) march(vNear, d, twt[k], 20, true, tw[k]);
    if (lay.y > lay.x && keep >= 0.03) march(vNear, d, lay, 16, false, ivec2(0));
    for (int k = 0; !under && k < ntw && keep >= 0.03; ++k) march(vNear, d, twt[k], 20, true, tw[k]);
    if (hit < 0.0) { sky_only(); return; }

    float alpha = 1.0 - keep;
    rgb /= alpha;

    // From the eye and not from vNear: through a portal the near plane is
    // the opening's, and the haze would change as you step through.
    vec3  at   = vNear + d * hit;
    float dist = length(at.xz - eye.xz);
    float haze = smoothstep(80.0, kFar, dist);
    rgb   = rgb * (0.12 + 0.88 * uDaylight);
    rgb   = mix(rgb, uSkyColor.rgb, 0.08 + 0.7 * haze);   // the sky's hour tints them
    alpha *= 1.0 - smoothstep(kFar * 0.7, kFar, dist);
    if (alpha < 0.02) { sky_only(); return; }   // no depth for what cannot be seen
    rgb = water_shade(rgb, water_between(vNear, at));

    vec4 clip    = uProjection * uView * vec4(at, 1.0);
    gl_FragDepth = min(clip.z / clip.w * 0.5 + 0.5, 0.99999);
    // A thin cloud in front of the sun lets it through.
    float a2     = alpha + disc.a * (1.0 - alpha);
    fragColor    = vec4((rgb * alpha + disc.rgb * disc.a * (1.0 - alpha)) / a2, a2);
}
)";

// The portal opening. Its fragments sample the render target at their own
// screen position rather than through interpolated UVs, so the image inside the
// hole lines up with the world around it to the pixel no matter how the quad is
// angled. That exactness is what the continuity gate measures.
inline constexpr const char *kPortalVS = R"(#version 330 core
layout(location = 0) in vec3 aPos;

uniform mat4 uProjection;
uniform mat4 uView;
uniform mat4 uModel;
uniform vec4 uClipPlanes[5];
uniform int  uClipCount;

out float gl_ClipDistance[5];
out vec4  vClip;    // for the water in front of it
out vec3  vWorld;   // for the closed membrane's pattern

void main()
{
    vec4 world = uModel * vec4(aPos, 1.0);
    for (int k = 0; k < 5; ++k)
        gl_ClipDistance[k] = (k < uClipCount) ? dot(vec4(world.xyz, 1.0), uClipPlanes[k]) : 1.0;
    gl_Position = uProjection * uView * world;
    vClip       = gl_Position;
    vWorld      = world.xyz;
}
)";

inline constexpr const char *kPortalFS = R"(#version 330 core
in vec4 vClip;
in vec3 vWorld;

uniform sampler2D uPortalTex;
uniform vec2      uScreenSize;

// Which half of the hole's side wall this fragment is on. In FRONT of the
// portal plane it is wall, and its material is the wall's: sampling the render
// target there is what filled the bottom of a floor-level opening with clear
// colour, because those fragments project outside the destination's own
// opening, where the target has been clipped and holds nothing. Behind the
// portal plane there is no wall to draw -- that half of the node is already the
// other place -- so it samples like the back face and the hole reads as a hole
// rather than as a pipe.
uniform int  uTunnel;
uniform vec4 uTunnelColor;

// uTunnel 2: an opening that leads nowhere yet, plugged with a membrane in its
// colour, swirling, so it reads as a portal waiting for its other end rather
// than as a hole. Centre, right and up of the opening, and its half size.
uniform vec4 uClosedColor;
uniform vec4 uCentre;
uniform vec4 uRight;
uniform vec4 uUp;
uniform vec2 uHalf;
uniform float uTime;
// 0 closed; rising to 1 as the opening's other end arrives, a hole widening
// from the middle with a bright edge, the image showing through it.
uniform float uOpen;

out vec4 fragColor;
)" CNP_WATER_GLSL R"(
vec3 closed_membrane()
{
    vec3  d = vWorld - uCentre.xyz;
    vec2  p = vec2(dot(d, uRight.xyz) / uHalf.x, dot(d, uUp.xyz) / uHalf.y);
    float r = length(p);
    float a = atan(p.y, p.x);
    vec3  c = uClosedColor.rgb;

    float swirl  = 0.5 + 0.5 * sin(a * 3.0 + r * 9.0 - uTime * 2.5);
    float ripple = 0.5 + 0.5 * sin(r * 16.0 - uTime * 4.0);
    vec3  col    = mix(c * 0.2, c, mix(swirl, ripple, 0.35));
    col *= 0.85 + 0.15 * sin(uTime * 3.0);

    // A bright rim round the oval, and the corners outside it dark.
    float rim = smoothstep(0.78, 0.96, r) * (1.0 - smoothstep(0.96, 1.02, r));
    col = mix(col, mix(c, vec3(1.0), 0.55), rim);
    col = mix(col, c * 0.1, smoothstep(1.0, 1.06, r));

    // 1.5 reaches past the corners, so at 1 nothing of it is left.
    float hole = uOpen * 1.5;
    if (r < hole) discard;
    float edge = (1.0 - smoothstep(0.0, 0.15, r - hole)) * step(0.001, uOpen);
    return mix(col, mix(c, vec3(1.0), 0.7), edge);
}

void main()
{
    if (uTunnel == 2) {
        fragColor = vec4(water_fade(closed_membrane(), vClip), 1.0);
        return;
    }
    if (uTunnel != 0) {
        fragColor = vec4(water_fade(uTunnelColor.rgb, vClip), uTunnelColor.a);
        return;
    }
    // Normalising by the screen, not by the render target, is deliberate: the
    // target may be rendered at reduced resolution (DESIGN.md 2.5 scales it
    // with recursion depth) and this still samples the right texel.
    vec2 uv   = gl_FragCoord.xy / uScreenSize;
    fragColor = vec4(water_fade(texture(uPortalTex, uv).rgb, vClip), 1.0);
}
)";

// Flat colour. Used for the selection outline, which is drawn as the BACK
// faces of a slightly inflated cube: the front faces are culled, so what
// survives is a thin shell peeking out around the block underneath. Cheaper
// than line geometry and it needs no line-width support, which core profile
// does not really have.
inline constexpr const char *kSolidVS = R"(#version 330 core
layout(location = 0) in vec3 aPos;

uniform mat4 uProjection;
uniform mat4 uView;
uniform mat4 uModel;

void main()
{
    gl_Position = uProjection * uView * uModel * vec4(aPos, 1.0);
}
)";

inline constexpr const char *kSolidFS = R"(#version 330 core
uniform vec4 uColor;
out vec4 fragColor;
void main() { fragColor = uColor; }
)";

// A filled, shaded, clippable box: a body.
//
// The selection outline uses kSolid above and has no business growing clip
// planes it never needs. This one exists because a body halfway through a
// portal has to be CUT, and a cut is only visible on something filled -- a
// wireframe sliced in half still reads as a wireframe.
//
// The shade comes in per vertex rather than from a normal and a light, because
// the only thing it has to do is tell one face from another so the shape reads
// as a solid rather than as a silhouette.
inline constexpr const char *kBodyVS = R"(#version 330 core
layout(location = 0) in vec3  aPos;
layout(location = 1) in float aShade;

uniform mat4 uProjection;
uniform mat4 uView;
uniform mat4 uModel;
uniform vec4 uClipPlanes[5];
uniform int  uClipCount;

out float vShade;
out vec4  vClip;   // for the water in front of it
out float gl_ClipDistance[5];

void main()
{
    vec4 world = uModel * vec4(aPos, 1.0);
    for (int k = 0; k < 5; ++k)
        gl_ClipDistance[k] = (k < uClipCount) ? dot(vec4(world.xyz, 1.0), uClipPlanes[k]) : 1.0;
    vShade      = aShade;
    gl_Position = uProjection * uView * world;
    vClip       = gl_Position;
}
)";

inline constexpr const char *kBodyFS = R"(#version 330 core
in float vShade;
in vec4  vClip;
uniform vec4 uColor;
out vec4 fragColor;
)" CNP_WATER_GLSL R"(
void main() { fragColor = vec4(water_fade(uColor.rgb * vShade, vClip), uColor.a); }
)";


// The sun's shadow map: the world's own vertex, seen from the sun, depth only.
// The atlas is read for its alpha alone, so leaves cast the shadow of leaves
// and not of a cube.
inline constexpr const char *kShadowVS = R"(#version 330 core
layout(location = 0) in vec3  aPos;
layout(location = 1) in float aFaceAo;
layout(location = 2) in vec2  aUV;
layout(location = 4) in float aTex;

uniform mat4 uProjection;
uniform mat4 uView;
uniform mat4 uModel;

out vec2 vUV;
flat out float vTex;

void main()
{
    vUV  = aUV;
    vTex = aTex + 256.0 * float(int(aFaceAo + 0.5) >> 5);
    gl_Position = uProjection * uView * uModel * vec4(aPos, 1.0);
}
)";

inline constexpr const char *kShadowFS = R"(#version 330 core
in vec2 vUV;
flat in float vTex;

uniform sampler2D uTexture;
uniform float     uAtlasTiles;

void main()
{
    float layer = floor(vTex + 0.5);
    vec2  uv    = (vec2(mod(layer, uAtlasTiles), floor(layer / uAtlasTiles)) +
                   clamp(fract(vUV), 0.002, 0.998)) / uAtlasTiles;
    if (texture(uTexture, uv).a < 0.5) discard;
}
)";

// ------------------------------------------------------------------ a character
//
// Bone matrices arrive as THREE vec4 per joint, not as a mat4. GL 3.3 core
// guarantees 1024 uniform components in a vertex shader; 82 joints -- the
// larger of the two models this was written against -- want 1312 as mat4 and
// 984 this way. A bone matrix is affine and its projection row is a row of
// nothing. See core/media/gltf.h, kMaxJoints.
//
// Joint indices arrive as an UNSIGNED_SHORT that was NOT normalised, so the
// fetch hands them over as floats holding integer values -- the same trick the
// world vertex uses, and for the same reason: no glVertexAttribIPointer, which
// rlgl does not expose.
inline constexpr const char *kModelVS = R"(#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aUV;
layout(location = 3) in vec4 aJoints;    // integer values, as floats
layout(location = 4) in vec4 aWeights;

// ONE matrix and not three, and it is the difference between this linking and
// not on a conforming GL 3.3 driver. The floor is 1024 vertex uniform
// components: three mat4 cost 48 and leave room for 81 joints, one costs 16 and
// leaves room for 84. The multiply belongs on the CPU either way -- it is once
// per draw against once per vertex.
uniform mat4 uMVP;
uniform vec4 uBones[252];                // 84 joints x 3 rows, 1008 components

out vec2 vUV;
out vec3 vNormal;
out vec4 vClip;   // for the water in front of it

void main()
{
    vec4 p = vec4(aPos, 1.0);
    vec3 skinned = vec3(0.0);
    vec3 normal  = vec3(0.0);

    for (int i = 0; i < 4; ++i) {
        float w = aWeights[i];
        if (w <= 0.0) continue;
        int b = int(aJoints[i] + 0.5) * 3;
        vec4 r0 = uBones[b + 0];
        vec4 r1 = uBones[b + 1];
        vec4 r2 = uBones[b + 2];
        skinned += w * vec3(dot(r0, p), dot(r1, p), dot(r2, p));
        normal  += w * vec3(dot(r0.xyz, aNormal), dot(r1.xyz, aNormal), dot(r2.xyz, aNormal));
    }

    vUV     = aUV;
    vNormal = normal;
    gl_Position = uMVP * vec4(skinned, 1.0);
    vClip       = gl_Position;
}
)";

// One directional light and no shadow. A character has to read as a solid with
// a front and a side before it is worth lighting properly, and the sun this
// world has is a scalar the sky multiplies by rather than a direction.
inline constexpr const char *kModelFS = R"(#version 330 core
in vec2 vUV;
in vec3 vNormal;
in vec4 vClip;

uniform sampler2D uTexture;
uniform vec4 uColour;
uniform float uGlow;   // 1 for a material that gives its own light
uniform float uLight;  // how bright the cell it stands in is; 1 for a body
uniform float uAlpha;  // 1 but for something fading out: a creature that died

out vec4 fragColour;
)" CNP_WATER_GLSL R"(
void main()
{
    vec3 n = normalize(vNormal);
    float lit = 0.45 + 0.55 * max(dot(n, normalize(vec3(0.4, 0.9, 0.25))), 0.0);
    lit = mix(lit * uLight, 1.0, uGlow);
    // The texture's alpha cuts (a leaf, a rail); the material's sees through
    // (a vehicle's window, drawn after the rest of the model).
    vec4 tex = texture(uTexture, vUV);
    if (tex.a < 0.5) discard;
    fragColour = vec4(water_fade(tex.rgb * uColour.rgb * lit, vClip), uAlpha * uColour.a);
}
)";

} // namespace cnp::shaders
