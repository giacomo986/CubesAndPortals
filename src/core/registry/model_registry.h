#pragma once

// What a body in this world may look like.
//
// The same shape as the node and item registries and for the same reason: a
// name is what a mod says, an id is what travels, and the table that turns one
// into the other is sent once in the welcome (core/net/protocol.h). Ids start
// at 1 -- 0 is a body nothing was said about, which is the box.
//
// NOT part of content_hash, and that is a decision rather than an omission. A
// block id is in every stored chunk, so two sides that disagree about the block
// table read a world made of the wrong blocks; a model id is in nothing but a
// live message about a body, and a client that has never heard of a model draws
// what it drew before. A difference here is a poorer picture, not a wrong
// world, and refusing the join over it would be a rule nobody asked for.
//
// The FILE is kept beside the name for whoever has the model on disk. It is not
// on the wire: a client resolves a name into a file of its own, in its own
// directory (client/render/model_store.h), because a path from somebody else's
// server is a path into this machine.

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace cnp {

using ModelId = uint16_t;

inline constexpr ModelId kNoModel = 0;

// What a mod may name as the file its model is in: a plain file name, inside the
// mod's own directory and nowhere else.
//
// One function and not one copy per caller. A mod is content -- it can arrive
// from anywhere a world's files arrive from -- and a rule about paths written
// three times is a rule that is right in two places and wrong in the third.
bool model_file_is_plain(const std::string &file);

// The one file name a model's NAME turns into: "mymod:crab" is
// "mymod_crab.gltf" and nothing else. Answers false for a name that could name
// a file somewhere other than inside the one directory it is looked for in --
// the names come off somebody else's server, so the colon is the only
// punctuation that survives and everything else has to be a letter, a digit, an
// underscore or a dash.
//
// Here rather than on the client because both sides need the same answer: the
// server serves a model's files under a name derived from it, and the client
// looks for them under the name it derives itself.
bool model_file_name(const std::string &name, std::string &out);

// What a model's files are called in a world's media: "model." plus the model's
// name flattened, plus "." plus the file. The .gltf itself is served as
// "model.gltf" whatever it was called on the server's disk, and everything it
// names beside itself keeps the name the .gltf calls it by -- which is the only
// way the loader on the other side finds them.
//
// FLAT, with dots and never with slashes, and that is not a style choice: a
// media name with a separator in it is refused by MediaOffer::decode, and one
// bad name costs the WHOLE offer -- every texture of that world with it. The
// first version of this used "model/<name>/<file>" and every client joining a
// world with a model in it silently received no media at all.
std::string model_media_name(const std::string &model_name, const std::string &file);

inline constexpr const char *kModelMediaEntry = "model.gltf";

class ModelRegistry {
public:
    struct ModelDef {
        std::string name;
        std::string file;   // where it is on THIS machine; never sent
    };

    // Declaring the same name again keeps its id and takes the new file, which
    // is what makes a hot reload possible without renumbering the bodies that
    // are already wearing it.
    ModelId add(const std::string &name, const std::string &file);

    ModelId find(const std::string &name) const;

    const ModelDef &get(ModelId id) const;
    size_t          size() const { return m_defs.size(); }

    // The names in id order, which is what the welcome carries: position plus
    // one is the id.
    std::vector<std::string> table() const;

private:
    std::vector<ModelDef>                        m_defs;
    std::unordered_map<std::string, ModelId>     m_by_name;
};

} // namespace cnp
