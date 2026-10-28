#pragma once

#include "client/gfx/gfx.h"
#include "client/render/water.h"
#include "core/math/frustum.h"
#include "core/media/face_tiles.h"
#include "core/item/item_registry.h"
#include "core/media/tile_bake.h"
#include "core/math/mat4.h"
#include "core/registry/node_registry.h"
#include "core/voxel/chunk.h"
#include "core/voxel/mesh_pool.h"
#include "core/voxel/mesher.h"
#include "core/world/daylight.h"

#include <functional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace cnp {

// Draws the voxel world. One mesh per chunk, rebuilt when the chunk is dirty.
//
// Deliberately has no idea which camera it is drawing for: the portal pass
// calls draw() with a virtual camera's matrices and its own clip planes already
// installed. Keeping the renderer camera-agnostic is what makes a second view
// cost a call rather than a special case.
// Where a block's texture comes from. Returns false when this source has none
// for that file, which is not an error -- it is the ordinary case for a block
// whose mod shipped no art, and the renderer bakes a placeholder instead.
//
// A callback and not a directory: today it reads `assets/textures`, and the
// next thing to put behind it is the media a session was sent (DESIGN.md 4.5),
// which lives in a cache keyed by hash and not in a folder next to the binary.
using MediaSource = std::function<bool(const std::string &file, std::vector<uint8_t> &out)>;

// How much of a frame meshing may take.
//
// A quarter of a sixtieth of a second. The number is chosen against the budget
// and not against what meshing happens to cost: 16.7 ms is the frame, drawing
// and the portal passes are the part the player is actually there for, and a
// world that is rebuilding gets the smaller share of the two. It is a slice
// rather than "as much as fits" because the frame it has to fit inside is not
// this renderer's to spend -- there is a second world being meshed for the
// opening, and there is everything that is not meshing at all.
inline constexpr double kMeshBudgetMs = 4.0;

// A vertex of a block that is drawn as boxes.
//
// Not the chunk's Vertex and it cannot be: that one is eight bytes with byte
// positions, one unit per node, which is what makes a world of cubes cheap and
// what leaves no room for anything inside a node. These are floats in WORLD
// space, built on the frame thread for the handful of cells that need them, so
// a chest costs what a chest costs and a chunk of stone still costs 8 bytes a
// vertex.
//
// The fields after the position are the chunk vertex's own, in the same order
// and meaning the same things, because the two are drawn by the SAME shader: a
// second shader would be a second place for the daylight rule, the tile
// arithmetic and the face shading to live.
struct PartVertex {
    float   x, y, z;
    uint8_t face_ao;
    float   u, v;
    uint8_t sky;    // Vertex::sky
    uint8_t tex;

    // Which way the picture runs, for a liquid: 0 still, 1..8 one of eight
    // directions across a surface (downhill), 9 straight down a falling side.
    // Every box leaves it 0. It was the padding byte, so it costs nothing.
    uint8_t flow = 0;
    uint8_t lamp;   // Vertex::lamp
    uint8_t classic;   // Vertex::classic

    // From a cell's packed light, with no direction: a box is inside its cell.
    void set_light(uint8_t packed)
    {
        classic = packed;
        sky  = (uint8_t)(light_curve((float)(packed >> 4)) * 255.0f + 0.5f);
        lamp = (uint8_t)(light_curve((float)(packed & 15)) * 255.0f + 0.5f);
    }
};

// The triangles one box of one cell is drawn as, appended to `out`.
//
// `turn` rotates the BOX and not the pictures, which is the opposite of what a
// cube does and is the only thing that can be right here: a cube is six quads
// on the walls of its cell, so turning it can only mean moving the pictures
// between them, while a box has a shape that has to turn with it. The face a
// quad ends up looking along is what it wears and what shades it.
//
// A free function so that a gate can ask what it built without a window.
// `open` is 0 for a box at rest and 1 for one thrown wide, and it moves only a
// box that is NAMED: an unnamed box is part of the shape and a named one is a
// piece that swings. The hinge is the box's own back-bottom edge in unturned
// node space, so a lid opens away from the front the block was put down facing
// -- and the turn is applied after it, which is what keeps a chest turned east
// from opening northwards.
void emit_node_box(const NodeDef::Box &box, const NodeDef &def, float wx, float wy, float wz,
                   uint8_t turn, uint8_t light, float open, std::vector<PartVertex> &out);

// The triangles one cell of liquid is drawn as, appended to `out`, in the cell's
// own chunk-local space. Each face the mesher kept, with its top edge at the
// corner heights it worked out -- so the top slopes and the sides follow it.
void emit_liquid_cell(const LiquidCell &cell, const NodeDef &def, std::vector<PartVertex> &out);

class WorldRenderer {
public:
    bool init(NodeRegistry &reg, const MediaSource &media = {});

    // Rebuilt when the registry changes, which is what a mod reload does.
    // Without a source every block gets the generated placeholder, which is
    // what this did before there were any files at all.
    //
    // The WORLD is a parameter because a rebuild RENUMBERS the tiles, and the
    // numbers are in the vertices: every mesh built against the old atlas is
    // wearing somebody else's picture until it is built again. That used to be
    // the caller's job to remember, and two of the callers -- the two on the
    // media path, where a world's textures arrive after its chunks -- did not.
    // Nothing caught it because one tile per file NAME meant the numbering
    // hardly ever moved; one tile per PICTURE moves it whenever a pack aliases
    // two blocks to one file, and the frame comes back with the ground wearing
    // whatever tile fell into its old number.
    // `items`, when there is one, gets its squares dressed in the same pass:
    // an item asks for one file under its own name, shares the block atlas and
    // shares its budget. Optional because not every caller has an item table --
    // the baker and a couple of gates render a world and nothing else -- and an
    // absent one simply leaves every item wearing what it wore.
    void rebuild_atlas(World &world, NodeRegistry &reg, const MediaSource &media = {},
                       ItemRegistry *items = nullptr);

private:
    // The atlas alone, without saying the meshes are stale. Private, and used
    // by init() only: at init there are no meshes to be stale.
    void build_atlas(NodeRegistry &reg, const MediaSource &media, ItemRegistry *items = nullptr);

public:

    // How the last atlas was filled. Reported rather than assumed: "the block
    // is drawn" and "the block is drawn from the texture somebody made for it"
    // are different claims, and only one of them used to be checkable.
    //
    // "Supplied" and not "from a file": the source is a callback, and for a
    // world on somebody else's server what it reads is the media that world
    // sent, out of the shared cache. The renderer has never known which, and
    // the name used to say it did.
    int textures_supplied() const { return m_from_file; }
    int textures_generated() const { return m_generated; }

    // The atlas, for anything that wants to draw a block outside the world --
    // which today is the hotbar. Handed out rather than copied: an icon drawn
    // from a second atlas is an icon that can disagree with the block it names
    // after a mod reload rebuilds one of them and not the other.
    //
    // The layout is the one kWorldFS reads: tile `t` sits at column
    // t % atlas_tiles, row t / atlas_tiles, and NodeDef::tex holds `t` per
    // face. Face 0 is what an icon should use.
    gfx_texture atlas() const { return m_texture; }
    int         atlas_tiles() const { return m_atlas_tiles; }

    // How big one tile in it is. THE PACK'S SIZE, not this engine's constant:
    // art drawn at 128 is used at 128, and kTilePixels is only what a generated
    // placeholder falls back to when nothing else has an opinion. A caller that
    // assumed the constant would read every tile at a quarter of its width.
    int tile_pixels() const { return m_tile_px; }
    int atlas_pixels() const { return m_atlas_tiles * m_tile_px; }

    // Distinct pictures in it. Not the number of blocks: two blocks wearing one
    // file share a tile, and a block with a top, a bottom and a side costs
    // three. The number a pack changes, and the one the ceiling applies to.
    int tiles_used() const { return m_tiles_used; }

    // Whether the last rebuild MOVED any tile number, which is the only reason
    // a mesh built against the old atlas is wrong. Read by the gate: "the world
    // was re-meshed" and "the world did not need re-meshing" look identical
    // from the outside on a world whose numbering did not change.
    bool tiles_moved() const { return m_tiles_moved; }

    // Pictures there was no room for. Non-zero means blocks are wearing a tile
    // that is not theirs, and it is a number rather than a flag so that a
    // steady one under a threshold cannot be mistaken for slack.
    int tiles_over_budget() const { return m_over_budget; }
    void shutdown();

private:
    int m_from_file = 0;
    int m_generated = 0;

public:

    // Rebuilds meshes for chunks the world marked dirty. Bounded per call: a
    // rebuild storm must cost frames of latency, never a freeze.
    //
    // The build runs on a worker (core/voxel/mesh_pool.h) and this call does
    // the two halves that cannot leave the frame thread: the snapshot, which
    // reads the World, and the upload, which is a GL call. What it uploads is
    // whatever the pool has finished -- usually the chunks handed over on an
    // EARLIER call, which is the whole point and the one thing a caller has to
    // know about it.
    //
    // Unless it asks for no budget. With `budget_ms` of zero or less this
    // blocks until everything it handed over has come back and been uploaded,
    // which is what every caller that builds a scene and then measures it
    // wants, and what all of them did before there was a pool.
    //
    // Two bounds, because they answer different questions. `max_uploads_per_call`
    // is a count and it is what the gates that build a scene up front turn off
    // by passing a huge one. `budget_ms` is a slice of the frame, and it is the
    // one a player feels: chunks are not the same size as each other -- a wall
    // of stone and a chunk of leaves differ by an order of magnitude in vertices
    // -- so a count that holds the frame on average lets four expensive chunks
    // land in the same one.
    //
    // A budget of zero or less means no time limit, which is what every caller
    // that is not a frame loop wants and what all of them did before this
    // existed. Chunks left unmeshed when the budget runs out are marked dirty
    // again rather than dropped: take_dirty clears the flag, and stopping early
    // without putting them back is a hole in the world nobody ever rebuilds.
    void sync(World &world, const NodeRegistry &reg, int max_uploads_per_call = 4,
              double budget_ms = 0.0);

    // `cull` skips chunks outside the frustum. The portal pass passes the
    // virtual camera's frustum, which is a different set of chunks from the
    // player's -- that separation is the point of taking it as an argument
    // rather than keeping one inside.
    void draw(const Mat4 &projection, const Mat4 &view, bool cull = true);
    // The translucent pass (glass, water), after draw() and the bodies.
    void draw_translucent(const Mat4 &projection, const Mat4 &view);

    // The day's light for drawing, and the classic look's (daylight_at).
    void set_daylight(float d, float classic_day = 0.0f)
    {
        m_daylight    = d;
        m_classic_day = classic_day;
    }
    float daylight() const { return m_classic ? m_classic_day : m_daylight; }

    // The light as it was before the rework, kept as an option: flat per face,
    // a straight line over a floor of 15%, a night floor of 22%, no sun and no
    // shadows, and bodies at full light.
    void set_classic(bool on) { m_classic = on; }
    bool classic() const { return m_classic; }

    // How bright to draw something another shader draws, standing in a cell
    // with this packed light. A body is not lit at all in the classic look.
    float cell_light(uint8_t packed, bool body) const
    {
        if (!m_classic) return light_shade(packed, m_daylight);
        if (body) return 1.0f;
        const float sun = (float)(packed >> 4) / 15.0f * m_classic_day, lamp = (float)(packed & 15) / 15.0f;
        return 0.15f + 0.85f * (sun > lamp ? sun : lamp);
    }

    // The sun or the moon, for the faces it lights and the shadows it casts.
    // A world with a fixed sky has none: pass nothing and only lamps light it.
    void set_sky_light(const SkyLight *l)
    {
        m_has_sky_light = l != nullptr;
        if (l) m_sky_light = *l;
    }

    // The shadow map's size in texels; 0 turns shadows off.
    void set_shadow_size(int texels);
    int  shadow_size() const { return m_shadow_size; }

    // Draws the sun's shadow map around the eye, for every draw() this frame,
    // the main view's and the openings'. Called once a frame, before any
    // render target is begun: it is one itself. A world drawn without it
    // (one only seen through an opening) has the sun on every face it turns
    // to the sun.
    void draw_shadows(Vec3 eye, Vec3 forward);
    void forget_shadows() { m_shadow_drawn = false; }

    // Clouds over this world, tinted by its sky of the hour. Off for a world
    // with a fixed sky (core.world_sky): a Inferno has a ceiling, not weather.
    // `drift` is how far the wind has carried them, in nodes.
    // `storm`, when not null, is core.world_storm's colour: the clouds are
    // that colour, and turn in whirls here and there.
    void set_clouds(bool on, const float sky[3], float drift = 0.0f, const float *storm = nullptr)
    {
        m_clouds      = on;
        m_cloud_drift = drift;
        for (int c = 0; c < 3; ++c) m_cloud_sky[c] = sky[c];
        m_cloud_storm[3] = storm ? 1.0f : 0.0f;
        for (int c = 0; c < 3; ++c) m_cloud_storm[c] = storm ? storm[c] : 0.0f;
    }

    // Weather (core.overcast), 0..1: the clouds thicker and grey.
    void set_overcast(float o) { m_cloud_overcast = o; }

    // A world's twisters (core.storm_funnel): four of x, ground y, z, and 1
    // when there is one, drawn by the cloud pass from the clouds to the ground.
    void set_funnels(const float f[16])
    {
        for (int i = 0; i < 16; ++i) m_cloud_funnel[i] = f[i];
    }

    // Whether the walk runs. On everywhere; off is for the one gate that has to
    // render the same view both ways and compare the pixels, because the only
    // thing that makes occlusion culling safe to ship is a picture that did not
    // change.
    void set_occlusion(bool on) { m_occlusion = on; }

    // Water for the camera at `eye`: everything drawn until it is turned off
    // again -- by this renderer, and by every shader with WaterUniforms: the
    // sky behind, bodies, models, openings -- fades into the water's colour by
    // how much of the line a pixel sees, from the near plane on, runs through
    // the water around the eye (CNP_WATER_GLSL in shaders.h).
    //
    // `slot` is which box of water cells: 0 for the player's camera, 1 + the
    // link for a portal's. A virtual camera is at the other end of its opening,
    // and one box that followed every camera would be rebuilt several times a
    // frame whenever the two ends are apart.
    static constexpr int kWaterSlots = 5;   // the player, and the four links drawn at most
    void set_water(bool on, Vec3 eye = Vec3{}, int slot = 0);
    bool occlusion() const { return m_occlusion; }

    // How far this world is drawn, in nodes, measured from the eye to the
    // nearest point of a chunk.
    //
    // Per renderer and therefore per world, which is the point: DESIGN.md 4.2
    // budgets a passive session at about a third of the active one's, and a
    // view range that lived once would be the fourth of the fork's historic
    // dual-client bugs. A huge default means every gate written before this
    // draws exactly what it drew.
    void set_view_range(float nodes) { m_view_range = nodes; }
    float view_range() const { return m_view_range; }

    // Chunks dropped by the range on the last draw, as opposed to by the
    // frustum. Two numbers, because "nothing was drawn" has two causes and
    // they want different fixes.
    int range_culled() const { return m_range_culled; }

    int    draw_calls()    const { return m_draw_calls; }
    int    culled_chunks() const { return m_culled; }

    // Chunks the frustum kept and the rock hid. A third number beside the other
    // two for the same reason they are two: "nothing was drawn" has causes that
    // want different fixes, and telling a camera looking the wrong way from a
    // camera buried in a hill is the whole of this one.
    int    occluded_chunks() const { return m_occluded; }
    size_t vertex_count()  const { return m_total_vertices; }

    // How many chunks the last sync() UPLOADED. A frame that remeshes is a
    // frame that costs milliseconds instead of microseconds, so counting them
    // is how a frame-time percentile gets explained instead of guessed at.
    //
    // Uploaded and not started, because uploaded is when a chunk is on the
    // screen. With a budget the two are different calls: a frame hands four
    // chunks over and uploads the four it was given last time.
    int last_meshed() const { return m_last_meshed; }

    // How many it handed to the pool. With no budget this equals the above; with
    // one it is the front of the pipeline and the other number is the back.
    int last_queued() const { return m_last_queued; }

    // Whether there is anything still to come -- being meshed, or meshed and
    // not yet uploaded. "Nothing was uploaded this frame" has two meanings and
    // only this tells them apart: the world is finished, or the world is still
    // being built.
    bool catching_up() const;

    // What the last sync() spent, in milliseconds. Reported by the renderer
    // rather than timed by the caller because the caller that wants it is the
    // one deciding what is left of a shared budget, and a second clock around
    // the call would be measuring a slightly different interval than the one
    // the budget was enforced against.
    double last_sync_ms() const { return m_last_sync_ms; }

    // The eye the last draw() sorted its translucent chunks by, read back out
    // of the view matrix it was given. The portal passes hand this function a
    // virtual camera, so this is how "sorted by the virtual camera rather than
    // by the player" stops being a claim about the code and becomes a number.
    Vec3 last_sort_eye() const { return m_last_sort_eye; }
    size_t gpu_bytes()     const { return m_total_vertices * sizeof(Vertex); }

private:
    void bind_world(const Mat4 &projection, const Mat4 &view);   // this pass's shader and what it reads
    // One buffer per chunk per pass. Two buffers rather than one with an
    // offset because a chunk's translucent face count changes on its own -- a
    // window opened in a stone wall grows the alpha stream and leaves the
    // opaque one alone -- and a single buffer would have to be rebuilt whole
    // every time either half moved.
    struct MeshBuffer {
        gfx_buffer vb{0};
        gfx_mesh   mesh{0};
        int        vertex_count = 0;
        int        capacity     = 0;   // vertices, so a shrinking chunk reuses its buffer
    };

    struct WaterCell {
        uint8_t x, y, z;   // chunk-local
        uint8_t h[4];      // LiquidCell::h
    };

    struct ChunkMesh {
        MeshBuffer opaque;
        MeshBuffer alpha;

        // The chunk's liquid, in floats: a surface at its own height is a
        // thing an eight-byte vertex cannot say. Drawn with the alpha stream,
        // in the same back-to-front order, because water is seen through.
        MeshBuffer liquid;
        Vec3       origin;             // world position of the chunk's minimum corner

        // The cells in this chunk that are drawn as boxes, as the mesher found
        // them. Kept per chunk rather than in one table for the world because
        // they arrive and go stale one chunk at a time, exactly as the mesh
        // does.
        std::vector<PartCell> parts;

        // Whether one of them is a track piece's corner, whose road reaches
        // out of this chunk (core/track/piece.h): it is drawn while that road
        // could be on screen, not only while this chunk is.
        bool pieces = false;

        // Every cell of water in it with its surface's corners, for the box
        // refresh_water fills: the faces are in `liquid`, the body is here.
        std::vector<WaterCell> water;
    };

    void upload(MeshBuffer &cm, const std::vector<Vertex> &verts);
    void upload_liquid(MeshBuffer &cm, const std::vector<PartVertex> &verts);
    std::vector<PartVertex> m_scratch_liquid;

    // One buffer for every box in sight, rebuilt each frame and drawn once.
    // Chests are rare and a box is 36 vertices, so the whole of it is a few
    // hundred vertices in a world that has any at all -- against a draw call
    // per cell, which is what a buffer per chest would cost.
    void draw_parts(const NodeRegistry &reg, const Mat4 &projection, const Mat4 &view);

public:
    // How far open the cell at x,y,z is, 0..1. Set by the client, which is
    // where the answer lives -- a networked client is TOLD which cells are open
    // and a single-process one knows -- and where the tween between the two
    // states belongs, because that is where a frame's dt already is.
    //
    // Unset means nothing is open, which is what a world with no containers in
    // it is.
    void set_open_query(std::function<float(int, int, int)> q) { m_open_at = std::move(q); }

    // How a cell that wears a MODEL gets drawn. Set by the client, because this
    // renderer knows about an atlas and a chunk and deliberately nothing about
    // glTF: the models a world has are a table the session owns
    // (client/render/model_store.h), and a body already draws itself through
    // it. What arrives here is the model's id and where the cell is; what the
    // client does with it is find the model and hand it to its own renderer.
    //
    // Unset means a world with no models drawn at blocks, which is every world
    // before M11's `drawtype = mesh`. Such a cell then draws its boxes, or
    // nothing if it has none -- never a cube, because a cube is what it is not.
    //
    // The first float is how far open the cell is (set_open_query), so a
    // chest's model can swing its lid the way a chest's boxes do; the second
    // is how bright the cell is, 0.15..1, by the rule a box is lit by.
    using ModelDraw =
        std::function<void(ModelId, const Mat4 &, float, float, const Mat4 &, const Mat4 &)>;
    void set_model_draw(ModelDraw d)
    {
        m_draw_model = std::move(d);
    }

private:
    std::function<float(int, int, int)>          m_open_at;
    ModelDraw                                    m_draw_model;

    const NodeRegistry     *m_reg = nullptr;   // whatever sync() was last given
    gfx_buffer              m_parts_vb{0};
    gfx_mesh                m_parts_mesh{0};
    int                     m_parts_capacity = 0;   // vertices
    std::vector<PartVertex> m_part_vertices;
    // The boxes of a see-through block (a glass pane): after the opaque ones in
    // the same buffer, drawn in the translucent pass from m_part_see_first.
    std::vector<PartVertex> m_part_see;
    int                     m_part_see_first = 0;
    int                     m_part_see_count = 0;
    std::vector<const ChunkMesh *> m_part_chunks;   // the visible ones, this frame

    // The cells that wear a model, found while walking the boxes and drawn
    // after them: a model binds its own shader and the boxes are one call.
    struct ModelCell {
        ModelId model;
        Mat4    place;
        float   open;
        float   light;
    };
    std::vector<ModelCell> m_model_cells;
    int                     m_parts_drawn = 0;      // cells, not vertices

    gfx_shader  m_shader{0};
    gfx_texture m_texture{0};
    int         m_loc_texture     = -1;
    int         m_loc_atlas_tiles = -1;
    int         m_atlas_tiles     = 4;
    int         m_tile_px         = kTilePixels;
    int         m_tiles_used      = 0;
    bool        m_tiles_moved     = false;
    int         m_over_budget     = 0;
    int         m_loc_daylight    = -1;
    int         m_loc_time        = -1;

    // The sun (draw_shadows).
    bool        m_classic     = false;
    float       m_classic_day = 1.0f;
    int         m_loc_classic = -1, m_loc_classic_day = -1;
    SkyLight    m_sky_light{};
    bool        m_has_sky_light = false;
    gfx_shader  m_shadow_shader{0};
    gfx_rt      m_shadow_rt{0};
    gfx_rt      m_shadow_none{0};   // 1 texel, bound when there is no map: a shadow sampler wants a depth texture
    int         m_shadow_size  = 2048;
    bool        m_shadow_drawn = false;
    Mat4        m_shadow_vp;
    float       m_shadow_texel = 0.0f;
    int         m_loc_shadow_tiles = -1, m_loc_shadow_tex = -1;
    int         m_loc_cloud_sun = -1, m_loc_cloud_sun_on = -1, m_loc_cloud_moon = -1, m_loc_cloud_storm = -1;
    int         m_loc_cloud_funnel = -1, m_loc_cloud_range = -1, m_loc_cloud_overcast = -1;
    int         m_loc_sun_dir = -1, m_loc_sun_color = -1, m_loc_ambient = -1;
    int         m_loc_shadow_vp = -1, m_loc_shadow = -1, m_loc_shadow_on = -1, m_loc_shadow_texel = -1;

    // The sky behind the water (shaders::kWaterSkyFS): one triangle over the
    // screen, drawn after the opaque pass whenever there is water in the box.
    gfx_shader    m_sky_shader{0};
    gfx_buffer    m_sky_vb{0};
    gfx_mesh      m_sky_mesh{0};
    WaterUniforms m_water_u, m_sky_water_u, m_cloud_water_u;

    // The clouds (shaders::kCloudFS), on the same triangle as the sky above.
    gfx_shader m_cloud_shader{0};
    int        m_loc_cloud_drift = -1, m_loc_cloud_daylight = -1, m_loc_cloud_sky = -1;
    bool       m_clouds          = false;
    float      m_cloud_drift     = 0.0f;
    float      m_cloud_sky[4]    = {0.0f, 0.0f, 0.0f, 1.0f};
    float      m_cloud_storm[4]  = {0.0f, 0.0f, 0.0f, 0.0f};   // rgb, and 1 when a storm
    float      m_cloud_funnel[16] = {};                         // set_funnels
    float      m_cloud_overcast   = 0.0f;                       // set_overcast
    friend struct WaterUniforms;

    // The camera the water is for (set_water), and the cells of water around
    // it: a box of kWaterBox cells from m_water_box, as a texture the shaders
    // walk, 4 bytes a cell -- its corners, all 0 for no water. Filled again
    // when the eye has gone a quarter of the box from its middle or a chunk's
    // water changed.
    void refresh_water();
    static constexpr int kWaterBox[3] = {64, 32, 64};   // kWaterBox in shaders.h
    static constexpr int kWaterTexW   = 8 * 64;         // its 32 layers, 8 x 4
    static constexpr int kWaterTexH   = 4 * 64;
    struct WaterBox {
        gfx_texture          tex{0};
        std::vector<uint8_t> px;
        int                  box[3] = {0, 0, 0};
        bool                 stale  = true;
        bool                 any    = false;
        int                  lo[3] = {0, 0, 0}, hi[3] = {0, 0, 0};   // uWaterLo/Hi
    };
    void mark_water_stale()
    {
        for (WaterBox &w : m_water) w.stale = true;
    }
    bool     m_water_on   = false;
    Vec3     m_water_eye{};
    int      m_water_slot = 0;
    WaterBox m_water[kWaterSlots];

    std::unordered_map<ChunkPos, ChunkMesh, ChunkPosHash> m_meshes;

    // Kept for every chunk the pool answers about, INCLUDING the ones with no
    // geometry. A chunk of pure air has no mesh and no draw call, and the walk
    // still has to be able to cross it -- a table with only the drawable chunks
    // in it would wall the sky off.
    std::unordered_map<ChunkPos, ChunkVisibility, ChunkPosHash> m_visibility;

    // Where the walk is, and what it is carrying.
    //
    // `regions` is which pieces of `pos`'s air the walk arrived in, one bit
    // each, and kAllRegions for a chunk entered with no idea of where -- the
    // camera's own, one seeded from outside the world, or one with more ways
    // through it than the mask can hold. `entry` is the face it came in by, or
    // -1 for a chunk it did not come into from anywhere, and it is only read
    // when there is nothing finer to read.
    static constexpr uint64_t kAllRegions = ~0ull;

    struct WalkItem {
        ChunkPos pos{};
        int      entry   = -1;
        uint64_t regions = kAllRegions;
    };

    // Scratch for the walk, kept so a frame allocates nothing for it. The
    // reached chunks carry the regions they were reached IN, because a chunk
    // reached twice by two different ways through it has to expand by both --
    // keeping only the first would be a room entered by the wrong door and
    // never drawn.
    std::unordered_map<ChunkPos, uint64_t, ChunkPosHash> m_reached;
    std::vector<WalkItem>                                m_walk;

    // Back-to-front order for the alpha pass, rebuilt per draw. A member so it
    // keeps its capacity: this runs twice per portal per frame.
    std::vector<std::pair<float, const ChunkMesh *>> m_alpha_order;
    std::vector<std::pair<float, const ChunkMesh *>> m_opaque_order;

    // Reused every rebuild rather than reallocated, so a session that never
    // stops editing never keeps growing the allocator's arena.
    ChunkSnapshot       m_scratch_snapshot;
    MeshPool::Result    m_scratch_result;
    std::vector<Vertex> m_scratch_vertices;
    std::vector<Vertex> m_scratch_alpha;
    Vec3                m_last_sort_eye{};

    float  m_daylight       = 1.0f;
    int    m_draw_calls     = 0;
    int    m_culled         = 0;
    int    m_occluded       = 0;
    bool   m_occlusion      = true;
    int    m_range_culled   = 0;
    float  m_view_range     = 1e9f;
    int    m_last_meshed = 0;
    int    m_last_queued = 0;
    double m_last_sync_ms = 0.0;
    size_t m_total_vertices = 0;
    uint64_t m_erased_seen  = 0;   // World::erased() when the meshes were last pruned
};

} // namespace cnp
