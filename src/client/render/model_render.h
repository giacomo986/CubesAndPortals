#pragma once

// Drawing a character.
//
// The skinning is on the GPU and the bone matrices are 3x4, which is the one
// decision in this file that is not a preference. GL 3.3 core -- the floor
// DESIGN.md 2 targets -- guarantees 1024 uniform components in a vertex shader.
// The larger of the two models this was written against has 82 joints: as 4x4
// that is 1312 components and fails to link on a conforming driver while
// working perfectly on the one it was written on. As 3x4 it is 984. A bone
// matrix is affine and its projection row is a row of nothing.
//
// CPU skinning was the alternative and it is the one raylib takes. It costs a
// full vertex buffer upload every frame per character, which is 1.3 MB a frame
// for the larger model -- the exact shape of cost M9 spent itself removing from
// the meshing path.
//
// One model per renderer, uploaded once. A crowd of the same character is a
// second draw with a second set of bones and the same buffers, which is what
// this shape allows and does not yet do.

#include "client/gfx/gfx.h"
#include "client/render/water.h"
#include "core/math/mat4.h"
#include "core/media/gltf.h"

#include <vector>

namespace cnp {

class ModelRenderer {
public:
    bool init();
    void shutdown();

    // Hands the model to the GPU. Returns false and leaves the renderer empty
    // when the model has more joints than a shader can hold, or when a texture
    // it names cannot be read -- a character drawn with a missing texture is a
    // white silhouette, and silently drawing one is how a broken asset path
    // ships.
    bool upload(const SkinnedModel &model, std::string &why);

    bool ready() const { return m_index_count > 0; }

    // `bones` is joint_count * 12 floats, 3x4 row major -- the same shape
    // SkinnedModel::bind_pose comes in. Passing the bind pose draws the model
    // exactly as its file describes it.
    //
    // `texture`, when given, is sampled by every part instead of its own: a
    // block held in the hand is a cube cut out of the world's atlas.
    void draw(const Mat4 &projection, const Mat4 &view, const Mat4 &model,
              const std::vector<float> &bones, gfx_texture texture = gfx_texture{0},
              float light = 1.0f,    // how bright where it stands, 0..1; a body is 1
              float alpha = 1.0f,    // under 1 blended over what is behind it: fading out
              uint32_t tag = 0);     // a stack's colours (ItemStack::tag) on its tint_1..3 materials

    int joint_count() const { return m_joint_count; }

private:
    struct Part {
        uint32_t    first_index = 0;
        uint32_t    index_count = 0;
        gfx_texture texture{0};
        float       colour[4] = {1.0f, 1.0f, 1.0f, 1.0f};
        bool        double_sided = false;
        bool        glow = false;
        int         tint = -1;   // 0..2 for a material named tint_1..tint_3: a stack's colour
    };

    gfx_shader m_shader{0};
    gfx_buffer m_vb{0};
    gfx_buffer m_ib{0};
    gfx_mesh   m_mesh{0};
    gfx_texture m_white{0};    // for a material that names no texture

    std::vector<Part> m_parts;
    int m_index_count = 0;
    int m_joint_count = 0;

    int m_loc_mvp = -1;
    int m_loc_bones = -1;
    int m_loc_colour = -1;
    int m_loc_texture = -1;
    int m_loc_glow = -1;
    int m_loc_light = -1;
    int m_loc_alpha = -1;
    WaterUniforms m_water;
};

} // namespace cnp
