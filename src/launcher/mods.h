#pragma once

// The mods the launcher shows and manages (TODO.md, "Launcher", step 8).
//
// Three kinds: the game's own (in the build, read only), the player's
// everywhere (core/paths.h user_mods_dir) and a world's own (<world>/mods). A
// mod is turned off by name in a mods.conf -- user_mods_conf for everywhere, the
// world's for that world -- which the client hands its server as --no-mod.

#include "script/mod_host.h"

#include <string>
#include <vector>

namespace cnp::launcher {

enum class Where { Game, Everywhere, World };

struct ModEntry {
    Where       where = Where::Game;
    std::string path;            // the directory or the .cnpmod
    ModManifest manifest;        // read from it; name empty when it could not be
    std::string error;           // why the manifest could not be read
    bool        off      = false;   // in this view's off list
    bool        off_all  = false;   // a world's view: off everywhere already
};

// The game's mods for a build whose cnp_client is at `client`.
std::string game_mods_dir(const std::string &client, bool bundled);

// The mod API range a build serves: this launcher's own for the packaged one,
// else what its release.manifest says (kept beside it at install).
void api_range(const std::string &client, bool bundled, unsigned &oldest, unsigned &newest);

// What a view shows: `world` empty for everywhere, else that world's name.
std::vector<ModEntry> list_mods(const std::string &game_dir, const std::string &world);

// A directory or .cnpmod's manifest, without installing it. False and `err`
// when it is not a mod.
bool read_mod(const std::string &path, ModEntry &out, std::string &err);

// What stands in the way of a mod running with this build, among `all`: an API
// it was not written for, a dependency absent, off or too old. Empty: fine.
std::vector<std::string> problems(const ModEntry &e, const std::vector<ModEntry> &all,
                                  unsigned api_oldest, unsigned api_newest);

// One line a capability: what it lets the mod do.
std::string capability_meaning(const std::string &cap);

// Turns `name` off or on in a view's list.
void set_off(const std::string &world, const std::string &name, bool off);

// Copies a mod (directory or .cnpmod) into the view's folder, refused when a mod
// of that name is in `all` already. Empty on success.
std::string install_mod(const ModEntry &mod, const std::string &world, const std::vector<ModEntry> &all);

// Deletes a player's mod (never a game one). Empty on success.
std::string remove_mod(const ModEntry &e);

} // namespace cnp::launcher
