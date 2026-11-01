#pragma once

#include <string>
#include <vector>

// Where a build looks for the files it ships with.
//
// Until M10 there was one answer: the source tree, baked into the binary at
// compile time as CNP_SOURCE_ROOT. That is right on the machine that built it
// and wrong everywhere else -- a copied binary reads mods out of a directory
// that does not exist on the machine it was copied to, or, worse, exists and
// belongs to somebody else's checkout.
//
// The order is: what the flag said, then what is next to the executable, then
// the source tree it was built from. Each is a place a build can legitimately
// be run from, and they are tried in the order of how much somebody meant it.

namespace cnp {

// The running executable's own path, empty if it cannot be found:
// /proc/self/exe on Linux, the loader's answer on Windows and macOS.
std::string exe_path();

// The directory the running executable is in, empty if it cannot be found. On
// Linux this is /proc/self/exe; a build that cannot read it falls back to the
// source tree, which is the behaviour that existed before this file.
std::string exe_dir();

// `name` is a directory a build ships, like "mods" or "assets/textures".
// `built_at` is the path baked in at compile time -- pass CNP_SOURCE_ROOT
// joined with the name, which is what every caller had hard-coded before.
//
// Returns the first that exists: <exe dir>/<name>, then <exe dir>/../<name>
// (a build tree has its binaries a directory or two down from the files), then
// `built_at`. When none exists it returns `built_at` anyway, so the error a
// caller prints names the place it would have looked rather than nothing.
std::string data_dir(const std::string &name, const std::string &built_at);

// What data_dir chose, for the line a program prints about it. A build that
// reads somebody else's mods must be able to say so without an strace.
std::string data_dir_reason(const std::string &chosen, const std::string &built_at);

// This player's files that are not options: the worlds, the identity they join
// them with, the launcher's versions. $XDG_DATA_HOME or ~/.local/share, or
// %APPDATA% on Windows, then cubes-and-portals; empty with no home at all.
std::string user_data_dir();

// The options file the client keeps (the language among them): $XDG_CONFIG_HOME
// or ~/.config, or %APPDATA%, then cubes-and-portals/options.
std::string options_path();

// The player's own mods. Beside the worlds on Windows; elsewhere a hidden folder
// in the home, ~/.cubes-and-portals/mods, where a player looks for it (the
// flatpak is given that folder and no other).
std::string user_mods_dir();

// The mods turned off everywhere: mods.conf beside user_mods_dir.
std::string user_mods_conf();

// A mods.conf's `off = a, b` (a world's, or user_mods_conf): the names, in
// order. `#` starts a comment; a missing file is an empty list.
std::vector<std::string> read_mods_off(const std::string &path);
bool                     write_mods_off(const std::string &path, const std::vector<std::string> &names);

} // namespace cnp
