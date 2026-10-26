#include <algorithm>
#include "client/render/model_store.h"

#include "core/media/content_hash.h"

#include <filesystem>
#include <fstream>

namespace cnp {

const ModelStore::Entry *ModelStore::entry(uint16_t id) const
{
    if (id == 0 || (size_t)id > m_by_id.size()) return nullptr;
    const std::unique_ptr<Entry> &e = m_by_id[(size_t)id - 1];
    return (e && e->ready) ? e.get() : nullptr;
}

ModelStore::Entry *ModelStore::entry(uint16_t id)
{
    return const_cast<Entry *>(static_cast<const ModelStore *>(this)->entry(id));
}

void ModelStore::clear()
{
    for (std::unique_ptr<Entry> &e : m_by_id)
        if (e && e->ready) e->renderer.shutdown();
    m_by_id.clear();
    m_refused = 0;
}

bool ModelStore::put(uint16_t id, const std::string &name, const std::string &path,
                     std::string &why)
{
    if (id == 0) {
        why = "0 is the id of a body wearing nothing, and nothing can be put there";
        return false;
    }
    if ((size_t)id > m_by_id.size()) m_by_id.resize((size_t)id);

    auto e  = std::make_unique<Entry>();
    e->name = name;
    e->file = path;

    if (!load_gltf(path, e->model, why)) return false;
    if (!e->renderer.init()) {
        why = "the renderer would not start for " + name;
        return false;
    }
    if (!e->renderer.upload(e->model, why)) {
        e->renderer.shutdown();
        return false;
    }

    e->ready              = true;
    m_by_id[(size_t)id - 1] = std::move(e);
    return true;
}

namespace {

bool write_beside(const std::string &dir, const std::string &file,
                  const std::vector<uint8_t> &bytes)
{
    std::ofstream f(dir + "/" + file, std::ios::binary | std::ios::trunc);
    if (!f) return false;
    f.write((const char *)bytes.data(), (std::streamsize)bytes.size());
    return (bool)f;
}

} // namespace

int ModelStore::load_from_media(
    const std::vector<std::string>                                                   &names,
    const std::function<bool(const std::string &, std::vector<uint8_t> &)>           &media,
    const std::string &cache_dir, std::string &why, size_t first, size_t count)
{
    if (cache_dir.empty() || !media) return 0;

    int          have = 0;
    const size_t end  = first + std::min(count, names.size() - std::min(first, names.size()));
    for (size_t i = first; i < end; ++i) {
        const std::string entry = model_media_name(names[i], kModelMediaEntry);
        if (entry.empty()) {
            ++m_refused;
            why += "model '" + names[i] + "' is not a name a file can be served under\n";
            continue;
        }

        std::vector<uint8_t> gltf_bytes;
        if (!media(entry, gltf_bytes) || gltf_bytes.empty()) continue;   // this world serves none

        // Named by what it IS. Two worlds serving the same character rebuild
        // one folder, and a model that changes gets a new one rather than
        // being written over while something is reading it.
        const std::string dir =
            cache_dir + "/" + hash_content(gltf_bytes).hex();

        std::error_code ec;
        std::filesystem::create_directories(dir, ec);
        if (ec || !write_beside(dir, kModelMediaEntry, gltf_bytes)) {
            why += "model '" + names[i] + "': could not write it into " + dir + "\n";
            continue;
        }

        // What else it is made of, asked of the .gltf itself rather than
        // guessed: the names it calls its buffer and its images by are the only
        // names the loader will look for, and they are refused there if any of
        // them could climb out of this directory.
        std::vector<std::string> beside;
        std::string              said;
        if (!gltf_files_beside(dir + "/" + kModelMediaEntry, beside, said)) {
            why += "model '" + names[i] + "': " + said + "\n";
            continue;
        }

        bool whole = true;
        for (const std::string &f : beside) {
            std::vector<uint8_t> part;
            if (!media(model_media_name(names[i], f), part) || part.empty() ||
                !write_beside(dir, f, part)) {
                whole = false;
                why += "model '" + names[i] + "': this world did not serve " + f +
                       ", which its .gltf names\n";
                break;
            }
        }
        if (!whole) continue;   // half a model is a character with a hole in it

        std::string one;
        if (!put((uint16_t)(i + 1), names[i], dir + "/" + kModelMediaEntry, one)) {
            why += "model '" + names[i] + "': " + one + "\n";
            continue;
        }
        ++have;
    }
    return have;
}

int ModelStore::load(const std::vector<std::string> &names, const std::string &dir,
                     std::string &why)
{
    clear();
    return fill_from_dir(names, dir, why);
}

int ModelStore::fill_from_dir(const std::vector<std::string> &names, const std::string &dir,
                              std::string &why, size_t first, size_t count)
{
    if (dir.empty()) return 0;

    int          have = 0;
    const size_t end  = first + std::min(count, names.size() - std::min(first, names.size()));
    for (size_t i = first; i < end; ++i) {
        if (has((uint16_t)(i + 1))) continue;   // this world already sent it

        std::string file;
        if (!model_file_name(names[i], file)) {
            ++m_refused;
            why += "model '" + names[i] + "' is not a name a file can be called\n";
            continue;
        }

        std::string one;
        if (!put((uint16_t)(i + 1), names[i], dir + "/" + file, one)) {
            // Not an error and not counted as a refusal: a world may name a
            // model this player has never downloaded, and that body is drawn
            // as the box every body was drawn as before.
            why += "model '" + names[i] + "': " + one + "\n";
            continue;
        }
        ++have;
    }
    return have;
}

} // namespace cnp
