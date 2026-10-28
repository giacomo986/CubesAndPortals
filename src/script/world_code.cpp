#include "script/world_code.h"

#include "modpack/mod_package.h"
#include "script/mod_host.h"

#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>

namespace cnp {
namespace {

constexpr const char *kPackageSuffix = ".cnpmod";

// How a body moves is the server's on a connected world: its welcome says it,
// and it checks every claim against it. A world's code asking otherwise gets 0,
// and the log says why rather than leaving the mod to guess (TODO.md OPEN-44).
int refuse_movement(const char *call)
{
    const ModManifest *m = mod_host_current();
    std::printf("[mod] %s: %s: refused -- on a connected world the server says how a body "
                "moves, in its welcome\n", m ? m->name.c_str() : "?", call);
    return 0;
}
int no_movement(void *, double, double, double, double, double) { return refuse_movement("movement"); }
int no_body_movement(void *, int, double, double, double, double, double, double)
{
    return refuse_movement("body_movement");
}
int no_drive(void *, double, double, double, double) { return refuse_movement("drive"); }
int no_body_drive(void *, int, double, double, double, double, double, double)
{
    return refuse_movement("body_drive");
}

} // namespace

std::string world_code_media_name(const std::string &mod_name)
{
    return std::string(kWorldCodePrefix) + mod_name + kPackageSuffix;
}

bool world_code_name_of(const std::string &media_name, std::string &mod_name)
{
    const std::string prefix = kWorldCodePrefix;
    const std::string suffix = kPackageSuffix;
    if (media_name.size() <= prefix.size() + suffix.size()) return false;
    if (media_name.compare(0, prefix.size(), prefix) != 0) return false;
    if (media_name.compare(media_name.size() - suffix.size(), suffix.size(), suffix) != 0)
        return false;

    mod_name = media_name.substr(prefix.size(), media_name.size() - prefix.size() - suffix.size());
    // One path segment: a name with a slash in it is a name trying to be a path.
    return !mod_name.empty() && mod_name.find('/') == std::string::npos &&
           mod_name.find('\\') == std::string::npos && mod_name != "." && mod_name != "..";
}

bool world_code_pack(const std::string &mod_dir, std::vector<uint8_t> &out, std::string &error)
{
    ModPackage pkg;
    if (!mod_package_read_dir(mod_dir, pkg, error)) return false;

    // Code only. A mod's models, their buffers and pictures, and its sounds
    // reach a player's machine as media of their own (add_models_to_media,
    // add_mod_sounds_to_media), which is where the client looks for them; in
    // the package they were the same bytes twice, and a mod with a deck of
    // cards (mods/cards, 5.7 MB) went past what one media file may be and
    // was not offered at all.
    static const char *const kServedApart[] = {".gltf", ".glb", ".bin", ".png", ".jpg", ".ogg", ".wav"};
    auto served_apart = [](const std::string &path) {
        for (const char *ext : kServedApart) {
            const size_t n = std::strlen(ext);
            if (path.size() > n && path.compare(path.size() - n, n, ext) == 0) return true;
        }
        return false;
    };
    std::erase_if(pkg.files, [&](const ModPackageFile &f) { return served_apart(f.path); });
    return mod_package_write(pkg, out, error);
}

script_host world_code_host(script_host full)
{
    script_host h = full;

    // Writing the world. The server owns it; a client that wrote here would be
    // drawing a world the server has never heard of.
    h.set_block  = nullptr;
    h.set_facing = nullptr;
    h.set_portal = nullptr;

    // Registering content: blocks, shapes, models, items, recipes, stages,
    // modifiers. A world's table is the server's, sent in the welcome.
    h.register_node    = nullptr;
    h.node_model       = nullptr;
    h.node_drop        = nullptr;
    h.node_drop_item   = nullptr;
    h.node_alias       = nullptr;
    h.item_alias       = nullptr;
    h.player_regen     = nullptr;
    h.player_max_health = nullptr;
    h.time_of_day      = nullptr;
    h.set_time         = nullptr;
    h.player_spawn     = nullptr;
    h.node_climbable   = nullptr;
    h.node_door        = nullptr;
    h.node_conveyor    = nullptr;
    h.node_box         = nullptr;
    h.node_piece       = nullptr;
    h.place_piece      = nullptr;
    h.piece_entry      = nullptr;
    h.take_piece       = nullptr;
    h.register_mapgen  = nullptr;
    h.register_abm     = nullptr;
    h.register_lbm     = nullptr;
    h.register_item    = nullptr;
    h.register_model   = nullptr;
    h.player_model     = nullptr;
    h.mob_model        = nullptr;
    h.world_sky        = nullptr;
    h.world_storm      = nullptr;
    h.mapgen_core      = nullptr;
    h.mapgen_only      = nullptr;
    h.portal_place     = nullptr;
    h.register_mob     = nullptr;
    h.spawn_mob        = nullptr;
    h.mob_tame         = nullptr;
    h.mob_shear        = nullptr;
    h.mob_lays         = nullptr;
    h.mob_drop         = nullptr;
    h.mob_breed        = nullptr;
    h.mob_sight        = nullptr;
    h.find_path        = nullptr;
    h.slow_mobs        = nullptr;   // a creature's pace is the server's
    h.mob_blow_every   = nullptr;
    // Kept: model_id reads the welcome's table, and body_model here dresses
    // only how this machine draws its own body -- a kart while it drives
    // (games/track). Everybody else sees what the server put on it.
    h.recipe_shaped    = nullptr;
    h.recipe_shapeless = nullptr;

    // Other people's things and the player's own: bags, chests, panels, health,
    // how a body moves. All of it is decided by the server and told to the client.
    h.take_item       = nullptr;
    h.wear_held       = nullptr;
    h.bag_count       = nullptr;
    h.give_item       = nullptr;
    // held_item is kept: on a client it answers only for this machine's own
    // player, what the hand already shows here -- a gun's HUD needs it.
    h.meta_set        = nullptr;
    h.storage_set     = nullptr;   // a world's code keeps nothing on this machine,
    h.storage_get     = nullptr;   // and must not read what a local mod of its name kept
    h.shared_storage_set = nullptr;
    h.shared_storage_get = nullptr;
    h.ui_define       = nullptr;
    h.ui_label        = nullptr;
    h.ui_button       = nullptr;
    h.ui_box          = nullptr;
    h.ui_show         = nullptr;
    h.container_make  = nullptr;
    h.container_add   = nullptr;
    h.container_show  = nullptr;
    h.container_close = nullptr;
    h.container_spill = nullptr;
    h.container_clear = nullptr;
    h.container_take  = nullptr;
    h.node_liquid     = nullptr;
    h.craft_show      = nullptr;
    h.craft_close     = nullptr;
    h.drop_item       = nullptr;
    h.damage          = nullptr;
    h.heal            = nullptr;
    h.movement        = no_movement;
    h.body_movement   = no_body_movement;
    h.player_fly      = nullptr;
    h.worn_set_id     = nullptr;
    h.recipe_dye      = nullptr;
    h.item_dye        = nullptr;
    h.drive           = no_drive;
    h.body_drive      = no_body_drive;
    h.body_boat       = nullptr;
    h.body_width      = nullptr;
    h.eye_height      = nullptr;
    h.parkour         = nullptr;
    h.ledges          = nullptr;
    h.player_place    = nullptr;
    h.player_lie      = nullptr;
    h.play_sound      = nullptr;

    return h;
}

WorldCodeReport world_code_load(ModHost &host, const std::vector<std::string> &offered,
                                const WorldCodeFetch &fetch, const std::string &cache_dir,
                                bool refused)
{
    namespace fs = std::filesystem;

    WorldCodeReport report;
    std::vector<std::string> names;
    std::vector<std::string> mods;
    for (const std::string &media_name : offered) {
        std::string mod;
        if (world_code_name_of(media_name, mod)) {
            names.push_back(media_name);
            mods.push_back(mod);
        }
    }
    report.offered = (int)names.size();
    host.set_world_code_mods(mods);

    // Refused before a byte is fetched into anything that runs: a player who
    // said no has not had this world's code unpacked on their machine either.
    if (refused) {
        report.refused_by_player = report.offered;
        return report;
    }

    // Unpacked first and loaded together, so `depends` orders them as it does
    // a mods directory -- not the order the media happened to be offered in.
    std::vector<std::string> dirs;
    for (const std::string &media_name : names) {
        std::vector<uint8_t> bytes;
        if (!fetch(media_name, bytes)) {
            report.told += "world code '" + media_name + "': the world offered it and did not send it\n";
            continue;
        }

        ModPackage  pkg;
        std::string error;
        if (!mod_package_parse(bytes.data(), bytes.size(), pkg, error)) {
            report.told += "world code '" + media_name + "': " + error + "\n";
            continue;
        }

        // Named by what is inside, the way ModHost::unpack_package names a
        // package it found on disk: two versions of one mod are two directories.
        std::string mod;
        world_code_name_of(media_name, mod);
        const fs::path  dir = fs::path(cache_dir) / (mod + "-" + pkg.hash.substr(0, 16));
        std::error_code ec;
        fs::remove_all(dir, ec);
        fs::create_directories(dir.parent_path(), ec);
        if (!mod_package_unpack(pkg, dir.string(), error)) {
            report.told += "world code '" + media_name + "': " + error + "\n";
            continue;
        }

        dirs.push_back(dir.string());
    }

    const size_t before = host.mods().size();
    host.load_dirs(dirs);
    for (size_t i = before; i < host.mods().size(); ++i) {
        const Mod &m = host.mods()[i];
        if (m.running)
            ++report.running;
        else
            report.told += "world code '" + m.manifest.name + "': " + m.error + "\n";
    }
    return report;
}

} // namespace cnp
