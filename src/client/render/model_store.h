#pragma once

// The models this client has, in the ids the world it is looking at uses.
//
// A world names its models in the welcome -- "core:person", "mymod:crab" -- and
// a body on the wire carries a POSITION in that list plus one. This turns that
// number back into something to draw, and it is the only place that knows a
// model is a file at all.
//
// A NAME IS NOT A PATH, and this is where that is enforced. The names come off
// the wire from somebody else's server, so "mymod:crab" becomes
// "mymod_crab.gltf" inside one directory and a name that could climb out of it
// -- a slash, a backslash, a leading dot -- is refused and counted. The same
// rule the texture pack manifest lives by, for the same reason: content arrives
// over the wire, and a model table that could name ../../.ssh/id_ed25519 reads
// whatever the player can read.
//
// A MISSING MODEL IS A BOX AND NEVER A HOLE. A client that has not got the file
// keeps no entry for that id, and the body is drawn the way every body was
// drawn before there were models. An invisible player is worse than a
// rectangular one.

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "client/render/model_render.h"
#include "core/media/gltf.h"
#include "core/registry/model_registry.h"

namespace cnp {

class ModelStore {
public:
    struct Entry {
        std::string   name;
        std::string   file;
        SkinnedModel  model;
        ModelRenderer renderer;
        bool          ready = false;
    };

    // Takes the world's model table and the directory to look in. Reads what is
    // there, skips what is not, and answers how many it now has. `why` collects
    // one line per model it could not use, because a character that does not
    // appear has to say so somewhere.
    //
    // Every id is re-read: a client that walks into a second world is a client
    // whose table changed underneath it, and keeping the old entries would draw
    // one world's characters in another.
    int load(const std::vector<std::string> &names, const std::string &dir, std::string &why);

    // The same, for the ids this store has not got YET. A world serves what it
    // serves; whatever it did not serve may still be sitting in the player's
    // own folder, and this is the second look -- so a client is never left
    // without a character it has on disk because the server had none.
    //
    // Both take a range of the table, `first` and up to `count` ids after it,
    // so a caller can spread a world's models over several frames.
    int fill_from_dir(const std::vector<std::string> &names, const std::string &dir,
                      std::string &why, size_t first = 0, size_t count = SIZE_MAX);

    // One model, put in by hand at the id it should answer to. What the drawing
    // gate uses, and what a single-process world would use if it had models.
    bool put(uint16_t id, const std::string &name, const std::string &path, std::string &why);

    // The same table, out of a world's MEDIA rather than out of a folder on
    // this machine -- which is what lets a player join a server whose models
    // they have never had.
    //
    // `media` answers a media name with bytes, the way every other file of a
    // world arrives. A model is rebuilt as a DIRECTORY under `cache_dir`, named
    // by the hash of its .gltf: the .gltf names its buffer and its images
    // beside itself, so they have to sit beside it under the names it calls
    // them by, and content-addressing the folder means two worlds serving the
    // same character cost one copy of it.
    //
    // A model this world does not serve is left alone -- not an error, and the
    // caller may then look in its own folder for one of that name. Answers how
    // many it now has out of the media.
    int load_from_media(const std::vector<std::string>         &names,
                        const std::function<bool(const std::string &, std::vector<uint8_t> &)>
                                          &media,
                        const std::string &cache_dir, std::string &why, size_t first = 0,
                        size_t count = SIZE_MAX);

    void clear();

    bool has(uint16_t id) const { return entry(id) != nullptr; }
    const Entry *entry(uint16_t id) const;
    Entry       *entry(uint16_t id);

    size_t size() const { return m_by_id.size(); }
    int    refused() const { return m_refused; }

private:
    // Index is the id minus one, and a gap is a model this client has not got.
    std::vector<std::unique_ptr<Entry>> m_by_id;
    int                                 m_refused = 0;
};

} // namespace cnp
