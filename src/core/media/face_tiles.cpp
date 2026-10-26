#include "core/media/face_tiles.h"

#include "core/media/tile_bake.h"

#include <cmath>
#include <unordered_map>

namespace cnp {
namespace {

// The word for one face, and the word for the group it belongs to. A face whose
// two words are the same -- top and bottom are groups of one -- asks twice for
// the same file, and face_texture_names drops the repeat.
struct FaceWords {
    const char *own;
    const char *group;
};

constexpr FaceWords kWords[6] = {
    {"right",  "side"},     // +X
    {"left",   "side"},     // -X
    {"top",    "top"},      // +Y
    {"bottom", "bottom"},   // -Y
    {"front",  "side"},     // +Z
    {"back",   "side"},     // -Z
};

// The file a block wears on a named face: the plain name with the word before
// the extension, so core:grass and "top" make core_grass_top.png. Built from
// texture_file_name so the colon rule lives in one place.
std::string named_file(const std::string &node_name, const char *word)
{
    std::string base = texture_file_name(node_name);   // "core_grass.png"
    const size_t dot = base.rfind('.');
    if (dot == std::string::npos) return base + "_" + word;
    return base.substr(0, dot) + "_" + word + base.substr(dot);
}

} // namespace

int turn_towards(float look_x, float look_z)
{
    // The front after `turn` turns is turned_face(kFaceFront, turn): +Z, +X, -Z,
    // -X for 0..3. Wanted is the opposite of the look direction, so a player
    // looking along +x wants a front along -x, which is turn 3.
    if (std::fabs(look_x) > std::fabs(look_z)) return look_x > 0.0f ? 3 : 1;
    return look_z > 0.0f ? 2 : 0;
}

uint8_t faces_named(const std::string &word)
{
    if (word == "all")  return 0x3F;
    if (word == "side") return (1 << kFaceRight) | (1 << kFaceLeft) | (1 << kFaceFront) |
                               (1 << kFaceBack);
    for (int f = 0; f < 6; ++f)
        if (word == kWords[f].own) return (uint8_t)(1 << f);
    return 0;
}

std::string face_texture_name(const std::string &node_name, const std::string &word)
{
    if (word.empty() || word == "all") return texture_file_name(node_name);
    return named_file(node_name, word.c_str());
}

std::vector<std::string> face_texture_names(const std::string &node_name, int face)
{
    std::vector<std::string> out;
    if (face < 0 || face >= 6) {
        out.push_back(texture_file_name(node_name));
        return out;
    }

    out.push_back(named_file(node_name, kWords[face].own));

    const std::string group = named_file(node_name, kWords[face].group);
    if (group != out.front()) out.push_back(group);

    out.push_back(texture_file_name(node_name));
    return out;
}

std::vector<std::string> all_texture_names(const std::string &node_name)
{
    std::vector<std::string> out;
    for (int f = 0; f < 6; ++f)
        for (const std::string &n : face_texture_names(node_name, f)) {
            bool seen = false;
            for (const std::string &had : out) seen = seen || had == n;
            if (!seen) out.push_back(n);
        }
    return out;
}

TilePlan plan_tiles(const std::vector<std::string> &node_names, const TileIdentity &picture_of,
                    int max_tiles)
{
    std::vector<NodeShape> shapes(node_names.size());
    for (size_t i = 0; i < node_names.size(); ++i) shapes[i].name = node_names[i];
    return plan_tiles(shapes, picture_of, max_tiles);
}

TilePlan plan_tiles(const std::vector<NodeShape> &nodes, const TileIdentity &picture_of,
                    int max_tiles)
{
    return plan_tiles(nodes, std::vector<std::string>{}, picture_of, max_tiles);
}

TilePlan plan_tiles(const std::vector<NodeShape> &nodes, const std::vector<std::string> &items,
                    const TileIdentity &picture_of, int max_tiles)
{
    TilePlan plan;
    plan.face_tile.resize(nodes.size());

    if (max_tiles < 1) max_tiles = 1;

    // Keyed on the PICTURE and not on the file name. A pack whose manifest
    // sends six blocks to one flat.png asks under six engine names, and six
    // identical tiles is six tiles of a budget of 256 -- TODO.md OPEN-26. It
    // also catches two genuinely different files holding the same picture,
    // which a name can never catch.
    std::unordered_map<std::string, uint16_t> by_picture;
    std::unordered_map<int, uint16_t>         by_node;   // the baked ones

    // Asking about the same file twice is asking the disk twice, and with a
    // remote world it is asking the media cache twice. Once each.
    std::unordered_map<std::string, std::string> answered;
    auto supplied = [&](const std::string &file) -> const std::string & {
        const auto it = answered.find(file);
        if (it != answered.end()) return it->second;
        return answered.emplace(file, picture_of ? picture_of(file) : std::string()).first->second;
    };

    for (size_t id = 0; id < nodes.size(); ++id) {
        for (int f = 0; f < 6; ++f) {
            // Air is the loud red and takes no art: giving it a file would put a
            // picture on a block that has no faces, and the day one appears in
            // the world is the day that colour has to be unmistakable.
            std::string chosen;
            std::string picture;
            if (id != 0)
                for (const std::string &name : face_texture_names(nodes[id].name, f)) {
                    const std::string &id_of = supplied(name);
                    if (!id_of.empty()) {
                        chosen  = name;
                        picture = id_of;
                        break;
                    }
                }

            uint16_t tile = 0;
            bool     have_tile = false;

            if (!chosen.empty()) {
                const auto it = by_picture.find(picture);
                if (it != by_picture.end()) {
                    tile      = it->second;
                    have_tile = true;
                }
            } else {
                const auto it = by_node.find((int)id);
                if (it != by_node.end()) {
                    tile      = it->second;
                    have_tile = true;
                }
            }

            if (!have_tile) {
                if ((int)plan.tiles.size() < max_tiles) {
                    tile = (uint16_t)plan.tiles.size();
                    plan.tiles.push_back(TileSource{chosen, (int)id});
                    if (chosen.empty()) by_node.emplace((int)id, tile);
                    else                by_picture.emplace(picture, tile);
                } else {
                    // No room. Wear something this block already wears rather
                    // than something another block does, and count it.
                    ++plan.over_budget;
                    tile = 0;
                    for (int g = 0; g < f; ++g) {
                        tile = plan.face_tile[id][(size_t)g];
                        break;
                    }
                }
            }

            plan.face_tile[id][(size_t)f] = tile;
        }
    }

    // And the items, AFTER every block, so that wanting a picture for an item
    // cannot move a block's tile: a world whose items nobody drew comes out
    // numbered exactly as it did before items could be drawn at all.
    //
    // One file each and no chain: an item is a square in a bag, never a cube,
    // so there is no top, no side and no face to fall back to. What it falls
    // back to is nothing, and the bag decides what to do about that -- the
    // block it places, or a plain square.
    plan.item_tile.assign(items.size(), 0);

    for (size_t id = 0; id < items.size(); ++id) {
        if (items[id].empty()) continue;   // item 0 is "nothing at all"

        const std::string  file    = texture_file_name(items[id]);
        const std::string &picture = supplied(file);
        if (picture.empty()) continue;     // nobody drew it

        const auto it = by_picture.find(picture);
        if (it != by_picture.end()) {
            plan.item_tile[id] = it->second;
            continue;
        }

        if ((int)plan.tiles.size() >= max_tiles) {
            // Nothing rather than somebody else's picture, and counted: a bag
            // square wearing the wrong art is worse than a bag square wearing
            // none, because the wrong one is believed.
            ++plan.over_budget;
            continue;
        }

        const uint16_t tile = (uint16_t)plan.tiles.size();
        plan.tiles.push_back(TileSource{file, 0});
        by_picture.emplace(picture, tile);
        plan.item_tile[id] = tile;
    }

    return plan;
}

} // namespace cnp
