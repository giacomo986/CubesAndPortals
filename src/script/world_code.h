#pragma once

#include "script/mod_host.h"
#include "script/script.h"

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

// Code a world sends to a player's machine, which the player may refuse
// (DESIGN.md §5.6, TODO.md OPEN-49).
//
// One place, in src/script, that the server, the client and the headless probe
// all call: what a world's code is called in its media, how it is packed, what
// it may touch, and how a client loads it. A gate that loaded world code its own
// way would be measuring a sibling of the thing that ships.
//
// What travels is a mod package (modpack/mod_package.h) -- verified whole, its
// paths checked, unpacked into a cache -- served through the media path every
// texture takes. What runs it is a ModHost set to ModSide::Client with world
// code on, against the host world_code_host() returns.

namespace cnp {

// The prefix a world's code has in its media. A client tells code from pictures
// by the name the world gave it and never by what the bytes look like.
//
// A dot and not a slash, the way a model's files are named ("model.<name>.<file>"):
// a media name with a slash in it is a path, and the client refuses the WHOLE
// offer it arrives in -- textures and all. The first version used "worldcode/"
// and a client joining that world was offered nothing at all.
inline constexpr const char *kWorldCodePrefix = "worldcode.";

// The media name a world offers one mod's package under.
std::string world_code_media_name(const std::string &mod_name);

// Whether a media name is a world's code, and the mod name inside it.
bool world_code_name_of(const std::string &media_name, std::string &mod_name);

// One mod directory, packed for the offer.
bool world_code_pack(const std::string &mod_dir, std::vector<uint8_t> &out, std::string &error);

// The host a world's code runs against: `full`, with everything that writes the
// world or registers content taken away. What is left can read, draw, listen,
// and talk on its mod's own channel. A call to something taken away does
// nothing, or raises a Lua error naming why -- it never reaches the world.
script_host world_code_host(script_host full);

struct WorldCodeReport {
    int         offered           = 0;   // packages the world named
    int         running           = 0;   // mods now running from them
    int         refused_by_player = 0;   // offered, and not run because this player said no
    std::string told;                    // a line for each thing that did not load, and why
};

using WorldCodeFetch = std::function<bool(const std::string &, std::vector<uint8_t> &)>;

// Loads a world's code into `host`, which the caller has already given
// world_code_host(...), ModSide::Client and world code on. `offered` is every
// media name the world offered; the ones that are not code are skipped. A
// refused world loads nothing and says how much it did not load.
WorldCodeReport world_code_load(ModHost &host, const std::vector<std::string> &offered,
                                const WorldCodeFetch &fetch, const std::string &cache_dir,
                                bool refused);

} // namespace cnp
