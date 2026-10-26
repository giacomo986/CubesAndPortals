#pragma once

#include <stdbool.h>
#include <stdint.h>

// The only door between the client and the graphics platform.
//
// Nothing above this header includes raylib.h. That rule is the whole point:
// DESIGN.md 7 budgets a move to sokol_gfx at "days, not months", and the price
// of that option is keeping this surface small and free of raylib types. A
// float[16] crossing the boundary is column-major, m[row + 4*col], the layout
// glUniformMatrix4fv wants with transpose=false.
//
// Handles are wrapped in structs so a texture cannot be passed where a buffer
// is expected. id == 0 is always the invalid handle.

#ifdef __cplusplus
extern "C" {
#endif

typedef struct gfx_buffer       { uint32_t id; } gfx_buffer;
typedef struct gfx_mesh         { uint32_t id; } gfx_mesh;
typedef struct gfx_texture      { uint32_t id; } gfx_texture;
typedef struct gfx_shader       { uint32_t id; } gfx_shader;
typedef struct gfx_rt           { uint32_t id; } gfx_rt;

// ---------------------------------------------------------------- lifecycle

typedef struct gfx_desc {
    int         width;
    int         height;
    const char *title;
    bool        vsync;
    int         msaa;           // 0 or 4

    // Whether the person using it may resize the window.
    //
    // Off for every measurement, and that is the point of it being a flag: a
    // gate that renders into a target of a fixed size and a window somebody can
    // drag are two different things, and a check that silently measured
    // whatever shape the window happened to be is a check nobody can reproduce.
    bool        resizable;
} gfx_desc;

bool gfx_init(const gfx_desc *desc);
void gfx_shutdown(void);
void gfx_begin_frame(void);
void gfx_end_frame(void);

// Whether end_frame presents the finished image.
//
// Off during measurement. Presenting hands the frame to the compositor, and if
// the window is occluded or the compositor throttles, the swap blocks for as
// long as it likes -- a single 89-second frame was measured that way, with the
// process at 4% CPU. That is the compositor's latency, not the renderer's, and
// it swamps any percentile it lands in.
void gfx_set_present(bool enabled);

// Blocks until the GPU has finished the commands already submitted. Needed to
// time a frame honestly: without it the timer stops when the driver accepts the
// work, not when the work is done.
void gfx_finish(void);

void gfx_viewport(int x, int y, int w, int h);
// Draws and clears only inside this rectangle (pixels, from the bottom left)
// until gfx_scissor_off.
void gfx_scissor(int x, int y, int w, int h);
void gfx_scissor_off(void);
void gfx_clear(float r, float g, float b, float a, bool depth);
// The size of what is being drawn to, in pixels, as of this frame.
//
// It FOLLOWS the window: a resizable window that is dragged reports its new
// size here on the next frame, so a caller that lays anything out from these
// has to read them every frame rather than once at startup. Whoever holds a
// render target sized to the screen has to notice the change and build a new
// one -- there is no callback, because a caller comparing what it built against
// what it is drawing to is one fact, and a flag somebody has to clear is two.
int  gfx_screen_width(void);
int  gfx_screen_height(void);

// Reads the colour buffer back as RGBA8, TOP-DOWN: row 0 is the top of the
// screen. glReadPixels itself hands back bottom-up and the backend flips it,
// so callers that computed a rectangle in OpenGL's convention have to mirror
// their rows. Returns a buffer the caller frees with gfx_free_pixels, or null.
//
// Used by the self-check that compares a portal's image against a reference
// render, so a regression fails a build rather than waiting to be noticed.
unsigned char *gfx_read_screen(int *out_w, int *out_h);

// Read back a render target instead of the window.
//
// Reading the WINDOW is not reliable and the failure is not ours: glReadPixels
// on the default framebuffer is subject to the pixel ownership test, so
// wherever another window overlaps ours -- a notification, a tooltip, a
// screenshot tool, the compositor moving something across the screen -- the
// pixels that come back are undefined. That is a gate failing once in a dozen
// runs, on an arbitrary check, unreproducibly, which is the worst kind of test:
// it teaches people to run it again. An offscreen target has no such rule.
unsigned char *gfx_read_target(gfx_rt rt, int *out_w, int *out_h);
void           gfx_free_pixels(unsigned char *pixels);

// ------------------------------------------------------------------ buffers

gfx_buffer gfx_buffer_create(const void *data, int size_bytes, bool dynamic, bool index);
void       gfx_buffer_update(gfx_buffer buf, const void *data, int size_bytes, int offset_bytes);
void       gfx_buffer_destroy(gfx_buffer buf);

typedef enum gfx_attr_type {
    GFX_ATTR_F32 = 0,
    GFX_ATTR_U8,
    GFX_ATTR_U16,
    GFX_ATTR_I16,
} gfx_attr_type;

typedef struct gfx_attr {
    int           location;
    int           count;        // 1..4
    gfx_attr_type type;
    bool          normalized;   // integers scaled into [0,1] / [-1,1]
    int           offset;       // bytes from the start of the vertex
} gfx_attr;

typedef struct gfx_vertexlayout {
    int      stride;
    int      attr_count;
    gfx_attr attrs[8];
} gfx_vertexlayout;

// Binds a vertex buffer, an optional index buffer and a layout into something
// drawable (a VAO underneath). Pass index.id == 0 for non-indexed geometry.
//
// AN INDEX IS SIXTEEN BITS. rlDrawVertexArrayElements draws GL_UNSIGNED_SHORT
// and takes no type, so a buffer of 32-bit indices is read as twice as many
// wrong ones and draws nothing recognisable -- silently, with no GL error. It
// is written here because that is where somebody looks: a caller with more than
// 65535 vertices has to split its geometry, and a caller that does not know
// this loses an afternoon.
//
// Lifting it is a direct glDrawElements with GL_UNSIGNED_INT, which is the same
// small GL shim gfx_texture_array_create has been waiting for.
gfx_mesh gfx_mesh_create(const gfx_vertexlayout *layout, gfx_buffer vertex, gfx_buffer index);
void     gfx_mesh_destroy(gfx_mesh mesh);

// ----------------------------------------------------------------- textures

typedef enum gfx_pixel_format {
    GFX_PF_RGBA8 = 0,
    GFX_PF_R8,
} gfx_pixel_format;

typedef enum gfx_filter {
    GFX_FILTER_NEAREST = 0,
    GFX_FILTER_LINEAR,
    GFX_FILTER_NEAREST_MIP,
} gfx_filter;

gfx_texture gfx_texture_create(const void *pixels, int w, int h, gfx_pixel_format fmt,
                               gfx_filter filter);
void        gfx_texture_update(gfx_texture tex, const void *pixels, int x, int y, int w, int h,
                               gfx_pixel_format fmt);
void        gfx_texture_destroy(gfx_texture tex);
void        gfx_texture_bind(gfx_texture tex, int slot);

// Decodes an image file that is already in memory -- PNG today, whatever the
// backend reads tomorrow. Returns RGBA8 pixels the caller frees with
// gfx_image_free, or null for bytes that are not a picture.
//
// From MEMORY and not from a path on purpose: the next thing to hand it is a
// file that arrived over the wire and lives in a cache, not next to the binary
// (DESIGN.md 4.5). A decoder that only reads paths would have to be replaced
// the moment media start travelling.
unsigned char *gfx_image_decode_rgba(const void *bytes, int size, int *w, int *h);
void           gfx_image_free(unsigned char *pixels);

// GL_TEXTURE_2D_ARRAY, which raylib does not expose. Backed by a small GL shim.
// One layer per block texture is what kills atlas bleeding and makes mipmaps
// free (DESIGN.md 2.6); it is stubbed until M1 needs it.
gfx_texture gfx_texture_array_create(const void *pixels, int w, int h, int layers,
                                     gfx_pixel_format fmt, gfx_filter filter);

// ----------------------------------------------------------------------- 2D
//
// A screen-space pass, in pixels, origin top-left and y going down -- which is
// the convention every UI in the world uses and the opposite of OpenGL's, so
// the backend does the flip once here rather than at every call site.
//
// This exists because DESIGN.md 8 puts a real UI in M7 and plat_debug_text has
// said "debug overlay only" since M0. It is deliberately four calls and not a
// widget set: microui and Clay (DESIGN.md 2.7) both emit a list of rectangles,
// images and text runs with a clip rectangle, and this is exactly that list's
// back end. Building the widgets first and the surface after is how a UI ends
// up unable to run on anything but the toolkit it was born on.
//
// Between begin and end the depth test is off and alpha blending is on; both
// are put back at end. Nesting is not supported and not needed.
void gfx_2d_begin(int width, int height);
void gfx_2d_end(void);

// A filled rectangle. Colour is RGBA, 0..1, straight alpha.
void gfx_2d_rect(float x, float y, float w, float h, const float rgba[4]);

// A filled circle, for the things a rectangle cannot say: the dot beside a
// server in the menu, whose colour is whether it answered.
void gfx_2d_circle(float cx, float cy, float radius, const float rgba[4]);

// A piece of a texture, stretched into a rectangle. The source is NORMALISED
// -- origin and size in 0..1 -- and not in texels, because the caller that
// exists knows its tile as a fraction of the atlas (column/tiles) and does not
// know how many pixels the atlas is. Texels would mean this layer had to track
// every texture's size so that callers could immediately divide it back out.
void gfx_2d_image(gfx_texture tex, float u, float v, float uw, float vh,
                  float x, float y, float w, float h, const float rgba[4]);

// Clips everything drawn after it. A width or height of zero or less clears the
// clip rather than hiding everything, because "no rectangle" is what a caller
// means when it stops clipping.
void gfx_2d_scissor(float x, float y, float w, float h);

// Text in the platform's built-in font. Returns the width drawn, so a caller
// can lay out a row without measuring twice.
float gfx_2d_text(const char *text, float x, float y, float size, const float rgba[4]);
float gfx_2d_text_width(const char *text, float size);

// ------------------------------------------------------------------ shaders

gfx_shader gfx_shader_create(const char *vs_source, const char *fs_source);
void       gfx_shader_destroy(gfx_shader shader);
void       gfx_shader_bind(gfx_shader shader);

// Hands the pipeline back to the platform's own shader. Anything the platform
// draws for itself -- debug text, in this build -- goes through its batch, and
// that batch uses whatever shader is current. Leave one of ours bound and the
// font renders as garbage, because our vertex layout is not the one raylib's
// batch feeds it.
void gfx_shader_bind_default(void);

int  gfx_uniform_location(gfx_shader shader, const char *name);
void gfx_uniform_set_f(int loc, float v);
void gfx_uniform_set_v2(int loc, const float v[2]);
void gfx_uniform_set_v3(int loc, const float v[3]);
void gfx_uniform_set_v4(int loc, const float v[4], int count);
void gfx_uniform_set_mat4(int loc, const float m[16]);
void gfx_uniform_set_int(int loc, int v);

// --------------------------------------------------------------- matrices

// The entry point for Lengyel's oblique near plane: the client hands over an
// already-modified projection and this layer asks no questions.
void gfx_projection_set(const float m[16]);
void gfx_view_set(const float m[16]);
void gfx_model_set(const float m[16]);

// ------------------------------------------------------------- clip planes

// Installs up to 5 world-space planes and enables GL_CLIP_DISTANCE0..n-1.
// Passing count == 0 disables clipping. Shaders opt in by writing
// gl_ClipDistance[k]; see shaders.h.
//
// The desktop path uses gl_ClipDistance. The portable fallback that discards in
// the fragment shader (DESIGN.md 2.4, "strada B") is a shader #ifdef, not a
// second code path here.
void gfx_clip_planes_set(const float *planes_xyzw, int count);

// ----------------------------------------------------------- render targets

gfx_rt      gfx_rt_create(int w, int h, bool with_depth);
void        gfx_rt_destroy(gfx_rt rt);
void        gfx_rt_begin(gfx_rt rt);
void        gfx_rt_end(void);
gfx_texture gfx_rt_texture(gfx_rt rt);

// A target with no colour, only a depth texture, made to be sampled by a
// sampler2DShadow: compared against a reference in hardware and filtered, so
// four taps give a soft edge. What the sun's shadow map is drawn into.
gfx_rt      gfx_shadow_create(int size);
gfx_texture gfx_rt_depth(gfx_rt rt);
int         gfx_rt_width(gfx_rt rt);
int         gfx_rt_height(gfx_rt rt);

// ------------------------------------------------------------------- state

typedef enum gfx_cull  { GFX_CULL_NONE = 0, GFX_CULL_BACK, GFX_CULL_FRONT } gfx_cull;
// ADD puts colour times alpha on top of what is there: light, which only brightens.
typedef enum gfx_blend { GFX_BLEND_NONE = 0, GFX_BLEND_ALPHA, GFX_BLEND_ADD } gfx_blend;

typedef struct gfx_state {
    bool      depth_test;
    bool      depth_write;
    gfx_cull  cull;
    gfx_blend blend;
    bool      color_mask[4];
} gfx_state;

gfx_state gfx_state_default(void);
void      gfx_state_set(const gfx_state *state);

// ------------------------------------------------------------------- draw

void gfx_draw(gfx_mesh mesh, int count, int offset);
void gfx_draw_instanced(gfx_mesh mesh, int count, int offset, int instances);

#ifdef __cplusplus
} // extern "C"
#endif
