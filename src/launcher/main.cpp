// The launcher: which build of the game to play, and later which mods
// (TODO.md, "Launcher"). It looks like the game's title screen and draws with
// the same code (client/ui/title.h), and it never replaces itself: it starts a
// build of the game and waits for it, so the game it started dies with it.
//
// A build is a directory holding cnp_client: the one this launcher came in
// (beside it, or the build tree's src/client), and each one under
// <user data>/versions/<version>/. Which one Play starts is kept in
// <user data>/launcher as `default = <version>`.

#include "client/gfx/gfx.h"
#include "client/gfx/platform.h"
#include "client/ui/hud.h"
#include "client/ui/lang.h"
#include "client/ui/title.h"
#include "launcher/fetch.h"
#include "launcher/mods.h"
#include "core/os.h"
#include "core/paths.h"
#include "core/update_check.h"
#include "core/version.h"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <future>
#include <fstream>
#include <string>
#include <thread>
#include <vector>

using namespace cnp;
namespace fs = std::filesystem;

namespace {

#ifdef _WIN32
constexpr const char *kClientName = "cnp_client.exe";
#else
constexpr const char *kClientName = "cnp_client";
#endif

struct Build {
    std::string version;
    std::string client;    // the program's path
    bool        bundled = false;   // the one this launcher came in
};

std::string launcher_file() { return user_data_dir() + "/launcher"; }

// Every build there is, newest first.
std::vector<Build> find_builds()
{
    std::vector<Build> out;
    std::error_code    ec;
    const std::string  here = exe_dir();
    for (const fs::path &p : {fs::path(here) / kClientName, fs::path(here) / ".." / "client" / kClientName})
        if (!here.empty() && fs::is_regular_file(p, ec)) {
            out.push_back({kEngineVersion, fs::weakly_canonical(p, ec).string(), true});
            break;
        }
    for (const fs::directory_entry &e : fs::directory_iterator(user_data_dir() + "/versions", ec)) {
        const fs::path client = e.path() / kClientName;
        if (!e.is_directory(ec) || !fs::is_regular_file(client, ec)) continue;
        const std::string v = e.path().filename().string();
        if (std::any_of(out.begin(), out.end(), [&](const Build &b) { return b.version == v; })) continue;
        out.push_back({v, client.string(), false});
    }
    std::sort(out.begin(), out.end(),
              [](const Build &a, const Build &b) { return update::compare(a.version, b.version) > 0; });
    return out;
}

std::string read_default()
{
    std::ifstream in(launcher_file());
    for (std::string line; std::getline(in, line);)
        if (line.rfind("default = ", 0) == 0) return line.substr(10);
    return "";
}

void write_default(const std::string &version)
{
    std::error_code ec;
    fs::create_directories(user_data_dir(), ec);
    std::ofstream(launcher_file()) << "default = " << version << "\n";
}

// The language the game was last set to, from its options file.
std::string options_language()
{
    std::ifstream in(options_path());
    std::string   key, value;
    while (in >> key >> value)
        if (key == "lang") return value;
    return lang::from_environment();
}

enum class Page { Main, Builds, Download, Worlds, Mods, ModDetail, ModInstall };

// The mods pages' state: which view (0 everywhere, k the k-th world), what is in
// it, which row is open, and a mod dropped on the window waiting to be installed.
struct ModsView {
    size_t                         scope = 0;
    std::vector<std::string>       worlds;
    std::vector<launcher::ModEntry> mods;
    unsigned                       api_oldest = 0, api_newest = 0;
    size_t                         first = 0, open = 0;
    launcher::ModEntry             pending;
    std::string                    confirm;

    std::string world() const { return scope == 0 ? "" : worlds[scope - 1]; }
};

constexpr size_t kModsShown = 5;

std::string where_text(const launcher::ModEntry &m)
{
    return m.where == launcher::Where::Game         ? lang::tr("the game's")
           : m.where == launcher::Where::Everywhere ? lang::tr("yours, everywhere")
                                                    : lang::tr("this world's");
}

// What a mod is, for the detail and install pages: name, where, description,
// what stands in its way, what it may do.
float mod_lines(ui::Panel &p, const ModsView &v, const launcher::ModEntry &m, float y)
{
    panel_label(p, m.manifest.name + " " + m.manifest.version.str() + "   (" + where_text(m) + ")", 0.5f, y);
    y += 0.9f;
    if (!m.manifest.description.empty()) {
        panel_label(p, m.manifest.description.substr(0, 70), 0.5f, y);
        y += 0.9f;
    }
    const std::vector<std::string> trouble = launcher::problems(m, v.mods, v.api_oldest, v.api_newest);
    if (trouble.empty()) panel_label(p, "Works with this version", 0.5f, y);
    for (const std::string &t : trouble) {
        panel_label(p, "! " + t.substr(0, 68), 0.5f, y);
        y += 0.8f;
    }
    if (trouble.empty()) y += 0.9f;
    if (m.manifest.capabilities.empty()) {
        panel_label(p, "Asks for nothing beyond the world", 0.5f, y);
        y += 0.9f;
    } else {
        panel_label(p, "Asks to:", 0.5f, y);
        y += 0.8f;
        for (const std::string &c : m.manifest.capabilities) {
            panel_label(p, "  " + launcher::capability_meaning(c), 0.5f, y);
            y += 0.8f;
        }
    }
    return y + 0.2f;
}

float mods_panel(ui::Panel &p, Page page, const ModsView &v, float y)
{
    const std::string view = v.scope == 0 ? std::string(lang::tr("Mods everywhere"))
                                          : std::string(lang::tr("Mods in")) + " " + v.world();
    if (page == Page::Mods) {
        panel_button(p, "scope", view + "   >", 0.5f, y, 11.0f);
        y += 1.3f;
        for (size_t i = v.first; i < v.mods.size() && i < v.first + kModsShown; ++i) {
            const launcher::ModEntry &m = v.mods[i];
            std::string text = m.manifest.name.empty() ? m.path.substr(m.path.find_last_of("/\\") + 1)
                                                       : m.manifest.name + " " + m.manifest.version.str();
            if (m.where == launcher::Where::Game) text += std::string("   ") + lang::tr("(game)");
            if (m.off) text += std::string("   ") + lang::tr(m.off_all ? "(off everywhere)" : "(off)");
            if (!launcher::problems(m, v.mods, v.api_oldest, v.api_newest).empty()) text = "! " + text;
            panel_add(p, ui::ElemKind::Button, "mod:" + std::to_string(i), text, 0.5f, y, 11.0f);
            y += 1.1f;
        }
        y += 0.2f;
        panel_label(p, "Drop a mod folder or a .cnpmod on this window to add it", 0.5f, y);
        y += 1.1f;
        if (v.first > 0) panel_button(p, "mods_prev", "<", 0.5f, y, 1.2f);
        panel_button(p, "back", "Back", 2.0f, y, 8.0f);
        if (v.first + kModsShown < v.mods.size()) panel_button(p, "mods_next", ">", 10.3f, y, 1.2f);
        return y + 1.3f;
    }
    if (page == Page::ModDetail) {
        const launcher::ModEntry &m = v.mods[v.open];
        y = mod_lines(p, v, m, y);
        if (m.where != launcher::Where::Game && !m.off_all) {
            panel_button(p, "toggle", m.off ? "Turn on" : "Turn off", 0.5f, y, 5.4f);
            const bool mine = (m.where == launcher::Where::Everywhere) == (v.scope == 0);
            if (mine)
                panel_button(p, "mod_remove", v.confirm == m.path ? "Click again to remove" : "Remove", 6.1f, y, 5.4f);
            y += 1.3f;
        }
        panel_button(p, "mods", "Back", 0.5f, y, 11.0f);
        return y + 1.3f;
    }
    // ModInstall: what was dropped, before it is copied anywhere.
    y = mod_lines(p, v, v.pending, y);
    panel_button(p, "mod_install", std::string(lang::tr("Add to")) + " " + view, 0.5f, y, 11.0f);
    y += 1.3f;
    panel_button(p, "mods", "Cancel", 0.5f, y, 11.0f);
    return y + 1.3f;
}

ui::Panel launcher_panel(Page page, const std::vector<Build> &builds, size_t chosen,
                         const std::vector<std::string> &remote,
                         const std::vector<launcher::World> &worlds, const ModsView &mods,
                         const std::string &confirm, const std::string &error)
{
    ui::Panel p;
    p.name = "launcher";
    p.w    = 12.0f;
    float y = 0.3f;
    if (page == Page::Main) {
        panel_add(p, ui::ElemKind::Box, "", "", 0.35f, 0.15f, 11.3f);
        p.elements.back().h = 1.9f;
        p.elements.back().r = 80;
        p.elements.back().g = 170;
        p.elements.back().b = 70;
        panel_button(p, "play", builds.empty() ? std::string("Play") : std::string(lang::tr("Play")) + " " + builds[chosen].version,
                     0.5f, y, 11.0f);
        p.elements.back().h = 1.6f;
        y += 2.0f;
        panel_button(p, "worlds", "Worlds", 0.5f, y, 11.0f);
        y += 1.3f;
        panel_button(p, "mods", "Mods", 0.5f, y, 11.0f);
        y += 1.3f;
        panel_button(p, "builds", "Versions", 0.5f, y, 11.0f);
        y += 1.3f;
        panel_button(p, "quit", "Quit", 0.5f, y, 11.0f);
        y += 1.3f;
    } else if (page == Page::Builds) {
        panel_label(p, "Versions: the one marked is the one Play starts", 0.5f, y);
        y += 1.2f;
        for (size_t i = 0; i < builds.size(); ++i) {
            // Translated in pieces here: the panel looks up whole strings, and
            // a version number is in none.
            std::string text = builds[i].version;
            if (builds[i].bundled) text += std::string("   ") + lang::tr("(in this package)");
            if (i == chosen) text = "> " + text + " <";
            panel_add(p, ui::ElemKind::Button, "build:" + std::to_string(i), text, 0.5f, y,
                      builds[i].bundled ? 11.0f : 9.8f);
            if (!builds[i].bundled) {
                // Twice: the first click says what it takes with it.
                const bool armed = confirm == builds[i].version;
                panel_add(p, ui::ElemKind::Button, "remove:" + std::to_string(i), armed ? "!" : "x",
                          10.5f, y, 1.0f);
            }
            y += 1.2f;
        }
        y += 0.2f;
        panel_button(p, "download", "Download another version", 0.5f, y, 11.0f);
        y += 1.3f;
        panel_button(p, "back", "Back", 0.5f, y, 11.0f);
        y += 1.3f;
    } else if (page == Page::Worlds) {
        panel_label(p, builds.empty() ? std::string("Worlds")
                                      : std::string(lang::tr("Worlds, played with")) + " " + builds[chosen].version,
                    0.5f, y);
        y += 1.2f;
        for (size_t i = 0; i < worlds.size() && i < 8; ++i) {
            const launcher::World &w = worlds[i];
            std::string text = w.name;
            if (!w.made_with.empty()) text += "   (" + w.made_with + ")";
            if (confirm == "world:" + w.name) text = "! " + text + " !";
            panel_add(p, ui::ElemKind::Button, "world:" + std::to_string(i), text, 0.5f, y, 11.0f);
            y += 1.2f;
        }
        if (worlds.empty()) {
            panel_label(p, "no world yet: Play makes one", 0.5f, y);
            y += 1.2f;
        }
        y += 0.2f;
        panel_button(p, "back", "Back", 0.5f, y, 11.0f);
        y += 1.3f;
    } else if (page == Page::Mods || page == Page::ModDetail || page == Page::ModInstall) {
        y = mods_panel(p, page, mods, y);
    } else {
        panel_label(p, "Versions on the releases page", 0.5f, y);
        y += 1.2f;
        size_t shown = 0;
        for (size_t i = 0; i < remote.size() && shown < 8; ++i) {
            if (std::any_of(builds.begin(), builds.end(), [&](const Build &b) { return b.version == remote[i]; }))
                continue;
            panel_add(p, ui::ElemKind::Button, "get:" + remote[i], remote[i], 0.5f, y, 11.0f);
            y += 1.2f;
            ++shown;
        }
        if (shown == 0) {
            panel_label(p, remote.empty() ? "asking ..." : "every version is here already", 0.5f, y);
            y += 1.2f;
        }
        y += 0.2f;
        panel_button(p, "back", "Back", 0.5f, y, 11.0f);
        y += 1.3f;
    }
    for (size_t at = 0; at < error.size();) {   // a line a row
        const size_t nl = error.find('\n', at);
        panel_label(p, error.substr(at, std::min<size_t>(70, nl == std::string::npos ? std::string::npos : nl - at)), 0.5f, y);
        y += 0.8f;
        at = nl == std::string::npos ? error.size() : nl + 1;
    }
    if (!error.empty()) y += 0.2f;
    p.h = y;
    return p;
}

} // namespace

int main(int argc, char **argv)
{
    // Without a window, for a script and for checking: the same calls the
    // screens make.
    bool        headless = false;
    std::string install_v, remove_v, add_mod, mods_world;
    bool        list_mods = false;
    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        if (a == "--releases-from" && i + 1 < argc) launcher::g_download_base = argv[++i];
        else if (a == "--releases-api" && i + 1 < argc) launcher::g_releases_api = argv[++i];
        else if (a == "--install" && i + 1 < argc) install_v = argv[++i], headless = true;
        else if (a == "--remove" && i + 1 < argc) remove_v = argv[++i], headless = true;
        else if (a == "--list") headless = true;
        else if (a == "--add-mod" && i + 1 < argc) add_mod = argv[++i], headless = list_mods = true;
        else if (a == "--mods") headless = list_mods = true;
        else if (a == "--world" && i + 1 < argc) mods_world = argv[++i];
        else {
            std::fprintf(stderr, "usage: cnp_launcher [--releases-from URL] [--releases-api URL]\n"
                                 "                    [--list | --install VERSION | --remove VERSION]\n"
                                 "                    [--mods | --add-mod PATH] [--world NAME]\n");
            return 2;
        }
    }
    if (headless && list_mods) {
        // The mods of the packaged build's view, as the mods page shows them.
        const std::vector<Build> all = find_builds();
        const auto b = std::find_if(all.begin(), all.end(), [](const Build &x) { return x.bundled; });
        const std::string game = b == all.end() ? "" : launcher::game_mods_dir(b->client, true);
        std::vector<launcher::ModEntry> mods = launcher::list_mods(game, mods_world);
        if (!add_mod.empty()) {
            launcher::ModEntry m;
            std::string        err;
            if (launcher::read_mod(add_mod, m, err)) err = launcher::install_mod(m, mods_world, mods);
            if (!err.empty()) {
                std::fprintf(stderr, "cnp_launcher: %s\n", err.c_str());
                return 1;
            }
            mods = launcher::list_mods(game, mods_world);
        }
        for (const launcher::ModEntry &m : mods) {
            if (m.where == launcher::Where::Game) continue;
            std::printf("%s %s%s%s\n", m.manifest.name.c_str(), m.manifest.version.str().c_str(),
                        m.off ? " off" : "", m.where == launcher::Where::World ? " (world)" : "");
            for (const std::string &t : launcher::problems(m, mods, kModApiOldest, kModApi))
                std::printf("  ! %s\n", t.c_str());
        }
        return 0;
    }
    if (headless) {
        launcher::Progress progress;
        const std::string  err = !install_v.empty() ? launcher::install(install_v, progress)
                                 : !remove_v.empty() ? launcher::remove(remove_v)
                                                     : "";
        if (!err.empty()) {
            std::fprintf(stderr, "cnp_launcher: %s\n", err.c_str());
            return 1;
        }
        for (const Build &b : find_builds())
            std::printf("%s%s  %s\n", b.version.c_str(), b.bundled ? " (package)" : "", b.client.c_str());
        return 0;
    }

    lang::load(data_dir("assets/lang", CNP_SOURCE_ROOT "/assets/lang"), options_language());

    std::vector<Build> builds = find_builds();
    size_t             chosen = 0;
    std::string        wanted = read_default();
    for (size_t i = 0; i < builds.size(); ++i)
        if (builds[i].version == wanted) chosen = i;

    // The game knows it was started from here (core/paths.h, kExitWantsVersion).
#ifdef _WIN32
    _putenv_s("CNP_LAUNCHER", "1");
#else
    setenv("CNP_LAUNCHER", "1", 1);
#endif

    // A server the game met that wants another version: followed by starting
    // that version (installed first if it is not here) on that server.
    std::string follow_server, follow_version;

    for (;;) {
        builds = find_builds();
        for (size_t i = 0; i < builds.size(); ++i)
            if (builds[i].version == wanted) chosen = i;

        gfx_desc desc{};
        desc.width     = 1280;
        desc.height    = 720;
        desc.title     = "Cubes and Portals";
        desc.resizable = true;
        if (!gfx_init(&desc)) {
            std::fprintf(stderr, "cnp_launcher: no window\n");
            return 1;
        }
        plat_disable_default_exit_key();

        Page        page = Page::Main;
        std::string error = builds.empty() ? "no build of the game found" : "";
        const Build             *start = nullptr;
        std::vector<std::string> start_args;
        std::vector<launcher::World> worlds;
        ModsView                     mods;
        bool        pressed_here = false;

        std::vector<std::string>              remote;
        std::future<std::vector<std::string>> asking;
        std::future<std::string>              installing;
        std::string                           installing_v, confirm;
        launcher::Progress                    progress;

        while (!plat_should_close() && start == nullptr) {
            // An install going on says how far it is; one that ended says how.
            if (installing.valid()) {
                if (installing.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
                    const std::string err = installing.get();
                    error = err.empty() ? installing_v + " installed" : err;
                    const std::string keep = builds.empty() ? "" : builds[chosen].version;
                    builds = find_builds();
                    for (size_t i = 0; i < builds.size(); ++i)
                        if (builds[i].version == keep) chosen = i;
                    page = Page::Builds;
                    if (!err.empty()) follow_server.clear();
                } else {
                    std::error_code ec;
                    const uint64_t  have = fs::file_size(launcher::download_path(installing_v), ec);
                    const uint64_t  all  = progress.size;
                    error = std::string(lang::tr("downloading")) + " " + installing_v +
                            (all > 0 && !ec ? "  " + std::to_string(have * 100 / all) + "%" : "");
                }
            }
            // Following a server: its version if it is here, else fetched first.
            if (!follow_server.empty() && !installing.valid()) {
                const auto b = std::find_if(builds.begin(), builds.end(),
                                            [&](const Build &x) { return x.version == follow_version; });
                if (b != builds.end()) {
                    start      = &*b;
                    start_args = {"--join-listed", follow_server};
                    follow_server.clear();
                } else if (installing_v != follow_version) {
                    installing_v  = follow_version;
                    progress.size = 0;
                    installing    = std::async(std::launch::async, [v = installing_v, &progress] {
                        return launcher::install(v, progress);
                    });
                    page = Page::Builds;
                } else {
                    follow_server.clear();   // fetched and still not here: the error says why
                }
            }
            if (asking.valid() && asking.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
                remote = asking.get();
                if (remote.empty()) error = "the releases page did not answer";
            }

            // A mod dropped on the window, on any mods page: shown before it is added.
            if (const int dropped = plat_take_dropped();
                dropped > 0 && (page == Page::Mods || page == Page::ModDetail || page == Page::ModInstall)) {
                std::string err;
                if (launcher::read_mod(plat_dropped_path(0), mods.pending, err)) {
                    mods.pending.where = mods.scope == 0 ? launcher::Where::Everywhere : launcher::Where::World;
                    page               = Page::ModInstall;
                    error.clear();
                } else {
                    error = err;
                }
            }

            const ui::Panel p = launcher_panel(page, builds, chosen, remote, worlds, mods, confirm, error);
            // Below the game's name when it fits there: the mods pages are taller
            // than any of the title screen's.
            ui::PanelPlacement at = ui::place_panel(p, gfx_screen_width(), gfx_screen_height());
            const float        top = (float)gfx_screen_height() * 0.19f;
            if (at.y < top && top + p.h * at.cell <= (float)gfx_screen_height()) at.y = top;
            float mx = 0.0f, my = 0.0f;
            plat_mouse_position(&mx, &my);
            const int hover = at.button_at(p, mx, my);

            gfx_begin_frame();
            gfx_clear(0.0f, 0.0f, 0.0f, 1.0f, true);
            gfx_2d_begin(gfx_screen_width(), gfx_screen_height());
            draw_title_backdrop();
            ui::draw_mod_panel(p, at, hover, gfx_screen_width(), gfx_screen_height());
            draw_title_name();
            gfx_2d_end();
            gfx_end_frame();

            if (plat_mouse_pressed(PLAT_MOUSE_LEFT)) pressed_here = true;
            std::string id;
            if (plat_mouse_released(PLAT_MOUSE_LEFT) && pressed_here && hover >= 0) id = p.elements[(size_t)hover].id;
            if (plat_mouse_released(PLAT_MOUSE_LEFT)) pressed_here = false;
            if (plat_key_pressed(PLAT_KEY_ESC)) id = page == Page::Main ? "quit" : "back";

            if (!id.empty() && id.rfind("remove:", 0) != 0 && id.rfind("world:", 0) != 0 && id != "mod_remove") {
                confirm.clear();
                if (!installing.valid()) error.clear();
            }
            if (id == "quit") break;
            if (id == "builds") page = Page::Builds;
            if (id == "worlds") {
                worlds = launcher::list_worlds();
                page   = Page::Worlds;
            }
            const auto reload_mods = [&] {
                mods.mods = launcher::list_mods(builds.empty() ? "" : launcher::game_mods_dir(builds[chosen].client, builds[chosen].bundled),
                                                mods.world());
                if (!builds.empty()) launcher::api_range(builds[chosen].client, builds[chosen].bundled, mods.api_oldest, mods.api_newest);
                mods.first = std::min(mods.first, mods.mods.empty() ? 0 : (mods.mods.size() - 1) / kModsShown * kModsShown);
            };
            if (id == "mods") {
                if (page == Page::Main) {
                    mods.scope  = 0;
                    mods.worlds.clear();
                    for (const launcher::World &w : launcher::list_worlds()) mods.worlds.push_back(w.name);
                }
                mods.confirm.clear();
                reload_mods();
                page = Page::Mods;
            }
            if (id == "scope") {
                mods.scope = (mods.scope + 1) % (mods.worlds.size() + 1);
                mods.first = 0;
                reload_mods();
            }
            if (id == "mods_prev") mods.first -= std::min(mods.first, kModsShown);
            if (id == "mods_next") mods.first += kModsShown;
            if (id.rfind("mod:", 0) == 0) {
                mods.open    = std::min(mods.mods.size() - 1, (size_t)std::strtoul(id.c_str() + 4, nullptr, 10));
                mods.confirm.clear();
                page = Page::ModDetail;
            }
            if (id == "toggle") {
                const launcher::ModEntry &m = mods.mods[mods.open];
                launcher::set_off(mods.world(), m.manifest.name, !m.off);
                const std::string path = m.path;
                reload_mods();
                for (size_t i = 0; i < mods.mods.size(); ++i)
                    if (mods.mods[i].path == path) mods.open = i;
            }
            if (id == "mod_remove") {
                const launcher::ModEntry &m = mods.mods[mods.open];
                if (mods.confirm != m.path) {
                    mods.confirm = m.path;
                } else {
                    const std::string err = launcher::remove_mod(m);
                    error = err.empty() ? m.manifest.name + " " + lang::tr("removed") : err;
                    mods.confirm.clear();
                    reload_mods();
                    page = Page::Mods;
                }
            }
            if (id == "mod_install") {
                const std::string err = launcher::install_mod(mods.pending, mods.world(), mods.mods);
                error = err.empty() ? mods.pending.manifest.name + " " + lang::tr("added") : err;
                reload_mods();
                page = Page::Mods;
            }
            if (id.rfind("world:", 0) == 0 && !builds.empty() && !installing.valid()) {
                // Which build last had it: what made it, or a newer one the launcher
                // opened it with since. A save line apart is asked twice; a newer
                // build gets a copy of the world as it was first, kept in backups/.
                const launcher::World &w = worlds[std::min(worlds.size() - 1, (size_t)std::strtoul(id.c_str() + 6, nullptr, 10))];
                const Build &b   = builds[chosen];
                std::string  ref = w.made_with;
                if (!w.last_opened.empty() && (ref.empty() || update::compare(w.last_opened, ref) > 0)) ref = w.last_opened;
                const bool apart = !ref.empty() && launcher::save_line(ref) != launcher::save_line(b.version);
                if (apart && confirm != "world:" + w.name) {
                    confirm = "world:" + w.name;
                    error   = ref + " " + lang::tr("made it: this version may not open it.") + "\n" +
                              lang::tr("Click again to play; a copy is kept first.");
                } else {
                    std::string err;
                    if (ref.empty() || update::compare(b.version, ref) > 0)
                        err = launcher::backup_world(w.name, ref.empty() ? "before-" + b.version : ref);
                    confirm.clear();
                    if (!err.empty()) {
                        error = err;
                    } else {
                        launcher::opened_with(w.name, b.version);
                        start      = &b;
                        start_args = {"--play-world", w.name};
                    }
                }
            }
            if (id == "back") {
                page = page == Page::Download ? Page::Builds : Page::Main;
                confirm.clear();
            }
            if (id == "download") {
                page = Page::Download;
                if (remote.empty() && !asking.valid()) asking = std::async(std::launch::async, launcher::remote_versions);
            }
            if (id.rfind("get:", 0) == 0 && !installing.valid()) {
                installing_v  = id.substr(4);
                progress.size = 0;
                installing    = std::async(std::launch::async, [v = installing_v, &progress] {
                    return launcher::install(v, progress);
                });
                page = Page::Builds;
            }
            if (id.rfind("remove:", 0) == 0) {
                const Build &b = builds[std::min(builds.size() - 1, (size_t)std::strtoul(id.c_str() + 7, nullptr, 10))];
                if (confirm != b.version) {
                    confirm = b.version;
                    const size_t made = launcher::worlds_made_with(b.version).size();
                    error = made == 0 ? std::string(lang::tr("click again to remove")) + " " + b.version
                                        : std::to_string(made) + " " + lang::tr("world(s) were made with it: click again to remove it");
                } else {
                    const std::string keep = builds[chosen].version, err = launcher::remove(b.version);
                    error   = err.empty() ? b.version + " " + lang::tr("removed") : err;
                    confirm.clear();
                    builds = find_builds();
                    chosen = 0;
                    for (size_t i = 0; i < builds.size(); ++i)
                        if (builds[i].version == keep) chosen = i;
                    if (!builds.empty() && builds[chosen].version != keep) write_default(builds[chosen].version);
                }
            }
            if (id.rfind("build:", 0) == 0) {
                chosen = std::min(builds.size() - 1, (size_t)std::strtoul(id.c_str() + 6, nullptr, 10));
                write_default(builds[chosen].version);
                page = Page::Main;
            }
            if (id == "play" && !builds.empty() && !installing.valid()) start = &builds[chosen];

            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }

        if (start == nullptr) {
            gfx_shutdown();
            return 0;
        }

        // The game in a window of its own; this one closes and waits for it,
        // because a launcher that left would take the game down with it (os.h).
        gfx_shutdown();
        ChildProcess game;
        std::string  err;
        const std::string dir = fs::path(start->client).parent_path().string();
        if (!game.start(start->client, start_args, dir, user_data_dir() + "/game.log", err)) {
            std::fprintf(stderr, "cnp_launcher: %s: %s\n", start->client.c_str(), err.c_str());
            return 1;
        }
        std::printf("cnp_launcher: started %s (%s)\n", start->version.c_str(), start->client.c_str());
        while (!game.exited()) std::this_thread::sleep_for(std::chrono::milliseconds(200));
        if (game.exit_code != kExitWantsVersion) return 0;

        // The game met a server wanting another version: what it wrote, read once.
        std::ifstream in(launcher_request_path());
        follow_server.clear();
        follow_version.clear();
        for (std::string line; std::getline(in, line);) {
            if (line.rfind("server = ", 0) == 0) follow_server = line.substr(9);
            if (line.rfind("version = ", 0) == 0) follow_version = line.substr(10);
        }
        in.close();
        std::error_code ec;
        fs::remove(launcher_request_path(), ec);
        if (!follow_version.empty() && follow_version[0] == 'v') follow_version.erase(0, 1);
        std::printf("cnp_launcher: %s wants %s\n", follow_server.c_str(), follow_version.c_str());
    }
}
