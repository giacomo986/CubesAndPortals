#include "launcher/mods.h"

#include "core/paths.h"
#include "core/release.h"
#include "modpack/mod_package.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iterator>

namespace cnp::launcher {

namespace fs = std::filesystem;

namespace {

std::string world_dir(const std::string &world) { return user_data_dir() + "/worlds/" + world; }

std::string off_file(const std::string &world)
{
    return world.empty() ? user_mods_conf() : world_dir(world) + "/mods.conf";
}

bool is_package(const fs::path &p) { return p.extension() == kModPackageExtension; }

void add_dir(std::vector<ModEntry> &out, const std::string &dir, Where where)
{
    std::error_code         ec;
    std::vector<fs::path> paths;
    for (const fs::directory_entry &e : fs::directory_iterator(dir, ec))
        if (e.is_directory(ec) || is_package(e.path())) paths.push_back(e.path());
    std::sort(paths.begin(), paths.end());
    for (const fs::path &p : paths) {
        ModEntry    m;
        std::string err;
        if (!read_mod(p.string(), m, err)) m.error = err;
        m.where = where;
        out.push_back(std::move(m));
    }
}

} // namespace

std::string game_mods_dir(const std::string &client, bool bundled)
{
    if (bundled) return data_dir("mods", CNP_SOURCE_ROOT "/mods");
    return (fs::path(client).parent_path() / "mods").string();
}

void api_range(const std::string &client, bool bundled, unsigned &oldest, unsigned &newest)
{
    oldest = kModApiOldest;
    newest = kModApi;
    if (bundled) return;
    std::ifstream     in(fs::path(client).parent_path() / release::kManifestName, std::ios::binary);
    const std::string text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    uint8_t           key[net::kSignPublicBytes];
    release::Manifest m;
    std::string       err;
    if (release::release_key(key) && release::read(text, key, m, err)) {
        oldest = m.mod_api_oldest;
        newest = m.mod_api;
    }
}

bool read_mod(const std::string &path, ModEntry &out, std::string &err)
{
    out      = ModEntry{};
    out.path = path;
    std::error_code ec;
    std::string     text;
    if (fs::is_directory(path, ec)) {
        std::ifstream in(fs::path(path) / "mod.conf", std::ios::binary);
        if (!in) {
            err = "no mod.conf in " + fs::path(path).filename().string();
            return false;
        }
        text.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
    } else if (is_package(path)) {
        // Read whole and checked by the package's own reader; its mod.conf is
        // one of its files, no unpacking needed to read it.
        ModPackage pkg;
        if (!mod_package_read_file(path, pkg, err)) return false;
        const auto conf = std::find_if(pkg.files.begin(), pkg.files.end(),
                                       [](const ModPackageFile &f) { return f.path == "mod.conf"; });
        if (conf == pkg.files.end()) {
            err = "no mod.conf in " + fs::path(path).filename().string();
            return false;
        }
        text.assign(conf->bytes.begin(), conf->bytes.end());
    } else {
        err = "not a mod: a folder with mod.conf, or a .cnpmod";
        return false;
    }
    out.manifest.dir = path;
    if (!mod_manifest_parse(text, out.manifest, err)) {
        err = "mod.conf: " + err;
        return false;
    }
    return true;
}

std::vector<ModEntry> list_mods(const std::string &game_dir, const std::string &world)
{
    std::vector<ModEntry> out;
    // The player's first, the game's (sixty and more) after them.
    if (!world.empty()) add_dir(out, world_dir(world) + "/mods", Where::World);
    add_dir(out, user_mods_dir(), Where::Everywhere);
    add_dir(out, game_dir, Where::Game);

    const std::vector<std::string> everywhere = read_mods_off(user_mods_conf());
    const std::vector<std::string> here       = read_mods_off(off_file(world));
    for (ModEntry &m : out) {
        const std::string &n = m.manifest.name;
        m.off_all = !world.empty() && std::find(everywhere.begin(), everywhere.end(), n) != everywhere.end();
        m.off     = std::find(here.begin(), here.end(), n) != here.end() || m.off_all;
    }
    return out;
}

std::vector<std::string> problems(const ModEntry &e, const std::vector<ModEntry> &all,
                                  unsigned api_oldest, unsigned api_newest)
{
    std::vector<std::string> out;
    if (!e.error.empty()) return {e.error};
    if (e.manifest.api < api_oldest || e.manifest.api > api_newest)
        out.push_back("written for mod API " + std::to_string(e.manifest.api) + ", this version serves " +
                      std::to_string(api_oldest) + " to " + std::to_string(api_newest));
    for (const ModDependency &d : e.manifest.depends) {
        const auto it = std::find_if(all.begin(), all.end(), [&](const ModEntry &m) {
            return m.error.empty() && m.manifest.name == d.name;
        });
        if (it == all.end()) out.push_back("needs " + d.str() + ", which is not here");
        else if (it->off) out.push_back("needs " + d.name + ", which is off");
        else if (!d.satisfied_by(it->manifest.version))
            out.push_back("needs " + d.str() + ", and " + it->manifest.version.str() + " is here");
    }
    for (size_t i = 0; i < all.size(); ++i)
        if (&all[i] != &e && all[i].error.empty() && all[i].manifest.name == e.manifest.name &&
            all[i].where < e.where) {
            out.push_back("a mod called " + e.manifest.name + " is here already: this one does not load");
            break;
        }
    return out;
}

std::string capability_meaning(const std::string &cap)
{
    if (cap == "os") return "os: reads the clock and the date";
    if (cap == "debug") return "debug: Luau's debug table";
    return cap + ": refused by this game, the mod will not load";
}

void set_off(const std::string &world, const std::string &name, bool off)
{
    const std::string        path  = off_file(world);
    std::vector<std::string> names = read_mods_off(path);
    std::erase(names, name);
    if (off) names.push_back(name);
    write_mods_off(path, names);
}

std::string install_mod(const ModEntry &mod, const std::string &world, const std::vector<ModEntry> &all)
{
    if (!mod.error.empty() || mod.manifest.name.empty()) return "not a mod";
    for (const ModEntry &m : all)
        if (m.error.empty() && m.manifest.name == mod.manifest.name)
            return "a mod called " + mod.manifest.name + " is here already";
    const fs::path  dir = world.empty() ? fs::path(user_mods_dir()) : fs::path(world_dir(world)) / "mods";
    std::error_code ec;
    fs::create_directories(dir, ec);
    if (fs::is_directory(mod.path, ec)) {
        // Named by its manifest, not by the folder it came in: the name a mod
        // loads under is the one place that cannot lie.
        const fs::path to = dir / mod.manifest.name;
        if (fs::exists(to, ec)) return to.string() + " exists";
        fs::copy(mod.path, to, fs::copy_options::recursive, ec);
    } else {
        const fs::path to = dir / fs::path(mod.path).filename();
        if (fs::exists(to, ec)) return to.string() + " exists";
        fs::copy_file(mod.path, to, ec);
    }
    return ec ? "the copy failed: " + ec.message() : "";
}

std::string remove_mod(const ModEntry &e)
{
    if (e.where == Where::Game) return "a mod of the game is not removed";
    std::error_code ec;
    fs::remove_all(e.path, ec);
    return ec ? ec.message() : "";
}

} // namespace cnp::launcher
