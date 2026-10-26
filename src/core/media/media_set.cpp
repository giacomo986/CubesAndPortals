#include "core/media/media_set.h"

#include <unordered_set>

#include "core/media/face_tiles.h"
#include "core/media/gltf.h"
#include "core/media/texture_pack.h"

#include "core/media/png.h"
#include "core/media/tile_bake.h"

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>

namespace cnp {

bool MediaSet::add(const std::string &name, std::vector<uint8_t> bytes)
{
    if (name.empty() || bytes.empty() || bytes.size() > kMaxMediaBytes) return false;
    if (m_by_name.size() >= kMaxMediaItems && m_by_name.find(name) == m_by_name.end())
        return false;

    const ContentHash h = hash_content(bytes);
    m_by_name[name]     = h;

    // The store is keyed by the hash, which is where the deduplication comes
    // from: two names for the same content are one entry because they are the
    // same key. The test that says so is the one that keys it by NAME instead
    // and watches the blob count double.
    //
    // The lookup below saves moving the bytes, and nothing else -- emplace on a
    // key already present does nothing anyway. Said here rather than left to
    // read as the dedup itself.
    if (m_blobs.find(h) == m_blobs.end()) {
        MediaBlob blob;
        blob.hash  = h;
        blob.bytes = std::move(bytes);
        m_blobs.emplace(h, std::move(blob));
    }
    return true;
}

const ContentHash *MediaSet::hash_for(const std::string &name) const
{
    auto it = m_by_name.find(name);
    return it == m_by_name.end() ? nullptr : &it->second;
}

const MediaBlob *MediaSet::find(const ContentHash &hash) const
{
    auto it = m_blobs.find(hash);
    return it == m_blobs.end() ? nullptr : &it->second;
}

size_t MediaSet::total_bytes() const
{
    size_t n = 0;
    for (const auto &kv : m_blobs) n += kv.second.bytes.size();
    return n;
}

std::vector<std::pair<std::string, ContentHash>> MediaSet::table() const
{
    std::vector<std::pair<std::string, ContentHash>> out;
    out.reserve(m_by_name.size());
    for (const auto &kv : m_by_name) out.emplace_back(kv.first, kv.second);
    std::sort(out.begin(), out.end(),
              [](const auto &a, const auto &b) { return a.first < b.first; });
    return out;
}

// The directory a path is in, with the separator. A model's parts are named
// relative to its .gltf and not to whoever is running the server.
static std::string dir_of(const std::string &path)
{
    const size_t cut = path.find_last_of("/\\");
    return (cut == std::string::npos) ? std::string() : path.substr(0, cut + 1);
}

static bool read_file(const std::string &path, std::vector<uint8_t> &out)
{
    FILE *f = std::fopen(path.c_str(), "rb");
    if (!f) return false;

    out.clear();
    uint8_t buf[8192];
    size_t  n;
    while ((n = std::fread(buf, 1, sizeof(buf), f)) > 0) {
        out.insert(out.end(), buf, buf + n);
        if (out.size() > kMaxMediaBytes) {   // refuse before it costs memory
            std::fclose(f);
            out.clear();
            return false;
        }
    }
    std::fclose(f);
    return !out.empty();
}

MediaBuildStats build_media_set(const NodeRegistry &reg, const std::string &dir, MediaSet &out,
                                const std::string &fallback)
{
    MediaBuildStats stats;

    // The folder may be a pack with a manifest in it. Loading it here rather
    // than at the call site is what keeps "a server dresses its world in a
    // pack" from being a thing every caller has to remember to do.
    TexturePack pack;
    std::string why;
    if (!dir.empty() && !load_texture_pack(dir, pack, why))
        std::fprintf(stderr, "media: %s\n", why.c_str());

    for (size_t id = 0; id < reg.size(); ++id) {
        const NodeDef &def = reg.get((BlockId)id);
        if (def.name.empty() || def.name == "core:air") {
            ++stats.skipped;
            continue;
        }

        // Every file any face could ask for, not just the block's plain one.
        // A pack that dresses the top of grass differently from its sides has
        // to reach a client whole, and the client asks for the per-face names by
        // itself -- so a server that offered only the plain one would serve a
        // world that looks like this build's and is not.
        //
        // The NAME a client asks for is this engine's, always. The pack's
        // manifest only decides which file in the folder answers for it, and it
        // is resolved here, on the side that owns the folder -- so the protocol
        // never learns that packs exist.
        const std::string plain = texture_file_name(def.name);
        bool              plain_served = false;

        std::vector<uint8_t> bytes;
        for (const std::string &file : all_texture_names(def.name)) {
            if ((dir.empty() || !read_file(dir + "/" + pack_file(pack, file), bytes)) &&
                (fallback.empty() || !read_file(fallback + "/" + file, bytes)))
                continue;
            if (!out.add(file, std::move(bytes))) {
                ++stats.skipped;
                continue;
            }
            ++stats.from_file;
            plain_served = plain_served || file == plain;
        }

        // Whatever else a pack did or did not supply, a block always has its
        // plain file: it is the last resort of every face's chain, and a block
        // whose chain runs out has no picture at all.
        if (!plain_served) {
            const std::vector<uint8_t> px = bake_tile(def);
            bytes = png_encode_rgba(px.data(), kTilePixels, kTilePixels);
            if (bytes.empty()) {
                ++stats.skipped;
                continue;
            }
            if (!out.add(plain, std::move(bytes))) {
                ++stats.skipped;
                continue;
            }
            ++stats.generated;
        }
    }

    // And the pack a second time, as ITSELF.
    //
    // Everything above put the pack's pictures in under the ENGINE's names,
    // which is what the atlas asks for and is why the protocol never had to
    // learn that packs exist. It also means a player cannot keep a world's
    // pack, put it on another world, or cycle to it -- TODO.md OPEN-25. So the
    // manifest goes in under one reserved name and each file it points at under
    // its own, and the other side rebuilds the folder from those.
    //
    // It costs no BLOB: the set is content-addressed, so a second name for
    // bytes already in it is one row in a table.
    if (pack.manifest) {
        std::vector<uint8_t> bytes;
        if (read_file(dir + "/" + kPackManifest, bytes) && out.add(kPackMediaManifest,
                                                                   std::move(bytes))) {
            stats.pack_served = true;

            // The targets, deduplicated: a manifest that sends six blocks to
            // one file names it six times and it travels once.
            std::unordered_set<std::string> sent;
            for (const auto &entry : pack.alias) {
                const std::string &target = entry.second;
                if (!sent.insert(target).second) continue;

                const std::string name = pack_media_name(target);
                std::vector<uint8_t> file;
                if (name.empty() || !read_file(dir + "/" + target, file) ||
                    !out.add(name, std::move(file))) {
                    // A target the manifest names and this machine cannot read
                    // is a target the server's OWN atlas falls through on, so a
                    // client without it behaves the same way. Counted, not
                    // fatal: the two folders still agree, which is the whole
                    // claim being made here.
                    ++stats.pack_missing;
                    continue;
                }
                ++stats.pack_files;
            }
        }
    }

    stats.bytes = out.total_bytes();
    return stats;
}

ItemMediaStats add_items_to_media(const ItemRegistry &items, const std::string &dir,
                                  MediaSet &out, const std::string &fallback)
{
    ItemMediaStats stats;

    TexturePack pack;
    std::string why;
    if (!dir.empty() && !load_texture_pack(dir, pack, why))
        std::fprintf(stderr, "media: %s\n", why.c_str());

    for (size_t id = 0; id < items.size(); ++id) {
        const ItemDef &def = items.get((ItemId)id);
        if (def.name.empty()) {
            ++stats.skipped;
            continue;
        }

        // The same two-step every block goes through: the engine's name is what
        // travels, and the pack's manifest only decides which file in the folder
        // answers for it -- resolved here, on the side that owns the folder.
        const std::string    file = texture_file_name(def.name);
        std::vector<uint8_t> bytes;
        if (((dir.empty() || !read_file(dir + "/" + pack_file(pack, file), bytes)) &&
             (fallback.empty() || !read_file(fallback + "/" + file, bytes))) ||
            !out.add(file, std::move(bytes))) {
            ++stats.skipped;
            continue;
        }
        ++stats.files;
    }

    stats.bytes = out.total_bytes();
    return stats;
}

ModelMediaStats add_models_to_media(const ModelRegistry &models, MediaSet &out, std::string &why)
{
    ModelMediaStats stats;
    const size_t    before = out.total_bytes();

    for (ModelId id = 1; id <= (ModelId)models.size(); ++id) {
        const ModelRegistry::ModelDef &def = models.get(id);
        if (def.name.empty() || def.file.empty()) continue;

        // Everything this model is made of, refused whole rather than in part:
        // a .gltf served without the buffer it names is a character-shaped
        // hole on the other side, and nobody looking at one can tell which
        // file went missing.
        std::vector<std::string> beside;
        std::string              said;
        if (!gltf_files_beside(def.file, beside, said)) {
            ++stats.skipped;
            why += "model '" + def.name + "': " + said + "\n";
            continue;
        }

        std::vector<uint8_t> gltf_bytes;
        if (!read_file(def.file, gltf_bytes)) {
            ++stats.skipped;
            why += "model '" + def.name + "': could not read " + def.file + "\n";
            continue;
        }

        // Read them all BEFORE any of them is added, so a model missing its
        // second file leaves nothing of itself in the set.
        const std::string             dir = dir_of(def.file);
        std::vector<std::vector<uint8_t>> parts;
        bool                          whole = true;
        for (const std::string &f : beside) {
            std::vector<uint8_t> bytes;
            if (!read_file(dir + f, bytes)) {
                whole = false;
                why += "model '" + def.name + "': could not read " + f + ", named by its .gltf\n";
                break;
            }
            parts.push_back(std::move(bytes));
        }
        if (!whole) {
            ++stats.skipped;
            continue;
        }

        const std::string entry = model_media_name(def.name, kModelMediaEntry);
        if (entry.empty() || !out.add(entry, std::move(gltf_bytes))) {
            ++stats.skipped;
            why += "model '" + def.name + "': its name is not one a file can be served under\n";
            continue;
        }
        ++stats.files;

        for (size_t i = 0; i < beside.size(); ++i) {
            if (out.add(model_media_name(def.name, beside[i]), std::move(parts[i])))
                ++stats.files;
        }

        ++stats.models;
    }

    stats.bytes = out.total_bytes() - before;
    return stats;
}

std::string sound_media_name(const std::string &mod, const std::string &file)
{
    return "sound." + mod + "." + file;
}

int add_mod_sounds_to_media(const std::string &mod, const std::string &dir, MediaSet &out)
{
    std::error_code ec;
    int             n = 0;
    for (const auto &e : std::filesystem::directory_iterator(dir, ec)) {
        const std::string ext = e.path().extension().string();
        if ((ext != ".ogg" && ext != ".wav") || !e.is_regular_file(ec)) continue;
        std::ifstream        f(e.path(), std::ios::binary);
        std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
        if (out.add(sound_media_name(mod, e.path().filename().string()), std::move(bytes))) ++n;
    }
    return n;
}

} // namespace cnp
