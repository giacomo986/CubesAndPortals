#include "client/render/world_render.h"

#include "core/diag/profiler.h"

#include <chrono>
#include <cmath>

#include "core/media/content_hash.h"
#include "core/media/tile_bake.h"

#include <algorithm>

#include "client/render/shaders.h"

#include <cstdint>
#include <cstdio>
#include <array>
#include <map>

#include "core/track/piece.h"

namespace cnp {
namespace {

// One tile per registered node. The atlas grows to whatever the registry ended
// up holding, so a mod adding twenty blocks needs no constant changed anywhere.
// The texture array of DESIGN.md 2.6 replaces this outright -- one layer per
// texture, no bleeding, free mipmaps -- but that needs glTexImage3D through a
// GL shim.
int atlas_tiles_for(size_t node_count)
{
    int t = 4;
    while ((size_t)(t * t) < node_count) t *= 2;
    return t;
}

gfx_vertexlayout world_layout()
{
    gfx_vertexlayout l{};
    l.stride     = (int)sizeof(Vertex);
    l.attr_count = 7;
    //                    loc  n  type          normalised  offset
    l.attrs[0] = gfx_attr{  0, 3, GFX_ATTR_U8,  false,      0};   // position 0..32
    l.attrs[1] = gfx_attr{  1, 1, GFX_ATTR_U8,  false,      3};   // face | ao
    l.attrs[2] = gfx_attr{  2, 2, GFX_ATTR_U8,  false,      4};   // uv in tiles
    l.attrs[3] = gfx_attr{  3, 1, GFX_ATTR_U8,  false,      6};   // sky brightness
    l.attrs[4] = gfx_attr{  4, 1, GFX_ATTR_U8,  false,      7};   // texture layer
    l.attrs[5] = gfx_attr{  6, 1, GFX_ATTR_U8,  false,      8};   // lamp brightness
    l.attrs[6] = gfx_attr{  7, 1, GFX_ATTR_U8,  false,      9};   // the classic look's packed light
    return l;
}

// One tile into the atlas. `src` is `tile_px` square, RGBA8.
void blit_tile(std::vector<uint8_t> &px, int tiles, int tile_px, int tile, const uint8_t *src)
{
    const int atlas_px = tiles * tile_px;
    const int ox = (tile % tiles) * tile_px;
    const int oy = (tile / tiles) * tile_px;

    for (int y = 0; y < tile_px; ++y) {
        uint8_t       *dst = &px[((size_t)(oy + y) * atlas_px + ox) * 4];
        const uint8_t *row = src + (size_t)y * tile_px * 4;
        std::copy(row, row + (size_t)tile_px * 4, dst);
    }
}

// What a tile may measure. The lower bound is there because an atlas of 1px
// tiles is a palette and the half-texel inset in kWorldFS has nothing to inset
// into; the upper one, with the atlas bound below, is what keeps a pack from
// asking for a texture no GL 3.3 driver has to support.
constexpr int kMinTilePixels  = 4;
constexpr int kMaxTilePixels  = 512;
constexpr int kMaxAtlasPixels = 4096;

// One decoded picture, before there is an atlas to put it in.
struct LoadedTile {
    std::vector<uint8_t> px;
    int                  size = 0;   // 0 means nobody supplied one
};

// The size the pack agrees on.
//
// The MOST COMMON size among the pictures that arrived, and the larger of two
// that tie. Not "the first one": which picture comes first is registration
// order, so with that rule a pack's whole atlas would be decided by which mod
// happened to load, and adding an unrelated mod would change the resolution of
// every block in the world.
//
// Nothing supplied means nothing to agree on, and the answer is the size this
// engine bakes its own placeholders at.
int agreed_tile_size(const std::vector<LoadedTile> &art, int tiles)
{
    std::map<int, int> votes;
    for (const LoadedTile &t : art)
        if (t.size > 0) ++votes[t.size];

    int best = 0, best_votes = 0;
    for (const auto &[size, n] : votes)
        if (n > best_votes || (n == best_votes && size > best)) {
            best       = size;
            best_votes = n;
        }

    if (best == 0) return kTilePixels;

    // A pack whose art is large and a registry with many blocks in it multiply,
    // and the product is one texture. Said out loud rather than clamped
    // silently: a pack drawn at a quarter of the size it came in is a pack
    // somebody will report as blurry, and this is the line that answers it.
    if (best * tiles > kMaxAtlasPixels) {
        std::fprintf(stderr,
                     "world renderer: a %dpx pack over %d tiles wants a %dpx atlas, which is "
                     "past the %dpx this asks of a driver -- using %dpx tiles instead\n",
                     best, tiles, best * tiles, kMaxAtlasPixels, kTilePixels);
        return kTilePixels;
    }
    return best;
}

} // namespace

bool WorldRenderer::init(NodeRegistry &reg, const MediaSource &media)
{
    m_shader = gfx_shader_create(shaders::kWorldVS, shaders::kWorldFS);
    if (!m_shader.id) {
        std::fprintf(stderr, "world renderer: shader compilation failed\n");
        return false;
    }
    m_loc_texture     = gfx_uniform_location(m_shader, "uTexture");
    m_loc_atlas_tiles = gfx_uniform_location(m_shader, "uAtlasTiles");
    m_loc_daylight    = gfx_uniform_location(m_shader, "uDaylight");
    m_loc_time        = gfx_uniform_location(m_shader, "uTime");
    m_loc_sun_dir      = gfx_uniform_location(m_shader, "uSunDir");
    m_loc_sun_color    = gfx_uniform_location(m_shader, "uSunColor");
    m_loc_ambient      = gfx_uniform_location(m_shader, "uAmbient");
    m_loc_shadow_vp    = gfx_uniform_location(m_shader, "uShadowVP");
    m_loc_shadow       = gfx_uniform_location(m_shader, "uShadow");
    m_loc_shadow_on    = gfx_uniform_location(m_shader, "uShadowOn");
    m_loc_shadow_texel = gfx_uniform_location(m_shader, "uShadowTexel");
    m_loc_classic      = gfx_uniform_location(m_shader, "uClassic");
    m_loc_classic_day  = gfx_uniform_location(m_shader, "uClassicDay");
    m_water_u.locate(m_shader);

    m_shadow_shader = gfx_shader_create(shaders::kShadowVS, shaders::kShadowFS);
    if (!m_shadow_shader.id) {
        std::fprintf(stderr, "world renderer: shadow shader compilation failed\n");
        return false;
    }
    m_loc_shadow_tex   = gfx_uniform_location(m_shadow_shader, "uTexture");
    m_loc_shadow_tiles = gfx_uniform_location(m_shadow_shader, "uAtlasTiles");
    m_shadow_none      = gfx_shadow_create(1);
    set_shadow_size(m_shadow_size);

    m_sky_shader = gfx_shader_create(shaders::kWaterSkyVS, shaders::kWaterSkyFS);
    if (!m_sky_shader.id) {
        std::fprintf(stderr, "world renderer: water sky shader compilation failed\n");
        return false;
    }
    m_sky_water_u.locate(m_sky_shader);

    m_cloud_shader = gfx_shader_create(shaders::kWaterSkyVS, shaders::kCloudFS);
    if (!m_cloud_shader.id) {
        std::fprintf(stderr, "world renderer: cloud shader compilation failed\n");
        return false;
    }
    m_loc_cloud_drift    = gfx_uniform_location(m_cloud_shader, "uCloudDrift");
    m_loc_cloud_daylight = gfx_uniform_location(m_cloud_shader, "uDaylight");
    m_loc_cloud_sky      = gfx_uniform_location(m_cloud_shader, "uSkyColor");
    m_loc_cloud_sun      = gfx_uniform_location(m_cloud_shader, "uSunDir");
    m_loc_cloud_sun_on   = gfx_uniform_location(m_cloud_shader, "uSunShown");
    m_loc_cloud_moon     = gfx_uniform_location(m_cloud_shader, "uMoon");
    m_loc_cloud_storm    = gfx_uniform_location(m_cloud_shader, "uStorm");
    m_loc_cloud_funnel   = gfx_uniform_location(m_cloud_shader, "uFunnel");
    m_loc_cloud_range    = gfx_uniform_location(m_cloud_shader, "uRange");
    m_cloud_water_u.locate(m_cloud_shader);

    for (WaterBox &w : m_water) {
        w.px.assign((size_t)kWaterTexW * kWaterTexH * 4, 0);
        w.tex   = gfx_texture_create(w.px.data(), kWaterTexW, kWaterTexH, GFX_PF_RGBA8,
                                     GFX_FILTER_NEAREST);
        w.stale = true;
    }

    // Big enough that the screen is inside it: the corners past +1 are clipped.
    const float sky[9] = {-1.0f, -1.0f, 0.0f, 3.0f, -1.0f, 0.0f, -1.0f, 3.0f, 0.0f};
    m_sky_vb = gfx_buffer_create(sky, (int)sizeof(sky), false, false);
    gfx_vertexlayout layout{};
    layout.stride     = 3 * (int)sizeof(float);
    layout.attr_count = 1;
    layout.attrs[0]   = gfx_attr{0, 3, GFX_ATTR_F32, false, 0};
    m_sky_mesh        = gfx_mesh_create(&layout, m_sky_vb, gfx_buffer{0});

    build_atlas(reg, media);
    return m_texture.id != 0;
}

void WorldRenderer::rebuild_atlas(World &world, NodeRegistry &reg, const MediaSource &media,
                                  ItemRegistry *items)
{
    build_atlas(reg, media, items);

    // And every mesh built against the old numbering is wrong, because the
    // numbers it carries mean other pictures. Here and not at each caller: it
    // is this that moved them, and a rule spread over nine call sites is a rule
    // two of them already did not follow.
    //
    // Only when they MOVED. See build_atlas: a rebuild that lands on the same
    // numbering leaves every mesh valid, and the world's files arriving is
    // exactly that case.
    if (m_tiles_moved) world.mark_all_dirty();
}

void WorldRenderer::build_atlas(NodeRegistry &reg, const MediaSource &media,
                                ItemRegistry *items)
{
    // Nothing may be meshing while the registry moves under it. There is no
    // lock on the registry and there will not be -- a lock around node lookup is
    // a lock taken a million times per chunk -- so the rule is that a caller
    // reaches a quiet pool first. This is the caller: a mod reload rewrites the
    // node table and then asks for a new atlas.
    shared_mesh_pool().drain();

    // One pass over the media, and it happens INSIDE the planner.
    //
    // The planner asks whether a file can be had; answering means reading and
    // decoding it, so the answer is kept. Asking first and decoding afterwards
    // would read every file twice, and with a world on somebody else's server
    // "reading a file" is a walk through the media cache.
    struct Picture {
        std::vector<uint8_t> px;
        int                  size = 0;
    };
    std::unordered_map<std::string, Picture> pictures;

    std::vector<uint8_t> file_bytes;

    // What a file HOLDS, not whether it exists. The hash is over the decoded
    // pixels and not over the file's bytes -- deliberately, and the other way
    // round from ContentHash's own rule (see content_hash.h: the media cache
    // hashes bytes as they travel, because it must not decode what it stores).
    // Here the question is a different one: two names that decode to the same
    // picture must cost ONE tile, and two encodings of one picture are the case
    // a pack actually produces. An atlas holds kMaxAtlasTiles; thirty-nine blocks
    // with six faces each is 234, and identical tiles spend a budget there is
    // not much of.
    auto picture_of = [&](const std::string &file) -> std::string {
        if (!media || !media(file, file_bytes) || file_bytes.empty()) return std::string();

        int            w = 0, h = 0;
        unsigned char *decoded =
            gfx_image_decode_rgba(file_bytes.data(), (int)file_bytes.size(), &w, &h);
        if (decoded == nullptr) return std::string();

        const bool  usable = (w == h && w >= kMinTilePixels && w <= kMaxTilePixels);
        std::string id;
        if (usable) {
            Picture pic;
            pic.size = w;
            pic.px.assign(decoded, decoded + (size_t)w * (size_t)w * 4);
            // The size is in the identity as well as the pixels. Two sizes
            // cannot hold the same bytes, so this is belt and braces -- and it
            // is what stops the identity from depending on that being true.
            id = std::to_string(w) + ":" + hash_content(pic.px).hex();
            pictures.emplace(file, std::move(pic));
        } else {
            // Not a tile. Refused rather than reshaped, and refused HERE rather
            // than later, so the face falls through to the next name in its
            // chain instead of straight to a placeholder.
            std::fprintf(stderr, "world renderer: %s is %dx%d, which is not a square tile\n",
                         file.c_str(), w, h);
        }
        gfx_image_free(decoded);
        return id;
    };

    // The shape of the table. Every block asks for its own pictures: turning is
    // a byte on the cell and the mesher resolves it, so nothing here has to know
    // that a block has a front.
    std::vector<NodeShape> shapes(reg.size());
    for (size_t i = 0; i < reg.size(); ++i) shapes[i].name = reg.get((BlockId)i).name;

    // And the items, which share this atlas: one square each, asked for under
    // the item's own name. An item form of a block asks for the block's file
    // and so lands on the block's tile without anybody arranging it.
    std::vector<std::string> item_names;
    if (items) {
        item_names.resize(items->size());
        for (size_t i = 0; i < items->size(); ++i) item_names[i] = items->get((ItemId)i).name;
    }

    const TilePlan plan = plan_tiles(shapes, item_names, picture_of, kMaxAtlasTiles);

    m_tiles_used  = (int)plan.tiles.size();
    m_over_budget = plan.over_budget;
    if (m_over_budget > 0)
        std::fprintf(stderr,
                     "world renderer: %d picture(s) had no tile -- an atlas holds %d, because "
                     "a tile index is eight bits in the vertex\n",
                     m_over_budget, kMaxAtlasTiles);

    m_atlas_tiles = atlas_tiles_for(plan.tiles.size());

    // The size the pack agrees on, over the pictures that are IN the plan --
    // one vote per tile and not one per file. Six blocks pointed at one file
    // share a tile, and counting that file six times would let a pack decide
    // the atlas's resolution by how many blocks happen to wear one picture.
    std::vector<LoadedTile> chosen(plan.tiles.size());
    for (size_t t = 0; t < plan.tiles.size(); ++t) {
        if (plan.tiles[t].file.empty()) continue;
        const auto it = pictures.find(plan.tiles[t].file);
        if (it != pictures.end()) chosen[t].size = it->second.size;
    }

    m_tile_px          = agreed_tile_size(chosen, m_atlas_tiles);
    const int atlas_px = m_atlas_tiles * m_tile_px;

    m_from_file = 0;
    m_generated = 0;

    std::vector<uint8_t> pixels((size_t)atlas_px * (size_t)atlas_px * 4, 0);

    for (size_t t = 0; t < plan.tiles.size(); ++t) {
        const TileSource &src = plan.tiles[t];

        if (!src.file.empty()) {
            const auto it = pictures.find(src.file);
            if (it != pictures.end() && it->second.size == m_tile_px) {
                blit_tile(pixels, m_atlas_tiles, m_tile_px, (int)t, it->second.px.data());
                ++m_from_file;
                continue;
            }
            // A picture at a size the rest of the pack does not use is a picture
            // for an atlas this is not: said once and skipped, rather than
            // scaled into something nobody drew.
            if (it != pictures.end())
                std::fprintf(stderr,
                             "world renderer: %s is %dpx and this pack is %dpx -- skipped\n",
                             src.file.c_str(), it->second.size, m_tile_px);
        }

        NodeDef def = reg.get((BlockId)src.node);

        // Air first, and it is not a texture anybody supplies: a loud red it
        // should never be seen wearing. If that colour appears in the world, a
        // face was emitted for a block that has none.
        if (src.node == 0) {
            def.colour[0] = 220; def.colour[1] = 40; def.colour[2] = 60;
            def.checker   = false;
            const std::vector<uint8_t> tile = bake_tile(def, m_tile_px);
            blit_tile(pixels, m_atlas_tiles, m_tile_px, (int)t, tile.data());
            continue;
        }

        // The placeholder, from the SAME function the baker writes to disk --
        // so a block drawn from its file and the same block drawn without one
        // look the same, and deleting a file changes nothing but a number.
        const std::vector<uint8_t> tile = bake_tile(def, m_tile_px);
        blit_tile(pixels, m_atlas_tiles, m_tile_px, (int)t, tile.data());
        ++m_generated;
    }

    // And the numbers go back into the registry, because the mesher reads them
    // per face on its hot path. The pool was drained above and every caller
    // dirties the world after this, so nothing is meshed against the old ones.
    // Which numbers MOVED. A rebuild that lands on the same numbering leaves
    // every mesh valid -- the pictures behind the numbers may be different and
    // the texture is re-uploaded below, but a vertex saying "tile 7" still means
    // tile 7. That is the common case on the media path: the offer arrives, the
    // atlas is rebuilt from the same block table, and nothing has to be meshed
    // again. Worth telling apart, because the alternative is re-meshing the
    // whole world every time a world's files land, on the tick a lossy link is
    // busiest.
    m_tiles_moved = false;
    for (size_t i = 0; i < plan.face_tile.size(); ++i) {
        const NodeDef &was = reg.get((BlockId)i);
        for (int f = 0; f < kFaceCount; ++f)
            m_tiles_moved = m_tiles_moved || was.tex[f] != plan.face_tile[i][(size_t)f];
        reg.set_face_tiles((BlockId)i, plan.face_tile[i].data());
    }

    // The items' squares, from the same plan. Written back the same way and for
    // the same reason: the numbering is the atlas's and it moves when the atlas
    // is rebuilt.
    if (items)
        for (size_t i = 0; i < plan.item_tile.size() && i < items->size(); ++i)
            items->set_icon((ItemId)i, plan.item_tile[i]);

    if (m_texture.id) gfx_texture_destroy(m_texture);
    m_texture = gfx_texture_create(pixels.data(), atlas_px, atlas_px, GFX_PF_RGBA8,
                                   GFX_FILTER_NEAREST);
}

void WorldRenderer::set_shadow_size(int texels)
{
    if (m_shadow_rt.id && texels == m_shadow_size) return;
    if (m_shadow_rt.id) gfx_rt_destroy(m_shadow_rt);
    m_shadow_rt   = texels > 0 ? gfx_shadow_create(texels) : gfx_rt{0};
    m_shadow_size = texels;
    if (texels > 0 && !m_shadow_rt.id)
        std::fprintf(stderr, "world renderer: no %d px shadow map, drawing without\n", texels);
}

void WorldRenderer::draw_shadows(Vec3 eye, Vec3 forward)
{
    m_shadow_drawn = false;
    if (m_classic || !m_shadow_rt.id || !m_has_sky_light || !m_shadow_shader.id) return;
    if (m_sky_light.direct[0] + m_sky_light.direct[1] + m_sky_light.direct[2] <= 0.0f) return;

    // How far the map reaches from its middle, in nodes, and how far above
    // that a caster may stand. The middle is ahead of the eye: what is behind
    // it is mostly not seen.
    // ponytail: one map; a near one at higher detail (cascades) if the edges of
    // shadows on the ground near the eye look soft.
    constexpr float kReach = 56.0f;
    constexpr float kAbove = 128.0f;

    const Vec3 to_sun{m_sky_light.dir[0], m_sky_light.dir[1], m_sky_light.dir[2]};
    Vec3       ahead{forward.x, 0.0f, forward.z};
    if (length_sq(ahead) > 1e-6f) ahead = normalize(ahead) * (kReach * 0.4f);
    const Vec3 centre = eye + ahead;

    // A rotation only, about the world's origin, so the map's grid is fixed in
    // the world and a shadow's edge does not crawl as the eye moves: the
    // middle moves by whole texels.
    const Mat4  turn  = mat4_look_to(Vec3{0.0f, 0.0f, 0.0f}, to_sun * -1.0f, Vec3{0.0f, 0.0f, 1.0f});
    Vec3        c     = mat4_transform_point(turn, centre);
    const float texel = 2.0f * kReach / (float)m_shadow_size;
    c.x               = std::floor(c.x / texel) * texel;
    c.y               = std::floor(c.y / texel) * texel;
    const Mat4 ortho  = mat4_ortho(c.x - kReach, c.x + kReach, c.y - kReach, c.y + kReach,
                                   -c.z - kAbove, -c.z + kReach + 32.0f);
    m_shadow_vp       = mat4_multiply(ortho, turn);
    m_shadow_texel    = texel;

    gfx_rt_begin(m_shadow_rt);
    gfx_clip_planes_set(nullptr, 0);
    gfx_clear(1.0f, 1.0f, 1.0f, 1.0f, true);
    gfx_state st = gfx_state_default();
    st.cull      = GFX_CULL_NONE;
    gfx_state_set(&st);

    gfx_shader_bind(m_shadow_shader);
    gfx_projection_set(ortho.m);
    gfx_view_set(turn.m);
    gfx_texture_bind(m_texture, 0);
    gfx_uniform_set_int(m_loc_shadow_tex, 0);
    gfx_uniform_set_f(m_loc_shadow_tiles, (float)m_atlas_tiles);

    // Every chunk whose box can throw a shadow into the map: inside it across,
    // and anywhere between the far side and kAbove towards the sun.
    const float half = 0.5f * (float)kChunkSize;
    const float diag = half * 1.7321f;
    for (const auto &entry : m_meshes) {
        const ChunkMesh &cm = entry.second;
        const Vec3 mid = mat4_transform_point(turn, cm.origin + Vec3{half, half, half});
        if (std::fabs(mid.x - c.x) > kReach + diag || std::fabs(mid.y - c.y) > kReach + diag) continue;
        if (mid.z - c.z > kAbove + diag || c.z - mid.z > kReach + 32.0f + diag) continue;

        gfx_model_set(mat4_translate(cm.origin).m);
        if (cm.opaque.vertex_count > 0 && cm.opaque.mesh.id) gfx_draw(cm.opaque.mesh, cm.opaque.vertex_count, 0);
        if (cm.alpha.vertex_count > 0 && cm.alpha.mesh.id) gfx_draw(cm.alpha.mesh, cm.alpha.vertex_count, 0);
    }
    // ponytail: boxes, models and bodies cast no shadow yet; they would go
    // through this pass with the model shader's own depth-only twin.

    gfx_rt_end();
    const gfx_state back = gfx_state_default();
    gfx_state_set(&back);
    m_shadow_drawn = true;
}

void WorldRenderer::shutdown()
{
    if (m_shadow_rt.id) gfx_rt_destroy(m_shadow_rt);
    if (m_shadow_none.id) gfx_rt_destroy(m_shadow_none);
    if (m_shadow_shader.id) gfx_shader_destroy(m_shadow_shader);
    m_shadow_rt     = gfx_rt{0};
    m_shadow_none   = gfx_rt{0};
    m_shadow_shader = gfx_shader{0};

    if (m_parts_mesh.id) gfx_mesh_destroy(m_parts_mesh);
    if (m_parts_vb.id) gfx_buffer_destroy(m_parts_vb);
    m_parts_mesh     = gfx_mesh{0};
    m_parts_vb       = gfx_buffer{0};
    m_parts_capacity = 0;

    if (m_sky_mesh.id) gfx_mesh_destroy(m_sky_mesh);
    if (m_sky_vb.id) gfx_buffer_destroy(m_sky_vb);
    if (m_sky_shader.id) gfx_shader_destroy(m_sky_shader);
    m_sky_mesh   = gfx_mesh{0};
    m_sky_vb     = gfx_buffer{0};
    m_sky_shader = gfx_shader{0};
    if (m_cloud_shader.id) gfx_shader_destroy(m_cloud_shader);
    m_cloud_shader = gfx_shader{0};
    set_water(false);   // no shader may read a texture that is gone
    for (WaterBox &w : m_water) {
        if (w.tex.id) gfx_texture_destroy(w.tex);
        w.tex = gfx_texture{0};
    }

    // Same rule, other end: a worker is reading a registry and a snapshot that
    // are about to go away. forget() waits by itself, so there is one call here
    // and not a rule to remember.
    shared_mesh_pool().forget(this);

    for (auto &entry : m_meshes)
        for (MeshBuffer *b : {&entry.second.opaque, &entry.second.alpha, &entry.second.liquid}) {
            if (b->mesh.id) gfx_mesh_destroy(b->mesh);
            if (b->vb.id)   gfx_buffer_destroy(b->vb);
        }
    m_meshes.clear();
    if (m_texture.id) gfx_texture_destroy(m_texture);
    if (m_shader.id)  gfx_shader_destroy(m_shader);
}

void WorldRenderer::upload(MeshBuffer &cm, const std::vector<Vertex> &verts)
{
    const int count = (int)verts.size();
    const int bytes = count * (int)sizeof(Vertex);

    if (count == 0) {
        cm.vertex_count = 0;
        return;
    }

    // Reallocating only when the chunk outgrows its buffer keeps a block being
    // placed and broken repeatedly from churning GPU allocations.
    if (cm.vb.id == 0 || count > cm.capacity) {
        if (cm.mesh.id) gfx_mesh_destroy(cm.mesh);
        if (cm.vb.id)   gfx_buffer_destroy(cm.vb);

        cm.vb       = gfx_buffer_create(verts.data(), bytes, true, false);
        cm.capacity = count;

        const gfx_vertexlayout layout = world_layout();
        cm.mesh = gfx_mesh_create(&layout, cm.vb, gfx_buffer{0});
    } else {
        gfx_buffer_update(cm.vb, verts.data(), bytes, 0);
    }

    cm.vertex_count = count;
}

// The same as upload(), for the float vertices a liquid is built from.
void WorldRenderer::upload_liquid(MeshBuffer &cm, const std::vector<PartVertex> &verts)
{
    const int count = (int)verts.size();
    const int bytes = count * (int)sizeof(PartVertex);

    if (count == 0) {
        cm.vertex_count = 0;
        return;
    }

    if (cm.vb.id == 0 || count > cm.capacity) {
        if (cm.mesh.id) gfx_mesh_destroy(cm.mesh);
        if (cm.vb.id)   gfx_buffer_destroy(cm.vb);

        cm.vb       = gfx_buffer_create(verts.data(), bytes, true, false);
        cm.capacity = count;

        gfx_vertexlayout layout{};
        layout.stride     = (int)sizeof(PartVertex);
        layout.attr_count = 6;
        layout.attrs[0]   = {0, 3, GFX_ATTR_F32, false, offsetof(PartVertex, x)};
        layout.attrs[1]   = {1, 1, GFX_ATTR_U8, false, offsetof(PartVertex, face_ao)};
        layout.attrs[2]   = {2, 2, GFX_ATTR_F32, false, offsetof(PartVertex, u)};
        layout.attrs[3]   = {3, 1, GFX_ATTR_U8, false, offsetof(PartVertex, sky)};
        layout.attrs[4]   = {4, 1, GFX_ATTR_U8, false, offsetof(PartVertex, tex)};
        layout.attrs[5]   = {5, 1, GFX_ATTR_U8, false, offsetof(PartVertex, flow)};
        layout.attrs[6]   = {6, 1, GFX_ATTR_U8, false, offsetof(PartVertex, lamp)};
        layout.attrs[7]   = {7, 1, GFX_ATTR_U8, false, offsetof(PartVertex, classic)};
        layout.attr_count = 8;
        cm.mesh           = gfx_mesh_create(&layout, cm.vb, gfx_buffer{0});
    } else {
        gfx_buffer_update(cm.vb, verts.data(), bytes, 0);
    }

    cm.vertex_count = count;
}

void WorldRenderer::sync(World &world, const NodeRegistry &reg, int max_uploads_per_call,
                         double budget_ms)
{
    diag::ProfScope _("mesh");

    // Remembered for the draw, which needs to know what shape a block is and
    // takes no registry of its own: every caller syncs before it draws, and a
    // renderer that has never synced has nothing to draw either.
    m_reg = &reg;

    // Chunks the world has let go of (TODO.md OPEN-38): their meshes and their
    // place in the walk go with them. Looked for only when something was
    // erased, so a world that never lets go pays one comparison a frame.
    if (world.erased() != m_erased_seen) {
        m_erased_seen = world.erased();
        for (auto it = m_meshes.begin(); it != m_meshes.end();) {
            if (world.find_chunk(it->first) != nullptr) {
                ++it;
                continue;
            }
            for (MeshBuffer *b : {&it->second.opaque, &it->second.alpha, &it->second.liquid}) {
                if (b->mesh.id) gfx_mesh_destroy(b->mesh);
                if (b->vb.id)   gfx_buffer_destroy(b->vb);
            }
            if (!it->second.water.empty()) mark_water_stale();
            it = m_meshes.erase(it);
        }
        for (auto it = m_visibility.begin(); it != m_visibility.end();)
            it = (world.find_chunk(it->first) != nullptr) ? std::next(it) : m_visibility.erase(it);
    }

    MeshPool &pool = shared_mesh_pool();
    if (pool.workers() == 0) pool.start();

    m_last_meshed = 0;
    m_last_queued = 0;

    const auto started = std::chrono::steady_clock::now();
    auto       spent   = [&] {
        return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() -
                                                               started)
            .count();
    };

    // ------------------------------------------------------- hand the work out
    const std::vector<ChunkPos> dirty = world.take_dirty(max_uploads_per_call);

    for (size_t i = 0; i < dirty.size(); ++i) {
        // Whether the NEXT chunk fits, not whether the budget is already gone.
        //
        // Stopping at "spent >= budget" measured 7.64 ms against a 4 ms budget
        // when the build still ran here: a call sitting at 3.9 ms started one
        // more 3 ms chunk and landed at 6.9. A budget that can be overshot by
        // ninety per cent is not a budget, and the fix was not a wider threshold
        // but the question that was meant. It still guards the snapshots, which
        // are what this call pays for now.
        //
        // The estimate is this call's own chunks and not a running average
        // across calls: what a chunk costs depends on what is in it, and the
        // chunks in one storm resemble each other far more than they resemble
        // last frame's.
        //
        // Never checked before the FIRST chunk, so a budget smaller than one
        // chunk builds the world slowly rather than never.
        const double so_far = spent();
        if (budget_ms > 0.0 && i > 0 && so_far + so_far / (double)i > budget_ms) {
            // Back on the queue. take_dirty cleared the flag, so a chunk dropped
            // here is a chunk that stays stale until something happens to touch
            // it.
            for (size_t j = i; j < dirty.size(); ++j) world.mark_dirty(dirty[j]);
            break;
        }

        const ChunkPos &p = dirty[i];
        {
            // Reads the World, so it stays here. It copies everything the
            // mesher reads, which is what lets the world go on being edited
            // while a worker is looking at the copy.
            diag::ProfScope snap("snapshot");
            snapshot_chunk_into(world, p, m_scratch_snapshot);
        }

        // Swaps a spare buffer back into m_scratch_snapshot, so this allocates
        // nothing after the first few frames.
        if (pool.submit(this, p, m_scratch_snapshot, reg) != 0) ++m_last_queued;
        else world.mark_dirty(p);
    }

    // A caller that asked for no budget wants a built world when this returns,
    // which is what every gate that makes a scene and then measures it needs.
    if (budget_ms <= 0.0) pool.drain();

    // ------------------------------------------------------- take the work back
    while (pool.try_take(this, m_scratch_result)) {
        const ChunkPos            &p     = m_scratch_result.pos;
        const std::vector<Vertex> &verts = m_scratch_result.opaque;
        const std::vector<Vertex> &alpha = m_scratch_result.alpha;

        ++m_last_meshed;

        // Meshed from a snapshot of a chunk the world has let go of since.
        if (world.find_chunk(p) == nullptr) continue;

        // Before the empty-chunk skip below, and that ordering is the point: a
        // chunk of pure air has no mesh and never will, and it is exactly the
        // chunk the walk has to be able to cross.
        m_visibility[p] = m_scratch_result.visibility;

        auto it = m_meshes.find(p);
        if (it == m_meshes.end()) {
            // Empty chunk: no entry, no draw call. Both streams have to be
            // empty, or a chunk of nothing but glass would never get one.
            // Both streams AND the boxes: a chunk holding nothing but a chest
            // has no cube vertices at all, and dropping it here would be a
            // chest that is never drawn.
            if (verts.empty() && alpha.empty() && m_scratch_result.parts.empty() &&
                m_scratch_result.liquids.empty())
                continue;
            ChunkMesh cm;
            cm.origin = Vec3{(float)(p.x * kChunkSize),
                             (float)(p.y * kChunkSize),
                             (float)(p.z * kChunkSize)};
            it = m_meshes.emplace(p, cm).first;
        }
        {
            // A GL call, so it can never leave this thread whatever else does.
            diag::ProfScope up("upload");
            upload(it->second.opaque, verts);
            upload(it->second.alpha, alpha);
            it->second.parts = m_scratch_result.parts;
            it->second.pieces = false;
            for (const PartCell &cell : it->second.parts)
                if (reg.get(cell.block).piece.plan != 0) it->second.pieces = true;

            m_scratch_liquid.clear();
            for (const LiquidCell &cell : m_scratch_result.liquids)
                emit_liquid_cell(cell, reg.get(cell.block), m_scratch_liquid);
            upload_liquid(it->second.liquid, m_scratch_liquid);

            std::vector<WaterCell> &water = it->second.water;
            if (!water.empty() || !m_scratch_result.liquids.empty()) mark_water_stale();
            water.clear();
            for (const LiquidCell &c : m_scratch_result.liquids)
                water.push_back({c.x, c.y, c.z, {c.h[0], c.h[1], c.h[2], c.h[3]}});
        }

        if (budget_ms > 0.0 && spent() >= budget_ms) break;
    }

    m_total_vertices = 0;
    for (const auto &entry : m_meshes)
        m_total_vertices += (size_t)entry.second.opaque.vertex_count +
                            (size_t)entry.second.alpha.vertex_count +
                            (size_t)entry.second.liquid.vertex_count;

    m_last_sync_ms = spent();
}

bool WorldRenderer::catching_up() const
{
    return shared_mesh_pool().in_flight(this) > 0;
}

// The eight corners of a box, per face, in the winding the world's own quads
// use: two triangles, counter-clockwise seen from outside.
void emit_node_box(const NodeDef::Box &box, const NodeDef &def, float wx, float wy, float wz,
                   uint8_t turn, uint8_t light, float open, std::vector<PartVertex> &out)
{
    // The cell's orientation, which for most blocks is the byte and for a
    // wallmounted one is a face (registry/node_registry.h). One of twenty-four
    // since OPEN-27 closed: the boxes are turned by the SAME rotation the
    // pictures are, as integers, because a box is corners in the cell and a
    // table of face permutations cannot turn one.
    const int o = node_cell_orientation(def.turn, turn);
    int       rot[3][3];
    orientation_matrix(o, rot);

    // About the middle of the node and not about a corner: a box turned about
    // its corner leaves the cell.
    // A named box swings, and it swings about its own back-bottom edge: the
    // lid of a chest lifts at the front and stays put at the hinge. Before the
    // turn below, in the block's unturned space, so a chest facing east opens
    // eastwards and not northwards.
    const bool  swings = !box.part.empty() && open > 0.0f;
    const float angle  = -open * 1.65f;   // a little past a right angle, thrown wide
    const float ha     = std::cos(angle), hb = std::sin(angle);

    const auto place = [&](float x, float y, float z, PartVertex &v) {
        if (swings) {
            const float dy = y - box.min[1];
            const float dz = z - box.min[2];
            y = box.min[1] + dy * ha - dz * hb;
            z = box.min[2] + dy * hb + dz * ha;
        }

        // About the middle of the cell in all three axes now. It was about the
        // vertical through the middle while a turn could only be about the
        // vertical, and for those four turns this is the same arithmetic: y
        // comes back to itself.
        const float px = x - 0.5f, py = y - 0.5f, pz = z - 0.5f;
        v.x = wx + 0.5f + (float)rot[0][0] * px + (float)rot[0][1] * py + (float)rot[0][2] * pz;
        v.y = wy + 0.5f + (float)rot[1][0] * px + (float)rot[1][1] * py + (float)rot[1][2] * pz;
        v.z = wz + 0.5f + (float)rot[2][0] * px + (float)rot[2][1] * py + (float)rot[2][2] * pz;
    };

    const float x0 = box.min[0], y0 = box.min[1], z0 = box.min[2];
    const float x1 = box.max[0], y1 = box.max[1], z1 = box.max[2];

    // Per face: four corners anticlockwise from outside, and the two texture
    // coordinates that go with them. The tile is the one the block wears on
    // THAT face before any turn, because here it is the geometry that turns.
    struct Corner { float x, y, z, u, v; };
    const Corner faces[6][4] = {
        // +X
        {{x1, y0, z1, z1, y0}, {x1, y0, z0, z0, y0}, {x1, y1, z0, z0, y1}, {x1, y1, z1, z1, y1}},
        // -X
        {{x0, y0, z0, z0, y0}, {x0, y0, z1, z1, y0}, {x0, y1, z1, z1, y1}, {x0, y1, z0, z0, y1}},
        // +Y
        {{x0, y1, z1, x0, z1}, {x1, y1, z1, x1, z1}, {x1, y1, z0, x1, z0}, {x0, y1, z0, x0, z0}},
        // -Y
        {{x0, y0, z0, x0, z0}, {x1, y0, z0, x1, z0}, {x1, y0, z1, x1, z1}, {x0, y0, z1, x0, z1}},
        // +Z
        {{x0, y0, z1, x0, y0}, {x1, y0, z1, x1, y0}, {x1, y1, z1, x1, y1}, {x0, y1, z1, x0, y1}},
        // -Z
        {{x1, y0, z0, x1, y0}, {x0, y0, z0, x0, y0}, {x0, y1, z0, x0, y1}, {x1, y1, z0, x1, y1}},
    };

    for (int f = 0; f < kFaceCount; ++f) {
        // Which way this face ends up looking, which is what the shader shades
        // by: a lid's top has to stay the bright face whichever way the chest
        // was put down.
        const uint8_t shade_face = (uint8_t)oriented_face_24(f, o);

        PartVertex quad[4];
        for (int c = 0; c < 4; ++c) {
            const Corner &k = faces[f][c];
            place(k.x, k.y, k.z, quad[c]);
            quad[c].u       = k.u;
            quad[c].v       = k.v;
            quad[c].face_ao = pack_face_ao(shade_face, 3, def.tex[f]);   // no ambient occlusion on a box
            quad[c].set_light(light);
            quad[c].tex     = (uint8_t)(def.tex[f] & 0xFF);
        }

        const int order[6] = {0, 1, 2, 0, 2, 3};
        for (const int i : order) out.push_back(quad[i]);
    }
}

// A track piece's road, curbs and sides (core/track/piece.h), from the cell of
// its corner. The triangles are made once for each shape and turn; each frame
// only moves them to where the piece is. The curbs are striped by shading
// every other segment darker, which needs no picture of their own.
// ponytail: drawn when the corner's chunk is, so a piece whose corner is off
// screen pops at the edge of the view; cull by the footprint if that shows.
void emit_piece(const NodeDef &def, float wx, float wy, float wz, uint8_t facing, uint8_t light,
                std::vector<PartVertex> &out)
{
    static std::map<std::array<int, 11>, std::vector<PieceTri>> made;
    const PieceShape  &p = def.piece;
    const std::array<int, 11> key{p.plan, p.size, p.climb, p.grade0, p.grade1,
                                  p.bank0, p.bank1, p.curbs, p.wall, p.width, facing};
    auto it = made.find(key);
    if (it == made.end()) {
        std::vector<PieceTri> tris;
        piece_triangles(p, facing, tris);
        it = made.emplace(key, std::move(tris)).first;
    }

    for (const PieceTri &t : it->second) {
        const Vec3 n = cross(t.v[1] - t.v[0], t.v[2] - t.v[0]);
        int        face;
        if (n.y > 0.5f * std::sqrt(dot(n, n)))
            face = kFaceTop;
        else if (std::fabs(n.x) > std::fabs(n.z))
            face = n.x > 0.0f ? kFaceRight : kFaceLeft;
        else
            face = n.z > 0.0f ? kFaceFront : kFaceBack;

        int ao = 3;
        if (t.role == kPieceCurb) {
            ao = t.stripe ? 1 : 3;
        } else if (t.role == kPieceSide) {
            ao = 2;
        }
        const int tile = t.role == kPieceRoad ? kFaceTop : t.role == kPieceCurb ? kFaceBottom : kFaceRight;

        for (const Vec3 &c : t.v) {
            PartVertex v{};
            v.x       = wx + c.x;
            v.y       = wy + c.y;
            v.z       = wz + c.z;
            v.u       = face == kFaceTop ? c.x : (face == kFaceRight || face == kFaceLeft) ? c.z : c.x;
            v.v       = face == kFaceTop ? c.z : c.y;
            v.face_ao = pack_face_ao(face, ao, def.tex[tile]);
            v.set_light(light);
            v.tex     = (uint8_t)(def.tex[tile] & 0xFF);
            out.push_back(v);
        }
    }
}

void emit_liquid_cell(const LiquidCell &cell, const NodeDef &def, std::vector<PartVertex> &out)
{
    const float x0 = (float)cell.x, y0 = (float)cell.y, z0 = (float)cell.z;
    const float x1 = x0 + 1.0f, z1 = z0 + 1.0f;

    // The top at each corner, indexed x + 2 z as the mesher left it.
    const auto top = [&](int cx, int cz) { return y0 + (float)cell.h[cx + 2 * cz] / 16.0f; };
    const float t00 = top(0, 0), t10 = top(1, 0), t01 = top(0, 1), t11 = top(1, 1);

    // Which way it runs. Water that is not full is running off something, and
    // its picture moves: across the top, downhill -- the way the corners fall --
    // and straight down its sides. A full cell (a lake's source) lies still.
    const bool running = def.liquid_fill < 8;
    uint8_t    across  = 0;
    if (running) {
        const float gx = (t10 + t11) - (t00 + t01);   // how much higher +x is
        const float gz = (t01 + t11) - (t00 + t10);   // and +z
        if (std::fabs(gx) + std::fabs(gz) > 0.01f) {
            const float a    = std::atan2(-gz, -gx);  // downhill
            const int   step = ((int)std::lround(a / 0.785398f) + 8) % 8;
            across           = (uint8_t)(1 + step);
        }
    }

    // Anticlockwise from outside, the winding every other quad here uses, with
    // the texture coordinates a box face of the same orientation would take:
    // the picture is not stretched up a sloping side, it is cut off by it.
    struct Corner { float x, y, z, u, v; };
    const Corner faces[6][4] = {
        // +X
        {{x1, y0, z1, z1, y0}, {x1, y0, z0, z0, y0}, {x1, t10, z0, z0, t10}, {x1, t11, z1, z1, t11}},
        // -X
        {{x0, y0, z0, z0, y0}, {x0, y0, z1, z1, y0}, {x0, t01, z1, z1, t01}, {x0, t00, z0, z0, t00}},
        // +Y
        {{x0, t01, z1, x0, z1}, {x1, t11, z1, x1, z1}, {x1, t10, z0, x1, z0}, {x0, t00, z0, x0, z0}},
        // -Y
        {{x0, y0, z0, x0, z0}, {x1, y0, z0, x1, z0}, {x1, y0, z1, x1, z1}, {x0, y0, z1, x0, z1}},
        // +Z
        {{x0, y0, z1, x0, y0}, {x1, y0, z1, x1, y0}, {x1, t11, z1, x1, t11}, {x0, t01, z1, x0, t01}},
        // -Z
        {{x1, y0, z0, x1, y0}, {x0, y0, z0, x0, y0}, {x0, t00, z0, x0, t00}, {x1, t10, z0, x1, t10}},
    };

    // Which of `h`'s corners each of the top's vertices is.
    static const int kTopCorner[4] = {2, 3, 1, 0};

    for (int f = 0; f < kFaceCount; ++f) {
        if (!(cell.faces & (1u << f))) continue;

        const uint8_t flow = (f == 2) ? across : (f == 3 || !running) ? (uint8_t)0 : (uint8_t)9;

        PartVertex quad[4];
        for (int c = 0; c < 4; ++c) {
            const Corner &k = faces[f][c];
            quad[c].x       = k.x;
            quad[c].y       = k.y;
            quad[c].z       = k.z;
            quad[c].u       = k.u;
            quad[c].v       = k.v;
            quad[c].face_ao = pack_face_ao(f, 3, def.tex[f]);
            quad[c].set_light(f == 2 ? cell.top_light[kTopCorner[c]] : cell.light[f]);
            quad[c].tex     = (uint8_t)(def.tex[f] & 0xFF);
            quad[c].flow    = flow;
        }

        // Split along the diagonal whose ends differ LEAST, so a surface that
        // slopes one way at one corner folds along the right edge instead of
        // making a ridge across the middle of the cell.
        const float d02 = std::fabs(quad[0].y - quad[2].y);
        const float d13 = std::fabs(quad[1].y - quad[3].y);
        const int   a[6] = {0, 1, 2, 0, 2, 3};
        const int   b[6] = {1, 2, 3, 1, 3, 0};
        const int  *order = d13 < d02 ? b : a;
        for (int i = 0; i < 6; ++i) out.push_back(quad[order[i]]);

        // The top from underneath as well. Faces are culled from behind, and a
        // swimmer looking up at the surface is behind it: without this the
        // surface is not there from below at all and the sky shows through a
        // lake. The same triangles, wound the other way.
        if (f == 2)
            for (int i = 5; i >= 0; --i) out.push_back(quad[order[i]]);
    }
}

void WorldRenderer::draw_parts(const NodeRegistry &reg, const Mat4 &projection,
                               const Mat4 &view)
{
    m_parts_drawn = 0;
    m_part_vertices.clear();
    m_part_see.clear();
    m_part_see_count = 0;
    m_model_cells.clear();

    for (const ChunkMesh *cm : m_part_chunks)
        for (const PartCell &cell : cm->parts) {
            const NodeDef &def = reg.get(cell.block);

            // A cell that wears a model is drawn as the model and not as its
            // boxes: the boxes are where a body may go (registry/node_shape.h)
            // and the model is the picture. A world whose client has no model
            // to draw falls through to the boxes, which is a shape rather than
            // a hole -- and to nothing at all if it has none, because the one
            // thing it is not is a cube.
            if (def.model != kNoModel && m_draw_model) {
                const float wx = cm->origin.x + (float)cell.x;
                const float wy = cm->origin.y + (float)cell.y;
                const float wz = cm->origin.z + (float)cell.z;

                // Turned about the middle of the cell, which is where a box is
                // turned: a model turned about a corner leaves its cell. The
                // rotation is the integer one the mesher uses, so what a player
                // walks into and what they see agree about which way it faces.
                const int o = node_cell_orientation(def.turn, cell.facing);
                int       rot[3][3];
                orientation_matrix(o, rot);

                Mat4 place = mat4_identity();
                for (int r = 0; r < 3; ++r)
                    for (int c = 0; c < 3; ++c) place.at(r, c) = (float)rot[r][c];

                // T(cell + half) * R * T(-half): the cell's own middle, and not
                // the world's origin.
                const Mat4 to_cell   = mat4_translate(Vec3{wx + 0.5f, wy + 0.5f, wz + 0.5f});
                const Mat4 from_mid  = mat4_translate(Vec3{-0.5f, -0.5f, -0.5f});
                // Collected and not drawn here: a model binds its own shader,
                // and the boxes below are drawn in one call afterwards with
                // this pass's shader still bound. Drawing a model in the middle
                // of the loop would leave that call painting with a shader that
                // knows nothing about an atlas.
                m_model_cells.push_back(
                    {def.model, mat4_multiply(mat4_multiply(to_cell, place), from_mid),
                     m_open_at ? m_open_at((int)wx, (int)wy, (int)wz) : 0.0f,
                     // The world shader's rule, said once more here because a
                     // model is drawn by the model shader.
                     cell_light(cell.light, false)});
                ++m_parts_drawn;
                continue;
            }

            if (def.piece.plan != 0) {
                ++m_parts_drawn;
                emit_piece(def, cm->origin.x + (float)cell.x, cm->origin.y + (float)cell.y,
                           cm->origin.z + (float)cell.z, cell.facing, cell.light, m_part_vertices);
                continue;
            }

            if (def.parts.empty()) continue;   // the table changed under the mesh

            ++m_parts_drawn;

            const float open =
                m_open_at ? m_open_at((int)(cm->origin.x + (float)cell.x),
                                      (int)(cm->origin.y + (float)cell.y),
                                      (int)(cm->origin.z + (float)cell.z))
                          : 0.0f;

            std::vector<PartVertex> &into = reg.is_translucent(cell.block) ? m_part_see : m_part_vertices;
            for (const NodeDef::Box &box : def.parts)
                emit_node_box(box, def, cm->origin.x + (float)cell.x,
                              cm->origin.y + (float)cell.y, cm->origin.z + (float)cell.z,
                              cell.facing, cell.light, open, into);
        }

    // The models, after the boxes of this frame have been handed over and
    // before anything else binds a shader -- see the loop above for why they
    // are not drawn where they are found.
    const auto draw_collected_models = [&]() {
        for (const ModelCell &mc : m_model_cells)
            m_draw_model(mc.model, mc.place, mc.open, mc.light, projection, view);
    };

    const int solid = (int)m_part_vertices.size();
    m_part_vertices.insert(m_part_vertices.end(), m_part_see.begin(), m_part_see.end());
    m_part_see_first = solid;
    m_part_see_count = (int)m_part_see.size();
    if (m_part_vertices.empty()) {
        draw_collected_models();
        return;
    }

    // One buffer, grown when it has to be and reused when it does not. A
    // dynamic buffer rewritten every frame is what this is for.
    const int need = (int)m_part_vertices.size();
    if (need > m_parts_capacity || m_parts_vb.id == 0) {
        if (m_parts_mesh.id) gfx_mesh_destroy(m_parts_mesh);
        if (m_parts_vb.id) gfx_buffer_destroy(m_parts_vb);

        m_parts_capacity = need * 2;
        m_parts_vb = gfx_buffer_create(nullptr, m_parts_capacity * (int)sizeof(PartVertex), true,
                                       false);

        gfx_vertexlayout layout{};
        layout.stride     = (int)sizeof(PartVertex);
        layout.attr_count = 5;
        layout.attrs[0]   = {0, 3, GFX_ATTR_F32, false, offsetof(PartVertex, x)};
        layout.attrs[1]   = {1, 1, GFX_ATTR_U8, false, offsetof(PartVertex, face_ao)};
        layout.attrs[2]   = {2, 2, GFX_ATTR_F32, false, offsetof(PartVertex, u)};
        layout.attrs[3]   = {3, 1, GFX_ATTR_U8, false, offsetof(PartVertex, sky)};
        layout.attrs[4]   = {4, 1, GFX_ATTR_U8, false, offsetof(PartVertex, tex)};
        layout.attrs[5]   = {6, 1, GFX_ATTR_U8, false, offsetof(PartVertex, lamp)};
        layout.attrs[6]   = {7, 1, GFX_ATTR_U8, false, offsetof(PartVertex, classic)};
        layout.attr_count = 7;
        m_parts_mesh      = gfx_mesh_create(&layout, m_parts_vb, gfx_buffer{0});
    }

    gfx_buffer_update(m_parts_vb, m_part_vertices.data(),
                      need * (int)sizeof(PartVertex), 0);

    // Positions are already in the world, so the model matrix is nothing. The
    // same shader, the same atlas, the same daylight: a box is lit and shaded
    // by the rules a cube is.
    const Mat4 identity = mat4_identity();
    gfx_model_set(identity.m);
    if (solid > 0) {
        gfx_draw(m_parts_mesh, solid, 0);
        ++m_draw_calls;
    }

    draw_collected_models();
}

void WorldRenderer::refresh_water()
{
    WaterBox &w = m_water[m_water_slot];

    // Centred on the eye, and moved only once the eye is a quarter of the box
    // from its middle: every line a pixel sees starts at the eye, and the
    // quarter it may have gone is still far from any edge.
    int  want[3];
    bool moved = false;
    for (int k = 0; k < 3; ++k) {
        const float e = k == 0 ? m_water_eye.x : k == 1 ? m_water_eye.y : m_water_eye.z;
        want[k]       = (int)std::floor(e) - kWaterBox[k] / 2;
        if (std::abs(want[k] - w.box[k]) > kWaterBox[k] / 4) moved = true;
    }
    if (!moved && !w.stale) return;
    if (moved)
        for (int k = 0; k < 3; ++k) w.box[k] = want[k];
    w.stale = false;
    w.any   = false;
    for (int k = 0; k < 3; ++k) {
        w.lo[k] = kWaterBox[k];
        w.hi[k] = 0;
    }

    std::fill(w.px.begin(), w.px.end(), (uint8_t)0);
    const auto chunk_of = [](int v) { return (int)std::floor((float)v / (float)kChunkSize); };
    for (int cz = chunk_of(w.box[2]); cz <= chunk_of(w.box[2] + kWaterBox[2] - 1); ++cz)
        for (int cy = chunk_of(w.box[1]); cy <= chunk_of(w.box[1] + kWaterBox[1] - 1); ++cy)
            for (int cx = chunk_of(w.box[0]); cx <= chunk_of(w.box[0] + kWaterBox[0] - 1);
                 ++cx) {
                const auto it = m_meshes.find(ChunkPos{cx, cy, cz});
                if (it == m_meshes.end()) continue;
                for (const WaterCell &c : it->second.water) {
                    const int x = cx * kChunkSize + c.x - w.box[0];
                    const int y = cy * kChunkSize + c.y - w.box[1];
                    const int z = cz * kChunkSize + c.z - w.box[2];
                    if (x < 0 || y < 0 || z < 0 || x >= kWaterBox[0] || y >= kWaterBox[1] ||
                        z >= kWaterBox[2])
                        continue;
                    // The layout water_corners reads: layer y is tile (y % 8, y / 8).
                    const int u = (y % 8) * kWaterBox[0] + x, v = (y / 8) * kWaterBox[2] + z;
                    uint8_t  *px = &w.px[((size_t)v * kWaterTexW + u) * 4];
                    for (int i = 0; i < 4; ++i) px[i] = c.h[i];
                    w.any = true;
                    const int at[3] = {x, y, z};
                    for (int k = 0; k < 3; ++k) {
                        w.lo[k] = std::min(w.lo[k], at[k]);
                        w.hi[k] = std::max(w.hi[k], at[k] + 1);
                    }
                }
            }
    gfx_texture_update(w.tex, w.px.data(), 0, 0, kWaterTexW, kWaterTexH, GFX_PF_RGBA8);
}

namespace {

// The renderer whose water is on (WorldRenderer::set_water): the world the
// player's own camera is in, or none.
const WorldRenderer *g_water_world = nullptr;

} // namespace

void WorldRenderer::set_water(bool on, Vec3 eye, int slot)
{
    m_water_on   = on;
    m_water_eye  = eye;
    m_water_slot = slot < 0 || slot >= kWaterSlots ? 0 : slot;
    if (on)
        g_water_world = this;
    else if (g_water_world == this)
        g_water_world = nullptr;
}

void WaterUniforms::locate(gfx_shader shader)
{
    water  = gfx_uniform_location(shader, "uWater");
    lo     = gfx_uniform_location(shader, "uWaterLo");
    hi     = gfx_uniform_location(shader, "uWaterHi");
    colour = gfx_uniform_location(shader, "uWaterColor");
    cells  = gfx_uniform_location(shader, "uWaterCells");
    eye    = gfx_uniform_location(shader, "uWaterEye");
    near   = gfx_uniform_location(shader, "uWaterNear");
    ray_x  = gfx_uniform_location(shader, "uWaterRayX");
    ray_y  = gfx_uniform_location(shader, "uWaterRayY");
    ray_z  = gfx_uniform_location(shader, "uWaterRayZ");
}

void WaterUniforms::off() const
{
    static const float none[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    gfx_uniform_set_v4(water, none, 1);
}

void WaterUniforms::set(const Mat4 &projection, const Mat4 &view) const
{
    const WorldRenderer *world = g_water_world;
    if (!world || !world->m_water[world->m_water_slot].any) {
        off();
        return;
    }
    const WorldRenderer::WaterBox &w = world->m_water[world->m_water_slot];

    const float box[4]  = {(float)w.box[0], (float)w.box[1], (float)w.box[2], 1.0f};
    const float from[4] = {(float)w.lo[0], (float)w.lo[1], (float)w.lo[2], 0.0f};
    const float to[4]   = {(float)w.hi[0], (float)w.hi[1], (float)w.hi[2], 0.0f};
    // Its colour dims with the day like everything under the sky.
    const float tint   = 0.25f + 0.75f * world->m_daylight;
    const float rgb[4] = {0.10f * tint, 0.27f * tint, 0.45f * tint, 1.0f};
    gfx_uniform_set_v4(water, box, 1);
    gfx_uniform_set_v4(lo, from, 1);
    gfx_uniform_set_v4(hi, to, 1);
    gfx_uniform_set_v4(colour, rgb, 1);

    // The camera, out of the matrices. The eye is -R^T t for V = R * T(-eye),
    // and R's rows are the camera's right, up and back -- so the point at clip
    // position (x, y, _, d) is eye + R^T (x/d / P00, y/d / P11, -1) d, which is
    // water_fade's line. A portal's oblique projection changes only the third
    // row, so that still holds.
    //
    // And the near plane, where the line begins, in the world: clip z + w >= 0,
    // rows 2 and 3 of P V. Not a distance along the view, which is what it was:
    // a portal's near plane is the far opening, tilted, and the distance read
    // off an oblique projection was nowhere near it.
    float plane[4];
    for (int k = 0; k < 4; ++k) {
        plane[k] = 0.0f;
        for (int j = 0; j < 4; ++j)
            plane[k] += (projection.at(2, j) + projection.at(3, j)) * view.at(j, k);
    }
    float at[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    for (int k = 0; k < 3; ++k)
        at[k] = -(view.at(0, k) * view.at(0, 3) + view.at(1, k) * view.at(1, 3) +
                  view.at(2, k) * view.at(2, 3));
    const float x[4] = {view.at(0, 0) / projection.at(0, 0), view.at(0, 1) / projection.at(0, 0),
                        view.at(0, 2) / projection.at(0, 0), 0.0f};
    const float y[4] = {view.at(1, 0) / projection.at(1, 1), view.at(1, 1) / projection.at(1, 1),
                        view.at(1, 2) / projection.at(1, 1), 0.0f};
    const float z[4] = {-view.at(2, 0), -view.at(2, 1), -view.at(2, 2), 0.0f};
    gfx_uniform_set_v4(eye, at, 1);
    gfx_uniform_set_v4(near, plane, 1);
    gfx_uniform_set_v4(ray_x, x, 1);
    gfx_uniform_set_v4(ray_y, y, 1);
    gfx_uniform_set_v4(ray_z, z, 1);

    gfx_texture_bind(w.tex, 1);
    gfx_uniform_set_int(cells, 1);
    gfx_texture_bind(gfx_texture{0}, 0);
}

void WorldRenderer::bind_world(const Mat4 &projection, const Mat4 &view)
{
    gfx_shader_bind(m_shader);
    gfx_projection_set(projection.m);
    gfx_view_set(view.m);

    // The sun, and its map on unit 2 -- before the water, which leaves
    // unit 0 active.
    const SkyLight none{};
    const SkyLight &sl = m_has_sky_light ? m_sky_light : none;
    gfx_uniform_set_v3(m_loc_sun_dir, sl.dir);
    gfx_uniform_set_v3(m_loc_sun_color, sl.direct);
    gfx_uniform_set_v3(m_loc_ambient, sl.ambient);
    const bool shadows = m_shadow_drawn && m_has_sky_light;
    gfx_texture_bind(gfx_rt_depth(shadows ? m_shadow_rt : m_shadow_none), 2);
    gfx_uniform_set_int(m_loc_shadow, 2);
    gfx_uniform_set_f(m_loc_shadow_on, shadows ? 1.0f : 0.0f);
    gfx_uniform_set_f(m_loc_shadow_texel, m_shadow_texel);
    gfx_uniform_set_mat4(m_loc_shadow_vp, m_shadow_vp.m);
    gfx_uniform_set_f(m_loc_classic, m_classic ? 1.0f : 0.0f);
    gfx_uniform_set_f(m_loc_classic_day, m_classic_day);

    // The water first, so the unit left active is 0, the one everything
    // drawn after this binds to without saying.
    m_water_u.set(projection, view);
    gfx_texture_bind(m_texture, 0);
    gfx_uniform_set_int(m_loc_texture, 0);
    gfx_uniform_set_f(m_loc_atlas_tiles, (float)m_atlas_tiles);
    gfx_uniform_set_f(m_loc_daylight, m_daylight);

    // Wrapped, because a float of seconds loses the fractions a moving
    // picture is made of after a few hours. A hundred seconds is a whole
    // number of tiles for the straight runs and the falling ones, so the
    // wrap does not show there; a diagonal run jumps a fraction of a tile
    // once every hundred seconds.
    static const auto start = std::chrono::steady_clock::now();
    const double      t = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
    gfx_uniform_set_f(m_loc_time, (float)std::fmod(t, 100.0));
}

void WorldRenderer::draw(const Mat4 &projection, const Mat4 &view, bool cull)
{
    // This pass's shader and everything it reads. A function and not four lines
    // at the top, because it is needed twice: a block that wears a MODEL is
    // drawn by the model's own shader in the middle of this pass, and whatever
    // came after it -- every translucent surface in sight -- was drawn with
    // THAT shader still bound, which puts glass and water nowhere. Reported
    // from play as a pool that vanished for some tilts of the camera: the tilts
    // that had a model block (mods/steps' marker) somewhere in the frame.
    //
    // The water in sight (CNP_WATER_GLSL), for this pass, the sky, and every
    // other shader with WaterUniforms drawn while it is on.
    if (m_water_on) refresh_water();


    // The sky behind the water: the pixels no block covered, found by the depth
    // they were cleared to. After the opaque pass and not before it, so a pixel
    // a block covers does not walk the water for nothing; before the
    // translucent one, which is faded by its own fragments.
    const auto fade_sky = [&]() {
        if (!m_water_on || !m_water[m_water_slot].any || !m_sky_mesh.id) return;
        gfx_shader_bind(m_sky_shader);
        gfx_projection_set(projection.m);
        gfx_view_set(view.m);
        m_sky_water_u.set(projection, view);

        gfx_state sky   = gfx_state_default();
        sky.depth_write = false;
        sky.cull        = GFX_CULL_NONE;
        sky.blend       = GFX_BLEND_ALPHA;
        gfx_state_set(&sky);
        gfx_draw(m_sky_mesh, 3, 0);

        const gfx_state back = gfx_state_default();
        gfx_state_set(&back);
        gfx_texture_bind(m_texture, 0);   // unit 0 active again, as bind_world leaves it
    };

    // The clouds, after the opaque pass so their depth is tested against the
    // ground, and before the translucent one so a lake behind one is hidden by
    // it. The water in front of them they fade themselves: the pixel is no
    // longer at the cleared depth fade_sky looks for.
    const auto draw_clouds = [&]() {
        if (!m_clouds || !m_cloud_shader.id || !m_sky_mesh.id) return;
        gfx_shader_bind(m_cloud_shader);
        gfx_projection_set(projection.m);
        gfx_view_set(view.m);
        m_cloud_water_u.set(projection, view);
        gfx_uniform_set_f(m_loc_cloud_drift, m_cloud_drift);
        gfx_uniform_set_f(m_loc_cloud_daylight, daylight());
        gfx_uniform_set_v4(m_loc_cloud_sky, m_cloud_sky, 1);
        gfx_uniform_set_v3(m_loc_cloud_sun, m_sky_light.dir);
        gfx_uniform_set_f(m_loc_cloud_sun_on, m_has_sky_light && !m_classic ? 1.0f : 0.0f);
        gfx_uniform_set_f(m_loc_cloud_moon, m_sky_light.moon ? 1.0f : 0.0f);
        gfx_uniform_set_v4(m_loc_cloud_storm, m_cloud_storm, 1);
        gfx_uniform_set_v4(m_loc_cloud_funnel, m_cloud_funnel, 4);
        gfx_uniform_set_f(m_loc_cloud_range, std::min(m_view_range, 1e6f));

        gfx_state sky = gfx_state_default();
        sky.cull      = GFX_CULL_NONE;
        sky.blend     = GFX_BLEND_ALPHA;
        gfx_state_set(&sky);
        gfx_draw(m_sky_mesh, 3, 0);

        const gfx_state back = gfx_state_default();
        gfx_state_set(&back);
        gfx_texture_bind(m_texture, 0);
    };
    bind_world(projection, view);

    const Frustum frustum = frustum_from_view_projection(mat4_multiply(projection, view));

    // Where the camera is, read back out of the view matrix rather than taken
    // as an argument.
    //
    // This is what makes the sort correct inside a portal for free. The alpha
    // pass has to run back to front from the camera doing the looking, and the
    // camera doing the looking through a portal is the VIRTUAL one -- which
    // this function already receives, because the whole view matrix is the
    // argument. Passing an eye position separately would let the two disagree,
    // and the case where they disagree is exactly the portal pass.
    //
    // For V = R * T(-eye), the translation column is -R*eye, so eye = -R^T * t.
    const Vec3 eye = Vec3{
        -(view.at(0, 0) * view.at(0, 3) + view.at(1, 0) * view.at(1, 3) +
          view.at(2, 0) * view.at(2, 3)),
        -(view.at(0, 1) * view.at(0, 3) + view.at(1, 1) * view.at(1, 3) +
          view.at(2, 1) * view.at(2, 3)),
        -(view.at(0, 2) * view.at(0, 3) + view.at(1, 2) * view.at(1, 3) +
          view.at(2, 2) * view.at(2, 3))};

    m_last_sort_eye = eye;
    m_draw_calls    = 0;
    m_culled        = 0;
    m_range_culled  = 0;
    m_occluded      = 0;
    m_alpha_order.clear();
    m_opaque_order.clear();
    m_part_chunks.clear();

    const float half = 0.5f * (float)kChunkSize;

    // ------------------------------------------------------------- the walk
    //
    // Occlusion culling, and it is a walk rather than a test because visibility
    // is not a property a chunk has on its own. A chunk is visible if there is
    // a way to it from the camera through chunks you can see through, so the
    // question is asked by going: start where the camera is, and only enter a
    // neighbour if the chunk you are leaving joins the face you came in by to
    // the face you are going out of.
    //
    // Pruned by the range as it goes, and NOT by the frustum any more. The
    // frustum pruning rested on convexity -- the straight line from the eye to
    // a chunk in view stays in view -- but the walk does not move along lines:
    // it steps face to face, six ways, and only through air that two chunks
    // agree about cell by cell. Where a line of sight crosses a chunk edge the
    // step has to go through one of the chunks beside it, and that one can be
    // rock there or out of frame; the way round was then pruned and the chunk
    // at the end of it went undrawn. Reported from play as a pool that came and
    // went with the tilt of the camera.
    //
    // The walk does NOT cross a chunk it has never been told about, and that is
    // the opposite of what this said first. Treating the unknown as see-through
    // is the safe-looking choice -- being wrong that way costs draw calls and
    // being wrong the other way is a hole -- and it cost 2.5 ms a frame: with
    // no range set the frustum reaches the far plane, and the walk wandered
    // through thousands of chunks of empty space beyond the edge of the world,
    // finding nothing, every frame. A quiet frame went from 0.44 ms to 2.93.
    //
    // Not crossing them loses nothing. A chunk with no entry here has no mesh
    // either -- both come off the same result -- so it draws nothing, and
    // chunks arrive in a ball around the player, so what is beyond an unknown
    // chunk is unknown too. A chunk being re-meshed keeps its old entry until
    // the new one lands, so a rebuild storm does not open holes in the walk.
    if (cull && m_occlusion) {
        diag::ProfScope walk("cull");
        m_reached.clear();
        m_walk.clear();

        static const int kStep[6][3] = {{-1, 0, 0}, {1, 0, 0}, {0, -1, 0},
                                        {0, 1, 0},  {0, 0, -1}, {0, 0, 1}};
        // Leaving by face f means arriving through the face opposite it.
        static const int kOpposite[6] = {1, 0, 3, 2, 5, 4};

        // Whether a chunk is worth walking to at all: in range. NOT in the
        // frustum as well, which is what this used to ask, and that was a hole
        // reported from play -- a pool that vanished for some tilts of the
        // camera and came back for others. The walk is a question about what
        // can be SEEN THROUGH, and the way to a chunk in front of the camera
        // can pass through chunks beside it: under a hill, round a corner, over
        // the edge of the frame. Stopping at the frame's edge left every chunk
        // behind such a path unreached, and an unreached chunk is not drawn
        // however squarely it sits in the view. The frustum is asked once, by
        // the draw below, of each chunk the walk reached.
        const auto in_view = [&](ChunkPos p) {
            const Vec3 min{(float)(p.x * kChunkSize), (float)(p.y * kChunkSize),
                           (float)(p.z * kChunkSize)};
            const Vec3 max{min.x + kChunkSize, min.y + kChunkSize, min.z + kChunkSize};

            const float dx = (eye.x < min.x) ? min.x - eye.x : (eye.x > max.x ? eye.x - max.x : 0.0f);
            const float dy = (eye.y < min.y) ? min.y - eye.y : (eye.y > max.y ? eye.y - max.y : 0.0f);
            const float dz = (eye.z < min.z) ? min.z - eye.z : (eye.z > max.z ? eye.z - max.z : 0.0f);
            return dx * dx + dy * dy + dz * dz <= m_view_range * m_view_range;
        };

        const auto look = [&](ChunkPos p) -> const ChunkVisibility * {
            const auto it = m_visibility.find(p);
            return it == m_visibility.end() ? nullptr : &it->second;
        };

        // Reaching a chunk is what draws it; the regions say how far the walk
        // goes on from there. A chunk reached a second time in regions it has
        // not been in yet is walked again from those -- without that, a room
        // entered by the wrong door first would never be left by the right one,
        // and what is past it would go undrawn.
        const auto reach = [&](ChunkPos p, int entry, uint64_t regions) {
            const auto it = m_reached.find(p);
            if (it == m_reached.end()) {
                m_reached.emplace(p, regions);
                m_walk.push_back({p, entry, regions});
                return;
            }
            if (it->second == kAllRegions) return;
            if (regions != kAllRegions && (regions & ~it->second) == 0) return;
            it->second = (regions == kAllRegions) ? kAllRegions : (it->second | regions);
            m_walk.push_back({p, entry, it->second});
        };

        const ChunkPos start = World::chunk_of((int)std::floor(eye.x), (int)std::floor(eye.y),
                                               (int)std::floor(eye.z));

        if (const ChunkVisibility *vis = look(start)) {
            // The camera's own chunk has no face it came in by, so the coarse
            // answer is that it leaves by all six -- and that answer is what
            // OPEN-23 measured as worthless underground: a chunk holding both a
            // cave and the open sky over it leaves by every face whichever of
            // the two the player is standing in.
            //
            // So ask which piece of the chunk's air the eye is in. Nothing
            // holding the eye means it is inside rock, or inside a room sealed
            // away from every face, and this cannot tell those apart without
            // the blocks -- so it takes the old answer, which over-draws. An
            // eye buried in stone is a sixtieth of a second from being in the
            // open anyway, and its near plane may already be.
            uint64_t regions = kAllRegions;
            if (vis->fine()) {
                const int lx = (int)std::floor(eye.x) - start.x * kChunkSize;
                const int ly = (int)std::floor(eye.y) - start.y * kChunkSize;
                const int lz = (int)std::floor(eye.z) - start.z * kChunkSize;
                const uint64_t here = vis->regions_at(lx, ly, lz);
                if (here != 0) regions = here;
            }
            reach(start, -1, regions);
        } else {
            // The camera is NOT in a chunk this client has been told about --
            // it has flown off the edge of the world, or is standing in a hole
            // the stream has not filled yet.
            //
            // Seeding the walk with that chunk asks a question with no answer:
            // the walk will not cross a chunk it has never been told about, so
            // it stops where it started and the frame draws nothing. Reported
            // from play, with a screenshot: fly away from the island and the
            // island goes, leaving the portal hanging in the sky, with the F3
            // line saying `chunks 0 drawn / 0 culled` -- not thrown away, never
            // considered.
            //
            // What is true out here is that the camera is in open space, and
            // open space outside the world is all one piece. So every known
            // chunk that TOUCHES it is reachable, and the walk carries on
            // normally from there -- which keeps the culling instead of giving
            // it up: the far side of an island is still behind its near side.
            //
            // Assuming that space is connected to the camera can only over-draw
            // (an unknown pocket sealed inside the world would be seeded as if
            // it were the sky), and over-drawing costs draw calls where the
            // other direction costs a hole. Same asymmetry as everywhere else
            // in this walk.
            for (const auto &kv : m_visibility) {
                const ChunkPos p = kv.first;
                if (!in_view(p)) continue;

                // Every face that is open to the outside and not only the first
                // of them: a chunk on a corner of the world is entered from two
                // sides, and the regions behind those two sides are not the
                // same set.
                for (int f = 0; f < 6; ++f) {
                    const ChunkPos out{p.x + kStep[f][0], p.y + kStep[f][1], p.z + kStep[f][2]};
                    if (m_visibility.find(out) != m_visibility.end()) continue;
                    reach(p, f, kv.second.fine() ? kv.second.regions_touching(f) : kAllRegions);
                }
            }
        }

        ChunkVisibility open_all;
        open_all.bits = 0x7FFF;

        while (!m_walk.empty()) {
            const WalkItem item = m_walk.back();
            m_walk.pop_back();

            const ChunkVisibility *found = look(item.pos);
            if (!found && item.entry >= 0) continue;
            const ChunkVisibility &vis = found ? *found : open_all;

            // Which faces this chunk can be left by. From the regions where
            // there are regions, and from the fifteen bits where there are not
            // -- a chunk with more ways through it than the mask can carry, or
            // one entered with no idea of where.
            uint8_t exits = 0;
            if (item.regions != kAllRegions && vis.fine()) {
                for (size_t r = 0; r < vis.regions.size(); ++r)
                    if (item.regions & (1ull << r)) exits |= vis.regions[r].faces;
            } else {
                for (int f = 0; f < 6; ++f)
                    if (item.entry < 0 || vis.joined(item.entry, f)) exits |= (uint8_t)(1 << f);
            }

            for (int f = 0; f < 6; ++f) {
                if (!(exits & (1 << f))) continue;

                const ChunkPos next{item.pos.x + kStep[f][0], item.pos.y + kStep[f][1],
                                    item.pos.z + kStep[f][2]};
                const ChunkVisibility *nv = look(next);
                if (!nv) continue;
                if (!in_view(next)) continue;

                const int opp = kOpposite[f];

                // Which pieces of the neighbour's air this crossing lands in.
                // Two chunks agree about the plane between them cell by cell,
                // so the answer is an overlap and not a face: a cave leaving by
                // +x enters the cave on the other side and not the sky above
                // it, however open that face is as a whole.
                uint64_t entered = kAllRegions;
                if (nv->fine()) {
                    if (item.regions != kAllRegions && vis.fine()) {
                        uint64_t mine[kFaceWords] = {};
                        for (size_t i = vis.face_begin[f]; i < vis.face_begin[f + 1]; ++i) {
                            const FaceCells &fc = vis.face_cells[i];
                            if (!(item.regions & (1ull << fc.region))) continue;
                            for (int w = 0; w < kFaceWords; ++w) mine[w] |= fc.cells[w];
                        }

                        entered = 0;
                        for (size_t i = nv->face_begin[opp]; i < nv->face_begin[opp + 1]; ++i) {
                            const FaceCells &fc = nv->face_cells[i];
                            bool             met = false;
                            for (int w = 0; w < kFaceWords && !met; ++w)
                                met = (fc.cells[w] & mine[w]) != 0;
                            if (met) entered |= 1ull << fc.region;
                        }
                    } else {
                        entered = nv->regions_touching(opp);
                    }
                }

                // Reached even when nothing was entered: the walk got to its
                // face, which is a face the player can see, and a chunk of rock
                // has to be drawn by whoever is looking at the rock. What it
                // does not do is carry on -- a crossing that enters no region
                // leaves by none.
                reach(next, opp, entered);
            }
        }
    }

    for (const auto &entry : m_meshes) {
        const ChunkMesh &cm = entry.second;

        // The view range, before the frustum: a chunk out of range is out of
        // range whichever way the camera points, and the two are counted apart
        // because "nothing was drawn" has two causes wanting different fixes.
        //
        // Measured to the NEAREST point of the chunk rather than to its centre.
        // A centre test drops a chunk the player is standing at the edge of
        // once the range falls below about half a chunk, which is a hole in the
        // floor rather than a distant one.
        {
            const Vec3 min = cm.origin;
            const Vec3 max = Vec3{min.x + kChunkSize, min.y + kChunkSize, min.z + kChunkSize};
            const float dx = (eye.x < min.x) ? min.x - eye.x : (eye.x > max.x ? eye.x - max.x : 0.0f);
            const float dy = (eye.y < min.y) ? min.y - eye.y : (eye.y > max.y ? eye.y - max.y : 0.0f);
            const float dz = (eye.z < min.z) ? min.z - eye.z : (eye.z > max.z ? eye.z - max.z : 0.0f);
            if (dx * dx + dy * dy + dz * dz > m_view_range * m_view_range) {
                ++m_range_culled;
                continue;
            }
        }

        if (cull) {
            const Vec3 min = cm.origin;
            const Vec3 max = Vec3{min.x + kChunkSize, min.y + kChunkSize, min.z + kChunkSize};
            const bool in_view  = frustum.intersects_aabb(min, max);
            const bool occluded = in_view && m_occlusion && m_reached.find(entry.first) == m_reached.end();

            // A chunk out of sight whose track pieces' roads may not be: they
            // reach up to three squares on and a bank's edge under the corner,
            // and a piece drawn only with its corner's chunk popped in and
            // out at the edge of the view.
            if ((!in_view || occluded) && cm.pieces) {
                const Vec3 lo{min.x, min.y - (float)kPieceBelow, min.z};
                const Vec3 hi{max.x + (float)(kPieceMaxUnits * kPieceUnit),
                              max.y + (float)kPieceMaxHeight,
                              max.z + (float)(kPieceMaxUnits * kPieceUnit)};
                if (frustum.intersects_aabb(lo, hi)) m_part_chunks.push_back(&cm);
            }
            if (!in_view) {
                ++m_culled;
                continue;
            }

            // In front of the camera, in range, and behind something.
            if (occluded) {
                ++m_occluded;
                continue;
            }
        }

        const Vec3 c = Vec3{cm.origin.x + half, cm.origin.y + half, cm.origin.z + half};
        const Vec3 d = c - eye;
        if (cm.opaque.vertex_count > 0 && cm.opaque.mesh.id != 0)
            m_opaque_order.push_back({dot(d, d), &cm});

        if (!cm.parts.empty()) m_part_chunks.push_back(&cm);

        if ((cm.alpha.vertex_count > 0 && cm.alpha.mesh.id != 0) ||
            (cm.liquid.vertex_count > 0 && cm.liquid.mesh.id != 0))
            m_alpha_order.push_back({dot(d, d), &cm});
    }

    // Front to back, so what is hidden fails the depth test before its
    // fragment is shaded. It matters since a fragment walks the water in front
    // of it (CNP_WATER_GLSL): drawn in the map's order, a frame at a pool's
    // surface ran at 356 fps on an RX 5700, and at 414 sorted.
    //
    // Positions are chunk-local bytes; the model matrix puts the chunk where
    // it belongs. That is what keeps the vertex down to 8 bytes.
    std::sort(m_opaque_order.begin(), m_opaque_order.end(),
              [](const auto &a, const auto &b) { return a.first < b.first; });
    for (const auto &item : m_opaque_order) {
        const Mat4 model = mat4_translate(item.second->origin);
        gfx_model_set(model.m);
        gfx_draw(item.second->opaque.mesh, item.second->opaque.vertex_count, 0);
        ++m_draw_calls;
    }

    // The boxes, after everything opaque and before anything translucent --
    // they are opaque themselves, and a chest behind a window has to be drawn
    // before the window is blended over it.
    if (m_reg) draw_parts(*m_reg, projection, view);
    draw_clouds();
    fade_sky();
}

// Glass and water, back to front: after draw() and after whatever else is
// opaque -- the bodies -- so one standing behind a window is seen through it
// rather than drawn over it.
void WorldRenderer::draw_translucent(const Mat4 &projection, const Mat4 &view)
{
    if (m_alpha_order.empty() && m_part_see_count == 0) return;

    // Back to this pass's shader: draw_parts, the bodies, may have bound their own.
    bind_world(projection, view);

    // Back to front, and only per chunk. Sorting within a chunk would need the
    // faces re-ordered per camera, which is a per-frame rebuild of the vertex
    // buffer; per chunk is what fits the mesh format and it is right whenever
    // the translucent surfaces of one chunk do not overlap each other along the
    // line of sight. A window has one pane, so that holds here; a fish tank
    // inside a greenhouse would not, and this is where that would be fixed.
    std::sort(m_alpha_order.begin(), m_alpha_order.end(),
              [](const auto &a, const auto &b) { return a.first > b.first; });

    gfx_state state = gfx_state_default();
    state.blend      = GFX_BLEND_ALPHA;
    state.depth_write = false;   // translucent surfaces must not occlude each other
    gfx_state_set(&state);

    for (const auto &item : m_alpha_order) {
        const ChunkMesh &cm  = *item.second;
        const Mat4       model = mat4_translate(cm.origin);
        gfx_model_set(model.m);
        if (cm.liquid.vertex_count > 0 && cm.liquid.mesh.id != 0) {
            gfx_draw(cm.liquid.mesh, cm.liquid.vertex_count, 0);
            ++m_draw_calls;
        }
        if (cm.alpha.vertex_count > 0 && cm.alpha.mesh.id != 0) {
            gfx_draw(cm.alpha.mesh, cm.alpha.vertex_count, 0);
            ++m_draw_calls;
        }
    }

    // The see-through boxes, after the chunks' glass and water.
    // ponytail: not sorted against them; a pane behind a glass block blends in the wrong order.
    if (m_part_see_count > 0) {
        const Mat4 identity = mat4_identity();
        gfx_model_set(identity.m);
        gfx_draw(m_parts_mesh, m_part_see_count, m_part_see_first);
        ++m_draw_calls;
    }

    state.blend       = GFX_BLEND_NONE;
    state.depth_write = true;
    gfx_state_set(&state);
}

} // namespace cnp
