#include "core/media/tile_bake.h"

#include <cmath>

namespace cnp {
namespace {

// FNV-1a over the name. Any stable hash would do; this one is four lines and
// needs nothing linked.
uint32_t name_seed(const std::string &s)
{
    uint32_t h = 2166136261u;
    for (const char c : s) {
        h ^= (uint8_t)c;
        h *= 16777619u;
    }
    return h ? h : 1u;
}

uint32_t hash2(uint32_t seed, int x, int y)
{
    uint32_t h = seed;
    h ^= (uint32_t)(x * 374761393);
    h *= 2246822519u;
    h ^= (uint32_t)(y * 668265263);
    h *= 3266489917u;
    h ^= h >> 15;
    return h;
}

float lattice(uint32_t seed, int x, int y)
{
    return (float)(hash2(seed, x, y) & 0xFFFFu) / 65535.0f;
}

// Value noise on a lattice, wrapped so the tile repeats without a seam. A block
// face is tiled against itself in every direction, and noise that does not wrap
// draws a grid over the whole world.
float wrapped_noise(uint32_t seed, float u, float v, int cells)
{
    const float fx = u * (float)cells;
    const float fy = v * (float)cells;
    const int   x0 = (int)std::floor(fx), y0 = (int)std::floor(fy);
    const float tx = fx - (float)x0, ty = fy - (float)y0;

    auto wrap = [cells](int a) { return ((a % cells) + cells) % cells; };
    const float a = lattice(seed, wrap(x0), wrap(y0));
    const float b = lattice(seed, wrap(x0 + 1), wrap(y0));
    const float c = lattice(seed, wrap(x0), wrap(y0 + 1));
    const float d = lattice(seed, wrap(x0 + 1), wrap(y0 + 1));

    const float sx = tx * tx * (3.0f - 2.0f * tx);
    const float sy = ty * ty * (3.0f - 2.0f * ty);
    return (a + (b - a) * sx) + ((c + (d - c) * sx) - (a + (b - a) * sx)) * sy;
}

} // namespace

std::vector<uint8_t> bake_tile(const NodeDef &def, int size, bool border)
{
    if (size <= 0) return {};

    std::vector<uint8_t> px((size_t)size * (size_t)size * 4, 0);
    const uint32_t       seed = name_seed(def.name);

    for (int y = 0; y < size; ++y) {
        for (int x = 0; x < size; ++x) {
            const float u = ((float)x + 0.5f) / (float)size;
            const float v = ((float)y + 0.5f) / (float)size;

            // Two octaves of mottling, so a flat colour reads as a material
            // rather than as a missing texture. Small: this is a placeholder
            // and it should look like one, not like an attempt at art.
            float k = 1.0f;
            k += (wrapped_noise(seed, u, v, 4) - 0.5f) * 0.22f;
            k += (wrapped_noise(seed * 2654435761u, u, v, 8) - 0.5f) * 0.10f;

            // The checker the node asked for, as a third band rather than as
            // the whole texture.
            if (def.checker && ((((x * 4) / size) + ((y * 4) / size)) & 1) != 0) k *= 1.08f;

            // A darker border. It reads as a bevel and it is what makes the
            // edges of blocks visible at all in a world of one colour.
            const bool edge = border && (x == 0 || y == 0 || x == size - 1 || y == size - 1);
            if (edge) k *= 0.62f;

            uint8_t *p = &px[((size_t)y * (size_t)size + (size_t)x) * 4];
            for (int c = 0; c < 3; ++c) {
                const float value = (float)def.colour[c] * k;
                p[c] = (uint8_t)(value < 0.0f ? 0.0f : (value > 255.0f ? 255.0f : value));
            }

            // The node's opacity, carried in the texture rather than in a
            // uniform. A uniform would be one transparency for the whole alpha
            // pass, so glass and water could not differ.
            p[3] = def.opacity;
        }
    }
    return px;
}

std::vector<uint8_t> bake_face_tile(const NodeDef &def, const std::string &word, int size,
                                    bool border)
{
    std::vector<uint8_t> px = bake_tile(def, size, border);
    if (px.empty() || size <= 0) return px;

    // Rings for a top, a bullseye for a front, and nothing for anything else.
    // Drawn ON the placeholder rather than instead of it, so a marked face is
    // recognisably the same block -- a log's end is still that log's colour.
    const bool rings    = (word == "top" || word == "bottom");
    const bool bullseye = (word == "front");
    if (!rings && !bullseye) return px;

    const float centre = (float)size * 0.5f;
    const float unit   = (float)size * 0.5f;

    for (int y = 0; y < size; ++y) {
        for (int x = 0; x < size; ++x) {
            const float dx = ((float)x + 0.5f) - centre;
            const float dy = ((float)y + 0.5f) - centre;
            const float r  = std::sqrt(dx * dx + dy * dy) / unit;   // 0 at the middle, 1 at the edge

            float k = 1.0f;
            if (rings) {
                // Six bands, darker and lighter in turn. Concentric, so the
                // mark says which way the AXIS points and not which way round
                // the face is -- which is exactly what a facedir block's top
                // has to say and all it can.
                const int band = (int)(r * 6.0f);
                k = (band & 1) ? 0.78f : 1.16f;
            } else {
                if (r < 0.30f)      k = 0.66f;
                else if (r < 0.46f) k = 1.24f;
                else                k = 1.0f;
            }
            if (k == 1.0f) continue;

            uint8_t *p = &px[((size_t)y * (size_t)size + (size_t)x) * 4];
            for (int c = 0; c < 3; ++c) {
                const float value = (float)p[c] * k;
                p[c] = (uint8_t)(value < 0.0f ? 0.0f : (value > 255.0f ? 255.0f : value));
            }
        }
    }
    return px;
}

std::string texture_file_name(const std::string &node_name)
{
    std::string out;
    out.reserve(node_name.size() + 4);
    for (const char c : node_name) out.push_back(c == ':' ? '_' : c);
    out += ".png";
    return out;
}

} // namespace cnp
