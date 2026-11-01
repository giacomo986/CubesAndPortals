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
#include "core/os.h"
#include "core/paths.h"
#include "core/update_check.h"
#include "core/version.h"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <filesystem>
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

enum class Page { Main, Builds };

ui::Panel launcher_panel(Page page, const std::vector<Build> &builds, size_t chosen,
                         const std::string &error)
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
        panel_button(p, "builds", "Versions", 0.5f, y, 11.0f);
        y += 1.3f;
        panel_button(p, "quit", "Quit", 0.5f, y, 11.0f);
        y += 1.3f;
    } else {
        panel_label(p, "Versions: the one marked is the one Play starts", 0.5f, y);
        y += 1.2f;
        for (size_t i = 0; i < builds.size(); ++i) {
            // Translated in pieces here: the panel looks up whole strings, and
            // a version number is in none.
            std::string text = builds[i].version;
            if (builds[i].bundled) text += std::string("   ") + lang::tr("(in this package)");
            if (i == chosen) text = "> " + text + " <";
            panel_add(p, ui::ElemKind::Button, "build:" + std::to_string(i), text, 0.5f, y, 11.0f);
            y += 1.2f;
        }
        y += 0.2f;
        panel_button(p, "back", "Back", 0.5f, y, 11.0f);
        y += 1.3f;
    }
    if (!error.empty()) {
        panel_label(p, error.substr(0, 70), 0.5f, y);
        y += 1.0f;
    }
    p.h = y;
    return p;
}

} // namespace

int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    lang::load(data_dir("assets/lang", CNP_SOURCE_ROOT "/assets/lang"), options_language());

    std::vector<Build> builds = find_builds();
    size_t             chosen = 0;
    const std::string  wanted = read_default();
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
    const Build *start = nullptr;
    bool        pressed_here = false;

    while (!plat_should_close() && start == nullptr) {
        const ui::Panel          p  = launcher_panel(page, builds, chosen, error);
        const ui::PanelPlacement at = ui::place_panel(p, gfx_screen_width(), gfx_screen_height());
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

        if (id == "quit") break;
        if (id == "builds") page = Page::Builds;
        if (id == "back") page = Page::Main;
        if (id.rfind("build:", 0) == 0) {
            chosen = std::min(builds.size() - 1, (size_t)std::strtoul(id.c_str() + 6, nullptr, 10));
            write_default(builds[chosen].version);
            page = Page::Main;
        }
        if (id == "play" && !builds.empty()) start = &builds[chosen];

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
    if (!game.start(start->client, {}, dir, user_data_dir() + "/game.log", err)) {
        std::fprintf(stderr, "cnp_launcher: %s: %s\n", start->client.c_str(), err.c_str());
        return 1;
    }
    std::printf("cnp_launcher: started %s (%s)\n", start->version.c_str(), start->client.c_str());
    while (!game.exited()) std::this_thread::sleep_for(std::chrono::milliseconds(200));
    return 0;
}
