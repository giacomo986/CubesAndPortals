#pragma once

// A character, out of a file somebody made in a modelling tool.
//
// glTF 2.0 and not b3d, and the reason is a fork in the road worth stating
// where it is taken. The original engine's mods ship .b3d, and DESIGN.md 8 puts running them
// in M11 -- so a b3d reader is coming whatever happens here. What must NOT
// happen is that this engine's idea of a skeleton is glTF's idea wearing a
// different name, because then b3d needs a second one and the animation code is
// written twice. What comes out of here is the engine's own shape; glTF is the
// first thing that gets translated into it, not the definition of it.
//
// FOUR INFLUENCES PER VERTEX, which is what both of the models this was written
// against use and what every tool exports by default. A vertex that needs a
// fifth is a vertex whose smallest weight is dropped and the rest renormalised,
// said out loud rather than silently producing a limb that tears.
//
// The bind pose is computed here and not left to the caller. It is the one
// thing every consumer needs -- how big the thing is, where its feet are -- and
// working it out means walking the node hierarchy and multiplying by the
// inverse bind matrices, which is exactly the code a caller would get wrong.

#include <cstdint>
#include <string>
#include <vector>

#include "core/math/vec.h"

namespace cnp {

// What the vertex buffer holds. 56 bytes, which is seven times what a voxel
// vertex costs -- and right, because there are four thousand of these and a
// million of those. The voxel format is packed to the byte because chunks are
// the thing that fills memory; a character is not.
struct SkinVertex {
    float    pos[3];
    float    normal[3];
    float    uv[2];
    uint16_t joints[4];
    float    weights[4];
};

// One draw's worth: a run of indices and the material to draw it with.
struct ModelPart {
    uint32_t first_index = 0;
    uint32_t index_count = 0;
    int      material    = -1;
};

struct ModelMaterial {
    std::string name;
    float       base_colour[4] = {1.0f, 1.0f, 1.0f, 1.0f};

    // The image this material samples, as a path relative to the model file.
    // EMPTY when there is none, and that is not a rare case to be tidied away
    // later: the first model this was written against has no textures at all
    // and paints itself with base_colour alone.
    std::string texture;

    // Both sides drawn. True for every material in both models tested, because
    // a garment modelled as a surface has no inside -- and back-face culling it
    // shows the world through the character.
    bool double_sided = false;

    // Gives its own light: any emissive colour at all, and it is drawn at full
    // brightness whichever way it faces -- a furnace's fire is not shaded.
    bool glow = false;
};

// How many joints a skeleton may have.
//
// Not a number chosen to be round. GL 3.3 core -- the floor DESIGN.md 2 targets
// -- guarantees only 1024 uniform components in a vertex shader. A joint costs
// 12 of them, because a bone matrix is affine and the projection row is a row
// of nothing: 3x4 and not 4x4. 84 joints is 1008 components, which is what fits
// with the handful the rest of the shader needs. As 4x4 the same skeleton would
// want 1344 and would fail to link on a conforming driver while working
// perfectly on the one it was written on.
inline constexpr int kMaxJoints = 84;

// One animation: keys for some joints' translation, rotation or scale. A key
// between two is interpolated, linearly or held (glTF's STEP); a cubic spline
// is read as linear through its keys.
struct ClipChannel {
    int                joint = 0;
    int                path  = 0;   // 0 translation, 1 rotation, 2 scale
    bool               step  = false;
    std::vector<float> times;
    std::vector<float> values;      // 3 a key, or 4 for a rotation
};

struct ModelClip {
    std::string              name;
    float                    duration = 0.0f;
    std::vector<ClipChannel> channels;
};

// A clip at a moment, and how much of it goes over what came before it.
struct ClipAt {
    int   clip   = -1;
    float time   = 0.0f;
    float weight = 1.0f;
};

struct SkinnedModel {
    std::vector<SkinVertex> vertices;
    std::vector<uint32_t>   indices;
    std::vector<ModelPart>  parts;
    std::vector<ModelMaterial> materials;

    // The bind pose, as joint matrices ready for a shader: 3x4 ROW major, 12
    // floats each, joint_count of them. Row major because that is what a
    // shader multiplying a position by three dot products wants, and the
    // transpose belongs wherever it is written down once.
    std::vector<float> bind_pose;
    int                joint_count = 0;

    // What the bind pose occupies, in the units the file uses. Both models
    // tested came out 1.825 tall with their feet at zero, which is a player's
    // height in blocks -- so nothing here scales anything, and a model that
    // arrives in centimetres is a model whose own node hierarchy says so.
    Vec3 min{0.0f, 0.0f, 0.0f};
    Vec3 max{0.0f, 0.0f, 0.0f};

    // The skeleton itself, one entry per joint, for a pose other than the
    // bind pose (pose). Each joint's own transform against its parent as
    // translation, rotation (x y z w) and scale, ten floats; for a joint whose
    // parent is not a joint, where that parent node stands (4x4 column major,
    // identity otherwise). joint_rest is where each joint stands bound.
    std::vector<std::string> joint_names;
    std::vector<int>         joint_parent;   // an index into these, or -1
    std::vector<float>       joint_trs;
    std::vector<float>       joint_base;
    std::vector<float>       joint_inverse_bind;
    std::vector<Vec3>        joint_rest;

    // What the file animates, by name: "walk", "idle".
    std::vector<ModelClip> clips;

    int clip(const std::string &name) const
    {
        for (size_t i = 0; i < clips.size(); ++i)
            if (clips[i].name == name) return (int)i;
        return -1;
    }

    bool skinned() const { return joint_count > 0; }
};

// The skeleton posed, as bone matrices shaped like bind_pose. Each layer's clip
// is laid over the rest pose and over the layers before it by its weight -- a
// crossfade is two layers -- each layer past the first on the joints its clip
// has channels for and no others, so a clip of the arms alone rides a walk. `turn`, when not null, holds joint_count angles in
// radians, each about that joint's own Y -- the axis a Blender bone points
// along -- and a turned joint carries its children with it: a wheel under a
// steering knuckle steers. `show`, when not null, holds joint_count of -1
// (as posed), 0 (scaled to nothing, children and all) or 1 (at its own size,
// whatever a clip said): a tool in the hand is a bone shown or hidden.
void pose(const SkinnedModel &model, const ClipAt *layers, int layer_count, const float *turn,
          const int8_t *show, std::vector<float> &out);

// Reads a .gltf and the .bin beside it.
//
// Answers false and puts the reason in `why`, leaving `out` alone. Nothing,
// not a half-built model: a caller handed a model with no vertices has to guess
// whether the file was empty or the read failed, and it will guess wrong.
bool load_gltf(const std::string &path, SkinnedModel &out, std::string &why);

// What else this model is made of: every file the .gltf names beside itself --
// its buffers and its images -- as the names it names them by.
//
// A model is not a file, and this is the call that says so. Sending somebody a
// model means sending the .gltf and everything in here, under these names, into
// one directory.
//
// A `data:` uri is embedded in the JSON and names no file, so it is not
// returned and is not an error. Everything else has to be a PLAIN name beside
// the .gltf: a uri with a slash or a leading dot in it is refused and nothing
// comes back, because a model arrives from wherever its world did and a name
// that can climb out of the directory reads whatever the player can read.
bool gltf_files_beside(const std::string &path, std::vector<std::string> &out, std::string &why);

} // namespace cnp
