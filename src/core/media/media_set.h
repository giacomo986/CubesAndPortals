#pragma once

#include "core/media/content_hash.h"
#include "core/media/media_limits.h"
#include "core/item/item_registry.h"
#include "core/registry/model_registry.h"
#include "core/registry/node_registry.h"

#include <string>
#include <unordered_map>
#include <vector>

// What a world's media are, on the side that owns them.
//
// A name-to-hash table and a hash-to-bytes store, which is the whole of
// DESIGN.md 4.5's server half. The two are separate on purpose: the name is
// what a block asks for and the hash is what the file is, and keeping them
// apart is what makes two names that resolve to identical bytes cost one blob
// -- which is the same mechanism that makes two SERVERS sending identical
// bytes cost one download on the client.
//
// Built once at startup and read from every session. It never changes while
// sessions are running, which is why sessions may hold a pointer to it; a mod
// reload that changed the blocks would have to build a new one and swap it
// between ticks, and that is not something this does yet.

namespace cnp {

// A file a world can serve.
struct MediaBlob {
    ContentHash          hash;
    std::vector<uint8_t> bytes;
};

class MediaSet {
public:
    // Returns false for bytes that are empty or over the cap. A name already
    // present is replaced, which is what makes "the file on disk wins over the
    // generated placeholder" a matter of insertion order rather than a flag.
    bool add(const std::string &name, std::vector<uint8_t> bytes);

    const ContentHash *hash_for(const std::string &name) const;
    const MediaBlob   *find(const ContentHash &hash) const;

    size_t names() const { return m_by_name.size(); }
    size_t blobs() const { return m_blobs.size(); }
    size_t total_bytes() const;

    // Every name with the hash it resolves to, in a stable order (sorted by
    // name) so two runs of the same server produce the same offer and a diff
    // between two servers' offers means something.
    std::vector<std::pair<std::string, ContentHash>> table() const;

private:
    std::unordered_map<std::string, ContentHash>                     m_by_name;
    std::unordered_map<ContentHash, MediaBlob, ContentHashHasher>    m_blobs;
};

// How a set got built, so a server can say it rather than a caller assume it.
struct MediaBuildStats {
    int    from_file  = 0;   // a texture somebody drew
    int    generated  = 0;   // a placeholder baked from the node's colour
    int    skipped    = 0;   // air, and anything with no name
    size_t bytes      = 0;

    // And the pack as a PACK, beside the same files under the engine's names.
    // See texture_pack.h: it costs no blob, because a second name for bytes
    // already in the set is one row in a table.
    bool pack_served  = false;   // this world offers its manifest
    int  pack_files   = 0;       // ...and this many of the files it names
    int  pack_missing = 0;       // named by the manifest and not readable here
};

// The media a world serves for its own block table.
//
// One entry per registered node: the file in `dir` if there is one, otherwise
// the placeholder bake_tile() produces. The fallback lives HERE and not on the
// client, and that is the change this pipeline makes: a client cannot bake a
// placeholder for a block it has never heard of, because the placeholder is
// derived from the node's colour and the wire carries names, not colours. Every
// cross-game block used to be magenta for exactly that reason.
//
// `dir` may be empty, and then everything is generated. `fallback` is where a
// file `dir` has not got is looked for next, by its engine name: a game that
// borrows the sandbox's mods (the Inferno) borrows their pictures with them.
MediaBuildStats build_media_set(const NodeRegistry &reg, const std::string &dir, MediaSet &out,
                                const std::string &fallback = "");

// How many of a world's ITEM pictures it managed to serve.
struct ItemMediaStats {
    int    files   = 0;   // an item somebody drew a square for
    int    skipped = 0;   // ...and one nobody did, which is most of them
    size_t bytes   = 0;
};

// The item squares a world serves, added to the same set its blocks are in.
//
// One file per item, by the item's own name -- tools_pick.png for tools:pick --
// and nothing at all for an item nobody drew. NO placeholder is baked here, and
// that is the difference from the blocks above: a block's placeholder comes
// from its colour, and an item has none. An item with no picture falls back on
// the client to the block it places, which is a picture the client already has,
// and to a plain square for a tool that places nothing.
//
// Separate from build_media_set rather than a parameter of it because the two
// tables are separate: a server may have blocks and no items at all, and every
// existing caller of that function has exactly that.
ItemMediaStats add_items_to_media(const ItemRegistry &items, const std::string &dir,
                                  MediaSet &out, const std::string &fallback = "");

// How much of a world's models it managed to serve.
struct ModelMediaStats {
    int    models  = 0;   // served whole
    int    files   = 0;   // the .gltf plus everything each one names
    int    skipped = 0;   // named by the world and not readable here
    size_t bytes   = 0;
};

// The models a world serves, added to the same set its textures are in.
//
// A model is not a file: the .gltf names its buffer and its images beside
// itself, so what goes in is the .gltf under `model/<name>/model.gltf` and each
// of those files under `model/<name>/<the name the .gltf calls it by>` -- the
// only names the loader on the other side will look for.
//
// A model this machine cannot read is SKIPPED, counted, and said in `why`. It
// is not an error: a world may name a model whose file somebody moved, and the
// bodies wearing it are drawn as the box every body was drawn as before. Half a
// model would be worse than none -- a character with a hole in it.
ModelMediaStats add_models_to_media(const ModelRegistry &models, MediaSet &out, std::string &why);

// What a mod's sound file is called in a world's media: "sound.<mod>.<file>",
// so a client can find "mod:thing" there before its own folders.
std::string sound_media_name(const std::string &mod, const std::string &file);

// Every .ogg and .wav in a mod's folder, links followed, into `out` under that
// name. Returns how many went in.
int add_mod_sounds_to_media(const std::string &mod, const std::string &dir, MediaSet &out);

} // namespace cnp
