#include "core/media/gltf.h"

#include "core/registry/model_registry.h"

#include <algorithm>
#include <cmath>
#include <cstring>

#define CGLTF_IMPLEMENTATION
#include "cgltf.h"

namespace cnp {

namespace {

// A 4x4, column major, the way glTF and OpenGL both write one.
struct M4 {
    float m[16] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};

    float at(int row, int col) const { return m[col * 4 + row]; }
};

M4 multiply(const M4 &a, const M4 &b)
{
    M4 r;
    for (int c = 0; c < 4; ++c)
        for (int row = 0; row < 4; ++row) {
            float sum = 0.0f;
            for (int k = 0; k < 4; ++k) sum += a.at(row, k) * b.at(k, c);
            r.m[c * 4 + row] = sum;
        }
    return r;
}

Vec3 transform_point(const M4 &t, const Vec3 &v)
{
    return Vec3{t.at(0, 0) * v.x + t.at(0, 1) * v.y + t.at(0, 2) * v.z + t.at(0, 3),
                t.at(1, 0) * v.x + t.at(1, 1) * v.y + t.at(1, 2) * v.z + t.at(1, 3),
                t.at(2, 0) * v.x + t.at(2, 1) * v.y + t.at(2, 2) * v.z + t.at(2, 3)};
}

// The directory a path is in, with the separator, or empty. Textures and the
// .bin are named relative to the .gltf and not to whoever is running.
std::string dir_of(const std::string &path)
{
    const size_t cut = path.find_last_of("/\\");
    return (cut == std::string::npos) ? std::string() : path.substr(0, cut + 1);
}

} // namespace

bool gltf_files_beside(const std::string &path, std::vector<std::string> &out, std::string &why)
{
    out.clear();
    why.clear();

    cgltf_options options{};
    cgltf_data   *data = nullptr;
    if (cgltf_parse_file(&options, path.c_str(), &data) != cgltf_result_success) {
        why = "not a glTF, or not readable: " + path;
        return false;
    }
    struct Guard {
        cgltf_data *d;
        ~Guard() { if (d) cgltf_free(d); }
    } guard{data};

    // A uri this reads and a uri it refuses. Embedded data is not a file and is
    // not an error; a name with a path in it is an error, and it takes the
    // whole model down with it rather than costing that one file -- half a
    // model is a character with a hole in it, and a caller handed one cannot
    // tell which half it got.
    auto want = [&](const char *uri) {
        if (uri == nullptr) return true;
        const std::string u = uri;
        if (u.rfind("data:", 0) == 0) return true;   // in the JSON, not on disk
        if (!model_file_is_plain(u)) {
            why = "this model names '" + u + "', which is not a file beside it";
            return false;
        }
        for (const std::string &had : out)
            if (had == u) return true;   // one name, however many things use it
        out.push_back(u);
        return true;
    };

    for (cgltf_size i = 0; i < data->buffers_count; ++i)
        if (!want(data->buffers[i].uri)) { out.clear(); return false; }
    for (cgltf_size i = 0; i < data->images_count; ++i)
        if (!want(data->images[i].uri)) { out.clear(); return false; }

    return true;
}

bool load_gltf(const std::string &path, SkinnedModel &out, std::string &why)
{
    why.clear();

    cgltf_options options{};
    cgltf_data   *data = nullptr;

    if (cgltf_parse_file(&options, path.c_str(), &data) != cgltf_result_success) {
        why = "not a glTF, or not readable: " + path;
        return false;
    }

    // Everything below can fail, and every one of those paths has to free this.
    struct Guard {
        cgltf_data *d;
        ~Guard() { if (d) cgltf_free(d); }
    } guard{data};

    // The .bin beside it. A loader that treats a missing buffer as an empty one
    // hands back a model with the right vertex COUNT and nothing in it, which
    // draws as a character-shaped hole and looks like a shader bug.
    //
    // Belt and braces, said out loud: taking this check away leaves the gate
    // green, because the first position read then fails and the model is
    // refused anyway. What it buys is the MESSAGE -- "the buffers this model
    // names could not be read" sends somebody to look for a file, and "a
    // position could not be read" sends them to look at their exporter.
    if (cgltf_load_buffers(&options, data, path.c_str()) != cgltf_result_success) {
        why = "the buffers this model names could not be read";
        return false;
    }
    if (cgltf_validate(data) != cgltf_result_success) {
        why = "the file is a glTF and does not agree with itself";
        return false;
    }

    // ------------------------------------------------------------- the skin
    //
    // One skin. Both models tested have exactly one, and a model with several
    // is several characters -- which wants a caller that knows which one it
    // asked for rather than a loader that picks.
    const cgltf_skin *skin = (data->skins_count > 0) ? &data->skins[0] : nullptr;
    if (skin && skin->joints_count > (size_t)kMaxJoints) {
        why = "this skeleton has more joints than a GL 3.3 vertex shader can hold";
        return false;
    }

    SkinnedModel built;
    built.joint_count = skin ? (int)skin->joints_count : 0;

    // Where each joint's node ends up, and where it was bound. The product is
    // what a shader multiplies a vertex by, and computing it here is the point
    // of this function: a caller doing it would have to walk the node hierarchy
    // and would get the order wrong.
    std::vector<M4> joint_matrix((size_t)built.joint_count);
    for (int j = 0; j < built.joint_count; ++j) {
        M4 world;
        cgltf_node_transform_world(skin->joints[(size_t)j], world.m);

        M4 inverse_bind;
        if (skin->inverse_bind_matrices) {
            if (!cgltf_accessor_read_float(skin->inverse_bind_matrices, (cgltf_size)j,
                                           inverse_bind.m, 16)) {
                why = "a joint has no inverse bind matrix";
                return false;
            }
        }
        joint_matrix[(size_t)j] = multiply(world, inverse_bind);

        // What pose needs to walk the skeleton again. A joint written as a
        // matrix is taken as its translation alone: no exporter this was
        // tried with writes one, and pulling a rotation out of a matrix that
        // may be skewed is code for a file nobody has.
        const cgltf_node *node = skin->joints[(size_t)j];
        int parent = -1;
        for (int k = 0; k < built.joint_count; ++k)
            if (node->parent != nullptr && skin->joints[(size_t)k] == node->parent) parent = k;
        float trs[10] = {0, 0, 0, 0, 0, 0, 1, 1, 1, 1};
        if (node->has_translation) std::memcpy(trs, node->translation, 12);
        if (node->has_matrix) std::memcpy(trs, node->matrix + 12, 12);
        if (node->has_rotation) std::memcpy(trs + 3, node->rotation, 16);
        if (node->has_scale) std::memcpy(trs + 7, node->scale, 12);
        M4 base;
        if (parent < 0 && node->parent != nullptr) cgltf_node_transform_world(node->parent, base.m);
        built.joint_names.push_back(node->name ? node->name : "");
        built.joint_parent.push_back(parent);
        built.joint_trs.insert(built.joint_trs.end(), trs, trs + 10);
        built.joint_base.insert(built.joint_base.end(), base.m, base.m + 16);
        built.joint_inverse_bind.insert(built.joint_inverse_bind.end(), inverse_bind.m,
                                        inverse_bind.m + 16);
        built.joint_rest.push_back(Vec3{world.m[12], world.m[13], world.m[14]});
    }

    // ---------------------------------------------------------- the clips
    //
    // Channels on nodes that are not joints of the skin move nothing that is
    // drawn, and morph weights are not something this engine draws: both left.
    for (cgltf_size a = 0; a < data->animations_count && skin; ++a) {
        const cgltf_animation &src = data->animations[a];
        ModelClip clip;
        clip.name = src.name ? src.name : "";
        for (cgltf_size c = 0; c < src.channels_count; ++c) {
            const cgltf_animation_channel &ch = src.channels[c];
            ClipChannel keys_of;
            switch (ch.target_path) {
            case cgltf_animation_path_type_translation: keys_of.path = 0; break;
            case cgltf_animation_path_type_rotation:    keys_of.path = 1; break;
            case cgltf_animation_path_type_scale:       keys_of.path = 2; break;
            default: continue;
            }
            keys_of.joint = -1;
            for (int k = 0; k < built.joint_count; ++k)
                if (skin->joints[(size_t)k] == ch.target_node) keys_of.joint = k;
            if (keys_of.joint < 0) continue;

            const cgltf_animation_sampler *smp = ch.sampler;
            const size_t keys  = smp->input->count;
            const int    width = keys_of.path == 1 ? 4 : 3;
            const bool   cubic = smp->interpolation == cgltf_interpolation_type_cubic_spline;
            keys_of.step           = smp->interpolation == cgltf_interpolation_type_step;
            keys_of.times.resize(keys);
            keys_of.values.resize(keys * (size_t)width);
            for (size_t k = 0; k < keys; ++k) {
                // A cubic spline stores in-tangent, value, out-tangent a key.
                if (!cgltf_accessor_read_float(smp->input, k, &keys_of.times[k], 1) ||
                    !cgltf_accessor_read_float(smp->output, cubic ? k * 3 + 1 : k,
                                               &keys_of.values[k * (size_t)width], (cgltf_size)width)) {
                    why = "an animation's keys could not be read";
                    return false;
                }
            }
            if (keys > 0) clip.duration = std::max(clip.duration, keys_of.times.back());
            clip.channels.push_back(std::move(keys_of));
        }
        built.clips.push_back(std::move(clip));
    }

    // --------------------------------------------------------- the geometry
    //
    // By NODE, not by mesh. A mesh's positions are in its node's space, and a
    // model made of rigid parts is only itself through its nodes: the model in
    // games/track has four wheels placed by four nodes and a Sketchfab root that
    // turns Z-up into Y-up, and read by mesh every wheel sat at the origin of a
    // model lying on its side. A skinned mesh is left in its own space -- its
    // joints place it, and glTF says the node carrying it does not.
    bool any_bounds = false;
    for (cgltf_size ni = 0; ni < data->nodes_count; ++ni) {
        const cgltf_node &node = data->nodes[ni];
        if (node.mesh == nullptr) continue;
        const cgltf_mesh &mesh  = *node.mesh;
        const bool        rigid = node.skin == nullptr;

        M4 place;
        if (rigid) cgltf_node_transform_world(&node, place.m);

        // Normals go through the inverse transpose, which for the 3x3 part is
        // its cofactor matrix over the determinant: a wheel scaled 73 one way
        // and 85 another has normals that lean differently from its surface.
        // A negative determinant is a mirror, and a mirror also turns every
        // triangle inside out.
        float cof[3][3];
        for (int r = 0; r < 3; ++r)
            for (int c = 0; c < 3; ++c) {
                const int r1 = (r + 1) % 3, r2 = (r + 2) % 3, c1 = (c + 1) % 3, c2 = (c + 2) % 3;
                cof[r][c] = place.at(r1, c1) * place.at(r2, c2) - place.at(r1, c2) * place.at(r2, c1);
            }
        const float det = place.at(0, 0) * cof[0][0] + place.at(0, 1) * cof[0][1] +
                          place.at(0, 2) * cof[0][2];
        const bool mirrored = det < 0.0f;

        for (cgltf_size pi = 0; pi < mesh.primitives_count; ++pi) {
            const cgltf_primitive &prim = mesh.primitives[pi];
            if (prim.type != cgltf_primitive_type_triangles) continue;

            const cgltf_accessor *pos = nullptr, *nrm = nullptr, *uv = nullptr, *jnt = nullptr,
                                 *wgt = nullptr;
            for (cgltf_size a = 0; a < prim.attributes_count; ++a) {
                const cgltf_attribute &at = prim.attributes[a];
                if (at.index != 0) continue;   // TEXCOORD_1 and friends are not read
                switch (at.type) {
                    case cgltf_attribute_type_position: pos = at.data; break;
                    case cgltf_attribute_type_normal:   nrm = at.data; break;
                    case cgltf_attribute_type_texcoord: uv  = at.data; break;
                    case cgltf_attribute_type_joints:   jnt = at.data; break;
                    case cgltf_attribute_type_weights:  wgt = at.data; break;
                    default: break;
                }
            }
            if (!pos) {
                why = "a primitive has no positions";
                return false;
            }

            const uint32_t base = (uint32_t)built.vertices.size();
            const size_t   n    = pos->count;
            built.vertices.resize(base + n);

            for (size_t v = 0; v < n; ++v) {
                SkinVertex &sv = built.vertices[base + v];

                if (!cgltf_accessor_read_float(pos, v, sv.pos, 3)) {
                    why = "a position could not be read";
                    return false;
                }
                if (nrm) cgltf_accessor_read_float(nrm, v, sv.normal, 3);
                if (uv) cgltf_accessor_read_float(uv, v, sv.uv, 2);

                if (rigid) {
                    const Vec3 p = transform_point(place, Vec3{sv.pos[0], sv.pos[1], sv.pos[2]});
                    sv.pos[0] = p.x;
                    sv.pos[1] = p.y;
                    sv.pos[2] = p.z;

                    float nr[3];
                    for (int r = 0; r < 3; ++r)
                        nr[r] = cof[r][0] * sv.normal[0] + cof[r][1] * sv.normal[1] +
                                cof[r][2] * sv.normal[2];
                    const float len = std::sqrt(nr[0] * nr[0] + nr[1] * nr[1] + nr[2] * nr[2]);
                    if (len > 0.0f) {
                        const float k = (mirrored ? -1.0f : 1.0f) / len;
                        for (int r = 0; r < 3; ++r) sv.normal[r] = nr[r] * k;
                    }
                }

                if (jnt && wgt) {
                    cgltf_uint j4[4] = {0, 0, 0, 0};
                    cgltf_accessor_read_uint(jnt, v, j4, 4);
                    cgltf_accessor_read_float(wgt, v, sv.weights, 4);
                    for (int k = 0; k < 4; ++k) {
                        // What the file says about itself is not trusted. A
                        // joint index past the end of the skeleton is a read
                        // past the end of the bone array in a shader -- a wrong
                        // limb on a good day and a crash on a bad one -- and
                        // clamping it would draw a plausible wrong character
                        // for ever.
                        if ((int)j4[k] >= built.joint_count) {
                            why = "a vertex names a joint this skeleton does not have";
                            return false;
                        }
                        sv.joints[k] = (uint16_t)j4[k];
                    }
                } else {
                    // Rigid: bound to joint 0, or to nothing at all when there
                    // is no skeleton, which is the same arithmetic either way.
                    sv.joints[0] = sv.joints[1] = sv.joints[2] = sv.joints[3] = 0;
                    sv.weights[0] = 1.0f;
                    sv.weights[1] = sv.weights[2] = sv.weights[3] = 0.0f;
                }
            }

            ModelPart part;
            part.first_index = (uint32_t)built.indices.size();
            if (prim.indices) {
                for (size_t i = 0; i < prim.indices->count; ++i)
                    built.indices.push_back(base +
                                            (uint32_t)cgltf_accessor_read_index(prim.indices, i));
            } else {
                for (size_t i = 0; i < n; ++i) built.indices.push_back(base + (uint32_t)i);
            }
            part.index_count = (uint32_t)built.indices.size() - part.first_index;
            if (mirrored)
                for (size_t k = part.first_index; k + 2 < built.indices.size(); k += 3)
                    std::swap(built.indices[k + 1], built.indices[k + 2]);
            part.material =
                prim.material ? (int)cgltf_material_index(data, prim.material) : -1;
            built.parts.push_back(part);

            // The bind pose, vertex by vertex. Not the accessor's min and max:
            // those are the numbers in the file, and what gets drawn is the
            // mesh through its skeleton. On a rig whose joints are identity the
            // two agree, which is what makes the mistake invisible.
            for (size_t v = 0; v < n; ++v) {
                const SkinVertex &sv = built.vertices[base + v];
                const Vec3        p{sv.pos[0], sv.pos[1], sv.pos[2]};

                Vec3  world{0.0f, 0.0f, 0.0f};
                float total = 0.0f;
                if (built.joint_count > 0) {
                    for (int k = 0; k < 4; ++k) {
                        if (sv.weights[k] == 0.0f) continue;
                        const Vec3 t = transform_point(joint_matrix[sv.joints[k]], p);
                        world.x += t.x * sv.weights[k];
                        world.y += t.y * sv.weights[k];
                        world.z += t.z * sv.weights[k];
                        total += sv.weights[k];
                    }
                }
                if (total <= 0.0f) world = p;

                if (!any_bounds) {
                    built.min = built.max = world;
                    any_bounds            = true;
                } else {
                    built.min.x = std::min(built.min.x, world.x);
                    built.min.y = std::min(built.min.y, world.y);
                    built.min.z = std::min(built.min.z, world.z);
                    built.max.x = std::max(built.max.x, world.x);
                    built.max.y = std::max(built.max.y, world.y);
                    built.max.z = std::max(built.max.z, world.z);
                }
            }
        }
    }

    if (built.vertices.empty()) {
        why = "this glTF has no triangles in it";
        return false;
    }

    // --------------------------------------------------------- the materials
    const std::string base_dir = dir_of(path);
    for (cgltf_size i = 0; i < data->materials_count; ++i) {
        const cgltf_material &src = data->materials[i];
        ModelMaterial         mat;
        mat.name         = src.name ? src.name : "";
        mat.double_sided = src.double_sided != 0;
        mat.glow = src.emissive_factor[0] + src.emissive_factor[1] + src.emissive_factor[2] > 0.0f;

        if (src.has_pbr_metallic_roughness) {
            for (int k = 0; k < 4; ++k)
                mat.base_colour[k] = src.pbr_metallic_roughness.base_color_factor[k];
            const cgltf_texture *tex = src.pbr_metallic_roughness.base_color_texture.texture;
            if (tex && tex->image && tex->image->uri) mat.texture = base_dir + tex->image->uri;
        }
        built.materials.push_back(std::move(mat));
    }

    // ------------------------------------------------------- the bind pose out
    //
    // 3x4 ROW major: twelve floats a joint, the projection row dropped. See
    // kMaxJoints for why the shape and not the count is what matters.
    built.bind_pose.resize((size_t)built.joint_count * 12);
    for (int j = 0; j < built.joint_count; ++j) {
        const M4 &t = joint_matrix[(size_t)j];
        for (int row = 0; row < 3; ++row)
            for (int col = 0; col < 4; ++col)
                built.bind_pose[(size_t)j * 12 + (size_t)row * 4 + (size_t)col] = t.at(row, col);
    }

    out = std::move(built);
    return true;
}

namespace {

// A channel at a time, into `v`: held before the first key and after the last.
void sample(const ClipChannel &ch, float t, float *v)
{
    const int    width = ch.path == 1 ? 4 : 3;
    const size_t n     = ch.times.size();
    if (n == 0) return;
    const size_t hi = (size_t)(std::upper_bound(ch.times.begin(), ch.times.end(), t) - ch.times.begin());
    if (hi == 0 || hi == n || ch.step) {
        const size_t k = hi == 0 ? 0 : hi - 1;
        std::memcpy(v, &ch.values[k * (size_t)width], sizeof(float) * (size_t)width);
        return;
    }
    const float  span = ch.times[hi] - ch.times[hi - 1];
    const float  f    = span > 0.0f ? (t - ch.times[hi - 1]) / span : 0.0f;
    const float *a    = &ch.values[(hi - 1) * (size_t)width];
    const float *b    = &ch.values[hi * (size_t)width];
    float        sign = 1.0f;
    if (width == 4 && a[0] * b[0] + a[1] * b[1] + a[2] * b[2] + a[3] * b[3] < 0.0f) sign = -1.0f;
    for (int i = 0; i < width; ++i) v[i] = a[i] + (b[i] * sign - a[i]) * f;
}

// `into` moved `f` of the way to `to`, ten floats of translation, rotation and
// scale. A rotation takes the short way round and is normalised after: a
// linear blend of two quaternions is close enough to a slerp at these angles.
void blend(float *into, const float *to, float f)
{
    const float d = into[3] * to[3] + into[4] * to[4] + into[5] * to[5] + into[6] * to[6];
    for (int i = 0; i < 10; ++i) {
        const float target = (i >= 3 && i < 7 && d < 0.0f) ? -to[i] : to[i];
        into[i] += (target - into[i]) * f;
    }
    const float len = std::sqrt(into[3] * into[3] + into[4] * into[4] + into[5] * into[5] +
                                into[6] * into[6]);
    if (len > 0.0f)
        for (int i = 3; i < 7; ++i) into[i] /= len;
}

// T * R * S, then turned about its own Y between the rotation and the scale.
M4 compose(const float *trs, float turn)
{
    float x = trs[3], y = trs[4], z = trs[5], w = trs[6];
    if (turn != 0.0f) {   // the rotation times one about Y
        const float s = std::sin(0.5f * turn), c = std::cos(0.5f * turn);
        const float nx = x * c - z * s, ny = y * c + w * s, nz = z * c + x * s, nw = w * c - y * s;
        x = nx, y = ny, z = nz, w = nw;
    }
    M4 m;
    m.m[0]  = (1 - 2 * (y * y + z * z)) * trs[7];
    m.m[1]  = (2 * (x * y + z * w)) * trs[7];
    m.m[2]  = (2 * (x * z - y * w)) * trs[7];
    m.m[4]  = (2 * (x * y - z * w)) * trs[8];
    m.m[5]  = (1 - 2 * (x * x + z * z)) * trs[8];
    m.m[6]  = (2 * (y * z + x * w)) * trs[8];
    m.m[8]  = (2 * (x * z + y * w)) * trs[9];
    m.m[9]  = (2 * (y * z - x * w)) * trs[9];
    m.m[10] = (1 - 2 * (x * x + y * y)) * trs[9];
    m.m[12] = trs[0];
    m.m[13] = trs[1];
    m.m[14] = trs[2];
    return m;
}

} // namespace

void pose(const SkinnedModel &model, const ClipAt *layers, int layer_count, const float *turn,
          const int8_t *show, std::vector<float> &out)
{
    const int n = model.joint_count;

    // Every joint at rest, then each layer over it.
    std::vector<float> trs(model.joint_trs);
    std::vector<float> clip_trs;
    for (int l = 0; l < layer_count; ++l) {
        const ClipAt &at = layers[l];
        if (at.clip < 0 || (size_t)at.clip >= model.clips.size() || at.weight <= 0.0f) continue;
        clip_trs = model.joint_trs;
        // Only the joints the clip moves: a clip of the arms alone laid over
        // a walk leaves the legs walking (front_game's guns held and aimed).
        std::vector<char> moves((size_t)n, 0);
        for (const ClipChannel &ch : model.clips[(size_t)at.clip].channels) {
            sample(ch, at.time, &clip_trs[(size_t)ch.joint * 10 + (ch.path == 0 ? 0 : ch.path == 1 ? 3 : 7)]);
            if (ch.joint >= 0 && ch.joint < n) moves[(size_t)ch.joint] = 1;
        }
        for (int j = 0; j < n; ++j)
            if (moves[(size_t)j] || l == 0)
                blend(&trs[(size_t)j * 10], &clip_trs[(size_t)j * 10], std::min(at.weight, 1.0f));
    }

    if (show)
        for (int j = 0; j < n; ++j)
            if (show[j] >= 0)
                for (int k = 7; k < 10; ++k) trs[(size_t)j * 10 + (size_t)k] = (float)show[j];

    std::vector<M4>   world((size_t)n);
    std::vector<char> done((size_t)n, 0);

    // Parents first, whatever order the skin lists its joints in.
    auto place = [&](auto &&self, int j) -> void {
        if (done[(size_t)j]) return;
        const int p = model.joint_parent[(size_t)j];
        if (p >= 0) self(self, p);
        M4 parent;
        if (p >= 0) parent = world[(size_t)p];
        else std::memcpy(parent.m, &model.joint_base[(size_t)j * 16], sizeof(parent.m));
        world[(size_t)j] = multiply(parent, compose(&trs[(size_t)j * 10], turn ? turn[j] : 0.0f));
        done[(size_t)j]  = 1;
    };

    out.resize((size_t)n * 12);
    for (int j = 0; j < n; ++j) {
        place(place, j);
        M4 inverse_bind;
        std::memcpy(inverse_bind.m, &model.joint_inverse_bind[(size_t)j * 16], sizeof(inverse_bind.m));
        const M4 t = multiply(world[(size_t)j], inverse_bind);
        for (int row = 0; row < 3; ++row)
            for (int col = 0; col < 4; ++col)
                out[(size_t)j * 12 + (size_t)row * 4 + (size_t)col] = t.at(row, col);
    }
}

} // namespace cnp
