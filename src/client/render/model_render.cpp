#include "client/render/model_render.h"

#include "client/render/shaders.h"
#include "core/item/inventory.h"
#include "core/media/media_cache.h"

#include <cstdio>
#include <cstring>
#include <vector>

namespace cnp {

namespace {

// Reads a file whole. Models name their textures relative to themselves, and
// what is on the other end of that name is a file on disk today and a hash in
// the media cache tomorrow (DESIGN.md 4.5) -- which is why the decode below
// goes through gfx_image_decode_rgba, from memory, and not through a path.
bool read_file(const std::string &path, std::vector<uint8_t> &out)
{
    std::FILE *f = std::fopen(path.c_str(), "rb");
    if (!f) return false;
    std::fseek(f, 0, SEEK_END);
    const long n = std::ftell(f);
    std::fseek(f, 0, SEEK_SET);
    if (n <= 0) { std::fclose(f); return false; }
    out.resize((size_t)n);
    const bool ok = std::fread(out.data(), 1, (size_t)n, f) == (size_t)n;
    std::fclose(f);
    return ok;
}

} // namespace

bool ModelRenderer::init()
{
    m_shader = gfx_shader_create(shaders::kModelVS, shaders::kModelFS);
    if (m_shader.id == 0) return false;

    m_loc_mvp     = gfx_uniform_location(m_shader, "uMVP");
    m_loc_bones   = gfx_uniform_location(m_shader, "uBones");
    m_loc_colour  = gfx_uniform_location(m_shader, "uColour");
    m_loc_texture = gfx_uniform_location(m_shader, "uTexture");
    m_loc_glow    = gfx_uniform_location(m_shader, "uGlow");
    m_loc_light   = gfx_uniform_location(m_shader, "uLight");
    m_loc_alpha   = gfx_uniform_location(m_shader, "uAlpha");
    m_water.locate(m_shader);

    // One white texel, for a material that names no texture. The alternative is
    // a second shader with the sampler taken out, and two shaders that differ
    // by one multiplication is two shaders to keep in step.
    const uint8_t white[4] = {255, 255, 255, 255};
    m_white = gfx_texture_create(white, 1, 1, GFX_PF_RGBA8, GFX_FILTER_NEAREST);
    return m_white.id != 0;
}

void ModelRenderer::shutdown()
{
    for (Part &p : m_parts)
        if (p.texture.id != 0 && p.texture.id != m_white.id) gfx_texture_destroy(p.texture);
    m_parts.clear();

    if (m_mesh.id) gfx_mesh_destroy(m_mesh);
    if (m_vb.id) gfx_buffer_destroy(m_vb);
    if (m_ib.id) gfx_buffer_destroy(m_ib);
    if (m_white.id) gfx_texture_destroy(m_white);
    if (m_shader.id) gfx_shader_destroy(m_shader);

    m_mesh = gfx_mesh{0};
    m_vb = m_ib = gfx_buffer{0};
    m_white = gfx_texture{0};
    m_shader = gfx_shader{0};
    m_index_count = 0;
    m_joint_count = 0;
}

bool ModelRenderer::upload(const SkinnedModel &model, std::string &why)
{
    why.clear();

    if (model.joint_count > kMaxJoints) {
        why = "this skeleton has more joints than a GL 3.3 vertex shader can hold";
        return false;
    }
    if (model.vertices.empty() || model.indices.empty()) {
        why = "this model has no geometry";
        return false;
    }

    m_vb = gfx_buffer_create(model.vertices.data(),
                             (int)(model.vertices.size() * sizeof(SkinVertex)), false, false);
    // Sixteen bits, because that is what the backend draws -- see gfx.h. A
    // model past 65535 vertices is refused rather than drawn as noise: the
    // failure of a 32-bit buffer read as 16 is silent and looks like a shader
    // bug for as long as somebody is willing to look at the shader.
    if (model.vertices.size() > 65535) {
        why = "this model has more vertices than a 16-bit index can name";
        return false;
    }
    std::vector<uint16_t> indices16(model.indices.size());
    for (size_t i = 0; i < model.indices.size(); ++i)
        indices16[i] = (uint16_t)model.indices[i];

    m_ib = gfx_buffer_create(indices16.data(), (int)(indices16.size() * sizeof(uint16_t)), false,
                             true);
    if (m_vb.id == 0 || m_ib.id == 0) {
        why = "the model's buffers could not be made";
        return false;
    }

    // The layout is the struct, written out once. Joint indices are NOT
    // normalised: the shader wants 0..83, not 0..1.
    gfx_vertexlayout layout{};
    layout.stride     = (int)sizeof(SkinVertex);
    layout.attr_count = 5;
    layout.attrs[0] = gfx_attr{0, 3, GFX_ATTR_F32, false, (int)offsetof(SkinVertex, pos)};
    layout.attrs[1] = gfx_attr{1, 3, GFX_ATTR_F32, false, (int)offsetof(SkinVertex, normal)};
    layout.attrs[2] = gfx_attr{2, 2, GFX_ATTR_F32, false, (int)offsetof(SkinVertex, uv)};
    layout.attrs[3] = gfx_attr{3, 4, GFX_ATTR_U16, false, (int)offsetof(SkinVertex, joints)};
    layout.attrs[4] = gfx_attr{4, 4, GFX_ATTR_F32, false, (int)offsetof(SkinVertex, weights)};

    m_mesh = gfx_mesh_create(&layout, m_vb, m_ib);
    if (m_mesh.id == 0) {
        why = "the model's mesh could not be made";
        return false;
    }

    for (const ModelPart &src : model.parts) {
        Part p;
        p.first_index = src.first_index;
        p.index_count = src.index_count;
        p.texture     = m_white;

        if (src.material >= 0 && (size_t)src.material < model.materials.size()) {
            const ModelMaterial &mat = model.materials[(size_t)src.material];
            std::memcpy(p.colour, mat.base_colour, sizeof(p.colour));
            p.double_sided = mat.double_sided;
            p.glow         = mat.glow;
            // "tint_1".."tint_3", and Blender's ".001" copies of them.
            if (mat.name.rfind("tint_", 0) == 0 && mat.name.size() >= 6 && mat.name[5] >= '1' &&
                mat.name[5] <= '3')
                p.tint = mat.name[5] - '1';

            if (!mat.texture.empty()) {
                std::vector<uint8_t> bytes;
                if (!read_file(mat.texture, bytes)) {
                    // Refused rather than drawn white. A character with a
                    // missing texture is a white silhouette, and silently
                    // drawing one is how a broken asset path ships.
                    why = "a texture this model names could not be read: " + mat.texture;
                    return false;
                }
                int            w = 0, h = 0;
                unsigned char *px = gfx_image_decode_rgba(bytes.data(), (int)bytes.size(), &w, &h);
                if (!px) {
                    why = "a texture this model names is not a picture: " + mat.texture;
                    return false;
                }
                p.texture = gfx_texture_create(px, w, h, GFX_PF_RGBA8, GFX_FILTER_LINEAR);
                gfx_image_free(px);
                if (p.texture.id == 0) {
                    why = "a texture this model names could not go to the GPU";
                    return false;
                }
            }
        }
        m_parts.push_back(p);
    }

    m_index_count = (int)model.indices.size();
    m_joint_count = model.joint_count;
    return true;
}

void ModelRenderer::draw(const Mat4 &projection, const Mat4 &view, const Mat4 &model,
                         const std::vector<float> &bones, gfx_texture texture, float light,
                         float alpha, uint32_t tag)
{
    if (m_index_count == 0 || m_shader.id == 0) return;

    gfx_shader_bind(m_shader);

    // Folded here rather than in the shader, which is what makes room for the
    // bones -- see shaders.h. Once per draw against once per vertex, too.
    const Mat4 mvp = mat4_multiply(mat4_multiply(projection, view), model);
    if (m_loc_mvp >= 0) gfx_uniform_set_mat4(m_loc_mvp, mvp.m);

    // Three vec4 a joint, and the array is uploaded whole rather than per part:
    // the parts of one character share one skeleton, and uploading it twice
    // would be uploading it twice.
    if (m_loc_bones >= 0 && !bones.empty()) {
        const int rows = (int)(bones.size() / 4);
        gfx_uniform_set_v4(m_loc_bones, bones.data(), rows);
    } else if (m_loc_bones >= 0) {
        // A model with NO skeleton -- a block's, and any glTF exported without
        // one. The loader binds such a vertex to joint 0 with weight 1, so the
        // shader reads three rows that nobody uploaded: the mesh collapses to
        // the origin and draws nothing at all, which is exactly what a block
        // wearing a model did until this line (TODO.md OPEN-43).
        //
        // One identity joint, here rather than in the shader: the shader runs
        // per vertex and this runs once per draw, and the alternative is a
        // branch on every vertex of every character.
        static const float identity[12] = {1.0f, 0.0f, 0.0f, 0.0f,
                                           0.0f, 1.0f, 0.0f, 0.0f,
                                           0.0f, 0.0f, 1.0f, 0.0f};
        gfx_uniform_set_v4(m_loc_bones, identity, 3);
    }
    if (m_loc_texture >= 0) gfx_uniform_set_int(m_loc_texture, 0);
    if (m_loc_light >= 0) gfx_uniform_set_f(m_loc_light, std::max(light, night_sight * 0.55f));
    if (m_loc_alpha >= 0) gfx_uniform_set_f(m_loc_alpha, alpha);
    m_water.set(projection, view);   // before the parts bind their textures to unit 0

    gfx_state state = gfx_state_default();
    // Still writing depth, so a fading body shows its near side and not its
    // own insides through it.
    if (alpha < 1.0f) state.blend = GFX_BLEND_ALPHA;
    // The solid parts, then the see-through ones (a material whose colour's
    // alpha is under 1: a windscreen, a canopy) over them, blended and not
    // writing depth, so what is inside shows.
    // ponytail: not sorted against other models' glass; sort when two overlap.
    for (int see = 0; see < 2; ++see)
    for (const Part &p : m_parts) {
        if ((p.colour[3] < 1.0f) != (see == 1)) continue;
        if (see == 1) {
            state.blend       = GFX_BLEND_ALPHA;
            state.depth_write = false;
        }
        // Both sides, when the material says so. A garment modelled as a
        // surface has no inside, and culling its back faces shows the world
        // through the character.
        state.cull = p.double_sided ? GFX_CULL_NONE : GFX_CULL_BACK;
        gfx_state_set(&state);

        gfx_texture_bind(texture.id != 0 ? texture : p.texture, 0);
        if (m_loc_colour >= 0) {
            const int dye = p.tint >= 0 ? tag_colour(tag, p.tint) : 0;
            if (dye > 0) {
                const float c[4] = {kDyeColours[dye - 1][0] / 255.0f, kDyeColours[dye - 1][1] / 255.0f,
                                    kDyeColours[dye - 1][2] / 255.0f, p.colour[3]};
                gfx_uniform_set_v4(m_loc_colour, c, 1);
            } else {
                gfx_uniform_set_v4(m_loc_colour, p.colour, 1);
            }
        }
        if (m_loc_glow >= 0) gfx_uniform_set_f(m_loc_glow, p.glow ? 1.0f : 0.0f);
        gfx_draw(m_mesh, (int)p.index_count, (int)p.first_index);
    }

    state.cull        = GFX_CULL_BACK;
    state.blend       = GFX_BLEND_NONE;
    state.depth_write = true;
    gfx_state_set(&state);
}

} // namespace cnp
