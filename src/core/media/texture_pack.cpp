#include "core/media/texture_pack.h"

#include "core/media/face_tiles.h"
#include "core/media/tile_bake.h"
#include "core/media/content_hash.h"

#include <sys/stat.h>

#include <filesystem>
#include <system_error>
#include <cstdio>

namespace cnp {
namespace {

// The longest a file name may be. Not a buffer size -- everything here is a
// std::string -- but a limit on what a manifest can make this engine go and
// stat, since a manifest can arrive from anywhere.
constexpr size_t kMaxTargetName = 128;

std::string trim(const std::string &s)
{
    size_t a = 0, b = s.size();
    while (a < b && (s[a] == ' ' || s[a] == '\t' || s[a] == '\r')) ++a;
    while (b > a && (s[b - 1] == ' ' || s[b - 1] == '\t' || s[b - 1] == '\r')) --b;
    return s.substr(a, b - a);
}

// A target must be a PLAIN FILE NAME inside the pack folder.
//
// Written as one function with every rule in it rather than as a check at the
// point of use, because the rules are not independent: a parser that stops at
// '/' catches four ways out of the folder and lets '..' and a backslash
// through, and each one that gets through is a file the player can read.
bool name_is_inside_the_pack(const std::string &v)
{
    if (v.empty() || v.size() > kMaxTargetName) return false;
    if (v[0] == '.') return false;   // ".", "..", and hidden files, in one rule
    for (const char c : v)
        if (c == '/' || c == '\\' || c == '\0') return false;
    return true;
}

// The folder's own name, for a pack that did not say what it is called.
std::string leaf(const std::string &dir)
{
    size_t end = dir.size();
    while (end > 0 && (dir[end - 1] == '/' || dir[end - 1] == '\\')) --end;
    size_t start = end;
    while (start > 0 && dir[start - 1] != '/' && dir[start - 1] != '\\') --start;
    return dir.substr(start, end - start);
}

} // namespace

void parse_texture_pack(const std::string &text, TexturePack &out)
{
    size_t pos = 0;
    while (pos <= text.size()) {
        const size_t nl   = text.find('\n', pos);
        const std::string raw = text.substr(pos, nl == std::string::npos ? nl : nl - pos);
        pos = (nl == std::string::npos) ? text.size() + 1 : nl + 1;

        const std::string line = trim(raw);
        if (line.empty() || line[0] == '#' || line[0] == ';') continue;

        const size_t eq = line.find('=');
        if (eq == std::string::npos) continue;   // not a mapping; it said nothing

        const std::string key   = trim(line.substr(0, eq));
        const std::string value = trim(line.substr(eq + 1));
        if (key.empty()) continue;

        ++out.lines;
        out.manifest = true;

        // A node name in this engine is owner:block. A key without the colon is
        // a property of the pack and never a block, which is what keeps a pack
        // called "name" from being a block called "name".
        if (key.find(':') == std::string::npos) {
            if (key == "name") out.name = value;
            continue;
        }

        // A third part names a face: `core:grass:top`. Only when what is left in
        // front of it is still a block name -- otherwise a mod that registered
        // `mod:top` would find its block being read as the top of a block called
        // `mod`, and it would wear the wrong picture with nothing said.
        std::string node = key;
        std::string word;
        if (const size_t last = key.rfind(':'); last != std::string::npos && last > 0) {
            const std::string tail = key.substr(last + 1);
            const std::string head = key.substr(0, last);
            if (faces_named(tail) != 0 && head.find(':') != std::string::npos) {
                node = head;
                word = tail;
            }
        }

        if (!name_is_inside_the_pack(value)) {
            ++out.refused;
            continue;
        }

        // Keyed by the file the ATLAS will ask for, so the lookup there is one
        // hash of a string it already has. A face word becomes part of that name
        // -- `core:grass:top` is `core_grass_top.png` -- so nothing downstream
        // has to know that packs can talk about faces.
        out.alias[face_texture_name(node, word)] = value;
    }
}

bool load_texture_pack(const std::string &dir, TexturePack &out, std::string &why)
{
    out = TexturePack{};
    why.clear();

    if (dir.empty()) {
        why = "no folder named";
        return false;
    }

    struct stat st{};
    if (::stat(dir.c_str(), &st) != 0 || !S_ISDIR(st.st_mode)) {
        why = dir + ": no such folder";
        return false;
    }

    out.dir  = dir;
    out.name = leaf(dir);

    // A folder with no manifest is a pack all the same -- the one this engine
    // ships has none -- and its files answer for the blocks they are named
    // after. See texture_pack.h.
    const std::string path = dir + "/" + kPackManifest;
    std::FILE        *f    = std::fopen(path.c_str(), "rb");
    if (f == nullptr) return true;

    std::string text;
    char        buf[4096];
    size_t      got = 0;
    while ((got = std::fread(buf, 1, sizeof(buf), f)) > 0) {
        text.append(buf, got);
        if (text.size() > 1024 * 1024) break;   // a manifest, not a payload
    }
    std::fclose(f);

    out.manifest = true;
    parse_texture_pack(text, out);
    return true;
}

const std::string &pack_file(const TexturePack &pack, const std::string &file)
{
    const auto it = pack.alias.find(file);
    return it == pack.alias.end() ? file : it->second;
}


// ---------------------------------------------------------------- travelling

std::string pack_media_name(const std::string &file)
{
    // The SAME rule that decides whether a manifest target is inside the pack,
    // and not a second copy of it: what may travel is exactly what may be
    // named, or a pack would serve a file the folder it came from refused.
    if (!name_is_inside_the_pack(file)) return std::string();
    return std::string(kPackMediaPrefix) + file;
}

std::string pack_media_file(const std::string &media_name)
{
    const std::string prefix = kPackMediaPrefix;
    if (media_name.size() <= prefix.size()) return std::string();
    if (media_name.compare(0, prefix.size(), prefix) != 0) return std::string();

    // Checked on the way BACK too. The name came off somebody else's server,
    // and a client that trusted the prefix and wrote whatever followed it would
    // write "pack.file.../../../.bashrc" wherever the cache is.
    const std::string file = media_name.substr(prefix.size());
    if (!name_is_inside_the_pack(file)) return std::string();
    return file;
}

std::string rebuild_pack_folder(
    const std::vector<std::string> &names,
    const std::function<bool(const std::string &, std::vector<uint8_t> &)> &media,
    const std::string &cache_dir, std::string &why)
{
    why.clear();

    std::vector<uint8_t> manifest;
    if (!media || !media(kPackMediaManifest, manifest) || manifest.empty()) {
        why = "this world serves no pack";
        return std::string();
    }

    // Named by the MANIFEST's content. The same pack from two worlds is one
    // folder, so a player who has it downloads nothing; a pack whose manifest
    // changed is a different folder, rather than a folder half of which is old.
    const std::string folder =
        cache_dir + "/" + hash_content(manifest).hex();

    std::error_code ec;
    std::filesystem::create_directories(folder, ec);
    if (ec) {
        why = "could not make " + folder + ": " + ec.message();
        return std::string();
    }

    const auto put = [&](const std::string &path, const std::vector<uint8_t> &bytes) {
        std::FILE *f = std::fopen(path.c_str(), "wb");
        if (f == nullptr) return false;
        const bool ok = bytes.empty() || std::fwrite(bytes.data(), 1, bytes.size(), f) ==
                                             bytes.size();
        std::fclose(f);
        return ok;
    };

    if (!put(folder + "/" + kPackManifest, manifest)) {
        why = "could not write the manifest into " + folder;
        return std::string();
    }

    int wrote = 0, missing = 0;
    for (const std::string &name : names) {
        const std::string file = pack_media_file(name);
        if (file.empty()) continue;

        std::vector<uint8_t> bytes;
        if (!media(name, bytes) || !put(folder + "/" + file, bytes)) {
            ++missing;
            continue;
        }
        ++wrote;
    }

    why = std::to_string(wrote) + " file(s)";
    if (missing > 0) why += ", " + std::to_string(missing) + " that would not come";
    return folder;
}

} // namespace cnp
