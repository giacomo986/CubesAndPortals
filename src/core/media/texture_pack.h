#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

// A texture pack: a folder of pictures, and a note saying which picture a block
// wears.
//
// Without the note a pack has to name its files the way this engine names them
// -- core_stone.png, beacons_pillar.png -- which means a pack made for anything
// else has to be renamed file by file before it can be tried. The note is what
// makes somebody else's art usable as it came: `core:stone = greystone.png`.
//
// Two rules that are not conveniences:
//
//   A target is a PLAIN FILE NAME. No slash, no `..`, no leading dot. A pack is
//   content, it can arrive over the wire, and a manifest that could name
//   ../../../etc/passwd is a manifest that reads whatever the player can read.
//   Refused lines are counted, not ignored quietly.
//
//   A pack that names nothing is still a pack. The default folder has no
//   manifest and never needed one, so an empty alias table has to behave as the
//   old naming rule and not as an error.

namespace cnp {

// What one manifest said.
struct TexturePack {
    std::string name;   // what the pack calls itself; the folder name if it did not say
    std::string dir;    // where the files are

    // Keyed by the file name the ENGINE asks for (texture_file_name of the node
    // name), so a lookup at atlas time is one hash and no string building.
    std::unordered_map<std::string, std::string> alias;

    int lines    = 0;   // manifest lines that said something
    int refused  = 0;   // ...of which named a file outside the pack
    bool manifest = false;   // whether there was a manifest at all

    bool empty() const { return dir.empty(); }
};

// The manifest a pack folder is read from.
inline constexpr const char *kPackManifest = "pack.txt";

// Reads `dir`/pack.txt if it is there. A folder with no manifest loads fine and
// yields a pack with no aliases -- see the second rule above. Returns false only
// when `dir` itself is unusable, and puts the reason in `why`.
bool load_texture_pack(const std::string &dir, TexturePack &out, std::string &why);

// Parses manifest TEXT rather than a file, so the rules above can be gated
// without a filesystem. Same counting.
void parse_texture_pack(const std::string &text, TexturePack &out);

// Which file in the pack answers for the engine's `file`. The alias if there is
// one, otherwise `file` itself.
const std::string &pack_file(const TexturePack &pack, const std::string &file);

// ---------------------------------------------------------------- travelling
//
// A server wearing a pack serves its FILES under the engine's names -- see
// build_media_set -- so a client sees core_stone.png and never learns a pack
// existed. That is what kept the protocol from having to know about packs, and
// it means a player cannot keep a world's pack, put it on another world, or
// cycle to it with F7 on a world that is not local. TODO.md OPEN-25.
//
// So the pack travels a second time, as itself: the manifest under one reserved
// name and each file the manifest points at under its own. It costs no blob --
// the media set is content-addressed, so a second NAME for bytes already in it
// is one row in a table -- and no protocol change at all: a world with no pack
// offers no such name, and that is the whole of the negotiation.
//
// FLAT names, because MediaOffer::decode refuses a media name with a separator
// in it and ONE bad name costs the whole offer, every texture with it.

inline constexpr const char *kPackMediaManifest = "pack.manifest";
inline constexpr const char *kPackMediaPrefix   = "pack.file.";

// The media name a pack's own file travels under, or EMPTY for a file name that
// cannot be one: a separator, a leading dot, `..`, or nothing at all. The
// manifest already refuses those as targets, so an empty answer here means the
// two rules disagree and the file is left behind rather than guessed at.
std::string pack_media_name(const std::string &file);

// The way back: what to call the file inside a rebuilt folder, or EMPTY for a
// media name that is not one of a pack's.
std::string pack_media_file(const std::string &media_name);

// Writes the folder a world's pack travels as, out of what that world offered.
//
// `names` is every name the world offered and `media` fetches one. Returns the
// folder it wrote -- named by the manifest's content, so the same pack from two
// worlds is one folder and a changed pack is a new one -- or EMPTY when the
// world served no pack. `why` says which, either way.
//
// The folder is a PACK, not a copy of the atlas: it holds pack.txt and the
// files pack.txt names, so load_texture_pack reads it exactly as it reads the
// folder on the server.
std::string rebuild_pack_folder(
    const std::vector<std::string> &names,
    const std::function<bool(const std::string &, std::vector<uint8_t> &)> &media,
    const std::string &cache_dir, std::string &why);

} // namespace cnp
