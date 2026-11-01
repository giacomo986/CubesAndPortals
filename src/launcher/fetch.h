#pragma once

// Builds of the game from the releases page: which there are, fetching one and
// checking it against its signed manifest (core/release.h) before anything of
// it is unpacked, and taking one away. curl and tar do the moving, through the
// shell, as core/update_check does; Linux, Windows 10+ and macOS carry both.

#include <atomic>
#include <cstdint>
#include <string>
#include <vector>

namespace cnp::launcher {

// Where releases are downloaded from: kReleasesPage + "/download", or what
// --releases-from gave (a file:// directory too, which is how it is tested).
extern std::string g_download_base;
extern std::string g_releases_api;

// The versions on the releases page, newest first ("0.10.0-alpha.2", no "v").
// Empty when it could not be asked.
std::vector<std::string> remote_versions();

// The package this platform plays: "cubes-and-portals-<v>-linux-x86_64.tar.gz",
// or empty where the launcher cannot install one (macOS for now).
std::string package_name(const std::string &version);

// What an install is doing, for the screen: the package's size once the
// manifest has said it (0 before), to read download_path's bytes against.
struct Progress {
    std::atomic<uint64_t> size{0};
};

// Where install puts the package while it is fetched.
std::string download_path(const std::string &version);

// Fetches release `version` into <data>/versions/<version>/: the manifest,
// checked against the release key; the package, checked against the manifest;
// only then unpacked. Empty on success, else why not. `progress` is written
// before the package's download starts.
std::string install(const std::string &version, Progress &progress);

// The worlds of this machine whose `version` file names `version`.
std::vector<std::string> worlds_made_with(const std::string &version);

// A world of this machine: the version that made it (its `version` file), the
// newest build the launcher last opened it with (`launcher_last`), when it
// was played. Newest played first.
struct World {
    std::string name, made_with, last_opened;
    long long   played = 0;   // seconds, for ordering only
};
std::vector<World> list_worlds();

// The two numbers that decide whether a build reads a world: "0.10" of
// "0.10.0-alpha.2" (docs/BUILDING.md, "Versions": MINOR moves when saves break).
std::string save_line(const std::string &version);

// A copy of world `name` into <data>/backups/<name>-<tag>/, lock files left
// out; nothing when that copy is there already. Empty on success.
std::string backup_world(const std::string &name, const std::string &tag);

// Records that `version` opened world `name` (launcher_last), when it is newer
// than what the file said.
void opened_with(const std::string &name, const std::string &version);

// Takes <data>/versions/<version> away. Empty on success.
std::string remove(const std::string &version);

} // namespace cnp::launcher
