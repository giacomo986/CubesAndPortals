// raylib/rlgl implementation of gfx.h.
//
// This is the only translation unit in the client allowed to include raylib.h.
// It deliberately drives rlgl rather than raylib's high-level drawing: we own
// the render graph (DESIGN.md 2.2), so there is no foreign Draw3D to hook into
// and no reason to let raylib decide when matrices get uploaded.

#include "client/gfx/gfx.h"

#include "core/diag/stack_dump.h"

#include <raylib.h>
#include <rlgl.h>

#include <cstdio>
#include <cstdlib>
#include <unordered_map>
#include <vector>

// GL core entry points raylib does not re-export. glEnable/glDisable have been
// in libGL since 1.0 and are part of the Linux OpenGL ABI, so declaring them is
// enough -- no loader, no GLAD, no second GL binding in the process
// (DESIGN.md 2.4). Anything newer than GL 1.2 must go through
// glfwGetProcAddress instead; the texture-array shim in M1 is the first case.
extern "C" {
void glEnable(unsigned int cap);
void glDisable(unsigned int cap);
void glFinish(void);
void glBindTexture(unsigned int target, unsigned int texture);
void glTexParameteri(unsigned int target, unsigned int pname, int param);
void glDrawBuffer(unsigned int mode);
void glReadBuffer(unsigned int mode);
}

#define GL_CLIP_DISTANCE0 0x3000
#define GL_UNSIGNED_SHORT 0x1403
#define GL_SHORT          0x1402
#define GL_TEXTURE_2D           0x0DE1
#define GL_TEXTURE_MIN_FILTER   0x2801
#define GL_TEXTURE_MAG_FILTER   0x2800
#define GL_TEXTURE_WRAP_S       0x2802
#define GL_TEXTURE_WRAP_T       0x2803
#define GL_LINEAR               0x2601
#define GL_CLAMP_TO_EDGE        0x812F
#define GL_TEXTURE_COMPARE_MODE 0x884C
#define GL_TEXTURE_COMPARE_FUNC 0x884D
#define GL_COMPARE_REF_TO_TEXTURE 0x884E
#define GL_LEQUAL               0x0203
#define GL_NONE                 0

namespace {

constexpr int kMaxClipPlanes = 5;

struct MeshEntry {
    unsigned int vao       = 0;
    unsigned int index_vbo = 0;
    bool         in_use    = false;
};

struct RtEntry {
    unsigned int fbo    = 0;
    unsigned int color  = 0;
    unsigned int depth  = 0;
    int          width  = 0;
    int          height = 0;
    bool         in_use = false;
};

// Uniform locations resolved once per shader. The names are a contract with
// shaders.h; a shader missing one simply gets -1 and the upload is skipped.
struct ShaderEntry {
    int loc_projection = -1;
    int loc_view       = -1;
    int loc_model      = -1;
    int loc_clip       = -1;
    int loc_clip_count = -1;
};

struct State {
    // Handle tables with free lists. Without the free list a destroyed handle
    // leaks its slot, and a renderer that recreates a chunk mesh whenever it
    // outgrows its buffer recreates them by the million over a long session --
    // which is what a ten-minute soak caught as a slow, steady climb in
    // resident memory.
    std::vector<MeshEntry> meshes;
    std::vector<uint32_t>  free_meshes;
    std::vector<RtEntry>   rts;
    std::vector<uint32_t>  free_rts;
    std::unordered_map<unsigned int, ShaderEntry> shaders;

    float projection[16] = {1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1};
    float view[16]       = {1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1};
    float model[16]      = {1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1};

    float clip_planes[kMaxClipPlanes * 4] = {};
    int   clip_count = 0;

    bool         present = true;
    unsigned int current_shader = 0;
    int          screen_w = 0;
    int          screen_h = 0;
    int          rt_depth = 0;    // >0 while a render target is bound
};

State g;

// raylib's Matrix names its fields by index (mN == row + 4*col, column major)
// but declares them in a different order than it stores them, and raymath
// reshuffles on upload. So a float[16] column-major array maps element-for-
// element onto mN, and this conversion has to be written out field by field:
// a memcpy compiles and silently transposes.
Matrix to_raylib(const float m[16])
{
    Matrix r;
    r.m0  = m[0];  r.m1  = m[1];  r.m2  = m[2];  r.m3  = m[3];
    r.m4  = m[4];  r.m5  = m[5];  r.m6  = m[6];  r.m7  = m[7];
    r.m8  = m[8];  r.m9  = m[9];  r.m10 = m[10]; r.m11 = m[11];
    r.m12 = m[12]; r.m13 = m[13]; r.m14 = m[14]; r.m15 = m[15];
    return r;
}

int gl_type_of(gfx_attr_type t)
{
    switch (t) {
        case GFX_ATTR_F32: return RL_FLOAT;
        case GFX_ATTR_U8:  return RL_UNSIGNED_BYTE;
        case GFX_ATTR_U16: return GL_UNSIGNED_SHORT;
        case GFX_ATTR_I16: return GL_SHORT;
    }
    return RL_FLOAT;
}

int rl_pixel_format(gfx_pixel_format f)
{
    return (f == GFX_PF_R8) ? RL_PIXELFORMAT_UNCOMPRESSED_GRAYSCALE
                            : RL_PIXELFORMAT_UNCOMPRESSED_R8G8B8A8;
}

ShaderEntry &shader_entry(unsigned int id)
{
    auto it = g.shaders.find(id);
    if (it != g.shaders.end()) return it->second;

    ShaderEntry e;
    e.loc_projection = rlGetLocationUniform(id, "uProjection");
    e.loc_view       = rlGetLocationUniform(id, "uView");
    e.loc_model      = rlGetLocationUniform(id, "uModel");
    e.loc_clip       = rlGetLocationUniform(id, "uClipPlanes");
    e.loc_clip_count = rlGetLocationUniform(id, "uClipCount");
    return g.shaders.emplace(id, e).first->second;
}

// Matrices and clip planes are pushed at draw time rather than when they are
// set. Our draws bypass rlgl's batch, so nothing else would ever upload them.
void flush_uniforms()
{
    if (g.current_shader == 0) return;
    const ShaderEntry &e = shader_entry(g.current_shader);

    if (e.loc_projection >= 0) rlSetUniformMatrix(e.loc_projection, to_raylib(g.projection));
    if (e.loc_view >= 0)       rlSetUniformMatrix(e.loc_view, to_raylib(g.view));
    if (e.loc_model >= 0)      rlSetUniformMatrix(e.loc_model, to_raylib(g.model));

    if (e.loc_clip >= 0 && g.clip_count > 0)
        rlSetUniform(e.loc_clip, g.clip_planes, RL_SHADER_UNIFORM_VEC4, g.clip_count);
    if (e.loc_clip_count >= 0)
        rlSetUniform(e.loc_clip_count, &g.clip_count, RL_SHADER_UNIFORM_INT, 1);
}

} // namespace

// ---------------------------------------------------------------- lifecycle

bool gfx_init(const gfx_desc *desc)
{
    // ALWAYS_RUN, and it is not a preference. Without it raylib parks the frame
    // loop in glfwWaitEvents() the moment the window is minimised:
    //
    //     if (eventWaiting || (MINIMIZED && !ALWAYS_RUN)) glfwWaitEvents();
    //
    // glfwWaitEvents BLOCKS until an input event arrives, and a window nobody
    // is looking at gets none -- so the process stops dead, holding open
    // network sessions it no longer services. That is TODO.md OPEN-6: a hang
    // about once in fifty runs, more often on a busy desktop, that vanished
    // under every debugger pointed at it. It was caught by the watchdog with
    // _glfwWaitEventsX11 on the stack.
    //
    // A game that keeps rendering while minimised is a choice; a client that
    // keeps ANSWERING while minimised is not optional here, because the world
    // on the other end is somebody else's server and a session that stops
    // being serviced is a session that gets dropped.
    unsigned int flags = FLAG_WINDOW_ALWAYS_RUN;
    if (desc->vsync)   flags |= FLAG_VSYNC_HINT;
    if (desc->msaa > 0) flags |= FLAG_MSAA_4X_HINT;
    if (desc->resizable) flags |= FLAG_WINDOW_RESIZABLE;
    SetConfigFlags(flags);

    InitWindow(desc->width, desc->height, desc->title ? desc->title : "cubes and portals");
    if (!IsWindowReady()) return false;

    // A window the size of the screen does not fit ON the screen: the title bar
    // and the desktop's own panel are somewhere, and asking for exactly the
    // monitor's resolution puts the bottom of the game behind the taskbar with
    // no way to drag it out. Measured on a 1920x1080 desktop, which is exactly
    // the size this asks for by default.
    //
    // Only for a window somebody can resize. A measurement asks for the shape
    // it intends to measure and gets it, whatever monitor it is on, or the
    // numbers stop being comparable between machines.
    if (desc->resizable) {
        const int monitor = GetCurrentMonitor();
        const int mw      = GetMonitorWidth(monitor);
        const int mh      = GetMonitorHeight(monitor);

        // Nine tenths, which clears a panel on any edge and still fills the
        // screen. Centred afterwards, because a window that fits and is jammed
        // into a corner has only solved half of it.
        if (mw > 0 && mh > 0) {
            const int w = desc->width  < mw * 9 / 10 ? desc->width  : mw * 9 / 10;
            const int h = desc->height < mh * 9 / 10 ? desc->height : mh * 9 / 10;

            if (w != desc->width || h != desc->height) {
                SetWindowSize(w, h);
                SetWindowPosition((mw - w) / 2, (mh - h) / 2);
            }
        }
    }

    // No SetTargetFPS: exit criterion 3 is a frame-rate measurement, and a
    // frame limiter would make it meaningless.
    g.screen_w = GetScreenWidth();
    g.screen_h = GetScreenHeight();

    g.meshes.reserve(64);
    g.rts.reserve(16);
    return true;
}

void gfx_shutdown(void)
{
    CloseWindow();
}

void gfx_begin_frame(void)
{
    // The window may have been dragged since the last frame. Read once, here,
    // so everything downstream in this frame agrees about how big the screen
    // is -- asking raylib again halfway through a frame is how a projection and
    // a viewport come to be built from two different sizes.
    g.screen_w = GetScreenWidth();
    g.screen_h = GetScreenHeight();

    // One store, once a frame: the watchdog's definition of "still moving".
    // Here rather than in each of the twelve run_* loops, because a loop that
    // forgot to say it was alive would be a watchdog firing on a healthy
    // process, which is worse than no watchdog.
    cnp::watchdog_beat();

    BeginDrawing();
    g.screen_w = GetScreenWidth();
    g.screen_h = GetScreenHeight();
}

void gfx_end_frame(void)
{
    if (g.present) {
        EndDrawing();
        return;
    }
    // Everything EndDrawing does except handing the image to the compositor.
    // Input still has to be pumped or the window stops responding.
    rlDrawRenderBatchActive();
    PollInputEvents();
}

void gfx_set_present(bool enabled) { g.present = enabled; }
void gfx_open_url(const char *url) { OpenURL(url); }

void gfx_finish(void) { glFinish(); }

void gfx_viewport(int x, int y, int w, int h)
{
    rlViewport(x, y, w, h);
}

void gfx_scissor(int x, int y, int w, int h)
{
    rlDrawRenderBatchActive();
    rlEnableScissorTest();
    rlScissor(x, y, w, h);
}

void gfx_scissor_off(void)
{
    rlDrawRenderBatchActive();
    rlDisableScissorTest();
}

void gfx_clear(float r, float gg, float b, float a, bool depth)
{
    // rlgl only offers a combined colour+depth clear, so `depth` is accepted
    // for the interface's sake and ignored here. A backend with finer control
    // (sokol, raw GL) can honour it without changing any caller.
    (void)depth;

    // Depth writes ON first, because a clear obeys the depth mask: with writes
    // disabled, glClear leaves the depth buffer exactly as it was and says
    // nothing. The HUD pass turns depth off for its text and leaves it off at
    // the end of a frame, so the next frame's clear kept the PREVIOUS frame's
    // depth -- geometry failing the test against a scene that is no longer
    // there. It stayed invisible because the portal pass ran first and set the
    // default state on its way in; the frame that skipped that pass (a one-way
    // portal seen from behind) is what finally showed it, as a torn ground and
    // a world half missing. See docs/archive/M5_FINDINGS.md 13.6.
    //
    // Leaving writes enabled afterwards is the honest default: it is what
    // gfx_state_default says, and a caller wanting them off sets its own state
    // after the clear, which is the order everything here already uses.
    rlEnableDepthMask();

    rlClearColor((unsigned char)(r * 255.0f), (unsigned char)(gg * 255.0f),
                 (unsigned char)(b * 255.0f), (unsigned char)(a * 255.0f));
    rlClearScreenBuffers();
}

int gfx_screen_width(void)  { return g.screen_w; }
int gfx_screen_height(void) { return g.screen_h; }

unsigned char *gfx_read_screen(int *out_w, int *out_h)
{
    // Flush first, or the read races whatever is still queued.
    rlDrawRenderBatchActive();
    if (out_w) *out_w = g.screen_w;
    if (out_h) *out_h = g.screen_h;
    return rlReadScreenPixels(g.screen_w, g.screen_h);
}

unsigned char *gfx_read_target(gfx_rt rt, int *out_w, int *out_h)
{
    if (rt.id == 0 || rt.id > g.rts.size()) return nullptr;
    const RtEntry &e = g.rts[rt.id - 1];

    rlDrawRenderBatchActive();
    rlEnableFramebuffer(e.fbo);
    unsigned char *px = rlReadScreenPixels(e.width, e.height);
    rlDisableFramebuffer();
    rlViewport(0, 0, g.screen_w, g.screen_h);

    if (out_w) *out_w = e.width;
    if (out_h) *out_h = e.height;
    return px;
}

void gfx_free_pixels(unsigned char *pixels)
{
    // rlReadScreenPixels hands back a malloc'd buffer.
    std::free(pixels);
}

// ------------------------------------------------------------------ buffers

gfx_buffer gfx_buffer_create(const void *data, int size_bytes, bool dynamic, bool index)
{
    gfx_buffer b;
    b.id = index ? rlLoadVertexBufferElement(data, size_bytes, dynamic)
                 : rlLoadVertexBuffer(data, size_bytes, dynamic);
    return b;
}

void gfx_buffer_update(gfx_buffer buf, const void *data, int size_bytes, int offset_bytes)
{
    rlUpdateVertexBuffer(buf.id, data, size_bytes, offset_bytes);
}

void gfx_buffer_destroy(gfx_buffer buf)
{
    if (buf.id) rlUnloadVertexBuffer(buf.id);
}

gfx_mesh gfx_mesh_create(const gfx_vertexlayout *layout, gfx_buffer vertex, gfx_buffer index)
{
    gfx_mesh m{0};

    const unsigned int vao = rlLoadVertexArray();
    if (!rlEnableVertexArray(vao)) {
        // No VAO support: the spike targets GL 3.3 core, where VAOs are
        // mandatory, so this is a hard failure rather than a fallback.
        TraceLog(LOG_ERROR, "gfx: VAOs unavailable, GL 3.3 core required");
        return m;
    }

    rlEnableVertexBuffer(vertex.id);
    for (int i = 0; i < layout->attr_count; ++i) {
        const gfx_attr &a = layout->attrs[i];
        rlSetVertexAttribute((unsigned int)a.location, a.count, gl_type_of(a.type),
                             a.normalized, layout->stride, a.offset);
        rlEnableVertexAttribute((unsigned int)a.location);
    }
    if (index.id) rlEnableVertexBufferElement(index.id);
    rlDisableVertexArray();

    MeshEntry e;
    e.vao       = vao;
    e.index_vbo = index.id;
    e.in_use    = true;

    if (!g.free_meshes.empty()) {
        m.id = g.free_meshes.back();
        g.free_meshes.pop_back();
        g.meshes[m.id - 1] = e;
    } else {
        g.meshes.push_back(e);
        m.id = (uint32_t)g.meshes.size();   // handle is 1-based; 0 stays invalid
    }
    return m;
}

void gfx_mesh_destroy(gfx_mesh mesh)
{
    if (mesh.id == 0 || mesh.id > g.meshes.size()) return;
    MeshEntry &e = g.meshes[mesh.id - 1];
    if (!e.in_use) return;
    rlUnloadVertexArray(e.vao);
    e.in_use = false;
    g.free_meshes.push_back(mesh.id);
}

// ----------------------------------------------------------------- textures

unsigned char *gfx_image_decode_rgba(const void *bytes, int size, int *w, int *h)
{
    if (bytes == nullptr || size <= 0) return nullptr;

    // The extension tells raylib which decoder to use; the bytes are the file.
    Image img = LoadImageFromMemory(".png", (const unsigned char *)bytes, size);
    if (img.data == nullptr || img.width <= 0 || img.height <= 0) return nullptr;

    // Whatever it was, it comes out RGBA8: the atlas has one layout and a
    // caller should not have to branch on what a file happened to be saved as.
    if (img.format != PIXELFORMAT_UNCOMPRESSED_R8G8B8A8)
        ImageFormat(&img, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);

    if (w) *w = img.width;
    if (h) *h = img.height;
    return (unsigned char *)img.data;   // freed by gfx_image_free
}

void gfx_image_free(unsigned char *pixels)
{
    if (pixels) MemFree(pixels);
}

gfx_texture gfx_texture_create(const void *pixels, int w, int h, gfx_pixel_format fmt,
                               gfx_filter filter)
{
    gfx_texture t;
    // One level uploaded; the mip chain, when asked for, is generated below.
    t.id = rlLoadTexture(pixels, w, h, rl_pixel_format(fmt), 1);
    if (!t.id) return t;

    if (filter == GFX_FILTER_NEAREST_MIP) {
        int levels = 1;
        rlGenTextureMipmaps(t.id, w, h, rl_pixel_format(fmt), &levels);
        rlTextureParameters(t.id, RL_TEXTURE_MIN_FILTER, RL_TEXTURE_FILTER_NEAREST_MIP_LINEAR);
        rlTextureParameters(t.id, RL_TEXTURE_MAG_FILTER, RL_TEXTURE_FILTER_NEAREST);
    } else {
        const int f = (filter == GFX_FILTER_LINEAR) ? RL_TEXTURE_FILTER_LINEAR
                                                    : RL_TEXTURE_FILTER_NEAREST;
        rlTextureParameters(t.id, RL_TEXTURE_MIN_FILTER, f);
        rlTextureParameters(t.id, RL_TEXTURE_MAG_FILTER, f);
    }
    return t;
}

void gfx_texture_update(gfx_texture tex, const void *pixels, int x, int y, int w, int h,
                        gfx_pixel_format fmt)
{
    rlUpdateTexture(tex.id, x, y, w, h, rl_pixel_format(fmt), pixels);
}

void gfx_texture_destroy(gfx_texture tex)
{
    if (tex.id) rlUnloadTexture(tex.id);
}

void gfx_texture_bind(gfx_texture tex, int slot)
{
    rlActiveTextureSlot(slot);
    rlEnableTexture(tex.id);
}

gfx_texture gfx_texture_array_create(const void *pixels, int w, int h, int layers,
                                     gfx_pixel_format fmt, gfx_filter filter)
{
    // GL_TEXTURE_2D_ARRAY needs glTexImage3D, which is GL 1.2+ and therefore
    // must come through glfwGetProcAddress rather than a bare declaration.
    // M1 owns that shim; M0 has one texture and does not need it.
    (void)pixels; (void)w; (void)h; (void)layers; (void)fmt; (void)filter;
    TraceLog(LOG_WARNING, "gfx: texture arrays not implemented before M1");
    gfx_texture t{0};
    return t;
}

// ------------------------------------------------------------------ shaders

gfx_shader gfx_shader_create(const char *vs_source, const char *fs_source)
{
    gfx_shader s;
    s.id = rlLoadShaderProgram(vs_source, fs_source);
    return s;
}

void gfx_shader_destroy(gfx_shader shader)
{
    if (!shader.id) return;
    g.shaders.erase(shader.id);
    rlUnloadShaderProgram(shader.id);
}

void gfx_shader_bind(gfx_shader shader)
{
    g.current_shader = shader.id;
    rlEnableShader(shader.id);
}

void gfx_shader_bind_default(void)
{
    // Flush first: whatever is queued was recorded expecting the shader that is
    // current right now, not the default one.
    rlDrawRenderBatchActive();
    rlSetShader(rlGetShaderIdDefault(), rlGetShaderLocsDefault());
    g.current_shader = 0;
}

// ----------------------------------------------------------------------- 2D
//
// Driven through rlgl like everything else in this file, and not through
// raylib's DrawRectangle/DrawTexturePro: those set up their own matrices, and
// the whole point of gfx_2d_begin is that ONE ortho is installed and everything
// after it lands in the same space -- including anything drawn into a render
// target, which is where the gate reads it back from.
//
// Text is the exception and it is a deliberate one: raylib's font atlas, its
// glyph metrics and its codepoint handling are a thousand lines nobody here
// wants to own, and DrawTextEx feeds the same rlgl batch, so it lands in our
// ortho like everything else.

namespace {

// Everything gfx_2d_begin turned off, so gfx_2d_end can put it back rather than
// leaving the pipeline in a state the next 3D pass has to guess at.
struct Gfx2dState {
    bool active   = false;
    int  width    = 0;
    int  height   = 0;
    bool scissored = false;
};

Gfx2dState g_2d;

} // namespace

void gfx_2d_begin(int width, int height)
{
    if (g_2d.active) return;

    // The batch is flushed and the default shader put back for the same reason
    // gfx_shader_bind_default exists: one of ours left bound feeds our vertex
    // layout to rlgl's shader, and the result is garbage rather than an error.
    gfx_shader_bind_default();

    rlMatrixMode(RL_PROJECTION);
    rlPushMatrix();
    rlLoadIdentity();
    // Top-left origin: near-plane last two arguments are rlgl's, and the flip
    // is the second and third -- 0 at the top, `height` at the bottom.
    rlOrtho(0.0, (double)width, (double)height, 0.0, -1.0, 1.0);

    rlMatrixMode(RL_MODELVIEW);
    rlPushMatrix();
    rlLoadIdentity();

    rlDisableDepthTest();
    rlEnableColorBlend();
    rlSetBlendMode(RL_BLEND_ALPHA);

    g_2d.active    = true;
    g_2d.width     = width;
    g_2d.height    = height;
    g_2d.scissored = false;
}

void gfx_2d_end(void)
{
    if (!g_2d.active) return;

    rlDrawRenderBatchActive();

    if (g_2d.scissored) {
        rlDisableScissorTest();
        g_2d.scissored = false;
    }

    rlMatrixMode(RL_MODELVIEW);
    rlPopMatrix();
    rlMatrixMode(RL_PROJECTION);
    rlPopMatrix();
    rlMatrixMode(RL_MODELVIEW);

    rlEnableDepthTest();
    g_2d.active = false;
}

void gfx_2d_rect(float x, float y, float w, float h, const float rgba[4])
{
    if (!g_2d.active || w <= 0.0f || h <= 0.0f) return;

    // rlgl's untextured path is a white 1x1 texture, not a separate shader:
    // binding it is what makes a coloured quad come out the colour it was
    // given rather than whatever was bound last.
    rlSetTexture(rlGetTextureIdDefault());
    rlBegin(RL_QUADS);
    rlColor4f(rgba[0], rgba[1], rgba[2], rgba[3]);
    rlNormal3f(0.0f, 0.0f, 1.0f);
    rlTexCoord2f(0.0f, 0.0f); rlVertex2f(x,     y);
    rlTexCoord2f(0.0f, 1.0f); rlVertex2f(x,     y + h);
    rlTexCoord2f(1.0f, 1.0f); rlVertex2f(x + w, y + h);
    rlTexCoord2f(1.0f, 0.0f); rlVertex2f(x + w, y);
    rlEnd();
    rlSetTexture(0);
}

void gfx_2d_image(gfx_texture tex, float u, float v, float uw, float vh,
                  float x, float y, float w, float h, const float rgba[4])
{
    if (!g_2d.active || tex.id == 0 || w <= 0.0f || h <= 0.0f) return;

    const float u0 = u, v0 = v;
    const float u1 = u + uw, v1 = v + vh;

    rlSetTexture(tex.id);
    rlBegin(RL_QUADS);
    rlColor4f(rgba[0], rgba[1], rgba[2], rgba[3]);
    rlNormal3f(0.0f, 0.0f, 1.0f);
    rlTexCoord2f(u0, v0); rlVertex2f(x,     y);
    rlTexCoord2f(u0, v1); rlVertex2f(x,     y + h);
    rlTexCoord2f(u1, v1); rlVertex2f(x + w, y + h);
    rlTexCoord2f(u1, v0); rlVertex2f(x + w, y);
    rlEnd();
    rlSetTexture(0);
}

void gfx_2d_scissor(float x, float y, float w, float h)
{
    if (!g_2d.active) return;

    rlDrawRenderBatchActive();   // what is queued belongs to the old clip

    if (w <= 0.0f || h <= 0.0f) {
        if (g_2d.scissored) rlDisableScissorTest();
        g_2d.scissored = false;
        return;
    }

    // GL's scissor box is bottom-up in the CURRENT framebuffer, so the flip is
    // against the height this pass was begun with rather than the window's --
    // inside a render target those are not the same number.
    const int gy = g_2d.height - (int)(y + h);
    rlEnableScissorTest();
    rlScissor((int)x, gy, (int)w, (int)h);
    g_2d.scissored = true;
}

void gfx_2d_circle(float cx, float cy, float radius, const float rgba[4])
{
    if (!g_2d.active || radius <= 0.0f) return;
    const Color c{(unsigned char)(rgba[0] * 255.0f), (unsigned char)(rgba[1] * 255.0f),
                  (unsigned char)(rgba[2] * 255.0f), (unsigned char)(rgba[3] * 255.0f)};
    DrawCircleV(Vector2{cx, cy}, radius, c);
}

float gfx_2d_text(const char *text, float x, float y, float size, const float rgba[4])
{
    if (!g_2d.active || text == nullptr) return 0.0f;

    const Font  font = GetFontDefault();
    const Color c{(unsigned char)(rgba[0] * 255.0f), (unsigned char)(rgba[1] * 255.0f),
                  (unsigned char)(rgba[2] * 255.0f), (unsigned char)(rgba[3] * 255.0f)};

    // Spacing scaled with the size, which is what raylib's own DrawText does;
    // passing zero gives letters that touch at any size above the font's own.
    const float spacing = size / 10.0f;
    DrawTextEx(font, text, Vector2{x, y}, size, spacing, c);
    return MeasureTextEx(font, text, size, spacing).x;
}

float gfx_2d_text_width(const char *text, float size)
{
    if (text == nullptr) return 0.0f;
    return MeasureTextEx(GetFontDefault(), text, size, size / 10.0f).x;
}

int gfx_uniform_location(gfx_shader shader, const char *name)
{
    return rlGetLocationUniform(shader.id, name);
}

void gfx_uniform_set_f(int loc, float v)
{
    if (loc >= 0) rlSetUniform(loc, &v, RL_SHADER_UNIFORM_FLOAT, 1);
}

void gfx_uniform_set_v2(int loc, const float v[2])
{
    if (loc >= 0) rlSetUniform(loc, v, RL_SHADER_UNIFORM_VEC2, 1);
}

void gfx_uniform_set_v3(int loc, const float v[3])
{
    if (loc >= 0) rlSetUniform(loc, v, RL_SHADER_UNIFORM_VEC3, 1);
}

void gfx_uniform_set_v4(int loc, const float v[4], int count)
{
    if (loc >= 0) rlSetUniform(loc, v, RL_SHADER_UNIFORM_VEC4, count);
}

void gfx_uniform_set_mat4(int loc, const float m[16])
{
    if (loc >= 0) rlSetUniformMatrix(loc, to_raylib(m));
}

void gfx_uniform_set_int(int loc, int v)
{
    if (loc >= 0) rlSetUniform(loc, &v, RL_SHADER_UNIFORM_INT, 1);
}

// ----------------------------------------------------------------- matrices

void gfx_projection_set(const float m[16]) { for (int i = 0; i < 16; ++i) g.projection[i] = m[i]; }
void gfx_view_set(const float m[16])       { for (int i = 0; i < 16; ++i) g.view[i] = m[i]; }
void gfx_model_set(const float m[16])      { for (int i = 0; i < 16; ++i) g.model[i] = m[i]; }

// -------------------------------------------------------------- clip planes

void gfx_clip_planes_set(const float *planes_xyzw, int count)
{
    if (count > kMaxClipPlanes) count = kMaxClipPlanes;
    if (count < 0) count = 0;

    for (int i = 0; i < count * 4; ++i) g.clip_planes[i] = planes_xyzw[i];

    // Enable exactly the distances in use and disable the rest every time.
    // Leaving a stale GL_CLIP_DISTANCE enabled clips the next pass against a
    // plane it never asked for, which looks like geometry randomly missing.
    for (int i = 0; i < kMaxClipPlanes; ++i) {
        if (i < count) glEnable(GL_CLIP_DISTANCE0 + i);
        else           glDisable(GL_CLIP_DISTANCE0 + i);
    }
    g.clip_count = count;
}

// ------------------------------------------------------------ render targets

gfx_rt gfx_rt_create(int w, int h, bool with_depth)
{
    gfx_rt rt{0};

    RtEntry e;
    e.width  = w;
    e.height = h;
    e.fbo    = rlLoadFramebuffer();
    if (!e.fbo) return rt;

    rlEnableFramebuffer(e.fbo);

    e.color = rlLoadTexture(nullptr, w, h, RL_PIXELFORMAT_UNCOMPRESSED_R8G8B8A8, 1);
    rlTextureParameters(e.color, RL_TEXTURE_MIN_FILTER, RL_TEXTURE_FILTER_LINEAR);
    rlTextureParameters(e.color, RL_TEXTURE_MAG_FILTER, RL_TEXTURE_FILTER_LINEAR);
    rlFramebufferAttach(e.fbo, e.color, RL_ATTACHMENT_COLOR_CHANNEL0, RL_ATTACHMENT_TEXTURE2D, 0);

    if (with_depth) {
        // A renderbuffer, not a texture: nothing samples portal depth yet, and
        // renderbuffers are the cheaper attachment.
        e.depth = rlLoadTextureDepth(w, h, true);
        rlFramebufferAttach(e.fbo, e.depth, RL_ATTACHMENT_DEPTH, RL_ATTACHMENT_RENDERBUFFER, 0);
    }

    const bool complete = rlFramebufferComplete(e.fbo);
    rlDisableFramebuffer();
    if (!complete) {
        rlUnloadFramebuffer(e.fbo);
        return rt;
    }

    e.in_use = true;
    if (!g.free_rts.empty()) {
        rt.id = g.free_rts.back();
        g.free_rts.pop_back();
        g.rts[rt.id - 1] = e;
    } else {
        g.rts.push_back(e);
        rt.id = (uint32_t)g.rts.size();
    }
    return rt;
}

gfx_rt gfx_shadow_create(int size)
{
    gfx_rt rt{0};

    RtEntry e;
    e.width  = size;
    e.height = size;
    e.fbo    = rlLoadFramebuffer();
    if (!e.fbo) return rt;

    rlEnableFramebuffer(e.fbo);
    e.depth = rlLoadTextureDepth(size, size, false);
    glBindTexture(GL_TEXTURE_2D, e.depth);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);
    glBindTexture(GL_TEXTURE_2D, 0);
    rlFramebufferAttach(e.fbo, e.depth, RL_ATTACHMENT_DEPTH, RL_ATTACHMENT_TEXTURE2D, 0);

    // No colour attachment, and GL 3.3 calls a framebuffer whose draw buffer
    // names a missing attachment incomplete. Bound again first: the attach
    // above leaves framebuffer 0 bound, and the window's draw buffer set to
    // none is a black window.
    rlEnableFramebuffer(e.fbo);
    glDrawBuffer(GL_NONE);
    glReadBuffer(GL_NONE);

    const bool complete = rlFramebufferComplete(e.fbo);
    rlDisableFramebuffer();
    if (!complete) {
        rlUnloadFramebuffer(e.fbo);
        return rt;
    }

    e.in_use = true;
    if (!g.free_rts.empty()) {
        rt.id = g.free_rts.back();
        g.free_rts.pop_back();
        g.rts[rt.id - 1] = e;
    } else {
        g.rts.push_back(e);
        rt.id = (uint32_t)g.rts.size();
    }
    return rt;
}

gfx_texture gfx_rt_depth(gfx_rt rt)
{
    gfx_texture t{0};
    if (rt.id == 0 || rt.id > g.rts.size()) return t;
    t.id = g.rts[rt.id - 1].depth;
    return t;
}

void gfx_rt_destroy(gfx_rt rt)
{
    if (rt.id == 0 || rt.id > g.rts.size()) return;
    RtEntry &e = g.rts[rt.id - 1];
    if (!e.in_use) return;
    rlUnloadFramebuffer(e.fbo);   // also drops its attachments
    e.in_use = false;
    g.free_rts.push_back(rt.id);
}

void gfx_rt_begin(gfx_rt rt)
{
    if (rt.id == 0 || rt.id > g.rts.size()) return;
    const RtEntry &e = g.rts[rt.id - 1];

    // Flush anything rlgl still has pending so it lands on the previous target
    // rather than this one.
    rlDrawRenderBatchActive();
    rlEnableFramebuffer(e.fbo);
    rlViewport(0, 0, e.width, e.height);
    g.rt_depth++;
}

void gfx_rt_end(void)
{
    rlDrawRenderBatchActive();
    rlDisableFramebuffer();
    rlViewport(0, 0, g.screen_w, g.screen_h);
    if (g.rt_depth > 0) g.rt_depth--;
}

gfx_texture gfx_rt_texture(gfx_rt rt)
{
    gfx_texture t{0};
    if (rt.id == 0 || rt.id > g.rts.size()) return t;
    t.id = g.rts[rt.id - 1].color;
    return t;
}

int gfx_rt_width(gfx_rt rt)
{
    return (rt.id && rt.id <= g.rts.size()) ? g.rts[rt.id - 1].width : 0;
}

int gfx_rt_height(gfx_rt rt)
{
    return (rt.id && rt.id <= g.rts.size()) ? g.rts[rt.id - 1].height : 0;
}

// -------------------------------------------------------------------- state

gfx_state gfx_state_default(void)
{
    gfx_state s;
    s.depth_test  = true;
    s.depth_write = true;
    s.cull        = GFX_CULL_BACK;
    s.blend       = GFX_BLEND_NONE;
    s.color_mask[0] = s.color_mask[1] = s.color_mask[2] = s.color_mask[3] = true;
    return s;
}

void gfx_state_set(const gfx_state *state)
{
    if (state->depth_test) rlEnableDepthTest(); else rlDisableDepthTest();
    if (state->depth_write) rlEnableDepthMask(); else rlDisableDepthMask();

    if (state->cull == GFX_CULL_NONE) {
        rlDisableBackfaceCulling();
    } else {
        rlEnableBackfaceCulling();
        rlSetCullFace(state->cull == GFX_CULL_BACK ? RL_CULL_FACE_BACK : RL_CULL_FACE_FRONT);
    }

    if (state->blend == GFX_BLEND_NONE) {
        rlDisableColorBlend();
    } else {
        rlEnableColorBlend();
        rlSetBlendMode(state->blend == GFX_BLEND_ADD ? RL_BLEND_ADDITIVE : RL_BLEND_ALPHA);
    }

    rlColorMask(state->color_mask[0], state->color_mask[1],
                state->color_mask[2], state->color_mask[3]);
}

// --------------------------------------------------------------------- draw

void gfx_draw(gfx_mesh mesh, int count, int offset)
{
    if (mesh.id == 0 || mesh.id > g.meshes.size()) return;
    const MeshEntry &e = g.meshes[mesh.id - 1];
    if (!e.in_use) return;

    flush_uniforms();
    rlEnableVertexArray(e.vao);
    if (e.index_vbo) rlDrawVertexArrayElements(offset, count, nullptr);
    else             rlDrawVertexArray(offset, count);
    rlDisableVertexArray();
}

void gfx_draw_instanced(gfx_mesh mesh, int count, int offset, int instances)
{
    if (mesh.id == 0 || mesh.id > g.meshes.size()) return;
    const MeshEntry &e = g.meshes[mesh.id - 1];
    if (!e.in_use) return;

    flush_uniforms();
    rlEnableVertexArray(e.vao);
    if (e.index_vbo) rlDrawVertexArrayElementsInstanced(offset, count, nullptr, instances);
    else             rlDrawVertexArrayInstanced(offset, count, instances);
    rlDisableVertexArray();
}
