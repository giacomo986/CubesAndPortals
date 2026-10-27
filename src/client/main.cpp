// M0 spike: the three hypotheses of DESIGN.md 8 that have to be falsified
// before any of the rest is worth writing.
//
//   1. A crossing is visually continuous -- no frame shows the geometry sitting
//      between the virtual camera and the portal.
//   2. The portal stays correct with the camera INSIDE the frame, the case that
//      breaks Lengyel's oblique near plane.
//   3. 200 FPS or better with two portals at 1080p, with scripting under 1% of
//      the frame.
//
// Run with no arguments to fly around; run with --bench for the scripted pass
// that captures each criterion to a PNG and measures the frame rate. A kill
// switch has to be decidable from evidence someone else can reproduce, which
// means screenshots on disk rather than a description of what was on screen.

#include "client/gfx/gfx.h"
#include "client/snd/snd.h"
#include "core/os.h"
#include "core/paths.h"
#include "core/version.h"
#include "core/audio/attenuation.h"
#include "core/audio/tone.h"
#include "client/gfx/platform.h"
#include "core/physics/player.h"
#include "core/physics/craft.h"
#include "client/render/body_render.h"
#include "client/render/fx_render.h"
#include "client/render/mod_pass.h"
#include "client/render/portal_render.h"
#include "client/render/selection.h"
#include "client/ui/hud.h"
#include "client/ui/lang.h"
#include "core/update_check.h"
#include "client/render/world_render.h"
#include "core/math/mat4.h"
#include "core/media/media_set.h"
#include "core/media/png.h"
#include "core/media/texture_pack.h"
#include "client/render/model_render.h"
#include "client/render/model_store.h"
#include "core/diag/profiler.h"
#include "core/diag/stack_dump.h"
#include "core/net/chat.h"
#include "core/net/client_session.h"
#include "core/physics/aabb.h"
#include "core/physics/movement.h"
#include "core/portal/portal_clip.h"
#include "core/portal/portal_math.h"
#include "core/portal/portal_traverse.h"
#include "core/portal/portal_tuning.h"
#include "core/registry/core_nodes.h"
#include "core/item/inventory.h"
#include "core/ecs/drop.h"
#include "core/ecs/health.h"
#include "core/ecs/mob.h"
#include "core/physics/fall.h"
#include "core/ui/page.h"
#include "core/ui/panel.h"
#include "core/item/item_registry.h"
#include "core/item/recipe.h"
#include "core/registry/model_registry.h"
#include "core/registry/node_registry.h"
#include "core/registry/node_shape.h"
#include "core/voxel/chunk.h"
#include "core/worldgen/spawn.h"
#include "core/worldgen/worldgen.h"
#include "core/voxel/light.h"
#include "core/voxel/raycast.h"
#include "core/storage/column_keeper.h"
#include "core/storage/mod_store.h"
#include "core/storage/world_db.h"
#include "core/world/modifiers.h"
#include "core/world/node_meta.h"
#include "core/ecs/lua_ecs.h"
#include "core/voxel/find.h"
#include "core/track/piece.h"
#include "core/voxel/pathfind.h"
#include "core/voxel/dig.h"
#include "script/mod_host.h"
#include "script/world_code.h"
#include "script/script.h"

#include <fstream>
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

#include <atomic>
#include <cctype>
#include <cerrno>
#include <future>
#include <map>
#include <optional>
#include <deque>
#include <memory>
#include <random>
#include <set>
#include <tuple>
#include <thread>

#ifdef _WIN32
#include <process.h>
#else
#include <unistd.h>
#endif
#if defined(__GLIBC__)
#include <malloc.h>
#endif

using namespace cnp;

namespace {

constexpr int   kWindowWidth  = 1920;
constexpr int   kWindowHeight = 1080;
constexpr float kFovY         = 1.2217f;   // 70 degrees
float           g_fov_y       = kFovY;     // narrowed by core.zoom: a bow drawn
constexpr float kZNear        = 0.05f;
constexpr float kZFar         = 500.0f;

// Assigned by the registry at startup rather than fixed here. That the engine
// no longer knows the id of stone is the point of M2.
BlockId kStone   = 0;
BlockId kGrass   = 0;
BlockId kFrame   = 0;
BlockId kBedrock = 0;
BlockId kGlass   = 0;

gfx_rt g_capture_rt{0};   // see read_frame_pixels

// F2: occlusion culling off, so everything the frustum keeps is drawn whether
// the walk reached it or not. The one switch that tells a player, in the middle
// of a game, whether something that vanished was hidden by the walk -- and the
// HUD says when it is off, so nobody measures a frame rate with it forgotten.
bool g_occlusion = true;

// Mouse sensitivity, as a multiple of kLookSpeed. Set from the pause menu's
// options and applied where both loops read the deltas.
float g_look_scale = 1.0f;

// The size a frame is DRAWN at: the window's, unless a gate has bound a capture
// target of another size. The two are the same everywhere but OPEN-37, which
// needs the same view at more than one resolution -- a crack that comes from
// rounding moves with the pixel grid and a hole in the mesh does not -- and
// asking the window there would draw a 1920-wide picture into a 2560-wide
// target and read back two thirds of a frame.
int render_width();
int render_height();

// How big a portal's render target is, as a fraction of the window.
//
// DESIGN.md 4.2 asks for the target at the PORTAL's resolution rather than the
// screen's. Today an opening a few hundred pixels wide is fed by a target the
// size of the whole window, and most of that fill rate lands outside the hole.
// It is the last item of the passive budget that can still move a number --
// audio and particles do not exist here and there is no prediction to suppress
// -- and it has a cost of its own rather than being free: the opening samples
// the target by SCREEN position, so a smaller target is a softer image inside
// the hole. Both halves are measured by --dual-bench.
float g_portal_scale = 1.0f;

// How deep portal images go, as main() was asked for it. A global only because
// the bench rebuilds the target pool at other resolutions and has to rebuild it
// the size it was, not the size the default would give.
int g_recursion_depth = 1;

int render_width()  { return g_capture_rt.id ? gfx_rt_width(g_capture_rt) : gfx_screen_width(); }
int render_height() { return g_capture_rt.id ? gfx_rt_height(g_capture_rt) : gfx_screen_height(); }

int portal_rt_dim(int screen_dim)
{
    const int d = (int)((float)screen_dim * g_portal_scale + 0.5f);
    return d < 16 ? 16 : d;   // a target smaller than this is not an image
}

// One shape for the length of a measurement, then the old one back.
//
// A gate that changes a setting and forgets to put it back does not fail: it
// changes what every check after it is measuring, quietly, and the first sign
// is a number nobody can reproduce by hand.
struct ScopedTuning {
    PortalTuning saved;

    explicit ScopedTuning(const PortalTuning &t) : saved(portal_tuning())
    {
        std::string err;
        portal_tuning_set(t, err);
    }
    ~ScopedTuning()
    {
        std::string err;
        portal_tuning_set(saved, err);
    }

    ScopedTuning(const ScopedTuning &)            = delete;
    ScopedTuning &operator=(const ScopedTuning &) = delete;
};

// The definitions live in core/registry/core_nodes.cpp, not here. The ids they
// come back with have to be the same ones a server assigns, and the way to make
// that true is one list rather than a client copy that happens to match.
void register_client_core_nodes(NodeRegistry &reg)
{
    const CoreNodes ids = register_core_nodes(reg);
    kStone   = ids.stone;
    kGrass   = ids.grass;
    kBedrock = ids.bedrock;
    kFrame   = ids.frame;
    kGlass   = ids.glass;
}


// Horizontal extent of the generated world, in blocks. --world N overrides it;
// the M1 exit test wants 512.
int g_world_extent = 128;

// How long a day lasts and what hour a world starts at, for the loops that own
// their world. --day-length 20 is how the whole cycle gets looked at without
// booking the afternoon, and --time 0.75 starts one at dusk.
//
// Neither reaches a network client: there the clock is the server's, and a
// second one here would be the second clock the design says not to have.
double g_day_length = kDayLength;
double g_start_time = kNoon;

Vec3 dir_from_angles(float yaw, float pitch)
{
    const float cp = std::cos(pitch);
    return Vec3{cp * std::sin(yaw), std::sin(pitch), -cp * std::cos(yaw)};
}

// The slab a portal is set into, with the opening carved out of it.
//
// "Slab" and not "wall" because the surface follows the portal: a wall portal
// gets a wall, a floor portal gets a floor, and a portal rolled onto its side
// gets a wall with the hole lying down. That generality is not decoration --
// build_opening in the renderer emits four tunnel sides, and a portal standing
// free in the open leaves those four quads hanging in mid air with nothing
// behind them.
//
// Everything is expressed in the portal's own basis, so there is no axis test
// anywhere in here. The version this replaced picked between two block-index
// formulas on whether the right vector pointed along x, which is a question
// only a portal whose up is +Y can answer.
void build_slab_with_opening(const MapgenContext &ctx, const PortalSurface &p, int span_w,
                             int span_h, BlockId material)
{
    const Vec3 r = p.right();
    const Vec3 u = p.up;

    for (int dh = -span_h; dh <= span_h; ++dh) {
        for (int dw = -span_w; dw <= span_w; ++dw) {
            // The CENTRE of the cell, one whole node per step. Along the short
            // in-plane axis the opening's centre already sits at a node centre,
            // so whole steps land on node centres; along the long one it sits
            // on a node BOUNDARY, so the centres are half a node off it.
            //
            // That half node is not a nicety. Stepping to the boundary itself
            // and flooring picks the cell on the positive side of it, which is
            // the right cell only while `u` points along a positive axis --
            // true of every portal built before this engine had one that was
            // not upright, and false for a destination turned a quarter turn,
            // where it put the hole one node past the opening and left the
            // opening's own first node walled up. That reads on screen as a
            // portal showing the wrong part of the world, not as a hole in the
            // wrong place.
            const Vec3 cell = p.center + r * (float)dw + u * ((float)dh + 0.5f);

            const int bx = (int)std::floor(cell.x);
            const int by = (int)std::floor(cell.y);
            const int bz = (int)std::floor(cell.z);

            // Carve exactly the cells whose centres fall inside the opening,
            // asked of the opening's own half extents rather than written out
            // as a pair of magic offsets.
            const bool in_opening = std::fabs((float)dw) < p.half_w &&
                                    std::fabs((float)dh + 0.5f) < p.half_h;
            ctx.set_block(bx, by, bz, in_opening ? kAir : material);
        }
    }
}

// The same opening, in the form a body needs rather than the form a camera
// needs. One function, called from every place that needs both, because two
// representations of one opening is exactly the pair that drifts: a renderer
// showing a portal somewhere the player cannot walk through is the drift that
// takes longest to diagnose.
//
// The rotation is READ OFF `up`, not assumed. It used to be left at zero, which
// is right for every portal whose up is +Y and silently wrong for any other:
// a floor portal's up lies in the XZ plane, and with rot 0 the body would have
// used a different in-plane basis from the camera -- same hole on screen, exit
// pose turned a quarter turn away from it. Nothing caught that because nothing
// had ever built a portal that was not upright.
Portal portal_from_surface(const PortalSurface &surf)
{
    Portal p;
    p.center = surf.center;
    p.half_w = surf.half_w;
    p.half_h = surf.half_h;
    p.kind   = PortalKind::Frame;

    // The axis and facing come off the normal, which is the one thing both
    // representations definitely agree on.
    if (std::fabs(surf.normal.z) > 0.5f) {
        p.axis        = PortalAxis::XY;
        p.normal_sign = surf.normal.z > 0.0f ? 1 : -1;
    } else if (std::fabs(surf.normal.x) > 0.5f) {
        p.axis        = PortalAxis::YZ;
        p.normal_sign = surf.normal.x > 0.0f ? 1 : -1;
    } else {
        p.axis        = PortalAxis::XZ;
        p.normal_sign = surf.normal.y > 0.0f ? 1 : -1;
    }

    // And the quarter turn is whichever of the four puts the body's up on the
    // surface's. Searched rather than derived: four candidates is cheaper to
    // read and impossible to get subtly wrong, and this runs twice per scene.
    for (int rot = 0; rot < 4; ++rot) {
        p.rot = rot;
        if (length(p.up() - surf.up) < 1e-3f) return p;
    }

    // No rotation matches, which means `up` is not perpendicular to `normal` or
    // not axis aligned. Said out loud: silently keeping rot 0 is how the bug
    // above survived, and a portal built from a bad basis should be noisy.
    std::printf("portal: up %.2f %.2f %.2f does not lie in the plane of normal "
                "%.2f %.2f %.2f -- keeping rot 0\n",
                surf.up.x, surf.up.y, surf.up.z, surf.normal.x, surf.normal.y, surf.normal.z);
    p.rot = 0;
    return p;
}

// Where block textures are read from when this process is the one generating
// the world. The files are generated by tools/bake_textures and committed, so
// a fresh clone draws blocks from files rather than from a fallback.
//
// A local world only. A world on somebody else's server is drawn from what
// that server sent -- see world_media() below -- and reading a remote world's
// stone out of this folder would be drawing THIS build's stone and calling it
// theirs.
std::string g_textures_dir = data_dir("assets/textures", CNP_SOURCE_ROOT "/assets/textures");

// Where a model named in a world's table is looked for. One directory and not a
// search path: the names come off somebody else's server, and a name is turned
// into exactly one file inside exactly one place (render/model_store.h).
std::string g_models_dir = data_dir("assets/models", CNP_SOURCE_ROOT "/assets/models");

// Where the client's own sounds are read from (SoundBank). Not a world's media:
// a sound is heard by this player only, so two clients disagreeing costs nothing.
std::string g_sounds_dir = data_dir("assets/sounds", CNP_SOURCE_ROOT "/assets/sounds");
// The translations, one file a language (client/ui/lang.h).
std::string g_lang_dir = data_dir("assets/lang", CNP_SOURCE_ROOT "/assets/lang");

// Which picture each block wears in that folder. Empty for the folder this
// engine ships, which names its files after its own blocks and needs no
// manifest -- see core/media/texture_pack.h on why an empty table has to be the
// old rule and not an error.
TexturePack g_pack;

// Every pack the command line named, in the order it named them, so a player
// can put one on and take it off again without restarting. The folder above is
// whichever of these is selected.
std::vector<std::string> g_pack_dirs;
int                      g_pack_index = 0;

// The media every session shares. One per process, on disk, keyed by content:
// DESIGN.md 4.5's "due server che spediscono la stessa texture la scaricano una
// volta sola" is this object being one object and not one per connection.
MediaCache  g_media_cache;
std::string g_media_dir;   // filled at startup; empty means no cache at all
bool        g_world_code_refused = false;   // --world-code refuse: no world's code runs here (OPEN-49)
size_t      g_media_budget = kMediaCacheBudget;

// Reads one texture file. Missing is not an error: a block whose mod shipped no
// art gets the generated placeholder, and the renderer counts how many.
bool read_texture_file(const std::string &file, std::vector<uint8_t> &out)
{
    out.clear();
    if (g_textures_dir.empty()) return false;

    // Through the pack's manifest, so a folder of somebody else's art answers
    // for a block whose name it has never heard of. Without a manifest this is
    // `file` unchanged.
    const std::string path = g_textures_dir + "/" + pack_file(g_pack, file);
    std::FILE        *f    = std::fopen(path.c_str(), "rb");
    if (f == nullptr) return false;

    std::fseek(f, 0, SEEK_END);
    const long size = std::ftell(f);
    std::fseek(f, 0, SEEK_SET);
    if (size <= 0 || size > 16 * 1024 * 1024) {
        std::fclose(f);
        return false;
    }
    out.resize((size_t)size);
    const size_t got = std::fread(out.data(), 1, out.size(), f);
    std::fclose(f);
    if (got != out.size()) {
        out.clear();
        return false;
    }
    return true;
}

// Puts a pack on, by index, wrapping -- so this is also "the next one".
//
// It says what it selected, every time, because a pack that is not there and a
// pack that is there and mapped nothing look identical on screen: brown stone
// either way. The line is the only place the difference is visible.
bool select_pack(int index)
{
    if (g_pack_dirs.empty()) {
        g_pack = TexturePack{};
        g_textures_dir.clear();
        return false;
    }

    const int n  = (int)g_pack_dirs.size();
    g_pack_index = ((index % n) + n) % n;

    const std::string dir = g_pack_dirs[(size_t)g_pack_index];
    std::string       why;
    if (!load_texture_pack(dir, g_pack, why)) {
        std::fprintf(stderr, "textures: %s\n", why.c_str());
        g_pack = TexturePack{};
        g_textures_dir.clear();
        return false;
    }

    g_textures_dir = dir;
    std::printf("textures: '%s' (%d of %d) from %s -- %d name(s) mapped, %d refused\n",
                g_pack.name.c_str(), g_pack_index + 1, n, dir.c_str(), (int)g_pack.alias.size(),
                g_pack.refused);
    return true;
}

// Where one world's blocks get their pictures.
//
// A session means a world somebody else owns: its media come from it, over the
// wire, out of the shared cache -- and nothing else. No fallback to
// g_textures_dir, on purpose. A client that could not find the far world's
// stone and drew its own would show a picture of the wrong game, and it would
// look exactly right; the renderer's generated placeholder at least says that
// nothing arrived.
//
// A null session means the world this process generated, and then the folder is
// the source, because it is the same build that registered the blocks.
MediaSource world_media(net::ClientSession *session)
{
    if (session == nullptr) return read_texture_file;

    return [session](const std::string &file, std::vector<uint8_t> &out) {
        return session->media_file(file, out);
    };
}

// Whether the world behind a mouth will take somebody who walks in.
//
// The rule itself lives on ClientSession, where a test without a window can
// reach it; this is only "and there is a session at all".
bool mouth_is_passable(const net::ClientSession *far)
{
    return far != nullptr && far->playing() && far->world_has_room();
}


// Where the shared cache lives when nobody said. Under the user's cache
// directory rather than next to the binary: media are per player and not per
// checkout, and a portal into somebody's world should not write into a source
// tree.
std::string default_media_dir()
{
#ifdef _WIN32
    if (const char *app = std::getenv("LOCALAPPDATA"); app != nullptr && app[0] != '\0')
        return std::string(app) + "/cubes-and-portals/media";
#endif
    if (const char *xdg = std::getenv("XDG_CACHE_HOME"); xdg != nullptr && xdg[0] != '\0')
        return std::string(xdg) + "/cubes-and-portals/media";
    if (const char *home = std::getenv("HOME"); home != nullptr && home[0] != '\0')
        return std::string(home) + "/.cache/cubes-and-portals/media";
    return "";
}

// The pause menu's options, kept between runs of interactive play: mouse
// sensitivity, the volume and whether the debug text is up. Third person is not one of them
// -- it belongs to the world (core.view), and a crossing back into a world that
// said nothing is the eye again. A line a later build does not know is skipped.
float g_volume = 1.0f;   // the whole game's, 0..1 (snd_set_volume)
int   g_shadow_size = 2048;   // the sun's shadow map, texels a side; 0 is no shadows
bool  g_classic_light = false;   // the light as it was before the rework (WorldRenderer::set_classic)
bool  g_fullscreen    = false;   // borderless over the monitor; F11 or the options
bool  g_update_check  = true;    // ask GitHub for a newer release on the title screen
bool  g_no_update_flag = false;  // --no-update-check: not this run, whatever the options say

std::string options_path()
{
#ifdef _WIN32
    if (const char *app = std::getenv("APPDATA"); app != nullptr && app[0] != '\0')
        return std::string(app) + "/cubes-and-portals/options";
#endif
    if (const char *xdg = std::getenv("XDG_CONFIG_HOME"); xdg != nullptr && xdg[0] != '\0')
        return std::string(xdg) + "/cubes-and-portals/options";
    if (const char *home = std::getenv("HOME"); home != nullptr && home[0] != '\0')
        return std::string(home) + "/.config/cubes-and-portals/options";
    return "";
}

void load_options(bool &debug)
{
    std::ifstream in(options_path());
    std::string   key, language = lang::from_environment();
    float         value = 0.0f;
    // The language is the one option that is a word, not a number.
    while (in >> key && (key == "lang" ? bool(in >> language) : bool(in >> value))) {
        if (key == "lang") continue;
        if (key == "look_scale") g_look_scale = std::clamp(value, 0.2f, 3.0f);
        else if (key == "volume") g_volume = std::clamp(value, 0.0f, 1.0f);
        else if (key == "debug") debug = value != 0.0f;
        else if (key == "classic_light") g_classic_light = value != 0.0f;
        else if (key == "fullscreen") g_fullscreen = value != 0.0f;
        else if (key == "update_check") g_update_check = value != 0.0f;
        // Under 2^24, so a float holds it whole.
        else if (key == "livery" && value >= 0.0f && value < (float)net::kLiveryMax)
            net::g_client_livery = (uint32_t)value;
        else if (key == "shadows") g_shadow_size = value >= 2048.0f ? 2048 : value >= 1024.0f ? 1024 : 0;
    }
    snd_set_volume(g_volume);
    lang::load(g_lang_dir, language);
}

void save_options(bool debug)
{
    const std::string path = options_path();
    if (path.empty()) return;
    std::error_code ec;
    std::filesystem::create_directories(std::filesystem::path(path).parent_path(), ec);
    std::ofstream out(path);
    out << "look_scale " << g_look_scale << "\nvolume " << g_volume << "\ndebug " << (debug ? 1 : 0)
        << "\nshadows " << g_shadow_size << "\nclassic_light " << (g_classic_light ? 1 : 0)
        << "\nfullscreen " << (g_fullscreen ? 1 : 0) << "\nupdate_check " << (g_update_check ? 1 : 0)
        << "\nlivery " << net::g_client_livery
        << "\nlang " << lang::current() << "\n";
    if (!out) std::fprintf(stderr, "could not save the options to %s\n", path.c_str());
}

// And back the other way, for openings that arrive from somewhere else.
//
// The pair above is not symmetric in what it costs to get wrong: this direction
// is a straight read of the basis the Portal already computes, while the other
// has to search for the quarter turn. Both exist because an opening has two
// representations and the day they disagree is the day the portal is drawn
// somewhere the player cannot walk through.
PortalSurface surface_from_portal(const Portal &p)
{
    PortalSurface surf;
    surf.center = p.center;
    surf.normal = p.normal();
    surf.up     = p.up();
    surf.half_w = p.half_w;
    surf.half_h = p.half_h;
    return surf;
}

// A landmark next to a portal, so "is this the source or the destination?" can
// be answered by looking rather than by reading coordinates. Two of them, built
// differently, is what makes continuity visible at all.
//
// Both of these are written from a mapgen stage, and so into the column being
// generated and nowhere else: a scene straddling two columns comes out whole
// once both are made, and generating a column far away later never writes the
// scene back over what a player has since done to it.
void build_landmark(const MapgenContext &ctx, int x, int y, int z, BlockId id, int height)
{
    for (int h = 0; h < height; ++h) {
        const int w = (h < height / 2) ? 1 : 0;
        for (int dz = -w; dz <= w; ++dz)
            for (int dx = -w; dx <= w; ++dx)
                ctx.set_block(x + dx, y + h, z + dz, id);
    }
}

// ------------------------------------------------------------------ a session
//
// Everything that belongs to ONE world, in one place, because DESIGN.md 4.1
// says the whole difficulty of the fork's dual-client came from state that
// existed exactly once: the sky, the view range, who draws their own avatar,
// the content registry. Every invariant in that section is a scar from a
// singleton.
//
// The rule this is built to make cheap: no function takes a world implicitly.
// A session is always a parameter, so a swap is `active = i` plus a handful of
// explicit transitions rather than surgery.
struct Session {
    NodeRegistry registry;      // the block table of THIS world, never shared
    ItemRegistry items;         // and the item table, for the same reason

    // And how a body moves here (TODO.md OPEN-44). A world's mods may declare
    // it, and a connected world's welcome says it -- over whatever a mod of
    // this process said, because the server is the one that checks it. Per
    // session like the tables above: walking through a portal into a race is
    // walking into the race's speeds.
    MovementRules movement;
    std::string   movement_by;   // which mod said so, to name it if another replaces it
    float         fly_held = -1.0f;  // core.player_fly: the flying speed kept aside, -1 when it may fly

    // The panels the mods of THIS world declared. Same argument as the two
    // registries above: a world's mods are its own, and a panel belonging to a
    // world nobody is standing in has no business on the screen.
    ui::PanelSet panels;

    // The containers of THIS world. Same argument as the registries and the
    // panels above: a chest belongs to the world it is in, and a world change
    // must not carry one across. Each square's role is the block's at its cell.
    // A carried one has no cell: its z names the block whose roles it takes.
    ContainerSet containers{[this](const ContainerAt &a) {
        if (a.y <= kCarriedBelow) return &registry.get((BlockId)(a.z < 0 ? 0 : a.z)).slot_rules;
        return &registry.get(world.get_block(a.x, a.y, a.z)).slot_rules;
    }};

    // And what its mods say turns some things into another thing. Same argument
    // again: a recipe is its world's, and carrying one across a portal would be
    // crafting with the far world's table.
    RecipeSet    recipes;
    World        world;
    WorldRenderer world_render; // the chunk meshes and the atlas built from `registry`
    ModHost       mods;

    // The world-generation stages this world's mods registered, and the
    // context they run against. Same shape and same rules as the server's --
    // see src/server/main.cpp, where the same three fields carry the same
    // comments -- because a mod that ran in one and not in the other would be
    // a mod that works until somebody opens a socket.
    //
    // The biomes are a COPY and not a pointer into the Mapgen that generated
    // this world: that object is a local of whatever built the scene and is
    // gone by the time a mod loads, and a context pointing into it would answer
    // core.terrain_height out of freed memory.
    struct LuaStage {
        std::string name;
        std::string owner;
        script_vm  *vm  = nullptr;
        int         ref = 0;
    };
    std::vector<LuaStage>          mapgen_stages;
    BiomeSet                       mapgen_biomes;
    std::unique_ptr<MapgenContext> mapgen_ctx;
    bool                           mapgen_finished = false;
    bool                           mapgen_generating = false;   // a stage runs over mapgen_ctx's column
    // core.watch_cell: cells a mod asked to hear about, and the ones written.
    std::set<std::tuple<int, int, int>> watch_cells;
    std::vector<std::tuple<int, int, int>> watch_hits;

    // A world of this process's own with no edge (TODO.md OPEN-35, OPEN-38):
    // the stages that make a column, kept for the life of the session, and what
    // brings columns in and lets them go. Null for a world a server streams,
    // and for the two scenes --dual builds, which stay boxes.
    std::unique_ptr<Mapgen>        mapgen;
    std::unique_ptr<ColumnKeeper>  keeper;
    uint64_t                       column_clock   = 0;   // milliseconds
    uint64_t                       next_unload_at = 0;

    // Things that happen TO cells on their own (core/world/modifiers.h). Per
    // SESSION, like everything else here: two worlds in one process have their
    // own cells, so they have their own sweeps over them.
    Modifiers                      modifiers;
    double                         modifier_clock = 0.0;

    // The light a mod's writes owe (TODO OPEN-29). This loop never relit them
    // at all -- host_set_block wrote the cell and nothing else -- so a mod's
    // lamp was dark here exactly as it was dark on every client before
    // CLOSED-17. Per write it cannot be: a pass around one cell is about 3 ms
    // and mods/beacons writes one every frame. So: batched, and flushed at most
    // every kLightFlushSeconds, which is what makes it affordable and is also
    // the whole of the compromise -- a mod's light lands within a quarter of a
    // second of the write rather than in the same frame.
    LightBatch                     light_batch;
    double                         light_flush_due = 0.0;

    // And what its cells remember. Its world's too, for the same reason: a
    // pedestal on the far side of an opening is that world's pedestal.
    NodeMetaSet   node_meta;
    std::string   meta_scratch;   // where a meta_get's answer lives until the next call

    // What this world's mods keep (core.storage_set). Nothing is saved here, so
    // it lasts as long as the process, like node_meta above.
    ModStore      mod_store;
    std::string   storage_scratch;

    std::vector<Body> bodies;

    // The creatures drawn last frame, by id, and the ones that have gone since
    // -- killed, or out of reach -- fading out where they were last seen
    // (fade_the_gone).
    std::unordered_map<uint64_t, Body> creatures_seen;
    struct Fading {
        Body   body;
        double since = 0.0;
    };
    std::vector<Fading> fading;

    // The marks karts' tyres left sliding (kart_marks): a ring of strips on
    // the road, each fading over kMarkLife, and where each body's rear wheels
    // last left one while it is still sliding.
    struct Mark {
        Vec3  a, b, side;   // from a to b, side the half width across
        float born = 0.0f;  // plat_time() when it was left
    };
    std::vector<Mark> marks;
    size_t            mark_next  = 0;
    bool              marks_dirty = true;
    std::unordered_map<uint64_t, std::array<Vec3, 2>> mark_last;
    std::vector<float> mark_corners;   // the marks as triangles (draw_kart_fx)

    // How each body wearing a model has been moving, for the wheels it turns
    // (animate_body). Keyed by the player's id, the player's own body by
    // kOwnBody. Nothing forgets a player who left: a few floats each.
    struct BodyMotion {
        Vec3   pos;
        float  yaw       = 0.0f;
        double at        = -1.0;
        float  steer     = 0.0f;
        float  travelled = 0.0f;
        float  speed     = 0.0f;   // across the ground, m/s
        float  climb     = 0.0f;   // up, m/s
        float  walked    = 0.0f;   // metres across the ground, any direction
        Vec3   velocity;           // m/s, eased: what somebody else's flight leans by

        // Somebody else's body glides from where it was drawn to each new
        // position the server sends, over the time between two of them
        // (glide), instead of jumping twenty times a second.
        Vec3   glide_from, glide_to, shown;
        float  yaw_from = 0.0f, yaw_to = 0.0f, shown_yaw = 0.0f;
        double glide_at = -1.0, glide_seen = 0.0, glide_used = 0.0;
        float  glide_gap = 0.05f;

        struct Playing {
            std::string name, was;
            double      start = 0.0, was_start = 0.0, since = 0.0;
        } body;
        // And an arms-only clip over it (arms_only_clip): its name and start.
        std::string arms_name;
        double      arms_start = 0.0;
        bool   driving  = false;
        double drove_at = 0.0;
        // Flying, how fast it goes forwards and to its right, m/s, eased
        // (fly_lean).
        float  lean_ahead = 0.0f, lean_right = 0.0f;
        double lean_at    = 0.0;
    };
    std::unordered_map<uint64_t, BodyMotion> motion;

    // What this machine's own body wears, when core.body_model said so here
    // (a kart while it drives); 0 is what the world dresses players in.
    ModelId own_body_model = kNoModel;
    // And what the world dresses players in, off its welcome (0 in a local
    // world, which dresses nobody): what first-person arms are looked up by.
    uint16_t player_model_id = 0;

    // How far behind the player's head this world is seen from, in nodes; 0 is
    // the eye. Per world, because a world asks for it (core.view) and a
    // crossing back into one that said nothing is the eye again. V switches it.
    float view_distance = 0.0f;
    // The camera from above (core.iso_view): how far from the head, and from
    // which way, in radians. 0 is off.
    float iso_distance = 0.0f;
    float iso_yaw      = 0.0f;
    float iso_pitch    = 0.0f;
    float iso_fov      = 0.0f;   // radians; 0 is the engine's
    // Going to it and back is a move of the camera, not a cut: iso_on is where
    // it is going, iso_t how far it has got (0 the eye, 1 above), over
    // iso_secs; iso_from the eye's look when it set off.
    bool  iso_on    = false;
    float iso_t     = 0.0f;
    float iso_secs  = 0.0f;
    Vec3  iso_from{0.0f, 0.0f, -1.0f};
    bool  iso_drawn = false;   // the body is drawn for it (view_distance)
    // Held on a point instead of the head (core.fixed_view): the body stands
    // still while it is there.
    bool  iso_fixed = false;
    Vec3  iso_target{};
    // A walk this world's code steers (core.walk): which way across the world,
    // and how much of the walk. Off gives the body back to the keys. Per world,
    // so a walk is not carried through a portal into a world that never asked.
    bool  mod_walk       = false;
    float mod_walk_x     = 0.0f;
    float mod_walk_z     = 0.0f;
    float mod_walk_speed = 1.0f;
    // The engine's hotbar and health under a world's own HUD (core.engine_hud).
    bool engine_hud = true;

    // What this world's mods declared a model to be: the names, and the file
    // each one is in on this machine. The same table the server publishes in
    // its welcome -- this client keeps it because it loads the same mods when
    // the world is its own.
    //
    // It is what a mod's `core.register_model` reaches, so a mod behaves the
    // same in this process as it does on a server.
    ModelRegistry model_defs;
    ModelId       declared_player_model = kNoModel;
    ModelId       declared_mob_model    = kNoModel;

    // The kinds of creature this world's mods registered (core.register_mob),
    // when the world is this process's own: the server's table, kept here.
    std::vector<ecs::MobKind> mob_kinds;

    // And the models those bodies wear, in the ids THIS world gives them
    // (render/model_store.h). Per world and not per process, because a model id
    // is a position in one world's table: the same 3 is a different character
    // through an opening, and one store for both would dress a stranger's mob
    // in this world's clothes.
    ModelStore models;

    // The two openings this world holds, in both the forms they are needed in.
    PortalLink         link_ab, link_ba;
    Portal             portal_a, portal_b;
    PortalTriggerState trigger_a, trigger_b;

    // A server's world with more than a pair (net::kMaxOpenings): the links
    // past link_ab and link_ba, which have no target of their own and borrow
    // one while they are among the nearest in sight (shown_links), and every
    // opening that leads somewhere, for the floor shift. Empty in every other
    // world, whose openings are the two above.
    std::vector<PortalLink> more_links;
    std::vector<Portal>     linked_openings;
    std::vector<Portal>     linked_to;   // the other end of each, index for index

    // And a world whose first openings are mouths onto other worlds may hold
    // pairs of its own besides them (a mod's core.portal_open): drawn with this
    // world inside, crossed by the server. Empty in every other world.
    std::vector<PortalLink> local_links;

    // The openings here that lead nowhere yet, plugged with their colour's
    // membrane (PortalRenderer::draw_closed). Not a mouth onto another world.
    std::vector<Portal> closed_openings;

    // Openings just linked, and when: their membrane opens onto the image
    // for kOpeningSeconds. One that arrives already linked opens the same
    // way, in its own colour, except in the first Portals a world sends.
    struct Opening {
        Portal                                p;
        std::chrono::steady_clock::time_point t0;
    };
    std::vector<Opening> opening;
    bool                 portals_heard = false;

    // Which mouths onto other worlds have an opening here (bit m), and which
    // were drawn last frame: one not drawn -- its world not up, or not among
    // the nearest kMouthsShown -- is plugged in green, and opens when it is.
    uint32_t mouths_here  = 0;
    uint32_t mouths_drawn = 0;

    // Mouths this world has shut to this player (core.portal_seal), bit m, as
    // its connection last said; and as they were drawn last frame, so one
    // unsealed parts in red. A sealed mouth is a red membrane, not walked into.
    uint32_t sealed       = 0;
    uint32_t sealed_drawn = 0;

    // Off only for the gate that measures what cutting buys, and on a network
    // client, where there is nothing for a render target to show.
    bool portals = true;

    // The sky, per world and not per process. A constant would be the same
    // singleton the fork paid for: two worlds have two skies, and the one a
    // player sees after a swap has to come from the world they arrived in.
    //
    // Derived from `time_of_day` every frame now rather than set once. The
    // value here is what a world clears to before anybody has told it the time.
    float sky[4] = {kSkyColour[0], kSkyColour[1], kSkyColour[2], kSkyColour[3]};

    // And the clock it keeps, for the same reason and with the same shape: two
    // worlds have two clocks, so one of them can be at dusk while the other is
    // at noon and a player crossing between them sees that. On a network client
    // this MIRRORS the server's -- see ClientSession::time_of_day -- and is not
    // advanced here; in the single-process loops this world is the only world
    // there is, so it runs its own.
    double time_of_day = g_start_time;
    double day_length  = g_day_length;
    double wind        = 0.0;   // nodes the clouds have drifted: the server's, or run here

    // A sky of one colour and no sun (core.world_sky): its own mods' when the
    // world is this process's, its server's Welcome when it is not.
    bool  sky_fixed = false;
    float sky_rgb[3]{};
    // A storm (core.world_storm): the sky of the hour turned this colour, the
    // clouds too, whirling.
    bool  storm = false;
    float storm_rgb[3]{};
    float funnel[16]{};   // core.storm_funnel: four of x, y, z, on

    // Whether the engine makes this world's ground (core.mapgen_core).
    bool core_mapgen = true;

    // How far this world is drawn, in nodes. DESIGN.md 4.2 budgets a passive
    // session at about a third: it is rendered into a portal-sized target and
    // nobody is standing in it.
    float view_range = 512.0f;

    // Whether the player's own avatar is drawn in this world. Exactly one
    // session may say yes, and a swap has to move that yes.
    bool draws_own_avatar = false;

    // How many of the two links this world actually has. Two is the spike pair;
    // a world that is one end of a pair of WORLDS has one.
    int link_count = 2;

    // WHICH session this one's openings lead into, or -1 for back into this
    // world.
    //
    // This was a bool called links_to_other, and the bool was the same "two"
    // that the array below used to be: it could only ever mean "the one that is
    // not this one". An index says which, so a world may lead into the third
    // one while the third leads into a fourth.
    //
    // -1 by default, and that is deliberate rather than lazy: every gate this
    // engine has links a world to itself, and a default the other way would
    // rewrite what all of them measure in one commit.
    int link_session = -1;

    // And the world link_ba leads into, when this world's SECOND opening is a
    // mouth onto yet another world -- a sandbox with a door to an arena and one
    // to a track. -1 otherwise, and then link_session speaks for both links:
    // link_ba is the local pair's other half, or nothing.
    int link_session_b = -1;

    // Mouths past the second (net::kMaxMouths): the opening, the link drawn
    // in it (its target is Scene::mouth_rt), the trigger, and the world it
    // shows, -1 until linked. Unused in every world with two mouths or fewer.
    struct MoreMouth {
        Portal             portal;
        PortalLink         link;
        PortalTriggerState trigger;
        int                session = -1;
    };
    MoreMouth more_mouths[net::kMaxMouths - 2];

    bool links_elsewhere() const { return link_session >= 0; }
    bool two_mouths() const { return link_session_b >= 0; }
    // A second mouth linked, or any past it: each is drawn with its own world.
    bool many_mouths() const
    {
        if (two_mouths()) return true;
        for (const MoreMouth &m : more_mouths)
            if (m.session >= 0) return true;
        return false;
    }
};

// Mouth m of a world, whichever of its forms: the first two are the pair every
// scene has (portal_a, link_ab ...), the rest Session::more_mouths.
Portal &mouth_portal(Session &w, int m)
{
    return m == 0 ? w.portal_a : m == 1 ? w.portal_b : w.more_mouths[m - 2].portal;
}
PortalTriggerState &mouth_trigger(Session &w, int m)
{
    return m == 0 ? w.trigger_a : m == 1 ? w.trigger_b : w.more_mouths[m - 2].trigger;
}
PortalLink &mouth_link(Session &w, int m)
{
    return m == 0 ? w.link_ab : m == 1 ? w.link_ba : w.more_mouths[m - 2].link;
}
int mouth_world(const Session &w, int m)
{
    return m == 0 ? w.link_session : m == 1 ? w.link_session_b : w.more_mouths[m - 2].session;
}

// How many of a world's mouths are looked into: the nearest. A sandbox has a
// door to every themed world round its plaza, and each one shown is a window
// kept open and a whole world drawn again every frame.
inline constexpr int kMouthsShown = 2;

// Whether mouth m is one of the kMouthsShown nearest `eye`, among those `at`
// gives a centre for (false: not a mouth).
template <class At> bool mouth_near(int m, Vec3 eye, At at)
{
    Vec3 c;
    if (!at(m, c)) return false;
    const float d      = length_sq(c - eye);
    int         closer = 0;
    for (int k = 0; k < net::kMaxMouths; ++k) {
        Vec3 o;
        if (k != m && at(k, o) && length_sq(o - eye) < d) ++closer;
    }
    return closer < kMouthsShown;
}

// Among the mouths of the world being stood in that are linked.
bool mouth_shown(Session &w, int m, Vec3 eye)
{
    return mouth_near(m, eye, [&](int k, Vec3 &c) {
        if (mouth_world(w, k) < 0 || (w.sealed >> k & 1u)) return false;
        c = mouth_portal(w, k).center;
        return true;
    });
}

// The sounds this build has.
//
// A table and not a registry, and the difference is declared rather than
// implied: a mod cannot add a sound yet. An empty registry with an add() nobody
// calls would look like it could.
//
// Read from assets/sounds (made with ElevenLabs, trimmed to mono .ogg). The
// three the game had before the files keep their tone.h bake as a fallback, so
// a build without the folder still breaks and places with a sound; the rest are
// silent without their file.
struct SoundBank {
    // Up to four takes of one sound (name, name_2 ...), one picked at random
    // each play so a hundred blocks broken do not sound like one.
    struct Takes {
        snd_sound take[4]{};
        int       n = 0;
        snd_sound any() const { return n == 0 ? snd_sound{0} : take[n == 1 ? 0 : std::rand() % n]; }
    };
    Takes     broke, placed, hurt;
    snd_sound step{0};         // soil, and any block that does not say
    snd_sound step_stone{0};
    snd_sound step_wood{0};
    snd_sound splash{0};
    snd_sound portal{0};
    snd_sound chest{0};
    snd_sound jetpack{0};

    void load()
    {
        auto bake = [](const char *name, float hz, float hz_end, float seconds, bool noise) {
            SoundDef d;
            d.name    = name;
            d.hz      = hz;
            d.hz_end  = hz_end;
            d.seconds = seconds;
            d.noise   = noise;
            const std::vector<int16_t> pcm = bake_sound(d);
            return snd_upload(pcm.data(), (int)pcm.size(), kSampleRate);
        };
        auto file = [](const std::string &name) {
            return snd_load((g_sounds_dir + "/" + name + ".ogg").c_str());
        };
        auto takes = [&](const char *name, auto fallback) {
            Takes t;
            for (int i = 0; i < 4; ++i) {
                const std::string n = i == 0 ? name : std::string(name) + "_" + std::to_string(i + 1);
                if (const snd_sound got = file(n.c_str()); got.id != 0) t.take[t.n++] = got;
            }
            if (t.n == 0) t.take[t.n++] = fallback();
            return t;
        };

        broke  = takes("break", [&] { return bake("core:break", 0.0f, 0.0f, 0.12f, true); });
        placed = takes("place", [&] { return bake("core:place", 180.0f, 90.0f, 0.09f, false); });
        hurt   = takes("hurt", [&] { return bake("core:hurt", 300.0f, 120.0f, 0.22f, false); });
        step       = file("step");
        step_stone = file("step_stone");
        step_wood  = file("step_wood");
        splash  = file("splash");
        portal  = file("portal");
        chest   = file("chest");
        jetpack = file("jetpack");
    }

    // A mod's sound by name (core.play_sound): mods/<mod>/<thing>.ogg or .wav,
    // then assets/sounds/<mod>_<thing>.ogg. Looked for once; a name with no
    // file is remembered as silent rather than looked for on every play.
    std::string                                mods_dir;
    std::unordered_map<std::string, snd_sound> by_name;

    //
    // A world that sent its sounds (sound_media_name) is heard with them first:
    // its creatures' voices are files beside ITS mods, not this machine's.
    snd_sound named(const std::string &name, const MediaSource &media = nullptr)
    {
        if (!net::sound_name_ok(name)) return snd_sound{0};
        const auto it = by_name.find(name);
        if (it != by_name.end()) return it->second;

        const size_t      at    = name.find(':');
        const std::string mod   = name.substr(0, at);
        const std::string thing = name.substr(at + 1);
        snd_sound got{0};
        if (media)
            for (const char *ext : {".ogg", ".wav"}) {
                std::vector<uint8_t> bytes;
                if (got.id == 0 && media(sound_media_name(mod, thing + ext), bytes))
                    got = snd_load_memory(ext, bytes.data(), (int)bytes.size());
            }
        for (const std::string &path :
             {mods_dir + "/" + mod + "/" + thing + ".ogg", mods_dir + "/" + mod + "/" + thing + ".wav",
              g_sounds_dir + "/" + mod + "_" + thing + ".ogg"})
            if (got.id == 0) got = snd_load(path.c_str());
        if (got.id == 0) std::printf("audio: no file for sound '%s'\n", name.c_str());
        return by_name[name] = got;
    }
};

// A block held in a hand, built once per block and model (draw_held_block).
struct HeldCube {
    ModelRenderer render;
    uint64_t      built = 0;
};

struct Scene {
    // Every world this process holds.
    //
    // A list, and not an array of two. The two came from the shape of the
    // spike -- one world here, one behind the portal -- and there is nothing
    // about a portal that stops at two: a portal in the world behind a portal
    // is a third server, and a hub with four doorways is five. Neither is
    // exotic and neither should need this line changed.
    //
    // unique_ptr because these hold their addresses: a ClientSession is handed
    // pointers to a session's world and registry, and a list that moved its
    // contents when it grew would hand a live connection a dangling world.
    //
    // Slot 0 is not "the active one": `active` says which is. Keeping the index
    // rather than swapping the contents is what makes a swap cheap and, more to
    // the point, what stops half a session being promoted.
    std::vector<std::unique_ptr<Session>> sessions;
    int                                   active = 0;

    // Where a stack a mod drops lands, and the clock it ages against.
    //
    // A pointer because the single-process loop owns the ECS world its drops
    // live in and this struct outlives none of it -- and null everywhere else,
    // which is the honest answer: a gate with no drop world cannot drop
    // anything, and a host function that made one of its own would be dropping
    // stacks into a world nobody draws.
    flecs::world *drops      = nullptr;
    double        drop_clock = 0.0;
    uint64_t      drops_me   = 0;   // the player's own entity in it: an arrow's archer

    // Entities a mod running in THIS process owns, and the systems that step
    // them. Owned here and not borrowed, unlike `drops`: a client's entities
    // come off the wire as a list of bodies to draw, so there is no other flecs
    // world here for a mod's things to live in. On a server they share the one
    // the sessions own, which is the world the mobs and the drops are in.
    flecs::world  mod_ecs;
    ecs::LuaEcs   lua_ecs;

    // The connection this process's mods talk to a server through, when there
    // is one. A pointer, and null everywhere else, for the same reason `drops`
    // is: the networked loop owns it and this struct outlives none of it -- and
    // a mod in a single-process gate has nobody to send anything to, which is
    // the honest answer rather than a queue nothing drains.
    net::ClientSession *conn = nullptr;

    // The passes this process's mods draw in, and the camera they are drawn
    // against. The matrices are here rather than passed to the host function
    // because a host function takes what a mod gives it and nothing else --
    // there is no argument in core.render.box for the camera, and there should
    // not be: the pass runs inside a frame the engine is drawing.
    ModPasses   passes;
    Mat4        pass_projection = mat4_identity();
    Mat4        pass_view       = mat4_identity();
    bool        in_pass         = false;

    // What a world's code draws over the screen, once a frame (TODO.md
    // OPEN-49), and whether one of those is running now.
    ModPasses   hud_passes;
    bool        in_hud_pass     = false;
    // core.hud.image's pictures, decoded once, by the connection and file they
    // came from: two worlds may each have a file of one name. 0 is a file that
    // is not there or not a picture, asked once.
    std::map<std::pair<const void *, std::string>, gfx_texture> hud_images;

    // Which keys were down at the last claim_buttons, for their releases.
    bool keys_down[PLAT_KEY_COUNT] = {};
    // A 1..9 pressed this frame that a world's code claimed (a machine's seat
    // in the front): it does not also change the hand.
    bool digit_claimed = false;

    // A left press a world's code claimed with core:input. Held until the
    // button comes up, so a claimed click does not turn into a dig on the
    // frames after it.
    bool        left_claimed    = false;

    // The last thing the world said to this player. Here rather than in either
    // loop, because both of them produce one and both of them draw it -- and
    // because what it holds is a sentence from the server, which is the one
    // copy of it that exists.
    ui::Notice    notice;

    // The chat (T), and the /commands this process's mods declared.
    ui::Chat          chat;
    net::ChatCommands chat_commands;

    // The pages this process's mods offer (core.page_register), name to owner
    // and title, and the one on the screen: cnp_server's pages, for the one
    // player of a world run here, without the administration it has no use for.
    struct PageUp {
        std::string    name, owner, title, markup;   // owner "core" for the engine's
        int            screen = 0;
        ui::PageLayout layout;
    };
    std::map<std::string, std::pair<std::string, std::string>> pages;
    std::optional<PageUp> page_up;

    // The grid the single-process player crafts in, when there is one. A
    // pointer because the loop owns it and this struct outlives none of it --
    // null everywhere else, which is the honest answer for a gate with no loop.
    CraftGrid    *craft = nullptr;

    // And the bag that grid empties into, with where its owner is standing so
    // that what will not fit has somewhere to land. Both null outside the
    // single-process loop, which is the honest answer: a gate with no loop has
    // no player and no bag.
    Inventory    *bag   = nullptr;
    float         player_x = 0.0f, player_y = 0.0f, player_z = 0.0f;

    // How much of the single-process player is left, the fall they are in the
    // middle of, and the clock they heal on. Pointers for the reason the bag is
    // one: the loop owns them and this struct outlives none of it.
    //
    // `died` is the same thing Session::death_owed is on the server: whatever
    // killed them says so, and the loop -- the only thing here that owns a
    // position -- is what puts the body back.
    ecs::Health  *health = nullptr;
    ecs::Regen   *regen  = nullptr;
    FallWatch    *fall   = nullptr;
    bool          died   = false;
    bool          has_bed = false;   // core.player_spawn
    bool          no_regen = false;  // core.player_regen
    Vec3          bed{};

    // Where the one player is, for core.players and core.player_pos. A pointer
    // for the reason `health` is one: the loop owns the body, and a mod asking
    // where somebody is has to be told what the loop is actually working from
    // rather than a copy taken when the mod loaded. Not const: core.player_place
    // moves it.
    Player *player = nullptr;

    // The clock the notice ages against. A field because the two loops keep
    // different clocks -- one counts its own seconds, the other asks the
    // platform -- and the drawing must not care which.
    double        notice_clock = 0.0;

    // What the bar along the bottom shows this frame.
    //
    // Filled by whoever owns the bag -- the single-process loop from its own
    // Inventory, the network loop from what the server last said -- rather than
    // read out of a session. A HUD that kept its own inventory would be a
    // second source of truth for something DESIGN.md 3.4 puts on the server,
    // and the first frame the two disagreed would be a player who thinks they
    // are holding something they are not.
    //
    // `items` null means there is no bar to draw, which is every gate that
    // renders a portal and never touches a block.
    struct HotbarShown {
        std::vector<ItemStack> slots;
        int                    selected = 0;
        const ItemRegistry    *items    = nullptr;

        // The whole bag, open over the world.
        bool open = false;

        // Whether a mod's panel is what took the cursor. Remembered, because
        // giving it back has to know it was TAKEN: a panel that vanishes when
        // its mod closes it leaves the player looking at the world with a free
        // mouse and no way to turn, and re-grabbing unconditionally would snatch
        // the cursor off an open bag.
        bool panel_took_cursor = false;

        // Where the button went down, and the whole of what a drag remembers.
        // -1 between gestures. It is a SLOT and not a point: the panel can move
        // under a drag (a resize, a world change) and a gesture that remembered
        // pixels would finish somewhere else.
        //
        // There is no "chosen slot" beside it and there is no stack held in the
        // cursor. Both would be state this side owned and the server did not --
        // see net::MoveItem -- and the first is also a mode a player cannot see
        // once they have looked away from the panel. A drag keeps its state in
        // the mouse button, where a hand already knows where it is.
        int      press      = -1;
        ui::Grid press_grid = ui::Grid::Bag;   // which grid that press landed in

        // And which BUTTON is holding the drag up, because the two carry
        // different amounts: the left one carries the stack, the right one
        // carries half of it. A flag and not a second `press`, so that "a drag
        // is running" stays one question with one answer.
        bool     press_right = false;

        // Where the panel put its squares, so whoever handles the click can hit
        // test against what was actually DRAWN rather than against a second
        // copy of the same arithmetic.
        ui::PanelLayout panel;

        // The container this player has open, and where its grid went. Empty
        // when there is none -- a chest is a place and this is only the picture
        // of one, filled the same way `slots` is: by whoever owns the truth.
        std::vector<ItemStack> chest;
        std::vector<uint8_t>   chest_roles;   // what each square is for (SlotRole)
        float                  chest_progress = -1.0f;   // what is going on in it, 0..1, -1 none
        float                  chest_rate     = 0.0f;    // per second, from chest_since
        float                  chest_scroll = 0.0f, chest_scroll_size = 0.0f;   // its scrollbar
        bool                   chest_search = false;   // and a search box, holding chest_query
        std::string            chest_query;
        double                 chest_since    = 0.0;     // when that was said (the frame clock)
        bool                   chest_open = false;
        ContainerAt            chest_at;
        ui::PanelLayout        chest_panel;

        // The grid this player crafts in, and what it makes. Two by two is the
        // one in the corner of their own bag and is always there; three by
        // three is what standing at a table makes it, which is why the SIZE is
        // the only thing that says a table is open -- see net::CraftState.
        //
        // A picture, like `slots` and `chest`: the grid itself is the server's,
        // and a client that kept its own would be a second answer to what a
        // player is holding halfway through arranging it.
        std::vector<ItemStack> craft;
        int                    craft_w = 2, craft_h = 2;
        ItemStack              craft_result;
        ui::CraftLayout        craft_panel;

        // Both grids, as one question. Written here rather than at each call
        // site because there are three, and three copies of "which grid is that
        // point in" is three chances for the answer to differ from the drawing.
        ui::BagPoint point_at(float px, float py) const
        {
            ui::BagPoint p;
            if (chest_open) {
                const int c = chest_panel.slot_at(px, py);
                if (c >= 0) {
                    p.slot = c;
                    p.grid = ui::Grid::Chest;
                    return p;
                }
            }

            // The craft grid and its result, when one is drawn. Before the bag
            // for the same reason the chest is: whatever is on top is what a
            // click belongs to.
            if (!chest_open) {
                const int c = craft_panel.grid.slot_at(px, py);
                if (c >= 0) {
                    p.slot = c;
                    p.grid = ui::Grid::Craft;
                    return p;
                }
                if (craft_panel.result.slot_at(px, py) == 0) {
                    p.slot = 0;
                    p.grid = ui::Grid::Result;
                    return p;
                }
            }

            p.slot = panel.slot_at(px, py);
            return p;
        }

        // The picture of one grid, by name. One place, because "which vector is
        // that" is asked by the hit test, by the drag ghost and by the sender,
        // and three copies of it is three chances to draw one grid and move
        // something out of another.
        const std::vector<ItemStack> &grid_of(ui::Grid g) const
        {
            static const std::vector<ItemStack> nowhere;
            switch (g) {
            case ui::Grid::Chest: return chest;
            case ui::Grid::Craft: return craft;
            case ui::Grid::Result: return nowhere;   // it holds one stack, not a grid
            case ui::Grid::Bag: break;
            }
            return slots;
        }

        // How much of this player is left, as whatever is authoritative last
        // said. A maximum of zero is "nobody has said yet", which is not the
        // same as empty -- see ui::draw_health.
        int health = 0, health_max = 0;

        // What is in the square a press landed on, whichever grid it was in.
        bool press_has_item() const { return press_count() > 0; }

        // And how many of it, which is what half of a stack is half OF. One
        // function under both, because "is there something there" and "how much
        // is there" asked separately is two answers that can disagree about an
        // empty square.
        uint16_t press_count() const
        {
            if (press_grid == ui::Grid::Result) return craft_result.count;

            const std::vector<ItemStack> &grid = grid_of(press_grid);
            if (press < 0 || press >= (int)grid.size()) return 0;
            return grid[(size_t)press].empty() ? 0 : grid[(size_t)press].count;
        }
    };
    HotbarShown hotbar;

    // The pause menu: up from the moment the player lets go of the world until
    // they take it back. See pause_frame.
    struct PauseShown {
        bool up      = false;
        bool options = false;   // the second page rather than the first
        bool look    = false;   // or the robot's (look_panel)
    };
    PauseShown pause;

    // Where the sounds are heard from, and which way round.
    //
    // Written once a frame by render_frame, because the ear is the camera and
    // the camera is what that function already knows -- a listener kept
    // anywhere else would be a second answer to "where is the player looking"
    // for something to disagree with.
    Listener  ear;
    SoundBank sounds;

    Scene() { sessions.push_back(std::make_unique<Session>()); }

    int count() const { return (int)sessions.size(); }

    Session       &at(int i)       { return *sessions[(size_t)i]; }
    const Session &at(int i) const { return *sessions[(size_t)i]; }

    Session &act()             { return at(active); }
    const Session &act() const { return at(active); }

    // The session the active one's openings lead into: another world when it
    // has a link, itself when it does not. Was `other()`, which could only mean
    // one thing and only while there were two.
    Session &linked()
    {
        const int t = act().link_session;
        return (t >= 0 && t < count()) ? at(t) : act();
    }
    const Session &linked() const
    {
        const int t = act().link_session;
        return (t >= 0 && t < count()) ? at(t) : act();
    }

    // One more world, empty, and its index. What opening a portal onto a new
    // server costs in this structure.
    int add()
    {
        sessions.push_back(std::make_unique<Session>());
        return count() - 1;
    }

    // Back to `n` empty worlds. For the scenes that build themselves from
    // scratch and must not inherit whatever the last one left.
    void reset(int n)
    {
        sessions.clear();
        for (int i = 0; i < n; ++i) sessions.push_back(std::make_unique<Session>());
        active = 0;
    }

    // ------------------------------------------------- per process, not per world
    //
    // Shaders and vertex buffers that do not depend on which world is being
    // drawn, and the state of the one player.
    PortalRenderer    portal_render;
    SelectionRenderer selection;
    BodyRenderer      body_render;
    FxRenderer        fx_render;   // core.render.fx, made at its first use
    int               fx_ready = 0;   // 0 not tried, 1 made, -1 failed
    // Karts' nitro flames this pass, nozzle and tip (flames_of, draw_kart_fx).
    std::vector<std::pair<Vec3, Vec3>> kart_flames;

    // One character, when --model named one. The trial that came first, kept
    // because `--model-check` drives it directly: a model somebody named on the
    // command line, drawn where the flags put it, with no body behind it.
    ModelRenderer      model_render;
    bool               model_ready = false;
    Vec3               model_pos{0.0f, 0.0f, 0.0f};
    float              model_yaw = 0.0f;
    std::vector<float> model_bones;

    bool cut_bodies = true;
    int  portal_shown = 0;   // the session a portal pass is drawing, for draw_bodies_cb
    RaycastHit target;

    // The dig in progress and how full its bar is. On the Scene and not in the
    // frame loop because BOTH loops in this file use it -- the single-process
    // one, which is its own authority and breaks the block when this says so,
    // and the networked one, which sends its second Edit then. See dig_step.
    DigState dig;
    float    dig_progress = 0.0f;

    // What the player holds, by the item's name ("tools:pick"; empty for a
    // bare hand), set by whichever loop is running, and how the
    // arms seen in first person are playing (draw_arms).
    std::string hand;
    // A loop somebody plays in: the arms are theirs whether or not F3 shows
    // the debug lines. A gate leaves it off and gets a frame without them.
    bool        play_arms  = false;
    BlockId     hand_block = kAir;   // what it puts down, when it is a block
    uint32_t    hand_tag   = 0;      // the held stack's colours (ItemStack::tag)
    uint8_t     piece_turn = 0;      // quarter turns a track piece in hand is given (Z / X)
    float       nitro      = -1.0f;  // the kart's nitro gauge, 0..1; -1 on foot
    bool        working    = false;  // the left button down on something: a dig, a hit
    // What the arms seen in first person are doing with the body, when that
    // is not the hand's business: hanging from a ledge, pulling up, climbing.
    const char *arms_clip  = nullptr;
    // And while the hands hold a ledge, how far the view has turned from
    // facing it -- yaw off the wall, and pitch -- so the arms stay on the
    // ledge and the head turns between them.
    bool  arms_held  = false;
    float arms_yaw   = 0.0f;
    float arms_pitch = 0.0f;
    bool  arms_still = false;   // off the ground: flying, falling, swimming -- no step to bob with
    // Driving in first person: no arms, the body in its seat instead, seen
    // from its own head (add_own_body), and the eye is that head.
    bool  driving_view = false;
    Vec3  rider_eye{};
    // Carried on a mod's machine (core.craft.seat): the machine as drawn this
    // frame, which the body sits on and turns with; and in first person the
    // body drawn without its head, seen from it, the look still the mouse's.
    bool  seat_on = false;
    float seat_yaw = 0.0f, seat_pitch = 0.0f, seat_roll = 0.0f;
    bool  seat_view = false;
    Vec3  seat_eye{};
    // The machine this player sits in, run here too (core.craft.make on a
    // client, which its world's code calls for the one its player drives):
    // drawn, sat on and steered where it is going to be, a round trip before
    // the server says so, and pulled towards what the server says
    // (predict_machine).
    struct Predicted {
        uint64_t               id = 0;
        std::shared_ptr<Craft> sim;
        CraftKeys              keys;
        std::deque<std::pair<double, CraftPose>> history;   // when, and where it was then
        double seen = -1.0;                     // the server's pose last compared
        Vec3   last_base;                       // and where it said, to know its speed
        double last_at = -1.0;
        // How it went, said when it ends: poses compared, how far off they
        // were in all, and how many were too far to ease.
        int    compared = 0, resets = 0;
        double off_sum  = 0.0;
    } predicted;
    // The world whose mouths were drawn last frame: one just arrived in takes
    // the ones it draws as already open (draw_portals).
    const Session *mouths_world = nullptr;
    Session::BodyMotion::Playing arms;
    // A clip world code asked the arms to play once (core.arms_clip), from
    // when, and whether it is still to start over.
    std::string mod_arms;
    double      mod_arms_start = 0.0;
    // And one they hold until told otherwise (core.player_clip): cards in hand.
    std::string own_clip;
    // What world code dresses this body in (core.player_wear): model and tint
    // per slot, seen in this machine's own drawing of it (the server shows
    // everybody else). ponytail: per scene like own_clip, not per world.
    uint16_t own_dress[10]      = {};
    uint16_t own_dress_tint[10] = {};
    // The camera the last frame was drawn from, for core.cursor_ray.
    bool cam_set = false;
    Vec3 cam_eye{}, cam_forward{}, cam_up{};
    // The model drawn in the hand in first person instead of "<item>_held"
    // (core.held_model): a bow as it bends. Empty for the item's own.
    std::string held_model;

    // The block in the hand, as a cube cut out of the atlas: in the arms, and
    // in the robot seen from behind (draw_held_block).
    HeldCube held_arms;
    // Other bodies' blocks, one cube per block and model: a single cube was
    // rebuilt (a shader compile) for every body in view holding another block.
    std::unordered_map<uint64_t, HeldCube> held_bodies;
    std::unordered_map<uint64_t, HeldCube> drop_cubes;   // a dropped block's cube, by block and atlas (draw_drop)

    // How far open each cell's lid is, 0..1, for the cells that are moving or
    // open. TODO OPEN-32: what the world knows is a BIT -- somebody has that
    // cell open or nobody does -- and the swing between the two is the
    // client's own business, because it is a picture and not a fact. So the
    // wire carries one message per lid rather than sixty, and every client
    // draws the same chest at its own frame rate.
    //
    // A cell that is shut and at rest is dropped, so this is the size of the
    // chests somebody is actually using.
    std::unordered_map<Cell, float, CellHash> open_angle;

    int blit_link = 0;

    // The targets the links past a world's first two borrow, made the first
    // time a world has them. Two: with link_ab's and link_ba's that is the
    // four nearest openings in sight drawn each frame, and at the default
    // recursion depth 4 + 16 world passes at worst.
    static constexpr int kMoreShown = 2;
    gfx_rt more_rt[kMoreShown] = {};

    // The targets of the mouths past the second (Session::more_mouths), made
    // when one is first linked.
    gfx_rt mouth_rt[net::kMaxMouths - 2] = {};

    // The local_links drawn this frame, with more_rt lent to them.
    PortalLink local_shown[kMoreShown];
    int        local_shown_count = 0;

    // How many crossings were refused because the arrival was inside a block.
    // A counter and not a flag: a sweep wants to know how many of a hundred
    // approaches were turned away, and a player wants the HUD to say it once.
    int refused_crossings = 0;

    bool last_oblique = false;
    bool last_oblique_b = false;
    bool last_clip_b    = false;
    bool last_clip    = false;

    float roll = 0.0f;

    // Turning the view out of the opening a crossing came out of, and where
    // to: see aim_after_crossing.
    bool   look_settling = false;
    Vec3   look_target;
    double look_since = 0.0;

    // The field on a mod's panel that has the keyboard, and what it holds so
    // far. Here and not on the panel: the panel is what the mod declared, and
    // what a player has half typed is nobody's until Enter. See type_in_field.
    struct Typing {
        bool        on = false;
        std::string panel, id, text;
    } typing;

    Vec3  cam_warp;
    float cam_warp_left = 0.0f;

    // A step up (a slab, a stair) not yet let into the view (StepSmooth).
    StepSmooth step;

    // How long THIS warp was given. Kept per warp rather than as a constant
    // because the time is now the distance at a bounded speed: see
    // cam_warp_time.
    float cam_warp_time = 0.15f;

    // How far the camera's yaw trails the body's, in radians, and the body's yaw
    // the frame before, to tell a turn from a jump (camera_follow).
    float cam_yaw_lag  = 0.0f;

    // Driving, how high the camera stands: the chassis's base (Player::
    // body_base), followed a little behind -- the chassis bumps over a join
    // and a landing, and a view that bumps with it every step reads as jolts.
    float drive_eye_y   = 0.0f;
    bool  drive_eye_set = false;
    float cam_prev_yaw = 0.0f;

    std::string net_status;
};
// ------------------------------------------------------------- script host
// The engine hands the VM a table of function pointers rather than letting it
// see core/ or client/ types, which is what keeps src/script independent of
// both and testable against a stub.

// A write to a cell some mod watches (core.watch_cell), for core:watched: a
// mod's through host_set_block, and this process's own player placing and
// breaking, which write the world directly.
void note_watched(Session &sess, int x, int y, int z)
{
    if (sess.watch_cells.empty() || !sess.watch_cells.count({x, y, z})) return;
    if (std::find(sess.watch_hits.begin(), sess.watch_hits.end(), std::make_tuple(x, y, z)) ==
        sess.watch_hits.end())
        sess.watch_hits.emplace_back(x, y, z);
}

struct Scene;
void loose_around(Scene &s, int x, int y, int z);

void host_set_block(void *ud, int x, int y, int z, int id)
{
    Session &sess = static_cast<Scene *>(ud)->act();

    // A stage writes only into the column it is generating, as on the server.
    if (sess.mapgen_generating && sess.mapgen_ctx && !sess.mapgen_ctx->owns(x, z)) return;

    // And anything else into a lit column, or it waits for one (TODO.md OPEN-35).
    if (sess.keeper && !sess.mapgen_generating) {
        if (!sess.keeper->takes_writes(x, z)) {
            sess.keeper->defer(PendingWrite{x, y, z, (BlockId)id, 0});
            return;
        }
        sess.keeper->wrote(x, z, sess.column_clock);
    }

    const BlockId had = sess.world.get_block(x, y, z);
    sess.world.set_block(x, y, z, (BlockId)id);
    note_watched(sess, x, y, z);

    // Only when the light could have changed at all: the pass reads whether a
    // block is opaque and what it emits, and mods/spike writes the same block
    // into the same cell every tick.
    //
    // And not while a stage is generating, which is the server's rule too: the
    // column is lit whole once it is made. Batched anyway, a column's caves,
    // veins and trees were thousands of cells scattered across it, and the
    // flush that relit them merged them into boxes hundreds of blocks wide --
    // 150 to 230 ms a frame, every few seconds of a flight over new ground.
    if (!sess.mapgen_generating && !light_unchanged(sess.registry, had, (BlockId)id))
        sess.light_batch.add(x, y, z);
    loose_around(*static_cast<Scene *>(ud), x, y, z);
}

int host_watch_cell(void *ud, int x, int y, int z, int on)
{
    Session &sess = static_cast<Scene *>(ud)->act();
    if (on) sess.watch_cells.insert({x, y, z});
    else sess.watch_cells.erase({x, y, z});
    return 1;
}

void host_set_facing(void *ud, int x, int y, int z, int facing)
{
    Scene *s = static_cast<Scene *>(ud);
    if (facing < 0) return;

    // Kept to the column a stage is making, and waiting for a column that is
    // not lit -- the same two rules host_set_block keeps.
    {
        Session &sess = s->act();
        if (sess.mapgen_generating && sess.mapgen_ctx && !sess.mapgen_ctx->owns(x, z)) return;
        if (sess.keeper && !sess.mapgen_generating && !sess.keeper->takes_writes(x, z)) {
            PendingWrite w{x, y, z, kAir, (uint8_t)facing};
            w.facing_only = true;
            sess.keeper->defer(w);
            return;
        }
    }

    // The same rule the networked server applies, from the same function: a
    // byte the block in that cell does not allow is refused and said, not
    // masked into range. One rule told two ways is two games.
    const NodeDef &def = s->act().registry.get(s->act().world.get_block(x, y, z));
    if (!node_turn_allows(def.turn, (unsigned)facing)) {
        const ModManifest *m = mod_host_current();
        std::fprintf(stderr, "mod '%s': core.set_facing %d is not a turn '%s' has\n",
                     m ? m->name.c_str() : "?", facing, def.name.c_str());
        return;
    }

    s->act().world.set_facing(x, y, z, (uint8_t)facing);
}

int host_get_facing(void *ud, int x, int y, int z)
{
    return (int)static_cast<Scene *>(ud)->act().world.get_facing(x, y, z);
}

int host_get_block(void *ud, int x, int y, int z)
{
    return (int)static_cast<Scene *>(ud)->act().world.get_block(x, y, z);
}

// Portal 0 is A, portal 1 is B. Each appears FOUR times, not twice, and every
// one of them has to move together.
//
// Twice in the renderer -- as one link's source and the other's destination --
// and once more as the Portal the body uses, which is a different type holding
// the same opening. That last one was missing, and it is not a subtle bug: the
// spike mod parks portal B at a hardcoded 79.5 every frame, so in a scene that
// puts B anywhere else the opening was DRAWN at 79.50 22.00 60.57 while walking
// into it delivered the player to 60.50 21.50 60.00. Nineteen nodes apart, in
// play, with both halves individually behaving perfectly.
//
// Only the centre moves. The axis, the facing and the quarter turn are the
// opening's shape, not its position, and an API that could change those by
// writing three floats would be an API that silently rebuilds the portal.
// One player, so one id: the same ui::kLocalPlayer every event's `player` field
// carries here. A mod written for a server runs on this side without knowing
// which side it is on, which is the whole point of the field existing at all
// where there is nobody else.
// The one bag this process has. Same rules as the server's, on the same
// Inventory the hotbar draws from -- two implementations of "take one out of a
// bag" are two bags that agree until one of them is edited.
int host_take_item(void *ud, int player, const char *item, int count)
{
    Scene *s = static_cast<Scene *>(ud);
    if (!item || count <= 0 || s->bag == nullptr) return 0;
    if ((ui::PlayerId)player != ui::kLocalPlayer) return 0;

    const ItemId id = s->act().items.find(item);
    if (id == kNoItem) return 0;

    return s->bag->take_by_item(id, count);
}

int host_bag_count(void *ud, int player, const char *item)
{
    Scene *s = static_cast<Scene *>(ud);
    if (!item || s->bag == nullptr || (ui::PlayerId)player != ui::kLocalPlayer) return 0;
    const ItemId id = s->act().items.find(item);
    if (id == kNoItem) return 0;
    int n = 0;
    for (int i = 0; i < s->bag->size(); ++i)
        if (s->bag->at(i).item == id) n += s->bag->at(i).count;
    return n;
}

const char *host_held_item(void *ud, int player)
{
    Scene *s = static_cast<Scene *>(ud);
    static std::string held;   // valid until the next call, like every host string
    held.clear();
    // What the hand shows, which either loop keeps: the local bag has no
    // counterpart on a connection, whose bag is the server's.
    if ((ui::PlayerId)player == ui::kLocalPlayer) held = s->hand;
    return held.c_str();
}

const char *host_worn_item(void *ud, int player, const char *square)
{
    Scene *s = static_cast<Scene *>(ud);
    static std::string worn;
    worn.clear();
    const int w = wear_slot(square);
    if (s->bag != nullptr && w != 0 && (ui::PlayerId)player == ui::kLocalPlayer) {
        const ItemStack &st = s->bag->at(Inventory::kDefaultSlots + w - 1);
        if (!st.empty()) worn = s->act().items.get(st.item).name;
    }
    return worn.c_str();
}

ItemStack *worn_stack(Scene *s, int player, const char *square)
{
    const int w = wear_slot(square);
    if (s->bag == nullptr || w == 0 || (ui::PlayerId)player != ui::kLocalPlayer) return nullptr;
    ItemStack &st = s->bag->slot(Inventory::kDefaultSlots + w - 1);
    return st.empty() ? nullptr : &st;
}

int host_worn_id(void *ud, int player, const char *square)
{
    const ItemStack *st = worn_stack(static_cast<Scene *>(ud), player, square);
    return st ? (int)(st->tag & kTagIdMask) : 0;
}

int host_worn_set_id(void *ud, int player, const char *square, int id)
{
    ItemStack *st = worn_stack(static_cast<Scene *>(ud), player, square);
    if (st == nullptr || id < 0 || (uint32_t)id > kTagIdMask) return 0;
    st->tag = (st->tag & ~kTagIdMask) | (uint32_t)id;
    return 1;
}

int host_enchant_held(void *ud, int player, int level, int kind)
{
    Scene *s = static_cast<Scene *>(ud);
    if (s->bag == nullptr || (ui::PlayerId)player != ui::kLocalPlayer) return -1;
    ItemStack &held = s->bag->slot(s->bag->selected());
    if (held.empty() || s->act().items.max_stack(held.item) != 1) return -1;
    if (kind < 1 || kind > 4) return -1;
    held.ench = enchant_with(held.ench, (Enchant)kind, std::clamp(level, 0, 3));
    return enchant_level_for(held.ench, (Enchant)kind);
}

int host_wear_held(void *ud, int player, int amount)
{
    Scene *s = static_cast<Scene *>(ud);
    if (s->bag == nullptr || (ui::PlayerId)player != ui::kLocalPlayer || amount <= 0) return 0;
    return s->bag->wear_slot(s->act().items, s->bag->selected(), (uint16_t)std::min(amount, 65535)) ? 1 : 0;
}

int host_held_enchant(void *ud, int player)
{
    Scene *s = static_cast<Scene *>(ud);
    if (s->bag == nullptr || (ui::PlayerId)player != ui::kLocalPlayer) return -1;
    const ItemStack &held = s->bag->held();
    return held.empty() ? -1 : held.ench;
}

int host_give_item(void *ud, int player, const char *item, int count)
{
    Scene *s = static_cast<Scene *>(ud);
    if (!item || count <= 0 || s->bag == nullptr) return 0;
    if ((ui::PlayerId)player != ui::kLocalPlayer) return 0;

    const ItemId id = s->act().items.find(item);
    if (id == kNoItem) return 0;

    const uint16_t asked = count > 65535 ? (uint16_t)65535 : (uint16_t)count;
    const uint16_t left  = s->bag->give_to_hand(s->act().items, id, asked);
    return (int)asked - (int)left;
}

// A channel of this mod's own, from the client's side.
//
// The number is the SERVER's: it comes back in the welcome, in the order that
// server's mods asked for one. So this answers with nothing until there is a
// connection, and a client mod should not need the number at all -- it sends
// with core.net.send and hears with core:net_message, both of which look the
// channel up when they are used.
int host_render_pass(void *ud, const char *name, int order, int handler_ref, char *err,
                     size_t err_size)
{
    Scene             *s = static_cast<Scene *>(ud);
    const ModManifest *m = mod_host_current();
    std::string        why;
    if (s->passes.add(name ? name : "", order, mod_host_current_vm(), handler_ref,
                      m ? m->name : "", why))
        return 1;
    std::snprintf(err, err_size, "%s", why.c_str());
    return 0;
}

int host_render_box(void *ud, double x, double y, double z, double w, double h, double d, int r,
                    int g, int b, int a)
{
    Scene *s = static_cast<Scene *>(ud);

    // Only inside a pass. Outside one there is no camera bound and no target
    // being drawn into, so a box would go wherever the last frame left things
    // -- which is not "nothing happens", it is a mod drawing into somebody
    // else's render and the author never finding out why.
    if (!s->in_pass) {
        std::printf("[mod] render.box: only a render pass may draw\n");
        return 0;
    }

    Body box;
    box.center = Vec3{(float)x, (float)y, (float)z};
    box.half   = Vec3{(float)w * 0.5f, (float)h * 0.5f, (float)d * 0.5f};
    box.colour[0] = (float)std::clamp(r, 0, 255) / 255.0f;
    box.colour[1] = (float)std::clamp(g, 0, 255) / 255.0f;
    box.colour[2] = (float)std::clamp(b, 0, 255) / 255.0f;
    box.colour[3] = (float)std::clamp(a, 0, 255) / 255.0f;

    s->body_render.draw(box, s->pass_projection, s->pass_view);
    return 1;
}

// core.render.model: one of the world's models at rest, inside a pass, as a
// block's model is drawn (wire_model_draw).
int host_render_model(void *ud, int model, double x, double y, double z, double yaw, double scale,
                      double pitch, double roll)
{
    Scene *s = static_cast<Scene *>(ud);
    if (!s->in_pass) {
        std::printf("[mod] render.model: only a render pass may draw\n");
        return 0;
    }
    if (model <= 0 || model > 0xffff || !std::isfinite(x) || !std::isfinite(y) || !std::isfinite(z) ||
        !std::isfinite(yaw) || !std::isfinite(pitch) || !std::isfinite(roll) || !(scale > 0.0) ||
        scale > 16.0)
        return 0;
    ModelStore::Entry *e = s->act().models.entry((uint16_t)model);
    if (e == nullptr) return 0;
    const Mat4 turn  = mat4_multiply(mat4_multiply(mat4_rotate_y((float)yaw), mat4_rotate_x((float)pitch)),
                                     mat4_rotate_z((float)roll));
    const Mat4 place = mat4_multiply(mat4_multiply(mat4_translate(Vec3{(float)x, (float)y, (float)z}), turn),
                                     mat4_scale(Vec3{(float)scale, (float)scale, (float)scale}));
    e->renderer.draw(s->pass_projection, s->pass_view, place, e->model.bind_pose, gfx_texture{0}, 1.0f);
    return 1;
}

// core.render.fx: an effect of light, inside a pass (render/fx_render.h).
int host_render_fx(void *ud, const char *kind, double x, double y, double z, double size, int r, int g,
                   int b, int a, double t, double x2, double y2, double z2)
{
    Scene *s = static_cast<Scene *>(ud);
    if (!s->in_pass) {
        std::printf("[mod] render.fx: only a render pass may draw\n");
        return 0;
    }
    const int k = fx_kind_named(kind);
    for (const double v : {x, y, z, size, t, x2, y2, z2})
        if (!std::isfinite(v)) return 0;
    if (k < 0 || !(size > 0.0) || size > 64.0) return 0;
    if (s->fx_ready == 0) s->fx_ready = s->fx_render.init() ? 1 : -1;
    if (s->fx_ready < 0) return 0;
    const float rgba[4] = {std::clamp(r, 0, 255) / 255.0f, std::clamp(g, 0, 255) / 255.0f,
                           std::clamp(b, 0, 255) / 255.0f, std::clamp(a, 0, 255) / 255.0f};
    s->fx_render.draw((FxKind)k, Vec3{(float)x, (float)y, (float)z}, Vec3{(float)x2, (float)y2, (float)z2},
                      (float)size, rgba, (float)std::clamp(t, 0.0, 1.0), (float)std::fmod(plat_time(), 3600.0),
                      s->pass_projection, s->pass_view);
    return 1;
}

int host_hud_pass(void *ud, const char *name, int order, int handler_ref, char *err,
                  size_t err_size)
{
    Scene             *s = static_cast<Scene *>(ud);
    const ModManifest *m = mod_host_current();
    std::string        why;
    if (s->hud_passes.add(name ? name : "", order, mod_host_current_vm(), handler_ref,
                          m ? m->name : "", why))
        return 1;
    std::snprintf(err, err_size, "%s", why.c_str());
    return 0;
}

namespace {

void hud_colour(int r, int g, int b, int a, float out[4])
{
    out[0] = (float)std::clamp(r, 0, 255) / 255.0f;
    out[1] = (float)std::clamp(g, 0, 255) / 255.0f;
    out[2] = (float)std::clamp(b, 0, 255) / 255.0f;
    out[3] = (float)std::clamp(a, 0, 255) / 255.0f;
}

} // namespace

// (x, y) is the middle of the line; `align` 0 puts its left end there instead,
// and 2 its right end.
int host_hud_text(void *ud, double x, double y, const char *text, double size, int align, int r,
                  int g, int b, int a)
{
    Scene *s = static_cast<Scene *>(ud);
    if (!s->in_hud_pass) {
        std::printf("[mod] hud.text: only a hud pass may draw\n");
        return 0;
    }
    if (!text) return 0;
    text = lang::tr(text);

    const float sw = (float)gfx_screen_width();
    const float sh = (float)gfx_screen_height();
    const float px = (float)std::clamp(size, 0.005, 0.5) * sh;

    float left = (float)x * sw;
    if (align != 0) {
        const float wide = gfx_2d_text_width(text, px);
        left -= align == 2 ? wide : wide * 0.5f;
    }

    float rgba[4];
    hud_colour(r, g, b, a, rgba);
    gfx_2d_text(text, left, (float)y * sh - px * 0.5f, px, rgba);
    return 1;
}

// (x, y) is the middle of the rectangle.
double host_hud_aspect(void *)
{
    const int h = gfx_screen_height();
    return h > 0 ? (double)gfx_screen_width() / (double)h : 0.0;
}

int host_hud_rect(void *ud, double x, double y, double w, double h, int r, int g, int b, int a)
{
    Scene *s = static_cast<Scene *>(ud);
    if (!s->in_hud_pass) {
        std::printf("[mod] hud.rect: only a hud pass may draw\n");
        return 0;
    }

    const float sw = (float)gfx_screen_width();
    const float sh = (float)gfx_screen_height();
    const float pw = (float)w * sh;
    const float ph = (float)h * sh;

    float rgba[4];
    hud_colour(r, g, b, a, rgba);
    gfx_2d_rect((float)x * sw - pw * 0.5f, (float)y * sh - ph * 0.5f, pw, ph, rgba);
    return 1;
}

int host_hud_image(void *ud, const char *name, double x, double y, double w, double h, int a)
{
    Scene *s = static_cast<Scene *>(ud);
    if (!s->in_hud_pass) {
        std::printf("[mod] hud.image: only a hud pass may draw\n");
        return 0;
    }
    if (!name) return 0;

    const auto key = std::make_pair((const void *)s->conn, std::string(name));
    auto       it  = s->hud_images.find(key);
    if (it == s->hud_images.end()) {
        gfx_texture          tex{};
        std::vector<uint8_t> bytes;
        int                  iw = 0, ih = 0;
        unsigned char       *px = nullptr;
        if (world_media(s->conn)(name, bytes) &&
            (px = gfx_image_decode_rgba(bytes.data(), (int)bytes.size(), &iw, &ih)) != nullptr) {
            tex = gfx_texture_create(px, iw, ih, GFX_PF_RGBA8, GFX_FILTER_NEAREST);   // pixel art stays crisp
            gfx_image_free(px);
        } else {
            std::printf("[mod] hud.image: '%s' is not a picture this world has\n", name);
        }
        it = s->hud_images.emplace(key, tex).first;
    }
    if (it->second.id == 0) return 0;

    const float sh = (float)gfx_screen_height();
    const float pw = (float)w * sh;
    const float ph = (float)h * sh;
    const float rgba[4] = {1.0f, 1.0f, 1.0f, (float)std::clamp(a, 0, 255) / 255.0f};
    gfx_2d_image(it->second, 0.0f, 0.0f, 1.0f, 1.0f,
                 (float)x * (float)gfx_screen_width() - pw * 0.5f, (float)y * sh - ph * 0.5f, pw, ph,
                 rgba);
    return 1;
}

int host_hud_item(void *ud, const char *name, double x, double y, double side)
{
    Scene *s = static_cast<Scene *>(ud);
    if (!s->in_hud_pass) {
        std::printf("[mod] hud.item: only a hud pass may draw\n");
        return 0;
    }
    // The hotbar's registry: on a connection the server's, which the world's
    // own does not have.
    const ItemRegistry &items = s->hotbar.items ? *s->hotbar.items : s->act().items;
    const ItemId        id    = name ? items.find(name) : kNoItem;
    if (id == kNoItem) return 0;

    ui::IconSource icons;
    icons.atlas = s->act().world_render.atlas();
    icons.tiles = s->act().world_render.atlas_tiles();
    ItemStack stack;
    stack.item  = id;
    stack.count = 1;
    const float sh = (float)gfx_screen_height();
    ui::draw_drag_ghost(stack, (float)x * (float)gfx_screen_width(), (float)y * sh, (float)side * sh,
                        items, s->act().registry, icons, false);
    return 1;
}

int host_hud_mouse(void *, double *x, double *y)
{
    const int w = gfx_screen_width(), h = gfx_screen_height();
    if (w <= 0 || h <= 0) return 0;
    float mx = 0.0f, my = 0.0f;
    plat_mouse_position(&mx, &my);
    *x = (double)mx / (double)w;
    *y = (double)my / (double)h;
    return 1;
}

int host_player_look(void *ud, int player, float *x, float *y, float *z)
{
    Scene *s = static_cast<Scene *>(ud);
    if (!x || !y || !z || s->player == nullptr) return 0;
    if ((ui::PlayerId)player != ui::kLocalPlayer) return 0;

    const Vec3 f = s->player->forward();
    *x = f.x;
    *y = f.y;
    *z = f.z;
    return 1;
}

// core.craft on a player's machine: only the machine its player sits in, run
// here as well as on the server (Scene::Predicted). make starts it where it is
// drawn; keys are what its driver asks; pose is where it is predicted to be.
int host_craft_make_here(void *ud, double id, const char *kind, double mass, double half_w, double half_h,
                         double half_l, double accel, double top, double turn)
{
    Scene &s = *static_cast<Scene *>(ud);
    net::Ride seat;
    if (s.conn == nullptr || !s.conn->riding(seat) || (double)seat.host != id) return 0;
    const std::string k = kind ? kind : "";
    CraftSpec spec;
    if (k == "wheeled") spec.kind = CraftKind::Wheeled;
    else if (k == "tracked") spec.kind = CraftKind::Tracked;
    else if (k == "rotor") spec.kind = CraftKind::Rotor;
    else if (k == "wing") spec.kind = CraftKind::Wing;
    else return 0;
    for (const double v : {mass, half_w, half_h, half_l, accel, top, turn})
        if (!std::isfinite(v) || v <= 0.0) return 0;
    for (const net::RemotePlayer &p : s.conn->players()) {
        if ((double)p.id != id) continue;
        spec.mass    = (float)mass;
        spec.half_w  = (float)half_w;
        spec.half_h  = (float)half_h;
        spec.half_l  = (float)half_l;
        spec.accel   = (float)accel;
        spec.top     = (float)top;
        spec.turn    = (float)turn;
        spec.gravity = s.act().movement.gravity;
        Scene::Predicted pr;
        pr.id  = p.id;
        pr.sim = craft_make(spec, Vec3{p.x, p.y, p.z}, p.yaw, s.act().world, s.act().registry);
        CraftPose at  = craft_pose(*pr.sim);
        at.pitch      = p.pitch;
        at.roll       = p.chassis ? p.body_roll : 0.0f;
        const Session::BodyMotion &m = s.act().motion[p.id];
        at.velocity   = (m.glide_to - m.glide_from) * (1.0f / std::max(m.glide_gap, 0.02f));
        craft_place(*pr.sim, at);
        s.predicted = std::move(pr);
        return 1;
    }
    return 0;
}

int host_craft_keys_here(void *ud, double id, double forward, double right, double up, double yaw)
{
    Scene &s = *static_cast<Scene *>(ud);
    if (!s.predicted.sim || (double)s.predicted.id != id) return 0;
    s.predicted.keys = CraftKeys{(float)forward, (float)right, (float)up, (float)yaw};
    return 1;
}

int host_craft_pose_here(void *ud, double id, double *out)
{
    Scene &s = *static_cast<Scene *>(ud);
    if (!s.predicted.sim || (double)s.predicted.id != id) return 0;
    const CraftPose p = s.predicted.history.empty() ? craft_pose(*s.predicted.sim)
                                                    : s.predicted.history.back().second;
    const double v[13] = {p.base.x, p.base.y, p.base.z, p.yaw, p.pitch, p.roll, p.velocity.x, p.velocity.y,
                          p.velocity.z, p.speed, p.grounded ? 1.0 : 0.0, 0.0, p.throttle};
    for (int i = 0; i < 13; ++i) out[i] = v[i];
    return 1;
}

// core.shoot_arrow in this process: the one body's eye and look, into the
// world its drops and creatures are in. The server's is in src/server/main.cpp.
int host_shoot_arrow(void *ud, int player, double speed, int damage, int model, const char *item)
{
    Scene *s = static_cast<Scene *>(ud);
    if (s->player == nullptr || s->drops == nullptr || (ui::PlayerId)player != ui::kLocalPlayer) return 0;
    const Vec3   f    = s->player->forward();
    const Vec3   from = s->player->pos + Vec3{0.0f, s->act().movement.eye, 0.0f} + f * 0.3f;
    const ItemId picked = item && *item ? s->act().items.find(item) : kNoItem;
    ecs::loose_player_arrow(*s->drops, from, f * (float)std::clamp(speed, 0.0, 100.0),
                            (uint16_t)std::clamp(damage, 1, 1000),
                            (uint16_t)(model < 0 || (size_t)model > s->act().model_defs.size() ? 0 : model),
                            s->drops_me, picked == kNoItem ? 0 : picked, s->drop_clock);
    if (s->bag != nullptr) s->bag->wear_slot(s->act().items, s->bag->selected(), 1);
    return 1;
}

// The one body in this process, moved and turned (core.player_place). The
// server's copy, host_player_place in src/server/main.cpp, is what moves a
// player on a connected world; a world's code on this machine is not given it.
int host_player_place(void *ud, int player, double x, double y, double z, double lx, double ly,
                      double lz)
{
    Scene *s = static_cast<Scene *>(ud);
    if (s->player == nullptr || (ui::PlayerId)player != ui::kLocalPlayer) return 0;

    s->player->pos      = Vec3{(float)x, (float)y, (float)z};
    s->player->velocity = Vec3{0.0f, 0.0f, 0.0f};
    if (std::fabs(lx) + std::fabs(lz) > 1e-6) {
        s->player->yaw   = std::atan2((float)lx, (float)-lz);
        s->player->pitch = std::atan2((float)ly, std::sqrt((float)(lx * lx + lz * lz)));
    }
    return 1;
}

// The one body here, laid down or got up (core.player_lie); the server's copy
// is host_player_lie in src/server/main.cpp.
void play_at(Scene &s, snd_sound which, Vec3 at, int world, float range = 24.0f);

// A mod's sound (core.play_sound), in the one world here; the server's copy
// sends it to everybody instead (host_play_sound in src/server/main.cpp).
int host_play_sound(void *ud, const char *name, double x, double y, double z)
{
    Scene *s = static_cast<Scene *>(ud);
    if (!name || !net::sound_name_ok(name)) return 0;
    play_at(*s, s->sounds.named(name), Vec3{(float)x, (float)y, (float)z}, s->active);
    return 1;
}

int host_player_lie(void *ud, int player, int on, double x, double y, double z, double hx,
                    double hz)
{
    Scene *s = static_cast<Scene *>(ud);
    if (s->player == nullptr || (ui::PlayerId)player != ui::kLocalPlayer) return 0;
    if (!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(z)) return 0;
    s->player->lying = on != 0;
    if (!s->player->lying) return 1;
    s->player->pos      = Vec3{(float)x, (float)y, (float)z};
    s->player->velocity = Vec3{0.0f, 0.0f, 0.0f};
    s->player->lie_at   = s->player->pos;
    s->player->lie_head = std::atan2((float)hx, (float)-hz);
    return 1;
}

// The arms seen in first person playing a clip of the mod's until told
// otherwise (core.player_clip); the server's copy shows the body to everybody else.
int host_player_clip(void *ud, int player, const char *clip)
{
    Scene *s = static_cast<Scene *>(ud);
    if ((ui::PlayerId)player != ui::kLocalPlayer) return 0;
    s->own_clip = clip ? clip : "";
    return 1;
}

// This machine's own body dressed by world code (core.player_wear); the
// server's copy shows it to everybody else.
int host_player_wear(void *ud, int player, int slot, int model, int tint)
{
    Scene *s = static_cast<Scene *>(ud);
    if ((ui::PlayerId)player != ui::kLocalPlayer || slot < 1 || slot > 10 || model < 0 || model > 0xFFFF ||
        tint < 0 || tint > 0x7FFF)
        return 0;
    s->own_dress[slot - 1]      = (uint16_t)model;
    s->own_dress_tint[slot - 1] = model != 0 ? (uint16_t)tint : 0;
    return 1;
}

int host_raycast(void *ud, double x, double y, double z, double dx, double dy, double dz,
                 double max, int *hx, int *hy, int *hz, float *distance)
{
    Scene     *s = static_cast<Scene *>(ud);
    const Vec3 dir{(float)dx, (float)dy, (float)dz};
    if (length(dir) < 1e-6f || !hx || !hy || !hz || !distance) return 0;

    const RaycastHit hit = raycast(s->act().world, s->act().registry,
                                   Vec3{(float)x, (float)y, (float)z}, normalize(dir),
                                   (float)std::clamp(max, 0.0, 256.0));
    if (!hit.hit) return 0;
    *hx       = hit.x;
    *hy       = hit.y;
    *hz       = hit.z;
    *distance = hit.distance;
    return 1;
}

int host_net_open(void *ud)
{
    Scene             *s = static_cast<Scene *>(ud);
    const ModManifest *m = mod_host_current();
    if (!m || !s->conn) return 0;
    return (int)s->conn->channel_for(m->name);
}

int host_net_send(void *ud, int player, const char *text)
{
    Scene             *s = static_cast<Scene *>(ud);
    const ModManifest *m = mod_host_current();
    if (!m || !s->conn || !text) return 0;

    // `player` is ignored here and that is not an omission: a client says
    // things to the SERVER, which is the only other end of this socket. Who
    // else hears it is the server-side mod's decision, and giving a client a
    // way to address another player directly would be a client deciding who
    // gets messages.
    (void)player;
    return s->conn->send_mod(m->name, text) ? 1 : 0;
}

// A sentence on this screen (TODO.md OPEN-11). There is one player here, so
// `player` is whoever is playing, as a notice from a server is.
int host_notice(void *ud, int player, const char *text)
{
    Scene *s = static_cast<Scene *>(ud);
    (void)player;
    if (!text) return 0;
    s->notice.say(text, s->notice_clock);
    return 1;
}

// A line in this player's chat (core.chat); one player, as for a notice.
int host_chat(void *ud, int player, const char *text)
{
    Scene *s = static_cast<Scene *>(ud);
    (void)player;
    if (!text || std::strlen(text) > net::kMaxWireModText) return 0;
    s->chat.add(text, s->notice_clock);
    return 1;
}

// How a body moves in this world, from a mod (TODO.md OPEN-44). The server's
// copy, with the same refusals for the same reasons, is host_movement in
// src/server/main.cpp. On a connected world the welcome's numbers are adopted
// over these, because the server is the side that checks them.
int host_movement(void *ud, double walk, double sprint, double fly, double jump, double gravity)
{
    Session           &sess = static_cast<Scene *>(ud)->act();
    const ModManifest *m    = mod_host_current();
    const char        *name = m ? m->name.c_str() : "?";

    if (sess.mapgen_finished) {
        std::printf("[mod] %s: movement: too late -- a world says how a body moves while its "
                    "mods load, and the numbers go out in the welcome\n", name);
        return 0;
    }

    MovementRules rules;
    rules.walk    = (float)walk;
    rules.sprint  = (float)sprint;
    rules.fly     = (float)fly;
    rules.jump    = (float)jump;
    rules.gravity = (float)gravity;
    keep_body(rules, sess.movement);

    const std::string why = movement_rules_problem(rules);
    if (!why.empty()) {
        std::printf("[mod] %s: movement: refused -- %s\n", name, why.c_str());
        return 0;
    }

    if (!sess.movement_by.empty() && sess.movement_by != name)
        std::printf("[mod] %s: movement: replaces what %s declared\n", name,
                    sess.movement_by.c_str());

    sess.movement    = rules;
    sess.movement_by = name;
    std::printf("[mod] %s: movement: walk %.2f, sprint %.2f, fly %.2f, jump %.2f, gravity %.2f\n",
                name, (double)rules.walk, (double)rules.sprint, (double)rules.fly,
                (double)rules.jump, (double)rules.gravity);
    return 1;
}

// Whether bodies in this world drive, from a mod (core.drive). The server's copy,
// with the same refusals, is host_drive in src/server/main.cpp; on a connected
// world the welcome's numbers are adopted over these.
int host_drive(void *ud, double accel, double brake, double turn, double drag)
{
    Session           &sess = static_cast<Scene *>(ud)->act();
    const ModManifest *m    = mod_host_current();
    const char        *name = m ? m->name.c_str() : "?";

    if (sess.mapgen_finished) {
        std::printf("[mod] %s: drive: too late -- a world says how a body moves while its mods "
                    "load, and the numbers go out in the welcome\n", name);
        return 0;
    }

    MovementRules rules = sess.movement;
    rules.accel = (float)accel;
    rules.brake = (float)brake;
    rules.turn  = (float)turn;
    rules.drag  = (float)drag;

    const std::string why = movement_rules_problem(rules);
    if (!why.empty()) {
        std::printf("[mod] %s: drive: refused -- %s\n", name, why.c_str());
        return 0;
    }

    sess.movement = rules;
    std::printf("[mod] %s: drive: accel %.2f, brake %.2f, turn %.2f, drag %.2f\n", name,
                (double)rules.accel, (double)rules.brake, (double)rules.turn, (double)rules.drag);
    return 1;
}

// How high a body's eye is in this world, from a mod (core.eye_height), with
// the refusals core.body_width has: while the mods load, and in the body.
int host_eye_height(void *ud, double height)
{
    Session           &sess = static_cast<Scene *>(ud)->act();
    const ModManifest *m    = mod_host_current();
    const char        *who  = m ? m->name.c_str() : "?";

    if (sess.mapgen_finished) {
        std::printf("[mod] %s: eye_height: too late -- a world says how a body moves while its "
                    "mods load, and the numbers go out in the welcome\n", who);
        return 0;
    }
    MovementRules rules = sess.movement;
    rules.eye           = (float)height;
    const std::string why = movement_rules_problem(rules);
    if (!why.empty()) {
        std::printf("[mod] %s: eye_height: refused -- %s\n", who, why.c_str());
        return 0;
    }
    sess.movement = rules;
    std::printf("[mod] %s: eye_height: %.2f\n", who, (double)rules.eye);
    return 1;
}

// Whether bodies in this world do parkour, from a mod (core.parkour). The
// server's copy is host_parkour in src/server/main.cpp.
int host_ledges(void *ud, int on)
{
    Session &sess = static_cast<Scene *>(ud)->act();
    if (sess.mapgen_finished) return 0;
    sess.movement.ledges = on != 0;
    return 1;
}

int host_parkour(void *ud, int on)
{
    Session           &sess = static_cast<Scene *>(ud)->act();
    const ModManifest *m    = mod_host_current();
    const char        *who  = m ? m->name.c_str() : "?";

    if (sess.mapgen_finished) {
        std::printf("[mod] %s: parkour: too late -- a world says how a body moves while its "
                    "mods load, and the numbers go out in the welcome\n", who);
        return 0;
    }
    sess.movement.parkour = on != 0;
    std::printf("[mod] %s: parkour: %s\n", who, on ? "on" : "off");
    return 1;
}

// How wide a body is in this world, from a mod (core.body_width). The server's
// copy, with the same refusals, is host_body_width in src/server/main.cpp; on a
// connected world the welcome's number is adopted over this one.
int host_body_width(void *ud, double width, double length)
{
    Session           &sess = static_cast<Scene *>(ud)->act();
    const ModManifest *m    = mod_host_current();
    const char        *name = m ? m->name.c_str() : "?";

    if (sess.mapgen_finished) {
        std::printf("[mod] %s: body_width: too late -- a world says how a body moves while its "
                    "mods load, and the numbers go out in the welcome\n", name);
        return 0;
    }

    MovementRules rules = sess.movement;
    rules.width  = (float)width;
    rules.length = (float)length;

    const std::string why = movement_rules_problem(rules);
    if (!why.empty()) {
        std::printf("[mod] %s: body_width: refused -- %s\n", name, why.c_str());
        return 0;
    }

    sess.movement = rules;
    std::printf("[mod] %s: body_width: %.2f, length %.2f\n", name, (double)rules.width,
                (double)rules.length);
    return 1;
}

// One body's movement, from a mod (TODO.md OPEN-47). In this process there is
// one body, so `player` is not asked about; on a connected world it is the
// server's copy that decides, and the message it sends that moves this body.
int host_body_movement(void *ud, int player, double walk, double sprint, double fly,
                       double jump, double gravity, double rise)
{
    // One body here, the player's: any other id is nobody in this world, as
    // on a server.
    if ((ui::PlayerId)player != ui::kLocalPlayer) return 0;
    Session           &sess = static_cast<Scene *>(ud)->act();
    const ModManifest *m    = mod_host_current();
    const char        *name = m ? m->name.c_str() : "?";

    MovementRules rules;
    rules.walk    = (float)walk;
    rules.sprint  = (float)sprint;
    rules.fly     = (float)fly;
    rules.jump    = (float)jump;
    rules.gravity = (float)gravity;
    rules.rise    = (float)rise;
    keep_body(rules, sess.movement);   // a body in a world that drives still drives

    const std::string why = movement_rules_problem(rules);
    if (!why.empty()) {
        std::printf("[mod] %s: body_movement: refused -- %s\n", name, why.c_str());
        return 0;
    }
    // Grounded (core.player_fly): this speed is kept aside.
    if (sess.fly_held >= 0.0f) {
        sess.fly_held = rules.fly;
        rules.fly     = 0.0f;
    }

    sess.movement = rules;
    return 1;
}

// core.player_fly for the one body here; the server's is in src/server/main.cpp.
int host_player_fly(void *ud, int player, int allowed)
{
    if ((ui::PlayerId)player != ui::kLocalPlayer) return 0;
    Session &sess = static_cast<Scene *>(ud)->act();
    if (allowed) {
        if (sess.fly_held < 0.0f) return 1;
        sess.movement.fly = sess.fly_held;
        sess.fly_held     = -1.0f;
    } else {
        if (sess.fly_held >= 0.0f) return 1;
        sess.fly_held     = sess.movement.fly;
        sess.movement.fly = 0.0f;
    }
    return 1;
}

// One body in a kart, or back on foot (core.body_drive). In this process there
// is one body, so `player` is not asked about; on a connected world it is the
// server's copy that decides. The server's is host_body_drive in
// src/server/main.cpp.
int host_body_drive(void *ud, int player, double accel, double brake, double turn, double drag,
                    double width, double length)
{
    // One body here, the player's: any other id is nobody in this world, as
    // on a server.
    if ((ui::PlayerId)player != ui::kLocalPlayer) return 0;
    Session           &sess = static_cast<Scene *>(ud)->act();
    const ModManifest *m    = mod_host_current();
    const char        *name = m ? m->name.c_str() : "?";

    MovementRules rules = sess.movement;   // its speeds are its own and stay
    rules.accel  = (float)accel;
    rules.brake  = (float)brake;
    rules.turn   = (float)turn;
    rules.drag   = (float)drag;
    rules.width  = (float)width;
    rules.length = (float)length;

    const std::string why = movement_rules_problem(rules);
    if (!why.empty()) {
        std::printf("[mod] %s: body_drive: refused -- %s\n", name, why.c_str());
        return 0;
    }

    sess.movement = rules;
    return 1;
}

int host_ecs_component(void *ud, const char *name, const char *const *fields, int count,
                       char *err, size_t err_size)
{
    Scene                   *s = static_cast<Scene *>(ud);
    std::vector<std::string> f;
    for (int i = 0; i < count; ++i) f.push_back(fields && fields[i] ? fields[i] : "");

    std::string why;
    const ModManifest *m = mod_host_current();
    if (s->lua_ecs.component(name ? name : "", f, m ? m->name : "", why)) return 1;
    std::snprintf(err, err_size, "%s", why.c_str());
    return 0;
}

int host_ecs_phase(void *ud, const char *name, int order, char *err, size_t err_size)
{
    Scene             *s = static_cast<Scene *>(ud);
    std::string        why;
    const ModManifest *m = mod_host_current();
    if (s->lua_ecs.phase(name ? name : "", order, m ? m->name : "", why)) return 1;
    std::snprintf(err, err_size, "%s", why.c_str());
    return 0;
}

int host_ecs_system(void *ud, const char *name, const char *phase, const char *component,
                    int handler_ref, char *err, size_t err_size)
{
    Scene             *s = static_cast<Scene *>(ud);
    std::string        why;
    const ModManifest *m = mod_host_current();
    if (s->lua_ecs.system(name ? name : "", phase ? phase : "", component ? component : "",
                          mod_host_current_vm(), handler_ref, m ? m->name : "", why))
        return 1;
    std::snprintf(err, err_size, "%s", why.c_str());
    return 0;
}

double host_ecs_spawn(void *ud, const char *component, double x, double y, double z)
{
    return static_cast<Scene *>(ud)->lua_ecs.spawn(component ? component : "", x, y, z);
}

int host_ecs_set(void *ud, double id, const char *field, double value)
{
    return static_cast<Scene *>(ud)->lua_ecs.set(id, field ? field : "", value) ? 1 : 0;
}

int host_ecs_get(void *ud, double id, const char *field, double *out)
{
    if (!out) return 0;
    return static_cast<Scene *>(ud)->lua_ecs.get(id, field ? field : "", *out) ? 1 : 0;
}

int host_ecs_kill(void *ud, double id)
{
    return static_cast<Scene *>(ud)->lua_ecs.kill(id) ? 1 : 0;
}

int host_ecs_visible(void *ud, double id, int on)
{
    return static_cast<Scene *>(ud)->lua_ecs.visible(id, on != 0) ? 1 : 0;
}

int host_ecs_model(void *ud, double id, int model)
{
    if (model < 0) return 0;
    return static_cast<Scene *>(ud)->lua_ecs.model(id, (uint16_t)model) ? 1 : 0;
}

int host_ecs_health(void *ud, double id, int set)
{
    return static_cast<Scene *>(ud)->lua_ecs.health(id, set);
}

int host_ecs_clip(void *ud, double id, const char *name)
{
    return static_cast<Scene *>(ud)->lua_ecs.clip(id, name ? name : "") ? 1 : 0;
}

int host_ecs_box(void *ud, double id, double half, double height)
{
    return static_cast<Scene *>(ud)->lua_ecs.box(id, half, height) ? 1 : 0;
}

int host_ecs_count(void *ud, const char *component)
{
    return static_cast<Scene *>(ud)->lua_ecs.count(component ? component : "");
}

int host_find_nodes(void *ud, int x1, int y1, int z1, int x2, int y2, int z2, const char *name,
                    int *out, int max)
{
    Scene      *s = static_cast<Scene *>(ud);
    std::string why;
    const int   n =
        find_nodes(s->act().world, s->act().registry, x1, y1, z1, x2, y2, z2, name, out, max, why);
    if (!why.empty()) std::printf("[mod] find_nodes: %s\n", why.c_str());
    return n;
}

// The way a body of that size would walk, for a mod: see core/voxel/pathfind.h.
int host_find_path(void *ud, int x1, int y1, int z1, int x2, int y2, int z2, int height,
                   int step_up, int drop, int swim, int max_nodes, double hurry, int smooth,
                   int *out, int max)
{
    Scene *s = static_cast<Scene *>(ud);
    PathBody body;
    body.height    = height < 1 ? 1 : height > 8 ? 8 : height;
    body.step_up   = step_up < 0 ? 0 : step_up > 4 ? 4 : step_up;
    body.drop      = drop < 0 ? 0 : drop > 32 ? 32 : drop;
    body.swim      = swim != 0;
    body.max_nodes = max_nodes < 1 ? 1 : max_nodes > 20000 ? 20000 : max_nodes;
    body.hurry     = (float)(hurry < 1.0 ? 1.0 : hurry > 4.0 ? 4.0 : hurry);
    const PathResult r = find_path(s->act().world, s->act().registry, PathCell{x1, y1, z1}, PathCell{x2, y2, z2}, body);
    int gx = x1, gy = y1, gz = z1;   // where the way starts, on the ground as find_path took it
    path_ground(s->act().world, s->act().registry, gx, gy, gz, body);
    const std::vector<PathCell> cells =
        smooth ? path_smooth(s->act().world, s->act().registry, PathCell{gx, gy, gz}, r.cells, body) : r.cells;
    int n = 0;
    for (const PathCell &c : cells) {
        if (n + 3 > max) break;
        out[n++] = c.x;
        out[n++] = c.y;
        out[n++] = c.z;
    }
    return n;
}

// The name this client plays as (--name). One player in this process, so one
// name, for the local player's id and for nobody else.
std::string g_self_name = "player";

const char *host_player_name(void *ud, int player)
{
    const Scene *s = static_cast<Scene *>(ud);
    if (s->player == nullptr || (ui::PlayerId)player != ui::kLocalPlayer) return "";
    return g_self_name.c_str();
}

int host_players(void *ud, int *out, int max)
{
    Scene *s = static_cast<Scene *>(ud);
    if (!out || max <= 0 || s->player == nullptr) return 0;
    out[0] = (int)ui::kLocalPlayer;
    return 1;
}

int host_player_pos(void *ud, int player, float *x, float *y, float *z)
{
    Scene *s = static_cast<Scene *>(ud);
    if (!x || !y || !z || s->player == nullptr) return 0;
    if ((ui::PlayerId)player != ui::kLocalPlayer) return 0;

    // Feet, like the server's: Player::pos is the feet and the eye is a method
    // on top of it. Two engines answering the same question differently is two
    // games, and this is the pair that decides whether somebody is standing on
    // a pad.
    *x = s->player->pos.x;
    *y = s->player->pos.y;
    *z = s->player->pos.z;
    return 1;
}

int host_meta_set(void *ud, int x, int y, int z, const char *key, const char *value)
{
    Scene      *s = static_cast<Scene *>(ud);
    std::string why;
    if (s->act().node_meta.set(Cell{x, y, z}, key ? key : "", value ? value : "", why)) return 1;
    std::printf("[mod] meta at %d %d %d: %s\n", x, y, z, why.c_str());
    return 0;
}

const char *host_meta_get(void *ud, int x, int y, int z, const char *key)
{
    Scene *s = static_cast<Scene *>(ud);
    s->act().meta_scratch = s->act().node_meta.get(Cell{x, y, z}, key ? key : "");
    return s->act().meta_scratch.c_str();
}

int host_storage_set(void *ud, const char *key, const char *value)
{
    Session           &sess = static_cast<Scene *>(ud)->act();
    const ModManifest *m    = mod_host_current();
    const std::string  name = m ? m->name : "";
    std::string        why;
    if (sess.mod_store.set(name, key ? key : "", value ? value : "", why)) return 1;
    std::printf("[mod] %s: storage_set: refused -- %s\n", name.c_str(), why.c_str());
    return 0;
}

const char *host_storage_get(void *ud, const char *key)
{
    Session           &sess = static_cast<Scene *>(ud)->act();
    const ModManifest *m    = mod_host_current();
    sess.storage_scratch    = sess.mod_store.get(m ? m->name : "", key ? key : "");
    return sess.storage_scratch.c_str();
}

void host_set_portal(void *ud, int index, float x, float y, float z)
{
    Scene *s = static_cast<Scene *>(ud);
    const Vec3 p{x, y, z};
    if (index == 0) {
        s->act().link_ab.src.center = p;
        s->act().link_ba.dst.center = p;
        s->act().portal_a.center    = p;
    } else {
        s->act().link_ab.dst.center = p;
        s->act().link_ba.src.center = p;
        s->act().portal_b.center    = p;
    }
}

// Where a portal is now, one axis at a time.
//
// One float and an axis index rather than three returns, because the generator
// emits a single return value and widening it for one function is a worse trade
// than this shape.
//
// It exists because without it a mod that MOVES a portal has no way to move it
// RELATIVE to where it is, so it hardcodes coordinates -- and hardcoded
// coordinates belong to whichever scene the mod was written against. That is
// exactly how the spike mod came to drag portal B across the world in every
// scene but one.
float host_get_portal(void *ud, int index, int axis)
{
    const Scene *s = static_cast<const Scene *>(ud);
    const Vec3   p = (index == 0) ? s->act().portal_a.center : s->act().portal_b.center;
    return (axis == 0) ? p.x : (axis == 1) ? p.y : p.z;
}

// A frame a mod wants to open. This process's two openings are its scene's --
// the plaza pair every local run is built around -- so there is no free one to
// give, and a reshape would move a scene the rest of this file places things by.
// Said once; the mod says it to the player.
int host_portal_open(void *, float, float, float, int, int, int, float, float, int)
{
    static bool told = false;
    if (!told) {
        told = true;
        std::printf("[mod] portal_open: the single-process client's two openings are its "
                    "scene's; a portal a mod opens needs a server\n");
    }
    return -1;
}

int host_portal_close(void *, float, float, float) { return -1; }

void host_log(void *ud, const char *message)
{
    (void)ud;
    std::printf("[mod] %s\n", message);
}

// A mod registering a block. The engine gives back the id it assigned; the mod
// is not expected to care what it is.
int host_register_node(void *ud, const char *name, int r, int g, int b, int solid,
                       int light_source, const char *turn, int dig_level,
                       double hardness, const char *material)
{
    Scene *s = static_cast<Scene *>(ud);
    NodeDef d;
    d.name = name ? name : "unnamed";

    // Who registered it, so a reload can tell this mod's blocks from the rest.
    if (const ModManifest *m = mod_host_current()) d.owner = m->name;
    d.colour[0] = (uint8_t)r;
    d.colour[1] = (uint8_t)g;
    d.colour[2] = (uint8_t)b;
    d.solid  = solid != 0;
    d.opaque = solid != 0;
    d.light_source = (uint8_t)light_source;
    bool turn_ok   = true;
    d.turn         = node_turn_from_name(turn, &turn_ok);
    if (!turn_ok) {
        std::fprintf(stderr, "register_node('%s'): '%s' is not a turn kind\n",
                     name ? name : "?", turn ? turn : "");
        return 0;
    }
    d.dig_level    = (uint8_t)(dig_level < 0 ? 0 : (dig_level > 255 ? 255 : dig_level));
    d.hardness     = (float)(hardness < 0.0 ? 0.0 : hardness);
    d.material     = material ? material : "";
    return (int)s->act().registry.register_node(d);
}

// The same, on the side that actually draws them.
int host_node_model(void *ud, int id_in, int model_in)
{
    // A Scene, like every other host call here: this took the Scene it is
    // handed for a Session until 2026-09-24, read another struct's fields, and
    // no model block was ever drawn in a local world.
    Session *s  = &static_cast<Scene *>(ud)->act();
    const BlockId id = (BlockId)id_in;
    if (id == kAir || id >= s->registry.size()) return 0;

    // A model this world does not have is refused rather than stored: a block
    // pointing at an id nobody registered is a block that draws nothing, and it
    // would be found by looking at it rather than by being told. Asked of what
    // the mods DECLARED: the files are loaded later, on the first frame.
    const ModelId model = (ModelId)(model_in < 0 ? 0 : model_in);
    if (model != kNoModel && model > s->model_defs.size()) return 0;

    NodeDef d = s->registry.get(id);
    d.model   = model;
    s->registry.register_node(d);   // same name, same id, new definition
    return (int)model;
}

int host_node_climbable(void *ud, int id_in, int on)
{
    NodeRegistry &R = static_cast<Scene *>(ud)->act().registry;
    const BlockId id = (BlockId)id_in;
    if (id == kAir || id >= R.size()) return 0;
    NodeDef d   = R.get(id);
    d.climbable = on != 0;
    R.register_node(d);
    return 1;
}

int host_container_roles(void *ud, int id_in, const char *roles)
{
    NodeRegistry &R = static_cast<Scene *>(ud)->act().registry;
    const BlockId id = (BlockId)id_in;
    if (id == kAir || id >= R.size() || roles == nullptr) return 0;
    std::vector<uint8_t> out;
    std::string word;
    for (const char *c = roles;; ++c) {
        if (*c != ' ' && *c != '\0') { word += *c; continue; }
        if (!word.empty()) {
            const int r = slot_role(word.c_str());
            if (r < 0) {
                std::printf("[mod] container_roles: '%s' is not a role (store, raw, fuel, product, source, trash)\n",
                            word.c_str());
                return 0;
            }
            out.push_back((uint8_t)r);
            word.clear();
        }
        if (*c == '\0') break;
    }
    NodeDef d    = R.get(id);
    d.slot_rules.roles = std::move(out);
    R.register_node(d);
    return 1;
}

int host_node_keeps_drops(void *ud, int id_in, int on)
{
    NodeRegistry &R = static_cast<Scene *>(ud)->act().registry;
    const BlockId id = (BlockId)id_in;
    if (id == kAir || id >= R.size()) return 0;
    NodeDef d     = R.get(id);
    d.keeps_drops = on != 0;
    R.register_node(d);
    return 1;
}

int host_container_accepts(void *ud, int id_in, const char *role, const char *items)
{
    NodeRegistry &R = static_cast<Scene *>(ud)->act().registry;
    const BlockId id = (BlockId)id_in;
    const int     r  = slot_role(role);
    if (id == kAir || id >= R.size() || r < 0) return 0;
    std::vector<std::string> names;
    std::string word;
    for (const char *c = items ? items : "";; ++c) {
        if (*c != ' ' && *c != '\0') { word += *c; continue; }
        if (!word.empty()) names.push_back(std::move(word));
        word.clear();
        if (*c == '\0') break;
    }
    NodeDef d                    = R.get(id);
    d.slot_rules.accepts[(size_t)r] = std::move(names);
    R.register_node(d);
    return 1;
}

int host_container_progress(void *ud, int x, int y, int z, float seconds)
{
    Scene *s = static_cast<Scene *>(ud);
    if (s->act().containers.find(ContainerAt{x, y, z}) == nullptr) return 0;
    s->act().containers.set_progress(ContainerAt{x, y, z}, s->drop_clock, seconds);
    return 1;
}

int host_container_scroll(void *ud, int x, int y, int z, float pos, float size)
{
    Scene *s = static_cast<Scene *>(ud);
    if (s->act().containers.find(ContainerAt{x, y, z}) == nullptr) return 0;
    s->act().containers.set_scroll(ContainerAt{x, y, z}, pos, size);
    return 1;
}

int host_container_search(void *ud, int x, int y, int z, const char *text)
{
    Scene *s = static_cast<Scene *>(ud);
    if (s->act().containers.find(ContainerAt{x, y, z}) == nullptr) return 0;
    s->act().containers.set_search(ContainerAt{x, y, z}, text ? std::string(text).substr(0, ui::kMaxPanelText) : "");
    return 1;
}

int host_node_conveyor(void *ud, int id_in, float speed, float height, float rise)
{
    NodeRegistry &R = static_cast<Scene *>(ud)->act().registry;
    const BlockId id = (BlockId)id_in;
    if (id == kAir || id >= R.size()) return 0;
    NodeDef d  = R.get(id);
    d.conveyor        = std::max(speed, 0.0f);
    d.conveyor_height = height;
    d.conveyor_rise   = rise;
    R.register_node(d);
    return 1;
}

// Two blocks that are one door: `shut` and `open`, each the other's other.
int host_node_piece(void *ud, int id, int fill, int plan, int size, int climb, int grade0,
                    int grade1, int bank0, int bank1, int curbs, int wall, int width)
{
    Scene     *s = static_cast<Scene *>(ud);
    const auto b = [](int v) { return (int8_t)std::clamp(v, -127, 127); };
    PieceShape p;
    p.plan   = (uint8_t)std::clamp(plan, 0, 255);
    p.size   = (uint8_t)std::clamp(size, 0, 255);
    p.climb  = b(climb);
    p.grade0 = b(grade0);
    p.grade1 = b(grade1);
    p.bank0  = b(bank0);
    p.bank1  = b(bank1);
    p.curbs  = curbs && !wall ? 1 : 0;
    p.wall   = wall ? 1 : 0;
    p.width  = (uint8_t)std::clamp(width, 0, 255);
    return id > 0 && fill > 0 && piece_declare(s->act().registry, (BlockId)id, (BlockId)fill, p) ? 1 : 0;
}

// A track piece's cells, all of them or none, written as try_place writes one.
bool put_piece_here(Scene &s, BlockId id, uint8_t facing, int ox, int oy, int oz)
{
    const NodeDef &d = s.act().registry.get(id);
    if (d.piece.plan == 0 || facing > 3) return false;
    std::vector<PieceCell> cells;
    piece_cells(d.piece, id, facing, ox, oy, oz, cells);
    if (!piece_has_room(s.act().world, s.act().registry, cells)) return false;
    piece_headroom(s.act().world, s.act().registry, cells, cells);
    for (const PieceCell &c : cells) {
        if (s.act().keeper) s.act().keeper->wrote(c.x, c.z, s.act().column_clock);
        s.act().world.set_block(c.x, c.y, c.z, c.id);
        s.act().world.set_facing(c.x, c.y, c.z, c.facing);
        relight_around(s.act().world, s.act().registry, c.x, c.y, c.z);
    }
    return true;
}

int host_place_piece(void *ud, int x, int y, int z, int id, int facing)
{
    Scene     *s    = static_cast<Scene *>(ud);
    if (id <= 0 || (size_t)id >= s->act().registry.size() || facing < 0 || facing > 3) return 0;
    int ox, oz;
    piece_origin(s->act().registry.get((BlockId)id).piece, (uint8_t)facing, x, z, ox, oz);
    return put_piece_here(*s, (BlockId)id, (uint8_t)facing, ox, y, oz) ? 1 : 0;
}

int host_piece_entry(void *ud, int x, int y, int z, int *ex, int *ey, int *ez, int *id, int *facing)
{
    Session &w = static_cast<Scene *>(ud)->act();
    int      ox, oy, oz;
    if (!ex || !ey || !ez || !id || !facing || !piece_find(w.world, w.registry, x, y, z, ox, oy, oz))
        return 0;
    const BlockId piece = w.world.get_block(ox, oy, oz);
    *facing             = w.world.get_facing(ox, oy, oz) & 3;
    piece_entry(w.registry.get(piece).piece, (uint8_t)*facing, ox, oz, *ex, *ez);
    *ey = oy;
    *id = (int)piece;
    return 1;
}

// A whole piece away in a world of this process's own, as breaking it does
// (try_break), without the events: a mod asked for it.
int host_take_piece(void *ud, int x, int y, int z)
{
    Scene &s = *static_cast<Scene *>(ud);
    int    ox, oy, oz;
    if (!piece_find(s.act().world, s.act().registry, x, y, z, ox, oy, oz)) return 0;
    const BlockId          piece = s.act().world.get_block(ox, oy, oz);
    std::vector<PieceCell> cells;
    piece_cells(s.act().registry.get(piece).piece, piece, s.act().world.get_facing(ox, oy, oz), ox, oy,
                oz, cells);
    for (const PieceCell &c : cells) {
        if (s.act().world.get_block(c.x, c.y, c.z) != c.id) continue;
        if (s.act().keeper) s.act().keeper->wrote(c.x, c.z, s.act().column_clock);
        s.act().world.set_block(c.x, c.y, c.z, kAir);
        relight_around(s.act().world, s.act().registry, c.x, c.y, c.z);
    }
    return (int)piece;
}

int host_node_door(void *ud, int shut_in, int open_in)
{
    NodeRegistry &R = static_cast<Scene *>(ud)->act().registry;
    const BlockId shut = (BlockId)shut_in, open = (BlockId)open_in;
    if (shut == kAir || open == kAir || shut >= R.size() || open >= R.size() || shut == open) return 0;
    NodeDef a = R.get(shut), b = R.get(open);
    a.door = 1, a.door_other = open;
    b.door = 2, b.door_other = shut;
    R.register_node(a);
    R.register_node(b);
    return 1;
}

int host_node_drop(void *ud, int id_in, int as_in)
{
    Session      *s  = &static_cast<Scene *>(ud)->act();
    const BlockId id = (BlockId)id_in, as = (BlockId)(as_in < 0 ? 0 : as_in);
    if (id == kAir || id >= s->registry.size() || as >= s->registry.size() || as == id) return 0;

    NodeDef d  = s->registry.get(id);
    d.drops_as = as;
    s->registry.register_node(d);
    return 1;
}

int host_node_drop_item(void *ud, int id_in, const char *item)
{
    Session *s = &static_cast<Scene *>(ud)->act();
    const BlockId id = (BlockId)id_in;
    if (id == kAir || id >= s->registry.size() || item == nullptr) return 0;

    NodeDef d    = s->registry.get(id);
    d.drops_item = item;
    s->registry.register_node(d);
    return 1;
}

int host_node_solid(void *ud, int id)
{
    return id > 0 && static_cast<Scene *>(ud)->act().registry.is_solid((BlockId)id) ? 1 : 0;
}

int host_light(void *ud, int x, int y, int z)
{
    const Session &w   = static_cast<Scene *>(ud)->act();
    const uint8_t  l   = w.world.get_light(x, y, z);
    const float    day = w.sky_fixed ? 0.0f : daylight_at(w.time_of_day);
    return std::max((int)block_light(l), (int)std::lround((float)sun_light(l) * day));
}

int host_block_light(void *ud, int x, int y, int z)
{
    return (int)block_light(static_cast<Scene *>(ud)->act().world.get_light(x, y, z));
}

int host_loaded(void *ud, int x, int y, int z)
{
    return static_cast<Scene *>(ud)->act().world.find_chunk(World::chunk_of(x, y, z)) ? 1 : 0;
}

int host_node_turns(void *ud, int id)
{
    const NodeRegistry &reg = static_cast<Scene *>(ud)->act().registry;
    return id > 0 && (size_t)id < reg.size() ? node_turn_values(reg.get((BlockId)id).turn) : 0;
}

int host_node_alias(void *ud, const char *old_name, const char *name)
{
    Session *s = &static_cast<Scene *>(ud)->act();
    if (old_name == nullptr || name == nullptr) return 0;
    s->registry.alias(old_name, name);
    return 1;
}

int host_item_alias(void *ud, const char *old_name, const char *name)
{
    if (old_name == nullptr || name == nullptr) return 0;
    static_cast<Scene *>(ud)->act().items.alias(old_name, name);
    return 1;
}

int host_player_regen(void *ud, int player, int on)
{
    Scene *s = static_cast<Scene *>(ud);
    if ((ui::PlayerId)player != ui::kLocalPlayer) return 0;
    s->no_regen = on == 0;
    return 1;
}

float host_time_of_day(void *ud)
{
    return (float)static_cast<Scene *>(ud)->act().time_of_day;
}

int host_set_time(void *ud, double t)
{
    if (!std::isfinite(t)) return 0;
    static_cast<Scene *>(ud)->act().time_of_day = clock_wrap(t);
    return 1;
}

int host_player_spawn(void *ud, int player, double x, double y, double z, int on)
{
    Scene *s = static_cast<Scene *>(ud);
    if ((ui::PlayerId)player != ui::kLocalPlayer) return 0;
    s->has_bed = on != 0;
    s->bed     = Vec3{(float)x, (float)y, (float)z};
    return 1;
}

int host_node_box(void *ud, int id_in, double x0, double y0, double z0, double x1, double y1,
                  double z1, const char *part)
{
    Scene        *s  = static_cast<Scene *>(ud);
    const BlockId id = (BlockId)id_in;
    if (id == kAir || id >= s->act().registry.size()) return 0;

    NodeDef d = s->act().registry.get(id);
    NodeDef::Box box;
    box.min[0] = (float)x0; box.min[1] = (float)y0; box.min[2] = (float)z0;
    box.max[0] = (float)x1; box.max[1] = (float)y1; box.max[2] = (float)z1;
    box.part   = part ? part : "";
    d.parts.push_back(box);
    s->act().registry.register_node(d);
    return (int)d.parts.size();
}

int host_register_item(void *ud, const char *name, int max_stack, const char *places,
                       int dig_level, double dig_speed, const char *digs, int damage,
                       const char *wear, int armor, int durability, int arms)
{
    Scene *s = static_cast<Scene *>(ud);

    ItemDef d;
    d.name = name ? name : "unnamed";
    if (const ModManifest *m = mod_host_current()) d.owner = m->name;
    d.max_stack = (uint16_t)(max_stack < 1 ? 1 : max_stack);
    d.dig_level = (uint8_t)(dig_level < 0 ? 0 : (dig_level > 255 ? 255 : dig_level));
    d.dig_speed = (float)(dig_speed > 0.0 ? dig_speed : 1.0);
    d.digs      = digs ? digs : "";
    d.damage    = (uint16_t)std::clamp(damage, 0, 1000);
    d.wear      = wear_slot(wear);
    d.armor     = (uint8_t)std::clamp(armor, 0, 20);
    d.durability = (uint16_t)std::clamp(durability, 0, 65535);
    d.arms       = (uint8_t)std::clamp(arms, 0, (int)kMaxArms);
    d.places    = (places && *places) ? s->act().registry.find(places) : kAir;

    return (int)s->act().items.register_item(d);
}

int host_item_id(void *ud, const char *name)
{
    return (int)static_cast<Scene *>(ud)->act().items.find(name ? name : "");
}

int host_item_count(void *ud)
{
    return (int)static_cast<Scene *>(ud)->act().items.size();
}

const char *host_item_name(void *ud, int id)
{
    Scene *s = static_cast<Scene *>(ud);
    if (id <= 0 || (size_t)id >= s->act().items.size() || s->act().items.is_orphan((ItemId)id))
        return "";
    return s->act().items.get((ItemId)id).name.c_str();
}

int host_item_stack(void *ud, int id)
{
    Scene *s = static_cast<Scene *>(ud);
    if (id <= 0 || (size_t)id >= s->act().items.size()) return 0;
    return (int)s->act().items.max_stack((ItemId)id);
}

// --- panels, from a mod's side ----------------------------------------------
//
// Every one of these needs to know WHICH mod is asking, because a panel belongs
// to whoever declared it. mod_host_current() is the same answer register_item
// uses to fill in an item's owner, and there is exactly one place a mod's code
// can be running from.
const char *asking_mod()
{
    const ModManifest *m = mod_host_current();
    return m ? m->name.c_str() : "";
}

// A /command for this process's mods (core.command), with the server's rules.
int host_command(void *ud, const char *name, const char *help, const char *)
{
    Scene *s = static_cast<Scene *>(ud);
    if (!name || !net::chat_command_name_ok(name) || net::chat_builtin(name)) return 0;
    if (std::strcmp(name, "page") == 0 || std::strcmp(name, "admin") == 0) return 0;
    const std::string owner = asking_mod();
    const auto        it    = s->chat_commands.find(name);
    if (it != s->chat_commands.end() && it->second.owner != owner) return 0;

    s->chat_commands[name] = net::ChatCommand{owner, help ? help : ""};
    return 1;
}

// The one player of a world run in this process owns it (src/server/admin.h).
int         host_has_perm(void *, int, const char *) { return 1; }
const char *host_role(void *, int) { return "owner"; }
int         host_is_muted(void *, int) { return 0; }

// core.page_register and core.page_show, cnp_server's refusals; a connected
// world's pages are the server's, so its code here has none.
bool core_page_name(const std::string &n)
{
    for (const char *c : {"pages", "admin", "bans", "whitelist", "roles", "perms", "server", "running", "players", "reports", "log",
                          "mods", "here", "rules"})
        if (n == c) return true;
    return false;
}

int host_page_register(void *ud, const char *name, const char *title, const char *)
{
    Scene *s = static_cast<Scene *>(ud);
    if (s->conn || !name || !net::chat_command_name_ok(name) || core_page_name(name)) return 0;
    const std::string owner = asking_mod();
    const auto        it    = s->pages.find(name);
    if (it != s->pages.end() && it->second.first != owner) return 0;
    s->pages[name] = {owner, title && *title ? title : name};
    return 1;
}

void show_local_page(Scene &s, Scene::PageUp up);   // with the chat, below

int host_page_show(void *ud, int, const char *name, const char *markup)
{
    Scene *s = static_cast<Scene *>(ud);
    if (s->conn || !name || !*name || !markup || std::strlen(markup) > 16384) return 0;
    Scene::PageUp up;
    up.name   = name;
    up.owner  = asking_mod();
    up.markup = markup;
    const auto decl = s->pages.find(name);
    up.title = decl != s->pages.end() && decl->second.first == up.owner ? decl->second.second : name;
    if (s->page_up && s->page_up->name == up.name) up.screen = s->page_up->screen;
    show_local_page(*s, std::move(up));
    return 1;
}

// A stage of world generation, from a mod. The server's copy of this, with the
// same refusals for the same reasons, is host_register_mapgen in
// src/server/main.cpp: what a mod is allowed to do must not depend on which of
// the two is holding the world.
int host_register_mapgen(void *ud, const char *name, int handler_ref, char *err, size_t err_size)
{
    Session          &sess  = static_cast<Scene *>(ud)->act();
    const std::string stage = name ? name : "";

    if (stage.empty()) {
        std::snprintf(err, err_size, "a mapgen stage needs a name");
        return 0;
    }
    if (sess.mapgen_finished) {
        std::snprintf(err, err_size,
                      "'%s' asked for a mapgen stage after the world was generated; a stage "
                      "has to be registered while the mod loads",
                      stage.c_str());
        return 0;
    }
    for (const Session::LuaStage &st : sess.mapgen_stages) {
        if (st.name == stage) {
            std::snprintf(err, err_size, "a mapgen stage called '%s' is already registered by %s",
                          stage.c_str(), st.owner.empty() ? "the engine" : st.owner.c_str());
            return 0;
        }
    }

    Session::LuaStage st;
    st.name  = stage;
    st.owner = asking_mod();
    st.vm    = mod_host_current_vm();
    st.ref   = handler_ref;
    sess.mapgen_stages.push_back(st);
    return 1;
}

// The mods' stages over one column, the way the server runs them. Returns how
// many failed; a stage whose mod an earlier column stopped does not run again.
size_t run_mod_stages(Session &sess, ColumnPos column)
{
    if (sess.mapgen_stages.empty() || !sess.mapgen_ctx) return 0;

    mapgen_set_column(*sess.mapgen_ctx, column);
    const MapgenContext &c = *sess.mapgen_ctx;

    size_t failed = 0;
    for (const Session::LuaStage &st : sess.mapgen_stages) {
        bool alive = false;
        for (const Mod &m : sess.mods.mods())
            if (st.vm && m.vm == st.vm) alive = m.running;
        if (!alive) continue;

        script_field fields[6] = {
            {"seed",   SCRIPT_V_NUMBER, (double)c.seed,       nullptr, 0},
            {"height", SCRIPT_V_NUMBER, (double)kWorldHeight, nullptr, 0},
            {"min_x",  SCRIPT_V_NUMBER, (double)c.min_x,      nullptr, 0},
            {"min_z",  SCRIPT_V_NUMBER, (double)c.min_z,      nullptr, 0},
            {"max_x",  SCRIPT_V_NUMBER, (double)c.max_x,      nullptr, 0},
            {"max_z",  SCRIPT_V_NUMBER, (double)c.max_z,      nullptr, 0},
        };

        char stage_err[256] = {0};
        sess.mapgen_generating = true;
        sess.mods.enter_vm(st.vm);
        const bool ok = script_call_handler(st.vm, st.ref, fields, 6, 0, nullptr, stage_err,
                                            sizeof(stage_err));
        sess.mods.leave_vm(st.vm);
        sess.mapgen_generating = false;
        if (!ok) {
            ++failed;
            sess.mods.disable_vm(st.vm, "mapgen stage '" + st.name + "': " + stage_err);
        }
    }
    return failed;
}

// The same two registrations the server has, refusing the same things: what a
// mod may do must not depend on which of the two is holding the world.
int client_register_modifier(void *ud, const char *name, const char *const *nodes, int count,
                             double interval, int chance, int handler_ref, bool is_abm,
                             char *err, size_t err_size)
{
    Session &sess = static_cast<Scene *>(ud)->act();

    Modifier m;
    m.name     = name ? name : "";
    m.owner    = asking_mod();
    m.interval = interval;
    m.chance   = chance;
    m.vm       = mod_host_current_vm();
    m.handler  = handler_ref;
    for (int i = 0; i < count; ++i) {
        if (nodes && nodes[i]) m.node_names.push_back(nodes[i]);
    }

    std::string error;
    const bool  ok = is_abm ? sess.modifiers.register_abm(std::move(m), error)
                            : sess.modifiers.register_lbm(std::move(m), error);
    if (!ok) {
        std::snprintf(err, err_size, "%s", error.c_str());
        return 0;
    }
    return 1;
}

int host_register_abm(void *ud, const char *name, const char *const *nodes, int count,
                      double interval, int chance, int handler_ref, char *err, size_t err_size)
{
    return client_register_modifier(ud, name, nodes, count, interval, chance, handler_ref, true,
                                    err, err_size);
}

int host_register_lbm(void *ud, const char *name, const char *const *nodes, int count,
                      int handler_ref, char *err, size_t err_size)
{
    return client_register_modifier(ud, name, nodes, count, 0.0, 1, handler_ref, false, err,
                                    err_size);
}

int host_terrain_height(void *ud, int x, int z)
{
    Session &sess = static_cast<Scene *>(ud)->act();
    if (!sess.mapgen_ctx) return 0;
    return mapgen_terrain_height(*sess.mapgen_ctx, x, z);
}

// Where the water stands over a column (core.water_level), from the same context.
int host_water_level(void *ud, int x, int z)
{
    Session &sess = static_cast<Scene *>(ud)->act();
    if (!sess.mapgen_ctx) return 0;
    return mapgen_water_level(*sess.mapgen_ctx, x, z);
}

// The climate at a column, from the same context core.terrain_height reads.
const char *host_biome(void *ud, int x, int z)
{
    Session &sess = static_cast<Scene *>(ud)->act();
    if (!sess.mapgen_ctx || !sess.mapgen_ctx->biomes) return "";
    const BiomeSet &set = *sess.mapgen_ctx->biomes;
    return set.at(mapgen_biome_at(set, sess.mapgen_ctx->seed, x, z)).name.c_str();
}

// A block a body swims in, the same way the server makes one.
int host_node_liquid(void *ud, int id_in, int opacity, int fill)
{
    Session      &sess = static_cast<Scene *>(ud)->act();
    const BlockId id   = (BlockId)id_in;
    if (id == kAir || id >= sess.registry.size()) return 0;

    NodeDef d   = sess.registry.get(id);
    d.solid     = false;
    d.opaque    = false;
    d.liquid    = true;
    d.opacity   = (uint8_t)std::clamp(opacity, 0, 254);
    d.liquid_fill = (uint8_t)std::clamp(fill, 1, 8);
    d.breakable = false;
    sess.registry.register_node(d);
    return 1;
}

int host_node_glass(void *ud, int id_in, int opacity)
{
    Session      &sess = static_cast<Scene *>(ud)->act();
    const BlockId id   = (BlockId)id_in;
    if (id == kAir || id >= sess.registry.size()) return 0;
    NodeDef d = sess.registry.get(id);
    d.opaque  = false;
    d.opacity = (uint8_t)std::clamp(opacity, 0, 254);
    sess.registry.register_node(d);
    return 1;
}

// core.fall in this process: out of the cell and into the world the drops are in.
int host_fall(void *ud, int x, int y, int z)
{
    Scene   *s    = static_cast<Scene *>(ud);
    Session &sess = s->act();
    if (s->drops == nullptr) return 0;
    const BlockId id = sess.world.get_block(x, y, z);
    if (id == kAir || id >= sess.registry.size() || !sess.registry.get(id).breakable) return 0;
    const uint8_t facing = sess.world.get_facing(x, y, z);
    host_set_block(ud, x, y, z, kAir);
    ecs::spawn_falling(*s->drops, id, facing, x, y, z);
    return 1;
}

// A block that falls (core.node_falls) over nothing lets go, and so does one
// standing on a cell that stopped holding it up: host_set_block's and the
// player's own writes both come here, as every write reaches write_block on a
// server.
void loose_around(Scene &s, int x, int y, int z)
{
    Session &w = s.act();
    if (s.drops == nullptr || w.mapgen_generating) return;
    const auto loose = [&](int cy) {
        const BlockId b = w.world.get_block(x, cy, z);
        return b < w.registry.size() && w.registry.get(b).falls && cy > 0 &&
               !w.registry.is_solid(w.world.get_block(x, cy - 1, z));
    };
    if (loose(y)) host_fall(&s, x, y, z);
    else if (loose(y + 1)) host_fall(&s, x, y + 1, z);
}


// core.replace_nodes: `from` to `to` in a box, or the ellipsoid it holds,
// through set_block so every rule a single write keeps is kept.
template <class Get, class Set>
int replace_in_box(int x1, int y1, int z1, int x2, int y2, int z2, int from, int to, bool round,
                   Get get, Set set)
{
    if (x1 > x2) std::swap(x1, x2);
    if (y1 > y2) std::swap(y1, y2);
    if (z1 > z2) std::swap(z1, z2);
    if ((long)(x2 - x1 + 1) * (y2 - y1 + 1) * (z2 - z1 + 1) > 262144) return 0;   // 64^3
    const float cx = (x1 + x2) * 0.5f, cy = (y1 + y2) * 0.5f, cz = (z1 + z2) * 0.5f;
    const float rx = (x2 - x1 + 1) * 0.5f, ry = (y2 - y1 + 1) * 0.5f, rz = (z2 - z1 + 1) * 0.5f;
    int n = 0;
    for (int y = y1; y <= y2; ++y)
        for (int z = z1; z <= z2; ++z)
            for (int x = x1; x <= x2; ++x) {
                if (round) {
                    const float dx = (x - cx) / rx, dy = (y - cy) / ry, dz = (z - cz) / rz;
                    if (dx * dx + dy * dy + dz * dz > 1.0f) continue;
                }
                if (get(x, y, z) != from) continue;
                set(x, y, z);
                ++n;
            }
    return n;
}

int host_replace_nodes(void *ud, int x1, int y1, int z1, int x2, int y2, int z2, int from, int to,
                       int round)
{
    Session &w = static_cast<Scene *>(ud)->act();
    if (to < 0 || (size_t)to >= w.registry.size()) return 0;
    return replace_in_box(
        x1, y1, z1, x2, y2, z2, from, to, round != 0,
        [&](int x, int y, int z) { return (int)w.world.get_block(x, y, z); },
        [&](int x, int y, int z) { host_set_block(ud, x, y, z, to); });
}

int host_repair_held(void *ud, int player, int points)
{
    Scene *s = static_cast<Scene *>(ud);
    if (s->bag == nullptr || (ui::PlayerId)player != ui::kLocalPlayer) return -1;
    ItemStack &held = s->bag->slot(s->bag->selected());
    const int  was  = held.empty() ? 0 : held.used;
    held.used       = (uint16_t)std::max(0, was - std::max(0, points));
    return was;
}

int host_node_falls(void *ud, int id_in, int on)
{
    Session      &sess = static_cast<Scene *>(ud)->act();
    const BlockId id   = (BlockId)id_in;
    if (id == kAir || id >= sess.registry.size()) return 0;
    NodeDef d = sess.registry.get(id);
    d.falls   = on != 0;
    sess.registry.register_node(d);
    return 1;
}

// A refusal goes to the log, because these are generated and the generator does
// not know about the err-buffer shape the event bus uses. Said out loud rather
// than swallowed: a mod that gets 0 back and no reason is a mod author reading
// their own arithmetic looking for a bug that is not there.
bool panel_ok(bool ok, const std::string &err)
{
    if (!ok) std::printf("[mod] ui: %s\n", err.c_str());
    return ok;
}

int host_ui_define(void *ud, const char *name, float w, float h)
{
    Scene      *s = static_cast<Scene *>(ud);
    std::string err;
    return panel_ok(s->act().panels.define(name ? name : "", asking_mod(), w, h, err), err) ? 1 : 0;
}

int host_ui_label(void *ud, const char *panel, float x, float y, const char *text)
{
    Scene *s = static_cast<Scene *>(ud);

    ui::Element e;
    e.kind = ui::ElemKind::Label;
    e.text = text ? text : "";
    e.x = x; e.y = y;
    e.w = 0.0f; e.h = 1.0f;   // a label is as wide as its text; the height is a line

    std::string err;
    return panel_ok(s->act().panels.add(panel ? panel : "", asking_mod(), e, err), err) ? 1 : 0;
}

int host_ui_button(void *ud, const char *panel, const char *id, float x, float y, float w,
                   float h, const char *text)
{
    Scene *s = static_cast<Scene *>(ud);

    ui::Element e;
    e.kind = ui::ElemKind::Button;
    e.id   = id ? id : "";
    e.text = text ? text : "";
    e.x = x; e.y = y; e.w = w; e.h = h;

    std::string err;
    return panel_ok(s->act().panels.add(panel ? panel : "", asking_mod(), e, err), err) ? 1 : 0;
}

int host_ui_item(void *ud, const char *panel, const char *id, float x, float y, float w,
                 float h, const char *item)
{
    Scene *s = static_cast<Scene *>(ud);

    ui::Element e;
    e.kind = ui::ElemKind::Item;
    e.id   = id ? id : "";
    e.text = item ? item : "";
    e.x = x; e.y = y; e.w = w; e.h = h;

    std::string err;
    return panel_ok(s->act().panels.add(panel ? panel : "", asking_mod(), e, err), err) ? 1 : 0;
}

int host_ui_field(void *ud, const char *panel, const char *id, float x, float y, float w,
                  float h, const char *text)
{
    Scene *s = static_cast<Scene *>(ud);

    ui::Element e;
    e.kind = ui::ElemKind::Field;
    e.id   = id ? id : "";
    e.text = text ? text : "";
    e.x = x; e.y = y; e.w = w; e.h = h;

    std::string err;
    return panel_ok(s->act().panels.add(panel ? panel : "", asking_mod(), e, err), err) ? 1 : 0;
}

int host_ui_box(void *ud, const char *panel, float x, float y, float w, float h, int r, int g,
                int b)
{
    Scene *s = static_cast<Scene *>(ud);

    ui::Element e;
    e.kind = ui::ElemKind::Box;
    e.x = x; e.y = y; e.w = w; e.h = h;
    e.r = (uint8_t)(r < 0 ? 0 : (r > 255 ? 255 : r));
    e.g = (uint8_t)(g < 0 ? 0 : (g > 255 ? 255 : g));
    e.b = (uint8_t)(b < 0 ? 0 : (b > 255 ? 255 : b));

    std::string err;
    return panel_ok(s->act().panels.add(panel ? panel : "", asking_mod(), e, err), err) ? 1 : 0;
}

int host_ui_model(void *ud, const char *panel, float x, float y, float w, float h, int model, int tint,
                  const char *pieces)
{
    Scene      *s = static_cast<Scene *>(ud);
    std::string err;
    return panel_ok(s->act().panels.add(panel ? panel : "", asking_mod(),
                                        ui::model_element(x, y, w, h, model, tint, pieces), err),
                    err)
               ? 1
               : 0;
}

int host_ui_show(void *ud, const char *name, int open, int player)
{
    Scene      *s = static_cast<Scene *>(ud);
    std::string err;

    // In this process there is one player, so a mod that names nobody and a mod
    // that names the person at the keyboard want the same panel on the same
    // screen. On a server those are different requests -- see the server's copy
    // of this function.
    const ui::PlayerId viewer =
        player <= 0 ? ui::kAllPlayers : (ui::PlayerId)player;

    return panel_ok(s->act().panels.show(name ? name : "", asking_mod(), open != 0, viewer, err),
                    err)
               ? 1
               : 0;
}

// --- containers, from a mod's side ------------------------------------------
//
// The same four the server gives a mod. In this process there is one player and
// one screen, so "open it for that player" is "open it", and the id is still
// checked: a mod that names somebody who is not here must not open a chest on
// the only screen there is.

int host_recipe_shaped(void *ud, const char *makes, int count, const char *row1,
                       const char *row2, const char *row3)
{
    Scene      *s = static_cast<Scene *>(ud);
    std::string err;
    if (s->act().recipes.add_shaped(asking_mod(), makes ? makes : "",
                                    (uint16_t)(count < 0 ? 0 : count > 65535 ? 65535 : count),
                                    row1 ? row1 : "", row2 ? row2 : "", row3 ? row3 : "", err)) {
        return 1;
    }
    std::printf("[mod] recipe: %s\n", err.c_str());
    return 0;
}

int host_recipe_shapeless(void *ud, const char *makes, int count, const char *items)
{
    Scene      *s = static_cast<Scene *>(ud);
    std::string err;
    if (s->act().recipes.add_shapeless(asking_mod(), makes ? makes : "",
                                       (uint16_t)(count < 0 ? 0 : count > 65535 ? 65535 : count),
                                       items ? items : "", err)) {
        return 1;
    }
    std::printf("[mod] recipe: %s\n", err.c_str());
    return 0;
}

int host_recipe_dye(void *ud, const char *item)
{
    std::string err;
    if (static_cast<Scene *>(ud)->act().recipes.add_dyeable(item ? item : "", err)) return 1;
    std::printf("[mod] recipe_dye: %s\n", err.c_str());
    return 0;
}

int host_item_dye(void *ud, const char *item, int dye)
{
    std::string err;
    if (static_cast<Scene *>(ud)->act().recipes.add_dye(item ? item : "", dye, err)) return 1;
    std::printf("[mod] item_dye: %s\n", err.c_str());
    return 0;
}

int host_drop_item(void *ud, int x, int y, int z, int item, int count)
{
    Scene *s = static_cast<Scene *>(ud);
    if (s->drops == nullptr) return 0;
    if ((ItemId)item == kNoItem || count <= 0 || count > 65535) return 0;

    ItemStack stack;
    stack.item  = (ItemId)item;
    stack.count = (uint16_t)count;

    cnp::ecs::spawn_drop(*s->drops, stack, (float)x + 0.5f, (float)y + 0.5f, (float)z + 0.5f,
                         s->drop_clock);
    return 1;
}

int host_container_spill(void *ud, int x, int y, int z, int from)
{
    Scene            *s = static_cast<Scene *>(ud);
    const ContainerAt at{x, y, z};

    Inventory *bag = s->act().containers.find(at);
    if (bag == nullptr) return 0;

    int fell = 0;
    for (int i = from < 0 ? 0 : from; i < bag->size(); ++i) {
        const ItemStack out = bag->take_from(i);
        if (out.empty()) continue;
        if (host_drop_item(ud, x, y, z, (int)out.item, (int)out.count)) ++fell;
    }

    // From a slot on: those slots are empty now, and the container ends there.
    if (from > 0 && from < bag->size()) {
        bag->resize(from);
        s->act().containers.touched(at);
    }

    if (fell > 0) s->act().containers.touched(at);
    return fell;
}

// Everything in this process's craft grid back into its bag, and what will not
// fit on the ground where the player is. The same two steps the server takes,
// through the same core function -- see core/item/craft_grid.h.
void spill_craft(Scene *s, int w, int h)
{
    if (s->craft == nullptr || s->bag == nullptr) return;

    const std::vector<ItemStack> spilled =
        craft_resize(s->act().items, *s->craft, *s->bag, w, h);

    if (s->drops == nullptr) return;
    for (const ItemStack &rest : spilled)
        cnp::ecs::spawn_drop(*s->drops, rest, s->player_x, s->player_y + s->act().movement.eye,
                             s->player_z, s->drop_clock);
}

// One blow, for this process's player. The same two steps the server takes --
// see ServerSessions::hurt_body -- with the respawn left to the loop, because
// the loop is the only thing here that owns a position.
// A sound at a place in a world, heard from where the last frame was drawn.
//
// `world` is which session it happened in, and that is the whole reason this
// takes it: a sound in the world through the opening arrives THROUGH the
// opening -- the way round rather than the way through, and out of the doorway
// rather than out of its own coordinates. See core/audio/attenuation.h.
void play_at(Scene &s, snd_sound which, Vec3 at, int world, float range)
{
    if (which.id == 0) return;

    Emitter e;
    e.pos   = at;
    e.range = range;

    const Heard h = (world == s.active)
                        ? hear(s.ear, e)
                        : hear_through(s.ear, s.act().link_ab.src, s.act().link_ab.dst, e);
    snd_play(which, h.gain, h.pan);
}

// And one that is not in the world at all: it happened to the player. No
// distance and no side, because there is no "where" for it to come from.
void play_here(Scene &s, snd_sound which)
{
    if (which.id != 0) snd_play(which, 1.0f, 0.0f);
}

// What the player's own body sounds like this frame: a step every so much
// ground walked, a splash going into water, the jetpack while flying. Called
// by both loops after the move, with the body as it now is.
void sound_body(Scene &s, const Player &p, float dt)
{
    static float walked       = 0.0f;
    static bool  was_in_water = false;

    const bool in_water = box_in_liquid(s.act().world, s.act().registry,
                                        body_box(p.pos, s.act().movement.width * 0.5f));
    if (in_water && !was_in_water) play_here(s, s.sounds.splash);
    was_in_water = in_water;

    if (p.on_ground && !p.flying && !p.vehicle && !in_water) {
        walked += std::hypot(p.velocity.x, p.velocity.z) * dt;
        if (walked >= 1.8f) {
            walked = 0.0f;

            // What the feet are on says which step: its material, as a tool
            // reads it (NodeDef::material).
            const std::string &m = s.act().registry.get(s.act().world.get_block(
                (int)std::floor(p.pos.x), (int)std::floor(p.pos.y - 0.05f), (int)std::floor(p.pos.z))).material;
            const snd_sound step = m == "stone" && s.sounds.step_stone.id ? s.sounds.step_stone
                                 : m == "wood" && s.sounds.step_wood.id   ? s.sounds.step_wood
                                                                          : s.sounds.step;
            // Quiet: a step every two nodes is heard all the time, and at
            // half volume it covered everything else (2026-10-18); 0.12 was
            // still too loud (2026-10-23).
            snd_play(step, 0.06f, 0.0f);
        }
    }

    // The jetpack: a hum held low, a little louder as the body moves (full
    // at 14 m/s, a flight's speed), eased so a start or a stop is no jump.
    static float jet = 0.0f;
    const float  want = 0.03f + 0.07f * std::min(length(p.velocity) / 14.0f, 1.0f);
    jet += (want - jet) * std::min(dt * 4.0f, 1.0f);
    snd_loop(s.sounds.jetpack, jet, p.flying);
}

void hurt_local(Scene *s, uint16_t amount, bool armoured = true)
{
    if (s != nullptr && amount > 0) play_here(*s, s->sounds.hurt.any());

    if (s->health == nullptr || amount == 0) return;
    if (!s->health->alive()) return;   // a body at zero is not a bank

    // The server's rule, ServerSessions::hurt_body: what is worn takes its share.
    if (armoured && s->bag != nullptr) {
        const uint16_t wear = amount / 4 > 1 ? (uint16_t)(amount / 4) : (uint16_t)1;
        amount = cnp::ecs::armoured(amount, s->bag->armor(s->act().items));
        s->bag->wear_out(s->act().items, wear);
    }

    cnp::ecs::hurt(*s->health, amount);
    if (s->regen != nullptr) cnp::ecs::regen_interrupt(*s->regen);

    // Whole again at once, not on arrival: a body walking around at zero is one
    // anything could kill a second time before it was put back.
    if (!s->health->alive()) {
        s->health->hp = s->health->max;
        s->died       = true;
    }
}

int host_damage(void *ud, int player, int amount)
{
    Scene *s = static_cast<Scene *>(ud);
    if ((ui::PlayerId)player != ui::kLocalPlayer || s->health == nullptr) return 0;
    if (amount <= 0) return (int)s->health->hp;

    hurt_local(s, amount > 65535 ? (uint16_t)65535 : (uint16_t)amount);
    return (int)s->health->hp;
}

int host_player_max_health(void *ud, int player, int max)
{
    Scene *s = static_cast<Scene *>(ud);
    if ((ui::PlayerId)player != ui::kLocalPlayer || s->health == nullptr || max < 1 || max > 1000)
        return 0;
    s->health->max = (uint16_t)max;
    s->health->hp  = std::min(s->health->hp, s->health->max);
    return 1;
}

int host_heal(void *ud, int player, int amount)
{
    Scene *s = static_cast<Scene *>(ud);
    if ((ui::PlayerId)player != ui::kLocalPlayer || s->health == nullptr) return 0;
    if (amount > 0) cnp::ecs::heal(*s->health, amount > 65535 ? (uint16_t)65535 : (uint16_t)amount);

    return (int)s->health->hp;
}

int host_health(void *ud, int player)
{
    Scene *s = static_cast<Scene *>(ud);
    if ((ui::PlayerId)player != ui::kLocalPlayer || s->health == nullptr) return 0;

    return (int)s->health->hp;
}

int host_craft_show(void *ud, int x, int y, int z, int player)
{
    Scene *s = static_cast<Scene *>(ud);
    if ((ui::PlayerId)player != ui::kLocalPlayer) return 0;
    if (s->craft == nullptr) return 0;

    spill_craft(s, kCraftMax, kCraftMax);
    s->craft->table = true;
    s->craft->at    = ContainerAt{x, y, z};

    // The bag comes up with it, and the cursor with the bag. The same three
    // lines a chest needs and for the same reason -- see host_container_show.
    s->hotbar.open  = true;
    s->hotbar.press = -1;
    plat_cursor_grab(false);
    return 1;
}

int host_craft_close(void *ud, int player)
{
    Scene *s = static_cast<Scene *>(ud);
    if ((ui::PlayerId)player != ui::kLocalPlayer) return 0;
    if (s->craft == nullptr) return 0;

    spill_craft(s, 2, 2);
    s->craft->table = false;
    return 1;
}

// The bag panel shuts, and with it everything that was open behind it.
//
// One function and not a copy at every key. "Shutting the panel shuts the
// chest" was written at E, and written again at ESC, and only the one at E ever
// learned about the table -- so a player who left the panel with ESC kept a 3x3
// grid that belonged to a table they were no longer standing at, and opened
// their own bag to find it still there.
void shut_bag(Scene *s)
{
    s->hotbar.open       = false;
    s->hotbar.press      = -1;
    s->hotbar.press_right = false;
    s->hotbar.chest_open = false;

    // Which EMPTIES the grid back into the bag. A grid is not saved and is not
    // drawn once the panel is down, so a stack left in one when the window
    // shuts is a stack the player can neither see nor reach.
    if (s->craft != nullptr && (s->craft->table || s->craft->w > 2))
        host_craft_close(s, (int)ui::kLocalPlayer);
}

int host_container_make(void *ud, int x, int y, int z, int slots)
{
    Scene            *s = static_cast<Scene *>(ud);
    const ContainerAt at{x, y, z};
    std::string       err;
    if (s->act().containers.open(at, slots, err) == nullptr) {
        std::printf("[mod] container: %s\n", err.c_str());
        return 0;
    }
    return 1;
}

int host_container_add(void *ud, int x, int y, int z, int item, int count, const char *role)
{
    Scene *s = static_cast<Scene *>(ud);
    if (count <= 0 || count > 65535) return count < 0 ? 0 : count;

    // Nothing is not a thing that can be put in a chest, and Inventory::add
    // answers "none of it was left over" for it -- true, and the opposite of
    // what a caller asking what did not fit needs to hear.
    if ((ItemId)item == kNoItem) return count;

    const ContainerAt at{x, y, z};
    Inventory        *bag = s->act().containers.find(at);
    if (bag == nullptr) return count;

    const unsigned roles = role_mask(role);
    if (roles == 0) return count;
    const uint16_t left = bag->add(s->act().items, (ItemId)item, (uint16_t)count, -1, 0, 0, 0, roles);
    if (left != (uint16_t)count) s->act().containers.touched(at);
    return (int)left;
}

int host_container_show(void *ud, int x, int y, int z, int slots, int player)
{
    Scene *s = static_cast<Scene *>(ud);
    if ((ui::PlayerId)player != ui::kLocalPlayer) return 0;
    if (!host_container_make(ud, x, y, z, slots)) return 0;

    if (!s->hotbar.chest_open)
        play_at(*s, s->sounds.chest, Vec3{(float)x + 0.5f, (float)y + 0.5f, (float)z + 0.5f}, s->active);
    s->hotbar.chest_open = true;
    s->hotbar.chest_at   = ContainerAt{x, y, z};

    // And the bag with it, or a chest opens behind a closed bag and the player
    // is looking at the world with their hands in a box they cannot see.
    //
    // The CURSOR with it too, and that was the bug: opening the panel without
    // freeing the mouse draws a chest the player can see and cannot touch,
    // because every movement is still going to the camera. The networked loop
    // did this and this one did not -- the same rule in two places, written
    // once in each, which is how they came to differ.
    s->hotbar.open  = true;
    s->hotbar.press = -1;
    plat_cursor_grab(false);
    return 1;
}

int host_container_close(void *ud, int player)
{
    Scene *s = static_cast<Scene *>(ud);
    if ((ui::PlayerId)player != ui::kLocalPlayer) return 0;

    s->hotbar.chest_open = false;
    return 1;
}

int host_container_revision(void *ud, int x, int y, int z)
{
    Scene *s = static_cast<Scene *>(ud);
    return (int)(s->act().containers.revision(ContainerAt{x, y, z}) & 0x7fffffff);
}

int host_container_move(void *ud, int fx, int fy, int fz, int tx, int ty, int tz, int count)
{
    Scene *s = static_cast<Scene *>(ud);
    const ContainerAt from{fx, fy, fz}, to{tx, ty, tz};
    Inventory *a = s->act().containers.find(from), *b = s->act().containers.find(to);
    if (a == nullptr || b == nullptr || a == b || count <= 0) return 0;
    for (int slot = 0; slot < a->size(); ++slot) {
        if (a->at(slot).empty() || !((kRolesOut >> a->role(slot)) & 1u)) continue;   // what may come out
        const ItemStack out  = a->take_from(slot, (uint16_t)std::min(count, 65535));
        const uint16_t  left = b->add(s->act().items, out.item, out.count, -1, out.used, out.ench, out.tag);
        if (left > 0) {
            ItemStack &back = a->slot(slot);
            if (back.empty()) back = ItemStack{out.item, 0, out.used, out.ench, out.tag};
            back.count = (uint16_t)(back.count + left);
        }
        const int moved = (int)out.count - (int)left;
        if (moved > 0) {
            s->act().containers.touched(from);
            s->act().containers.touched(to);
        }
        return moved;
    }
    return 0;
}

int host_drop_at(void *ud, int x, int y, int z)
{
    Scene *s = static_cast<Scene *>(ud);
    return s->drops ? (int)cnp::ecs::drop_in_cell(*s->drops, x, y, z) : 0;
}

int host_drop_send(void *ud, int x, int y, int z, float tx, float ty, float tz)
{
    Scene *s = static_cast<Scene *>(ud);
    return s->drops ? (int)cnp::ecs::send_drop(*s->drops, x, y, z, tx, ty, tz) : 0;
}

int host_container_drop(void *ud, int x, int y, int z, float tx, float ty, float tz, int count)
{
    Scene     *s   = static_cast<Scene *>(ud);
    Inventory *bag = s->act().containers.find(ContainerAt{x, y, z});
    if (bag == nullptr || s->drops == nullptr) return 0;
    const int n = cnp::ecs::drop_from(*s->drops, *bag, count, tx, ty, tz, s->drop_clock);
    if (n > 0) s->act().containers.touched(ContainerAt{x, y, z});
    return n;
}

int host_container_item(void *ud, int x, int y, int z, int slot)
{
    const Inventory *bag = static_cast<Scene *>(ud)->act().containers.find(ContainerAt{x, y, z});
    if (bag == nullptr || slot < 1 || slot > bag->size()) return 0;
    return (int)bag->at(slot - 1).item;
}

int host_collect_drops(void *ud, int x, int y, int z, int tx, int ty, int tz)
{
    Scene *s = static_cast<Scene *>(ud);
    Inventory *into = s->act().containers.find(ContainerAt{tx, ty, tz});
    if (into == nullptr || s->drops == nullptr) return 0;
    int taken = 0;
    std::vector<flecs::entity> gone;
    s->drops->each([&](flecs::entity e, const ecs::Position &p, ecs::Dropped &d) {
        if ((int)std::floor(p.x) != x || (int)std::floor(p.y) != y || (int)std::floor(p.z) != z)
            return;
        const uint16_t left = into->add(s->act().items, d.stack.item, d.stack.count, -1, d.stack.used,
                                        d.stack.ench, d.stack.tag);
        taken += (int)d.stack.count - (int)left;
        d.stack.count = left;
        if (left == 0) gone.push_back(e);
    });
    for (flecs::entity e : gone) e.destruct();
    if (taken > 0) s->act().containers.touched(ContainerAt{tx, ty, tz});
    return taken;
}

float host_container_fill(void *ud, int x, int y, int z)
{
    Scene           *s = static_cast<Scene *>(ud);
    const Inventory *c = s->act().containers.find(ContainerAt{x, y, z});
    if (c == nullptr || c->size() == 0) return -1.0f;
    double sum = 0.0;
    for (int slot = 0; slot < c->size(); ++slot) {
        const ItemStack &st = c->at(slot);
        if (!st.empty()) sum += (double)st.count / (double)s->act().items.max_stack(st.item);
    }
    return (float)(sum / (double)c->size());
}

int host_container_count(void *ud, int x, int y, int z, int item, const char *role)
{
    Scene           *s = static_cast<Scene *>(ud);
    const Inventory *c = s->act().containers.find(ContainerAt{x, y, z});
    if (c == nullptr) return 0;
    const unsigned roles = role_mask_any(role);
    int n = 0;
    for (int i = 0; i < c->size(); ++i)
        if (c->at(i).item == (ItemId)item && ((roles >> c->role(i)) & 1u)) n += c->at(i).count;
    return n;
}

int host_container_take(void *ud, int x, int y, int z, int item, int count, const char *role)
{
    Scene            *s = static_cast<Scene *>(ud);
    const ContainerAt at{x, y, z};
    Inventory        *c = s->act().containers.find(at);
    if (c == nullptr || count <= 0 || (ItemId)item == kNoItem) return 0;

    int taken = 0;
    for (int slot = 0; slot < c->size() && taken < count; ++slot) {
        const ItemStack &in = c->at(slot);
        if (in.empty() || in.item != (ItemId)item || !((role_mask_any(role) >> c->role(slot)) & 1u)) continue;

        const int want = count - taken;
        const int here = (int)in.count < want ? (int)in.count : want;
        if (c->take(slot, (uint16_t)here)) taken += here;
    }
    if (taken > 0) s->act().containers.touched(at);
    return taken;
}

int host_container_clear(void *ud, int x, int y, int z)
{
    Scene            *s = static_cast<Scene *>(ud);
    const ContainerAt at{x, y, z};

    // Off the screen as well as out of the set. On the server the two are one
    // call for a reason -- a session left pointing at an erased cell has the
    // NEXT chest built there open by itself -- and this loop is the same rule
    // with one screen instead of a session table.
    if (s->hotbar.chest_open && s->hotbar.chest_at == at) s->hotbar.chest_open = false;

    return s->act().containers.erase(at) ? 1 : 0;
}

int host_node_id(void *ud, const char *name)
{
    return (int)static_cast<Scene *>(ud)->act().registry.find(name ? name : "");
}

// The same rule the server's copy keeps: a mod names a file beside its own
// init.luau, and a name that could climb out of that directory is refused
// rather than followed.
int host_register_model(void *ud, const char *name, const char *file)
{
    Scene            *s    = static_cast<Scene *>(ud);
    const std::string want = file ? file : "";
    if (!model_file_is_plain(want)) return 0;

    const ModManifest *m = mod_host_current();
    return (int)s->act().model_defs.add(name ? name : "", m ? m->dir + "/" + want : want);
}

// core.view: how far behind the player this world's camera sits, 0 for the eye.
// Clamped rather than refused: a camera sixty nodes back is a world that wanted
// "far", and sixteen is far.
bool g_first_person = false;   // --first-person: every world seen from the eye, whatever it asks

void host_arms_clip(void *ud, const char *clip)
{
    Scene &s         = *static_cast<Scene *>(ud);
    s.mod_arms       = clip ? clip : "";
    s.mod_arms_start = plat_time();
    s.arms.name.clear();   // from its start, and not faded into: a shot is at once
}

void host_zoom(void *ud, double factor)
{
    (void)ud;
    g_fov_y = kFovY * (float)(std::isfinite(factor) ? std::clamp(factor, 0.2, 1.5) : 1.0);
}

void host_held_model(void *ud, const char *name)
{
    static_cast<Scene *>(ud)->held_model = name ? name : "";
}

// This machine's body on a rope, or off it (core.rope): only where the world
// does parkour, whose server allows the swing.
int host_rope(void *ud, int on)
{
    Scene &s = *static_cast<Scene *>(ud);
    if (!s.player || !s.act().movement.parkour) return 0;
    if (!on) {
        s.player->roped = false;
        return 1;
    }
    const Vec3 eye  = s.cam_set ? s.cam_eye : s.player->eye();
    const Vec3 look = s.cam_set ? s.cam_forward : s.player->forward();
    return rope_fire(*s.player, s.act().world, s.act().registry, eye, look) ? 1 : 0;
}

void set_view_distance(Scene &s, float d);

double host_view(void *ud, double distance)
{
    if (g_first_person) distance = 0.0;
    const double d   = std::isfinite(distance) ? std::clamp(distance, 0.0, 16.0) : 0.0;
    Scene       &s   = *static_cast<Scene *>(ud);
    const double was = s.act().view_distance;
    if ((float)d != s.act().view_distance) set_view_distance(s, (float)d);
    return was;
}

void host_iso_view(void *ud, double distance, double yaw, double pitch, double fov, double seconds)
{
    Scene       &s = *static_cast<Scene *>(ud);
    Session     &w = s.act();
    const double d = std::isfinite(distance) ? std::clamp(distance, 0.0, 40.0) : 0.0;
    w.iso_secs     = (float)(std::isfinite(seconds) ? std::clamp(seconds, 0.0, 10.0) : 0.0);
    if (d <= 0.0) {
        w.iso_on = false;   // the rest is kept: it is where the way back starts
    } else {
        w.iso_fixed    = false;
        w.iso_distance = (float)d;
        w.iso_yaw      = (float)((std::isfinite(yaw) ? yaw : 45.0) * 3.14159265358979 / 180.0);
        w.iso_pitch    = (float)(std::clamp(std::isfinite(pitch) ? pitch : 50.0, 10.0, 89.0) *
                              3.14159265358979 / 180.0);
        w.iso_fov = (float)(std::isfinite(fov) && fov > 0.0
                                ? std::clamp(fov, 10.0, 100.0) * 3.14159265358979 / 180.0
                                : 0.0);
        if (!w.iso_on && w.iso_t <= 0.0f)
            w.iso_from = s.player ? s.player->forward() : s.cam_set ? s.cam_forward : w.iso_from;
        w.iso_on = true;
    }
    if (w.iso_secs <= 0.0f) w.iso_t = w.iso_on ? 1.0f : 0.0f;
}

void host_fixed_view(void *ud, double x, double y, double z, double distance, double yaw,
                     double pitch, double fov, double seconds)
{
    Session &w = static_cast<Scene *>(ud)->act();
    if (std::isfinite(distance) && distance > 0.0 && !(std::isfinite(x) && std::isfinite(y) && std::isfinite(z)))
        return;
    host_iso_view(ud, distance, yaw, pitch, fov, seconds);
    if (w.iso_on) {
        w.iso_fixed  = true;
        w.iso_target = Vec3{(float)x, (float)y, (float)z};
    }
}

bool iso_engaged(const Session &w);

// Through the pixel under the cursor, from the camera the last frame was drawn
// with: the same eye, look, up and field of view.
int host_cursor_ray(void *ud, float *ox, float *oy, float *oz, float *dx, float *dy, float *dz)
{
    const Scene &s = *static_cast<Scene *>(ud);
    const int    w = gfx_screen_width(), h = gfx_screen_height();
    if (!s.cam_set || w <= 0 || h <= 0) return 0;
    float mx = 0.0f, my = 0.0f;
    plat_mouse_position(&mx, &my);
    // The mouse held by the game drives the view, and the crosshair is the
    // middle of the screen: the ray goes through it, wherever the hidden
    // cursor drifted. From above the cursor is shown and points.
    if (plat_cursor_grabbed() && !iso_engaged(s.act())) {
        mx = 0.5f * (float)w;
        my = 0.5f * (float)h;
    }
    const float t     = std::tan(g_fov_y * 0.5f);
    const float x     = (2.0f * mx / (float)w - 1.0f) * t * (float)w / (float)h;
    const float y     = (1.0f - 2.0f * my / (float)h) * t;
    const Vec3  right = normalize(cross(s.cam_forward, s.cam_up));
    const Vec3  up    = cross(right, s.cam_forward);
    const Vec3  d     = normalize(s.cam_forward + right * x + up * y);
    *ox = s.cam_eye.x, *oy = s.cam_eye.y, *oz = s.cam_eye.z;
    *dx = d.x, *dy = d.y, *dz = d.z;
    return 1;
}

void host_walk(void *ud, double dx, double dz, double speed)
{
    Session     &s = static_cast<Scene *>(ud)->act();
    const double l = std::isfinite(dx) && std::isfinite(dz) ? std::hypot(dx, dz) : 0.0;
    s.mod_walk = l > 1e-6;
    if (!s.mod_walk) return;
    s.mod_walk_x     = (float)(dx / l);
    s.mod_walk_z     = (float)(dz / l);
    s.mod_walk_speed = (float)std::clamp(std::isfinite(speed) ? speed : 1.0, 0.0, 1.0);
}

// A model this machine's mods declared, or else one the world it is connected
// to sent: world code that runs here names the world's models too.
int host_model_id(void *ud, const char *name)
{
    Scene            &s    = *static_cast<Scene *>(ud);
    Session          &w    = s.act();
    const std::string want = name ? name : "";
    // Connected, the server's table is the one a body's model is a position
    // in, plus one (Welcome::models) -- and this machine's store may not have
    // read it yet: asked as a kart was got into, it answered 0, and the driver
    // was drawn as the robot standing beside its own kart (reported from play).
    if (s.conn != nullptr) {
        const std::vector<std::string> &names = s.conn->welcome().models;
        for (size_t i = 0; i < names.size(); ++i)
            if (names[i] == want) return (int)i + 1;
    }
    if (const int id = (int)w.model_defs.find(want)) return id;
    for (size_t id = 1; id <= w.models.size(); ++id)
        if (const ModelStore::Entry *e = w.models.entry((uint16_t)id); e && e->name == want)
            return (int)id;
    return 0;
}

void host_player_model(void *ud, int model)
{
    static_cast<Scene *>(ud)->act().declared_player_model = (ModelId)(model < 0 ? 0 : model);
}

// What ONE body wears (core.body_model). In this process there is one body, so
// `player` is not asked about, and it goes where core.player_model's goes -- a
// local world dresses nobody yet (add_own_body below). On a connected world it
// is the server's copy that decides, and the entity messages it sends that
// dress everybody else.
int host_body_model(void *ud, int player, int model)
{
    (void)player;
    static_cast<Scene *>(ud)->act().own_body_model = (ModelId)(model < 0 ? 0 : model);
    return 1;
}

void host_world_sky(void *ud, int r, int g, int b)
{
    Session &w  = static_cast<Scene *>(ud)->act();
    w.sky_fixed = true;
    w.sky_rgb[0] = (float)std::clamp(r, 0, 255) / 255.0f;
    w.sky_rgb[1] = (float)std::clamp(g, 0, 255) / 255.0f;
    w.sky_rgb[2] = (float)std::clamp(b, 0, 255) / 255.0f;
}

void host_world_storm(void *ud, int r, int g, int b)
{
    Session &w = static_cast<Scene *>(ud)->act();
    w.storm    = true;
    w.storm_rgb[0] = (float)std::clamp(r, 0, 255) / 255.0f;
    w.storm_rgb[1] = (float)std::clamp(g, 0, 255) / 255.0f;
    w.storm_rgb[2] = (float)std::clamp(b, 0, 255) / 255.0f;
}

void host_storm_funnel(void *ud, int slot, double x, double y, double z, int on)
{
    if (slot < 0 || slot > 3 || !std::isfinite(x) || !std::isfinite(y) || !std::isfinite(z)) return;
    float *f = static_cast<Scene *>(ud)->act().funnel + slot * 4;
    f[0] = (float)x;
    f[1] = (float)y;
    f[2] = (float)z;
    f[3] = on ? 1.0f : 0.0f;
}

void host_mapgen_core(void *ud, int on)
{
    Session &w = static_cast<Scene *>(ud)->act();
    if (w.mapgen_finished) {
        std::fprintf(stderr, "mapgen_core: too late -- only while the mods load\n");
        return;
    }
    w.core_mapgen = on != 0;
}

// The openings of a world this process runs are its scene's (core.portal_place).
int host_portal_place(void *, int, float, float, float, int, int, float, float) { return 0; }

void host_mob_model(void *ud, int model)
{
    static_cast<Scene *>(ud)->act().declared_mob_model = (ModelId)(model < 0 ? 0 : model);
}

// A kind of creature, into this world's table: the server's rule, from the
// same function.
int host_register_mob(void *ud, const char *name, int model, int health, double speed,
                      const char *fight, int damage, double reach, double every,
                      const char *drop, int drop_count, int shot_model, int opens_doors,
                      double hover, int fireball, const char *spawn_on)
{
    Scene            *s = static_cast<Scene *>(ud);
    ecs::MobKind      k;
    std::string       why;
    if (!ecs::make_mob_kind(k, why, name, model, health, speed, fight, damage, reach, every, drop,
                            drop_count, shot_model, opens_doors, hover, fireball, spawn_on, s->act().model_defs.size())) {
        std::fprintf(stderr, "register_mob('%s'): %s\n", name ? name : "?", why.c_str());
        return 0;
    }
    if (k.model != 0) ecs::size_mob_kind(k, s->act().model_defs.get(k.model).file);
    return ecs::add_mob_kind(s->act().mob_kinds, k);
}

// What each kind leaves, by name until the item exists: every mod has loaded
// by the time a creature comes out, not by the time it is registered.
void find_mob_drops(Session &w)
{
    ecs::find_mob_items(w.mob_kinds, w.items);
}

// What tames a pet (core.mob_tame).
int host_mob_tame(void *ud, const char *name, const char *item, int tamed_model)
{
    for (ecs::MobKind &k : static_cast<Scene *>(ud)->act().mob_kinds)
        if (k.name == (name ? name : "") && k.fight == ecs::MobKind::Fight::Pet) {
            k.tame_name  = item ? item : "";
            k.tame_model = (uint16_t)(tamed_model < 0 ? 0 : tamed_model);
            return 1;
        }
    return 0;
}

// What shears a creature, and what it lays (core.mob_shear, core.mob_lays).
int host_mob_shear(void *ud, const char *name, const char *item, const char *drop, int count,
                   int shorn_model, double regrow)
{
    return ecs::mob_shear(static_cast<Scene *>(ud)->act().mob_kinds, name, item, drop, count,
                          shorn_model, regrow);
}

int host_push_mobs(void *ud, int x, int y, int z, int dx, int dy, int dz)
{
    Scene *s = static_cast<Scene *>(ud);
    return s->drops ? ecs::push_mobs(*s->drops, x, y, z, dx, dy, dz) : 0;
}

int host_mobs_near(void *ud, double x, double y, double z, double r, float *out, int max)
{
    Scene *s = static_cast<Scene *>(ud);
    return s->drops ? ecs::mobs_near(*s->drops, s->act().mob_kinds, (float)x, (float)y, (float)z,
                                     (float)r, out, max)
                    : 0;
}

int host_mob_ids(void *ud, double x, double y, double z, double r, int *out, int max)
{
    Scene *s = static_cast<Scene *>(ud);
    return s->drops ? ecs::mob_ids(*s->drops, s->act().mob_kinds, (float)x, (float)y, (float)z,
                                   (float)r, out, max)
                    : 0;
}

int host_mob_pos(void *ud, int id, float *x, float *y, float *z)
{
    Scene *s = static_cast<Scene *>(ud);
    return s->drops && ecs::mob_pos(*s->drops, (uint32_t)id, x, y, z) ? 1 : 0;
}

int host_slow_mobs(void *ud, double x, double y, double z, double r, double factor, double seconds)
{
    Scene *s = static_cast<Scene *>(ud);
    return s->drops ? ecs::slow_mobs(*s->drops, s->act().mob_kinds, (float)x, (float)y, (float)z,
                                     (float)r, (float)factor, seconds, s->drop_clock)
                    : 0;
}

int host_engine_hud_on(void *ud)
{
    return static_cast<Scene *>(ud)->act().engine_hud ? 1 : 0;
}

void host_engine_hud(void *ud, int on)
{
    static_cast<Scene *>(ud)->act().engine_hud = on != 0;
}

int host_monsters_near(void *ud, double x, double y, double z, double r)
{
    Scene *s = static_cast<Scene *>(ud);
    return s->drops ? ecs::monsters_near(*s->drops, s->act().mob_kinds, (float)x, (float)y,
                                         (float)z, (float)r)
                    : 0;
}

int host_mob_sight(void *ud, const char *name, double nodes)
{
    return ecs::mob_sight(static_cast<Scene *>(ud)->act().mob_kinds, name, nodes);
}

int host_mob_drop(void *ud, const char *name, const char *item, int max)
{
    return ecs::mob_drop(static_cast<Scene *>(ud)->act().mob_kinds, name, item, max);
}

int host_mob_lays(void *ud, const char *name, const char *item, double min_s, double max_s)
{
    return ecs::mob_lays(static_cast<Scene *>(ud)->act().mob_kinds, name, item, min_s, max_s);
}

int host_mob_breed(void *ud, const char *name, const char *item, int baby_model, double grow,
                   int hearts_model)
{
    return ecs::mob_breed(static_cast<Scene *>(ud)->act().mob_kinds, name, item, baby_model, grow,
                          hearts_model);
}

int host_spawn_mob(void *ud, const char *name, double x, double y, double z)
{
    Scene *s = static_cast<Scene *>(ud);
    if (s->drops == nullptr || name == nullptr) return 0;
    find_mob_drops(s->act());
    const std::vector<ecs::MobKind> &kinds = s->act().mob_kinds;
    for (size_t i = 0; i < kinds.size(); ++i)
        if (kinds[i].name == name) {
            const flecs::entity e = ecs::spawn_mob(*s->drops, kinds, (uint16_t)i, (float)x,
                                                   (float)y, (float)z, s->drop_clock);
            return e.is_valid() ? (int)(uint32_t)e.id() : 0;
        }
    return 0;
}

int host_mob_goto(void *ud, int id, int x, int y, int z)
{
    Scene *s = static_cast<Scene *>(ud);
    return s->drops ? ecs::mob_goto(*s->drops, s->act().mob_kinds, (uint32_t)id, x, y, z) : 0;
}

int host_mob_going(void *ud, int id)
{
    Scene *s = static_cast<Scene *>(ud);
    return s->drops ? ecs::mob_going(*s->drops, (uint32_t)id) : 0;
}

int host_mob_act(void *ud, int id, const char *clip, const char *item)
{
    Scene *s = static_cast<Scene *>(ud);
    if (!s->drops) return 0;
    const ItemId held = item && *item ? s->act().items.find(item) : kNoItem;
    return ecs::mob_act(*s->drops, s->act().mob_kinds, (uint32_t)id, clip, held);
}

int host_mob_place(void *ud, int id, double x, double y, double z)
{
    Scene *s = static_cast<Scene *>(ud);
    return s->drops ? ecs::mob_place(*s->drops, (uint32_t)id, (float)x, (float)y, (float)z) : 0;
}

int host_mob_remove(void *ud, int id)
{
    Scene *s = static_cast<Scene *>(ud);
    return s->drops ? ecs::mob_remove(*s->drops, (uint32_t)id) : 0;
}

int host_mob_face(void *ud, int id, double x, double z)
{
    Scene *s = static_cast<Scene *>(ud);
    return s->drops ? ecs::mob_face(*s->drops, (uint32_t)id, (float)x, (float)z) : 0;
}

const char *host_node_yield(void *ud, int id)
{
    Scene *s = static_cast<Scene *>(ud);
    if (id <= 0 || (size_t)id >= s->act().registry.size()) return "";
    const ItemId item = s->act().items.of_block((BlockId)id);
    return item != kNoItem ? s->act().items.get(item).name.c_str() : "";
}

const char *host_node_name(void *ud, int id)
{
    Scene *s = static_cast<Scene *>(ud);
    if (id < 0 || (size_t)id >= s->act().registry.size()) return "";
    return s->act().registry.get((BlockId)id).name.c_str();
}


// ------------------------------------------------------------- event glue
// The bus lives in the mod host; these are the three host functions a VM calls
// to reach it. Owner comes from whichever mod is currently running, so a mod
// cannot declare an event in somebody else's name.

int host_event_define(void *ud, const char *event, const script_field *fields, int count,
                      int cancellable, char *err, size_t err_size)
{
    Scene *s = static_cast<Scene *>(ud);

    EventSchema schema;
    schema.name        = event ? event : "";
    schema.cancellable = cancellable != 0;
    schema.owner       = mod_host_current() ? mod_host_current()->name : "core";
    for (int i = 0; i < count; ++i) schema.fields.push_back(EventField{fields[i].name, fields[i].type});

    std::string e;
    if (!s->act().mods.events().define(schema, e)) {
        std::snprintf(err, err_size, "%s", e.c_str());
        return 0;
    }
    return 1;
}

int host_event_on(void *ud, const char *event, int priority, int handler_ref,
                  char *err, size_t err_size)
{
    Scene *s = static_cast<Scene *>(ud);
    const std::string owner = mod_host_current() ? mod_host_current()->name : "core";

    std::string e;
    if (!s->act().mods.events().on(event ? event : "", priority, mod_host_current_vm(), handler_ref,
                             owner, e)) {
        std::snprintf(err, err_size, "%s", e.c_str());
        return 0;
    }
    return 1;
}

int host_event_emit(void *ud, const char *event, script_field *fields, int count,
                    int *out_cancelled, char *err, size_t err_size)
{
    Scene *s = static_cast<Scene *>(ud);

    bool        cancelled = false;
    std::string e;
    if (!s->act().mods.events().emit(event ? event : "", fields, count, &cancelled, e)) {
        std::snprintf(err, err_size, "%s", e.c_str());
        return 0;
    }
    if (out_cancelled) *out_cancelled = cancelled ? 1 : 0;
    return 1;
}

// The events the engine itself emits. Declared here rather than hidden in C++:
// the core uses the same bus, with the same schemas, as any mod (DESIGN.md 6.3
// -- "il core stesso emette solo eventi, nessun path speciale").
void define_core_events(Scene &s)
{
    const std::vector<EventField> block_fields = {
        {"x", SCRIPT_V_NUMBER}, {"y", SCRIPT_V_NUMBER}, {"z", SCRIPT_V_NUMBER},
        {"id", SCRIPT_V_NUMBER}, {"player", SCRIPT_V_NUMBER},
    };

    const struct { const char *name; bool cancellable; } defs[] = {
        {"core:pre_place", true},
        {"core:placed",    false},
        {"core:pre_break", true},
        {"core:broken",    false},
    };

    std::string err;
    for (const auto &d : defs) {
        EventSchema schema;
        schema.name        = d.name;
        schema.owner       = "core";
        schema.fields      = block_fields;
        schema.cancellable = d.cancellable;
        if (!s.act().mods.events().define(schema, err))
            std::fprintf(stderr, "core event '%s': %s\n", d.name, err.c_str());
    }

    // A right click on a block, whatever is in the hand. Cancellable: a mod
    // that cancels has claimed the click and the placement behind it does not
    // happen -- see net::EditPlan::use. The same schema the server declares,
    // extra field and all: an event that carries one thing on a server and
    // another on a client is two games.
    EventSchema used;
    used.name        = "core:used";
    used.owner       = "core";
    used.fields      = block_fields;
    used.fields.push_back({"held", SCRIPT_V_STRING});
    used.cancellable = true;
    if (!s.act().mods.events().define(used, err))
        std::fprintf(stderr, "core event 'core:used': %s\n", err.c_str());

    // A button the player pressed while playing (TODO.md OPEN-49): the mouse
    // buttons, R and C. Client only -- the server never sees a key, it sees what
    // a mod's client half decided to send. Cancellable: a handler that cancels
    // a mouse button has claimed it, and the engine does not dig, hit, place or
    // use with that press. R and C are heard and not claimable: the engine has
    // no use of its own for them to give up.
    // A block core.fall let go of, come to rest, as on the server.
    EventSchema landed;
    landed.name   = "core:landed";
    landed.owner  = "core";
    landed.fields = {{"x", SCRIPT_V_NUMBER}, {"y", SCRIPT_V_NUMBER}, {"z", SCRIPT_V_NUMBER},
                     {"id", SCRIPT_V_NUMBER}, {"fell", SCRIPT_V_NUMBER}};
    if (!s.act().mods.events().define(landed, err))
        std::fprintf(stderr, "core event 'core:landed': %s\n", err.c_str());

    EventSchema input;
    input.name        = "core:input";
    input.owner       = "core";
    input.fields      = {{"action", SCRIPT_V_STRING}, {"player", SCRIPT_V_NUMBER}};
    input.cancellable = true;
    if (!s.act().mods.events().define(input, err))
        std::fprintf(stderr, "core event 'core:input': %s\n", err.c_str());

    // The world is as it is going to be. On a server this is what separates a
    // mod loading from a mod looking at the world file, which is opened after
    // the mods have registered their blocks. Here there is no file and the
    // world is generated before any of this runs -- but a mod is one mod, and
    // an event that fires on a server and not on a client is two games.
    EventSchema ready;
    ready.name  = "core:world_ready";
    ready.owner = "core";
    if (!s.act().mods.events().define(ready, err))
        std::fprintf(stderr, "core event 'core:world_ready': %s\n", err.c_str());

    // What arrived on a mod's own channel. Declared here even in a process that
    // is not connected to anything, for the reason core:ui_click is declared on
    // a server that cannot emit one: a mod that listens for an event nobody
    // declared does not load at all, and a mod that runs on a server and not on
    // a client is two games.
    EventSchema said;
    said.name   = "core:net_message";
    said.owner  = "core";
    said.fields = {{"player", SCRIPT_V_NUMBER},
                   {"mod", SCRIPT_V_STRING},
                   {"text", SCRIPT_V_STRING}};
    if (!s.act().mods.events().define(said, err))
        std::fprintf(stderr, "core event 'core:net_message': %s\n", err.c_str());

    // Rooms of a server only; declared so a mod listening for it loads here.
    EventSchema far;
    far.name   = "core:far_moved";
    far.owner  = "core";
    far.fields = {{"index", SCRIPT_V_NUMBER},  {"x", SCRIPT_V_NUMBER},
                  {"y", SCRIPT_V_NUMBER},      {"z", SCRIPT_V_NUMBER},
                  {"axis", SCRIPT_V_NUMBER},   {"sign", SCRIPT_V_NUMBER},
                  {"half_w", SCRIPT_V_NUMBER}, {"half_h", SCRIPT_V_NUMBER}};
    if (!s.act().mods.events().define(far, err))
        std::fprintf(stderr, "core event 'core:far_moved': %s\n", err.c_str());

    EventSchema tick;
    tick.name   = "core:tick";
    tick.owner  = "core";
    tick.fields = {{"dt", SCRIPT_V_NUMBER}};
    if (!s.act().mods.events().define(tick, err))
        std::fprintf(stderr, "core event 'core:tick': %s\n", err.c_str());

    // A click on a mod's panel comes back the same way everything else does:
    // as an event on the same bus, with no special path. Not cancellable --
    // there is nothing to cancel, the click has already happened.
    EventSchema click;
    click.name   = "core:ui_click";
    click.owner  = "core";
    click.fields = {{"panel", SCRIPT_V_STRING},
                    {"id", SCRIPT_V_STRING},
                    {"player", SCRIPT_V_NUMBER},
                    {"text", SCRIPT_V_STRING}};   // a field's, on Enter; "" for a button
    if (!s.act().mods.events().define(click, err))
        std::fprintf(stderr, "core event 'core:ui_click': %s\n", err.c_str());

    // A blow the player landed on a body; emitted by this process only when it
    // is its own server, and declared either way (see core:net_message).
    EventSchema hit;
    hit.name   = "core:hit";
    hit.owner  = "core";
    hit.fields = {{"player", SCRIPT_V_NUMBER},
                  {"target", SCRIPT_V_NUMBER},
                  {"damage", SCRIPT_V_NUMBER},
                  {"killed", SCRIPT_V_BOOL}};
    if (!s.act().mods.events().define(hit, err))
        std::fprintf(stderr, "core event 'core:hit': %s\n", err.c_str());

    // A right click on a worker creature, as the server says it.
    EventSchema used_mob;
    used_mob.name   = "core:used_mob";
    used_mob.owner  = "core";
    used_mob.fields = {{"player", SCRIPT_V_NUMBER}, {"target", SCRIPT_V_NUMBER}, {"held", SCRIPT_V_STRING}};
    if (!s.act().mods.events().define(used_mob, err))
        std::fprintf(stderr, "core event 'core:used_mob': %s\n", err.c_str());

    // A /command the player typed, to the mod that declared it (core.command).
    EventSchema command;
    command.name   = "core:command";
    command.owner  = "core";
    command.fields = {{"player", SCRIPT_V_NUMBER}, {"name", SCRIPT_V_STRING}, {"args", SCRIPT_V_STRING}};
    if (!s.act().mods.events().define(command, err))
        std::fprintf(stderr, "core event 'core:command': %s\n", err.c_str());

    // Pages (core.page_register), opened with /page (local_chat).
    EventSchema page_open;
    page_open.name   = "core:page_open";
    page_open.owner  = "core";
    page_open.fields = {{"player", SCRIPT_V_NUMBER}, {"page", SCRIPT_V_STRING}};
    s.act().mods.events().define(page_open, err);
    EventSchema page_action;
    page_action.name   = "core:page_action";
    page_action.owner  = "core";
    page_action.fields = {{"player", SCRIPT_V_NUMBER}, {"page", SCRIPT_V_STRING},
                          {"id", SCRIPT_V_STRING}, {"text", SCRIPT_V_STRING}};
    s.act().mods.events().define(page_action, err);

    EventSchema watched;
    watched.name   = "core:watched";
    watched.owner  = "core";
    watched.fields = {{"x", SCRIPT_V_NUMBER}, {"y", SCRIPT_V_NUMBER}, {"z", SCRIPT_V_NUMBER}};
    if (!s.act().mods.events().define(watched, err))
        std::fprintf(stderr, "core event 'core:watched': %s\n", err.c_str());
}

constexpr const char *kLocalPage = "core:page";   // the page's panel, and its owner
void local_page_click(Scene &s, const std::string &id, const std::string &text);

// A click on a panel, back to the mod that declared it.
void emit_ui_click(Scene &s, const std::string &panel, const std::string &id,
                   const std::string &text = "")
{
    if (panel == kLocalPage) {
        local_page_click(s, id, text);
        return;
    }
    script_field fields[4] = {
        {"panel",  SCRIPT_V_STRING, 0.0,                        panel.c_str(), 0},
        {"id",     SCRIPT_V_STRING, 0.0,                        id.c_str(),    0},
        {"player", SCRIPT_V_NUMBER, (double)ui::kLocalPlayer,   nullptr,       0},
        {"text",   SCRIPT_V_STRING, 0.0,                        text.c_str(),  0},
    };

    bool        cancelled = false;
    std::string err;
    if (!s.act().mods.events().emit("core:ui_click", fields, 4, &cancelled, err))
        std::fprintf(stderr, "emit core:ui_click: %s\n", err.c_str());
}

void stop_typing(Scene &s)
{
    s.typing = Scene::Typing{};
    plat_text_input(false);
}

// A left click on a mod's panel: in a field, which takes the keyboard, or on a
// button, whose id comes back to be sent. Anywhere takes the keyboard back
// from the field that had it -- clicking away is how a line is left unsent.
std::string click_panel(Scene &s, const ui::Panel &p, float mx, float my)
{
    const ui::PanelPlacement at  = ui::place_panel(p, gfx_screen_width(), gfx_screen_height());
    const int                hit = at.button_at(p, mx, my);
    stop_typing(s);
    if (hit < 0) return "";

    const ui::Element &e = p.elements[(size_t)hit];
    if (e.kind != ui::ElemKind::Field) return e.id;
    s.typing.on    = true;
    s.typing.panel = p.name;
    s.typing.id    = e.id;
    s.typing.text  = e.text;
    plat_text_input(true);
    return "";
}

// This frame's typing and Backspace, into `line`. Latin-1 only, the characters
// the HUD's font has -- enough for a name in Italian.
void type_line(std::string &line, size_t cap)
{
    for (int c = plat_char_pressed(); c != 0; c = plat_char_pressed()) {
        if (!((c >= 32 && c < 127) || (c >= 160 && c < 256))) continue;
        const std::string ch = c < 128 ? std::string(1, (char)c)
                                       : std::string{(char)(0xC0 | (c >> 6)),
                                                     (char)(0x80 | (c & 0x3F))};
        if (line.size() + ch.size() <= cap) line += ch;
    }
    if (plat_key_repeated(PLAT_KEY_BACKSPACE) && !line.empty()) {
        size_t cut = line.size() - 1;   // back over a whole character, not a byte of one
        while (cut > 0 && ((uint8_t)line[cut] & 0xC0) == 0x80) --cut;
        line.erase(cut);
    }
}

// The chat's frame, for both loops. T or / opens it while the player holds the
// world; open, it has the keyboard and the cursor is free, so nothing reaches
// the world. ESC shuts it and is spent. True on Enter with something typed:
// `out` is then the line to say.
bool chat_frame(Scene &s, LookSettle &settle, bool &esc, std::string &out)
{
    ui::Chat  &c    = s.chat;
    const auto shut = [&](bool regrab) {
        c.open = false;
        c.line.clear();
        c.back = -1;
        plat_text_input(false);
        if (regrab) {
            plat_cursor_grab(true);
            settle.grabbed();
        }
    };

    if (!c.open) {
        // Only from the world: a free cursor belongs to a bag, a panel or the
        // pause menu, and a field that is being typed in owns the characters.
        if (!plat_cursor_grabbed() || s.typing.on) return false;
        for (int ch = plat_char_pressed(); ch != 0; ch = plat_char_pressed()) {
            if (ch != 't' && ch != 'T' && ch != '/') continue;
            c.open = true;
            c.line = ch == '/' ? "/" : "";
            plat_text_input(true);
            plat_cursor_grab(false);
            while (plat_char_pressed() != 0) {}   // the rest of this frame's keys
            break;
        }
        return false;
    }

    // Something else took the screen -- a mod's panel, the pause menu on focus
    // loss -- and it keeps the cursor it has.
    if (s.pause.up || s.act().panels.topmost_for(ui::kLocalPlayer)) {
        shut(false);
        return false;
    }
    if (esc) {
        esc = false;
        shut(true);
        return false;
    }

    type_line(c.line, net::kMaxChatLine);
    const int n = (int)c.sent.size();
    if (n > 0 && plat_key_repeated(PLAT_KEY_UP)) {
        c.back = c.back < 0 ? n - 1 : std::max(c.back - 1, 0);
        c.line = c.sent[(size_t)c.back];
    }
    if (c.back >= 0 && plat_key_repeated(PLAT_KEY_DOWN)) {
        c.back = c.back + 1 < n ? c.back + 1 : -1;
        c.line = c.back < 0 ? "" : c.sent[(size_t)c.back];
    }
    if (!plat_key_pressed(PLAT_KEY_ENTER)) return false;

    std::string line = c.line;
    line.erase(0, line.find_first_not_of(' '));
    line.erase(line.find_last_not_of(' ') + 1);
    shut(true);
    if (line.empty()) return false;
    if (c.sent.empty() || c.sent.back() != line) c.sent.push_back(line);
    if (c.sent.size() > ui::kChatKeep) c.sent.erase(c.sent.begin());
    out = line;
    return true;
}

// The single-process player's page (src/core/ui/page.h), as cnp_server shows
// one (show_page there).
void show_local_page(Scene &s, Scene::PageUp up)
{
    up.layout = ui::layout_page(kLocalPage, kLocalPage, up.title, up.markup, up.screen);
    up.screen = up.layout.screen;
    ui::PanelSet &ps = s.act().panels;
    std::string   err;
    bool ok = ps.define(kLocalPage, kLocalPage, up.layout.panel.w, up.layout.panel.h, err);
    for (const ui::Element &e : up.layout.panel.elements) ok = ok && ps.add(kLocalPage, kLocalPage, e, err);
    ok = ok && ps.show(kLocalPage, kLocalPage, true, ui::kLocalPlayer, err);
    if (!ok) {
        std::fprintf(stderr, "page '%s': %s\n", up.name.c_str(), err.c_str());
        return;
    }
    s.page_up = std::move(up);
}

void close_local_page(Scene &s)
{
    s.page_up.reset();
    s.act().panels.drop_owner(kLocalPage);
}

// The engine's pages here, or a mod's, whose mod hears core:page_open.
bool open_local_page(Scene &s, const std::string &name, int screen = 0)
{
    using ui::page_text;
    Scene::PageUp up;
    up.name   = name;
    up.owner  = "core";
    up.screen = screen;
    if (name == "pages") {
        up.title  = "Pages";
        up.markup = "[Mods](page:mods)\n";
        if (!s.pages.empty()) up.markup += "## From the mods\n";
        for (const auto &[n, d] : s.pages) up.markup += "[" + page_text(d.second) + "](page:" + n + ") ";
        up.markup += "\n> Administration is a server's: a world of one player is its owner's.\n";
    } else if (name == "mods") {
        up.title  = "Mods in this world";
        up.markup = "| mod | version | |\n";
        for (const Mod &mod : s.act().mods.mods())
            up.markup += "| " + page_text(mod.manifest.name) + " | " + mod.manifest.version.str() +
                         " | " + (mod.running ? "running" : page_text("stopped: " + mod.error)) + " |\n";
    } else {
        const auto it = s.pages.find(name);
        if (it == s.pages.end()) return false;
        if (s.page_up && s.page_up->name != name) s.page_up.reset();
        script_field fields[2] = {
            {"player", SCRIPT_V_NUMBER, (double)ui::kLocalPlayer, nullptr, 0},
            {"page", SCRIPT_V_STRING, 0.0, name.c_str(), 0},
        };
        std::string err;
        if (!s.act().mods.events().emit("core:page_open", fields, 2, nullptr, err, &it->second.first))
            std::fprintf(stderr, "emit core:page_open: %s\n", err.c_str());
        return true;
    }
    show_local_page(s, std::move(up));
    return true;
}

void local_chat(Scene &s, const std::string &line);

// A click on the page: cnp_server's page_click.
void local_page_click(Scene &s, const std::string &id, const std::string &text)
{
    if (!s.page_up) return;
    Scene::PageUp up = *s.page_up;   // a copy: what runs below may replace it
    if (id == ui::kPanelEscape || id == ui::kPageClose) {
        close_local_page(s);
        return;
    }
    if (id == ui::kPagePrev || id == ui::kPageNext) {
        up.screen += id == ui::kPagePrev ? -1 : 1;
        show_local_page(s, std::move(up));
        return;
    }
    const ui::PageLink *l = ui::page_link(up.layout, id);
    if (!l) return;
    std::string target = l->target;
    if (l->field)
        for (size_t at; (at = target.find("{text}")) != std::string::npos;) target.replace(at, 6, text);

    if (target.rfind("page:", 0) == 0) {
        if (!open_local_page(s, target.substr(5)))
            s.chat.add("No page '" + target.substr(5) + "' here", s.notice_clock);
    } else if (target.rfind("cmd:", 0) == 0) {
        local_chat(s, target.substr(4));
        if (s.page_up && s.page_up->name == up.name) open_local_page(s, up.name, up.screen);
    } else if (target.rfind("act:", 0) == 0 && up.owner != "core") {
        const std::string act = target.substr(4);
        script_field fields[4] = {
            {"player", SCRIPT_V_NUMBER, (double)ui::kLocalPlayer, nullptr, 0},
            {"page", SCRIPT_V_STRING, 0.0, up.name.c_str(), 0},
            {"id", SCRIPT_V_STRING, 0.0, act.c_str(), 0},
            {"text", SCRIPT_V_STRING, 0.0, l->field ? text.c_str() : "", 0},
        };
        std::string err;
        if (!s.act().mods.events().emit("core:page_action", fields, 4, nullptr, err, &up.owner))
            std::fprintf(stderr, "emit core:page_action: %s\n", err.c_str());
    }
}

// A line the single-process player said: this process is its own server, so
// it answers /help, /list, /page and a mod's command the way cnp_server does.
void local_chat(Scene &s, const std::string &line)
{
    std::string name, args;
    const double now = s.notice_clock;
    if (!net::chat_split(line, name, args)) {
        s.chat.add("<" + g_self_name + "> " + line, now);
    } else if (name == "help") {
        for (const std::string &l : net::chat_help(s.chat_commands)) s.chat.add(l, now);
        s.chat.add("/page [NAME] -- the pages: this world's, the mods'", now);
    } else if (name == "list") {
        s.chat.add("1 in this world: " + g_self_name, now);
    } else if (name == "page") {
        std::string want = args.substr(0, args.find(' '));
        if (want.empty()) want = "pages";
        if (!open_local_page(s, want)) s.chat.add("No page '" + want + "' here -- /page lists them", now);
    } else if (const auto c = s.chat_commands.find(name); c != s.chat_commands.end()) {
        script_field fields[3] = {
            {"player", SCRIPT_V_NUMBER, (double)ui::kLocalPlayer, nullptr, 0},
            {"name", SCRIPT_V_STRING, 0.0, name.c_str(), 0},
            {"args", SCRIPT_V_STRING, 0.0, args.c_str(), 0},
        };
        std::string err;
        if (!s.act().mods.events().emit("core:command", fields, 3, nullptr, err, &c->second.owner))
            std::fprintf(stderr, "emit core:command: %s\n", err.c_str());
    } else {
        s.chat.add("Unknown command /" + name + " -- /help lists them", now);
    }
}

// The open chest's search box (core.container_search): a click in it takes the
// keyboard, a click anywhere else or Enter lets go, and every change is sent at
// once -- a catalogue narrows as it is typed. True with `text` to send.
bool chest_search(Scene &s, std::string &text)
{
    const bool mine = s.typing.on && s.typing.panel == "core:chest";
    if (!s.hotbar.open || !s.hotbar.chest_open || !s.hotbar.chest_search) {
        if (mine) stop_typing(s);
        return false;
    }
    if (plat_mouse_pressed(PLAT_MOUSE_LEFT)) {
        float mx = 0.0f, my = 0.0f;
        plat_mouse_position(&mx, &my);
        if (!s.hotbar.chest_panel.in_field(mx, my)) {
            if (mine) stop_typing(s);
        } else if (!mine) {
            s.typing.on    = true;
            s.typing.panel = "core:chest";
            s.typing.id    = "search";
            s.typing.text  = s.hotbar.chest_query;
            plat_text_input(true);
        }
        return false;
    }
    if (!mine) return false;
    if (plat_key_pressed(PLAT_KEY_ENTER)) {
        stop_typing(s);
        return false;
    }
    const std::string before = s.typing.text;
    type_line(s.typing.text, 64);
    if (s.typing.text == before) return false;
    text                 = s.typing.text;
    s.hotbar.chest_query = text;   // shown now, not a round trip later
    return true;
}

// What the keyboard did this frame to the field that has it. True on Enter:
// `id` and `text` are then what to send, and the field lets go. So does one
// whose panel closed or was declared again without it. Latin-1 only, the
// characters the HUD's font has -- enough for a name in Italian.
bool type_in_field(Scene &s, const ui::Panel *p, std::string &id, std::string &text)
{
    if (!s.typing.on) return false;

    bool still = p != nullptr && p->name == s.typing.panel;
    if (still)
        still = std::any_of(p->elements.begin(), p->elements.end(), [&](const ui::Element &e) {
            return e.kind == ui::ElemKind::Field && e.id == s.typing.id;
        });
    if (!still) {
        stop_typing(s);
        return false;
    }

    type_line(s.typing.text, ui::kMaxPanelText);
    if (!plat_key_pressed(PLAT_KEY_ENTER)) return false;

    id   = s.typing.id;
    text = s.typing.text;
    stop_typing(s);
    return true;
}

// The panel a server says is up, put into this client's own PanelSet.
//
// Into the SAME set the single-process client fills from its own mods, and that
// is the point: the drawing and the hit test then have one path whether the mod
// is in this process or on the far side of a socket. A second path for the
// networked panel would be a second place for the button to stop being where it
// looks -- the thing PanelPlacement exists to prevent.
//
// Everything that was there goes first. What the server sends is the whole
// truth about what is up, so a panel left over from an earlier message is a
// panel this client would go on drawing after the mod closed it.
bool adopt_panel(ui::PanelSet &set, const net::PanelShow *ps, std::string &err)
{
    std::vector<std::string> owners;
    for (const ui::Panel &p : set.all()) owners.push_back(p.owner);
    std::sort(owners.begin(), owners.end());
    owners.erase(std::unique(owners.begin(), owners.end()), owners.end());
    for (const std::string &o : owners) set.drop_owner(o);

    if (ps == nullptr) return true;

    if (!set.define(ps->name, ps->owner, ps->w, ps->h, err)) return false;

    for (const net::PanelShow::Elem &e : ps->elements) {
        ui::Element el;
        el.kind = (ui::ElemKind)e.kind;
        el.id   = e.id;
        el.text = e.text;
        el.x    = e.x;
        el.y    = e.y;
        el.w    = e.w;
        el.h    = e.h;
        el.r    = e.r;
        el.g    = e.g;
        el.b    = e.b;

        // Refused rather than skipped. The same PanelSet::add a mod's own call
        // goes through, so a panel that arrives over the wire is held to the
        // rules a local one is -- and a panel with a button missing is worse
        // than no panel, because the player cannot tell.
        if (!set.add(ps->name, ps->owner, el, err)) return false;
    }

    // To the one viewer this process has. The server already decided this
    // client should see it; there is nobody else here to decide about.
    return set.show(ps->name, ps->owner, true, ui::kLocalPlayer, err);
}

// Emits one of the block events. Returns false if a handler cancelled it, which
// is the whole reason pre_place and pre_break exist.
// Which inventory on the wire a grid on the screen is.
//
// One place, because the drawing names grids and the protocol names sides and
// the two have to agree; written at each call site it would be a mapping that
// drifts, and a drag would move something out of a grid nobody was looking at.
net::Side side_of(ui::Grid g)
{
    switch (g) {
    case ui::Grid::Chest: return net::Side::Container;
    case ui::Grid::Craft: return net::Side::Craft;
    case ui::Grid::Result: return net::Side::Craft;   // taking it is its own request
    case ui::Grid::Bag: break;
    }
    return net::Side::Bag;
}

// core:input, and whether a handler claimed it.
bool emit_input(Scene &s, const char *action)
{
    script_field fields[2] = {
        {"action", SCRIPT_V_STRING, 0.0, action, 0},
        {"player", SCRIPT_V_NUMBER, (double)ui::kLocalPlayer, nullptr, 0},
    };

    bool        cancelled = false;
    std::string err;
    if (!s.act().mods.events().emit("core:input", fields, 2, &cancelled, err))
        std::fprintf(stderr, "emit core:input: %s\n", err.c_str());
    return cancelled;
}

// This frame's presses, told to the mods before the engine acts on them. Both
// loops call it at the same point, before the dig. Sets Scene::left_claimed for
// a claimed left press and clears it when the button is up; returns whether a
// right press was claimed.
bool claim_buttons(Scene &s, bool playing)
{
    if (!plat_mouse_down(PLAT_MOUSE_LEFT)) s.left_claimed = false;
    if (!playing) return false;

    if (plat_mouse_pressed(PLAT_MOUSE_LEFT) && emit_input(s, "mouse_left")) s.left_claimed = true;
    const bool right = plat_mouse_pressed(PLAT_MOUSE_RIGHT) && emit_input(s, "mouse_right");
    if (plat_mouse_released(PLAT_MOUSE_LEFT)) emit_input(s, "mouse_left_up");
    if (plat_mouse_released(PLAT_MOUSE_RIGHT)) emit_input(s, "mouse_right_up");

    // The keys a mod hears, pressed and let go ("key_w", "key_w_up"): heard
    // only -- the engine still walks, flies and opens the bag with them. A mod
    // that wants a key held keeps the press until its release (OPEN-49).
    static const struct { plat_key key; const char *name; } kHeard[] = {
        {PLAT_KEY_W, "key_w"}, {PLAT_KEY_A, "key_a"}, {PLAT_KEY_S, "key_s"},
        {PLAT_KEY_D, "key_d"}, {PLAT_KEY_SPACE, "key_space"}, {PLAT_KEY_SHIFT, "key_shift"},
        {PLAT_KEY_CTRL, "key_ctrl"}, {PLAT_KEY_F, "key_f"}, {PLAT_KEY_R, "key_r"},
        {PLAT_KEY_E, "key_e"}, {PLAT_KEY_Q, "key_q"}, {PLAT_KEY_V, "key_v"},
        {PLAT_KEY_C, "key_c"}, {PLAT_KEY_1, "key_1"}, {PLAT_KEY_2, "key_2"},
        {PLAT_KEY_3, "key_3"}, {PLAT_KEY_4, "key_4"}, {PLAT_KEY_5, "key_5"},
        {PLAT_KEY_6, "key_6"}, {PLAT_KEY_7, "key_7"}, {PLAT_KEY_8, "key_8"},
        {PLAT_KEY_9, "key_9"}, {PLAT_KEY_B, "key_b"}, {PLAT_KEY_F4, "key_f4"},
        {PLAT_KEY_F8, "key_f8"}, {PLAT_KEY_I, "key_i"}, {PLAT_KEY_K, "key_k"},
    };
    s.digit_claimed = false;
    for (const auto &k : kHeard) {
        const bool down = plat_key_down(k.key);
        const bool was  = s.keys_down[k.key];
        if (down && !was && emit_input(s, k.name) && k.key >= PLAT_KEY_1 && k.key <= PLAT_KEY_9)
            s.digit_claimed = true;
        if (!down && was) emit_input(s, (std::string(k.name) + "_up").c_str());
        s.keys_down[k.key] = down;
    }
    return right;
}

bool emit_block_event(Scene &s, const char *event, int x, int y, int z, int id)
{
    if (std::strcmp(event, "core:broken") == 0 || std::strcmp(event, "core:placed") == 0)
        loose_around(s, x, y, z);
    // core:used carries one field more than the others: what the clicker is
    // HOLDING, by name. `id` is the block that was clicked, and a mod reading
    // it to find out what it was being offered was reading itself -- see
    // net::EditPlan::use_holding and docs/archive/M8_FINDINGS.md 1.3.
    //
    // Read from the bag here rather than passed in, because here is the one
    // place that has it: unlike the server, this side's selected slot IS the
    // hand, kept by the hotbar the player is looking at.
    const bool is_use = std::strcmp(event, "core:used") == 0;
    std::string holding;
    if (is_use && s.bag != nullptr) {
        const ItemStack &in_hand = s.bag->held();
        if (!in_hand.empty()) holding = s.act().items.get(in_hand.item).name;
    }

    script_field fields[6] = {
        {"x",  SCRIPT_V_NUMBER, (double)x,  nullptr, 0},
        {"y",  SCRIPT_V_NUMBER, (double)y,  nullptr, 0},
        {"z",  SCRIPT_V_NUMBER, (double)z,  nullptr, 0},
        {"id", SCRIPT_V_NUMBER, (double)id, nullptr, 0},
        // The one player this process has. A mod written for a server reads
        // this field and passes it to ui_show without knowing which it is on.
        {"player", SCRIPT_V_NUMBER, (double)ui::kLocalPlayer, nullptr, 0},
        {"held", SCRIPT_V_STRING, 0.0, holding.c_str(), 0},
    };

    bool        cancelled = false;
    std::string err;
    if (!s.act().mods.events().emit(event, fields, is_use ? 6 : 5, &cancelled, err)) {
        std::fprintf(stderr, "emit %s: %s\n", event, err.c_str());
        return true;   // a broken emit must not silently block the player
    }
    return !cancelled;
}

// Placing and breaking, as the engine does it: through the bus. Both the
// interactive loop and the bench use these, so the demonstration cannot drift
// away from what the game actually runs.
//
// Returns whether the edit happened. `want` may be rewritten by a handler --
// that is the difference between an event you can modify and a notification.
bool try_place(Scene &s, int x, int y, int z, BlockId want, uint8_t facing,
               std::string *why = nullptr)
{
    script_field fields[5] = {
        {"x",  SCRIPT_V_NUMBER, (double)x, nullptr, 0},
        {"y",  SCRIPT_V_NUMBER, (double)y, nullptr, 0},
        {"z",  SCRIPT_V_NUMBER, (double)z, nullptr, 0},
        {"id", SCRIPT_V_NUMBER, (double)want, nullptr, 0},
        {"player", SCRIPT_V_NUMBER, (double)ui::kLocalPlayer, nullptr, 0},
    };

    bool        cancelled = false;
    std::string err;
    if (!s.act().mods.events().emit("core:pre_place", fields, 5, &cancelled, err)) {
        std::fprintf(stderr, "emit core:pre_place: %s\n", err.c_str());
        cancelled = false;   // a broken emit must not silently block the player
    }
    if (cancelled) {
        // The same sentence the networked server sends, from the same function.
        // A player told two different things about one rule is a player
        // learning two games -- see net::refused_by_mod.
        if (why) *why = net::refused_by_mod(false);
        return false;
    }

    const BlockId id = (BlockId)fields[3].num;

    // A track piece: on the grid, its entry over the cell aimed at, all of its
    // cells or none (core/track/piece.h).
    if (const NodeDef &def = s.act().registry.get(id); def.piece.plan != 0) {
        int ox, oz;
        piece_origin(def.piece, facing, x, z, ox, oz);
        if (!put_piece_here(s, id, facing, ox, y, oz)) {
            if (why) *why = "a track piece cannot go over another one";
            return false;
        }
        emit_block_event(s, "core:placed", ox, y, oz, (int)id);
        return true;
    }

    // Through the keeper like a mod's write, so the columns the relight below
    // reads have their blocks back (OPEN-38).
    if (s.act().keeper) s.act().keeper->wrote(x, z, s.act().column_clock);
    s.act().world.set_block(x, y, z, id);
    note_watched(s.act(), x, y, z);

    // The turn goes on the CELL, and it is written even when it is 0: a cell
    // keeps its turn until something says otherwise, so a placement that did
    // not say would inherit the turn of whatever stood there before it.
    s.act().world.set_facing(x, y, z, facing);
    relight_around(s.act().world, s.act().registry, x, y, z);
    emit_block_event(s, "core:placed", x, y, z, (int)id);
    return true;
}

bool try_break(Scene &s, int x, int y, int z)
{
    const int id = (int)s.act().world.get_block(x, y, z);
    if (!s.act().registry.is_breakable((BlockId)id)) return false;
    if (!emit_block_event(s, "core:pre_break", x, y, z, id)) return false;

    // Any cell of a track piece takes the whole piece.
    int ox, oy, oz;
    if (piece_find(s.act().world, s.act().registry, x, y, z, ox, oy, oz)) {
        const BlockId          piece = s.act().world.get_block(ox, oy, oz);
        std::vector<PieceCell> cells;
        piece_cells(s.act().registry.get(piece).piece, piece, s.act().world.get_facing(ox, oy, oz),
                    ox, oy, oz, cells);
        for (const PieceCell &c : cells) {
            if (s.act().world.get_block(c.x, c.y, c.z) != c.id) continue;
            if (s.act().keeper) s.act().keeper->wrote(c.x, c.z, s.act().column_clock);
            s.act().world.set_block(c.x, c.y, c.z, kAir);
            relight_around(s.act().world, s.act().registry, c.x, c.y, c.z);
        }
        emit_block_event(s, "core:broken", ox, oy, oz, (int)piece);
        return true;
    }

    if (s.act().keeper) s.act().keeper->wrote(x, z, s.act().column_clock);
    s.act().world.set_block(x, y, z, kAir);
    note_watched(s.act(), x, y, z);
    relight_around(s.act().world, s.act().registry, x, y, z);
    emit_block_event(s, "core:broken", x, y, z, id);
    return true;
}

// The same two actions, but with a bag on the other end.
//
// Kept apart from try_place/try_break above rather than growing an Inventory
// parameter on them, because the two have different callers and one of them is
// not a player: a mod placing a block, and the bench that measures placement,
// spend nothing and gain nothing. Folding the bag into those would mean either
// giving the engine an inventory of its own or passing a dummy one, and a dummy
// inventory is a rule nobody is really following.
//
// DESIGN.md 3.4 says inventories are server-side only. There is no server here
// -- this is the single-process client, which is its own authority -- and the
// point of putting the rule in core/ is that the networked path (M7's next
// piece) runs the same code rather than a second copy that has to agree.

// Places what the hand is holding, and pays for it. Returns whether a block
// went down.
bool place_from_bag(Scene &s, Inventory &bag, int x, int y, int z, Vec3 placer,
                    std::string *why = nullptr)
{
    const ItemStack held = bag.held();
    if (held.empty()) return false;

    BlockId want = s.act().items.get(held.item).places;
    if (want == kAir) return false;    // a tool, or an item whose block was dropped by a reload

    // A block with a front is turned to face whoever put it down. From the
    // PLACER'S POSITION and not from where they were looking, because the
    // server has the first and not the second -- and one rule told two ways is
    // two games. The CELL is turned and not the id, since OPEN-27, and a block
    // with no front is left at 0 rather than turned to face nobody.
    //
    // The face somebody clicked decides a WALLMOUNTED block's byte, and this
    // path does not have one: place_from_bag is handed a cell and not a ray.
    // The face of the cell nearest the placer is the honest answer for it --
    // and the shared rule is asked either way, so the two paths cannot drift.
    const NodeTurn turn = s.act().registry.get(want).turn;
    uint8_t        facing =
        node_placement_facing(turn, (float)x + 0.5f - placer.x, (float)z + 0.5f - placer.z,
                              0, 1, 0);
    if (s.act().registry.get(want).piece.plan != 0) facing = (uint8_t)((facing + s.piece_turn) & 3);

    // Spent only if it actually went down. A mod may cancel the placement, and
    // paying for a placement a handler refused is the defect this ordering
    // exists to make impossible.
    if (!try_place(s, x, y, z, want, facing, why)) return false;

    bag.spend_held(1);
    return true;
}

// Breaks a block and puts its item form in the bag. Returns whether the block
// broke, which is NOT the same as whether anything was picked up: a full bag
// still breaks the block, and the drop is lost. That is a decision and not an
// oversight -- the alternative is a player who cannot dig because their bag is
// full, and a world where a block sometimes refuses to break for a reason
// nothing on screen explains.
// How long the cell under the crosshair takes in the hand that is holding the
// button. The block's seconds divided by the tool's speed -- net::dig_seconds,
// the same call the server makes, so the bar this fills and the moment the
// block comes away are the same number on both sides.
double dig_need(const NodeRegistry &reg, const ItemRegistry &items, BlockId block,
                const ItemStack &held, float factor = 1.0f)
{
    // `factor`: the body's own (core.player_factors, BodyMovement::dig).
    const float speed =
        held.empty() ? 1.0f : net::tool_speed_for(items.get(held.item), reg.get(block).material, held.ench);
    return net::dig_seconds(reg.get(block).hardness, speed * factor);
}

bool break_into_bag(Scene &s, Inventory &bag, int x, int y, int z, std::string *why = nullptr)
{
    const BlockId was = s.act().world.get_block(x, y, z);

    // What is in the hand, before anything else asks. The same rule the
    // networked server applies in plan_edit, from the same function, so the two
    // refuse the same digs with the same sentence -- see net::tool_digs.
    //
    // Here and not in try_break, because try_break is the ENGINE breaking a
    // block: a mod's write and the bench spend nothing and hold nothing. A
    // level is about the player's hand, so it lives on the player's path.
    //
    // After nothing: a block that does not break at all is refused by try_break
    // below, and it must not be refused HERE with a sentence telling the player
    // to go and find a better tool for the floor of the world.
    if (s.act().registry.is_breakable(was)) {
        const ItemStack   &tool     = bag.held();
        const std::string &material = s.act().registry.get(was).material;
        const uint8_t      level =
            tool.empty() ? (uint8_t)0
                         : net::tool_level_for(s.act().items.get(tool.item), material);
        const uint8_t needs = s.act().registry.get(was).dig_level;
        if (!net::tool_digs(level, needs)) {
            if (why) *why = net::refused_for_tool(needs, level, material);
            return false;
        }
    }

    if (!try_break(s, x, y, z)) return false;

    // A use of the tool that dug it, as on the server (EditPlan::wears).
    if (s.act().registry.get(was).hardness > 0.0f)
        bag.wear_slot(s.act().items, net::best_dig_slot(s.act().items, bag.slots(), bag.selected(),
                                                       s.act().registry.get(was).material),
                      1);

    // The id straight: a block with a front is one block, and the cell carries
    // the turn.
    const ItemId drop = s.act().items.of_block(was);
    if (drop != kNoItem) bag.add(s.act().items, drop, 1);
    return true;
}

// How a lid moves between shut and thrown open.
//
// A quarter of a second, which is long enough to be seen and short enough that
// a chest opened and shut again reads as one gesture. The world says only
// WHICH cells are open; this is the picture between the two, and it runs on
// whatever frame rate this machine has.
//
// `is_open` is asked for the cells that are already moving and for the ones the
// caller names as open, so a chest nobody has touched costs nothing.
void advance_lids(Scene &s, double dt, const std::vector<Cell> &open_now)
{
    constexpr float kOpensIn = 4.0f;   // 1/0.25 s

    for (const Cell &at : open_now) s.open_angle.emplace(at, 0.0f);

    for (auto it = s.open_angle.begin(); it != s.open_angle.end();) {
        bool open = false;
        for (const Cell &at : open_now) open = open || at == it->first;

        const float step = (float)dt * kOpensIn;
        it->second       = open ? (it->second + step > 1.0f ? 1.0f : it->second + step)
                                : (it->second - step < 0.0f ? 0.0f : it->second - step);

        // Shut and still: forgotten, so the table is the chests in use and not
        // every chest the player has ever walked past.
        if (!open && it->second <= 0.0f) it = s.open_angle.erase(it);
        else ++it;
    }
}

// How a cell that wears a model is drawn (TODO.md OPEN-43, M11's other
// drawtype). The world renderer knows about an atlas and a chunk and nothing
// about glTF, so it hands out the model's id and the place, and this is where
// that turns back into a model: the session's own table, the same one a body
// is drawn through.
//
// A model this client has not got draws nothing, and the cell falls back to its
// boxes -- the renderer's own rule. Never a cube: a cube is the one thing that
// block is not.
void wire_model_draw(Scene &s, int world)
{
    s.at(world).world_render.set_model_draw(
        [&s, world](ModelId id, const Mat4 &place, float open, float light,
                    const Mat4 &projection, const Mat4 &view) {
            ModelStore::Entry *e = s.at(world).models.entry(id);
            if (e == nullptr) return;

            // Two clips a block's model may have, by name, and nothing else:
            //   "open"  held at how far open the cell is -- a chest's lid, which
            //           the world says is open or shut and advance_lids swings;
            //   "loop"  played round and round on the clock -- a furnace's fire.
            // Neither, and it is its bind pose: a block does not walk.
            const SkinnedModel &m = e->model;
            const int opens = m.clip("open"), loops = m.clip("loop");
            if (opens < 0 && loops < 0) {
                e->renderer.draw(projection, view, place, m.bind_pose, gfx_texture{0}, light);
                return;
            }

            ClipAt layers[2];
            int    n = 0;
            if (loops >= 0 && m.clips[(size_t)loops].duration > 0.0f) {
                const double now = std::chrono::duration<double>(
                                       std::chrono::steady_clock::now().time_since_epoch())
                                       .count();
                layers[n++] = {loops, (float)std::fmod(now, (double)m.clips[(size_t)loops].duration),
                               1.0f};
            }
            if (opens >= 0) layers[n++] = {opens, open * m.clips[(size_t)opens].duration, 1.0f};

            // ponytail: posed per cell per frame; fine for the handful of
            // chests and furnaces in sight, cache by (model, open) if a
            // world ever has hundreds.
            static std::vector<float> bones;
            pose(m, layers, n, nullptr, nullptr, bones);
            e->renderer.draw(projection, view, place, bones, gfx_texture{0}, light);
        });
}

// The query the renderer asks, which is the tween above read back.
void wire_lid_query(Scene &s, int world)
{
    s.at(world).world_render.set_open_query([&s](int x, int y, int z) {
        const auto it = s.open_angle.find(Cell{x, y, z});
        return it == s.open_angle.end() ? 0.0f : it->second;
    });
}

// What a frame may spend on the modifiers. The same two milliseconds the server
// allows, said in one place per program rather than shared: the two have
// different frames to make, and a constant they both read would look like an
// agreement they have not made.
constexpr double kModifierBudgetMs = 2.0;

// How often a mod's writes get their light. A pass around one cell is about
// 3 ms and this loop has 0.6 ms frames, so per write is out of the question and
// per frame nearly so; four times a second is 12 ms of every second, and a lamp
// a mod switched on is lit before anybody has noticed it was not.
constexpr double kLightFlushSeconds = 0.25;

// One tick of one session's modifiers. An action that errors costs its own mod,
// which is the rule every other call into a mod already keeps.
void step_modifiers(Session &sess, double dt)
{
    if (!sess.modifiers.registration_closed()) return;

    sess.modifier_clock += dt;

    sess.modifiers.step(sess.world, sess.modifier_clock, kModifierBudgetMs,
                        [&](const Modifier &m, int x, int y, int z, BlockId id) {
        script_field fields[4] = {
            {"x",  SCRIPT_V_NUMBER, (double)x,  nullptr, 0},
            {"y",  SCRIPT_V_NUMBER, (double)y,  nullptr, 0},
            {"z",  SCRIPT_V_NUMBER, (double)z,  nullptr, 0},
            {"id", SCRIPT_V_NUMBER, (double)id, nullptr, 0},
        };

        char       action_err[256] = {0};
        script_vm *vm              = static_cast<script_vm *>(m.vm);
        sess.mods.enter_vm(vm);
        const bool ok = script_call_handler(vm, m.handler, fields, 4, 0, nullptr, action_err,
                                            sizeof(action_err));
        sess.mods.leave_vm(vm);

        if (!ok) sess.mods.disable_vm(vm, "modifier '" + m.name + "': " + action_err);
        return ok;
    });
}

// The light a mod's writes have earned, at most four times a second.
void flush_mod_light(Session &sess, double dt)
{
    sess.light_flush_due -= dt;
    if (sess.light_flush_due > 0.0) return;

    sess.light_flush_due = kLightFlushSeconds;
    if (sess.light_batch.empty()) return;

    const std::string boxes = sess.light_batch.describe();
    const double      t0    = plat_time();
    const size_t      cells = sess.light_batch.flush(sess.world, sess.registry);
    const double      ms    = (plat_time() - t0) * 1000.0;
    if (ms > 50.0)
        std::printf("light: a flush of mod writes took %.0f ms over %zu cell(s): %s\n", ms, cells,
                    boxes.c_str());
}

// Defined beside main, and used by the connected loop that comes before it
// (TODO.md OPEN-49): the host table every mod in this process is given, and
// whether this player has said no to a world's code.
script_host make_client_host(Scene &scene);
bool        world_code_refused_for(const net::ClientSession &conn, std::string &why);

// How long each part of the last step_script took, in milliseconds: systems,
// on_step, modifiers, the light flush, core:tick. For a frame that stalled to
// say which part it was, rather than "script".
double g_script_phase_ms[5] = {};

void step_script(Scene &s, double dt)
{
    double t = plat_time();
    const auto lap = [&t](int i) {
        const double now     = plat_time();
        g_script_phase_ms[i] = (now - t) * 1000.0;
        t                    = now;
    };

    // A mod that errors is disabled inside here and the rest of the tick
    // carries on, so this call has nothing left to handle.
    // The mods' systems before their on_step, the same order the server uses.
    s.lua_ecs.step(dt, script_call_system);
    lap(0);
    s.act().mods.step(dt);
    lap(1);

    // What happens to cells on their own, with the same budget the server
    // gives them: this loop has a frame to make and a mod that asks for more
    // than two milliseconds of it has to be the one that waits.
    step_modifiers(s.act(), dt);
    lap(2);

    // And the light everything above owes, in one pass for the run of writes
    // rather than one per write.
    flush_mod_light(s.act(), dt);
    lap(3);

    // The tick as an event, for mods that would rather subscribe than define a
    // global. An emit with no handlers is a hash lookup and an empty vector.
    script_field dt_field{"dt", SCRIPT_V_NUMBER, dt, nullptr, 0};
    bool         cancelled = false;
    std::string  err;

    std::vector<std::tuple<int, int, int>> hits;
    hits.swap(s.act().watch_hits);
    for (const auto &[wx, wy, wz] : hits) {
        script_field wf[3] = {{"x", SCRIPT_V_NUMBER, (double)wx, nullptr, 0},
                              {"y", SCRIPT_V_NUMBER, (double)wy, nullptr, 0},
                              {"z", SCRIPT_V_NUMBER, (double)wz, nullptr, 0}};
        s.act().mods.events().emit("core:watched", wf, 3, nullptr, err);
    }

    s.act().mods.events().emit("core:tick", &dt_field, 1, &cancelled, err);
    lap(4);
}

// ------------------------------------------------------------- hot reload
// The mod host knows nothing about registries or chunks, so the two ends of a
// reload live here: mark the mod's blocks, and afterwards either keep what it
// registered again or turn what it dropped into an obvious orphan.

void on_before_reload(void *ud, const char *mod_name)
{
    Scene *s = static_cast<Scene *>(ud);
    const std::string name = mod_name ? mod_name : "";
    s->act().registry.begin_reload(name);
    s->act().items.begin_reload(name);
    s->act().panels.begin_reload(name);
    s->act().recipes.begin_reload(name);
}

void on_after_reload(void *ud, const char *mod_name, bool ok)
{
    Scene *s = static_cast<Scene *>(ud);
    const std::string name = mod_name ? mod_name : "";

    if (!ok) {
        // Nothing was replaced, so nothing was dropped.
        s->act().registry.cancel_reload(name);
        s->act().items.cancel_reload(name);
        s->act().panels.cancel_reload(name);
        s->act().recipes.cancel_reload(name);
        return;
    }

    const size_t orphans = s->act().registry.end_reload(name);
    if (orphans > 0)
        std::printf("mod '%s': %zu block type(s) it no longer defines are now orphans\n",
                    name.c_str(), orphans);

    // The item side of the same drop, and then the adoption again: a block the
    // mod registered THIS time round needs an item form, and one it dropped
    // needs its item form to stop placing a block that is now magenta.
    // Panels the new version stopped declaring. Erased rather than orphaned:
    // a world is not full of panels, so there is nothing left standing that
    // needs a placeholder.
    const size_t panels_gone = s->act().panels.end_reload(name);
    if (panels_gone > 0)
        std::printf("mod '%s': %zu panel(s) it no longer declares are gone\n", name.c_str(),
                    panels_gone);

    // Recipes the new version stopped registering. Removed and not orphaned:
    // nothing in a save file points at a recipe, so there is no id to keep --
    // which is the whole difference from the item table above.
    const size_t recipes_gone = s->act().recipes.end_reload(name);
    if (recipes_gone > 0)
        std::printf("mod '%s': %zu recipe(s) it no longer registers are gone\n", name.c_str(),
                    recipes_gone);

    const size_t item_orphans = s->act().items.end_reload(name, s->act().registry);
    const size_t adopted      = s->act().items.adopt_blocks(s->act().registry);
    if (item_orphans > 0 || adopted > 0)
        std::printf("mod '%s': %zu item(s) can no longer be obtained, %zu new one(s) can\n",
                    name.c_str(), item_orphans, adopted);

    // Colours, light and solidity may all have changed, and the atlas is
    // generated from the registry: everything on screen has to be built again.
    s->act().world_render.rebuild_atlas(s->act().world, s->act().registry, read_texture_file, &s->act().items);

    // A reload can change what emits light and what stops it, so the bulk pass
    // runs again. It costs a fraction of a second on the default world and a
    // reload is not a hot path.
    if (s->act().keeper)
        s->act().keeper->relight();
    else
        compute_lighting(s->act().world, s->act().registry, 0, 0, 0,
                         g_world_extent - 1, kWorldHeight - 1, g_world_extent - 1);
}

// Screenshots go to one directory, created here, whose absolute path is printed
// once. raylib writes relative to the working directory, so running the bench
// from the source root and from the build directory used to leave two sets of
// images with the same names -- and reading the older set is a way to spend an
// afternoon debugging a change that did happen.
void shot_write(const char *name)
{
    std::error_code ec;
    std::filesystem::create_directories("bench", ec);
    plat_screenshot((std::string("bench/") + name).c_str());
}

// One frame of the render graph of DESIGN.md 2.2, shared by the interactive
// loop and the bench so the bench cannot drift into measuring something else.
// `draw_portals` exists for the continuity reference: to compare what a portal
// SHOWS against what is really there, the reference frame has to be the plain
// world from the virtual camera, with no portal of its own in the way.
// Draws every body, cut where it is going through a portal.
//
// The rule is portal_clip.h's: the part that has not gone through is drawn
// where it is, and the rest is drawn as a copy in front of the exit. Two draws
// with a clip plane each, no geometry rebuilt.
//
// Only the FIRST portal a body straddles is used. Two portals close enough for
// one body to be in both at once is a case with no right answer -- the copy
// would need cutting against the second portal, and its copy against the first,
// which is a recursion in the wrong dimension. It is on M4's list rather than
// half-done here.
// The ones wearing something this client actually has. Answers whether it drew
// the body, so the box loops below can leave it alone: a character with a crate
// inside it is what drawing both looks like.
//
// NOT CUT AT A PORTAL, and that is a limit and not an oversight. The box is cut
// with a clip plane the body shader knows about (render/body_render.h) and the
// model shader has no such plane, so a character standing half way through an
// opening is drawn whole. The alternative -- a character that turns back into a
// crate the moment it nears a portal -- is a bigger lie than an uncut one, and
// the honest fix is a clip plane in the model shader. TODO.md OPEN-29.
// A model posed: its clips (animate_body), and its wheels by the names its
// bones go by: steer_* turn about their
// upright, wheel_* roll about their axle and steering_wheel about its column --
// each about its own Y, which is where Blender points a bone (pose). A
// wheel rolls the distance over its radius, and its radius is how high its
// middle stands over the bottom of the model. Every other bone stays bound.
// "tools:pick" is "pick".
std::string item_short_name(const std::string &name)
{
    const size_t colon = name.find(':');
    return colon == std::string::npos ? name : name.substr(colon + 1);
}

// Which tool_* bones a hand shows: the one named for what is held ("tools:pick"
// shows tool_pick), none of the others. Every other bone as posed.
std::vector<int8_t> tools_shown(const SkinnedModel &m, const std::string &hand)
{
    const std::string tool = item_short_name(hand);
    std::vector<int8_t> show((size_t)m.joint_count, -1);
    for (int j = 0; j < m.joint_count; ++j) {
        const std::string &name = m.joint_names[(size_t)j];
        if (name.rfind("tool_", 0) == 0) show[(size_t)j] = name.compare(5, std::string::npos, tool) == 0 ? 1 : 0;
    }
    return show;
}

void wheel_pose(const SkinnedModel &m, const Body &b, std::vector<float> &out)
{
    constexpr float kLock = 0.52f;   // 30 degrees, the wheels at full lock

    std::vector<float> turn((size_t)m.joint_count, 0.0f);
    for (int j = 0; j < m.joint_count; ++j) {
        const std::string &name = m.joint_names[(size_t)j];
        if (name.rfind("steer_", 0) == 0) {
            turn[(size_t)j] = -b.steer * kLock;
        } else if (name.rfind("wheel_", 0) == 0) {
            const float radius = m.joint_rest[(size_t)j].y - m.min.y;
            if (radius > 0.01f) turn[(size_t)j] = -b.travelled / radius;
        } else if (name == "steering_wheel") {
            turn[(size_t)j] = -b.steer * 1.5707963f;   // a quarter turn at full lock
        }
    }
    const std::vector<int8_t> show = b.hand_known ? tools_shown(m, b.hand) : std::vector<int8_t>();
    pose(m, b.clips, 3, turn.data(), show.empty() ? nullptr : show.data(), out);
    // Driving in first person: the arms on the wheel and nothing else. A
    // joint's scale carries its children, and the arms hang from the chest,
    // so the rest is taken out after the pose: every other joint's skinning
    // gathered into a point, which draws nothing.
    if (b.arms_only)
        for (int j = 0; j < m.joint_count; ++j) {
            const std::string &name = m.joint_names[(size_t)j];
            const bool arm = name.rfind("upper_arm_", 0) == 0 || name.rfind("forearm_", 0) == 0 ||
                             name.rfind("hand_", 0) == 0 || name.rfind("tool_", 0) == 0;
            if (!arm) std::fill(out.begin() + (ptrdiff_t)j * 12, out.begin() + (ptrdiff_t)j * 12 + 12, 0.0f);
        }
}

void draw_held_block(HeldCube &cube, Session &w, const SkinnedModel &m,
                     const std::vector<float> &posed, const std::string &item, BlockId block,
                     const Mat4 &projection, const Mat4 &view, const Mat4 &place,
                     const std::string &model = "", uint32_t tag = 0, float light = 1.0f);

// The world's model of that name, or null.
ModelStore::Entry *model_named(ModelStore &models, const std::string &name)
{
    for (size_t id = 1; id <= models.size(); ++id) {   // ponytail: linear, a hundred models
        ModelStore::Entry *e = models.entry((uint16_t)id);
        if (e != nullptr && e->name == name) return e;
    }
    return nullptr;
}
ModelStore::Entry *model_named(Session &w, const std::string &name) { return model_named(w.models, name); }

// A worn piece: the model named for the item, skinned to the wearer's bones by
// their NAMES -- a piece made on the robot's rig follows whatever pose the
// robot is in. A bone the wearer has not got follows its parent, and one named
// sway_* swings about where it is bound as well, a slow pendulum out of step
// from one body to the next (the scarf's ends, a medal: make_robot_extras.py);
// one with no parent stays where it was bound.
//
// A piece with clips of its own plays `clip` on the bones the wearer has not
// got (mods/armpack's arms): each one moved against its parent as the clip
// moves it, and the parent as the wearer is posed. Its bones as drawn go to
// `out`, for what it holds (draw_pack_tools).
void draw_worn(ModelStore &models, const SkinnedModel &body, const std::vector<float> &posed,
               const std::string &name, const Mat4 &projection, const Mat4 &view,
               const Mat4 &place, uint16_t tint, const char *clip = nullptr,
               std::vector<float> *out = nullptr)
{
    ModelStore::Entry *piece = model_named(models, name);
    if (piece == nullptr || !piece->model.skinned()) return;
    const int played = clip != nullptr ? piece->model.clip(clip) : -1;
    static std::vector<float> own;   // the piece in its own clip, whole
    if (played >= 0) {
        const float d   = piece->model.clips[(size_t)played].duration;
        ClipAt      at;
        at.clip = played;
        at.time = d > 0.0f ? std::fmod((float)plat_time(), d) : 0.0f;
        pose(piece->model, &at, 1, nullptr, nullptr, own);
    }

    const SkinnedModel &m = piece->model;
    static std::vector<float> mine;
    static std::vector<char>  done;
    mine = m.bind_pose;
    done.assign((size_t)m.joint_count, 0);
    for (int j = 0; j < m.joint_count; ++j)
        for (int k = 0; k < body.joint_count; ++k)
            if (body.joint_names[(size_t)k] == m.joint_names[(size_t)j]) {
                std::copy(posed.begin() + k * 12, posed.begin() + k * 12 + 12, mine.begin() + j * 12);
                done[(size_t)j] = 1;
                break;
            }
    // The rest, parents first whatever order the file lists them in.
    const auto rows = [](const float *r) {
        Mat4 out = mat4_identity();
        for (int row = 0; row < 3; ++row)
            for (int col = 0; col < 4; ++col) out.at(row, col) = r[row * 4 + col];
        return out;
    };
    const double t     = plat_time();
    const float  phase = place.at(0, 3) * 1.3f + place.at(2, 3) * 0.7f;
    for (bool more = true; more;) {
        more = false;
        for (int j = 0; j < m.joint_count; ++j) {
            const int parent = m.joint_parent[(size_t)j];
            if (done[(size_t)j] || (parent >= 0 && !done[(size_t)parent])) continue;
            done[(size_t)j] = 1;
            more            = true;
            if (parent < 0) continue;
            Mat4 local = mat4_identity();
            if (played >= 0)
                local = mat4_multiply(mat4_invert(rows(&own[(size_t)parent * 12])), rows(&own[(size_t)j * 12]));
            else if (m.joint_names[(size_t)j].rfind("sway_", 0) == 0) {
                const Vec3  at    = m.joint_rest[(size_t)j];
                // Forwards only: what hangs on a chest swings away from it and
                // back to it, never into it (a medal on a flat chest went in).
                const float front = 0.08f * (1.0f + (float)std::sin(t * 2.4 + phase + j));
                const float side  = 0.07f * (float)std::sin(t * 1.7 + phase * 1.9 + j);
                local = mat4_multiply(mat4_translate(at),
                                      mat4_multiply(mat4_multiply(mat4_rotate_x(front), mat4_rotate_z(side)),
                                                    mat4_translate(Vec3{-at.x, -at.y, -at.z})));
            }
            const Mat4 skin = mat4_multiply(rows(&mine[(size_t)parent * 12]), local);
            for (int row = 0; row < 3; ++row)
                for (int col = 0; col < 4; ++col) mine[(size_t)j * 12 + (size_t)(row * 4 + col)] = skin.at(row, col);
        }
    }
    piece->renderer.draw(projection, view, place, mine, gfx_texture{0}, 1.0f, 1.0f,
                         (uint32_t)tint << kTagIdBits);
    if (out != nullptr) *out = mine;
}

// The clip a piece with arms plays (mods/armpack's "armpack_idle" and the
// rest): at work, the arms that hold something strike -- both in turn, or the
// one -- and otherwise they idle.
std::string pack_clip(const std::string &piece, bool working, const std::string arm[2])
{
    const std::string base = item_short_name(piece);
    if (!working || (arm[0].empty() && arm[1].empty())) return base + "_idle";
    return base + (arm[1].empty() ? "_swing_L" : arm[0].empty() ? "_swing_R" : "_work");
}

// What a piece's arms hold, in its claws: at each pack_tool_L / pack_tool_R
// bone of `piece` as posed (`posed`), held as the right hand holds it at rest.
// An item with a "<item>_held" model is that model; a tool of the player
// model's own (a tool_* bone of `hands`, the body or the first-person arms)
// is that bone's part of `hands`, moved from the hand to the claw. A block is
// not drawn.
void draw_pack_tools(ModelStore &models, const SkinnedModel &piece, const std::vector<float> &posed,
                     ModelStore::Entry &hands, const std::string arm[2], const Mat4 &projection,
                     const Mat4 &view, const Mat4 &place, float light, uint32_t tag)
{
    const auto rows = [](const float *r) {
        Mat4 out = mat4_identity();
        for (int row = 0; row < 3; ++row)
            for (int col = 0; col < 4; ++col) out.at(row, col) = r[row * 4 + col];
        return out;
    };
    const auto flat = [](const Mat4 &t, float *r) {
        for (int row = 0; row < 3; ++row)
            for (int col = 0; col < 4; ++col) r[row * 4 + col] = t.at(row, col);
    };
    for (int k = 0; k < 2; ++k) {
        if (arm[k].empty()) continue;
        const std::string bone = k == 0 ? "pack_tool_L" : "pack_tool_R";
        int claw = -1;
        for (int j = 0; j < piece.joint_count; ++j)
            if (piece.joint_names[(size_t)j] == bone) claw = j;
        if (claw < 0 || posed.size() < (size_t)(claw + 1) * 12) continue;
        const Mat4 skin = rows(&posed[(size_t)claw * 12]);
        const Vec3 grip = piece.joint_rest[(size_t)claw];

        if (ModelStore::Entry *held = model_named(models, arm[k] + "_held");
            held != nullptr && !held->model.skinned()) {
            std::vector<float> r(12);
            flat(mat4_multiply(skin, mat4_translate(grip)), r.data());
            held->renderer.draw(projection, view, place, r, gfx_texture{0}, light, 1.0f, 0);
            continue;
        }
        const SkinnedModel &m    = hands.model;
        const std::string   tool = "tool_" + item_short_name(arm[k]);
        for (int j = 0; j < m.joint_count; ++j) {
            if (m.joint_names[(size_t)j] != tool) continue;
            // Every other bone to nothing, and this one's part from where
            // the hand holds it at rest to where the claw does.
            static std::vector<float> r;
            r.assign((size_t)m.joint_count * 12, 0.0f);
            flat(mat4_multiply(skin, mat4_translate(grip - m.joint_rest[(size_t)j])), &r[(size_t)j * 12]);
            hands.renderer.draw(projection, view, place, r, gfx_texture{0}, light, 1.0f, tag);
            break;
        }
    }
}

Mat4 body_place(const Body &b);

// The extras a livery names (net::kLiveryHeadShift), 1 on: the model
// "<player model>_<extra>" (tools/make_robot_extras.py). Their order is the
// wire's -- a new one goes at the end.
const char *const kLiveryHeads[] = {"cap", "tophat", "headphones"};
const char *const kLiveryNecks[] = {"scarf", "bowtie", "medal"};
constexpr int     kLiveryHeadCount = 3, kLiveryNeckCount = 3;

// Who a livery says a player is (net::kLiverySpeciesShift), 1 on: the model
// "<player model>_<species>", on the player model's rig and with its clips,
// and each one's styles, "<that>_<style>" worn on it (tools/make_species.py).
// The wire's order, as the extras'.
const char *const kLiverySpecies[] = {"human", "mole", "frog"};
const char *const kLiveryStyles[3][3] = {
    {"short", "long", "beard"}, {"lamp", "goggles", "glasses"}, {"spots", "crown", "lily"}};
constexpr int kLiverySpeciesCount = 3, kLiveryStyleCount = 3;
// The styles that are hats: an extra on the head is worn instead of them.
constexpr bool kLiveryStyleHat[3][3] = {{false, false, false}, {true, false, false}, {false, true, true}};

int livery_species(uint32_t livery) { return (int)(livery >> net::kLiverySpeciesShift) & 3; }
int livery_style(uint32_t livery) { return (int)(livery >> net::kLiveryStyleShift) & 3; }

// The model a body wearing `model` is drawn as in this livery: the world's
// "<its name>_<species>" when it has one, else `model` -- a kart, a world with
// no such model, a robot.
uint16_t species_model(ModelStore &models, uint16_t model, uint32_t livery)
{
    const int species = livery_species(livery);
    const ModelStore::Entry *e = species >= 1 ? models.entry(model) : nullptr;
    if (e == nullptr) return model;
    const std::string name = e->name + "_" + kLiverySpecies[species - 1];
    for (size_t id = 1; id <= models.size(); ++id) {
        const ModelStore::Entry *s = models.entry((uint16_t)id);
        if (s != nullptr && s->name == name) return (uint16_t)id;
    }
    return model;
}

// A livery's colours as a stack's tag, for a model's tint_1..3.
uint32_t livery_tag(uint32_t livery) { return (livery & net::kLiveryColours) << kTagIdBits; }

// The extras a livery wears, on a body posed as `posed`: none on the head of
// one drawn without it. A species wears its own fit of an extra
// ("robot:robot_mole_cap", tools/make_species.py), else the robot's
// ("robot:robot_cap"), and its style -- a hat style only with no extra on the head.
void draw_extras(ModelStore &models, const ModelStore::Entry &e, const std::vector<float> &posed,
                 uint32_t livery, bool headless, const Mat4 &projection, const Mat4 &view,
                 const Mat4 &place)
{
    const int head = (int)(livery >> net::kLiveryHeadShift) & 7;
    const int neck = (int)(livery >> net::kLiveryNeckShift) & 7;
    const uint16_t tint = (uint16_t)(livery & net::kLiveryColours);
    const int species = livery_species(livery), style = livery_style(livery);
    const bool hat = head >= 1 && head <= kLiveryHeadCount;
    std::string base = e.name;
    if (species >= 1) {
        const std::string tail = std::string("_") + kLiverySpecies[species - 1];
        if (base.size() > tail.size() && base.compare(base.size() - tail.size(), tail.size(), tail) == 0) {
            base.resize(base.size() - tail.size());
            if (style >= 1 && !headless && !(hat && kLiveryStyleHat[species - 1][style - 1]))
                draw_worn(models, e.model, posed, e.name + "_" + kLiveryStyles[species - 1][style - 1],
                          projection, view, place, tint);
        }
    }
    const auto wear = [&](const char *extra) {
        const std::string own = e.name + "_" + extra;
        draw_worn(models, e.model, posed, model_named(models, own) != nullptr ? own : base + "_" + extra,
                  projection, view, place, tint);
    };
    if (hat && !headless) wear(kLiveryHeads[head - 1]);
    if (neck >= 1 && neck <= kLiveryNeckCount) wear(kLiveryNecks[neck - 1]);
}

SkinnedModel held_block_model(const NodeDef &def, int tiles, Vec3 centre, float size);
SkinnedModel held_piece_model(const NodeDef &def, int tiles, Vec3 centre, float size);

// A stack on the ground, as a body. Its box is the one the server collects it
// with, in its block's own colour -- grey for an item with no block -- for an
// item draw_drop has nothing better for; and it is drawn as the thing itself
// the way the classic block game shows one: a hand's breadth off the ground, bobbing and
// turning, each stack out of step with the next.
Body drop_body(Session &w, const ItemDef &def, Vec3 at, uint64_t id)
{
    const float  h = ecs::kDropSize * 0.5f;
    const double t = plat_time() + (double)(id % 97) * 0.37;
    Body b;
    b.center = Vec3{at.x, at.y + 0.12f + 0.06f * (float)std::sin(t * 2.2), at.z};
    b.half   = Vec3{h, h, h};
    b.yaw    = (float)std::fmod(t * 1.6, 6.283185307);
    b.dropped    = true;
    b.hand       = def.name;
    b.hand_block = def.places;
    if (def.places != kAir) {
        const NodeDef &node = w.registry.get(def.places);
        for (int k = 0; k < 3; ++k) b.colour[k] = (float)node.colour[k] / 255.0f;
    } else {
        b.colour[0] = b.colour[1] = b.colour[2] = 0.72f;
    }
    return b;
}

// A block falling (core.fall): the whole block, as it stood, about `at`.
Body falling_body(BlockId block, Vec3 at)
{
    Body b;
    b.center     = at;
    b.half       = Vec3{0.5f, 0.5f, 0.5f};
    b.dropped    = true;
    b.falling    = true;
    b.hand_block = block;
    return b;
}

// A stack on the ground as the thing itself, about the body's centre and turned
// by its yaw: the item's own model ("<item>_held"), its tool_* bone out of the
// player's model stood on its handle, its worn piece, or its block as a cube of
// the block's own tiles. Each fitted to about a drop's size. False for an item
// that is none of those, which stays the box.
bool draw_drop(Scene &s, Session &w, const Body &b, const Mat4 &projection, const Mat4 &view)
{
    constexpr float kItem  = 0.5f;    // the longest side of an item on the ground, as the classic block game's half block
    constexpr float kBlock = 0.25f;   // a block's cube, as the classic block game's quarter block
    const Mat4 at = mat4_multiply(mat4_translate(b.center), mat4_rotate_y(b.yaw));
    // A model drawn about the middle of what it occupies, its longest side kItem.
    const auto fitted = [&](Vec3 lo, Vec3 hi) {
        const Vec3  d    = hi - lo;
        const float size = std::max(std::max(d.x, d.y), std::max(d.z, 1e-3f));
        return mat4_multiply(at, mat4_multiply(mat4_scale(Vec3{kItem / size, kItem / size, kItem / size}),
                                               mat4_translate((lo + hi) * -0.5f)));
    };

    if (b.falling) {
        // Its own model when it wears one, else a cube of its own tiles.
        if (b.hand_block >= w.registry.size()) return false;
        const NodeDef &def = w.registry.get(b.hand_block);
        if (ModelStore::Entry *m = def.model != kNoModel ? w.models.entry(def.model) : nullptr) {
            m->renderer.draw(projection, view, mat4_translate(b.center - Vec3{0.5f, 0.5f, 0.5f}),
                             m->model.bind_pose);
            return true;
        }
    } else if (b.hand.empty()) {
        return false;
    }
    // A block drawn as a model lies there as itself, standing, at a quarter of
    // its size like a cube does: its "_held" model is made to face the eye in
    // a hand, and on the ground would lie on its back.
    if (!b.falling && b.hand_block != kAir && b.hand_block < w.registry.size()) {
        const NodeDef &def = w.registry.get(b.hand_block);
        if (ModelStore::Entry *m = def.model != kNoModel ? w.models.entry(def.model) : nullptr) {
            m->renderer.draw(projection, view,
                             mat4_multiply(at, mat4_multiply(mat4_scale(Vec3{kBlock, kBlock, kBlock}),
                                                             mat4_translate(Vec3{-0.5f, -0.5f, -0.5f}))),
                             m->model.bind_pose);
            return true;
        }
    }
    ModelStore::Entry *held = b.falling ? nullptr : model_named(w, b.hand + "_held");
    if (held != nullptr && !held->model.skinned()) {
        held->renderer.draw(projection, view, fitted(held->model.min, held->model.max), held->model.bind_pose);
        return true;
    }
    if (ModelStore::Entry *piece = b.falling ? nullptr : model_named(w, b.hand);
        piece != nullptr && piece->model.skinned()) {
        // Worn: drawn on its own bones as bound, where the file has it.
        piece->renderer.draw(projection, view, fitted(piece->model.min, piece->model.max),
                             piece->model.bind_pose);
        return true;
    }
    if (ModelStore::Entry *player = b.falling ? nullptr : w.models.entry(w.player_model_id)) {
        const SkinnedModel &m    = player->model;
        const std::string   bone = "tool_" + item_short_name(b.hand);
        for (int j = 0; j < m.joint_count; ++j) {
            if (m.joint_names[(size_t)j] != bone) continue;
            // Every other bone to nothing, and this one's vertices into its own
            // space, where the handle runs up +Y.
            // ponytail: the robot's tools' extent, -0.14 .. 0.71 up the handle;
            // measure the bone's vertices if a player model's tools differ.
            static std::vector<float> rows;
            rows.assign((size_t)m.joint_count * 12, 0.0f);
            Mat4 inverse_bind;
            std::memcpy(inverse_bind.m, &m.joint_inverse_bind[(size_t)j * 16], sizeof(inverse_bind.m));
            for (int row = 0; row < 3; ++row)
                for (int col = 0; col < 4; ++col)
                    rows[(size_t)j * 12 + (size_t)(row * 4 + col)] = inverse_bind.at(row, col);
            player->renderer.draw(projection, view, fitted(Vec3{-0.1f, -0.14f, -0.1f}, Vec3{0.1f, 0.71f, 0.1f}),
                                  rows);
            return true;
        }
    }
    if (b.hand_block == kAir || w.world_render.atlas().id == 0) return false;

    const NodeDef &def   = w.registry.get(b.hand_block);
    const int      tiles = w.world_render.atlas_tiles();
    uint64_t       key   = (uint64_t)b.hand_block * 1000003u + (uint64_t)tiles + (b.falling ? 7u : 0u);
    for (int f = 0; f < 6; ++f) key = key * 31u + def.tex[f];
    HeldCube &cube = s.drop_cubes[key];
    if (cube.built != key) {
        std::string why;
        // A track piece lying on the ground is the piece in miniature, as in the hand.
        const SkinnedModel made = def.piece.plan != 0 && !b.falling
                                      ? held_piece_model(def, tiles, Vec3{}, kBlock * 1.5f)
                                      : held_block_model(def, tiles, Vec3{}, b.falling ? 1.0f : kBlock);
        if (!cube.render.init() || !cube.render.upload(made, why))
            std::fprintf(stderr, "dropped block: %s\n", why.c_str());
        cube.built = key;
    }
    cube.render.draw(projection, view, at, {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0}, w.world_render.atlas());
    return true;
}

// A mod's piece made for a vehicle rather than for whoever sits in it: named
// for the vehicle, "<vehicle>_<part>_<variant>" ("track_core:kart_wheel_slick"
// on "track_core:kart"). Drawn on the vehicle's own bones, and the vehicle's
// bones named <part> or <part>_* are not drawn: the piece replaces them (the
// kart's four wheel_* by other wheels). The vehicle's own model worn as a
// piece paints it: its tint is the vehicle's tint_1..3 (core.player_wear).
// Answers the part, empty for a piece that is not the vehicle's.
std::string vehicle_part(const std::string &vehicle, const std::string &piece)
{
    if (piece.size() <= vehicle.size() + 1 || piece.compare(0, vehicle.size(), vehicle) != 0 ||
        piece[vehicle.size()] != '_')
        return std::string();
    const size_t from = vehicle.size() + 1, end = piece.find('_', from);
    return piece.substr(from, end == std::string::npos ? std::string::npos : end - from);
}

bool bone_of_part(const std::string &bone, const std::string &part)
{
    return bone == part || (bone.size() > part.size() && bone.compare(0, part.size(), part) == 0 &&
                            bone[part.size()] == '_');
}

// The flames of a kart's nitro: out of each of its flame_* joints, along the
// bone, the vehicle's own unless a piece of it has some (its other exhausts).
// Gathered while the bodies are drawn and drawn after them (draw_kart_fx).
void flames_of(Scene &s, const SkinnedModel &m, const Mat4 &place)
{
    constexpr float kFlame = 0.85f;   // how far out of the pipe, nodes
    for (int j = 0; j < m.joint_count; ++j) {
        if (m.joint_names[(size_t)j].rfind("flame_", 0) != 0) continue;
        Mat4 inverse_bind;
        std::memcpy(inverse_bind.m, &m.joint_inverse_bind[(size_t)j * 16], sizeof(inverse_bind.m));
        const Mat4 bound = mat4_multiply(place, mat4_invert(inverse_bind));
        const Vec3 at    = mat4_transform_point(bound, Vec3{});
        const Vec3 along = normalize(mat4_transform_dir(bound, Vec3{0.0f, 1.0f, 0.0f}));
        s.kart_flames.push_back({at, at + along * kFlame});
    }
}

// The marks a kart's tyres leave sliding: a strip from where each rear wheel
// last left one to where it is, once it has gone kMarkStep on. The wheels are
// the model's wheel_R* bones; the strip lies on what is under each one, found
// by a ray down the kart's own up -- the bottom of the model is not the road
// (the springs, a kerb, a jump: strips floated in the air). A wheel with
// nothing under it leaves none.
constexpr size_t kMarks    = 4096;   // strips kept, the oldest written over
constexpr float  kMarkStep = 0.3f;
constexpr float  kMarkLife = 30.0f;   // seconds to fade out

void leave_marks(Session &w, const SkinnedModel &m, const Body &b, const Mat4 &place)
{
    if (!b.sliding) {
        w.mark_last.erase(b.id);
        return;
    }
    constexpr float kLift = 0.02f;   // over the road, so it does not fight it for depth
    const Vec3 down = normalize(mat4_transform_dir(place, Vec3{0.0f, -1.0f, 0.0f}));
    Vec3 now[2];
    int  got = 0;
    for (int j = 0; j < m.joint_count && got < 2; ++j)
        if (m.joint_names[(size_t)j].rfind("wheel_R", 0) == 0) {
            const Vec3 rest = m.joint_rest[(size_t)j];
            const Vec3 from = mat4_transform_point(place, Vec3{rest.x, rest.y, rest.z});
            const RaycastHit hit = raycast(w.world, w.registry, from, down, 1.0f);
            if (!hit.hit) break;
            // A track piece's cells are boxes under its road, which is drawn
            // smooth over them: on the box a strip was under the road.
            Vec3 at = from + down * hit.distance;
            int  ox, oy, oz;
            float road;
            if (piece_find(w.world, w.registry, hit.x, hit.y, hit.z, ox, oy, oz) &&
                piece_surface(w.registry.get(w.world.get_block(ox, oy, oz)).piece,
                              (uint8_t)(w.world.get_facing(ox, oy, oz) & 3), at.x - (float)ox,
                              at.z - (float)oz, road))
                at.y = (float)oy + road;
            now[got++] = at - down * kLift;
        }
    if (got < 2) {
        w.mark_last.erase(b.id);
        return;
    }
    const auto it = w.mark_last.find(b.id);
    if (it == w.mark_last.end()) {
        w.mark_last[b.id] = {now[0], now[1]};
        return;
    }
    const Vec3 side = normalize(mat4_transform_dir(place, Vec3{1.0f, 0.0f, 0.0f})) * 0.09f;
    if (length(now[0] - it->second[0]) < kMarkStep) return;
    for (int k = 0; k < 2; ++k) {
        const Vec3 from = it->second[(size_t)k];
        if (length(now[k] - from) > 3.0f) continue;   // a teleport, a portal: no strip across the gap
        if (w.marks.size() < kMarks) w.marks.resize(kMarks);
        w.marks[w.mark_next] = Session::Mark{from, now[k], side, (float)plat_time()};
        w.mark_next          = (w.mark_next + 1) % kMarks;
    }
    it->second   = {now[0], now[1]};
    w.marks_dirty = true;
}

bool draw_body_model(Scene &s, Session &w, const Body &b, const Mat4 &projection, const Mat4 &view)
{
    if (b.dropped) return draw_drop(s, w, b, projection, view);
    ModelStore::Entry *e = w.models.entry(b.model);
    if (e == nullptr) return false;
    const Mat4 place = body_place(b);

    // The light of the cell its middle is in, the way a block-model is lit: a
    // creature in a dark cave is dark.
    const float light = w.world_render.cell_light(w.world.get_light((int)std::floor(place.at(0, 3)),
                                                                    (int)std::floor(place.at(1, 3) + 0.5f),
                                                                    (int)std::floor(place.at(2, 3))),
                                                  true);

    // What is worn and held goes on whoever wears it: the rider when there is
    // one, never the vehicle -- a kart has no chest, and a pack bound to one
    // stood where it was made, about the driver's head.
    static std::vector<float> posed;   // one draw at a time
    const auto dress = [&](ModelStore::Entry &on) {
        for (int k = 0; k < 5; ++k) {
            if (b.worn[k].empty()) continue;
            if (k != 4) {
                draw_worn(w.models, on.model, posed, b.worn[k], projection, view, place, b.worn_tint[k]);
                continue;
            }
            // The back: its arms' own clips, and what they hold.
            static std::vector<float> pack;
            pack.clear();
            const std::string clip = pack_clip(b.worn[k], b.arms_working, b.arm);
            draw_worn(w.models, on.model, posed, b.worn[k], projection, view, place, b.worn_tint[k],
                      clip.c_str(), &pack);
            if (ModelStore::Entry *piece = model_named(w.models, b.worn[k]); piece != nullptr && !pack.empty())
                draw_pack_tools(w.models, piece->model, pack, on, b.arm, projection, view, place, light,
                                livery_tag(b.livery));
        }
        for (int k = 0; k < 10; ++k)   // what a mod dresses them in (core.player_wear)
            if (const ModelStore::Entry *piece = b.dress[k] != 0 ? w.models.entry(b.dress[k]) : nullptr)
                if (b.dress[k] != b.model && vehicle_part(e->name, piece->name).empty())   // the vehicle's own
                    draw_worn(w.models, on.model, posed, piece->name, projection, view, place, b.dress_tint[k]);
        if (b.hand_known)
            draw_held_block(s.held_bodies[(uint64_t)b.hand_block * 1000003u + (uint64_t)(uintptr_t)&on.model],
                            w, on.model, posed, b.hand, b.hand_block, projection, view, place, "",
                            b.hand_tag, light);
    };
    ModelStore::Entry *r = b.rider != 0 ? w.models.entry(b.rider) : nullptr;
    if (e->model.skinned() && r != nullptr) {
        // A vehicle: its own pieces on its own bones, the parts they replace
        // left out of it, and painted when it wears itself (vehicle_part).
        wheel_pose(e->model, b, posed);
        uint16_t paint = 0;
        bool     flamed = false;
        static std::vector<float> bare;
        bare = posed;
        for (int k = 0; k < 10; ++k) {
            if (b.dress[k] == 0) continue;
            if (b.dress[k] == b.model) {
                paint = b.dress_tint[k];
                continue;
            }
            const ModelStore::Entry *piece = w.models.entry(b.dress[k]);
            const std::string part = piece != nullptr ? vehicle_part(e->name, piece->name) : std::string();
            if (part.empty()) continue;
            draw_worn(w.models, e->model, posed, piece->name, projection, view, place, b.dress_tint[k]);
            for (int j = 0; j < e->model.joint_count; ++j)
                if (bone_of_part(e->model.joint_names[(size_t)j], part))
                    std::fill(bare.begin() + (ptrdiff_t)j * 12, bare.begin() + (ptrdiff_t)j * 12 + 12, 0.0f);
            if (b.boosting) {
                const size_t was = s.kart_flames.size();
                flames_of(s, piece->model, place);
                flamed = flamed || s.kart_flames.size() > was;
            }
        }
        e->renderer.draw(projection, view, place, bare, gfx_texture{0}, light, b.alpha,
                         (uint32_t)paint << kTagIdBits);
        if (b.boosting && !flamed) flames_of(s, e->model, place);
        if (b.sliding || w.mark_last.count(b.id)) leave_marks(w, e->model, b, place);
    } else if (e->model.skinned()) {
        wheel_pose(e->model, b, posed);
        e->renderer.draw(projection, view, place, posed, gfx_texture{0}, light, b.alpha, livery_tag(b.livery));
        if (!b.arms_only) {   // nothing worn shows from inside the head
            draw_extras(w.models, *e, posed, b.livery, false, projection, view, place);
            if (r == nullptr) dress(*e);
        }
    } else {
        e->renderer.draw(projection, view, place, e->model.bind_pose, gfx_texture{0}, light, b.alpha);
    }
    // Whoever is sat in it, in the same place: a rider's clips are authored
    // about the vehicle's own origin.
    // Without its head when the eye is in it: its arms on the wheel and its
    // knees in the seat are what a driver sees of themselves -- and nothing
    // worn on the head, which would be over the eye.
    if (r != nullptr) {
        std::vector<int8_t> show;
        if (b.rider_headless) {
            show.assign((size_t)r->model.joint_count, -1);
            for (int j = 0; j < r->model.joint_count; ++j)
                if (r->model.joint_names[(size_t)j] == "head") show[(size_t)j] = 0;
        }
        pose(r->model, b.rider_clips, 2, nullptr, show.empty() ? nullptr : show.data(), posed);
        r->renderer.draw(projection, view, place, posed, gfx_texture{0}, 1.0f, 1.0f, livery_tag(b.livery));
        draw_extras(w.models, *r, posed, b.livery, b.rider_headless, projection, view, place);
        dress(*r);
    }
    return true;
}

// Where a body's model stands, and which way. The body's centre is the middle
// of its box and a model stands on its feet, so what the matrix carries is the
// bottom of the box -- a model placed by the centre floats half its height off
// the ground and looks like a bug in the physics.
//
// The yaw negated: a body's yaw faces (sin, 0, -cos) (Player::forward) and
// mat4_rotate_y turns -Z to (-sin, 0, -cos). Unnegated, the model agreed with
// its body only facing -Z or +Z, and a model steering right was drawn turning
// left.
//
// Then tipped the way a chassis sits: about the model's own right (+x) for the
// nose, whose -z rises with a positive angle, and about its own -z for the
// roll, the right side going down.
Mat4 body_place(const Body &b)
{
    return mat4_multiply(
        mat4_multiply(mat4_translate(Vec3{b.center.x, b.center.y - b.half.y, b.center.z}),
                      mat4_rotate_y(-b.yaw)),
        mat4_multiply(mat4_rotate_x(b.pitch), mat4_rotate_z(-b.roll)));
}

// A body lying on its back (core.player_lie, protocol 75): the middle of it
// at `at` over what it lies on, its head along `head`. body_place tips a model
// about its feet, and a nose-up quarter turn lays it down behind them face up,
// so it faces away from its head and its feet are half a body the other way.
void lay(Body &b, Vec3 at, float head)
{
    const float hx = std::sin(head), hz = -std::cos(head);
    b.yaw    = head + 3.14159265f;
    b.pitch  = 1.5707963f;
    b.roll   = 0.0f;
    b.center = Vec3{at.x - hx * b.half.y, at.y + net::kLieLift + b.half.y, at.z - hz * b.half.y};
}

// Where the rider of a vehicle has its eyes, in the world: a little over its
// head bone, posed as it is drawn -- leaning into a bend, it looks from where
// it leans. Its feet's eye height over the body when the model has no head.
Vec3 rider_eye(Session &w, const Body &b, Vec3 fallback);

// The eye of a body sat on a seat: over its own head bone as it is posed now.
Vec3 seat_eye(Session &w, const Body &b, Vec3 fallback)
{
    Body as_rider   = b;
    as_rider.rider  = b.model;
    as_rider.rider_clips[0] = b.clips[0];
    as_rider.rider_clips[1] = b.clips[1];
    return rider_eye(w, as_rider, fallback);
}

Vec3 rider_eye(Session &w, const Body &b, Vec3 fallback)
{
    // ponytail: one height for every rider, the robot's (its head is 0.25 tall
    // over the head bone); a model's own eye bone if another needs it.
    constexpr float kEyeOverHead = 0.12f;
    const ModelStore::Entry *r = w.models.entry(b.rider);
    if (r == nullptr) return fallback;
    const SkinnedModel &m = r->model;
    int head = -1;
    for (int j = 0; j < m.joint_count && head < 0; ++j)
        if (m.joint_names[(size_t)j] == "head") head = j;
    if (head < 0) return fallback;

    static std::vector<float> posed;
    pose(m, b.rider_clips, 2, nullptr, nullptr, posed);
    const float *M = &posed[(size_t)head * 12];   // 3x4, row major
    const Vec3   v = m.joint_rest[(size_t)head] + Vec3{0.0f, kEyeOverHead, 0.0f};
    const Vec3   at{M[0] * v.x + M[1] * v.y + M[2] * v.z + M[3],
                  M[4] * v.x + M[5] * v.y + M[6] * v.z + M[7],
                  M[8] * v.x + M[9] * v.y + M[10] * v.z + M[11]};
    return mat4_transform_point(body_place(b), at);
}

// A creature that was drawn last frame and is not now has died -- or gone out
// of reach, which looks the same from here: rather than vanish between one
// frame and the next, it stays where it was last seen and fades out, playing
// its model's "death" clip when it has one and held still in its last pose when
// it has not. The server says nothing of this; the list of bodies near is
// whole in every message (ClientSession, Entities), so a body missing from it
// is gone.
constexpr double kFadeOut = 1.2;   // seconds from a death to nothing

void fade_the_gone(Session &w, std::unordered_map<uint64_t, Body> &now_seen, double now)
{
    for (const auto &[id, b] : w.creatures_seen)
        if (now_seen.find(id) == now_seen.end()) w.fading.push_back({b, now});
    w.creatures_seen.swap(now_seen);

    for (size_t i = 0; i < w.fading.size();) {
        const double t = now - w.fading[i].since;
        if (t >= kFadeOut) {
            w.fading.erase(w.fading.begin() + (std::ptrdiff_t)i);
            continue;
        }
        Body b  = w.fading[i].body;
        b.alpha = (float)(1.0 - t / kFadeOut);
        if (const ModelStore::Entry *e = w.models.entry(b.model)) {
            const int death = e->model.clip("death");
            if (death >= 0) {
                // Once, and held at its end: a fall does not get up again.
                const float d = e->model.clips[(size_t)death].duration;
                b.clips[0]    = ClipAt{-1, 0.0f, 0.0f};
                b.clips[1]    = ClipAt{death, std::min((float)t, std::max(0.0f, d - 1e-3f)), 1.0f};
            }
        }
        w.bodies.push_back(b);
        ++i;
    }
}

// What karts do to the road and the air, after the bodies: the marks their
// tyres left, mixed over the road, and the flames of their nitro, added.
void draw_kart_fx(Scene &s, Session &w, const Mat4 &projection, const Mat4 &view)
{
    if (w.marks.empty() && s.kart_flames.empty()) return;
    if (s.fx_ready == 0) s.fx_ready = s.fx_render.init() ? 1 : -1;
    if (s.fx_ready < 0) {
        s.kart_flames.clear();
        return;
    }
    const float now = (float)plat_time();
    if (!w.marks.empty()) {
        // ponytail: the renderer's one buffer is re-sent for whichever world
        // drew last; two worlds with marks in sight upload twice a frame.
        static const Session *sent = nullptr;
        const bool changed = w.marks_dirty || sent != &w;
        sent = &w;
        if (w.marks_dirty) {
            w.mark_corners.clear();
            for (const Session::Mark &m : w.marks) {
                const Vec3 c[4] = {m.a - m.side, m.a + m.side, m.b + m.side, m.b - m.side};
                for (const int i : {0, 1, 2, 0, 2, 3})
                    w.mark_corners.insert(w.mark_corners.end(), {c[i].x, c[i].y, c[i].z, m.born});
            }
        }
        s.fx_render.draw_marks(w.mark_corners, changed, now, kMarkLife, projection, view);
        w.marks_dirty = false;
    }
    const float time = std::fmod(now, 3600.0f);
    // A ribbon down the flame for whoever sees it from the side, and puffs
    // along it facing the eye for whoever is behind it: a ribbon seen end on
    // is a line, and the camera of the one driving is right behind the pipe.
    for (const auto &[at, to] : s.kart_flames) {
        static const float kJet[4]  = {1.0f, 0.45f, 0.12f, 1.0f};
        static const float kCore[4] = {0.45f, 0.65f, 1.0f, 0.9f};
        s.fx_render.draw(FxKind::Jet, at, to, 0.22f, kJet, 0.0f, time, projection, view);
        for (int k = 0; k < 4; ++k) {
            const float f       = (float)k / 4.0f;
            const float flicker = 0.8f + 0.2f * std::sin(time * 37.0f + (float)k * 1.9f + at.x * 3.0f);
            const float puff[4] = {1.0f, 0.55f - 0.3f * f, 0.15f, 0.9f - 0.5f * f};
            s.fx_render.draw(FxKind::Glow, at + (to - at) * f, Vec3{}, (0.42f - 0.2f * f) * flicker, puff, 0.0f,
                             time, projection, view);
        }
        s.fx_render.draw(FxKind::Glow, at, at, 0.24f, kCore, 0.0f, time, projection, view);
    }
    s.kart_flames.clear();
}

void draw_bodies(Scene &s, Session &w, const Mat4 &projection, const Mat4 &view)
{
    struct After {   // the karts' marks and flames once the bodies are down, whichever way out
        Scene &s; Session &w; const Mat4 &projection, &view;
        ~After() { draw_kart_fx(s, w, projection, view); }
    } after{s, w, projection, view};
    if (w.bodies.empty()) return;

    if (!w.portals || !s.cut_bodies) {
        for (const Body &b : w.bodies) {
            if (draw_body_model(s, w, b, projection, view)) continue;
            s.body_render.draw(b, projection, view);
        }
        return;
    }

    const PortalLink *links[2] = {&w.link_ab, &w.link_ba};

    for (const Body &b : w.bodies) {
        if (draw_body_model(s, w, b, projection, view)) continue;

        const AABB box = b.bounds();

        PortalCut cut;
        for (const PortalLink *l : links) {
            const PortalCut c = portal_cut(l->src, l->dst, box);
            if (c.cut) {
                cut = c;
                break;
            }
        }

        if (!cut.cut) {
            s.body_render.draw(b, projection, view);
            continue;
        }

        s.body_render.draw_clipped(b, cut.keep_here, mat4_identity(), projection, view);
        s.body_render.draw_clipped(b, cut.keep_there, cut.clone_transform, projection, view);
    }
}

// What the mods draw, in the order they asked for.
//
// The handler takes no arguments and cannot cancel anything: it is called to
// draw, and everything it needs to know about where it is drawing came from the
// engine when it set the camera up. A pass that errors costs its mod, the same
// way an event handler or a system does -- otherwise a mod with a bad frame
// takes the frame rate with it for the rest of the run.
void draw_mod_passes(Scene &s, int world, const Mat4 &projection, const Mat4 &view)
{
    const std::vector<const ModPass *> order = s.passes.in_order();
    if (order.empty()) return;

    s.pass_projection = projection;
    s.pass_view       = view;
    s.in_pass         = true;

    // Only the passes of the world being drawn: a world left through a portal
    // stays alive as a window, and its drawing at its own coordinates landed
    // in the world walked back into (arpg's gold and globes lying in the
    // sandbox). Through the opening it is drawn by draw_bodies_cb instead,
    // and that world is the active one while its passes run: what they ask
    // (core.players, core.model_id, render.model) is about the world drawn.
    const int stood = s.active;
    s.active        = world;
    Session                &w    = s.act();
    const std::vector<Mod> &here = w.mods.mods();

    for (const ModPass *p : order) {
        if (!p->vm) continue;
        if (std::none_of(here.begin(), here.end(), [p](const Mod &m) { return m.vm == p->vm; }))
            continue;

        script_event_result result{};
        char                err[256] = {0};
        w.mods.enter_vm(p->vm);
        const bool ok = script_call_handler(p->vm, p->ref, nullptr, 0, 0, &result, err,
                                            sizeof(err));
        w.mods.leave_vm(p->vm);

        if (!ok) {
            w.mods.disable_vm(p->vm, "render pass '" + p->name + "': " + err);
            s.passes.drop_vm(p->vm);
            break;   // the list just changed under this loop
        }
    }

    s.active  = stood;
    s.in_pass = false;
}

// A world's code drawing over the screen. Once a frame, inside the 2D begin and
// end the caller opened; a pass that fails disables its mod, like a render pass.
void draw_hud_passes(Scene &s)
{
    const std::vector<const ModPass *> order = s.hud_passes.in_order();
    if (order.empty()) return;

    // Only the world being stood in. A world just left through a portal stays
    // alive as a window, code and all, and its counters on the screen of the
    // world the player walked back into would be a HUD from somewhere else.
    const std::vector<Mod> &here = s.act().mods.mods();

    s.in_hud_pass = true;
    for (const ModPass *p : order) {
        if (!p->vm) continue;
        if (std::none_of(here.begin(), here.end(), [p](const Mod &m) { return m.vm == p->vm; }))
            continue;

        script_event_result result{};
        char                err[256] = {0};
        s.act().mods.enter_vm(p->vm);
        const bool ok = script_call_handler(p->vm, p->ref, nullptr, 0, 0, &result, err,
                                            sizeof(err));
        s.act().mods.leave_vm(p->vm);

        if (!ok) {
            s.act().mods.disable_vm(p->vm, "hud pass '" + p->name + "': " + err);
            s.hud_passes.drop_vm(p->vm);
            break;   // the list just changed under this loop
        }
    }
    s.in_hud_pass = false;
}

// The portal renderer calls this to put the bodies inside a portal's image.
// Without it a crate halfway through is cut correctly in the world and simply
// absent from the view through the opening, which is worse than not cutting it
// at all.
//
// The bodies of the world the opening shows, not of the one stood in: from the
// Inferno its wraiths were drawn over the overworld behind the portal.
void draw_bodies_cb(void *ud, const Mat4 &projection, const Mat4 &view)
{
    Scene &s = *(Scene *)ud;
    draw_bodies(s, s.at(s.portal_shown), projection, view);
    draw_mod_passes(s, s.portal_shown, projection, view);
}

// What this player is aiming at, of the bodies the server has told them about.
// Zero for nothing, and `at` is how far away it is.
//
// Drops are skipped on purpose: a stack on the ground is walked into, not
// struck, and a crate lying in front of a mob would otherwise soak up every
// swing aimed past it.
//
// The box is the one that body actually occupies -- a mob's is smaller than a
// player's -- which is why net::EntityKind is on the wire. Aiming at everything
// with a person-shaped box would let a player miss a mob they were looking
// straight at, and hit one they were looking beside.
// A creature is aimed at by the box its model stands in, when it has one: a
// dog is not a player's height. Square, as wide as its widest way round, so
// its turning does not matter. The box it is struck in on the server is the
// taller one, which a click inside this one is always inside of too.
AABB creature_box(const ModelStore &models, uint16_t model, Vec3 feet)
{
    const ModelStore::Entry *e = model != 0 ? models.entry(model) : nullptr;
    if (e == nullptr || e->model.max.y <= e->model.min.y) return ecs::mob_box(feet.x, feet.y, feet.z);
    const Vec3 &lo = e->model.min, &hi = e->model.max;
    const float r = std::max({std::fabs(lo.x), std::fabs(hi.x), std::fabs(lo.z), std::fabs(hi.z), 0.2f});
    return AABB{Vec3{feet.x - r, feet.y, feet.z - r}, Vec3{feet.x + r, feet.y + hi.y, feet.z + r}};
}

uint64_t aimed_body(const net::ClientSession &conn, const ModelStore &models, Vec3 eye, Vec3 fwd,
                    float reach, float &at)
{
    uint64_t best   = 0;
    float    best_t = reach;

    for (const net::RemotePlayer &p : conn.players()) {
        if (p.kind == net::EntityKind::Drop || p.held) continue;   // nor what is in your hands

        const AABB box = (p.kind == net::EntityKind::Mob)
                             ? creature_box(models, p.model, Vec3{p.x, p.y, p.z})
                             : body_box(Vec3{p.x, p.y, p.z}, conn.welcome().movement.width * 0.5f);

        float t = 0.0f;
        if (!ray_hits_box(box, eye, fwd, reach, t)) continue;
        if (t > best_t) continue;

        best   = p.id;
        best_t = t;
    }

    at = best_t;
    return best;
}

// How long the camera takes to catch up with a body that was stepped up, and
// where it is right now.
//
// A fixed 0.15 s was the first answer, on the reasoning that a two centimetre
// lift and a one node one should both be over in about the same glance. Play
// says otherwise. The arrival resolver in the --oriented scene hands the camera
// up to 1.25 nodes -- the two openings sit half a node apart in height, so
// every crossing one way lands the feet in the floor, and looking up or jumping
// on the way in adds to it -- and 1.25 nodes in 0.15 s is 8.3 nodes a second,
// twice a sprint. That is the lurch, measured in docs/archive/M5_FINDINGS.md 12.7 and
// reported as the camera snapping.
//
// So the time is a distance at a bounded speed, floored so a small lift is
// still a glance and capped so a big one cannot become a lift ride. The speed
// is a shade under a walk (4.3 nodes a second): the camera rising slower than
// the player could have walked it is what makes it read as arriving rather than
// as being thrown.
inline constexpr float kCamWarpSpeed = 2.5f;   // nodes a second
inline constexpr float kCamWarpMin   = 0.15f;  // a glance, for the small ones
inline constexpr float kCamWarpMax   = 0.60f;  // and never longer than this

float cam_warp_time(float distance)
{
    return std::clamp(distance / kCamWarpSpeed, kCamWarpMin, kCamWarpMax);
}

Vec3 camera_warp(const Scene &s)
{
    if (s.cam_warp_left <= 0.0f) return Vec3{};
    return s.cam_warp * (s.cam_warp_left / s.cam_warp_time);
}

// The roll goes on the camera's up vector, not into the player's state:
// rolling is something the view does after a crossing, and the player's yaw and
// pitch have to stay the ones their input produces or moving the mouse while
// the roll settles would fight the animation.
//
// A function rather than four lines inside render_frame because the view trace
// below has to report the camera the player is actually looking through, and a
// second copy of this would let the report drift away from the picture.
Vec3 camera_up(float roll, Vec3 forward, Vec3 up)
{
    if (std::fabs(roll) <= 1e-4f) return up;
    const Vec3  f = normalize(forward);
    const Vec3  r = cross(f, up);
    const float c = std::cos(roll), sn = std::sin(roll);
    return normalize(up * c + r * sn);
}

// ------------------------------------------------------------- the view trace
//
// Reported from play, twice, in the --oriented scene: "la direzione della vista
// mi scatta di colpo dopo il teleport". portal_orientation section 9 walks that
// crossing frame by frame over nine poses and finds nothing bigger than a
// fiftieth of a degree, so the gate and the player disagree -- and it is the
// gate that is missing something, because the player is the one holding the
// evidence. What the gate does not have is a real mouse, a real frame time,
// and whatever the player happened to be doing when it fired.
//
// So this prints the camera the player is looking through, once per frame, for
// two seconds around every crossing. The column that decides it is `unexplned`:
// the angle between the look direction this frame and the one the PREVIOUS
// frame's angles plus THIS frame's mouse input would have produced. A mouse can
// turn the view as fast as a hand can move, and that is not a defect; a turn
// nothing asked for is. `clamp` is next to it because the pitch limit is the
// other thing that can move the view without being asked, and a crossing that
// lands the player looking nearly straight up is exactly where it would bite.
//
// Off unless --log-view says so. A crossing is rare but a line a frame is not.
struct ViewTrace {
    bool on = false;

    // Counted always, printed unless a gate is holding the trace. The window
    // this thing opens is the part that was wrong once already, and a gate that
    // checks it by reading stdout would be checking the terminal.
    bool quiet = false;
    long lines = 0;

    // How much longer to keep printing, in SECONDS. Frames were the obvious
    // unit and they were wrong: the first log came back with dt at 0.3 ms, so
    // the 120 frame window it was supposed to open after a crossing was 36
    // milliseconds wide and shut long before anything a player could see. The
    // window is a length of time because what it has to contain -- the roll
    // animation, the camera warp, a bounce back through the opening -- is.
    float left = 0.0f;
    long  since_cross = 0;    // frames, which is still what a line is
    float secs_since_cross = 0.0f;
    long  crossings   = 0;

    // Printing every frame at three thousand a second buries the log in lines
    // that say nothing, so ordinary frames are sampled. The measurement still
    // runs every frame: a printed line reports the WHOLE group of frames it
    // stands for -- degrees summed, mouse summed, the worst clamp -- so nothing
    // can hide in the frames between two lines.
    float since_print = 0.0f;
    float last_beat   = 0.0f;
    int   acc_frames  = 0;
    float acc_secs    = 0.0f;
    float acc_dx = 0.0f, acc_dy = 0.0f;           // after look_apply_roll
    float acc_raw_dx = 0.0f, acc_raw_dy = 0.0f;   // what the hand did
    float acc_turn = 0.0f, acc_asked = 0.0f, acc_unexplained = 0.0f;
    float acc_clamped = 0.0f, acc_horizon = 0.0f;

    // The look direction the last crossing handed the player, and how far the
    // view is from it now. The report is precise -- "the camera direction snaps
    // TOWARDS the exit direction of the last teleport" -- so that angle is a
    // column, and a fall in it is an event.
    bool  have_exit    = false;
    Vec3  exit_fwd{};
    float prev_toexit  = 0.0f;
    float closing      = 0.0f;   // degrees of approach to it, same leak

    // Total degrees turned, whoever asked. A mouse can legitimately turn a view
    // fast, but a burst nobody could have made with a hand is worth a line, and
    // without this the log is silent for every turn credited to the mouse.
    float recent_turn = 0.0f;

    // Each event fires once and then holds its tongue until the thing that set
    // it off has died down. Without this a single fast turn printed a line on
    // every frame for half a second, which at this frame rate was two thousand
    // lines for seven events.
    bool snap_armed = true, fast_armed = true, back_armed = true;

    // Unexplained degrees, leaking away over a tenth of a second.
    //
    // A per-frame threshold was the first thing tried and it is meaningless
    // here: one degree in a frame is sixty degrees a second at sixty frames and
    // three thousand at this client's actual rate, so a thirty degree snap
    // spread over a tenth of a second arrives as three hundred frames of a
    // tenth of a degree and trips nothing. What a player sees is degrees over a
    // stretch of time, so that is what is measured.
    float recent = 0.0f;

    // The previous frame's camera, for how far it turned, and the previous
    // frame's angles, for how far the mouse asked it to.
    bool  have_prev   = false;
    Vec3  prev_fwd{};
    Vec3  prev_up{};
    float prev_yaw    = 0.0f;
    float prev_pitch  = 0.0f;

    // The last few lines, kept whether or not the trace is printing. The frames
    // BEFORE the thing that is being looked at say what the player was doing
    // when it fired, and by the time it has fired they are gone.
    static constexpr int kRing = 6;
    char ring[kRing][384] = {};
    int  ring_next = 0;
    bool ring_full = false;
};

ViewTrace g_view_trace;

// Two seconds at sixty frames, which is long enough to contain the roll
// animation (half a second), the camera warp (0.15 s) and a second crossing if
// the arrival bounces back into the opening.
constexpr float kViewTraceSeconds = 2.0f;

// Outside a window the trace still says one line every half second, so that a
// log is a continuous record of where the view was rather than a few islands
// with nothing between them. Two lines a second costs nothing and is what makes
// "it snapped some time after the crossing" checkable at all.
constexpr float kViewBeatSeconds = 0.5f;

// One line every four milliseconds is 250 a second: fine enough to see an
// animation move and coarse enough to read.
constexpr float kViewTraceSampleSeconds = 0.004f;

// How much unexplained turn, inside one leak time, counts as a snap. Twelve
// degrees is about a fifth of the screen at this field of view: past what an
// eye reads as drift and well past anything the engine is supposed to do, since
// the roll animation moves the horizon rather than the direction and a walking
// body moves the direction not at all.
constexpr float kViewSnapDegrees = 12.0f;

// How long an unexplained turn keeps counting. A tenth of a second is about the
// window in which the eye fuses movement into one jump.
constexpr float kViewSnapLeak = 0.1f;

// A turn this big inside one leak time is worth a line whoever asked for it: it
// is a third of the screen, and a hand moving a mouse that fast is worth seeing
// in the log next to the pixels it sent.
// 60 degrees inside a tenth of a second is 600 a second. A mouse at this
// engine's 0.0025 rad per pixel needs 420 pixels in that tenth to do it, which
// is a hard flick; 25 was tried first and fired on ordinary looking around.
constexpr float kViewTurnDegrees = 60.0f;

// Pixels in a SINGLE frame that no hand produced. At 0.3 ms a frame a mouse
// reporting at 1000 Hz delivers its biggest honest burst in one frame -- 32
// pixels was the most this client has logged -- so ten times that is either a
// pointer warp or a frame the client sat out.
constexpr float kViewMouseBurst = 300.0f;

// The view coming back to within this of the last crossing's exit direction,
// having been at least this much further away a tenth of a second ago, is the
// thing that was reported.
// "Towards", not "onto": the report says the view snaps toward where the last
// teleport left it, so the gap has to shut by a lot and end small, not end at
// zero.
constexpr float kViewReturnNear = 25.0f;
constexpr float kViewReturnDrop = 30.0f;

// A camera warp worth saying out loud. The arrival resolver hands the camera a
// displacement to absorb in 0.15 s; a third of a node in that time is a visible
// lurch, and the log should mark it whether or not anything turned.
constexpr float kViewWarpNodes = 0.3f;

// How long to keep printing after one. Half a second, which is enough to say
// whether the view went and stayed or went and came back.
constexpr float kViewSnapSeconds = 0.5f;

float view_turn_degrees(Vec3 a, Vec3 b)
{
    const float c = std::clamp(dot(normalize(a), normalize(b)), -1.0f, 1.0f);
    return std::acos(c) * 180.0f / 3.14159265f;
}

// The frames leading up to whatever is being reported, oldest first, numbered
// backwards from it. The frame number is printed here rather than stored
// because "how long before" is not knowable when the line is made.
void view_trace_dump_ring(ViewTrace &v)
{
    const int n = v.ring_full ? ViewTrace::kRing : v.ring_next;
    for (int i = 0; i < n; ++i) {
        const int idx = v.ring_full ? (v.ring_next + i) % ViewTrace::kRing : i;
        ++v.lines;
        if (!v.quiet) std::printf("view  before %s", v.ring[idx]);
    }
    v.ring_next = 0;
    v.ring_full = false;
}

// What a crossing did, printed from the one place that applies one.
void view_trace_crossing(const Scene &s, const Traversal &t)
{
    ViewTrace &v = g_view_trace;
    if (!v.on) return;

    ++v.crossings;
    if (!v.quiet)
    std::printf("view ---- crossing %ld: out of world %d, feet %.3f %.3f %.3f, yaw %+.2f "
                "pitch %+.2f roll %+.2f, stepped_up %d, warp %.3f (%.3f %.3f %.3f), "
                "velocity delta %.2f %.2f %.2f\n",
                v.crossings, s.active, (double)t.feet.x, (double)t.feet.y, (double)t.feet.z,
                (double)(t.yaw * 180.0f / 3.14159265f),
                (double)(t.pitch * 180.0f / 3.14159265f),
                (double)(t.roll * 180.0f / 3.14159265f), t.stepped_up ? 1 : 0,
                (double)length(t.warp_offset), (double)t.warp_offset.x,
                (double)t.warp_offset.y, (double)t.warp_offset.z,
                (double)t.velocity_delta.x, (double)t.velocity_delta.y,
                (double)t.velocity_delta.z);

    view_trace_dump_ring(v);

    v.have_exit   = true;
    v.last_beat   = 0.0f;
    v.exit_fwd    = dir_from_angles(t.yaw, t.pitch);
    v.prev_toexit = 0.0f;
    v.closing     = 0.0f;

    v.left             = kViewTraceSeconds;
    v.since_cross      = 0;
    v.secs_since_cross = 0.0f;
}

// What one frame did to the view, in degrees, with no printing and no globals.
//
// Separate from the line that reports it so that a gate can check the
// instrument: a trace whose `unexplained` column reads zero through a snap is
// worse than no trace, because it clears the code that is at fault. See
// portal_orientation section 11.
struct ViewSample {
    float turn        = 0.0f;   // how far the look direction moved
    float asked       = 0.0f;   // how far the mouse asked it to
    float unexplained = 0.0f;   // how far it moved that nothing asked for
    float clamped     = 0.0f;   // how much of the asked-for pitch the limit ate
    float horizon     = 0.0f;   // how far the horizon rolled
};

// `in` must be the input AFTER look_apply_roll, which is the delta that
// actually reached yaw and pitch: predicting from the raw one would blame the
// roll compensation for turns it correctly asked for.
ViewSample view_measure(const ViewTrace &v, float roll, const Player &player,
                        const PlayerInput &in)
{
    ViewSample out;
    if (!v.have_prev) return out;

    const Vec3 fwd = player.forward();
    const Vec3 up  = camera_up(roll, fwd, player.up());

    out.turn = view_turn_degrees(v.prev_fwd, fwd);

    // Where the previous frame's angles plus this frame's mouse would have
    // pointed, with the clamp applied so that the clamp is reported in its own
    // column rather than hidden inside the residual.
    const float want_yaw     = v.prev_yaw + in.look_dx * kLookSpeed;
    const float want_pitch_0 = v.prev_pitch - in.look_dy * kLookSpeed;
    const float want_pitch   = std::clamp(want_pitch_0, -kMaxPitch, kMaxPitch);
    const Vec3  want_fwd     = dir_from_angles(want_yaw, want_pitch);

    out.asked       = view_turn_degrees(v.prev_fwd, want_fwd);
    out.unexplained = view_turn_degrees(want_fwd, fwd);
    out.clamped     = (want_pitch_0 - want_pitch) * 180.0f / 3.14159265f;
    out.horizon     = view_turn_degrees(v.prev_up, up);
    return out;
}

// One frame of it, called with the state the frame is about to be drawn with.
//
// `raw` is the mouse as the hand moved it and `in` is the same after
// look_apply_roll, which is the delta that actually reached yaw and pitch. Both
// are printed: while the horizon is tipped they are different vectors, and a
// view that goes somewhere the hand did not ask for would be credited to the
// mouse if only the compensated one were kept.
void view_trace_frame(const Scene &s, const Player &player, const PlayerInput &raw,
                      const PlayerInput &in, float dt, bool crossed)
{
    ViewTrace &v = g_view_trace;
    if (!v.on) return;

    const Vec3 fwd = player.forward();
    const Vec3 up  = camera_up(s.roll, fwd, player.up());
    const Vec3 eye = player.eye() + camera_warp(s) + Vec3{0.0f, s.step.lag, 0.0f};

    const ViewSample m = view_measure(v, s.roll, player, in);

    // Every frame, whether or not this one gets a line.
    ++v.acc_frames;
    v.acc_secs      += dt;
    v.acc_dx        += in.look_dx;
    v.acc_dy        += in.look_dy;
    v.acc_raw_dx    += raw.look_dx;
    v.acc_raw_dy    += raw.look_dy;
    v.acc_turn      += m.turn;
    v.acc_asked     += m.asked;
    v.acc_unexplained += m.unexplained;
    v.acc_horizon   += m.horizon;
    v.acc_clamped    = std::max(v.acc_clamped, m.clamped);

    // The crossing frame turns the view a long way on purpose: that is the
    // portal, and letting it into the bucket would mark every crossing a snap.
    const float leak = std::exp(-dt / kViewSnapLeak);
    v.recent      *= leak;
    v.recent_turn *= leak;
    v.closing     *= leak;
    if (!crossed) {
        v.recent      += m.unexplained;
        v.recent_turn += m.turn;
    }

    // How far the view is from where the last crossing left it, and how much of
    // that gap has closed inside one leak time. A view that walks back to the
    // exit direction under the mouse closes it slowly; one that is put back
    // closes it all at once.
    float toexit = 0.0f;
    if (v.have_exit) {
        toexit = view_turn_degrees(v.exit_fwd, fwd);
        if (!crossed) v.closing += std::max(0.0f, v.prev_toexit - toexit);
        v.prev_toexit = toexit;
    }

    // Where the crate is, because it was asked about: "can the snap be the cube
    // that slides back and forth interacting with the portal?". It slides along
    // the opening's own normal, so its distance from the plane says exactly
    // where it is in that travel -- zero is halfway through, and `CUT` is the
    // window where it is in two pieces in two worlds.
    //
    // Bodies reach the renderer and nothing else in this engine, so the answer
    // ought to be no; printing the number is what turns "ought to" into a log
    // the player and I can read together.
    float crate_d   = 0.0f;
    bool  crate_cut = false;
    if (!s.act().bodies.empty() && s.act().portals) {
        const Portal &pa    = s.act().portal_a;
        const Vec3    n     = pa.normal();
        const Vec3    c     = s.act().bodies[0].center;
        const Vec3    plane = pa.plane_point();
        crate_d = (c.x - plane.x) * n.x + (c.y - plane.y) * n.y + (c.z - plane.z) * n.z;

        const Vec3  h     = s.act().bodies[0].half;
        const float reach = std::fabs(n.x) * h.x + std::fabs(n.y) * h.y + std::fabs(n.z) * h.z;
        crate_cut = std::fabs(crate_d) < reach;
    }

    // The marker is there so that a player who saw it can find the moment
    // without reading the whole log.
    const float burst = std::fabs(raw.look_dx) + std::fabs(raw.look_dy);

    if (v.recent      < kViewSnapDegrees * 0.5f) v.snap_armed = true;
    if (v.recent_turn < kViewTurnDegrees * 0.5f) v.fast_armed = true;
    if (v.closing     < kViewReturnDrop * 0.5f)  v.back_armed = true;

    const bool snap = v.have_prev && v.snap_armed && v.recent > kViewSnapDegrees;
    const bool fast = v.have_prev && v.fast_armed &&
                      (v.recent_turn > kViewTurnDegrees || burst > kViewMouseBurst);
    const bool back = v.have_exit && v.back_armed && v.closing > kViewReturnDrop &&
                      toexit < kViewReturnNear;
    const bool event = snap || fast || back;

    if (snap) v.snap_armed = false;
    if (fast) v.fast_armed = false;
    if (back) v.back_armed = false;
    const float warp_now = length(camera_warp(s));

    // A frame that crosses or that snaps is never sampled away. Everything else
    // waits its turn.
    v.since_print += dt;
    const bool due = crossed || event || v.since_print >= kViewTraceSampleSeconds ||
                     (v.left <= 0.0f && v.secs_since_cross - v.last_beat >= kViewBeatSeconds);

    v.left = std::max(0.0f, v.left - dt);

    if (due) {
        v.since_print = 0.0f;

        // Everything but the leading timestamp, so that the ring can be printed
        // with the time it deserves rather than the one it had.
        char line[384];
        std::snprintf(line, sizeof line,
                      "%s w%d n%3d dt %5.2fms mouse %+7.1f %+7.1f raw %+7.1f %+7.1f "
                      "roll %+7.2f turn %6.2f asked %6.2f unexplned %6.2f recent %6.2f "
                      "clamp %5.2f horizon %6.2f warp %5.3f%s crate %+6.2f %s "
                      "toexit %6.2f closing %6.2f eye %7.2f %7.2f %7.2f%s%s%s\n",
                      crossed ? "CROSS" : "     ", s.active, v.acc_frames,
                      (double)(v.acc_secs * 1000.0f / (float)std::max(1, v.acc_frames)),
                      (double)v.acc_dx, (double)v.acc_dy, (double)v.acc_raw_dx,
                      (double)v.acc_raw_dy, (double)(s.roll * 180.0f / 3.14159265f),
                      (double)v.acc_turn, (double)v.acc_asked, (double)v.acc_unexplained,
                      (double)v.recent, (double)v.acc_clamped, (double)v.acc_horizon,
                      (double)warp_now, warp_now > kViewWarpNodes ? " WARP" : "     ",
                      (double)crate_d, crate_cut ? "CUT" : "   ", (double)toexit,
                      (double)v.closing, (double)eye.x, (double)eye.y, (double)eye.z,
                      snap ? "  <-- SNAP" : "", fast ? "  <-- FAST TURN" : "",
                      back ? "  <-- BACK TO THE EXIT DIRECTION" : "");

        v.acc_frames = 0;
        v.acc_secs = v.acc_dx = v.acc_dy = v.acc_raw_dx = v.acc_raw_dy = 0.0f;
        v.acc_turn = v.acc_asked = v.acc_unexplained = 0.0f;
        v.acc_clamped = v.acc_horizon = 0.0f;

        if (event && v.left <= 0.0f) {
            // A snap with no crossing anywhere near it: the case the crossing
            // window cannot catch, and the one the crate question needs. The
            // frames before it, the frame itself, and half a second after.
            if (!v.quiet)
                std::printf("view ---- %s, %.2f s after the last crossing: %.1f degrees "
                            "unasked, %.1f turned in all, %.1f of the gap to the exit "
                            "direction closed, %.1f left\n",
                            back ? "the view went back towards the exit direction"
                                 : (snap ? "a turn nobody asked for"
                                         : (burst > kViewMouseBurst
                                                ? "a mouse burst no hand made"
                                                : "a very fast turn")),
                            (double)v.secs_since_cross, (double)v.recent,
                            (double)v.recent_turn, (double)v.closing, (double)toexit);
            view_trace_dump_ring(v);
            ++v.lines;
            if (!v.quiet) std::printf("view %+7.3fs %s", (double)v.secs_since_cross, line);
            v.left = kViewSnapSeconds;
        } else if (v.left > 0.0f || v.secs_since_cross - v.last_beat >= kViewBeatSeconds) {
            v.last_beat = v.secs_since_cross;
            ++v.lines;
            if (!v.quiet) std::printf("view %+7.3fs %s", (double)v.secs_since_cross, line);
        } else {
            std::snprintf(v.ring[v.ring_next], sizeof v.ring[0], "%s", line);
            v.ring_next = (v.ring_next + 1) % ViewTrace::kRing;
            if (v.ring_next == 0) v.ring_full = true;
        }
    }

    v.have_prev  = true;
    v.prev_fwd   = fwd;
    v.prev_up    = up;
    v.prev_yaw   = player.yaw;
    v.prev_pitch = player.pitch;
    ++v.since_cross;
    v.secs_since_cross += dt;
}

// One place where a world's clock becomes a sky and a sun.
//
// Every session, not just the active one: the world inside the portal has its
// own hour, and drawing it at ours is the same singleton bug the sky and the
// view range were pulled out of in M5. And here rather than in the loops
// because there are several loops and one renderer.
void apply_clocks(Scene &s)
{
    for (int i = 0; i < s.count(); ++i) {
        Session &w = s.at(i);
        w.world_render.forget_shadows();   // drawn again below for the world the eye is in
        if (w.sky_fixed) {
            for (int c = 0; c < 3; ++c) w.sky[c] = w.sky_rgb[c];
            w.world_render.set_daylight(0.0f);
            w.world_render.set_clouds(false, w.sky);
            w.world_render.set_sky_light(nullptr);
            continue;
        }
        sky_at(w.time_of_day, w.sky);
        if (w.storm) {
            // The hour's brightness, in the storm's colour.
            const float lum = 0.3f * w.sky[0] + 0.55f * w.sky[1] + 0.15f * w.sky[2];
            for (int c = 0; c < 3; ++c)
                w.sky[c] = 0.15f * w.sky[c] + 0.85f * std::min(1.0f, w.storm_rgb[c] * (0.25f + 1.1f * lum));
        }
        w.world_render.set_daylight(sunlight_at(w.time_of_day), daylight_at(w.time_of_day));
        const SkyLight sun = sky_light_at(w.time_of_day);
        w.world_render.set_sky_light(&sun);
        // Still while a gate captures: its frames compare a view against a
        // reference taken a few frames apart.
        w.world_render.set_clouds(true, w.sky, g_capture_rt.id ? 0.0f : (float)w.wind,
                                  w.storm ? w.storm_rgb : nullptr);
        w.world_render.set_funnels(w.funnel);
    }
}

// And where the clock moves, for a loop that owns its world. A network client
// does NOT call this: its time is the server's, mirrored, and a client running
// its own clock alongside is the second clock the design says not to have.
void advance_clocks(Scene &s, double dt)
{
    for (int i = 0; i < s.count(); ++i) {
        Session &w = s.at(i);
        w.time_of_day = clock_advance(w.time_of_day, dt, w.day_length);
        w.wind += dt * kWindSpeed;
    }
}

// CNP_HITCH_MS=<ms>: every frame longer than that is printed with its
// sections, the profiler switched on from the start. A hitch seen in play
// lasts one frame, and F6's overlay is gone before anybody can read it.
double hitch_ms()
{
    static const double ms = [] {
        const char *env = std::getenv("CNP_HITCH_MS");
        return env ? std::atof(env) : 0.0;
    }();
    return ms;
}

void log_hitch()
{
    if (hitch_ms() <= 0.0) return;
    if (!diag::prof_enabled()) diag::prof_enable(true);
    const diag::ProfFrame &f = diag::prof_last_frame();
    if (f.frame_ms < hitch_ms()) return;
    std::fprintf(stderr, "hitch %.1f ms:", f.frame_ms);
    for (const diag::ProfSection &sec : f.sections)
        if (sec.self_ms >= 0.5) std::fprintf(stderr, " %s=%.1f/%d", sec.name, sec.self_ms, sec.calls);
    std::fprintf(stderr, " other=%.1f\n", f.unattributed_ms);
}

// The frame boundary, as a scope. A loop body with a `break` or a `continue`
// in the middle of it would otherwise leave a frame open, and an open frame is
// a begin with no end -- which the profiler counts and reports, but counting it
// every frame is not a diagnosis, it is noise.
struct ProfFrameScope {
    ProfFrameScope() { diag::prof_frame_begin(); }
    ~ProfFrameScope()
    {
        diag::prof_frame_end();
        log_hitch();
    }
    ProfFrameScope(const ProfFrameScope &)            = delete;
    ProfFrameScope &operator=(const ProfFrameScope &) = delete;
};

// The profiler overlay, drawn from the frame BEFORE this one.
//
// It has to be: this frame's numbers are not finished until after the last
// thing in it has been drawn, and this is a thing being drawn. A profiler that
// waited for its own frame to end before showing anything would show nothing,
// and one that reported a partial frame would leave everything after the
// overlay out of its own totals for ever.
int draw_profiler_overlay(int y)
{
    const diag::ProfFrame &f = diag::prof_last_frame();
    if (f.frame_ms <= 0.0) return y;

    char line[192];
    std::snprintf(line, sizeof(line), "frame %.2f ms  (%.0f fps)  -- last frame, not this one",
                  f.frame_ms, f.frame_ms > 0.0 ? 1000.0 / f.frame_ms : 0.0);
    plat_debug_text(line, 12, y, 20);
    y += 22;

    for (const diag::ProfSection &sec : f.sections) {
        std::snprintf(line, sizeof(line), "%*s%-10s %6.2f ms self  %6.2f ms total  x%d",
                      sec.depth * 2, "", sec.name, sec.self_ms, sec.total_ms, sec.calls);
        plat_debug_text(line, 12, y, 20);
        y += 22;
    }

    // The remainder, always, even when it is small. It is the only line here
    // that says how much of the frame nobody has instrumented, and a profiler
    // that hides it is the profiler this one was written not to be.
    std::snprintf(line, sizeof(line), "%-10s %6.2f ms  (%.0f%% of the frame)", "unnamed",
                  f.unattributed_ms, 100.0 * f.unattributed_ms / f.frame_ms);
    plat_debug_text(line, 12, y, 20);
    y += 22;

    if (f.unmatched_ends || f.unclosed_sections) {
        std::snprintf(line, sizeof(line), "UNBALANCED: %d end(s) with no begin, %d left open",
                      f.unmatched_ends, f.unclosed_sections);
        plat_debug_text(line, 12, y, 20);
        y += 22;
    }
    return y;
}

// How far back V puts the camera, in nodes: twice the longest model a world wears so far, which
// keeps a player-sized body a quarter of the screen tall.
constexpr float kThirdPersonDistance = 5.0f;

// A camera behind a body that drives turns a little after it. What the wheel
// turned is added to the lag every frame and the lag fades over kCamYawLagTime,
// so a steady bend at the track's 2.6 rad/s is looked at about 0.3 rad late and
// a straight catches up in a few tenths of a second. Capped, so the fastest
// wheel a world may declare cannot swing the body out of the picture.
inline constexpr float kCamYawLagTime = 0.12f;   // seconds
inline constexpr float kCamYawLagMax  = 0.5f;    // radians

// More than a wheel can turn in one update (the fastest declared turn over the
// 0.05 s player_update clamps to) is not a turn: a crossing that turned the
// player, a teleport, a heading set by a script. The camera goes with it
// whole, keeping its lag, instead of swinging round after it.
inline constexpr float kCamYawJump = kMaxDeclaredTurn * 0.05f + 0.05f;

// Once a frame, before the camera is placed. On foot and at the eye there is
// no lag: the mouse turns the view, and a view that trailed the mouse would be
// a view that does not go where the hand asked.
// Whether the camera from above has this world: there, going or coming back.
// While it has, the mouse points rather than turns.
bool iso_engaged(const Session &w)
{
    return w.iso_on || w.iso_t > 0.0f;
}

// Held on a point at the eye (core.fixed_view nearer than a tenth): first
// person with the cursor free -- the arms drawn, as at a card table.
bool at_fixed_eye(const Session &w)
{
    return w.iso_on && w.iso_fixed && w.iso_distance < 0.1f;
}

void camera_follow(Scene &s, const Player &player, float dt)
{
    // The camera from above coming or going (core.iso_view): how far it has
    // got, the body drawn while it is out of the eye, and its field of view
    // eased from the engine's to its own -- a narrow one from far away
    // flattens the picture, as isometric action RPGs'. The engine's again once it is back.
    {
        Session    &w    = s.act();
        const float goal = w.iso_on ? 1.0f : 0.0f;
        if (w.iso_t != goal) {
            const float step = w.iso_secs > 0.0f ? dt / w.iso_secs : 1.0f;
            w.iso_t = goal > w.iso_t ? std::min(goal, w.iso_t + step) : std::max(goal, w.iso_t - step);
        }
        static bool fov_ours = false;
        if (iso_engaged(w)) {
            const float t = std::clamp(w.iso_t, 0.0f, 1.0f), k = t * t * (3.0f - 2.0f * t);
            const float to = w.iso_fov > 0.0f ? w.iso_fov : kFovY;
            g_fov_y        = kFovY + (to - kFovY) * k;
            fov_ours       = true;
            w.view_distance = w.iso_distance;   // the body drawn, no arms, no crosshair
            w.iso_drawn     = true;
        } else {
            if (fov_ours) g_fov_y = kFovY;
            fov_ours = false;
            if (w.iso_drawn) w.view_distance = 0.0f;
            w.iso_drawn = false;
        }
    }

    constexpr float kDriveEyeTime = 0.06f;   // seconds to close most of a bump
    if (player.vehicle) {
        const float want = player.body_base.y;
        if (!s.drive_eye_set || std::fabs(want - s.drive_eye_y) > 2.0f)
            s.drive_eye_y = want;   // put down afresh, or through a portal
        else
            s.drive_eye_y += (want - s.drive_eye_y) * (1.0f - std::exp(-dt / kDriveEyeTime));
        s.drive_eye_set = true;
    } else {
        s.drive_eye_set = false;
    }

    float turned   = std::remainder(player.yaw - s.cam_prev_yaw, 6.2831853f);
    s.cam_prev_yaw = player.yaw;

    if (s.act().view_distance <= 0.0f || !s.act().movement.drives()) {
        s.cam_yaw_lag = 0.0f;
        return;
    }
    if (std::fabs(turned) > kCamYawJump) turned = 0.0f;
    // Up a loop the heading read off the kart has no meaning (straight up it
    // has none at all): over the top it sweeps half a circle in steps under
    // kCamYawJump, which the lag took for a turn and filled to its cap -- the
    // camera came out of the loop 0.5 rad to the side and swung back, and the
    // kart looked to stumble (reported from play, 2026-10-13). None of it
    // counts from where the camera goes over to the kart's own frame (37
    // degrees, kart_view), all of it from 18.
    const float up_y = std::cos(player.body_pitch) * std::cos(player.body_roll);
    turned *= std::clamp((up_y - 0.8f) / 0.15f, 0.0f, 1.0f);

    const float lag = (s.cam_yaw_lag + turned) * std::exp(-dt / kCamYawLagTime);
    s.cam_yaw_lag   = std::clamp(lag, -kCamYawLagMax, kCamYawLagMax);
}

// The direction a camera behind the player is turned to: the player's pitch,
// and their yaw less what the camera has not caught up with yet.
Vec3 camera_look(const Scene &s, const Player &player)
{
    return dir_from_angles(player.yaw - s.cam_yaw_lag, player.pitch);
}

// Where the camera is: the eye, or behind and a little above it when this world
// is seen from outside (Session::view_distance). Pulled in along the same line
// when a block is in the way, so a wall behind the player is never the whole
// picture. The ray starts at the head, which the body's collision already keeps
// out of every block.
//
// NOT a portal-aware camera, said where it is decided: a camera behind a player
// who has just walked out of an opening can sit on the far side of it, and what
// it draws there is the world the player left, not the one behind them.
// The eye the camera is placed from: the body's, and driving, raised or
// lowered to where the chassis is rather than the feet (Scene::drive_eye_y).
Vec3 eye_of(const Scene &s, const Player &player)
{
    Vec3 e = player.eye();
    if (s.drive_eye_set) e.y += s.drive_eye_y - player.pos.y;
    return e;
}

// The camera from above: one fixed way down onto the head, never turned by
// the mouse, never pulled in (a roof would put it inside the house).
Vec3 iso_look(const Session &w)
{
    return dir_from_angles(w.iso_yaw, -w.iso_pitch);
}

// How far between the eye and above the camera is, eased at both ends.
float iso_mix(const Session &w)
{
    const float t = std::clamp(w.iso_t, 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

// What the camera from above looks at: the head, or core.fixed_view's point.
Vec3 iso_center(const Session &w, Vec3 head)
{
    return w.iso_fixed ? w.iso_target : head;
}

Mat4 kart_turn(const Player &player);

// Behind a kart up a loop or a wall ride: in the kart's own frame, behind it
// and over it along its up, and nearer -- with the world's up the head was
// over a loop's top, outside it, and the camera behind it went on through the
// road (only blocks pull a camera in): the near plane cut the road away and
// the kart was seen from under its road. How much of that (0 on a road less
// steep than 37 degrees, all of it from 72), and the frame.
struct KartView {
    float w = 0.0f;
    Vec3  head, fwd, up;
};
KartView kart_view(const Scene &s, const Player &player)
{
    KartView k;
    if (!player.vehicle || s.act().view_distance <= 0.0f || iso_engaged(s.act())) return k;
    const Mat4 turn = kart_turn(player);
    k.up  = mat4_transform_dir(turn, Vec3{0.0f, 1.0f, 0.0f});
    k.fwd = mat4_transform_dir(turn, Vec3{0.0f, 0.0f, -1.0f});
    k.w   = std::clamp((0.8f - k.up.y) / 0.5f, 0.0f, 1.0f);
    k.w   = k.w * k.w * (3.0f - 2.0f * k.w);
    k.head = player.body_base + k.up * (player.eye().y - player.pos.y);
    return k;
}

Vec3 camera_eye(const Scene &s, const Player &player)
{
    const Vec3  head = eye_of(s, player);
    if (const Session &w = s.act(); iso_engaged(w)) {
        const Vec3 above = iso_center(w, head) - iso_look(w) * w.iso_distance;
        return head + (above - head) * iso_mix(w);
    }
    const float d    = s.act().view_distance;
    if (d <= 0.0f) return s.driving_view ? s.rider_eye : s.seat_view ? s.seat_eye : head;

    const Vec3  f    = camera_look(s, player);
    const Vec3  want = Vec3{head.x - f.x * d, head.y - f.y * d + d * 0.3f, head.z - f.z * d};
    const Vec3  to   = want - head;
    const float len  = length(to);
    if (len < 1e-3f) return head;
    const Vec3 dir = to * (1.0f / len);

    const RaycastHit hit = raycast(s.act().world, s.act().registry, head, dir, len);
    const float      go  = hit.hit ? std::max(0.0f, hit.distance - 0.3f) : len;
    const Vec3       world_eye = head + dir * go;

    const KartView k = kart_view(s, player);
    if (k.w <= 0.0f) return world_eye;
    const float      kd   = d * (1.0f - 0.5f * k.w);
    const Vec3       kto  = k.up * (0.3f * kd) - k.fwd * kd;
    const float      klen = length(kto);
    const RaycastHit khit = raycast(s.act().world, s.act().registry, k.head, kto * (1.0f / klen), klen);
    const Vec3       kart_eye = k.head + kto * ((khit.hit ? std::max(0.0f, khit.distance - 0.3f) : klen) / klen);
    return world_eye + (kart_eye - world_eye) * k.w;
}

// Which way that camera looks. The player's own direction at the eye; from
// behind, at a point two nodes ahead of the head along camera_look. Looking the
// player's way from a camera raised above them put the body under the bottom of
// the screen -- measured on the track, a driven body half hidden behind the hotbar.
// Driving in first person the eye is the driver's, and turns as the kart
// does: its heading, and its nose up a climb and its side down a bank or up a
// wall -- the same turn the kart's model is drawn with (body_place).
Mat4 kart_turn(const Player &player)
{
    return mat4_multiply(mat4_multiply(mat4_rotate_y(-player.yaw), mat4_rotate_x(player.body_pitch)),
                         mat4_rotate_z(-player.body_roll));
}

// Which way is up on the screen: the kart's up, driving in first person.
Vec3 camera_up(const Scene &s, const Player &player)
{
    if (iso_engaged(s.act())) return Vec3{0.0f, 1.0f, 0.0f};
    if (s.driving_view) return mat4_transform_dir(kart_turn(player), Vec3{0.0f, 1.0f, 0.0f});
    const KartView k = kart_view(s, player);
    return k.w > 0.0f ? normalize(player.up() + (k.up - player.up()) * k.w) : player.up();
}

Vec3 camera_forward(const Scene &s, const Player &player, Vec3 eye)
{
    // Driving, the eye looks a little down from where the kart points, so the
    // wheel and the hands on it are in the picture and the road still is.
    constexpr float kDriverLooksDown = 0.3f;   // radians, about 17 degrees
    if (const Session &w = s.act(); iso_engaged(w)) {
        // At a point that slides from two nodes ahead along the eye's own
        // look -- the one it had when it set off, or where the body faces on
        // the way back -- to the head, as the camera rises from the eye to
        // above: the player stays in the middle of the picture all the way.
        // Held on a point, the same slide ends at the point.
        const Vec3  from = w.iso_on ? w.iso_from : player.forward();
        const float k    = iso_mix(w);
        const Vec3  head = eye_of(s, player);
        const Vec3  dir  = head + from * 2.0f + (iso_center(w, head) - head - from * 2.0f) * k - eye;
        return length(dir) > 1e-4f ? normalize(dir) : iso_look(w);
    }
    if (s.act().view_distance <= 0.0f)
        return s.driving_view ? mat4_transform_dir(kart_turn(player),
                                                   dir_from_angles(0.0f, player.pitch - kDriverLooksDown))
                              : player.forward();
    const Vec3  f   = camera_look(s, player);
    Vec3        at  = eye_of(s, player) + f * 2.0f;
    if (const KartView k = kart_view(s, player); k.w > 0.0f)
        at = at + (k.head + k.fwd * 2.0f - at) * k.w;
    const Vec3  to  = at - eye;
    const float len = length(to);
    return len > 1e-3f ? to * (1.0f / len) : f;
}

// From the eye to behind the head and back with the crosshair on the same
// thing: the camera moves, so the look turns until the new camera's middle
// meets the point -- a block or a body -- the old one's did. Not for a kart,
// whose look is its heading.
void set_view_distance(Scene &s, float d)
{
    Session &w = s.act();
    if (s.player == nullptr || s.player->vehicle || iso_engaged(w)) {
        w.view_distance = d;
        return;
    }
    Player     &player = *s.player;
    const Vec3  eye    = camera_eye(s, player);
    const Vec3  fwd    = camera_forward(s, player, eye);
    constexpr float kFar = 256.0f;
    const RaycastHit hit = raycast(w.world, w.registry, eye, fwd, kFar);
    float at = hit.hit ? hit.distance : kFar;
    if (s.conn) {
        // Not the body the camera sits in (a seat's machine): nothing within a node.
        float body = at;
        if (aimed_body(*s.conn, w.models, eye + fwd, fwd, at - 1.0f, body) != 0) at = body + 1.0f;
    }
    const Vec3 point = eye + fwd * at;

    w.view_distance = d;
    for (int i = 0; i < 6; ++i) {   // the camera moves with the look: a few rounds settle it
        const Vec3  e   = camera_eye(s, player);
        const Vec3  f   = camera_forward(s, player, e);
        const Vec3  to  = point - e;
        const float len = length(to);
        if (len < 1.0f) break;
        const Vec3 want = to * (1.0f / len);
        const float dyaw = std::atan2(want.x, -want.z) - std::atan2(f.x, -f.z);
        player.yaw   += std::remainder(dyaw, 6.2831853f);
        player.pitch  = std::clamp(player.pitch + std::asin(std::clamp(want.y, -1.0f, 1.0f)) -
                                       std::asin(std::clamp(f.y, -1.0f, 1.0f)),
                                   -kMaxPitch, kMaxPitch);
    }
}

// The camera from above: the mouse points rather than turns, the keys walk
// along the screen's axes, and a walk world code steers (core.walk) turns the
// body to where it goes. Without either the keys are left as they were.
void steer_input(Scene &s, Player &player, PlayerInput &in)
{
    const bool iso = iso_engaged(s.act());
    plat_cursor_free(iso);
    float wx = 0.0f, wz = 0.0f, k = 1.0f;
    if (iso && s.act().iso_fixed) {
        // Looking at a point the body has nothing to do: it stands.
        in.look_dx = in.look_dy = 0.0f;
        in.move_forward = in.move_right = 0.0f;
        in.jump = in.sprint = in.descend = in.toggle_fly = false;
        return;
    }
    if (iso) {
        in.look_dx = in.look_dy = 0.0f;
        player.pitch = 0.0f;
        const Vec3  f = iso_look(s.act());
        const float l = std::hypot(f.x, f.z);
        const float fx = f.x / l, fz = f.z / l;
        wx = fx * in.move_forward - fz * in.move_right;
        wz = fz * in.move_forward + fx * in.move_right;
    }
    // Keys held win over a walk at the eye too: there a walk that took the
    // body each frame left the mouse no turn and the keys no step, for as long
    // as its goal was out of reach (reported from play, 2026-10-16).
    const Session &w = s.act();
    if (w.mod_walk && wx == 0.0f && wz == 0.0f && in.move_forward == 0.0f && in.move_right == 0.0f) {
        wx = w.mod_walk_x;
        wz = w.mod_walk_z;
        k  = w.mod_walk_speed;
    }
    if (wx == 0.0f && wz == 0.0f) return;
    player.yaw      = std::atan2(wx, -wz);
    in.move_forward = k;
    in.move_right   = 0.0f;
}

// What only this machine knows about its own body, for the clip it plays: the
// wire says where a body is and which way it faces, not whether it is flying.
struct OwnBody {
    bool        flying    = false;
    bool        on_ground = true;
    bool        hanging   = false;   // from a ledge by the hands
    bool        mantling  = false;   // pulling up over it
    bool        stepping  = false;   // climbing onto a block one high from the ground
    const char *working   = nullptr;   // the clip of the dig in progress, if any
    const char *parkour   = nullptr;   // the clip of a parkour move (parkour_clip)
    Vec3        velocity;              // m/s, for the lean of a flight (fly_lean)
};

// The clip of what a body doing parkour is doing (MovementRules::parkour), or
// nothing: a backflip and a kick off a wall for as long as their clips last.
const char *parkour_clip(const Player &p)
{
    if (p.flip > 0.0f)       return "backflip";
    if (p.kick > 0.0f)       return "wall_jump";
    if (p.wall_side > 0)     return "wall_run_right";
    if (p.wall_side < 0)     return "wall_run_left";
    if (p.low)               return "power_slide";
    if (p.roped && !p.on_ground) return "rope_swing";
    return nullptr;
}

// Whether a body seen from outside runs along a wall: nothing under its feet,
// going level and fast, a wall against its right (+1) or its left (-1).
int runs_on_wall(const Session &w, const Session::BodyMotion &m, Vec3 feet, float yaw)
{
    if (!w.movement.parkour || m.speed < 3.0f || std::fabs(m.climb) > 1.0f) return 0;
    const AABB below{Vec3{feet.x - 0.3f, feet.y - 0.1f, feet.z - 0.3f},
                     Vec3{feet.x + 0.3f, feet.y, feet.z + 0.3f}};
    if (box_hits_world(w.world, w.registry, below)) return 0;
    const float rx = std::cos(yaw), rz = std::sin(yaw);
    for (int side : {1, -1}) {
        const Vec3 at{feet.x + rx * 0.15f * (float)side, feet.y + 0.1f, feet.z + rz * 0.15f * (float)side};
        if (box_hits_world(w.world, w.registry, body_box(at, 0.3f, 0.0f, 1.6f))) return side;
    }
    return 0;
}

// The clip a body swings with: by the tool in its hand, and with no tool by
// what the block is made of (the tools mod's materials) -- a pick's swing when
// that is not known, which is somebody else's bare hand.
const char *work_clip(const std::string &item, const std::string &material)
{
    const std::string hand = item_short_name(item);
    if (hand == "pick")   return "mine";
    if (hand == "shovel") return "dig";
    if (hand == "axe")    return "chop";
    if (hand == "sword")  return "sword_slash";
    return material == "wood" ? "chop" : material == "soil" ? "dig" : "mine";
}

// A model with wheel_* bones is a vehicle: it rolls, and somebody sits in it.
bool has_wheels(const SkinnedModel &m)
{
    for (const std::string &name : m.joint_names)
        if (name.rfind("wheel_", 0) == 0) return true;
    return false;
}

// A named clip playing on a model, with the one before it fading out over a
// fifth of a second. A name the model has no clip for keeps what it played:
// "swim" on a model that cannot swim goes on walking.
// A clip that moves the arms and nothing below them: no channel on the
// hips, the legs or the root. Laid over the body's own clip rather than
// played instead of it (animate_body).
bool arms_only_clip(const SkinnedModel &m, const char *name)
{
    const int c = name ? m.clip(name) : -1;
    if (c < 0) return false;
    for (const ClipChannel &ch : m.clips[(size_t)c].channels) {
        if (ch.joint < 0 || ch.joint >= m.joint_count) continue;
        const std::string &j = m.joint_names[(size_t)ch.joint];
        if (j == "root" || j == "hips" || j.rfind("thigh_", 0) == 0 || j.rfind("shin_", 0) == 0 ||
            j.rfind("foot_", 0) == 0)
            return false;
    }
    return true;
}

void play(Session::BodyMotion::Playing &p, const SkinnedModel &m, const char *name, double now,
          ClipAt out[2])
{
    if (m.clip(name) >= 0 && p.name != name) {
        p.was       = p.name;
        p.was_start = p.start;
        p.name      = name;
        p.start     = now;
        p.since     = p.was.empty() ? now - 1.0 : now;   // nothing to fade from
    }
    auto at = [&](const std::string &n, double start) {
        ClipAt c;
        c.clip = m.clip(n);
        if (c.clip >= 0) {
            const float d = m.clips[(size_t)c.clip].duration;
            // "death" plays once and holds its last pose (core.ecs.clip); the
            // rest go round.
            c.time = d <= 0.0f    ? 0.0f
                   : n == "death" ? std::min((float)(now - start), d * 0.999f)
                                  : std::fmod((float)(now - start), d);
        }
        return c;
    };
    const float fade = (float)std::min(1.0, (now - p.since) / 0.2);
    out[0]           = at(p.was, p.was_start);
    out[0].weight    = fade < 1.0f ? 1.0f : 0.0f;
    out[1]           = at(p.name, p.start);
    out[1].weight    = fade;
}

// How a body wearing a model is moving, and what that makes it do. Read off
// its positions: the wire carries a position and a yaw and nothing else, and
// how a body moves is what its legs and its wheels would say anyway.
//
// Wheels: which way they point and how far they have rolled. A remote body
// moves 20 times a second, so a frame where nothing changed keeps what the
// last change said; half a second of nothing and the wheel is straight.
//
// Clips, by name, on a model that has them: idle, walk, run, jump_loop, swim,
// swim_idle, slide (riding a slide), and for this machine's own body fly,
// fly_hover and a dig (`own`).
// A body wearing a vehicle has `rider` sat in it -- the world's player model --
// playing kart_enter once and then drive_idle, leaning into drive_left or
// drive_right as far as the wheel is turned.
constexpr uint64_t kOwnBody = ~0ull;

// Whether a body seen from outside is hanging from a ledge: nothing under its
// feet, and a ledge by its hands (the server's own test, body_at_ledge).
bool hangs(const Session &w, Vec3 feet, int *dx = nullptr, int *dz = nullptr)
{
    const AABB below{Vec3{feet.x - 0.3f, feet.y - 0.1f, feet.z - 0.3f},
                     Vec3{feet.x + 0.3f, feet.y, feet.z + 0.3f}};
    return !box_hits_world(w.world, w.registry, below) &&
           body_at_ledge(w.world, w.registry, feet, 0.3f, dx, dz);
}

// Whether a body is riding a slide (chute_at): told by where the body is, so
// the same for this machine's own body and for anybody else's.
bool slides(const Session &w, Vec3 feet)
{
    return chute_at(w.world, w.registry, feet.x, feet.y, feet.z).on;
}

// Whether a body seen from outside is climbing onto a block one high
// (kStepUpTime): rising, nothing under its feet, and the top of a block over
// them by at most a node, along the axis it faces most -- the one the climb
// takes (player_update) -- or the clip it started not over yet, since the last
// of the climb is a walk onto the block. A jump at such a block looks the
// same, and is drawn the same.
bool steps_up(const Session &w, const Session::BodyMotion &m, const SkinnedModel &model, Vec3 feet,
              float yaw)
{
    if (const int c = model.clip("step_up"); c >= 0 && m.body.name == "step_up" &&
                                             plat_time() - m.body.start < model.clips[(size_t)c].duration)
        return true;
    if (m.climb < 0.8f || m.body.name == "pull_up" || m.body.name.rfind("hang", 0) == 0) return false;
    const AABB below{Vec3{feet.x - 0.3f, feet.y - 0.1f, feet.z - 0.3f},
                     Vec3{feet.x + 0.3f, feet.y, feet.z + 0.3f}};
    if (box_hits_world(w.world, w.registry, below)) return false;
    const float fx = std::sin(yaw), fz = -std::cos(yaw);
    const int   dx = std::fabs(fx) >= std::fabs(fz) ? (fx > 0 ? 1 : -1) : 0;
    const int   dz = dx == 0 ? (fz > 0 ? 1 : -1) : 0;
    // Over the feet: the ground it rose from is level with them.
    const Ledge l = find_ledge(w.world, w.registry, feet, 0.3f, dx, dz, 0.1f, 1.05f);
    const float along = dx != 0 ? feet.x : feet.z;
    const float gap   = dx + dz > 0 ? l.face - (along + 0.3f) : (along - 0.3f) - l.face;
    return l.found && gap < 0.3f;
}

// Fire on a creature in the sun: three small boxes going round it and up it,
// shrinking and yellowing as they rise. Unlit, like every box, so they glow.
void add_flames(std::vector<Body> &out, Vec3 feet, uint64_t key)
{
    const double now = plat_time();
    for (int i = 0; i < 3; ++i) {
        const float up = (float)std::fmod(now * 1.5 + i / 3.0 + (double)(key % 7) * 0.13, 1.0);
        const float a  = (float)std::fmod(now * 2.0, 6.2831853) + (float)i * 2.094f;
        const float h  = 0.16f * (1.0f - up) + 0.04f;

        Body b;
        b.center    = Vec3{feet.x + std::cos(a) * 0.25f, feet.y + 0.3f + up * 1.6f,
                        feet.z + std::sin(a) * 0.25f};
        b.half      = Vec3{h, h, h};
        b.colour[0] = 1.0f;
        b.colour[1] = 0.45f + 0.4f * up;
        b.colour[2] = 0.05f;
        out.push_back(b);
    }
}

// A body flying leans the way it goes: the fly clip blended over fly_hover as
// it goes forwards, and the whole body tipped back going backwards and onto
// its side going sideways, about its middle -- every direction between mixed
// by how much of the velocity points along each. Eased, so it leans in and
// straightens out again after landing. Somebody else's too, since the wire
// says who flies (protocol 97), by how their drawn body moves.
void fly_lean(const Session &w, Session::BodyMotion &m, const SkinnedModel &model, Body &b, Vec3 v,
              double now)
{
    constexpr float kTip     = 0.45f;   // 26 degrees, at the flying speed
    constexpr float kFlyFull = 3.0f;    // m/s forwards that is all fly clip
    const float dt = (float)std::clamp(now - m.lean_at, 0.0, 0.1);
    m.lean_at      = now;
    const float fs = std::sin(b.yaw), fc = std::cos(b.yaw);
    const float k  = 1.0f - std::exp(-dt / 0.2f);
    m.lean_ahead += (v.x * fs - v.z * fc - m.lean_ahead) * k;   // along Player::forward
    m.lean_right += (v.x * fc + v.z * fs - m.lean_right) * k;

    const int hover = model.clip("fly_hover"), fly = model.clip("fly");
    if (fly >= 0 && hover >= 0 && b.clips[1].clip == hover && b.clips[1].weight >= 1.0f &&
        m.lean_ahead > 0.0f) {
        b.clips[0] = b.clips[1];
        b.clips[1] = ClipAt{fly, std::fmod(b.clips[0].time, std::max(model.clips[(size_t)fly].duration, 0.01f)),
                            std::min(m.lean_ahead / kFlyFull, 1.0f)};
    }

    const float ref   = std::max(w.movement.fly, 1.0f);
    const float back  = kTip * std::clamp(-m.lean_ahead / ref, 0.0f, 1.0f);
    const float side  = kTip * std::clamp(m.lean_right / ref, -1.0f, 1.0f);
    if (back < 1e-3f && std::fabs(side) < 1e-3f) return;
    b.pitch += back;   // nose up
    b.roll  += side;   // right side down
    // body_place tips about the feet: the centre moved so the middle stays.
    const Vec3 mid{0.0f, b.half.y, 0.0f};
    const Vec3 tipped = mat4_transform_dir(
        mat4_multiply(mat4_rotate_y(-b.yaw), mat4_multiply(mat4_rotate_x(b.pitch), mat4_rotate_z(-b.roll))), mid);
    b.center = b.center + mid - tipped;
}

// Where somebody else is drawn this frame (BodyMotion::glide_from): from where
// they were drawn when the last position came to where it says, over the time
// positions have been coming apart, so a body moves every frame and arrives as
// the next one does. A body put somewhere -- far, or after a silence -- is put
// there.
void glide(Session::BodyMotion &m, net::RemotePlayer &p, double now)
{
    const Vec3 sent{p.x, p.y, p.z};
    if (m.glide_at < 0.0 || now - m.glide_used > 0.5 || length(sent - m.glide_to) > 5.0f) {
        m.glide_from = m.glide_to = m.shown = sent;
        m.yaw_from = m.yaw_to = m.shown_yaw = p.yaw;
        m.glide_at   = now;
        m.glide_seen = p.last_seen;
    } else if (p.last_seen != m.glide_seen) {
        const float gap = (float)std::clamp(p.last_seen - m.glide_seen, 1.0 / 120.0, 0.25);
        m.glide_gap += (gap - m.glide_gap) * 0.2f;
        m.glide_from = m.shown;
        m.glide_to   = sent;
        m.yaw_from   = m.shown_yaw;
        m.yaw_to     = p.yaw;
        m.glide_at   = now;
        m.glide_seen = p.last_seen;
    }
    m.glide_used = now;
    const float t = std::min(1.0f, (float)(now - m.glide_at) / m.glide_gap);
    m.shown     = m.glide_from + (m.glide_to - m.glide_from) * t;
    m.shown_yaw = m.yaw_from + std::remainder(m.yaw_to - m.yaw_from, 6.2831853f) * t;
    p.x = m.shown.x, p.y = m.shown.y, p.z = m.shown.z;
    p.yaw = m.shown_yaw;
}

// The machine this player drives a frame on (Scene::Predicted), run with the
// keys its world's code gave it. When the server has said where it is, that is
// held against where it was predicted to be a round trip ago -- the keys went
// out, a tick ran them, the pose came back -- and a fifth of the difference
// made up, the history moved with it so the next comparison does not count it
// again; a difference too big to ease puts it where the server says, at the
// speed its last two poses give, a round trip on.
void predict_machine(Scene &s, const net::RemotePlayer &host, double rtt, double now, float dt)
{
    Scene::Predicted &pr = s.predicted;
    craft_step(*pr.sim, s.act().world, s.act().registry, pr.keys, dt);
    CraftPose at = craft_pose(*pr.sim);
    if (host.last_seen != pr.seen) {
        pr.seen = host.last_seen;
        const Vec3  said{host.x, host.y, host.z};
        const float roll = host.chassis ? host.body_roll : 0.0f;
        const Vec3  vel  = pr.last_at >= 0.0 && host.last_seen > pr.last_at
                               ? (said - pr.last_base) * (float)(1.0 / std::max(host.last_seen - pr.last_at, 0.02))
                               : at.velocity;
        pr.last_base = said;
        pr.last_at   = host.last_seen;
        const double lag  = rtt + 0.05;
        const CraftPose *then = nullptr;
        double           best = 1e9;
        for (const auto &[t, p] : pr.history)
            if (std::fabs(now - lag - t) < best) best = std::fabs(now - lag - t), then = &p;
        if (then != nullptr) {
            const Vec3  off  = said - then->base;
            const float yaw  = std::remainder(host.yaw - then->yaw, 6.2831853f);
            const float tip  = host.pitch - then->pitch, lean = roll - then->roll;
            ++pr.compared;
            pr.off_sum += length(off);
            if (length(off) > 3.0f || std::fabs(yaw) > 0.6f) {
                at.base     = said + vel * (float)lag;
                at.yaw      = host.yaw;
                at.pitch    = host.pitch;
                at.roll     = roll;
                at.velocity = vel;
                pr.history.clear();
                ++pr.resets;
            } else {
                constexpr float k = 0.2f;
                at.base += off * k;
                at.yaw += yaw * k;
                at.pitch += tip * k;
                at.roll += lean * k;
                for (auto &[t, p] : pr.history) {
                    p.base += off * k;
                    p.yaw += yaw * k;
                    p.pitch += tip * k;
                    p.roll += lean * k;
                }
            }
            craft_place(*pr.sim, at);
        }
    }
    pr.history.push_back({now, at});
    while (pr.history.size() > 2 && pr.history.front().first < now - 1.5) pr.history.pop_front();
}

void animate_body(Session &w, uint64_t key, Body &b, Vec3 feet, uint16_t rider,
                  const OwnBody *own = nullptr, const char *working = nullptr,
                  bool crouching = false, bool lying = false, bool flying = false)
{
    // A turn this fast is full lock. ponytail: one number for every world (a
    // kart on the track turns 2.6 rad/s at most); a world's own if one needs it.
    constexpr float kFullLockRate = 2.0f;

    Session::BodyMotion &m  = w.motion[key];
    const double         now = plat_time();
    const double         dt  = now - m.at;
    const Vec3           d   = feet - m.pos;
    const bool           fresh = m.at < 0.0;   // never seen here before

    if (m.at < 0.0 || dt > 0.5 || length(d) > 5.0f) {   // new, stopped, or put somewhere
        m.pos   = feet;
        m.yaw   = b.yaw;
        m.at    = now;
        m.steer = m.speed = m.climb = 0.0f;
        m.velocity = Vec3{};
    } else if (d.x != 0.0f || d.y != 0.0f || d.z != 0.0f || b.yaw != m.yaw) {
        // On a belt the body is carried, not walking: what the belt moved it
        // is taken off, so standing on one is standing still.
        Vec3 step = d;
        if (const Belt belt = belt_under(w.world, w.registry, feet.x, feet.y, feet.z); belt.on) {
            step.x -= belt.dx * belt.speed * (float)dt;
            step.z -= belt.dz * belt.speed * (float)dt;
        }
        const float ahead  = step.x * std::sin(b.yaw) - step.z * std::cos(b.yaw);   // along Player::forward
        const float turned = std::remainder(b.yaw - m.yaw, 6.2831853f);
        float       target = 0.0f;
        if (std::fabs(ahead) > 0.5f * (float)dt)   // moving, and not by a hair
            target = std::clamp(turned / (float)dt / kFullLockRate * (ahead > 0.0f ? 1.0f : -1.0f),
                                -1.0f, 1.0f);
        const float k = std::min(1.0f, (float)dt / 0.1f);
        m.steer += (target - m.steer) * k;
        m.speed += (std::sqrt(step.x * step.x + step.z * step.z) / (float)dt - m.speed) * k;
        m.climb += (step.y / (float)dt - m.climb) * k;
        m.velocity = m.velocity + (Vec3{step.x, step.y, step.z} * (1.0f / (float)dt) - m.velocity) * k;
        m.travelled += ahead;
        m.walked += std::sqrt(step.x * step.x + step.z * step.z);
        m.pos = feet;
        m.yaw = b.yaw;
        m.at  = now;
    }
    b.steer     = m.steer;
    b.travelled = m.travelled;

    const ModelStore::Entry *e = w.models.entry(b.model);
    if (e == nullptr) return;

    if (has_wheels(e->model)) {
        const ModelStore::Entry *r = rider != b.model ? w.models.entry(rider) : nullptr;
        if (r == nullptr || r->model.clip("drive_idle") < 0) return;
        // Getting in is seen only by who saw the body on foot: one first seen
        // here already in its kart -- driven in through a mouth, or come into
        // view -- is sat in it (kart_enter played on arrival, reported).
        if (!m.driving) m.drove_at = fresh ? now - 1000.0 : now;
        m.driving = true;

        const SkinnedModel &rm    = r->model;
        const int           enter = rm.clip("kart_enter");
        const float         since = (float)(now - m.drove_at);
        b.rider                   = rider;
        if (enter >= 0 && since < rm.clips[(size_t)enter].duration) {
            b.rider_clips[0] = ClipAt{enter, since, 1.0f};
            return;
        }
        const int   idle = rm.clip("drive_idle");
        const int   lean = rm.clip(b.steer < 0.0f ? "drive_left" : "drive_right");
        const float t    = std::fmod(since, std::max(rm.clips[(size_t)idle].duration, 0.01f));
        b.rider_clips[0] = ClipAt{idle, t, 1.0f};
        b.rider_clips[1] = ClipAt{lean, t, std::fabs(b.steer)};
        return;
    }
    m.driving = false;
    if (e->model.clips.empty()) return;

    // A mod's clip may be two: "base+arms", the body playing the first and
    // the arms the second over it -- sat in a jeep's back seat, a gun fired.
    std::string base_part;
    const char *arms = working;
    if (working)
        if (const char *plus = std::strchr(working, '+')) {
            base_part.assign(working, plus);
            arms    = plus + 1;
            working = base_part.c_str();
        }

    const bool wet = w.registry.is_liquid(w.world.get_block(
        (int)std::floor(feet.x), (int)std::floor(feet.y + 0.5f), (int)std::floor(feet.z)));
    const char *name;
    const int wall = own ? 0 : runs_on_wall(w, m, feet, b.yaw);
    if (lying)                               name = e->model.clip("sleep") >= 0 ? "sleep" : "idle";
    else if (own && own->parkour && e->model.clip(own->parkour) >= 0) name = own->parkour;
    else if (wall != 0 && e->model.clip("wall_run_left") >= 0)
        name = wall > 0 ? "wall_run_right" : "wall_run_left";
    // Somebody else low and fast is sliding.
    else if (!own && crouching && m.speed > 4.5f && w.movement.parkour) name = "power_slide";
    else if (wet)                            name = m.speed > 0.5f ? "swim" : "swim_idle";
    else if (e->model.clip("slide") >= 0 && slides(w, feet)) name = "slide";
    else if (own ? own->flying : flying)     name = "fly_hover";   // fly blended in by fly_lean
    else if (own && own->working)            name = own->working;
    else if (working && !arms_only_clip(e->model, working)) name = working;
    else if (own ? own->stepping : !crouching && steps_up(w, m, e->model, feet, b.yaw))
        name = e->model.clip("step_up") >= 0 ? "step_up" : "jump_loop";
    // Somebody else falling past a ledge -- off a block -- is not holding it.
    else if (int hx = 0, hz = 0;
             own ? (own->hanging || own->mantling) : m.climb > -1.0f && hangs(w, feet, &hx, &hz)) {
        if (!own) b.yaw = std::atan2((float)hx, (float)-hz);   // somebody else faces their wall
        // At a ledge by the hands: pulling up while it rises, moving along
        // it, or hanging still. Somebody else's is told by where they are --
        // nothing under the feet and a ledge by the hands -- and how they move.
        const bool pulls = own ? own->mantling : m.climb > 0.8f;
        name = pulls ? "pull_up" : m.speed > 0.3f ? "hang_shimmy" : "hang";
        if (e->model.clip(name) < 0) name = e->model.clip("hang") >= 0 ? "hang" : "jump_loop";
    }
    else if (box_on_climbable(w.world, w.registry, body_box(feet, 0.3f)) &&
             !(own ? own->on_ground : std::fabs(m.climb) < 0.2f && m.speed > 0.4f)) {
        // On a ladder, off the ground: climbing while it moves up or down,
        // holding on while it does not. A model without the clips falls back
        // to the one it has for being off the ground.
        name = std::fabs(m.climb) > 0.4f ? "climb" : "climb_idle";
        if (e->model.clip(name) < 0) name = "jump_loop";
    }
    else if (own ? !own->on_ground : std::fabs(m.climb) > 2.0f) name = "jump_loop";
    else if (crouching)                      name = m.speed < 0.3f ? "crouch_idle" : "crouch_walk";
    else if (m.speed < 0.4f)                 name = "idle";
    else if (m.speed < 5.5f)                 name = "walk";   // ponytail: at its own pace, whatever the speed
    else                                     name = "run";
    play(m.body, e->model, name, now, b.clips);
    // A clip of the arms alone (a gun held, aimed, reloaded) over whatever
    // the body is doing, faded in over a tenth of a second, round and round.
    b.clips[2] = ClipAt{};
    if (arms && arms_only_clip(e->model, arms)) {
        if (m.arms_name != arms) {
            m.arms_name  = arms;
            m.arms_start = now;
        }
        const int   c = e->model.clip(arms);
        const float d = e->model.clips[(size_t)c].duration;
        b.clips[2] = ClipAt{c, d > 0.0f ? std::fmod((float)(now - m.arms_start), d) : 0.0f,
                            (float)std::min(1.0, (now - m.arms_start) / 0.1)};
    } else {
        m.arms_name.clear();
    }
    if (own) fly_lean(w, m, e->model, b, own->flying ? own->velocity : Vec3{}, now);
    else if (!lying) fly_lean(w, m, e->model, b, flying ? m.velocity : Vec3{}, now);
}

// A block in the hand: a cube `size` across about `centre`, each face the
// block's own tile of the atlas, bound to joint 0 so the hand's bone carries it.
SkinnedModel held_block_model(const NodeDef &def, int tiles, Vec3 centre, float size)
{
    SkinnedModel m;
    const float  h   = size * 0.5f;
    const float  per = 1.0f / (float)std::max(tiles, 1);
    const float  pad = per * 0.02f;   // off the tile's edge, so its neighbour never bleeds in
    static const Vec3 normals[6] = {{1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1}};
    for (int f = 0; f < 6; ++f) {
        const Vec3 n = normals[f];
        const Vec3 v = std::fabs(n.y) > 0.5f ? Vec3{0, 0, -n.y} : Vec3{0, 1, 0};   // the picture's up
        const Vec3 u = cross(v, n);
        const int  t = def.tex[f];
        const float u0 = (float)(t % tiles) * per + pad, u1 = u0 + per - 2 * pad;
        const float v0 = (float)(t / tiles) * per + pad, v1 = v0 + per - 2 * pad;
        const float corner[4][4] = {{-1, -1, u0, v1}, {1, -1, u1, v1}, {1, 1, u1, v0}, {-1, 1, u0, v0}};
        const uint32_t base = (uint32_t)m.vertices.size();
        for (const auto &c : corner) {
            const Vec3 p = centre + (n + u * c[0] + v * c[1]) * h;
            SkinVertex sv{};
            sv.pos[0] = p.x, sv.pos[1] = p.y, sv.pos[2] = p.z;
            sv.normal[0] = n.x, sv.normal[1] = n.y, sv.normal[2] = n.z;
            sv.uv[0] = c[2], sv.uv[1] = c[3];
            sv.weights[0] = 1.0f;
            m.vertices.push_back(sv);
        }
        for (uint32_t k : {0u, 1u, 2u, 0u, 2u, 3u}) m.indices.push_back(base + k);
    }
    m.parts.push_back(ModelPart{0, (uint32_t)m.indices.size(), 0});
    m.materials.push_back(ModelMaterial{});
    m.joint_count = 1;
    m.bind_pose   = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0};
    return m;
}

// A track piece in the hand: its road, curbs and sides (piece_triangles,
// unturned) made small, `size` its longest side, about `centre`. Each
// triangle takes the middle of its tile as emit_piece picks it, and three
// parts shade road, sides and every other curb segment as the world does.
SkinnedModel held_piece_model(const NodeDef &def, int tiles, Vec3 centre, float size)
{
    std::vector<PieceTri> tris;
    piece_triangles(def.piece, 2, tris);
    Vec3 lo{1e9f, 1e9f, 1e9f}, hi{-1e9f, -1e9f, -1e9f};
    for (const PieceTri &t : tris)
        for (const Vec3 &v : t.v) {
            lo = Vec3{std::min(lo.x, v.x), std::min(lo.y, v.y), std::min(lo.z, v.z)};
            hi = Vec3{std::max(hi.x, v.x), std::max(hi.y, v.y), std::max(hi.z, v.z)};
        }
    const Vec3  mid = (lo + hi) * 0.5f;
    const float k   = size / std::max({hi.x - lo.x, hi.y - lo.y, hi.z - lo.z, 1e-3f});
    const float per = 1.0f / (float)std::max(tiles, 1);

    SkinnedModel          m;
    std::vector<uint32_t> part[3];   // road and light curbs, sides, dark curbs
    for (const PieceTri &t : tris) {
        const int tile = def.tex[t.role == kPieceRoad ? kFaceTop : t.role == kPieceCurb ? kFaceBottom
                                                                                      : kFaceRight];
        const int which = t.role == kPieceSide ? 1 : t.role == kPieceCurb && t.stripe ? 2 : 0;
        Vec3      n     = cross(t.v[1] - t.v[0], t.v[2] - t.v[0]);
        n               = n * (1.0f / std::max(length(n), 1e-6f));
        for (const Vec3 &v : t.v) {
            const Vec3 p = centre + (v - mid) * k;
            SkinVertex sv{};
            sv.pos[0] = p.x, sv.pos[1] = p.y, sv.pos[2] = p.z;
            sv.normal[0] = n.x, sv.normal[1] = n.y, sv.normal[2] = n.z;
            sv.uv[0] = ((float)(tile % tiles) + 0.5f) * per;
            sv.uv[1] = ((float)(tile / tiles) + 0.5f) * per;
            sv.weights[0] = 1.0f;
            part[which].push_back((uint32_t)m.vertices.size());
            m.vertices.push_back(sv);
        }
    }
    const float shade[3] = {1.0f, 0.75f, 0.5f};
    for (int i = 0; i < 3; ++i) {
        m.parts.push_back(ModelPart{(uint32_t)m.indices.size(), (uint32_t)part[i].size(), i});
        m.indices.insert(m.indices.end(), part[i].begin(), part[i].end());
        ModelMaterial mat;
        mat.base_colour[0] = mat.base_colour[1] = mat.base_colour[2] = shade[i];
        mat.double_sided = true;   // a wall ride's road is seen from under it too
        m.materials.push_back(mat);
    }
    m.joint_count = 1;
    m.bind_pose   = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0};
    return m;
}

// What is in the palm of a model's hand_R, carried by that bone out of `posed`.
// The item's own model when the world has one ("<item>_held", made about the
// grip, see tools/make_held_models.py), else a cube of the block's own tiles
// (a track piece: the piece in miniature), rebuilt when the block, its tiles
// or the model change. Nothing for air, a
// model with no hand_R, or no atlas yet.
void draw_held_block(HeldCube &cube, Session &w, const SkinnedModel &m,
                     const std::vector<float> &posed, const std::string &item, BlockId block,
                     const Mat4 &projection, const Mat4 &view, const Mat4 &place,
                     const std::string &model, uint32_t tag, float light)
{
    int hand = -1;
    for (int j = 0; j < m.joint_count; ++j)
        if (m.joint_names[(size_t)j] == "hand_R") hand = j;
    if (hand < 0) return;
    const std::vector<float> bone(posed.begin() + hand * 12, posed.begin() + hand * 12 + 12);
    // Sized by the forearm (hand_R's parent), so a bigger robot holds a bigger
    // block: 0.21 is the forearm the numbers were fitted to.
    const int   arm   = m.joint_parent[(size_t)hand];
    const float scale = arm >= 0 ? length(m.joint_rest[(size_t)hand] - m.joint_rest[(size_t)arm]) / 0.21f
                                 : 1.0f;

    ModelStore::Entry *held = !model.empty() ? model_named(w, model)
                            : item.empty()   ? nullptr
                                             : model_named(w, item + "_held");
    if (held != nullptr && !held->model.skinned()) {
        // The bone's 3x4 rows, times the grip in the fist at rest, times the size.
        Mat4 b;
        for (int row = 0; row < 3; ++row)
            for (int col = 0; col < 4; ++col) b.at(row, col) = bone[(size_t)(row * 4 + col)];
        const Vec3 grip = m.joint_rest[(size_t)hand] + Vec3{0.0f, -0.05f, -0.01f} * scale;
        const Mat4 t    = mat4_multiply(b, mat4_multiply(mat4_translate(grip),
                                                         mat4_scale(Vec3{scale, scale, scale})));
        std::vector<float> rows(12);
        for (int row = 0; row < 3; ++row)
            for (int col = 0; col < 4; ++col) rows[(size_t)(row * 4 + col)] = t.at(row, col);
        held->renderer.draw(projection, view, place, rows, gfx_texture{0}, light, 1.0f, tag);
        return;
    }
    if (block == kAir || w.world_render.atlas().id == 0) return;

    const NodeDef &def   = w.registry.get(block);
    const int      tiles = w.world_render.atlas_tiles();
    uint64_t       key   = (uint64_t)block * 1000003u + (uint64_t)tiles + (uint64_t)(uintptr_t)&m;
    for (int f = 0; f < 6; ++f) key = key * 31u + def.tex[f];
    if (key != cube.built) {
        cube.render.shutdown();
        std::string why;
        const Vec3  palm  = m.joint_rest[(size_t)hand] + Vec3{0.0f, -0.08f, -0.06f} * scale;
        const SkinnedModel made = def.piece.plan != 0
                                      ? held_piece_model(def, tiles, palm, 0.24f * scale)
                                      : held_block_model(def, tiles, palm, 0.16f * scale);
        if (!cube.render.init() || !cube.render.upload(made, why))
            std::fprintf(stderr, "held block: %s\n", why.c_str());
        cube.built = key;
    }
    cube.render.draw(projection, view, place, bone, w.world_render.atlas(), light);
}

// The arms seen in first person: the model the world's player model is plus
// "_arms" ("robot:robot_arms"), when the world has one. Authored in the eye's
// own space -- the eye at the origin looking down -Z -- so it is drawn with no
// view at all, and with its depth squeezed into the front hundredth of the
// range: over the world whatever the world has in front of the eye, and still
// in front of and behind itself. Plays "swing" while the left button is down
// (a dig, a hit) and "idle" otherwise; bobs as the body walks; shows the
// tool_* bone of what is held, or the block it holds in the right hand.
// What the body is doing that the arms show instead of the hand's work.
const char *body_arms_clip(const Session &w, const Player &p)
{
    if (p.mantle > 0.0f && !p.stepping) return "pull_up";
    if (p.hanging) return "hang";
    if (!p.on_ground && !p.flying &&
        box_on_climbable(w.world, w.registry, body_box(p.pos, w.movement.width * 0.5f)))
        return "climb";
    return nullptr;
}

// Where the view is, against the wall the hands hold (Scene::arms_held).
void hold_arms(Scene &s, const Player &p)
{
    s.arms_still = !p.on_ground;
    s.arms_held  = p.hanging || (p.mantle > 0.0f && !p.stepping);
    if (!s.arms_held) return;
    const float wall = std::atan2((float)p.hang_dx, (float)-p.hang_dz);
    s.arms_yaw       = std::remainder(p.yaw - wall, 6.2831853f);
    s.arms_pitch     = p.pitch;
}

void draw_arms(Scene &s, const Mat4 &projection)
{
    Session &w = s.act();
    // The species' own arms, else the player model's.
    const ModelStore::Entry *player =
        w.models.entry(species_model(w.models, w.player_model_id, net::g_client_livery));
    const ModelStore::Entry *plain = w.models.entry(w.player_model_id);
    if (player == nullptr || plain == nullptr) return;
    ModelStore::Entry *arms = model_named(w, player->name + "_arms");
    if (arms == nullptr) arms = model_named(w, plain->name + "_arms");
    if (arms == nullptr) return;

    ClipAt clips[2];
    // At rest, the idle of what is held when the arms have one ("idle_shotgun":
    // a gun held its own way), else the one idle.
    const std::string held_idle = "idle_" + item_short_name(s.hand);
    const char *idle = !s.hand.empty() && arms->model.clip(held_idle) >= 0 ? held_idle.c_str() : "idle";
    const char *clip = s.arms_clip && arms->model.clip(s.arms_clip) >= 0 ? s.arms_clip
                     : !s.own_clip.empty() && arms->model.clip(s.own_clip) >= 0 ? s.own_clip.c_str()
                     : s.working                                          ? "swing"
                                                                          : idle;
    if (!s.mod_arms.empty()) {
        const int c = arms->model.clip(s.mod_arms);
        if (c < 0 || plat_time() - s.mod_arms_start >= arms->model.clips[(size_t)c].duration) {
            // Over, and not faded out of: a clip faded out keeps playing, from
            // its start again, and a shot played twice is a second shot.
            s.mod_arms.clear();
            s.arms.name.clear();
        } else {
            clip = s.mod_arms.c_str();
        }
    }
    play(s.arms, arms->model, clip, plat_time(), clips);
    const std::vector<int8_t> show = tools_shown(arms->model, s.hand);
    static std::vector<float> posed;
    pose(arms->model, clips, 2, nullptr, show.data(), posed);

    // A step is about 1.6 m of ground: side to side once and down twice in it,
    // as much as the body is walking -- on the ground. Off it there is no step:
    // arms bobbing through a flight read as walking on air.
    const Session::BodyMotion &me   = w.motion[kOwnBody];
    const float                step = s.arms_still ? 0.0f : std::min(me.speed / 4.0f, 1.0f);
    const float                ph   = me.walked * 3.14159265f / 1.6f;
    Mat4 bob = mat4_translate(
        Vec3{0.012f * step * std::sin(ph), -0.014f * step * std::fabs(std::cos(ph)), 0.0f});

    // Holding a ledge: drawn as a view facing the wall would see them, then
    // turned into this view about the eye -- the hands stay on the ledge and
    // go off the side of the screen as the head turns away from it.
    if (s.arms_held)
        bob = mat4_multiply(mat4_multiply(mat4_rotate_x(-s.arms_pitch), mat4_rotate_y(s.arms_yaw)),
                            bob);

    Mat4 front = mat4_identity();
    front.at(2, 2) = 0.01f;
    front.at(2, 3) = -0.99f;
    const Mat4 proj = mat4_multiply(front, projection);
    // The light where the eye is, so a hand in a dark cave is dark.
    const float light = w.world_render.cell_light(
        w.world.get_light((int)std::floor(s.cam_eye.x), (int)std::floor(s.cam_eye.y), (int)std::floor(s.cam_eye.z)),
        true);
    arms->renderer.draw(proj, mat4_identity(), bob, posed, gfx_texture{0}, light, 1.0f,
                        livery_tag(net::g_client_livery));

    draw_held_block(s.held_arms, w, arms->model, posed, s.hand, s.hand_block, proj, mat4_identity(), bob,
                    s.held_model, s.hand_tag, light);

    // The back piece's own arms in first person, "<piece>_arms" (mods/armpack:
    // its claws at the top corners of the view), with what they hold -- the
    // first-person arms' tools moved into them.
    const size_t back = (size_t)(Inventory::kDefaultSlots + kWornBack - 1);
    if (s.hotbar.items == nullptr || back >= s.hotbar.slots.size() || s.hotbar.slots[back].empty()) return;
    const std::string piece = s.hotbar.items->get(s.hotbar.slots[back].item).name;
    ModelStore::Entry *pack  = model_named(w, piece + "_arms");
    if (pack == nullptr || !pack->model.skinned()) return;
    std::string held[2];
    for (int k = 0; k < Inventory::kArms; ++k) {
        const size_t i = back + 1 + (size_t)k;
        if (i < s.hotbar.slots.size() && !s.hotbar.slots[i].empty())
            held[k] = s.hotbar.items->get(s.hotbar.slots[i].item).name;
    }
    static Session::BodyMotion::Playing playing;
    ClipAt                              pack_clips[2];
    play(playing, pack->model, pack_clip(piece, s.working, held).c_str(), plat_time(), pack_clips);
    static std::vector<float> pack_posed;
    pose(pack->model, pack_clips, 2, nullptr, nullptr, pack_posed);
    pack->renderer.draw(proj, mat4_identity(), bob, pack_posed, gfx_texture{0}, light, 1.0f,
                        (uint32_t)(s.hotbar.slots[back].tag >> kTagIdBits) << kTagIdBits);
    draw_pack_tools(w.models, pack->model, pack_posed, *arms, held, proj, mat4_identity(), bob, light,
                    livery_tag(net::g_client_livery));
}

// The player's own body, for a camera that can see it. The same box the
// server's movement check uses, facing the way the player does, wearing what
// this world dresses players in -- or what core.body_model put on it here, a
// kart while it drives, with the player's own model sat in it.
void add_own_body(Scene &s, const Player &player, uint16_t model)
{
    model = species_model(s.act().models, model, net::g_client_livery);   // as a rider too
    Body b;
    b.center = Vec3{player.pos.x, player.pos.y + s.step.lag + kPlayerHeight * 0.5f, player.pos.z};
    // A kart sits on its chassis, not on the wheel that touches highest.
    if (player.vehicle)
        b.center = player.body_base + Vec3{0.0f, kPlayerHeight * 0.5f, 0.0f};
    const float half = s.act().movement.width * 0.5f;
    b.half   = Vec3{half, kPlayerHeight * 0.5f, half};
    // Facing the wall while hanging from it or pulling up, whichever way the
    // camera looks.
    b.yaw    = (player.hanging || player.mantle > 0.0f)
                   ? std::atan2((float)player.hang_dx, (float)-player.hang_dz)
                   : player.yaw;
    b.pitch  = player.body_pitch;
    b.roll   = player.body_roll;
    b.id     = kOwnBody;
    const DriveLook look = vehicle_look(player);
    b.boosting = look.boosting;
    b.sliding  = look.sliding;
    // Turned as the machine under it at its wheel (a clip of driving alone,
    // drive_*); a gunner, a passenger with a gun, the way it looks -- as the
    // server shows it to everybody else (entities_for).
    const bool at_wheel = s.own_clip.rfind("drive_", 0) == 0 && s.own_clip.find('+') == std::string::npos;
    if (s.seat_on) {
        b.yaw   = at_wheel || s.own_clip.empty() ? s.seat_yaw : player.yaw;
        b.pitch = s.seat_pitch;
        b.roll  = s.seat_roll;
    }
    if (player.lying) lay(b, player.lie_at, player.lie_head);
    b.model  = s.act().own_body_model != 0
                   ? species_model(s.act().models, s.act().own_body_model, net::g_client_livery)
                   : model;
    b.hand_known = true;
    b.hand       = s.hand;
    b.hand_block = s.hand_block;
    b.hand_tag   = s.hand_tag;
    b.livery     = net::g_client_livery;
    std::copy(std::begin(s.own_dress), std::end(s.own_dress), std::begin(b.dress));
    std::copy(std::begin(s.own_dress_tint), std::end(s.own_dress_tint), std::begin(b.dress_tint));
    for (int k = 0; k < Inventory::kWorn && s.hotbar.items != nullptr; ++k) {
        const size_t i = (size_t)(Inventory::kDefaultSlots + k);
        if (i < s.hotbar.slots.size() && !s.hotbar.slots[i].empty()) {
            b.worn[k]      = s.hotbar.items->get(s.hotbar.slots[i].item).name;
            b.worn_tint[k] = (uint16_t)(s.hotbar.slots[i].tag >> kTagIdBits);
        }
    }
    for (int k = 0; k < Inventory::kArms && s.hotbar.items != nullptr; ++k) {
        const size_t i = (size_t)(Inventory::kDefaultSlots + Inventory::kWorn + k);
        if (i < s.hotbar.slots.size() && !s.hotbar.slots[i].empty())
            b.arm[k] = s.hotbar.items->get(s.hotbar.slots[i].item).name;
    }
    b.arms_working = s.working;
    {
        OwnBody own;
        own.flying    = player.flying;
        own.on_ground = player.on_ground;
        own.hanging   = player.hanging;
        own.mantling  = player.mantle > 0.0f;
        own.stepping  = player.stepping;
        own.parkour   = parkour_clip(player);
        own.velocity  = player.velocity;
        // Not over a clip a mod holds the body in: a click in a jeep's seat
        // is the mod's shot, and the body went on digging instead of sitting.
        if (s.working && s.own_clip.empty()) {
            const std::string material =
                s.target.hit ? s.act().registry.get(s.act().world.get_block(s.target.x, s.target.y,
                                                                            s.target.z)).material
                             : std::string();
            own.working = work_clip(s.hand, material);
        }
        // A clip a mod holds this body in (core.player_clip): seated in a
        // machine, holding cards -- what everybody else already sees it play.
        animate_body(s.act(), kOwnBody, b, player.pos, model, &own,
                     s.own_clip.empty() ? nullptr : s.own_clip.c_str(), player.crouching, player.lying);
    }
    // Moved in first person too: the arms bob by how far the body has walked.
    // Driving, the body is drawn at the eye as well, headless, and the eye
    // goes where its head is (camera_eye) -- fixed to the kart, as the kart is
    // drawn: the eased height (Scene::drive_eye_y) is the camera behind's, and
    // added here it left the eye bobbing a moment behind the kart it sat in
    // (reported from play).
    // A rope (core.rope): small orange boxes from the hands to where it holds,
    // in first person too. ponytail: only this machine's own rope; the wire
    // does not say who else is on one.
    if (player.roped) {
        const Vec3  from = player.hands(), d = player.rope_at - from;
        const int   n    = std::min(400, (int)(length(d) / 0.12f) + 1);
        for (int i = 0; i <= n; ++i) {
            Body knot;
            knot.center = from + d * ((float)i / (float)n);
            knot.half   = Vec3{0.035f, 0.035f, 0.035f};
            knot.colour[0] = 1.0f;
            knot.colour[1] = 0.45f;
            knot.colour[2] = 0.06f;
            s.act().bodies.push_back(knot);
        }
    }
    s.driving_view = s.act().view_distance <= 0.0f && b.rider != 0;
    if (s.driving_view) {
        b.rider_headless = true;
        s.rider_eye      = rider_eye(s.act(), b, player.eye());
    }
    s.seat_view = s.act().view_distance <= 0.0f && s.seat_on && !s.driving_view;
    if (s.seat_view) {
        b.arms_only = true;
        s.seat_eye  = seat_eye(s.act(), b, player.eye());
    }
    if (s.act().view_distance <= 0.0f && !s.driving_view && !s.seat_view) return;
    // Looking at a point (core.fixed_view) the camera can pass through the
    // head on its way: the body is left out of this machine's picture.
    if (s.act().iso_fixed && iso_engaged(s.act())) return;
    s.act().bodies.push_back(b);
}

// Where a world hosted from here can be reached, empty when this session is
// not hosting one. The pause menu shows it: a player who is hosting has to be
// able to read the address out to whoever is joining, and the title screen is
// gone by then.
std::string g_hosting_at;

// A run that began on the title screen can go back to it from the pause menu;
// main() then starts this program again once everything has been shut down.
bool g_from_title = false;
bool g_to_title   = false;
// The title's account for the server being played on, when it is one from the
// list: what a restart rejoins (TitleMenu::rejoin).
std::string g_rejoin_account;

// One element of an engine panel -- the pause menu, the title screen -- a cell
// tall. A label sits a third of a cell down, which is where a button's caption
// sits in its box.
void panel_add(ui::Panel &p, ui::ElemKind kind, std::string id, std::string text, float x,
               float y, float w)
{
    ui::Element e;
    e.kind = kind;
    e.id   = std::move(id);
    e.text = std::move(text);
    e.x    = x;
    e.y    = kind == ui::ElemKind::Label ? y + 0.33f : y;
    e.w    = w;
    e.h    = 1.0f;
    p.elements.push_back(std::move(e));
}

void panel_button(ui::Panel &p, const char *id, std::string text, float x, float y, float w)
{
    panel_add(p, ui::ElemKind::Button, id, std::move(text), x, y, w);
}

void panel_label(ui::Panel &p, std::string text, float x, float y)
{
    panel_add(p, ui::ElemKind::Label, "", std::move(text), x, y, 0.0f);
}

// The options, the pause menu's and the title screen's. `world` is null on the
// title screen, where there is nobody to see from behind yet.
ui::Panel options_panel(bool debug, const Session *world)
{
    char sens[48];
    std::snprintf(sens, sizeof(sens), "Mouse sensitivity %d%%", (int)std::lround(g_look_scale * 100.0f));

    char vol[32];
    std::snprintf(vol, sizeof(vol), "Volume %d%%", (int)std::lround(g_volume * 100.0f));

    ui::Panel p;
    p.w = 9.0f;
    p.h = world ? 9.2f : 7.9f;
    panel_label(p, "Options", 0.5f, 0.0f);
    // Back up here, beside the title, so the panel is no taller than before
    // the updates had a button and stays clear of the game's name.
    panel_button(p, "back", "Back", 4.6f, 0.0f, 3.9f);
    panel_label(p, sens, 0.5f, 1.2f);
    panel_button(p, "slower", "-", 5.8f, 1.2f, 1.2f);
    panel_button(p, "faster", "+", 7.3f, 1.2f, 1.2f);
    panel_label(p, vol, 0.5f, 2.5f);
    panel_button(p, "quieter", "-", 5.8f, 2.5f, 1.2f);
    panel_button(p, "louder", "+", 7.3f, 2.5f, 1.2f);
    float y = 3.8f;
    if (world) {
        panel_button(p, "third", world->view_distance > 0.0f ? "Third person: on" : "Third person: off",
                     0.5f, y, 8.0f);
        y += 1.3f;
    }
    // Two to a row, so the panel stays short enough for the title screen's
    // name above it.
    panel_button(p, "lighting", g_classic_light ? "Light: classic" : "Light: new", 0.5f, y, 3.9f);
    panel_button(p, "shadows",
                 g_shadow_size >= 2048 ? "Shadows: high" : g_shadow_size > 0 ? "Shadows: low" : "Shadows: off",
                 4.6f, y, 3.9f);
    y += 1.3f;
    panel_button(p, "fullscreen", g_fullscreen ? "Full screen: on" : "Full screen: off", 0.5f, y, 3.9f);
    panel_button(p, "debug", debug ? "Debug info: on" : "Debug info: off", 4.6f, y, 3.9f);
    y += 1.3f;
    // Each language in its own name, so one that cannot be read can be left.
    std::string language = lang::current();
    for (const auto &[code, name] : lang::available(g_lang_dir))
        if (code == lang::current()) language = name;
    panel_button(p, "language", "Language: " + language, 0.5f, y, 3.9f);
    panel_button(p, "updates", g_update_check ? "Updates: on" : "Updates: off", 4.6f, y, 3.9f);
    return p;
}

// Where the look page shows the robot, in its cells.
constexpr float kPreviewX = 13.2f, kPreviewY = 0.2f, kPreviewW = 4.1f, kPreviewH = 6.1f;

// The models the title screen's preview draws: there is no world yet, so the
// robot and its extras straight from the mods folder (g_robot_dir). Read once.
std::string g_robot_dir;
ModelStore &title_robot()
{
    static ModelStore store;
    static bool       read = false;
    if (!read) {
        read = true;
        std::string why;
        uint16_t    id = 1;
        if (!store.put(id++, "robot:robot", g_robot_dir + "/robot.gltf", why))
            std::fprintf(stderr, "robot preview: %s\n", why.c_str());
        std::vector<std::string> names;
        for (const char *const *list : {kLiveryHeads, kLiveryNecks, kLiverySpecies})
            for (int k = 0; k < 3; ++k) names.push_back(list[k]);
        for (int k = 0; k < kLiverySpeciesCount; ++k) {
            for (int j = 0; j < kLiveryStyleCount; ++j)
                names.push_back(std::string(kLiverySpecies[k]) + "_" + kLiveryStyles[k][j]);
            for (const char *const *list : {kLiveryHeads, kLiveryNecks})
                for (int j = 0; j < 3; ++j) names.push_back(std::string(kLiverySpecies[k]) + "_" + list[j]);
        }
        for (const std::string &n : names)
            if (!store.put(id++, "robot:robot_" + n, g_robot_dir + "/robot_" + n + ".gltf", why))
                std::fprintf(stderr, "robot preview: %s\n", why.c_str());
    }
    return store;
}

// The models a panel turns in its boxes (ui::ElemKind::Model, core.ui_model):
// each its own viewport over the panel, lit, slowly turning, dressed in its
// pieces as a body is -- a vehicle's own (vehicle_part) on its bones in place
// of its parts, the rest on it as worn pieces.
void draw_panel_models(ModelStore &models, const ui::Panel &p, const ui::PanelPlacement &at)
{
    const int    sh = render_height();
    const double t  = plat_time();
    for (const ui::Element &el : p.elements) {
        if (el.kind != ui::ElemKind::Model) continue;
        std::istringstream in(el.text);
        int model = 0, tint = 0;
        in >> model >> tint;
        ModelStore::Entry *e = models.entry((uint16_t)model);
        if (e == nullptr || !e->ready || !e->model.skinned()) continue;
        std::vector<uint16_t> pieces;
        for (int id; pieces.size() < 10 && in >> id;)
            if (id > 0 && id <= 0xFFFF) pieces.push_back((uint16_t)id);

        float fx, fy, fw, fh;
        at.rect_of(el, fx, fy, fw, fh);
        const int x = (int)fx, y = (int)fy, w = (int)fw, h = (int)fh;
        if (w <= 2 || h <= 2) continue;
        gfx_viewport(x, sh - y - h, w, h);   // GL counts up from the bottom
        gfx_scissor(x, sh - y - h, w, h);
        gfx_clear(0.17f, 0.19f, 0.24f, 1.0f, true);

        // Framed by its own size, the far corner of what it occupies in front
        // of the eye, whichever way it has turned.
        const Vec3  mid  = (e->model.min + e->model.max) * 0.5f;
        const float half = length(e->model.max - e->model.min) * 0.5f;
        const float fov  = 0.62f;
        const float back = half / std::sin(fov * 0.5f) * 1.3f;   // and a margin for the pieces
        const Mat4 projection = mat4_perspective(fov, (float)w / (float)h, 0.05f, back + half * 4.0f);
        const Mat4 view = mat4_multiply(mat4_translate(Vec3{0.0f, 0.0f, -back}), mat4_rotate_x(0.45f));
        const Mat4 place = mat4_multiply(mat4_rotate_y((float)std::fmod(t * 0.5, 6.283185307)),
                                         mat4_translate(Vec3{-mid.x, -mid.y, -mid.z}));

        static std::vector<float> posed, bare;
        Body still;
        wheel_pose(e->model, still, posed);
        bare = posed;
        for (const uint16_t id : pieces) {
            const ModelStore::Entry *piece = models.entry(id);
            if (piece == nullptr) continue;
            draw_worn(models, e->model, posed, piece->name, projection, view, place, (uint16_t)tint);
            const std::string part = vehicle_part(e->name, piece->name);
            for (int j = 0; j < e->model.joint_count && !part.empty(); ++j)
                if (bone_of_part(e->model.joint_names[(size_t)j], part))
                    std::fill(bare.begin() + (ptrdiff_t)j * 12, bare.begin() + (ptrdiff_t)j * 12 + 12, 0.0f);
        }
        e->renderer.draw(projection, view, place, bare, gfx_texture{0}, 1.0f, 1.0f,
                         (uint32_t)tint << kTagIdBits);

        gfx_scissor_off();
        gfx_viewport(0, 0, render_width(), sh);
    }
}

// The robot on the look page, in this livery: idling, turning slowly, its
// tools put away. Drawn after the panel, into the panel's box for it.
void draw_look_preview(ModelStore &models, uint16_t model, const ui::PanelPlacement &at)
{
    ModelStore::Entry *robot = models.entry(species_model(models, model, net::g_client_livery));
    if (robot == nullptr || !robot->ready || !robot->model.skinned() || at.cell <= 0.0f) return;
    const int sh = render_height();
    const int x  = (int)(at.x + kPreviewX * at.cell), y = (int)(at.y + kPreviewY * at.cell);
    const int w  = (int)(kPreviewW * at.cell), h = (int)(kPreviewH * at.cell);
    gfx_viewport(x, sh - y - h, w, h);   // GL counts up from the bottom
    gfx_scissor(x, sh - y - h, w, h);
    gfx_clear(0.17f, 0.19f, 0.24f, 1.0f, true);

    const double t = plat_time();
    ClipAt       idle;
    idle.clip = robot->model.clip("idle");
    if (idle.clip >= 0 && robot->model.clips[(size_t)idle.clip].duration > 0.0f)
        idle.time = (float)std::fmod(t, (double)robot->model.clips[(size_t)idle.clip].duration);
    const std::vector<int8_t> show = tools_shown(robot->model, "");
    static std::vector<float> posed;
    pose(robot->model, &idle, idle.clip >= 0 ? 1 : 0, nullptr, show.data(), posed);

    const Mat4 projection = mat4_perspective(0.62f, (float)w / (float)h, 0.1f, 20.0f);
    const Mat4 view       = mat4_multiply(mat4_rotate_x(0.1f), mat4_translate(Vec3{0.0f, -1.05f, -4.1f}));
    const Mat4 place      = mat4_rotate_y(3.14159265f + (float)std::fmod(t * 0.6, 6.283185307));
    const uint32_t livery = net::g_client_livery;
    robot->renderer.draw(projection, view, place, posed, gfx_texture{0}, 1.0f, 1.0f, livery_tag(livery));
    draw_extras(models, *robot, posed, livery, false, projection, view, place);

    gfx_scissor_off();
    gfx_viewport(0, 0, render_width(), sh);
}

// What the player looks like: the three colours of their robot and an extra
// on the head and one at the neck (net::Hello::livery). Picked on the title
// screen, so it goes in the hello of every world joined after.
ui::Panel look_panel()
{
    const uint32_t livery = net::g_client_livery;
    ui::Panel      p;
    p.w = 17.8f;
    p.h = 7.9f;   // short enough for the title screen's name above it
    // The robot itself is drawn over this box (draw_look_preview).
    panel_add(p, ui::ElemKind::Box, "", "", kPreviewX, kPreviewY, kPreviewW);
    p.elements.back().h = kPreviewH;
    panel_label(p, "You", 0.5f, 0.0f);
    const char *const slots[3] = {"Primary", "Secondary", "Accent"};
    for (int k = 0; k < 3; ++k) {
        const float y      = 1.2f + 0.9f * (float)k;
        const int   chosen = tag_colour(livery_tag(livery), k);
        panel_label(p, slots[k], 0.5f, y - 0.2f);
        // The model's own first, then the sixteen dyes; a swatch is a button
        // with its colour over it, and the chosen one is framed in white.
        for (int d = 0; d <= 16; ++d) {
            const float x = 3.2f + 0.56f * (float)d;
            const std::string id = "c" + std::to_string(k) + "_" + std::to_string(d);
            panel_add(p, ui::ElemKind::Button, id, d == 0 ? "-" : "", x, y, 0.52f);
            p.elements.back().h = 0.52f;
            if (d == chosen) {
                panel_add(p, ui::ElemKind::Box, "", "", x + 0.04f, y + 0.04f, 0.44f);
                p.elements.back().h = 0.44f;
                p.elements.back().r = p.elements.back().g = p.elements.back().b = 255;
            }
            if (d == 0) continue;
            const float in = d == chosen ? 0.1f : 0.06f;
            panel_add(p, ui::ElemKind::Box, "", "", x + in, y + in, 0.52f - 2.0f * in);
            p.elements.back().h = 0.52f - 2.0f * in;
            p.elements.back().r = kDyeColours[d - 1][0];
            p.elements.back().g = kDyeColours[d - 1][1];
            p.elements.back().b = kDyeColours[d - 1][2];
        }
    }
    const int head = (int)(livery >> net::kLiveryHeadShift) & 7;
    const int neck = (int)(livery >> net::kLiveryNeckShift) & 7;
    const auto said = [](const char *what, const char *const *names, int n, int at) {
        std::string name = at >= 1 && at <= n ? names[at - 1] : "none";
        return std::string(what) + ": " + name;
    };
    const int species = livery_species(livery);
    const std::string who = species >= 1 ? kLiverySpecies[species - 1] : "robot";
    panel_button(p, "species", "Species: " + who, 0.5f, 4.0f, 5.9f);
    if (species >= 1)
        panel_button(p, "style", said("Style", kLiveryStyles[species - 1], kLiveryStyleCount, livery_style(livery)),
                     6.8f, 4.0f, 5.9f);
    panel_button(p, "head", said("Head", kLiveryHeads, kLiveryHeadCount, head), 0.5f, 5.3f, 5.9f);
    panel_button(p, "neck", said("Neck", kLiveryNecks, kLiveryNeckCount, neck), 6.8f, 5.3f, 5.9f);
    panel_button(p, "back", "Back", 0.5f, 6.6f, 12.2f);
    return p;
}

// A click on the look page. False for any other button.
bool look_click(const std::string &id)
{
    uint32_t &livery = net::g_client_livery;
    int k = 0, d = 0;
    if (std::sscanf(id.c_str(), "c%d_%d", &k, &d) == 2 && k >= 0 && k < 3 && d >= 0 && d <= 16) {
        livery = (livery & ~net::kLiveryColours) |
                 (tag_with_colour(livery_tag(livery), k, d) >> kTagIdBits);
    } else if (id == "head" || id == "neck" || id == "species" || id == "style") {
        const int shift = id == "head"    ? net::kLiveryHeadShift
                        : id == "neck"    ? net::kLiveryNeckShift
                        : id == "species" ? net::kLiverySpeciesShift
                                          : net::kLiveryStyleShift;
        const int count = id == "head"    ? kLiveryHeadCount
                        : id == "neck"    ? kLiveryNeckCount
                        : id == "species" ? kLiverySpeciesCount
                                          : kLiveryStyleCount;
        const uint32_t mask = id == "head" || id == "neck" ? 7u : 3u;
        const int at = (((int)(livery >> shift) & (int)mask) + 1) % (count + 1);
        livery = (livery & ~(mask << shift)) | ((uint32_t)at << shift);
        if (id == "species") livery &= ~(3u << net::kLiveryStyleShift);   // another one's styles
    } else {
        return false;
    }
    return true;
}

// A click on one of the options both pages share. False for any other button.
// F11 in any loop a person plays in, and the window made to agree with the
// option every frame -- the options button and a saved setting go through the
// same line. A window being measured is never touched.
bool g_window_measured = false;
void window_keys(bool debug)
{
    if (g_window_measured) return;
    if (plat_key_pressed(PLAT_KEY_F11)) {
        g_fullscreen = !g_fullscreen;
        save_options(debug);
    }
    plat_set_fullscreen(g_fullscreen);
}

bool options_click(const std::string &id, bool &debug)
{
    if (id == "slower") g_look_scale = std::max(0.2f, g_look_scale - 0.1f);
    else if (id == "faster") g_look_scale = std::min(3.0f, g_look_scale + 0.1f);
    else if (id == "quieter" || id == "louder") {
        g_volume = std::clamp(std::round(g_volume * 10.0f + (id == "louder" ? 1.0f : -1.0f)) / 10.0f,
                              0.0f, 1.0f);
        snd_set_volume(g_volume);
    }
    else if (id == "debug") debug = !debug;
    else if (id == "lighting") g_classic_light = !g_classic_light;
    else if (id == "fullscreen") g_fullscreen = !g_fullscreen;
    else if (id == "updates") {
        g_update_check = !g_update_check;
        if (g_update_check && !g_no_update_flag) update::start();
    }
    else if (id == "shadows") g_shadow_size = g_shadow_size >= 2048 ? 1024 : g_shadow_size > 0 ? 0 : 2048;
    else if (id == "language") {
        const auto all = lang::available(g_lang_dir);
        size_t     at  = 0;
        while (at < all.size() && all[at].first != lang::current()) ++at;
        lang::load(g_lang_dir, all[(at + 1) % all.size()].first);
    }
    else return false;
    return true;
}

// The pause menu, as a panel: the kind a mod declares, drawn and hit by the same
// code, so the engine's own menu cannot drift from what a mod's looks like.
// Built when it is needed rather than kept, because the labels carry the
// settings, and a kept copy is one that shows the old value.
ui::Panel pause_panel(const Scene &s, bool debug)
{
    if (s.pause.options) return options_panel(debug, &s.act());
    if (s.pause.look) return look_panel();

    ui::Panel p;
    p.w = 8.0f;
    p.h = 5.3f;
    panel_label(p, "Paused", 0.5f, 0.0f);
    panel_button(p, "resume", "Resume", 0.5f, 1.2f, 7.0f);
    panel_button(p, "options", "Options", 0.5f, 2.5f, 3.4f);
    panel_button(p, "look", "Character", 4.1f, 2.5f, 3.4f);
    float y = 3.8f;
    if (g_from_title) {
        panel_button(p, "title", "Quit to title", 0.5f, y, 7.0f);
        y += 1.3f;
    }
    panel_button(p, "quit", "Quit", 0.5f, y, 7.0f);
    p.h = y + 1.5f;
    if (!g_hosting_at.empty()) {
        panel_label(p, "Hosting at " + g_hosting_at, 0.5f, y + 1.3f);
        p.h += 1.1f;
    }
    return p;
}

// The pause menu's frame, for both loops. True when the player asked to quit.
//
// It opens on ESC, and when another program takes the focus while the player
// is holding the world. The second is not a nicety. Left grabbed, GLFW lets go
// of the pointer when the focus goes and grabs it again by itself when the
// focus comes back -- without asking whether XGrabPointer succeeded, which it
// does not when something else still holds the pointer at that moment (the
// other program mid-click, the window manager mid-switch). The pointer is then
// free: it wanders onto the other screen and the view snaps. And the game never
// hears of that grab, so LookSettle is not re-armed for its warp either.
// Reported from play as the view snapping on its own, long before anyone tied
// it to a popup. With the menu up the cursor is released for real, GLFW has
// nothing to take back, and the only grab is the one Resume makes: from this
// window, with nobody else holding the pointer, through the settle.
//
// `esc` is ESC this frame, unless something else (the bag) spent it. Buttons
// fire on RELEASE: a Resume that fired on the press would grab with the button
// still down, and the world would read the rest of that click as a dig.
bool pause_frame(Scene &s, LookSettle &settle, bool &debug, bool esc, bool interactive)
{
    const auto resume = [&] {
        s.pause = {};
        plat_cursor_grab(true);
        settle.grabbed();
    };

    if (!s.pause.up) {
        // A cursor something else freed -- a mod's panel -- ESC takes back, as
        // it always has.
        if (esc && !plat_cursor_grabbed()) {
            resume();
            return false;
        }
        // Not in a scripted run: nobody is there to press Resume.
        const bool lost = interactive && plat_cursor_grabbed() && !plat_window_focused();
        if (esc || lost) {
            s.pause.up = true;
            plat_cursor_grab(false);
        }
        return false;
    }

    if (esc) {
        if (s.pause.options || s.pause.look) s.pause.options = s.pause.look = false;
        else resume();
        return false;
    }

    if (!plat_mouse_released(PLAT_MOUSE_LEFT)) return false;

    float mx = 0.0f, my = 0.0f;
    plat_mouse_position(&mx, &my);
    const ui::Panel          p   = pause_panel(s, debug);
    const ui::PanelPlacement at  = ui::place_panel(p, gfx_screen_width(), gfx_screen_height());
    const int                hit = at.button_at(p, mx, my);
    if (hit < 0) return false;

    const std::string &id = p.elements[(size_t)hit].id;
    if (id == "resume") resume();
    else if (id == "options") s.pause.options = true;
    else if (id == "look") s.pause.look = true;
    else if (id == "back") s.pause.options = s.pause.look = false;
    else if (s.pause.look && look_click(id)) {
        // Seen at once here, and by everybody else as soon as the server hears.
        save_options(debug);
        if (s.conn != nullptr) s.conn->send_livery(net::g_client_livery);
    }
    else if (id == "quit") return true;
    else if (id == "title") return g_to_title = true;
    else if (options_click(id, debug)) {}
    else if (id == "third" && !iso_engaged(s.act()))
        set_view_distance(s, s.act().view_distance > 0.0f ? 0.0f : kThirdPersonDistance);
    return false;
}

// ------------------------------------------------------------ the title screen
//
// Play, Options, Quit, and after Play a world to go into: one kept from before
// or a new one. A world of the title screen's is a directory under worlds_dir()
// served by a cnp_server of its own, on this machine only, which the client
// then joins as it joins anybody's. Not the single-process world: that one
// keeps nothing, and a server's world file is what already saves, opens again
// and refuses a block table that moved. --quick skips all of it for the
// single-process world, which is what a test or a scene wants.

// This player's files that are not options: the worlds, and the identity they
// join them with.
std::string user_data_dir()
{
#ifdef _WIN32
    if (const char *app = std::getenv("APPDATA"); app != nullptr && app[0] != '\0')
        return std::string(app) + "/cubes-and-portals";
#endif
    if (const char *xdg = std::getenv("XDG_DATA_HOME"); xdg != nullptr && xdg[0] != '\0')
        return std::string(xdg) + "/cubes-and-portals";
    if (const char *home = std::getenv("HOME"); home != nullptr && home[0] != '\0')
        return std::string(home) + "/.local/share/cubes-and-portals";
    return "";
}

std::string worlds_dir() { return user_data_dir() + "/worlds"; }

// A world directory holds the server's world file, its key, its log, and
// `seed`: the seed the world was made with, which the world file insists on,
// written again at every start so its time is when the world was last played.
struct SavedWorld {
    std::string                     name;   // the directory's
    uint32_t                        seed = 0;
    std::filesystem::file_time_type played;
};

std::vector<SavedWorld> saved_worlds()
{
    namespace fs = std::filesystem;
    std::vector<SavedWorld> out;
    std::error_code         ec;
    for (const fs::directory_entry &e : fs::directory_iterator(worlds_dir(), ec)) {
        std::ifstream in(e.path() / "seed");
        SavedWorld    w;
        if (!(in >> w.seed)) continue;   // not a world
        w.name   = e.path().filename().string();
        w.played = fs::last_write_time(e.path() / "seed", ec);
        out.push_back(std::move(w));
    }
    std::sort(out.begin(), out.end(),
              [](const SavedWorld &a, const SavedWorld &b) { return a.played > b.played; });
    return out;
}

// A typed name as a directory's: letters, digits, spaces, - and _, and the
// bytes of an accented letter; anything else, a slash or a dot among them, is
// an underscore. Then made unlike every world already there.
std::string new_world_dir(const std::string &typed)
{
    std::string name;
    for (const char c : typed)
        name += (std::isalnum((unsigned char)c) || c == ' ' || c == '-' || c == '_' ||
                 (unsigned char)c >= 0x80)
                    ? c
                    : '_';
    name.erase(0, name.find_first_not_of(' '));
    name.erase(name.find_last_not_of(' ') + 1);
    if (name.empty()) name = "World";

    std::error_code ec;
    std::string     dir = worlds_dir() + "/" + name;
    for (int n = 2; std::filesystem::exists(dir, ec); ++n)
        dir = worlds_dir() + "/" + name + " " + std::to_string(n);
    return dir;
}

// Digits are the seed; any other text is hashed into one (FNV-1a), and no
// text is a random one.
uint32_t seed_from_text(const std::string &text)
{
    if (text.empty()) return std::random_device{}();
    if (text.size() <= 9 && text.find_first_not_of("0123456789") == std::string::npos)
        return (uint32_t)std::strtoul(text.c_str(), nullptr, 10);
    uint32_t h = 2166136261u;
    for (const char c : text) h = (h ^ (uint8_t)c) * 16777619u;
    return h;
}

// Whether a server holds this world now -- the one this client started, or one
// another client or a terminal did. Asked by taking the lock a server holds
// for its life (lock_world_file in the server) and letting go at once.
bool world_in_use(const std::string &dir)
{
    return lock_file_held(dir + "/world.db.lock");
}

// The server a world of the title screen's runs on. Stopped with the client:
// SIGTERM makes a server save and close, as tools/play.sh relies on, and the
// kernel sends it by itself if the client dies first.
struct LocalServer : ChildProcess {
    ~LocalServer() { stop(); }
};

// cnp_server beside this program (a release), or where the build tree puts it.
std::string server_program()
{
    namespace fs = std::filesystem;
    const std::string here = exe_dir();
    if (here.empty()) return "";
    std::error_code ec;
    for (const fs::path &p : {fs::path(here) / "cnp_server",
                             fs::path(here).parent_path() / "server" / "cnp_server"})
        if (fs::exists(p, ec)) return p.string();
    return "";
}

std::string trimmed(const std::string &text)
{
    const size_t first = text.find_first_not_of(" \t");
    if (first == std::string::npos) return "";
    return text.substr(first, text.find_last_not_of(" \t") - first + 1);
}

// The name this player last used. Only the name: a password kept anywhere is a
// password somebody else can read.
std::string player_name_path() { return user_data_dir() + "/player"; }

std::string saved_player_name()
{
    std::string line;
    std::getline(std::ifstream(player_name_path()), line);
    return line;
}

// The servers this player has added, one address a line, in the order they
// were added.
std::string servers_path() { return user_data_dir() + "/servers"; }

// One of them, and what the last knock on its door found.
struct ServerRow {
    enum class State { Unchecked, Trying, Up, Down };

    std::string address;
    std::string player;    // the name this client last played under there
    std::string key;       // the server's key the first join saw, pinned from then on
    State       state   = State::Unchecked;
    double      ping_ms = 0.0;
    // What the server said of itself (net::Status); none from one too old to.
    bool        has_status = false;
    net::Status status;
};

// Each line is an address, and after a tab the name this client played under
// there, which a server remembers the identity of: joining as that name again
// is what makes it the same player, so the list asks for THAT name's password
// rather than whatever was typed at the start.
std::vector<ServerRow> saved_servers()
{
    std::vector<ServerRow> out;
    std::ifstream          in(servers_path());
    std::string            line;
    while (std::getline(in, line)) {
        if (line.find_first_not_of(" \t") == std::string::npos) continue;

        ServerRow         r;
        std::stringstream fields(line);
        std::getline(fields, r.address, '\t');
        std::getline(fields, r.player, '\t');
        std::getline(fields, r.key, '\t');
        out.push_back(std::move(r));
    }
    return out;
}

void save_servers(const std::vector<ServerRow> &rows)
{
    std::ofstream out(servers_path());
    for (const ServerRow &r : rows)
        out << r.address << "\t" << r.player << "\t" << r.key << "\n";
}

constexpr const char *kLocalAccount = "local", *kHostAccount = "host";

enum class MenuPage { Title, Player, Options, Look, Worlds, NewWorld, EditWorld, Server, Starting };

// What the title screen was left for: a world of this machine's, somebody
// else's server, or nothing.
enum class MenuChoice { Quit, World, Server };

struct TitleMenu {
    MenuPage                page = MenuPage::Title;
    // The server to join again once it answers: one that put this player out
    // because it was restarting (net::RefuseReason::Restarting).
    std::string             rejoin;
    double                  rejoin_from = 0.0, rejoin_until = 0.0;
    std::vector<SavedWorld> worlds;
    size_t                  first = 0;   // the first world listed
    std::string             name  = "New World";
    std::string             seed;
    std::string             address;     // what is typed into the list of servers
    std::vector<ServerRow>  servers;
    bool                    hosting   = false;   // the world list is picking a world to HOST
    std::string             port_text = "25301";
    size_t                  editing = 0;   // the world EditWorld is about
    std::string             rename;
    bool                    confirm_delete = false;

    // Who is playing. The name is remembered between runs, the password never
    // is: together they ARE the identity (sign_key_from_password), so typing
    // them again is what proves the player is who the name says. An account is
    // a server's: one is asked for once that server is chosen, and is good for
    // it alone -- the name and password of another server are another account.
    std::string             player;
    std::string             password;
    bool                    has_identity = false;
    // What the identity is for: kLocalAccount for player.key, the worlds of
    // one's own; kHostAccount for a world hosted here; else a server's address.
    std::string             identity_for;
    net::SignKeyPair        identity;
    // The identity being made from them, off the frame thread: Argon2id takes
    // ~130 ms, and on it the menu stood still for that long.
    std::future<std::pair<bool, net::SignKeyPair>> deriving;
    std::string             wanted;      // the button that asked for the identity first
    size_t                  chose_row = SIZE_MAX;   // the server row being joined
    std::string             starting;    // the world being started
    float                   progress = 0.0f;   // of starting it, 0..1, read off its log
    std::string             doing;             // what its server is at, from the same
    std::string             error;       // why the last one did not

    // What was chosen, once title_menu returns.
    std::string             dir;
    uint32_t                world_seed = 0;
    std::string             host;
    uint16_t                port = 0;
};

// host, or host:port, as the two the client joins with. Nothing said about the
// port is the one a server listens on by default.
void split_address(const std::string &text, std::string &host, uint16_t &port)
{
    const size_t colon = text.rfind(':');
    host = colon == std::string::npos ? text : text.substr(0, colon);
    port = colon == std::string::npos
               ? 25301
               : (uint16_t)std::strtoul(text.c_str() + colon + 1, nullptr, 10);
    if (port == 0) port = 25301;
}

// Worlds, or servers, listed at a time; the rest are a page turn away.
constexpr size_t kWorldsShown = 5;

// How long a server has to answer before the list calls it down.
constexpr double kProbeSeconds = 3.0;

// Knocking on a server's door, one at a time. ENet connects and the keys are
// exchanged before a Connected event arrives, so an answer means a server that
// is up and speaks this engine's transport -- and ENet has measured a round
// trip by then, which is the ping the list shows. Nothing joins: no identity is
// sent and the knock is hung up on at once, which the server sees as a peer
// that connected and left.
//
// A name is looked up on a thread of its own first: getaddrinfo blocks for as
// long as the resolver takes, and on the frame thread that held the menu still.
struct HostLookup {
    std::atomic<bool> done{false};
    std::string       ip;   // empty when the name resolved to nothing
};

std::shared_ptr<HostLookup> start_lookup(const std::string &host)
{
    auto lookup = std::make_shared<HostLookup>();
    // Detached, holding its own reference: a page left mid-lookup must not
    // wait for the resolver to give up.
    std::thread([lookup, host] {
        lookup->ip = resolve_ipv4(host);
        lookup->done = true;
    }).detach();
    return lookup;
}

struct ServerProbe {
    net::Transport              tr;
    size_t                      row     = SIZE_MAX;   // the row being knocked on
    double                      started = 0.0;
    std::shared_ptr<HostLookup> lookup;               // its name, while it resolves
    uint16_t                    port    = 0;
    double                      asked_at = 0.0;       // when the status was asked for

    void step(std::vector<ServerRow> &rows)
    {
        const double now = plat_time();

        if (row < rows.size() && lookup) {
            if (!lookup->done) {
                if (now - started < kProbeSeconds) return;
                rows[row].state = ServerRow::State::Down;
            } else {
                std::string err;
                if (!lookup->ip.empty() && tr.connect(lookup->ip, port, err)) {
                    lookup.reset();
                    started = now;   // the ping is the link's, not the resolver's
                    return;
                }
                rows[row].state = ServerRow::State::Down;
                tr.close();
            }
            lookup.reset();
            row = SIZE_MAX;
            return;
        }

        if (row < rows.size()) {
            // Waited on here rather than once a frame: the menu draws every
            // 10 ms, and an answer acknowledged that late reads as 10 ms of
            // round trip that is this loop's and not the link's.
            // Connected, then asked who is there (net::StatusQuery): the
            // answer, a refusal from a server too old to know the question, or
            // a second's silence all end it.
            net::NetEvent ev;
            bool          answered = false;
            for (int i = 0; i < 5 && !answered; ++i)
                while (tr.poll(ev, 1)) {
                    if (ev.type == net::NetEvent::Type::Connected && asked_at == 0.0) {
                        asked_at          = now;
                        const double rtt  = tr.round_trip(tr.server_peer());
                        rows[row].ping_ms = (rtt > 0.0 ? rtt : now - started) * 1e3;
                        tr.send(tr.server_peer(), net::Channel::Handshake, net::StatusQuery{}.encode(), true);
                    } else if (ev.type == net::NetEvent::Type::Received) {
                        ByteReader r(ev.data);
                        if (net::peek_type(ev.data.data(), ev.data.size()) == net::MsgType::Status)
                            rows[row].has_status = net::Status::decode(r, rows[row].status);
                        answered = true;
                    } else if (ev.type == net::NetEvent::Type::Disconnected && asked_at > 0.0) {
                        answered = true;
                    }
                }

            if (asked_at > 0.0 && (answered || now - asked_at > 1.0)) {
                rows[row].state = ServerRow::State::Up;
                asked_at        = 0.0;
            } else if (asked_at > 0.0 || now - started < kProbeSeconds) {
                return;
            } else {
                rows[row].state = ServerRow::State::Down;
            }
            tr.close();
            row = SIZE_MAX;
        }

        for (size_t i = 0; i < rows.size(); ++i) {
            if (rows[i].state != ServerRow::State::Unchecked) continue;

            std::string host;
            split_address(rows[i].address, host, port);
            rows[i].state = ServerRow::State::Trying;
            lookup        = start_lookup(host);
            row           = i;
            started       = now;
            return;
        }
    }

    void reset()
    {
        tr.close();
        lookup.reset();
        row      = SIZE_MAX;
        asked_at = 0.0;
    }
};

ui::Panel title_panel(const TitleMenu &m, bool debug)
{
    ui::Panel p;
    switch (m.page) {
    case MenuPage::Title:
        p.w = 8.0f;
        p.h = 7.5f;
        // Play is the big one, with green round it.
        panel_add(p, ui::ElemKind::Box, "", "", 0.35f, 0.15f, 7.3f);
        p.elements.back().h = 1.9f;
        p.elements.back().r = 80;
        p.elements.back().g = 170;
        p.elements.back().b = 70;
        panel_button(p, "play", "Play", 0.5f, 0.3f, 7.0f);
        p.elements.back().h = 1.6f;
        panel_button(p, "multiplayer", "Multiplayer", 0.5f, 2.3f, 7.0f);
        panel_button(p, "host", "Host a server", 0.5f, 3.6f, 7.0f);
        panel_button(p, "options", "Options", 0.5f, 4.9f, 3.4f);
        panel_button(p, "look", "Character", 4.1f, 4.9f, 3.4f);
        panel_button(p, "quit", "Quit", 0.5f, 6.2f, 7.0f);
        // A newer release on GitHub, once the question has an answer.
        if (const std::string tag = update::newer(); g_update_check && !tag.empty()) {
            panel_button(p, "update", "New version " + tag + ": download", 0.5f, 7.5f, 7.0f);
            p.h += 1.3f;
        }
        break;

    case MenuPage::Player:
        p.w = 10.0f;
        p.h = m.error.empty() ? 7.2f : 8.2f;
        panel_label(p, "Name", 0.5f, 0.0f);
        panel_add(p, ui::ElemKind::Field, "player", m.player, 0.5f, 1.0f, 9.0f);
        panel_label(p, "Password", 0.5f, 2.3f);
        panel_add(p, ui::ElemKind::Field, "password", std::string(m.password.size(), '*'), 0.5f,
                  3.3f, 9.0f);
        panel_button(p, "ready", "Continue", 0.5f, 4.6f, 9.0f);
        panel_button(p, "back", "Back", 0.5f, 5.9f, 9.0f);
        if (!m.error.empty()) panel_label(p, m.error.substr(0, 50), 0.5f, 7.2f);
        break;

    case MenuPage::Options:
        p = options_panel(debug, nullptr);
        break;

    case MenuPage::Look:
        p = look_panel();
        break;

    case MenuPage::Worlds: {
        p.w = 12.0f;
        // Making a world is the big green one on top, apart from the list of
        // the ones already made, so the two are never mistaken for each other.
        panel_add(p, ui::ElemKind::Box, "", "", 0.35f, 0.15f, 11.3f);
        p.elements.back().h = 1.6f;
        p.elements.back().r = 80;
        p.elements.back().g = 170;
        p.elements.back().b = 70;
        panel_button(p, "new", "+ Create a new world", 0.5f, 0.3f, 11.0f);
        p.elements.back().h = 1.3f;
        float y = 2.2f;
        panel_label(p, m.worlds.empty() ? "No saved world yet."
                       : m.hosting    ? "Or host a saved world:"
                                      : "Or play a saved world:",
                    0.5f, y);
        y += 1.2f;
        for (size_t i = m.first; i < m.worlds.size() && i < m.first + kWorldsShown; ++i) {
            const std::string id = "world:" + std::to_string(i);
            panel_add(p, ui::ElemKind::Button, id, m.worlds[i].name, 0.5f, y, 8.8f);
            panel_add(p, ui::ElemKind::Button, "edit:" + std::to_string(i), "Edit", 9.5f, y, 2.0f);
            y += 1.2f;
        }
        y += 0.2f;
        if (m.hosting) {
            panel_label(p, "Port", 0.5f, y);
            panel_add(p, ui::ElemKind::Field, "port", m.port_text, 2.5f, y, 3.0f);
            y += 1.3f;
        }
        if (m.first > 0) panel_button(p, "prev", "<", 0.5f, y, 1.2f);
        panel_button(p, "back", "Back", 2.0f, y, 8.0f);
        if (m.first + kWorldsShown < m.worlds.size()) panel_button(p, "next", ">", 10.3f, y, 1.2f);
        y += 1.3f;
        if (!m.error.empty()) {
            panel_label(p, m.error.substr(0, 60), 0.5f, y);
            y += 1.0f;
        }
        p.h = y;
        break;
    }

    case MenuPage::NewWorld:
        p.w = 10.0f;
        p.h = 8.3f;
        panel_label(p, "Create a new world", 0.5f, 0.0f);
        panel_label(p, "Name", 0.5f, 1.2f);
        panel_add(p, ui::ElemKind::Field, "name", m.name, 0.5f, 2.2f, 9.0f);
        panel_label(p, "Seed (none for a random one)", 0.5f, 3.5f);
        panel_add(p, ui::ElemKind::Field, "seed", m.seed, 0.5f, 4.5f, 9.0f);
        panel_button(p, "create", "Create", 0.5f, 5.8f, 9.0f);
        panel_button(p, "back", "Back", 0.5f, 7.1f, 9.0f);
        break;

    case MenuPage::EditWorld:
        p.w = 10.0f;
        p.h = m.error.empty() ? 7.2f : 8.2f;
        panel_label(p, "Name", 0.5f, 0.0f);
        panel_add(p, ui::ElemKind::Field, "rename", m.rename, 0.5f, 1.0f, 9.0f);
        panel_button(p, "do_rename", "Rename", 0.5f, 2.3f, 9.0f);
        panel_button(p, "delete",
                     m.confirm_delete ? "Delete it for good? Click again" : "Delete", 0.5f,
                     3.6f, 9.0f);
        panel_button(p, "back", "Back", 0.5f, 4.9f, 9.0f);
        if (!m.error.empty()) panel_label(p, m.error.substr(0, 50), 0.5f, 6.2f);
        break;

    case MenuPage::Server: {
        p.w = 12.0f;
        panel_label(p, "Servers", 0.5f, 0.0f);
        float y = 1.2f;
        if (m.servers.empty()) {
            panel_label(p, "No server yet: type an address below.", 0.5f, y);
            y += 1.2f;
        }
        for (size_t i = m.first; i < m.servers.size() && i < m.first + kWorldsShown; ++i) {
            const std::string n = std::to_string(i);
            const ServerRow  &row  = m.servers[i];
            std::string what = row.player.empty() ? row.address : row.address + "   as " + row.player;
            if (row.state == ServerRow::State::Up && row.has_status)
                what += row.status.maintenance.empty()
                            ? "   " + std::to_string(row.status.players) + " playing"
                            : std::string("   maintenance");
            panel_add(p, ui::ElemKind::Button, "server:" + n, what, 0.5f, y, 9.8f);
            panel_add(p, ui::ElemKind::Button, "drop:" + n, "x", 10.5f, y, 1.0f);
            y += 1.2f;
        }
        y += 0.2f;
        panel_add(p, ui::ElemKind::Field, "address", m.address, 0.5f, y, 8.3f);
        panel_button(p, "add", "Add", 9.0f, y, 2.5f);
        y += 1.3f;
        if (m.first > 0) panel_button(p, "prev", "<", 0.5f, y, 1.2f);
        panel_button(p, "back", "Back", 2.0f, y, 8.0f);
        if (m.first + kWorldsShown < m.servers.size()) panel_button(p, "next", ">", 10.3f, y, 1.2f);
        y += 1.3f;
        if (!m.error.empty()) {
            panel_label(p, m.error.substr(0, 60), 0.5f, y);
            y += 1.0f;
        }
        p.h = y;
        break;
    }

    case MenuPage::Starting:
        p.w = 16.0f;
        p.h = g_hosting_at.empty() ? 3.8f : 5.0f;
        panel_label(p, "Starting " + m.starting + " ...", 0.5f, 0.2f);
        // The bar: a dark track and the part done over it.
        panel_add(p, ui::ElemKind::Box, "", "", 0.5f, 1.4f, 15.0f);
        p.elements.back().r = 30, p.elements.back().g = 32, p.elements.back().b = 38;
        if (m.progress > 0.0f) {
            panel_add(p, ui::ElemKind::Box, "", "", 0.5f, 1.4f, 15.0f * std::min(m.progress, 1.0f));
            p.elements.back().r = 70, p.elements.back().g = 170, p.elements.back().b = 90;
        }
        panel_label(p, std::to_string((int)(m.progress * 100.0f)) + "%  " + m.doing, 0.5f, 2.6f);
        if (!g_hosting_at.empty()) panel_label(p, "Others join at " + g_hosting_at, 0.5f, 3.8f);
        break;
    }
    p.name = "title";
    return p;
}

// Behind the title screen: a sky, a row of blocks and a pair of openings on
// them, out of rectangles, so it needs nothing loaded.
void draw_title_backdrop()
{
    const float w = (float)gfx_screen_width(), h = (float)gfx_screen_height();
    const float ground = h * 0.72f, block = w / 24.0f;

    for (int i = 0; i < 24; ++i) {
        const float t      = (float)i / 23.0f;
        const float sky[4] = {0.18f + 0.32f * t, 0.30f + 0.38f * t, 0.55f + 0.33f * t, 1.0f};
        gfx_2d_rect(0.0f, ground * (float)i / 24.0f, w, ground / 24.0f + 1.0f, sky);
    }
    const float dirt[4] = {0.45f, 0.31f, 0.20f, 1.0f}, grass[4] = {0.33f, 0.62f, 0.25f, 1.0f},
                seam[4] = {0.0f, 0.0f, 0.0f, 0.25f};
    gfx_2d_rect(0.0f, ground, w, h - ground, dirt);
    gfx_2d_rect(0.0f, ground, w, block * 0.3f, grass);
    for (float y = ground; y < h; y += block) gfx_2d_rect(0.0f, y, w, 2.0f, seam);
    for (float x = 0.0f; x < w; x += block) gfx_2d_rect(x, ground, 2.0f, h - ground, seam);

    const float frame[4] = {0.22f, 0.22f, 0.26f, 1.0f};
    const float blue[4] = {0.20f, 0.55f, 1.0f, 1.0f}, orange[4] = {1.0f, 0.55f, 0.12f, 1.0f};
    for (const float cx : {w * 0.14f, w * 0.86f}) {
        const float bw = block * 1.2f, bh = block * 2.4f, e = block * 0.25f;
        gfx_2d_rect(cx - bw * 0.5f - e, ground - bh - e, bw + 2.0f * e, bh + e, frame);
        gfx_2d_rect(cx - bw * 0.5f, ground - bh, bw, bh, cx < w * 0.5f ? blue : orange);
    }
}

// The name of the game over it all, after the panel so its sheet does not dim it.
void draw_title_name()
{
    const float w = (float)gfx_screen_width(), h = (float)gfx_screen_height();
    const char *name = "Cubes and Portals";
    const float size = h * 0.09f, tw = gfx_2d_text_width(name, size);
    const float shadow[4] = {0.0f, 0.0f, 0.0f, 0.6f}, white[4] = {1.0f, 1.0f, 1.0f, 1.0f};
    gfx_2d_text(name, (w - tw) * 0.5f + size * 0.06f, h * 0.08f + size * 0.06f, size, shadow);
    gfx_2d_text(name, (w - tw) * 0.5f, h * 0.08f, size, white);

    // The version in a corner, for whoever reports a bug from an alpha.
    const std::string version = std::string("v") + kEngineVersion;
    const float       small = h * 0.022f, grey[4] = {1.0f, 1.0f, 1.0f, 0.55f};
    gfx_2d_text(version.c_str(), w - gfx_2d_text_width(version.c_str(), small) - small,
                h - small * 1.8f, small, grey);
}

// The dot and the ping beside every server in the list: grey before it has been
// knocked on, amber while it is being, green with its round trip, red when
// nothing answered. Drawn over the panel rather than in it, because a panel
// holds text and rectangles and this is neither.
void draw_server_marks(const TitleMenu &m, const ui::Panel &p, const ui::PanelPlacement &at)
{
    const float grey[4]  = {0.55f, 0.55f, 0.58f, 1.0f}, amber[4] = {0.95f, 0.75f, 0.20f, 1.0f};
    const float green[4] = {0.30f, 0.85f, 0.35f, 1.0f}, red[4]   = {0.90f, 0.30f, 0.25f, 1.0f};
    const float white[4] = {1.0f, 1.0f, 1.0f, 1.0f};

    for (const ui::Element &e : p.elements) {
        if (e.id.rfind("server:", 0) != 0) continue;
        const ServerRow &r = m.servers[std::strtoul(e.id.c_str() + 7, nullptr, 10)];

        float ex, ey, ew, eh;
        at.rect_of(e, ex, ey, ew, eh);

        const float *dot = r.state == ServerRow::State::Up      ? green
                           : r.state == ServerRow::State::Down  ? red
                           : r.state == ServerRow::State::Trying ? amber
                                                                 : grey;
        gfx_2d_circle(ex + eh * 0.5f, ey + eh * 0.5f, eh * 0.18f, dot);

        if (r.state != ServerRow::State::Up) continue;
        char ping[32];
        std::snprintf(ping, sizeof(ping), "%.0f ms", r.ping_ms);
        const float size = at.cell * 0.30f;
        gfx_2d_text(ping, ex + ew - gfx_2d_text_width(ping, size) - eh * 0.4f,
                    ey + (eh - size) * 0.5f, size, white);
    }
}

void draw_title(Scene &s, const TitleMenu &m, const ui::Panel &p)
{
    gfx_begin_frame();
    gfx_clear(0.0f, 0.0f, 0.0f, 1.0f, true);
    gfx_2d_begin(gfx_screen_width(), gfx_screen_height());
    draw_title_backdrop();

    const ui::PanelPlacement at = ui::place_panel(p, gfx_screen_width(), gfx_screen_height());
    float mx = 0.0f, my = 0.0f;
    plat_mouse_position(&mx, &my);
    int typing = -1;
    for (size_t i = 0; i < p.elements.size(); ++i)
        if (s.typing.on && p.elements[i].kind == ui::ElemKind::Field && p.elements[i].id == s.typing.id)
            typing = (int)i;
    const std::string shown = s.typing.id == "password" ? std::string(s.typing.text.size(), '*')
                                                        : s.typing.text;
    ui::draw_mod_panel(p, at, at.button_at(p, mx, my), gfx_screen_width(), gfx_screen_height(),
                       typing, shown.c_str());
    draw_server_marks(m, p, at);

    draw_title_name();
    gfx_2d_end();
    if (m.page == MenuPage::Look) {
        ModelStore &robot = title_robot();
        draw_look_preview(robot, 1, at);
    }
    gfx_end_frame();

    // The window runs without vsync, for the gates; a menu needs no more than this.
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
}

// The title screen, until something is chosen: a world of this machine's (its
// directory and seed in `m`), a server to join (its host and port), or quitting.
MenuChoice title_menu(Scene &s, TitleMenu &m, bool &debug)
{
    plat_cursor_grab(false);
    ServerProbe probe;

    // A click is a press and a release the MENU saw, not a release on its own.
    // While a join is settling nothing polls the window, and the release that
    // started it is still there when the menu comes back -- which joined the
    // same server again, on its own, for as long as it kept failing.
    bool pressed_here = false;

    while (!plat_should_close()) {
        window_keys(debug);
        if (m.page == MenuPage::Server) probe.step(m.servers);
        else probe.reset();

        const ui::Panel p = title_panel(m, debug);

        // A field's text is the menu's the moment it is typed, so a click on
        // Create without Enter first still creates what was typed.
        const auto field = [&m](const std::string &id) -> std::string & {
            return id == "name"       ? m.name
                   : id == "seed"     ? m.seed
                   : id == "port"     ? m.port_text
                   : id == "player"   ? m.player
                   : id == "password" ? m.password
                   : id == "rename"   ? m.rename
                                      : m.address;
        };
        std::string tid, ttext;
        if (type_in_field(s, &p, tid, ttext)) field(tid) = ttext;
        if (s.typing.on) field(s.typing.id) = s.typing.text;

        if (plat_key_pressed(PLAT_KEY_ESC)) {
            if (s.typing.on) stop_typing(s);
            else if (m.page == MenuPage::NewWorld || m.page == MenuPage::EditWorld)
                m.page = MenuPage::Worlds;
            else if (m.page == MenuPage::Player && m.wanted.rfind("server:", 0) == 0)
                m.page = MenuPage::Server;   // a server's account, so back to the servers
            else if (m.page == MenuPage::Options) {
                save_options(debug);
                m.page = MenuPage::Title;
            } else m.page = MenuPage::Title;
        }

        std::string id;
        if (plat_mouse_pressed(PLAT_MOUSE_LEFT)) pressed_here = true;
        if (pressed_here && plat_mouse_released(PLAT_MOUSE_LEFT)) {
            pressed_here = false;
            float mx = 0.0f, my = 0.0f;
            plat_mouse_position(&mx, &my);
            id = click_panel(s, p, mx, my);
        }

        // Back from a server that restarted: its row is knocked on until it
        // answers -- not in the first seconds, when the old one may still be
        // going down -- and then joined as if clicked.
        if (!m.rejoin.empty() && id.empty()) {
            const double now = plat_time();
            if (m.rejoin_until == 0.0) {
                m.rejoin_from  = now + 5.0;
                m.rejoin_until = now + 180.0;
                m.servers      = saved_servers();   // what Multiplayer loads
            }
            size_t i = 0;
            while (i < m.servers.size() && m.servers[i].address != m.rejoin) ++i;
            m.page = MenuPage::Server;
            if (i == m.servers.size() || now > m.rejoin_until) {
                m.error = "the server did not come back";
                m.rejoin.clear();
            } else if (m.servers[i].state == ServerRow::State::Up && now > m.rejoin_from) {
                id = "server:" + std::to_string(i);
                m.rejoin.clear();
            } else {
                m.error = "the server is restarting: joining again when it answers";
                if ((m.servers[i].state == ServerRow::State::Down ||
                     m.servers[i].state == ServerRow::State::Up) && now > m.rejoin_from)
                    m.servers[i].state = ServerRow::State::Unchecked;
            }
        }

        // Nothing this menu opens is anonymous, and the servers this client
        // starts are told to require an identity. A world of one's own is
        // joined with player.key, made once and kept: nobody else can reach
        // that loopback server, so there is nothing a password would prove.
        // A world hosted here and every server in the list are joined with
        // one made from a name and a password, the same on any machine.
        if (id == "play" && !(m.has_identity && m.identity_for == kLocalAccount)) {
            bool        created = false;
            std::string key_err;
            if (net::load_or_create_sign_key(user_data_dir() + "/player.key", m.identity, created,
                                             key_err)) {
                m.has_identity = true;
                m.identity_for = kLocalAccount;
                if (trimmed(m.player).empty()) m.player = "Player";
            } else {
                m.has_identity = false;
                m.error        = key_err.substr(0, 50);
                id.clear();
            }
        }
        // A server in the list, or the one hosted here, is joined with the
        // account of THAT server: chosen first, then asked who one is there.
        std::string account = id == "host" ? kHostAccount : "";
        if (id.rfind("server:", 0) == 0)
            account = m.servers[std::strtoul(id.c_str() + 7, nullptr, 10)].address;
        if (!account.empty() && !(m.has_identity && m.identity_for == account)) {
            if (account != kHostAccount) {
                // The name this client played under there, which that server
                // knows the identity of; a new server starts from the last name.
                const ServerRow &row = m.servers[std::strtoul(id.c_str() + 7, nullptr, 10)];
                m.player = row.player.empty() ? saved_player_name() : row.player;
            }
            m.password.clear();
            m.has_identity = false;
            m.identity_for = account;
            m.wanted       = id;
            m.error        = account == kHostAccount ? "your account on the server you host"
                                                     : "your account on " + account;
            m.page         = MenuPage::Player;
            id.clear();
        }
        if (id == "ready" && !m.deriving.valid()) {
            const std::string name = trimmed(m.player);
            if (name.empty()) {
                m.error = "type a name";
            } else if (m.password.size() < 6) {
                m.error = "a password of at least 6 characters";
            } else {
                m.player   = name;
                m.error    = "making your identity ...";
                m.deriving = std::async(std::launch::async, [name, pw = m.password] {
                    net::SignKeyPair key;
                    const bool       ok = net::sign_key_from_password(name, pw, key);
                    return std::make_pair(ok, key);
                });
            }
        }
        if (m.deriving.valid() &&
            m.deriving.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
            const auto made = m.deriving.get();
            if (!made.first) {
                m.error = "not enough memory to make an identity";
            } else {
                m.identity     = made.second;
                m.has_identity = true;
                m.error.clear();
                std::ofstream(player_name_path()) << m.player << "\n";
                std::printf("playing as '%s', identity %s (from the name and the password)\n",
                            m.player.c_str(),
                            net::key_fingerprint(m.identity.public_key).c_str());
                id = m.wanted;   // straight on to what was asked for before this
                m.wanted.clear();
            }
        }

        if (id == "play" || id == "host") {
            m.worlds  = saved_worlds();
            m.first   = 0;
            m.hosting = id == "host";
            m.error.clear();
            m.page = MenuPage::Worlds;
        } else if (id == "multiplayer") {
            m.servers = saved_servers();
            m.first   = 0;
            m.error.clear();
            m.page = MenuPage::Server;
        } else if (id == "options") {
            m.page = MenuPage::Options;
        } else if (id == "quit") {
            return MenuChoice::Quit;
        } else if (id == "update") {
            gfx_open_url(kReleasesPage);
        } else if (id == "look") {
            m.page = MenuPage::Look;
        } else if (m.page == MenuPage::Look && look_click(id)) {
            save_options(debug);
        } else if (m.page == MenuPage::Options && options_click(id, debug)) {
        } else if (id == "back") {
            if (m.page == MenuPage::Options) save_options(debug);
            m.page = m.page == MenuPage::NewWorld || m.page == MenuPage::EditWorld
                         ? MenuPage::Worlds
                     : m.page == MenuPage::Player && m.wanted.rfind("server:", 0) == 0
                         ? MenuPage::Server
                         : MenuPage::Title;
            m.wanted.clear();
            m.error.clear();
        } else if (id == "prev") {
            m.first -= kWorldsShown;
        } else if (id == "next") {
            m.first += kWorldsShown;
        } else if (id == "new") {
            m.page = MenuPage::NewWorld;
        } else if (id.rfind("world:", 0) == 0) {
            const SavedWorld &w = m.worlds[std::strtoul(id.c_str() + 6, nullptr, 10)];
            m.dir        = worlds_dir() + "/" + w.name;
            m.world_seed = w.seed;
            m.chose_row  = SIZE_MAX;
            return MenuChoice::World;
        } else if (id.rfind("edit:", 0) == 0) {
            m.editing        = std::strtoul(id.c_str() + 5, nullptr, 10);
            m.rename         = m.worlds[m.editing].name;
            m.confirm_delete = false;
            m.error.clear();
            m.page = MenuPage::EditWorld;
        } else if (id == "delete" && !m.confirm_delete) {
            m.confirm_delete = true;   // a world is hours of somebody's play
        } else if (id == "do_rename" || id == "delete") {
            const std::string dir = worlds_dir() + "/" + m.worlds[m.editing].name;
            std::error_code   ec;
            if (world_in_use(dir)) {
                m.error          = "a server is running on it: stop that first";
                m.confirm_delete = false;
            } else {
                if (id == "delete") std::filesystem::remove_all(dir, ec);
                else if (trimmed(m.rename) != m.worlds[m.editing].name)
                    std::filesystem::rename(dir, new_world_dir(m.rename), ec);

                if (ec) {
                    m.error = ec.message();
                } else {
                    m.worlds = saved_worlds();
                    m.first  = 0;
                    m.error.clear();
                    m.page = MenuPage::Worlds;
                }
            }
        } else if (id == "create") {
            m.dir        = new_world_dir(m.name);
            m.world_seed = seed_from_text(m.seed);
            m.chose_row  = SIZE_MAX;
            return MenuChoice::World;
        } else if (id == "add") {
            if (m.address.find_first_not_of(' ') == std::string::npos) {
                m.error = "type an address, like 192.168.1.2 or 192.168.1.2:25301";
            } else {
                m.servers.push_back(ServerRow{m.address});
                save_servers(m.servers);
                m.address.clear();
                m.error.clear();
            }
        } else if (id.rfind("drop:", 0) == 0) {
            m.servers.erase(m.servers.begin() + (long)std::strtoul(id.c_str() + 5, nullptr, 10));
            save_servers(m.servers);
            probe.reset();
            for (ServerRow &r : m.servers)
                if (r.state == ServerRow::State::Trying) r.state = ServerRow::State::Unchecked;
            if (m.first >= m.servers.size() && m.first >= kWorldsShown) m.first -= kWorldsShown;
        } else if (id.rfind("server:", 0) == 0) {
            const size_t row = std::strtoul(id.c_str() + 7, nullptr, 10);
            m.chose_row      = row;
            split_address(m.servers[row].address, m.host, m.port);
            return MenuChoice::Server;
        }

        draw_title(s, m, p);
    }
    return MenuChoice::Quit;
}

// Starts `dir`'s server on 127.0.0.1 and waits, the title screen up, until it
// listens. False with `err` when it stopped first -- the reason is the last
// thing its log says, and for a world a mod has since added blocks to, the line
// that says so.
bool serve_world(Scene &s, TitleMenu &m, LocalServer &srv, const std::string &dir,
                 uint32_t seed, uint16_t &port, std::string &key_hex, std::string &err)
{
    srv.stop();       // whatever was started before this choice
    key_hex.clear();

    std::error_code ec;
    std::filesystem::create_directories(dir, ec);
    std::ofstream(dir + "/seed") << seed << "\n";

    const std::string prog = server_program();
    if (prog.empty()) {
        err = "no cnp_server beside this program";
        return false;
    }

    // Hosting: the port the player typed, on every address this machine has,
    // so somebody else can join. Otherwise a world of their own, on the
    // loopback where nobody else can reach it.
    //
    // ponytail: a random port above the registered ones, and a clash is a
    // world that says it could not bind; the next try picks another.
    if (m.hosting) {
        port = (uint16_t)std::strtoul(m.port_text.c_str(), nullptr, 10);
        if (port == 0) port = 25301;
    } else {
        port = (uint16_t)(49152 + std::random_device{}() % 16000);
    }

    const std::string log = dir + "/server.log", file = dir + "/world.db",
                      key = dir + "/server.key", seed_s = std::to_string(seed),
                      port_s = std::to_string(port);

    // Played alone the world is joined with player.key, hosted with the key
    // of a password, and the world file knows the name as whichever came
    // first. The world is this machine's, so the name is freed for the key
    // joining now to take over, with the bag and the place that went with it
    // (cnp_server --free-name).
    std::vector<WorldDb::NameRow> names;
    std::string                   names_err;
    if (m.has_identity && WorldDb::list_names(file, names, names_err))
        for (const WorldDb::NameRow &r : names)
            if (r.name == m.player &&
                std::memcmp(r.identity, m.identity.public_key, net::kSignPublicBytes) != 0 &&
                WorldDb::free_name(file, r.name, names_err))
                std::printf("'%s' on this world is now this identity's\n", r.name.c_str());

    if (m.hosting) {
        const std::string ip = local_ipv4();
        g_hosting_at = ip.empty() ? "port " + port_s : ip + ":" + port_s;
    } else {
        g_hosting_at.clear();
    }
    // Emptied here and not only by the child: a log still saying the last run
    // was listening, read before the child got to it, is a world that looks up.
    std::ofstream(log, std::ios::trunc);

    std::vector<const char *> args{prog.c_str(),   "--listen",     port_s.c_str(),
                                   "--ticks",      "0",            "--seed",
                                   seed_s.c_str(), "--world-file", file.c_str(),
                                   "--key-file",   key.c_str(),    "--require-identity"};
    if (!m.hosting) {
        args.push_back("--bind");
        args.push_back("127.0.0.1");
    }
    // The world is this machine's, so whoever plays it from here owns it:
    // /ban, /role and the rest (src/server/admin.h) are theirs.
    if (m.has_identity) {
        args.push_back("--owner");
        args.push_back(m.player.c_str());
    }

    // The world with its two ways out, as tools/track.sh runs it: the plaza's
    // first opening to the track, its second to the Inferno, each a room of the
    // same server with a world file beside this one. What comes before the
    // first --room is every room's, so the sandbox's own file moves into its
    // room. Without the games beside this program, the lone world it always was.
    const std::string games = data_dir("games", CNP_SOURCE_ROOT "/games");
    const std::string track_mods = games + "/track/mods", track_tex = games + "/track/textures",
                      inferno_mods = games + "/inferno/mods",
                      inferno_tex = games + "/inferno/textures", track_file = dir + "/pista.db",
                      // The Inferno loads the sandbox's mods, and their pictures with them.
                      sandbox_tex = data_dir("assets/textures", CNP_SOURCE_ROOT "/assets/textures"),
                      inferno_file = dir + "/inferno.db";
    const std::string arena_mods = games + "/yafps/mods", arena_tex = games + "/yafps/textures",
                      arpg_mods = games + "/arpg/mods", chambers_mods = games + "/chambers/mods",
                      chambers_tex = games + "/chambers/textures",
                      arena_file = dir + "/arena.db", arpg_file = dir + "/arpg.db",
                      chambers_file = dir + "/chambers.db",
                      backrooms_mods = games + "/backrooms/mods",
                      backrooms_tex = games + "/backrooms/textures",
                      backrooms_file = dir + "/backrooms.db",
                      parkour_mods = games + "/parkour/mods",
                      parkour_tex = games + "/parkour/textures",
                      parkour_file = dir + "/parkour.db",
                      lab_mods = games + "/lab/mods", lab_tex = games + "/lab/textures",
                      lab_file = dir + "/laboratorio.db",
                      front_mods = games + "/front/mods", front_file = dir + "/front.db",
                      assault_mods = games + "/assault/mods";
    const bool rooms = std::filesystem::is_directory(track_mods, ec) &&
                       std::filesystem::is_directory(inferno_mods, ec) &&
                       std::filesystem::is_directory(arena_mods, ec) &&
                       std::filesystem::is_directory(arpg_mods, ec) &&
                       std::filesystem::is_directory(chambers_mods, ec) &&
                       std::filesystem::is_directory(backrooms_mods, ec) &&
                       std::filesystem::is_directory(parkour_mods, ec) &&
                       std::filesystem::is_directory(lab_mods, ec) &&
                       std::filesystem::is_directory(front_mods, ec) &&
                       std::filesystem::is_directory(assault_mods, ec);
    if (rooms) {
        args.erase(args.begin() + 7, args.begin() + 9);   // "--world-file", file
        for (const char *a : {"--portals", "--room", "sandbox", "--world-file", file.c_str(),
                              "--portal-to", "@pista/0", "--portal-to", "@inferno/0",
                              "--portal-to", "@arena/0", "--portal-to", "@arpg/0",
                              "--portal-to", "@chambers/0", "--portal-to", "@backrooms/0",
                              "--portal-to", "@parkour/0", "--portal-to", "@laboratorio/0",
                              // The arches of the ways out, on the plaza's north edge (mods/keys).
                              "--portal-to", "@pista/3", "--portal-to", "@inferno/3",
                              "--portal-to", "@arena/1", "--portal-to", "@arpg/1",
                              "--portal-to", "@chambers/1", "--portal-to", "@backrooms/1",
                              "--portal-to", "@parkour/1", "--portal-to", "@front/0",
                              "--portal-to", "@assalto/0",
                              "--room", "pista", "--mods", track_mods.c_str(), "--textures",
                              track_tex.c_str(), "--world-file", track_file.c_str(),
                              "--portal-to", "@sandbox/0", "--portal-to", "@inferno/2",
                              "--portal-to", "@inferno/1", "--portal-to", "@sandbox/8", "--room", "inferno", "--mods",
                              inferno_mods.c_str(), "--textures", inferno_tex.c_str(),
                              "--fallback-textures", sandbox_tex.c_str(),
                              "--world-file", inferno_file.c_str(), "--portal-to", "@sandbox/1",
                              "--portal-to", "@pista/2", "--portal-to", "@pista/1", "--portal-to", "@sandbox/9", "--room",
                              "arena", "--mods", arena_mods.c_str(), "--textures",
                              arena_tex.c_str(), "--world-file", arena_file.c_str(),
                              "--portal-to", "@sandbox/2", "--portal-to", "@sandbox/10", "--room",
                              "arpg", "--mods", arpg_mods.c_str(), "--world-file",
                              arpg_file.c_str(), "--portal-to", "@sandbox/3", "--portal-to",
                              "@sandbox/11", "--room", "chambers", "--mods", chambers_mods.c_str(),
                              "--textures", chambers_tex.c_str(), "--world-file",
                              chambers_file.c_str(), "--portal-to", "@sandbox/4", "--portal-to",
                              "@sandbox/12", "--room", "backrooms", "--mods",
                              backrooms_mods.c_str(), "--textures", backrooms_tex.c_str(),
                              "--world-file", backrooms_file.c_str(), "--portal-to",
                              "@sandbox/5", "--portal-to", "@sandbox/13", "--room", "parkour",
                              "--mods", parkour_mods.c_str(), "--textures", parkour_tex.c_str(),
                              "--world-file", parkour_file.c_str(), "--portal-to", "@sandbox/6",
                              "--portal-to", "@sandbox/14", "--room", "laboratorio", "--mods",
                              lab_mods.c_str(), "--textures", lab_tex.c_str(), "--world-file",
                              lab_file.c_str(), "--portal-to", "@sandbox/7", "--portal-to", "@sandbox/7",
                              "--room", "front", "--mods", front_mods.c_str(), "--world-file",
                              front_file.c_str(), "--portal-to", "@sandbox/15",
                              // No world file: the dimension is whole again once nobody is in it.
                              "--room", "assalto", "--mods", assault_mods.c_str(),
                              "--portal-to", "@sandbox/16", "--portal-to", "@sandbox/16"})
            args.push_back(a);
    }
    const int rooms_up = rooms ? 11 : 1;   // "listening on UDP" once per room

    if (!srv.start(prog, std::vector<std::string>(args.begin() + 1, args.end()), dir, log, err))
        return false;

    m.page     = MenuPage::Starting;
    m.starting = std::filesystem::path(dir).filename().string();
    m.progress = 0.0f;
    m.doing    = "starting the server";
    // A room's start, in the order its server logs it: each line says the step
    // before it is over and which one is next. The bar moves a sixth of a room
    // per step -- steps are not equal (the inferno's mapgen is most of its
    // room), but they are the only clock there is.
    struct Step { const char *line, *next; };
    static constexpr Step kSteps[] = {
        {"generated in", "loading the mods"},
        {"mods running", "the mods shaping the land"},
        {"mapgen stage(s)", "lighting"},
        {"lit ", "saving the world"},
        {"wrote ", "the mods building"},
        {"listening on UDP", "opening the next room"},
    };
    constexpr int kStepCount = (int)(sizeof kSteps / sizeof kSteps[0]);
    const double give_up = plat_time() + 120.0;
    while (!plat_should_close()) {
        std::ifstream in(log);
        std::string   line, last, why, room = m.starting;
        int           listening = 0, step = 0;
        while (std::getline(in, line)) {
            if (const size_t r = line.find("server: room '"); r != std::string::npos) {
                const size_t e = line.find('\'', r + 14);
                room = line.substr(r + 14, e == std::string::npos ? 0 : e - r - 14);
                step = 0;
                m.doing = room + ": generating the terrain";
            }
            for (int k = step; k < kStepCount; ++k)
                if (line.find(kSteps[k].line) != std::string::npos) {
                    step    = k + 1;
                    m.doing = room + ": " + kSteps[k].next;
                    break;
                }
            if (line.find("listening on UDP") != std::string::npos) ++listening, step = 0;
            if (const size_t k = line.find("--server-key "); k != std::string::npos)
                key_hex = line.substr(k + 13, 2 * net::kKeyBytes);
            if (line.find("different block table") != std::string::npos) why = line;
            if (line.find_first_not_of(' ') != std::string::npos) last = line;
        }
        if (listening >= rooms_up) return true;
        m.progress = std::min(1.0f, (listening + (float)step / kStepCount) / (float)rooms_up);

        if (srv.exited() || plat_time() > give_up) {
            srv.stop();
            err = why.empty() ? last : why;
            err.erase(0, err.find_first_not_of(' '));
            if (err.empty()) err = "the world's server stopped; see " + log;
            return false;
        }
        draw_title(s, m, title_panel(m, false));
    }
    srv.stop();
    return false;
}

// The title screen through to somewhere to go: a world of this machine's,
// started here and pinned by the key its server prints, or somebody else's
// server, which is not pinned. False when the player quit instead. Called
// again when a join fails, which is how a wrong address comes back to the list
// rather than ending the game.
bool title_choose(Scene &s, TitleMenu &m, bool &debug, LocalServer &local, std::string &host,
                  uint16_t &port, std::string &pin_key)
{
    for (;;) {
        const MenuChoice choice = title_menu(s, m, debug);
        if (choice == MenuChoice::Quit) return false;
        if (choice == MenuChoice::Server) {
            host = m.host;
            port = m.port;
            pin_key = m.chose_row < m.servers.size() ? m.servers[m.chose_row].key : "";
            return true;
        }

        std::string err;
        if (serve_world(s, m, local, m.dir, m.world_seed, port, pin_key, err)) {
            host = "127.0.0.1";
            return true;
        }
        std::printf("title screen: %s did not start: %s\n", m.dir.c_str(), err.c_str());
        m.worlds = saved_worlds();
        m.first  = 0;
        m.error  = err;
        m.page   = MenuPage::Worlds;
    }
}

// The links drawn this frame, into `out`, and how many. A world's first two as
// they are when it has no more -- every scene with a pair, whose gates measure
// exactly those. Past a pair, the nearest in sight, each lent a target:
// link_ab's, link_ba's, then Scene::more_rt, made here the first time. One in
// sight past those four is not drawn and shows what is behind its frame.
void make_more_rt(Scene &s)
{
    if (s.more_rt[0].id != 0) return;
    const int rw = portal_rt_dim(render_width()), rh = portal_rt_dim(render_height());
    for (gfx_rt &rt : s.more_rt) rt = gfx_rt_create(rw, rh, true);
    s.portal_render.shutdown_recursion();
    s.portal_render.init_recursion(2 + Scene::kMoreShown, g_recursion_depth, rw, rh);
}

int shown_links(Scene &s, Vec3 eye, const Mat4 &view_projection, PortalLink *out)
{
    Session &w = s.act();
    if (w.more_links.empty()) {
        out[0] = w.link_ab;
        if (w.link_count < 2) return 1;
        out[1] = w.link_ba;
        return 2;
    }

    make_more_rt(s);

    std::vector<const PortalLink *> seen;
    auto consider = [&](const PortalLink &l) {
        if (PortalRenderer::opening_in_view(l.src, view_projection)) seen.push_back(&l);
    };
    consider(w.link_ab);
    consider(w.link_ba);
    for (const PortalLink &l : w.more_links) consider(l);
    std::sort(seen.begin(), seen.end(), [&](const PortalLink *a, const PortalLink *b) {
        return length_sq(a->src.center - eye) < length_sq(b->src.center - eye);
    });

    const gfx_rt lent[2 + Scene::kMoreShown] = {w.link_ab.rt, w.link_ba.rt, s.more_rt[0],
                                                s.more_rt[1]};
    const int count = std::min((int)seen.size(), 2 + Scene::kMoreShown);
    for (int k = 0; k < count; ++k) {
        out[k]    = *seen[(size_t)k];
        out[k].rt = lent[k];
    }
    return count;
}

void render_frame(Scene &s, Vec3 eye, Vec3 forward, Vec3 up, bool hud, const char *caption,
                  bool draw_portals = true)
{
    s.cam_set     = true;
    s.cam_eye     = eye;
    s.cam_forward = forward;
    s.cam_up      = up;
    apply_clocks(s);

    // The body's eye, plus whatever of a step up has not been absorbed yet.
    eye = eye + camera_warp(s);
    eye.y += s.step.lag;

    up = camera_up(s.roll, forward, up);

    // The ear is the camera. Set here rather than at the call sites so that a
    // sound played from anywhere is heard from where the last frame was drawn,
    // which is the only place the player believes they are.
    s.ear.pos     = eye;
    s.ear.forward = normalize(forward);
    s.ear.right   = normalize(cross(forward, up));

    const float aspect     = (float)render_width() / (float)render_height();
    const Mat4  projection = mat4_perspective(g_fov_y, aspect, kZNear, kZFar);
    const Mat4  view       = mat4_look_to(eye, forward, up);

    gfx_begin_frame();

    for (int i = 0; i < s.count(); ++i) {
        s.at(i).world_render.set_shadow_size(g_shadow_size);
        s.at(i).world_render.set_classic(g_classic_light);
    }

    // Pass 1: render targets first, so the driver returns to framebuffer 0 and
    // the main pass binds a clean target.
    if (s.act().portals) {
        diag::ProfScope _("portals");

        // One call for both links, so that a portal seen through the other one
        // shows what is inside it instead of a hole. At recursion depth zero
        // this does exactly what two render_target calls did.
        PortalLink links[2 + Scene::kMoreShown];
        const int  count = shown_links(s, eye, mat4_multiply(projection, view), links);

        // Which world goes inside the opening, and which sky it clears to. When
        // the pair leads elsewhere these are the OTHER session's -- the whole of
        // M5 in two lines, and the reason the sky and the view range had to stop
        // being constants first.
        if (!s.act().many_mouths()) {
            Session &shown = s.linked();
            const int t    = s.act().link_session;   // the index linked() picks
            s.portal_shown = (t >= 0 && t < s.count()) ? t : s.active;
            shown.world_render.set_view_range(shown.view_range);
            s.portal_render.set_sky(shown.sky);
            s.portal_render.render_recursive(shown.world_render, links, count, eye, forward, up,
                                             g_fov_y, aspect, kZNear, kZFar);
            s.last_oblique = s.portal_render.oblique_applied(0);
            s.last_clip    = s.portal_render.clip_applied(0);
            s.last_oblique_b = s.portal_render.oblique_applied(1);
            s.last_clip_b    = s.portal_render.clip_applied(1);
        } else {
            // Mouths onto several worlds: one pass each, each with the world
            // and the sky it shows. A mouth whose world has not been linked
            // yet is skipped rather than drawn from a stale link.
            for (int k = 0; k < net::kMaxMouths; ++k) {
                const int shows = mouth_world(s.act(), k);
                if (shows < 0 || shows >= s.count() || !mouth_shown(s.act(), k, eye)) continue;
                Session &shown = s.at(shows);
                s.portal_shown = shows;
                shown.world_render.set_view_range(shown.view_range);
                s.portal_render.set_sky(shown.sky);
                s.portal_render.render_recursive(shown.world_render, &mouth_link(s.act(), k), 1,
                                                 eye, forward, up, g_fov_y, aspect, kZNear, kZFar);
                if (k < 2) {
                    (k == 0 ? s.last_oblique : s.last_oblique_b) = s.portal_render.oblique_applied(0);
                    (k == 0 ? s.last_clip : s.last_clip_b)       = s.portal_render.clip_applied(0);
                }
            }
        }
    }

    // The pairs of a world with mouths (Session::local_links): the nearest
    // two in sight, each lent one of more_rt, with this world inside them.
    s.local_shown_count = 0;
    if (!s.act().local_links.empty()) {
        diag::ProfScope _("portals");
        make_more_rt(s);
        const Mat4 vp = mat4_multiply(projection, view);
        std::vector<const PortalLink *> seen;
        for (const PortalLink &l : s.act().local_links)
            if (PortalRenderer::opening_in_view(l.src, vp)) seen.push_back(&l);
        std::sort(seen.begin(), seen.end(), [&](const PortalLink *a, const PortalLink *b) {
            return length_sq(a->src.center - eye) < length_sq(b->src.center - eye);
        });
        const int count = std::min((int)seen.size(), Scene::kMoreShown);
        for (int k = 0; k < count; ++k) {
            s.local_shown[k]    = *seen[(size_t)k];
            s.local_shown[k].rt = s.more_rt[k];
        }
        s.local_shown_count = count;
        if (count > 0) {
            s.portal_shown = s.active;
            s.act().world_render.set_view_range(s.act().view_range);
            s.portal_render.set_sky(s.act().sky);
            s.portal_render.render_recursive(s.act().world_render, s.local_shown, count, eye,
                                             forward, up, g_fov_y, aspect, kZNear, kZFar);
        }
    }

    // The sun's shadow map around the eye, after the openings, which drew
    // their own into the same target.
    {
        diag::ProfScope _("shadows");
        s.act().world_render.draw_shadows(eye, forward);
    }

    // Pass 2: the main view. Into the capture target when a gate is watching,
    // and straight to the window otherwise.
    const bool captured = g_capture_rt.id != 0;
    if (captured) gfx_rt_begin(g_capture_rt);
    gfx_viewport(0, 0, render_width(), render_height());

    gfx_clear(s.act().sky[0], s.act().sky[1], s.act().sky[2], s.act().sky[3], true);

    // Debug: the render target itself, across the whole screen, sampled exactly
    // the way an opening samples it. When a portal shows the wrong thing the
    // first question is whether the target is wrong or the opening is reading
    // it wrong, and every hour spent guessing between those two is an hour this
    // costs nothing to answer. Everything below is skipped, HUD included.
    if (s.act().portals && s.blit_link != 0) {
        s.portal_render.blit_rt(s.blit_link == 1 ? s.act().link_ab : s.act().link_ba);
        if (captured) gfx_rt_end();
        gfx_end_frame();
        return;
    }

    gfx_state state = gfx_state_default();
    gfx_state_set(&state);

    {
        diag::ProfScope _("world");
        s.act().world_render.set_view_range(s.act().view_range);
        s.act().world_render.set_occlusion(g_occlusion);
        // The water in sight, for everything this camera draws to fade what
        // is seen through it -- the sky, bodies, models, openings too, until
        // it is turned off below. Each pixel is wet or dry by where its own
        // line starts and runs, not by where the eye is (CNP_WATER_GLSL in
        // render/shaders.h).
        s.act().world_render.set_water(true, eye);
        s.act().world_render.draw(projection, view);
    }

    // Bodies go with the opaque pass, before the openings: they write depth,
    // and an opening drawn first would be occluded by a crate standing in it.
    {
        diag::ProfScope _("bodies");
        draw_bodies(s, s.act(), projection, view);
    }
    // Glass and water over them: a body behind a crystal is seen through it.
    s.act().world_render.draw_translucent(projection, view);

    if (s.model_ready) {
        diag::ProfScope _("model");
        const Mat4 place = mat4_multiply(mat4_translate(s.model_pos), mat4_rotate_y(s.model_yaw));
        s.model_render.draw(projection, view, place, s.model_bones);
    }

    // And whatever the mods draw, once per camera. Here and not once per frame:
    // a portal draws this world again from another camera, and a mod's drawing
    // belongs in that image too or a hologram is missing through every opening.
    {
        diag::ProfScope _("mods");
        draw_mod_passes(s, s.active, projection, view);
    }

    // Around the BOXES of what is aimed at, and not around its cell. A slab
    // outlined at its cell is an outline around half a node of air, and the
    // player is told they are pointing at something they are not: the boxes are
    // what a ray stopped on (registry/node_shape.h) and what stopped their feet.
    // A track piece is outlined whole, whichever of its cells is aimed at:
    // breaking any of them takes all of it. And a piece in the hand is shown
    // where it would go -- on the grid, entered from the player's side and
    // turned by Z / X -- as itself, seen through, and red where something is
    // in the way.
    const auto outline_piece = [&](const PieceShape &p, uint8_t facing, int ox, int oy, int oz,
                                   const float colour[4]) {
        int   nx, nz;
        float lo, hi;
        piece_size(p, facing, nx, nz);
        piece_heights(p, facing, lo, hi);
        s.selection.draw_box(Vec3{(float)ox, (float)oy + lo, (float)oz},
                             Vec3{(float)nx, hi - lo, (float)nz}, colour, projection, view);
    };
    if (s.target.hit && s.hand_block != kAir && s.act().registry.get(s.hand_block).piece.plan != 0) {
        const BlockId     id  = s.hand_block;
        const PieceShape &p   = s.act().registry.get(id).piece;
        const int         cx  = s.target.x + s.target.nx, cy = s.target.y + s.target.ny,
                          cz  = s.target.z + s.target.nz;
        const uint8_t     f   = (uint8_t)((node_placement_facing(NodeTurn::Upright,
                                                                 (float)cx + 0.5f - eye.x,
                                                                 (float)cz + 0.5f - eye.z, 0, 1, 0) +
                                           s.piece_turn) & 3);
        int ox, oz;
        piece_origin(p, f, cx, cz, ox, oz);
        std::vector<PieceCell> cells;
        piece_cells(p, id, f, ox, cy, oz, cells);
        // And not over the player: a piece turned back towards them is
        // refused by the server (net::Edit::turn).
        // Its cells, as the server asks: a turn's empty inner corner is free.
        const bool over_me = std::any_of(cells.begin(), cells.end(), [&](const PieceCell &c) {
            return eye.x > (float)c.x - 0.3f && eye.x < (float)c.x + 1.3f && eye.z > (float)c.z - 0.3f &&
                   eye.z < (float)c.z + 1.3f && eye.y - 1.6f < (float)c.y + 1.0f && eye.y > (float)c.y;
        });
        const bool room = !over_me && piece_has_room(s.act().world, s.act().registry, cells);

        // The road, its curbs and its sides as three shades of one colour, so
        // the shape reads without a light. Made again only when the piece or
        // its turn changes.
        static BlockId            ghost_id = kAir;
        static uint8_t            ghost_f  = 0;
        static std::vector<float> ghost[3];
        if (ghost_id != id || ghost_f != f) {
            ghost_id = id;
            ghost_f  = f;
            std::vector<PieceTri> tris;
            piece_triangles(p, f, tris);
            for (auto &g : ghost) g.clear();
            for (const PieceTri &t : tris)
                for (const Vec3 &v : t.v) ghost[t.role < 3 ? t.role : 2].insert(
                    ghost[t.role < 3 ? t.role : 2].end(), {v.x, v.y, v.z});
        }
        const float shade[3] = {1.0f, 0.8f, 0.6f};
        for (int r = 0; r < 3; ++r) {
            const float k         = shade[r];
            const float colour[4] = {room ? 0.55f * k : 0.95f * k, room ? 0.75f * k : 0.15f * k,
                                     room ? 0.95f * k : 0.10f * k, 0.45f};
            s.selection.draw_tris(ghost[r].data(), (int)ghost[r].size() / 3,
                                  Vec3{(float)ox, (float)cy, (float)oz}, colour, projection, view);
        }
    }

    if (s.target.hit) {
        const BlockId  id     = (BlockId)s.act().world.get_block(s.target.x, s.target.y,
                                                                 s.target.z);
        const NodeDef &def    = s.act().registry.get(id);
        const uint8_t  facing = s.act().world.get_facing(s.target.x, s.target.y, s.target.z);

        int ox, oy, oz;
        const bool of_piece = (def.turn == NodeTurn::Height || def.piece.plan != 0) &&
                              piece_find(s.act().world, s.act().registry, s.target.x, s.target.y,
                                         s.target.z, ox, oy, oz);
        const int count = of_piece ? 0 : node_shape_count(def);
        if (of_piece) {
            const float colour[4] = {0.03f, 0.03f, 0.05f, 1.0f};
            outline_piece(s.act().registry.get(s.act().world.get_block(ox, oy, oz)).piece,
                          s.act().world.get_facing(ox, oy, oz), ox, oy, oz, colour);
        }
        for (int i = 0; i < count; ++i) {
            const NodeBox b = node_shape_box(def, facing, i, s.target.x, s.target.y, s.target.z);
            const float   colour[4] = {0.03f, 0.03f, 0.05f, 1.0f};
            s.selection.draw_box(Vec3{b.min[0], b.min[1], b.min[2]},
                                 Vec3{b.max[0] - b.min[0], b.max[1] - b.min[1],
                                      b.max[2] - b.min[2]},
                                 colour, projection, view);
        }
    }

    // The openings go INSIDE the opaque pass, never after it: transparent
    // geometry writes depth, and an opening drawn afterwards is z-culled by any
    // glass standing in front of the portal.
    uint32_t mouths_drawn = 0;
    if (draw_portals && s.act().portals) {
        diag::ProfScope _("openings");
        if (!s.act().more_links.empty()) {
            PortalLink links[2 + Scene::kMoreShown];
            const int  count = shown_links(s, eye, mat4_multiply(projection, view), links);
            for (int k = 0; k < count; ++k)
                s.portal_render.draw_opening(links[k], eye, projection, view);
        } else {
            // With mouths onto several worlds, only those drawn into this frame.
            const bool many = s.act().many_mouths();
            const auto shown = [&](int m) {
                return !(s.act().sealed >> m & 1u) && (!many || mouth_shown(s.act(), m, eye));
            };
            if ((!s.act().two_mouths() || s.act().links_elsewhere()) && shown(0)) {
                s.portal_render.draw_opening(s.act().link_ab, eye, projection, view);
                mouths_drawn |= 1u;
            }
            if (s.act().link_count >= 2 && shown(1)) {
                s.portal_render.draw_opening(s.act().link_ba, eye, projection, view);
                mouths_drawn |= 2u;
            }
            for (int m = 2; m < net::kMaxMouths; ++m)
                if (mouth_world(s.act(), m) >= 0 && shown(m)) {
                    s.portal_render.draw_opening(mouth_link(s.act(), m), eye, projection, view);
                    mouths_drawn |= 1u << m;
                }
        }
    }
    if (draw_portals)
        for (int k = 0; k < s.local_shown_count; ++k)
            s.portal_render.draw_opening(s.local_shown[k], eye, projection, view);
    if (draw_portals) {
        // Green, blue, orange, and red for a mouth sealed to this player.
        static constexpr float kClosed[4][3] = {{0.20f, 0.90f, 0.35f},
                                                {0.16f, 0.51f, 1.00f},
                                                {1.00f, 0.55f, 0.08f},
                                                {0.95f, 0.10f, 0.08f}};
        const auto colour = [](const Portal &p) { return kClosed[p.colour <= 3 ? p.colour : 0]; };
        for (const Portal &p : s.act().closed_openings)
            s.portal_render.draw_closed(surface_from_portal(p), colour(p), eye, projection, view);

        // The mouths not drawn, and the ones drawn from this frame on, which
        // open: walking up to a door of the plaza swaps it for the nearest.
        const auto now = std::chrono::steady_clock::now();
        Session &w = s.act();
        // Just arrived: what it drew when it was last stood in -- nothing, the
        // first time -- is not what is open now. Without this the mouth just
        // come out of opened in green round the player, in a whole screen of
        // membrane at a kart's crossing into the Inferno and back.
        if (s.mouths_world != &w) {
            s.mouths_world = &w;
            w.mouths_drawn = mouths_drawn & w.mouths_here;
            w.sealed_drawn = w.sealed;
        }
        if (w.more_links.empty())
            for (int m = 0; m < net::kMaxMouths; ++m) {
                const uint32_t bit = 1u << m;
                if (!(w.mouths_here & bit)) continue;
                Portal p = mouth_portal(w, m);
                if (!(mouths_drawn & bit)) {
                    if (w.sealed & bit) p.colour = 3;
                    s.portal_render.draw_closed(surface_from_portal(p), colour(p), eye, projection, view);
                } else if (!(w.mouths_drawn & bit) && w.portals_heard) {
                    if (w.sealed_drawn & bit) p.colour = 3;
                    w.opening.push_back({p, now});
                }
            }
        w.mouths_drawn = mouths_drawn & w.mouths_here;
        w.sealed_drawn = w.sealed;

        // After the openings themselves, so the image is there to open onto.
        constexpr float kOpeningSeconds = 0.9f;
        auto &opening = s.act().opening;
        std::erase_if(opening, [&](const Session::Opening &o) {
            return std::chrono::duration<float>(now - o.t0).count() >= kOpeningSeconds;
        });
        for (const Session::Opening &o : opening) {
            const float k = std::chrono::duration<float>(now - o.t0).count() / kOpeningSeconds;
            s.portal_render.draw_closed(surface_from_portal(o.p), colour(o.p), eye, projection, view,
                                        k * k * (3.0f - 2.0f * k));
        }
    }
    if ((hud || s.play_arms) && (s.act().view_distance <= 0.0f || at_fixed_eye(s.act())) && !s.driving_view &&
        !s.seat_view)
        draw_arms(s, projection);
    s.act().world_render.set_water(false);

    // Back to the window before anything the platform draws: its text goes
    // through its own pipeline and has no business inside a capture.
    if (captured) gfx_rt_end();

    // The hotbar, always -- it is not the F3 overlay, it is part of the game.
    // Outside the capture for the same reason the debug text is: the portal
    // gates compare a captured frame against a reference render of the same
    // world, and a bar across the bottom of one of them is a difference that
    // has nothing to do with what they measure. The HUD gate renders it into a
    // target of its own instead.
    if (s.hotbar.items != nullptr && !s.hotbar.slots.empty()) {
        diag::ProfScope _("ui");
        gfx_2d_begin(gfx_screen_width(), gfx_screen_height());
        ui::IconSource icons;
        icons.atlas = s.act().world_render.atlas();
        icons.tiles = s.act().world_render.atlas_tiles();

        if (s.hotbar.open) {
            // One question, asked once, and both halves of the drag read the
            // answer: the square that empties and the stack that appears under
            // the cursor are the same stack, and asking twice is how they come
            // to disagree for a frame.
            //
            // Only while the button is actually DOWN: a square left empty after
            // the release, or a ghost left behind, is a player told they are
            // still holding something they let go of.
            const int      press = s.hotbar.press;
            const ui::Grid from  = s.hotbar.press_grid;

            // Whichever button is holding this drag up. Asking about the left
            // one only would leave a right drag with no ghost at all.
            const bool holding = s.hotbar.press_right ? plat_mouse_down(PLAT_MOUSE_RIGHT)
                                                      : plat_mouse_down(PLAT_MOUSE_LEFT);
            const bool lifting = press >= 0 && holding && s.hotbar.press_has_item();

            // Which square each grid should draw as lifted: its own, and -1 for
            // the two it is not. Written once because there are three grids now
            // and "is the drag mine" asked three times is three chances to draw
            // two holes.
            //
            // A RIGHT drag lifts nothing, because it carries only half: the
            // square keeps the other half, and what it draws is patched below.
            const auto lifted = [&](ui::Grid g) {
                return (lifting && !s.hotbar.press_right && from == g) ? press : -1;
            };

            // The picture a right drag leaves behind. A copy, patched, rather
            // than the square drawn empty: a square shown empty while it still
            // holds three is the panel lying about the bag -- and the count
            // under the cursor plus the count in the square have to add up to
            // what the player had.
            std::vector<ItemStack> kept;
            const ItemStack       *bag_shown   = s.hotbar.slots.data();
            const ItemStack       *chest_shown = s.hotbar.chest.data();
            const ItemStack       *craft_shown = s.hotbar.craft.data();

            if (lifting && s.hotbar.press_right) {
                kept = s.hotbar.grid_of(from);
                if (press < (int)kept.size()) {
                    ItemStack     &src   = kept[(size_t)press];
                    const uint16_t taken = ui::half_of(src.count);
                    src.count            = (uint16_t)(src.count - taken);
                    if (src.count == 0) src.item = kNoItem;
                }
                switch (from) {
                case ui::Grid::Bag: bag_shown = kept.data(); break;
                case ui::Grid::Chest: chest_shown = kept.data(); break;
                case ui::Grid::Craft: craft_shown = kept.data(); break;
                case ui::Grid::Result: break;   // half_drag_starts refuses it
                }
            }

            s.hotbar.panel =
                ui::draw_bag(bag_shown, (int)s.hotbar.slots.size(), s.hotbar.selected,
                             lifted(ui::Grid::Bag), *s.hotbar.items,
                             s.act().registry, icons, gfx_screen_width(), gfx_screen_height());

            // The chest above it, after the bag, because the bag draws the dim
            // sheet over the whole screen and a grid drawn before it would be
            // under it.
            if (s.hotbar.chest_open && !s.hotbar.chest.empty()) {
                s.hotbar.chest_panel = ui::draw_chest(
                    chest_shown, (int)s.hotbar.chest.size(),
                    lifted(ui::Grid::Chest), *s.hotbar.items,
                    s.act().registry, icons, s.hotbar.panel,
                    s.hotbar.chest_roles.size() == s.hotbar.chest.size() ? s.hotbar.chest_roles.data()
                                                                          : nullptr,
                    s.hotbar.chest_progress < 0.0f
                        ? -1.0f
                        : std::min(1.0f, s.hotbar.chest_progress +
                                             s.hotbar.chest_rate * (float)(plat_time() - s.hotbar.chest_since)),
                    s.hotbar.chest_scroll, s.hotbar.chest_scroll_size,
                    s.hotbar.chest_search ? s.hotbar.chest_query.c_str() : nullptr,
                    s.typing.on && s.typing.panel == "core:chest");
            } else {
                s.hotbar.chest_panel = ui::PanelLayout{};
            }

            // And the craft grid, in the same place, because the two are never
            // up at once: a player is either reaching into a box or arranging
            // things to make one.
            if (!s.hotbar.chest_open && !s.hotbar.craft.empty()) {
                s.hotbar.craft_panel = ui::draw_craft(
                    craft_shown, s.hotbar.craft_w, s.hotbar.craft_h,
                    s.hotbar.craft_result, lifted(ui::Grid::Craft), *s.hotbar.items,
                    s.act().registry, icons, s.hotbar.panel);
            } else {
                s.hotbar.craft_panel = ui::CraftLayout{};
            }

            if (lifting) {
                float mx = 0.0f, my = 0.0f;
                plat_mouse_position(&mx, &my);

                // The result square drags its answer and not a square of a
                // grid, which is why this is not grid_of() alone.
                ItemStack ghost =
                    (from == ui::Grid::Result)
                        ? s.hotbar.craft_result
                        : (press < (int)s.hotbar.grid_of(from).size()
                               ? s.hotbar.grid_of(from)[(size_t)press]
                               : ItemStack{});

                // Half of it under a right drag, and the other half is in the
                // square behind. The same function the move uses, so the two
                // cannot disagree about which half moved.
                if (s.hotbar.press_right && !ghost.empty())
                    ghost.count = ui::half_of(ghost.count);

                if (!ghost.empty())
                    ui::draw_drag_ghost(ghost, mx, my, s.hotbar.panel.slot, *s.hotbar.items,
                                        s.act().registry, icons);
            } else {
                // What the cursor is over, by name. Only with nothing in the
                // hand: while a stack is being dragged the answer would be the
                // square it is passing over, which is not what the player is
                // holding and not what they are asking about.
                //
                // The panels in the order they are drawn in, first answer wins,
                // because they do not overlap and a point is in at most one.
                float mx = 0.0f, my = 0.0f;
                plat_mouse_position(&mx, &my);

                std::string over =
                    ui::name_under(s.hotbar.panel, s.hotbar.slots.data(),
                                   (int)s.hotbar.slots.size(), mx, my, *s.hotbar.items);

                if (over.empty() && s.hotbar.chest_open && !s.hotbar.chest.empty())
                    over = ui::name_under(s.hotbar.chest_panel, s.hotbar.chest.data(),
                                          (int)s.hotbar.chest.size(), mx, my, *s.hotbar.items);

                if (over.empty() && !s.hotbar.chest_open && !s.hotbar.craft.empty()) {
                    over = ui::name_under(s.hotbar.craft_panel.grid, s.hotbar.craft.data(),
                                          (int)s.hotbar.craft.size(), mx, my, *s.hotbar.items);
                    if (over.empty())
                        over = ui::name_under(s.hotbar.craft_panel.result,
                                              &s.hotbar.craft_result, 1, mx, my,
                                              *s.hotbar.items);
                }

                if (!over.empty())
                    ui::draw_item_tooltip(over, mx, my, gfx_screen_width(), gfx_screen_height());
            }
        } else if (s.act().engine_hud) {
            ui::draw_hotbar(s.hotbar.slots.data(), (int)s.hotbar.slots.size(), s.hotbar.selected,
                            *s.hotbar.items, s.act().registry, icons,
                            gfx_screen_width(), gfx_screen_height());

            // And how much of the player is left, above it. Only with the bag
            // shut, because the panel is what the player is looking at while it
            // is up -- and a bar behind a dimmed sheet is a bar nobody reads.
            const ui::HealthLayout bar = ui::draw_health(
                s.hotbar.health, s.hotbar.health_max, gfx_screen_width(), gfx_screen_height());

            // And what is worn, over it: the last five squares of the bag.
            int armor = 0;
            for (size_t i = Inventory::kDefaultSlots; i < s.hotbar.slots.size(); ++i)
                if (!s.hotbar.slots[i].empty())
                    armor += s.hotbar.items->get(s.hotbar.slots[i].item).armor;
            ui::draw_armor(armor, bar);

            // The kart's nitro (shift), over the health: blue while there is
            // some, a dark trough for what has been burnt.
            if (s.nitro >= 0.0f) {
                const float h      = bar.h * 0.6f;
                const float y      = bar.y - h * 2.2f;
                const float trough[4] = {0.05f, 0.05f, 0.08f, 0.75f};
                const float fill[4]   = {0.25f, 0.65f, 1.0f, 0.95f};
                gfx_2d_rect(bar.x, y, bar.w, h, trough);
                gfx_2d_rect(bar.x, y, bar.w * s.nitro, h, fill);
                const float size = h * 2.2f;
                gfx_2d_text("NITRO", bar.x - gfx_2d_text_width("NITRO", size) - h, y - h * 0.6f, size, fill);
            }

            // Where a click lands, in first person and with nothing over the
            // world to click on instead.
            if (s.act().view_distance <= 0.0f && !s.pause.up &&
                !s.act().panels.topmost_for(ui::kLocalPlayer))
                ui::draw_crosshair(gfx_screen_width(), gfx_screen_height());
        }

        // What the world last said to this player. Over the bar and under a
        // mod's panel: it is an answer to something they did, so it belongs
        // where their eyes already are, and it must not sit on top of a thing
        // they are trying to click.
        ui::draw_notice(s.notice.text, s.notice.alpha(s.notice_clock), gfx_screen_width(),
                        gfx_screen_height());

        // And how far through a dig is, which is the only thing on screen while
        // one runs. Both loops fill s.dig_progress from the same function, so
        // this is the same bar in a single-process world and over a socket.
        ui::draw_dig_progress(s.dig_progress, gfx_screen_width(), gfx_screen_height());

        gfx_2d_end();
    }

    // What a world's code draws over the screen: over the bar and the notice,
    // under a panel, which is the thing being clicked when one is up.
    if (s.hud_passes.size() > 0) {
        gfx_2d_begin(gfx_screen_width(), gfx_screen_height());
        draw_hud_passes(s);
        gfx_2d_end();
    }

    // The chat, over what the world's code drew and under a panel.
    gfx_2d_begin(gfx_screen_width(), gfx_screen_height());
    ui::draw_chat(s.chat, s.notice_clock, gfx_screen_width(), gfx_screen_height());
    gfx_2d_end();

    // A mod's panel, over everything the engine drew. Last, because a panel a
    // mod put up is the thing being looked at -- and it is drawn even when the
    // bar is not, so a world with no inventory yet can still show one.
    if (const ui::Panel *panel = s.act().panels.topmost_for(ui::kLocalPlayer)) {
        gfx_2d_begin(gfx_screen_width(), gfx_screen_height());

        const ui::PanelPlacement at =
            ui::place_panel(*panel, gfx_screen_width(), gfx_screen_height());

        float mx = 0.0f, my = 0.0f;
        plat_mouse_position(&mx, &my);

        int typing = -1;
        if (s.typing.on && panel->name == s.typing.panel)
            for (size_t i = 0; i < panel->elements.size(); ++i)
                if (panel->elements[i].kind == ui::ElemKind::Field &&
                    panel->elements[i].id == s.typing.id)
                    typing = (int)i;
        ui::IconSource icons;
        icons.atlas = s.act().world_render.atlas();
        icons.tiles = s.act().world_render.atlas_tiles();
        const int hover = at.button_at(*panel, mx, my);
        ui::draw_mod_panel(*panel, at, hover, gfx_screen_width(), gfx_screen_height(), typing,
                           s.typing.text.c_str(), s.hotbar.items, &s.act().registry, &icons);
        // An item's picture says what it is under the pointer, as the bag's do.
        if (hover >= 0 && panel->elements[(size_t)hover].kind == ui::ElemKind::Item)
            ui::draw_item_tooltip(ui::item_display_name(panel->elements[(size_t)hover].text), mx, my,
                                  gfx_screen_width(), gfx_screen_height());
        gfx_2d_end();
        draw_panel_models(s.act().models, *panel, at);
    }

    // And the pause menu over even that: it is what the player is answering.
    if (s.pause.up) {
        gfx_2d_begin(gfx_screen_width(), gfx_screen_height());

        const ui::Panel          p  = pause_panel(s, hud);
        const ui::PanelPlacement at = ui::place_panel(p, gfx_screen_width(), gfx_screen_height());

        float mx = 0.0f, my = 0.0f;
        plat_mouse_position(&mx, &my);

        ui::draw_mod_panel(p, at, at.button_at(p, mx, my), gfx_screen_width(), gfx_screen_height());
        gfx_2d_end();
        // This world's own player model, as everybody here will see it.
        if (s.pause.look)
            draw_look_preview(s.act().models, s.act().player_model_id, at);
    }

    // The debug lines sit at the top of the screen, where the bag's chest and
    // craft grids and a mod's panel heading are drawn: with any of those up
    // they would be written over the thing being clicked.
    if (hud && !s.hotbar.open && !s.pause.up && !s.act().panels.topmost_for(ui::kLocalPlayer)) {
        // Hand the pipeline back before the platform draws its own text, and
        // give it the state its text expects: alpha blending on, depth off.
        // Glyphs are alpha cut-outs of one texture, so with blending disabled
        // every character renders as a filled rectangle.
        gfx_shader_bind_default();

        gfx_state ui = gfx_state_default();
        ui.depth_test  = false;
        ui.depth_write = false;
        ui.cull        = GFX_CULL_NONE;
        ui.blend       = GFX_BLEND_ALPHA;
        gfx_state_set(&ui);

        char line[256];
        std::snprintf(line, sizeof(line), "FPS %d   %dx%d", plat_fps(),
                      gfx_screen_width(), gfx_screen_height());
        plat_debug_text(line, 12, 12, 20);

        // Both links, because with a facing pair the interesting question is
        // which of the two openings has its oblique plane on, and one flag for
        // whichever was rendered last answered it wrong.
        // The hour of THIS world, and what the shader is multiplying its sun
        // channel by. Next to each other on purpose: the "sun 15" on the aiming
        // line is the light stored in the block and does not move with the
        // clock, and reading one for the other is how a lit world looks broken.
        const double tod  = s.act().time_of_day;
        const int    hour = (int)(tod * 24.0) % 24;
        const int    mins = (int)(tod * 24.0 * 60.0) % 60;

        std::snprintf(line, sizeof(line),
                      "eye %.2f %.2f %.2f   %02d:%02d daylight %.2f wind %.1f   "
                      "A oblique %s clip %s   B oblique %s clip %s",
                      eye.x, eye.y, eye.z, hour, mins, (double)daylight_at(tod), s.act().wind,
                      s.last_oblique ? "ON " : "off", s.last_clip ? "ON " : "off",
                      s.last_oblique_b ? "ON " : "off", s.last_clip_b ? "ON " : "off");
        plat_debug_text(line, 12, 38, 20);

        std::snprintf(line, sizeof(line),
                      "chunks %d drawn / %d culled   %zu verts   %.1f MB gpu   world %zu chunks"
                      " %.1f MB%s",
                      s.act().world_render.draw_calls(), s.act().world_render.culled_chunks(),
                      s.act().world_render.vertex_count(),
                      (double)s.act().world_render.gpu_bytes() / (1024.0 * 1024.0),
                      s.act().world.chunk_count(),
                      (double)s.act().world.memory_bytes() / (1024.0 * 1024.0),
                      g_occlusion ? "" : "   OCCLUSION OFF (F2)");
        plat_debug_text(line, 12, 64, 20);

        if (s.target.hit) {
            // What the block IS, and the two numbers that decide how it is
            // drawn. Added because "the blocks above a certain height go white"
            // was a report nothing in the engine could answer: the light and
            // the tile are what the shader multiplies together, and until they
            // were on the screen the only way to ask was to rebuild.
            const BlockId  id  = s.act().world.get_block(s.target.x, s.target.y, s.target.z);
            const NodeDef &def = s.act().registry.get(id);

            // The light of the cell in FRONT of the face being looked at, which
            // is the one the mesher takes -- the block's own cell is opaque and
            // holds nothing.
            const uint8_t lit = s.act().world.get_light(s.target.x + s.target.nx,
                                                        s.target.y + s.target.ny,
                                                        s.target.z + s.target.nz);

            std::snprintf(line, sizeof(line),
                          "aiming at %d %d %d  %s (id %u)  sun %u block %u  tile %u",
                          s.target.x, s.target.y, s.target.z, def.name.c_str(), (unsigned)id,
                          (unsigned)(lit >> 4), (unsigned)(lit & 15), (unsigned)def.tex[0]);
        } else {
            std::snprintf(line, sizeof(line), "aiming at nothing");
        }
        plat_debug_text(line, 12, 90, 20);

        if (!s.net_status.empty()) plat_debug_text(s.net_status.c_str(), 12, 116, 20);

        if (caption) plat_debug_text(caption, 12, s.net_status.empty() ? 116 : 142, 20);

        if (diag::prof_enabled())
            draw_profiler_overlay(s.net_status.empty() ? 168 : 194);
    }

    {
        // Everything the platform hands to the driver, including the swap when
        // presenting is on. Its own row because it is the one section here that
        // is not this program's work: a frame that is mostly `present` is a
        // frame waiting on the compositor or the GPU, and optimising anything
        // above it would change nothing.
        diag::ProfScope _("present");
        gfx_end_frame();
    }
}

struct Shot {
    const char *name;
    Vec3        feet;
    float       yaw;
    float       pitch;
    const char *caption;
};

// The collision-box override, refreshed before the physics runs.
//
// Kept separate from step_portals because it happens on the other side of the
// physics step: the shift has to be in place BEFORE the player moves, and the
// crossing is decided after they have. Running both from one function would put
// one of the two in the wrong half of the frame.
void update_floor_shift(Scene &s, Player &player, float dt)
{
    if (!s.act().portals && s.act().linked_openings.empty()) {
        player.cb_shift = 0.0f;
        player.cb_top   = 0.0f;
        player.cb_half  = 0.0f;
        return;
    }
    const float half = s.act().movement.width * 0.5f;
    const float was_top = player.cb_top, was_half = player.cb_half;
    player.cb_top  = 0.0f;
    player.cb_half = 0.0f;
    if (!s.act().linked_openings.empty()) {
        float most = 0.0f;
        // Into a wall portal set in the wall: the box made to fit its hole.
        // A mouth onto another world too (games/chambers's way home).
        const auto fit = [&](const Portal &p) {
            const WallShift w = portal_wall_shift(p, player.pos, half, kPlayerHeight, player.eye_height);
            if (w.on) {
                most           = std::max(most, w.bottom);
                player.cb_top  = std::max(player.cb_top, w.top);
                player.cb_half = player.cb_half > 0.0f ? std::min(player.cb_half, w.half) : w.half;
            }
        };
        for (const Portal &p : s.act().linked_openings) {
            most = std::max(most, portal_floor_shift(p, player.pos, player.velocity, dt, half));
            fit(p);
        }
        if (!s.act().portals) {
            fit(s.act().portal_a);
            fit(s.act().portal_b);
            for (const Session::MoreMouth &mm : s.act().more_mouths) fit(mm.portal);
        }
        player.cb_shift = most;
        // Never widened or raised into a wall. The fit narrows as the body
        // moves off the hole's middle and stops at a tenth, so a body in the
        // hole that drifted sideways got its whole width back inside the
        // frame beside it and could not move again (reported from play,
        // 2026-10-05). Kept fitted until the body is clear.
        const auto box = [&](float h, float top) {
            return body_box(player.pos, h > 0.0f ? std::min(h, half) : half, most,
                            std::max(0.3f, kPlayerHeight - top));
        };
        if ((player.cb_half != was_half || player.cb_top != was_top) &&
            box_hits_world(s.act().world, s.act().registry, box(player.cb_half, player.cb_top)) &&
            !box_hits_world(s.act().world, s.act().registry, box(was_half, was_top))) {
            player.cb_half = was_half;
            player.cb_top  = was_top;
        }
        return;
    }
    const float a = portal_floor_shift(s.act().portal_a, player.pos, player.velocity, dt, half);
    const float b = portal_floor_shift(s.act().portal_b, player.pos, player.velocity, dt, half);
    player.cb_shift = (a > b) ? a : b;
}

// Steers a fall towards the floor opening it is about to miss (portal_funnel),
// from the same openings update_floor_shift reads and at the same moment.
void update_funnel(Scene &s, Player &player, float dt)
{
    // Not a kart: its chassis has a velocity of its own, and the server drives
    // the same chassis with nothing pulling it (TODO.md OPEN-47).
    if ((!s.act().portals && s.act().linked_openings.empty()) || player.on_ground || player.flying ||
        s.act().movement.drives())
        return;
    const float g = s.act().movement.gravity;
    if (!s.act().linked_openings.empty()) {
        for (const Portal &p : s.act().linked_openings)
            player.velocity += portal_funnel(p, player.pos, player.velocity, g, dt);
        return;
    }
    player.velocity += portal_funnel(s.act().portal_a, player.pos, player.velocity, g, dt);
    player.velocity += portal_funnel(s.act().portal_b, player.pos, player.velocity, g, dt);
}

int run_bench(Scene &s)
{
    // Everything meshed up front: a bench that photographs a half-built world
    // measures the uploader, not the renderer.
    s.act().world_render.sync(s.act().world, s.act().registry, 100000);

    std::error_code ec;
    std::filesystem::create_directories("bench", ec);
    std::printf("screenshots: %s\n", std::filesystem::absolute("bench", ec).c_str());

    const Shot shots[] = {
        {"01_approach.png", {40.5f, kGroundTop, 47.0f}, 0.0f, 0.0f,
         "criterion 1: portal A at range -- image inside must line up with the world around it"},
        {"02_close.png", {40.5f, kGroundTop, 42.5f}, 0.0f, 0.0f,
         "criterion 1: close in, seam under scrutiny"},
        {"03_at_face.png", {40.5f, kGroundTop, 41.3f}, 0.0f, 0.0f,
         "criterion 1: at the outer face, oblique near plane at its limit"},
        {"04_inside_frame.png", {40.5f, kGroundTop, 40.5f}, 0.0f, 0.0f,
         "criterion 2: camera INSIDE the frame -- oblique must read off, clip on"},
        {"05_grazing.png", {43.0f, kGroundTop, 42.0f}, -1.03f, 0.0f,
         "clip planes: grazing angle, destination frame must not leak in"},
        {"06_behind.png", {40.5f, kGroundTop, 38.5f}, 0.0f, 0.0f,
         "far side of portal A"},
        {"07_portal_B.png", {84.0f, kGroundTop, 60.5f}, -1.5708f, 0.0f,
         "portal B seen directly, for comparison with what A shows"},
        {"10_mod_blocks.png", {45.5f, kGroundTop, 52.0f}, 0.0f, 0.0f,
         "the stepped tower here was placed from Luau, not by the world generator"},
        {"12_palette.png", {64.5f, kGroundTop, 51.0f}, 0.0f, 0.0f,
         "twenty blocks declared by one mod -- the atlas grew from 16 tiles to 64 by itself"},
        {"11_selection.png", {60.0f, kGroundTop, 60.0f}, 0.0f, -0.6f,
         "block outline: the shell of an inflated cube, back faces only"},
    };

    const Vec3 up{0.0f, 1.0f, 0.0f};

    for (const Shot &shot : shots) {
        const Vec3 eye{shot.feet.x, shot.feet.y + kPlayerEyeHeight, shot.feet.z};
        const Vec3 fwd = dir_from_angles(shot.yaw, shot.pitch);

        // Aim, so the selection outline appears in the captures too.
        s.target = raycast(s.act().world, s.act().registry, eye, fwd, 6.0f);

        // Settle: shader compilation and the first uploads land on frame one,
        // and raylib's FPS counter needs a few frames to mean anything.
        for (int i = 0; i < 30; ++i) render_frame(s, eye, fwd, up, true, shot.caption);

        shot_write(shot.name);
        std::printf("  %-16s eye %6.2f %6.2f %6.2f   oblique %-3s  clip %-3s\n",
                    shot.name, eye.x, eye.y, eye.z,
                    s.last_oblique ? "ON" : "off", s.last_clip ? "ON" : "off");
    }

    // Criterion 1, as a file comparison rather than an impression.
    //
    // Take shot 1's viewpoint and photograph it twice: once normally, so the
    // portal shows its image, and once with a real camera moved to where the
    // virtual one sits, with no portals drawn at all. Both use the same
    // projection and the portal samples its target by screen position, so
    // inside the opening the two images must agree pixel for pixel. Anywhere
    // they disagree is geometry leaking through the seam.
    //
    // No HUD on either: text over the frame would diff as a difference.
    {
        const Shot &s1 = shots[0];
        const Vec3 eye1{s1.feet.x, s1.feet.y + kPlayerEyeHeight, s1.feet.z};
        const Vec3 fwd1 = dir_from_angles(s1.yaw, s1.pitch);

        const VirtualCamera v =
            portal_virtual_camera(s.act().link_ab.src, s.act().link_ab.dst, eye1, fwd1, up);

        for (int i = 0; i < 30; ++i) render_frame(s, eye1, fwd1, up, false, nullptr, true);
        shot_write("08a_through_portal.png");

        for (int i = 0; i < 30; ++i) render_frame(s, v.pos, v.forward, v.up, false, nullptr, false);
        shot_write("08b_reference_no_portals.png");

        std::printf("  %-16s vcam %6.2f %6.2f %6.2f   fwd %5.2f %5.2f %5.2f\n",
                    "08_continuity", v.pos.x, v.pos.y, v.pos.z,
                    v.forward.x, v.forward.y, v.forward.z);
        std::printf("  %-16s compare 08a and 08b inside the opening: they must agree\n", "");

        // The contents of the render target, drawn straight to the screen: the
        // destination view with the oblique near plane and the five clip planes
        // applied. Everything the opening shows comes from here, so this is
        // where a wrong portal is diagnosed.
        for (int i = 0; i < 30; ++i) {
            gfx_begin_frame();
            s.portal_shown = s.active;
            s.portal_render.render_target(s.act().world_render, s.act().link_ab, eye1, fwd1, up,
                                          g_fov_y,
                                          (float)gfx_screen_width() / (float)gfx_screen_height(),
                                          kZNear, kZFar, /*to_screen=*/true);
            gfx_end_frame();
        }
        shot_write("09_render_target_contents.png");
        std::printf("  %-16s what the A->B target actually holds\n", "09_rt_contents");
    }

    // ------------------------------------------------------ the event bus
    // Two edits the engine would happily have made, put through the same path
    // the mouse uses. What happens to them is decided entirely by a mod.
    {
        std::printf("\n  events\n");

        const int px = 40, py = 21, pz = 40;          // inside the protected plaza
        const bool placed_in_plaza = try_place(s, px, py, pz, kStone, 0);
        std::printf("    place at %d %d %d inside the plaza: %s\n", px, py, pz,
                    placed_in_plaza ? "PLACED (a mod should have refused it)" : "refused by a mod");

        const int cx = 62, cy = 34, cz = 44;          // above the cloud line
        const bool placed_high = try_place(s, cx, cy, cz, kStone, 0);
        const BlockId got = s.act().world.get_block(cx, cy, cz);
        std::printf("    place at %d %d %d above y=30: %s, and the block is '%s'\n", cx, cy, cz,
                    placed_high ? "placed" : "refused",
                    s.act().registry.get(got).name.c_str());

        // And break it again, which fires core:broken -- where a mod turns the
        // id back into a name through core.node_name, one of the generated
        // bindings.
        try_break(s, cx, cy, cz);

        // The floor of the world must refuse. Checked here rather than trusted,
        // because "unbreakable" is a flag somebody can drop in a refactor and
        // nobody notices until a player digs out of the world again.
        const BlockId floor_before = s.act().world.get_block(60, 0, 60);
        const bool    floor_broke  = try_break(s, 60, 0, 60);
        std::printf("    break the world's floor at 60 0 60: %s, block is still '%s'\n",
                    floor_broke ? "BROKE IT (it should not)" : "refused",
                    s.act().registry.get(s.act().world.get_block(60, 0, 60)).name.c_str());
        if (floor_before != s.act().world.get_block(60, 0, 60))
            std::printf("    FAIL: the floor of the world changed\n");

        s.act().world_render.sync(s.act().world, s.act().registry, 100000);

        const Vec3 eye{52.0f, kGroundTop + kPlayerEyeHeight, 47.5f};
        const Vec3 fwd = dir_from_angles(0.0f, 0.05f);
        for (int i = 0; i < 30; ++i)
            render_frame(s, eye, fwd, up, true,
                         "a mod refused a placement and raised its own event; another mod lit "
                         "every beacon in answer");
        shot_write("13_events.png");
        std::printf("    13_events.png    beacons lit by guard:alarm, handled in another VM\n");
    }

    // ------------------------------------------------------ criterion 3
    {
        // Measured from the middle of the world looking across it, not from a
        // corner looking off the edge: with the camera at the rim most of the
        // world is behind it and the frustum cull does the work the renderer
        // is supposed to be measured on.
        const float mid = (float)g_world_extent * 0.5f;
        const Vec3  eye1{mid, (float)(kGroundTop + 22), mid};
        const Vec3  fwd1 = dir_from_angles(0.7f, -0.12f);

        constexpr int    kWarm    = 120;
        constexpr int    kMeasure = 900;
        constexpr double kFixedDt = 1.0 / 60.0;   // fixed, so the mod's motion is reproducible

        // The measured loop is a whole frame, not just the drawing: script tick,
        // then any remesh the script caused, then the render graph. Timing only
        // the parts that flatter the result is how a benchmark starts lying.
        for (int i = 0; i < kWarm; ++i) {
            step_script(s, kFixedDt);
            s.act().world_render.sync(s.act().world, s.act().registry);
            render_frame(s, eye1, fwd1, up, false, nullptr);
        }

        // Baselines rather than a reset: the mod host accumulates for the life
        // of the process, and the warm-up frames belong to nobody.
        const double   base_script_seconds = s.act().mods.step_seconds();
        const uint64_t base_script_calls   = s.act().mods.step_calls();

        const double t0 = plat_time();
        for (int i = 0; i < kMeasure; ++i) {
            step_script(s, kFixedDt);
            s.act().world_render.sync(s.act().world, s.act().registry);
            render_frame(s, eye1, fwd1, up, false, nullptr);
        }
        const double elapsed = plat_time() - t0;

        const double   script_seconds = s.act().mods.step_seconds() - base_script_seconds;
        const uint64_t script_calls   = s.act().mods.step_calls()   - base_script_calls;

        const double fps        = (double)kMeasure / elapsed;
        const double ms         = 1000.0 * elapsed / (double)kMeasure;
        // Per frame, not per call: with several mods a frame is several calls,
        // and it is the frame that has a budget.
        const double script_ms  = 1000.0 * script_seconds / (double)kMeasure;
        const double script_pct = (elapsed > 0.0) ? 100.0 * script_seconds / elapsed : 0.0;

        std::printf("\ncriterion 3: %d frames in %.3f s -> %.1f FPS (%.3f ms/frame)\n",
                    kMeasure, elapsed, fps, ms);
        std::printf("             %zu vertices total, %.2f MB on the GPU; %d chunks drawn,"
                    " %d frustum culled\n",
                    s.act().world_render.vertex_count(),
                    (double)s.act().world_render.gpu_bytes() / (1024.0 * 1024.0),
                    s.act().world_render.draw_calls(), s.act().world_render.culled_chunks());
        std::printf("             framebuffer %dx%d, 2 portals, vsync off\n",
                    gfx_screen_width(), gfx_screen_height());
        std::printf("             frame rate  %s (threshold 200 FPS)\n",
                    fps >= 200.0 ? "PASS" : "FAIL");

        if (script_calls > 0) {
            std::printf("             scripting   %s (%s, %d mods, %.4f ms/frame = %.3f%% of"
                        " frame, threshold 1%%)\n",
                        script_pct < 1.0 ? "PASS" : "FAIL",
                        script_backend_name(), s.act().mods.running_count(), script_ms, script_pct);
            std::printf("             script heap %zu bytes across every VM\n",
                        s.act().mods.memory_used());
            std::printf("             events      %llu emitted, %llu handler calls,"
                        " %zu schemas, %zu handlers\n",
                        (unsigned long long)s.act().mods.events().emits(),
                        (unsigned long long)s.act().mods.events().dispatches(),
                        s.act().mods.events().schema_count(),
                        s.act().mods.events().handler_count());

            for (const Mod &m : s.act().mods.mods()) {
                if (m.step_calls == 0) continue;
                std::printf("               %-12s %.4f ms/call, %llu calls%s\n",
                            m.manifest.name.c_str(),
                            1000.0 * m.step_seconds / (double)m.step_calls,
                            (unsigned long long)m.step_calls,
                            m.running ? "" : "  (DISABLED)");
            }
        } else {
            std::printf("             scripting   NOT MEASURED (no mod running)\n");
        }
    }

    return 0;
}

// ------------------------------------------------------ continuity self-check
//
// Criterion 1 as a build gate rather than an eyeball. M1 rewrites the mesher,
// the vertex format and the culling; without something that fails on its own,
// a regression in the portal seam would be found weeks later by looking at a
// screenshot and thinking "that seems off".

struct ScreenRect {
    int x0 = 0, y0 = 0, x1 = 0, y1 = 0;
    bool valid = false;
};

// Bounding box of a portal opening in pixels, in OpenGL's bottom-up
// convention, which is how the colour buffer comes back.
ScreenRect project_opening(const PortalSurface &p, const Mat4 &projection, const Mat4 &view,
                           int sw, int sh)
{
    const Vec3 r = p.right();
    const Vec3 u = p.up;
    const Vec3 c = p.face();

    const Vec3 corners[4] = {
        c - r * p.half_w - u * p.half_h,
        c + r * p.half_w - u * p.half_h,
        c + r * p.half_w + u * p.half_h,
        c - r * p.half_w + u * p.half_h,
    };

    const Mat4 vp = mat4_multiply(projection, view);

    float min_x = 1e30f, min_y = 1e30f, max_x = -1e30f, max_y = -1e30f;
    for (const Vec3 &corner : corners) {
        const Vec4 clip = mat4_transform_vec4(vp, Vec4{corner.x, corner.y, corner.z, 1.0f});
        if (clip.w <= 1e-6f) return ScreenRect{};   // behind the camera: give up
        const float nx = clip.x / clip.w;
        const float ny = clip.y / clip.w;
        const float px = (nx * 0.5f + 0.5f) * (float)sw;

        // Flipped, because the two halves of every pixel gate disagreed about
        // which way y runs. Clip space puts +1 at the top; gfx_read_screen
        // hands back rows top-down, so row 0 is the top. Without the flip this
        // returned the rectangle MIRRORED about the middle of the screen.
        //
        // It went unnoticed through the continuity gate, the recursion gate and
        // the cut gate because every opening they look at sits near the vertical
        // centre, where a mirrored rectangle still lands mostly on the hole. A
        // floor-level opening sits well below centre, and there the mirror puts
        // the region on the wall above -- which is how this was found, after
        // four mechanisms in the renderer had been ruled out one at a time for
        // never changing a number that was never measuring them.
        const float py = (1.0f - (ny * 0.5f + 0.5f)) * (float)sh;
        min_x = std::min(min_x, px); max_x = std::max(max_x, px);
        min_y = std::min(min_y, py); max_y = std::max(max_y, py);
    }

    ScreenRect rect;
    rect.x0 = (int)std::ceil(min_x);
    rect.y0 = (int)std::ceil(min_y);
    rect.x1 = (int)std::floor(max_x);
    rect.y1 = (int)std::floor(max_y);
    rect.valid = (rect.x1 > rect.x0 + 8) && (rect.y1 > rect.y0 + 8);
    return rect;
}

// Shrinks the rectangle away from the opening's border. The tunnel walls belong
// to different portals in the two renders, so the outermost band legitimately
// differs and is not what the check is about.
ScreenRect inset(ScreenRect r, float fraction)
{
    const int dx = (int)((float)(r.x1 - r.x0) * fraction);
    const int dy = (int)((float)(r.y1 - r.y0) * fraction);
    return ScreenRect{r.x0 + dx, r.y0 + dy, r.x1 - dx, r.y1 - dy, r.valid};
}

// The opening as the four corners it really projects to, rather than the box
// around them.
//
// A portal looked at head on projects to a rectangle and the box IS the
// opening. Looked at from an angle -- which is the only way to look into a
// floor portal, since you cannot stand level with the ground -- it projects to
// a trapezoid, the far edge shorter than the near one, and the box around it
// takes in a sliver of slab down each side of the far end. That sliver is real
// geometry and it legitimately differs between the two renders, so a gate
// measuring it reports a defect that is not there: 752 pixels of it, in both
// cases with a horizontal source, at a steady 2.9%.
//
// Used as a refinement of the rectangle rather than a replacement: a pixel
// counts when it is inside the inset box AND inside both apertures. For a head
// on view that takes one to two per cent of the border pixels off the region --
// 13284 down to 12960 at six and a half blocks, 61424 to 60726 at three, and
// not one pixel at one and a half -- because the box is inset with an integer
// truncation and the quad is shrunk about its own centroid, and the two round
// the last row differently. Every gate written before this one reports the same
// percentage it did, from a region a border pixel smaller.
struct ScreenQuad {
    float x[4] = {0, 0, 0, 0};
    float y[4] = {0, 0, 0, 0};
    bool  valid = false;
};

ScreenQuad project_quad(const PortalSurface &p, const Mat4 &projection, const Mat4 &view, int sw,
                        int sh)
{
    const Vec3 r = p.right();
    const Vec3 u = p.up;
    const Vec3 c = p.face();

    const Vec3 corners[4] = {
        c - r * p.half_w - u * p.half_h,
        c + r * p.half_w - u * p.half_h,
        c + r * p.half_w + u * p.half_h,
        c - r * p.half_w + u * p.half_h,
    };

    const Mat4 vp = mat4_multiply(projection, view);

    ScreenQuad q;
    for (int i = 0; i < 4; ++i) {
        const Vec4 clip = mat4_transform_vec4(vp, Vec4{corners[i].x, corners[i].y, corners[i].z,
                                                       1.0f});
        if (clip.w <= 1e-6f) return ScreenQuad{};
        q.x[i] = (clip.x / clip.w * 0.5f + 0.5f) * (float)sw;
        q.y[i] = (1.0f - (clip.y / clip.w * 0.5f + 0.5f)) * (float)sh;   // rows top down
    }
    q.valid = true;
    return q;
}

// Toward the centroid. `keep` is the fraction of the original extent left, so
// 0.68 removes sixteen per cent from each side -- the same band inset() takes
// off a rectangle.
ScreenQuad shrink_quad(ScreenQuad q, float keep)
{
    float cx = 0.0f, cy = 0.0f;
    for (int i = 0; i < 4; ++i) { cx += q.x[i]; cy += q.y[i]; }
    cx *= 0.25f;
    cy *= 0.25f;
    for (int i = 0; i < 4; ++i) {
        q.x[i] = cx + (q.x[i] - cx) * keep;
        q.y[i] = cy + (q.y[i] - cy) * keep;
    }
    return q;
}

// Convex, so a point is inside when it is on the same side of all four edges.
bool inside_quad(const ScreenQuad &q, float px, float py)
{
    if (!q.valid) return false;
    int positive = 0, negative = 0;
    for (int i = 0; i < 4; ++i) {
        const int   j    = (i + 1) & 3;
        const float side = (q.x[j] - q.x[i]) * (py - q.y[i]) - (q.y[j] - q.y[i]) * (px - q.x[i]);
        if (side > 0.0f) ++positive;
        if (side < 0.0f) ++negative;
    }
    return positive == 0 || negative == 0;
}

// Mesh everything and keep going until there is nothing left to mesh.
//
// One sync() is a BUDGET, not a promise: it uploads up to a limit and returns.
// A gate that edits the world, syncs once and photographs the result is
// photographing however much of the rebuild fitted in that one call, and the
// second photograph -- taken later -- gets more of it. That is a difference
// between two images with nothing to do with portals, and it showed up as one
// run of the half-offset case reporting 100% of pixels differing and every run
// after it reporting none.
void settle_world(Scene &s)
{
    for (int i = 0; i < 64; ++i) {
        s.act().world_render.sync(s.act().world, s.act().registry, 100000);
        if (s.act().world_render.last_meshed() == 0) return;
    }
    std::printf("        WARNING: the world was still meshing after 64 sync passes\n");
}

// How many chunks the world inside an opening is allowed to mesh per frame.
//
// A quarter of the active world's budget, which is DESIGN.md 4.2's "low
// priority mesh tick" and the first part of the passive budget this engine
// actually spends. The number is a fraction rather than a measurement: what
// matters is that the passive world meshes SLOWER than the one being walked
// in, not that it meshes at exactly this rate.
inline constexpr int kPassiveMeshBudget = 1;

// One meshing tick for every world on screen, which is both of them.
//
// The frame loop used to sync only the active session, and that is a whole
// world's geometry never being built: the opening showed its sky and nothing
// else until the player crossed once and the far world was promoted. Reported
// from play, in those words -- "prima del primo passaggio vedo solo il cielo".
//
// The gate above it could not see it because run_dual settles both worlds
// before measuring anything. A gate that builds its own scene measures the
// scene it built.
void sync_sessions(Scene &s)
{
    // A slice of the frame now that it buys more than one chunk.
    //
    // It could not before: with the build still on this thread a chunk cost
    // 3.13 ms of a 4 ms budget, so the slice bought exactly one -- the same as
    // the passive world's floor -- and the world being walked in meshed no
    // faster than the world behind the opening. `dual_session` said so in the
    // words it was written with: "the active world meshed 1 chunks, the
    // passive 1". With the build on a worker this call pays 0.32 ms of snapshot
    // and an upload per chunk, so the counts are what tell the two worlds apart
    // again and the budget is what holds the frame.
    //
    // SHARED and not one each: two worlds are being meshed and there is one
    // sixtieth of a second between them. The passive one gets what the active
    // one did not spend, never less than a sliver -- zero would mean "no limit"
    // to sync(), which is the opposite of what is meant here.
    //
    // Said plainly: the sharing CHANGES NOTHING TODAY, and that was measured
    // rather than assumed. Handing the far world a whole budget of its own
    // moved the worst tick from 2.14 ms to 2.19 -- its one-chunk count is what
    // binds it, not the time. It stays because the bound it expresses is the
    // true one (a frame has one budget, not one per world) and because
    // kPassiveMeshBudget will not be one for ever; `dual_session` measures the
    // total of the two so that the day it stops being true is a red line and
    // not a slower game.
    s.act().world_render.sync(s.act().world, s.act().registry, 4, kMeshBudgetMs);
    double left = kMeshBudgetMs - s.act().world_render.last_sync_ms();
    if (s.act().links_elsewhere() && !s.act().many_mouths()) {
        Session &shown = s.linked();
        shown.world_render.sync(shown.world, shown.registry, kPassiveMeshBudget,
                                left > 0.05 ? left : 0.05);
        return;
    }

    // And the worlds behind every mouth drawn, out of the same frame. Missed
    // once: a sandbox's door to the track drew only sky, because nothing here
    // had ever meshed a world that link_session did not name -- measured, 0
    // vertices with the portal pass running for it. A mouth not drawn
    // (kMouthsShown) is not meshed either until it is.
    int done[net::kMaxMouths], n = 0;
    for (int m = 0; m < net::kMaxMouths; ++m) {
        const int w = mouth_world(s.act(), m);
        if (w < 0 || w >= s.count() || std::find(done, done + n, w) != done + n ||
            !mouth_shown(s.act(), m, s.cam_eye))
            continue;
        done[n++]      = w;
        Session &shown = s.at(w);
        shown.world_render.sync(shown.world, shown.registry, kPassiveMeshBudget,
                                left > 0.05 ? left : 0.05);
        left -= shown.world_render.last_sync_ms();
    }
}

// One continuity measurement: what the portal shows, against what is really
// there, from one viewpoint. Returns the percentage of differing pixels inside
// the opening, or a negative number if the measurement could not be made.
bool g_verify_shots = false;

// A client that does not open passive sessions at all.
//
// Not a debugging switch: it is one of the three reasons DESIGN.md's redirect
// fallback exists, and the only one that can be arranged on purpose. With it a
// mouth has no window behind it, so walking into one has to be answered by the
// server's Redirect or not at all -- which is exactly the path the fallback is.
bool g_no_passive = false;

// A player who does not stop, for a run measured in half-hours rather than in
// seconds. See the Roam state in run_network: M7's exit test is thirty minutes
// from the start without a crash and without touching the console, and a client
// standing still for thirty minutes tests a socket and a leak rather than a
// game.
//
// Its own flag rather than "a long --play-seconds means this": every other gate
// here plays for a few seconds and expects the player to stay where the
// scenario put them, and a walk that switched itself on past some duration
// would be a rule nobody wrote.
bool g_roam = false;

// --soak-far: blocks a second the soak flies in a straight line, instead of
// circling the box it starts in.
double g_soak_far = 0.0;

// --cross-back S: after the scripted walk has crossed into the world behind a
// mouth, wait S seconds there and walk into THAT world's mouth too. The way
// back is the other half of DESIGN.md M13's test, and without it a run
// measures a door that only opens one way. Negative is off.
double g_cross_back = -1.0;

// --mouths "0,1": which of the joining world's mouths the scripted walk goes
// into, one per visit -- the first, then (with --cross-back) the second after
// coming home. A sandbox with a door to an arena and one to a track is walked
// through both in one run. The world walked into has one way home, its first.
// "none" walks into no mouth at all.
std::vector<int> g_walk_mouths{0};

// --walk-path "x,z;x,z;...": in a timed run, walk through these points in order
// and then stand. In the world the run crossed into when it crosses, and after
// the hands otherwise. What a lap of a track is to a run: the same turn and
// press forward as the walk into a mouth, towards one point after another.
std::vector<Vec3> g_walk_path;

// --use-at "x,y,z": before --walk-path, walk up to that block and right click
// it once, as a player gets into a parked kart. Then wait a moment for what the
// click changes -- a body that drives from now on -- before the first point.
bool g_use_at = false;
int  g_use_x = 0, g_use_y = 0, g_use_z = 0;

// --fly-down N: in a timed single-process run, fly N nodes up and come back
// down to the ground with nobody at the keyboard, then say what the landing
// cost. The server's rule -- a flight down is not a fall -- has a copy in this
// loop, and a copy nothing runs is a copy that rots. --walked-down lets the
// flight go at the top instead, which IS a fall and has to hurt.
float g_fly_down   = 0.0f;
bool  g_fly_flown  = true;

// --shot S: one screenshot, S seconds into a timed run, into bench/. For
// showing what is actually on the screen at a moment a script arranged -- a
// panel a mod put up, a world a mod built -- instead of describing it.
double g_shot_at = -1.0;

// --press-r-at S: R pressed once, S seconds into a run, the way a player would:
// told to the mods as core:input "key_r". A timed run reads no keyboard, and
// what R does on the track (back to the last line crossed) has no other way to
// be driven by a script. Networked loop only.
double g_press_r_at = -1.0;



// Whether this run is aimed at a world that has said it has no room.
//
// A statement about how the far server was started -- `--max-players 0` -- and
// the only place that fact lives. With it the gate asserts that the mouth was
// never walked through rather than reporting whichever way it went.
bool g_far_world_full = false;

// How long the frame loop may stop moving before this process writes its own
// stack and gives up. Off by default; TODO.md OPEN-6 is what it is for.
double g_watchdog_seconds = 0.0;

// Whether this run is DESIGN.md's "due server con game diversi": the worlds
// being looked into are expected to have block tables of their own, different
// from the one being stood in. A statement about how the servers were started,
// which is the only place that fact lives -- and with it the gate asserts the
// difference instead of reporting it.
bool g_different_games = false;

// Where the gates read their pixels from.
//
// Zero means the window, which is what interactive play uses and what every
// gate used until a test failed once in a dozen runs on an arbitrary check and
// would not do it again. glReadPixels on the default framebuffer is subject to
// the pixel ownership test: anything the desktop composites over this window --
// a notification, a tooltip, another window being dragged across it -- makes
// those pixels undefined. Rendering the same frame into an offscreen target
// removes the rule rather than reducing the odds.
unsigned char *read_frame_pixels(int *w, int *h)
{
    return g_capture_rt.id ? gfx_read_target(g_capture_rt, w, h) : gfx_read_screen(w, h);
}

// The measurement itself, for any pair from any viewpoint. `expected_pos`,
// `expected_fwd` and `expected_up` are where a REAL camera has to stand and how
// it has to be held to see what the portal claims to show, and they are the
// caller's job to derive by hand: derived with portal_virtual_camera they would
// move with the function under test and the check would pass however wrong it
// was.
//
// The up is a parameter and not a constant, and that is the whole reason a
// portal pair with a rolled or horizontal end can be measured at all. It used
// to be world up on both sides, which is correct for every pair whose two
// openings are upright -- the half turn about the destination's up carries
// world up to world up -- and quietly meaningless for any other. Worse, `v.up`
// was never compared to anything, so the one component of the virtual camera
// that an orientation actually changes was the one nothing looked at.
//
// Split in two on purpose. This half takes a reference viewpoint and asks how
// far the portal's image is from it, with no opinion about whether that
// viewpoint is the right one; continuity_between below adds the assertion that
// it is. The separation is what lets a caller ask the OPPOSITE question --
// point a reference somewhere deliberately wrong and demand a big number --
// which is the only way to show that a measurement reading zero was reading
// anything at all. An opening full of flat sky agrees with every camera pose
// there is.
double compare_through_opening(Scene &s, const PortalLink &link, Vec3 eye, Vec3 fwd,
                               Vec3 ref_pos, Vec3 ref_fwd, Vec3 ref_up,
                               const char *shot_prefix, long *out_total, long *out_diff)
{
    const Vec3  up     = Vec3{0.0f, 1.0f, 0.0f};
    const float aspect = (float)gfx_screen_width() / (float)gfx_screen_height();

    const Mat4 projection = mat4_perspective(g_fov_y, aspect, kZNear, kZFar);
    const Mat4 view       = mat4_look_to(eye, fwd, up);

    // The opening is a hole through a wall a block thick, so what can actually
    // be seen through it is the intersection of its two mouths, not the
    // mid-plane rectangle the portal surface describes. Six blocks out the
    // difference is a few pixels and the mid-plane rectangle is fine; at one
    // block it is most of the hole, and comparing there measures the inside of
    // the tunnel against the portal image and calls the difference a bug.
    PortalSurface near_mouth = link.src;
    PortalSurface far_mouth  = link.src;
    near_mouth.center = near_mouth.center + near_mouth.normal * (0.5f * kNodeSize);
    far_mouth.center  = far_mouth.center  - far_mouth.normal  * (0.5f * kNodeSize);

    const ScreenRect near_rect = project_opening(near_mouth, projection, view,
                                                 gfx_screen_width(), gfx_screen_height());
    const ScreenRect far_rect  = project_opening(far_mouth, projection, view,
                                                 gfx_screen_width(), gfx_screen_height());
    if (!near_rect.valid || !far_rect.valid) return -1.0;

    ScreenRect opening;
    opening.x0 = std::max(near_rect.x0, far_rect.x0);
    opening.y0 = std::max(near_rect.y0, far_rect.y0);
    opening.x1 = std::min(near_rect.x1, far_rect.x1);
    opening.y1 = std::min(near_rect.y1, far_rect.y1);
    opening.valid = opening.x1 > opening.x0 && opening.y1 > opening.y0;
    if (!opening.valid) return -1.0;

    const ScreenRect roi = inset(opening, 0.16f);

    // And the same band taken off the aperture's real outline, which is what
    // the rectangle is only an approximation of once the view is oblique.
    const ScreenQuad near_quad = shrink_quad(project_quad(near_mouth, projection, view,
                                                          gfx_screen_width(),
                                                          gfx_screen_height()), 0.68f);
    const ScreenQuad far_quad  = shrink_quad(project_quad(far_mouth, projection, view,
                                                          gfx_screen_width(),
                                                          gfx_screen_height()), 0.68f);
    if (!near_quad.valid || !far_quad.valid) return -1.0;

    // Render the portal view and keep the pixels.
    for (int i = 0; i < 20; ++i) render_frame(s, eye, fwd, up, false, nullptr, true);
    if (g_verify_shots && shot_prefix) {
        char name[96];
        std::snprintf(name, sizeof(name), "%s_a_through.png", shot_prefix);
        shot_write(name);
    }
    int w = 0, h = 0;
    unsigned char *through = read_frame_pixels(&w, &h);

    // Reference: a real camera at the given viewpoint, no portals drawn.
    for (int i = 0; i < 20; ++i)
        render_frame(s, ref_pos, ref_fwd, ref_up, false, nullptr, false);
    if (g_verify_shots && shot_prefix) {
        char name[96];
        std::snprintf(name, sizeof(name), "%s_b_reference.png", shot_prefix);
        shot_write(name);
    }
    int rw = 0, rh = 0;
    unsigned char *reference = read_frame_pixels(&rw, &rh);

    if (!through || !reference || w != rw || h != rh) {
        gfx_free_pixels(through);
        gfx_free_pixels(reference);
        return -1.0;
    }

    // A pixel counts as different only if a channel moves by more than this.
    // Both images come from the same shader on the same geometry, so the only
    // legitimate spread is rasterisation on a shared edge.
    constexpr int kChannelTolerance = 8;

    long total = 0, differing = 0;
    for (int y = roi.y0; y < roi.y1; ++y) {
        // Straight through. project_opening returns rows in the readback's own
        // convention now -- row zero at the top -- so the mirror that used to
        // live here is not a conversion any more, it is a second flip. It
        // survived the fix that put the flip upstream and it is worth naming:
        // for a while this gate was applying the correction twice, which is the
        // same defect it was written to catch, wearing the fix as a disguise.
        const int row = y;
        for (int x = roi.x0; x < roi.x1; ++x) {
            if (x < 0 || row < 0 || x >= w || row >= h) continue;
            const float px = (float)x + 0.5f, py = (float)row + 0.5f;
            if (!inside_quad(near_quad, px, py) || !inside_quad(far_quad, px, py)) continue;
            const size_t i = ((size_t)row * (size_t)w + (size_t)x) * 4u;
            const int dr = std::abs((int)through[i + 0] - (int)reference[i + 0]);
            const int dg = std::abs((int)through[i + 1] - (int)reference[i + 1]);
            const int db = std::abs((int)through[i + 2] - (int)reference[i + 2]);
            ++total;
            if (dr > kChannelTolerance || dg > kChannelTolerance || db > kChannelTolerance)
                ++differing;
        }
    }

    gfx_free_pixels(through);
    gfx_free_pixels(reference);

    if (total == 0) return -1.0;
    if (out_total) *out_total = total;
    if (out_diff)  *out_diff  = differing;
    return 100.0 * (double)differing / (double)total;
}

// The assertion and the measurement together: the virtual camera has to BE the
// hand-derived one before its image is worth comparing to anything.
double continuity_between(Scene &s, const PortalLink &link, Vec3 eye, Vec3 fwd,
                          Vec3 expected_pos, Vec3 expected_fwd, Vec3 expected_up,
                          const char *shot_prefix, long *out_total, long *out_diff)
{
    const Vec3          up = Vec3{0.0f, 1.0f, 0.0f};
    const VirtualCamera v  = portal_virtual_camera(link.src, link.dst, eye, fwd, up);

    if (length(v.pos - expected_pos) > 1e-3f || length(v.forward - expected_fwd) > 1e-3f ||
        length(v.up - normalize(expected_up)) > 1e-3f) {
        std::printf("            virtual camera wrong: pos %.3f %.3f %.3f  fwd %.3f %.3f %.3f  "
                    "up %.3f %.3f %.3f\n",
                    v.pos.x, v.pos.y, v.pos.z, v.forward.x, v.forward.y, v.forward.z, v.up.x,
                    v.up.y, v.up.z);
        return -1.0;
    }

    return compare_through_opening(s, link, eye, fwd, expected_pos, expected_fwd, expected_up,
                                   shot_prefix, out_total, out_diff);
}

// A hand-derived reference viewpoint, moved by config/portal.cfg's viewpoint
// offset.
//
// Every reference in this file is worked out the same way: the two bases and a
// half turn, which gives the point the map lands on when the destination end of
// it is the face the player emerges from. `view` pushes that end further out
// along the destination's normal -- see portal_view_origin -- so every one of
// those points moves with it, by one setting, along an axis each derivation has
// already named.
//
// This does not make the references tautological. The offset is a number read
// from a file, not portal_transform's answer, so a map that is wrong in any
// other way still misses the point by the amount it is wrong.
Vec3 with_view_offset(Vec3 hand_derived, const PortalSurface &dst)
{
    return hand_derived + dst.normal * portal_tuning().view_depth();
}

// The M0 spike pair, from `eye_z` on its axis. The reference viewpoint is worked
// out by hand for this fixed scene: portal A sits at (40.5, 22, 40.5) facing +Z
// with up +Y, so its right axis is cross(up, normal) = +X, and an eye d in front
// of A's PLANE is at A-local (right 0, up -0.38, normal +d). A half turn about
// up negates right and normal. Portal B sits at (79.5, 22, 60.5) facing +X, so
// the offset comes back out d behind B's plane, and the forward direction
// (0, 0, -1) arrives as (+1, 0, 0).
//
// In front of A's IN plane and behind B's OUT plane, which is what makes this
// line survive config/portal.cfg's `plane` unchanged: A's in-plane is at
// z = 40.5 + p and B's out-plane at x = 79.5 - p, so d = eye_z - (40.5 + p) and
// the answer is (79.5 - p) - d = 120 - eye_z. The two offsets cancel. A first
// version glued in-plane to in-plane, needed a 2p term here, and put the
// destination's whole wall inside the image.
//
// Still derived by hand: a setting and a half turn, not the function under
// test. A reference computed with portal_virtual_camera would move with
// whatever it was supposed to be checking.
double continuity_at(Scene &s, float eye_z, long *out_total, long *out_diff)
{
    char prefix[64];
    std::snprintf(prefix, sizeof(prefix), "verify_%.1f", (double)(eye_z - 40.5f));
    return continuity_between(s, s.act().link_ab,
                              Vec3{40.5f, kGroundTop + kPlayerEyeHeight, eye_z},
                              dir_from_angles(0.0f, 0.0f),
                              with_view_offset(Vec3{120.0f - eye_z, 21.62f, 60.5f},
                                               s.act().link_ab.dst),
                              Vec3{1.0f, 0.0f, 0.0f},
                              Vec3{0.0f, 1.0f, 0.0f}, prefix, out_total, out_diff);
}

// ------------------------------------------------------- two worlds at once
//
// M5's shape, from DESIGN.md 4.1: two Sessions in one process, one active and
// one passive rendered inside the portal, and a swap that is `active = i` plus
// a handful of explicit transitions.
//
// The budget comes from 4.2, where the fork already measured it: rendering
// world B costs 3-7% of the frame, so a passive session is affordable at a
// reduced view range and is not the performance problem. A third, as written
// there.
inline constexpr float kActiveViewRange  = 512.0f;
inline constexpr float kPassiveViewRange = 512.0f / 3.0f;

// What a frame of the single-process loop may spend on columns (TODO.md
// OPEN-35). A step is taken whatever this says, so the ground ahead of a player
// is never left for later; this is only how many more fit.
inline constexpr double kLocalColumnBudgetMs = 4.0;

// The world around a point, when this process holds a world with no edge
// (TODO.md OPEN-35, OPEN-38): the radius a server streams, a few milliseconds of
// the frame and at least one step, and what was left behind let go of after
// thirty seconds. Called once a frame by whatever moves the eye.
void step_columns(Session &here, float x, float z)
{
    if (!here.keeper) return;

    here.column_clock = (uint64_t)(plat_time() * 1000.0);
    const net::InterestConfig    reach;
    const std::vector<ColumnPos> centre{column_of_cell((int)std::floor(x), (int)std::floor(z))};
    std::vector<ColumnPos>       want    = columns_near(centre, reach.radius);
    const std::vector<ColumnPos> written = here.keeper->pending_columns();
    want.insert(want.end(), written.begin(), written.end());
    here.keeper->advance(want, kLocalColumnBudgetMs, here.column_clock);

    if (here.column_clock >= here.next_unload_at) {
        here.next_unload_at = here.column_clock + 1000;
        here.keeper->unload_idle(centre, reach.radius + reach.forget_margin + 2, 30000,
                                 here.column_clock);
    }
}

// Builds one local world into a session: its own block table, its own terrain,
// its own opening.
//
// `seed` and `sky` differ per world on purpose. Two worlds that looked alike
// would make every check below pass for the wrong reason -- "the portal shows
// the other world" is only measurable when the two do not match.
// `when` and not a sky: the clock owns the sky now, and a sky handed in beside
// a time would be a fourth place for the two to disagree. Two worlds still get
// two skies -- they get them by being at two different hours, which is what a
// player crossing between them is supposed to see.
void build_local_session(Session &sess, int link_to, uint32_t seed, const PortalSurface &here,
                         const PortalSurface &there, double when, float view_range,
                         int landmark_height, BlockId &out_frame)
{
    register_client_core_nodes(sess.registry);
    out_frame = sess.registry.find("core:frame");

    Mapgen mapgen;
    mapgen.add_stage("session:opening", [&](MapgenContext &ctx) {
        build_slab_with_opening(ctx,here, 3, 3, out_frame);

        // A landmark in front of this world's own opening, and off to one
        // side, so that what comes THROUGH the other world's portal is
        // recognisably this world from close up and not only by its distant
        // hills.
        //
        // IN FRONT, because the virtual camera stands behind the opening and
        // looks out through it: a landmark behind it is behind the camera, and
        // the first version put it there and changed the measurement by
        // nothing at all. Off to the side so it is not standing in the doorway.
        //
        // It has to be built rather than left to the terrain: the generator's
        // plaza is flat by construction over x,z in [20,100], so inside it two
        // seeds produce identical ground.
        const Vec3 mark = here.center + here.normal * 6.0f + here.right() * 3.0f;
        build_landmark(ctx,(int)mark.x, kGroundTop, (int)mark.z, out_frame,
                       landmark_height);

        // A single step where somebody arriving from the other world lands, in
        // THIS world and not the other one.
        //
        // It is what makes "the arrival was resolved against the destination's
        // blocks" measurable. Without it both worlds are flat plaza at the exit
        // and checking against the wrong one gives the same answer, which is
        // exactly what happened: the sabotage that passes the departing world's
        // blocks to portal_traverse left every check green.
    });
    mapgen.generate(sess.world, sess.registry, seed, g_world_extent, kWorldHeight);

    // Kept for the mods, which load later and after this Mapgen is gone. See
    // Session::mapgen_biomes for why the biomes are copied rather than pointed
    // at.
    sess.mapgen_biomes = mapgen.biomes();
    sess.mapgen_ctx.reset(new MapgenContext(mapgen_make_context(
        sess.world, sess.registry, sess.mapgen_biomes, seed, kWorldHeight)));

    compute_lighting(sess.world, sess.registry, 0, 0, 0, g_world_extent - 1, kWorldHeight - 1,
                     g_world_extent - 1);
    sess.world.mark_all_dirty();

    // One opening, leading to the other world. link_ba is left alone and
    // link_count says so: a second link would be a second hole in a wall that
    // has one.
    sess.link_ab    = PortalLink{here, there,
                                 gfx_rt_create(portal_rt_dim(kWindowWidth),
                                               portal_rt_dim(kWindowHeight), true)};
    sess.link_count = 1;
    sess.portal_a   = portal_from_surface(here);
    sess.portal_b   = portal_from_surface(there);
    sess.link_session = link_to;

    sess.time_of_day = when;
    sky_at(when, sess.sky);
    sess.view_range = view_range;
}

// The swap, and every transition DESIGN.md 4.3 lists as already paid for.
//
// Not one of these is a nicety. Each is a bug the fork shipped and then fixed:
// a sky taken from a replay that is empty the second time round, an avatar
// drawn twice or not at all, a view range that belonged to whoever was active
// when it was set.
void swap_sessions(Scene &s)
{
    // Demote: stop drawing the player's avatar here, and drop to the passive
    // budget. Done BEFORE the index moves, so it lands on the session being
    // demoted rather than the one being promoted.
    s.act().draws_own_avatar = false;
    s.act().view_range       = kPassiveViewRange;

    // Where to. A session with no link has nowhere to swap to, and going there
    // anyway used to be impossible to write: `1 - active` always named a
    // session. With an index it names -1, so the question has to be asked.
    const int target = s.act().link_session;
    if (target < 0 || target >= s.count()) {
        std::printf("portal: this world's openings lead nowhere else, not swapping\n");
        s.act().draws_own_avatar = true;
        s.act().view_range       = kActiveViewRange;
        return;
    }

    s.active = target;

    // Promote. The sky is not touched: it is already this session's own, held
    // live in the struct rather than replayed from events, which is the
    // invariant the fork learned the hard way -- a server sends set_sky once,
    // so a replay is empty on the second trip.
    s.act().draws_own_avatar = true;
    s.act().view_range       = kActiveViewRange;
}

// ------------------------------------------------ portals that are not upright
//
// Every portal this engine had ever rendered, in every gate up to here, had its
// up along +Y and its normal along one of the four horizontal directions. That
// is one orientation out of twenty four, and the code paths that the other
// twenty three would have taken were reachable, plausible and unmeasured.
//
// What that hid, found by writing this:
//
//   - the slab builder could only build a WALL. It picked between two block
//     index formulas by asking whether the right vector pointed along x, a
//     question that only means anything when up is +Y, so a floor portal got a
//     wall built through it edge on.
//   - the body's Portal was derived from the camera's PortalSurface with its
//     rotation left at zero. Right for an upright opening and wrong for every
//     other, and the two representations are supposed to be one opening.
//   - the continuity gate held its reference camera with world up and never
//     compared the virtual camera's up to anything. The single quantity an
//     orientation changes was the single quantity nothing looked at.
//
// Each case below is measured against a viewpoint derived by hand from the two
// bases, and then measured AGAIN against a deliberately wrong one. The second
// measurement is not decoration: an opening showing flat sky agrees with every
// camera pose there is, and a gate reading zero per cent there would be reading
// nothing at all.
// Defined further down, with the crossing it belongs to. Declared here because
// case 6 drives the roll animation through the real client path rather than
// through a copy of the decay: a copy would keep passing after the client's own
// stopped working, which is the failure this whole file is written against.
bool step_portals(Scene &s, Player &player, float dt);

// The horizon a crossing tipped, back to level at half a turn a second: the
// Portal-puzzle landing yaportal does with roll_targets. Both loops, local and
// connected: the connected one set the roll from the Teleport and never took it
// back, so a quarter-turned portal left the view and the mouse on their side for
// good -- reported from play.
void settle_roll(Scene &s, float dt)
{
    const float step = 3.14159265f * dt;
    s.roll = (std::fabs(s.roll) <= step) ? 0.0f : s.roll - (s.roll > 0.0f ? step : -step);
}

// A crossing out of a standing opening taken looking steeply up or down turns
// the view out of the opening, at the roll's half a turn a second. A fall into
// a floor portal taken looking ahead came out of a wall looking straight up and
// stayed there; now it ends looking out along the wall's normal, level. Out of
// a floor or a ceiling the look is left alone -- straight up is where a body
// shot out of a floor is going -- and so is anything within 45 degrees of
// level, which is the player's own aim carried through. The mouse ends it.
//
// The opening is the one nearest where the body now stands: every loop has
// the openings, and not every crossing says which one it came out of.
// A crossing of a pair in this world, which the server decides: on a move this
// client sent a round trip ago, and with the exit at the depth the settings
// name. The client has gone on running since, so taking the server's place as
// it stands throws the view back by what it ran meanwhile -- a small jolt at
// every crossing, larger the faster (reported from play, 2026-10-05). Where
// the opening's image had the eye now is the same map applied to the eye as it
// is (portal_shown_depth: the map's depth, plus `view`); the body goes there,
// and looks where the map sends its own look, when that is a place a body fits
// and not far from the server's. The server's grace window takes the rest.
void ahead_of_crossing(Scene &s, Player &player, Vec3 eye_before, Vec3 look_before)
{
    const Session &w    = s.act();
    int            best = -1;
    float          near = 0.0f;
    for (size_t k = 0; k < w.linked_openings.size() && k < w.linked_to.size(); ++k) {
        const float d = length_sq(w.linked_openings[k].center - eye_before);
        if (best < 0 || d < near) best = (int)k, near = d;
    }
    if (best < 0) return;
    const Portal &src = w.linked_openings[(size_t)best], &dst = w.linked_to[(size_t)best];
    const Vec3 eye  = portal_transform_pos(src, dst, eye_before) + dst.normal() * portal_tuning().view_depth();
    const Vec3 feet = eye - Vec3{0.0f, player.eye_height, 0.0f};
    if (length(feet - player.pos) > 0.75f) return;
    if (box_hits_world(w.world, w.registry, body_box(feet, w.movement.width * 0.5f))) return;
    std::printf("portal: %.2f ahead of where the server put us\n", (double)length(feet - player.pos));
    player.pos = feet;
    const Vec3 look = normalize(portal_transform_dir(src, dst, look_before));
    player.yaw      = std::atan2(look.x, -look.z);
    player.pitch    = std::asin(std::clamp(look.y, -1.0f, 1.0f));
}

void aim_after_crossing(Scene &s, const Player &player)
{
    const Session &w    = s.act();
    const Portal  *best = nullptr;
    float          near = 0.0f;
    auto consider = [&](const Portal &p) {
        const float d = length_sq(p.center - player.pos);
        if (!best || d < near) best = &p, near = d;
    };
    if (!w.linked_openings.empty()) {
        for (const Portal &p : w.linked_openings) consider(p);
    } else {
        consider(w.portal_a);
        consider(w.portal_b);
    }
    if (!best) return;

    const Vec3 n     = best->normal();
    // Not a body that walked in backwards: it comes out looking back at the
    // opening (into the wall), which a fall never does -- looking ahead it
    // comes out along the wall, looking down into the floor out of it. Turning
    // that one round to face away was reported from play (2026-10-05).
    s.look_settling  = std::fabs(n.y) < 0.5f && std::fabs(player.forward().y) > 0.7071f &&
                       dot(player.forward(), n) > -0.05f;
    s.look_target    = normalize(Vec3{n.x, 0.0f, n.z});
    s.look_since     = plat_time();
    if (s.look_settling)
        std::printf("portal: came out of a wall looking %.0f degrees %s; turning out of it\n",
                    (double)std::fabs(player.pitch) * 57.2958, player.pitch > 0.0f ? "up" : "down");
}

void settle_look(Scene &s, Player &player, float dt)
{
    if (!s.look_settling) return;

    // A kart's heading is its chassis's, which the server drives too
    // (TODO.md OPEN-47): only the camera's tilt settles, back to level.
    if (s.act().movement.drives()) {
        const float step = 3.14159265f * dt;
        if (std::fabs(player.pitch) > step) {
            player.pitch -= std::copysign(step, player.pitch);
            return;
        }
        player.pitch    = 0.0f;
        s.look_settling = false;
        return;
    }

    const Vec3  f     = player.forward();
    const float angle = std::acos(std::clamp(dot(f, s.look_target), -1.0f, 1.0f));
    const float step  = 3.14159265f * dt;
    Vec3        d     = s.look_target;
    const Vec3  axis  = cross(f, s.look_target);
    if (angle > step && length_sq(axis) > 1e-8f) {
        const Vec3 k = normalize(axis);   // f turned about k, towards the target
        d = f * std::cos(step) + cross(k, f) * std::sin(step);
    } else {
        s.look_settling = false;
        std::printf("portal: looking out of the opening, level, %.2f s after the crossing\n",
                    plat_time() - s.look_since);
    }
    player.yaw   = std::atan2(d.x, -d.z);
    player.pitch = std::asin(std::clamp(d.y, -1.0f, 1.0f));
}

// ---------------------------------------------------- M5: two worlds, one process
//
// DESIGN.md 8 asks M5 to prove: two Sessions in one process, one active and one
// passive rendered inside the portal, swapped in place with the invariants of
// 4.3 -- and its test is "go in and out ten times in a row; sky, held keys, own
// avatar and view range correct on every lap". Those four are named there as
// the four historic bugs of the fork's dual client, and every one of them is a
// scar from state that existed exactly once.
//
// Ten laps rather than one because all four are bugs that appear on the SECOND
// trip: a sky replayed from events is empty the second time a server sends it,
// an avatar flag toggled instead of assigned is right every other lap, and a
// view range written by whoever was active last is wrong exactly when you come
// back.
// The two-world scene, built once and used by both the gate and the bench.
//
// Shared rather than written twice on purpose: a benchmark that builds its own
// approximation of the scene measures its own approximation. The seeds, the
// skies, the view ranges and the openings are the ones dual_session asserts
// against, so the cost printed is the cost of the thing that was checked.
struct DualScene {
    PortalSurface oa, ob;
    // Two worlds, two clocks, and that is where the two skies come from. They
    // used to be two literals sitting next to a comment saying "an orange one";
    // now world B is simply at dusk, and the gate that tells the worlds apart
    // by their sky is telling them apart by something the player can also read
    // off the screen.
    double        time_a = kNoon;
    double        time_b = kDusk;
    float         sky_a[4] = {0.0f, 0.0f, 0.0f, 1.0f};
    float         sky_b[4] = {0.0f, 0.0f, 0.0f, 1.0f};
    BlockId       frame_a = 0, frame_b = 0;
};

// Returns false when a renderer would not start, which is the one failure here
// that is not the caller's business to diagnose.
bool build_dual_scene(Scene &s, DualScene &d)
{
    d.oa.center = Vec3{40.5f, (float)kGroundTop + 1.0f, 44.5f};
    d.oa.normal = Vec3{0.0f, 0.0f, 1.0f};
    d.oa.up     = Vec3{0.0f, 1.0f, 0.0f};

    d.ob.center = Vec3{70.5f, (float)kGroundTop + 1.0f, 24.5f};
    d.ob.normal = Vec3{1.0f, 0.0f, 0.0f};
    d.ob.up     = Vec3{0.0f, 1.0f, 0.0f};

    // Session 0 already exists and is the one main() built; this scene wants
    // two worlds of its own shape, so both are rebuilt here.
    s.reset(2);

    sky_at(d.time_a, d.sky_a);
    sky_at(d.time_b, d.sky_b);
    build_local_session(s.at(0), 1, 1, d.oa, d.ob, d.time_a, kActiveViewRange, 3, d.frame_a);
    build_local_session(s.at(1), 0, 7, d.ob, d.oa, d.time_b, kPassiveViewRange, 8, d.frame_b);

    s.at(0).draws_own_avatar = true;
    s.at(1).draws_own_avatar = false;

    if (!s.at(0).world_render.init(s.at(0).registry, read_texture_file) ||
        !s.at(1).world_render.init(s.at(1).registry, read_texture_file))
        return false;

    wire_model_draw(s, 0);
    wire_model_draw(s, 1);
    return true;
}

// What the second world costs, which DESIGN.md 4.2 asserts and this engine had
// never measured.
//
// The design quotes 3-7% of the frame from the fork's dual-client work and adds
// that "multi-session is not the performance problem". That is a claim about
// somebody else's renderer. This measures ours, on the scene dual_session
// checks, and prints the number whichever way it comes out.
//
// Three passes over the same frame from the same pose, so the differences are
// the only thing that moved:
//
//   both worlds   the portal shows the OTHER session -- the render target pass
//                 draws world B, then the main pass draws world A and the
//                 opening on top of it. This is what a player sees.
//   one world     the portal shows THIS session. The whole portal machinery
//                 still runs -- target, oblique near plane, clip planes, the
//                 quads -- but the geometry it draws is world A's, already in
//                 the cache from the main pass.
//   no portal     `portals` off, so the target pass does not happen at all and
//                 no opening is drawn. World A and nothing else.
//
// So `no portal -> one world` is what the portal pass costs with the geometry
// warm, and `one world -> both worlds` is what the second world adds on top.
// Neither is the whole story on its own, and quoting either alone is how a
// benchmark starts lying.
//
// Timed with vsync off, at a fixed camera, with both worlds fully meshed first:
// a frame that is still building geometry is measuring the mesher.
int run_dual_bench(Scene &s)
{
    DualScene d;
    if (!build_dual_scene(s, d)) {
        std::printf("FAIL: a session's renderer would not start\n");
        return 1;
    }

    // Both worlds whole before anything is timed.
    for (int i = 0; i < 64; ++i) {
        s.at(0).world_render.sync(s.at(0).world, s.at(0).registry, 100000);
        s.at(1).world_render.sync(s.at(1).world, s.at(1).registry, 100000);
        if (s.at(0).world_render.last_meshed() == 0 &&
            s.at(1).world_render.last_meshed() == 0)
            break;
    }

    // Three nodes in front of the opening, level, looking into it: the pose a
    // player walks up to it in, and the one where the opening is worth the most
    // pixels it will ever be worth.
    const Vec3 up  = Vec3{0.0f, 1.0f, 0.0f};
    const Vec3 eye = d.oa.center + d.oa.normal * 3.0f;
    const Vec3 fwd = Vec3{-d.oa.normal.x, -d.oa.normal.y, -d.oa.normal.z};

    constexpr int kWarm    = 120;
    constexpr int kMeasure = 900;

    // Present off and a fence every frame, for the two reasons the soak already
    // knows about: handing the frame to the compositor measures the compositor,
    // and stopping the clock when the driver ACCEPTS the work measures the
    // driver's inbox. Without the fence this scene reads 0.166 ms a frame and
    // the whole portal pass looks like it costs a third of nothing.
    gfx_set_present(false);

    auto time_view = [&](Vec3 e, Vec3 f, bool portals, bool other_world) {
        s.act().portals        = portals;
        s.act().link_session   = other_world ? 1 - s.active : -1;
        for (int i = 0; i < kWarm; ++i) {
            render_frame(s, e, f, up, false, nullptr);
            gfx_finish();
        }
        const double t0 = plat_time();
        for (int i = 0; i < kMeasure; ++i) {
            render_frame(s, e, f, up, false, nullptr);
            gfx_finish();
        }
        return 1000.0 * (plat_time() - t0) / (double)kMeasure;
    };
    auto time_frames = [&](bool portals, bool other_world) {
        return time_view(eye, fwd, portals, other_world);
    };

    // Every pass twice, interleaved, and the smaller reading kept. A frame time
    // has a floor and no ceiling -- whatever else the machine is doing can only
    // add -- so the minimum of repeated runs is the honest estimate and the
    // spread between them is the noise, printed rather than hidden.
    double both = 0.0, one = 0.0, none = 0.0, wide = 0.0;
    double both_hi = 0.0, one_hi = 0.0, none_hi = 0.0, wide_hi = 0.0;

    for (int pass = 0; pass < 2; ++pass) {
        const double b = time_frames(true, true);
        const double o = time_frames(true, false);
        const double n = time_frames(false, false);

        // And what the passive budget's one implemented item buys: the same
        // frame with world B drawn at the ACTIVE view range instead of a third
        // of it. DESIGN.md 4.2 asks for a reduced range and never says what it
        // is worth.
        s.at(1).view_range = kActiveViewRange;
        const double w = time_frames(true, true);
        s.at(1).view_range = kPassiveViewRange;

        if (pass == 0) {
            both = both_hi = b; one = one_hi = o; none = none_hi = n; wide = wide_hi = w;
        } else {
            both = std::min(both, b); both_hi = std::max(both_hi, b);
            one  = std::min(one,  o); one_hi  = std::max(one_hi,  o);
            none = std::min(none, n); none_hi = std::max(none_hi, n);
            wide = std::min(wide, w); wide_hi = std::max(wide_hi, w);
        }
    }

    std::printf("\nwhat the second world costs, %d frames per pass, twice over, at %dx%d, "
                "vsync off, GPU fenced\n", kMeasure, gfx_screen_width(), gfx_screen_height());
    std::printf("  a frame with the portal          %.3f ms  (%.0f FPS)   spread %.3f\n",
                both, 1000.0 / both, both_hi - both);
    std::printf("  a frame with no portal at all    %.3f ms               spread %.3f\n",
                none, none_hi - none);
    std::printf("  with the passive range removed   %.3f ms               spread %.3f\n",
                wide, wide_hi - wide);
    std::printf("  the portal showing THIS world    %.3f ms               spread %.3f\n",
                one, one_hi - one);

    const double portal_cost = both - none;
    const double wide_cost   = wide - none;
    std::printf("  -> the portal costs              %.3f ms, %.1f%% of the frame it is in\n",
                portal_cost, both > 0.0 ? 100.0 * portal_cost / both : 0.0);
    std::printf("     DESIGN.md 4.2 quotes 3-7%% of the frame from the fork. The share is bigger "
                "here because the frame is smaller: %.3f ms without it.\n", none);
    std::printf("  -> the passive view range saves  %.3f ms, %.1f%% of what the portal would "
                "otherwise cost (%.3f ms)\n", wide - both,
                wide_cost > 0.0 ? 100.0 * (wide - both) / wide_cost : 0.0, wide_cost);
    std::printf("  -> which world is in the target is worth %.3f ms: the same machinery pointed "
                "at THIS world costs %.3f against %.3f for the other one at the same range.\n",
                one - wide, one, wide);
    std::printf("  world A %zu vertices, %.2f MB; world B %zu vertices, %.2f MB "
                "(view range %.0f against %.0f)\n",
                s.at(0).world_render.vertex_count(),
                (double)s.at(0).world_render.gpu_bytes() / (1024.0 * 1024.0),
                s.at(1).world_render.vertex_count(),
                (double)s.at(1).world_render.gpu_bytes() / (1024.0 * 1024.0),
                (double)kActiveViewRange, (double)kPassiveViewRange);

    // ------------------------------------------ what a portal costs from behind
    //
    // A one-way portal is not drawn from behind, and nothing is rendered into
    // its target either: the whole world pass for that link is skipped. So the
    // cost of standing with your back to one should be nothing at all, and
    // "should be" is not a measurement.
    {
        const Vec3 behind     = d.oa.center - d.oa.normal * 3.0f;
        const Vec3 look_at_it = d.oa.normal;

        const double from_behind = std::min(time_view(behind, look_at_it, true, true),
                                            time_view(behind, look_at_it, true, true));
        const double no_portal   = std::min(time_view(behind, look_at_it, false, false),
                                            time_view(behind, look_at_it, false, false));

        PortalTuning two_sided = portal_tuning();
        two_sided.one_way      = 0.0f;
        double both_ways       = 0.0;
        {
            ScopedTuning hold(two_sided);
            both_ways = std::min(time_view(behind, look_at_it, true, true),
                                 time_view(behind, look_at_it, true, true));
        }

        std::printf("\n  standing behind the opening, looking at its back\n");
        std::printf("    one way, portal on               %.3f ms\n", from_behind);
        std::printf("    the same frame with no portal    %.3f ms\n", no_portal);
        std::printf("    seen from both sides             %.3f ms\n", both_ways);
        std::printf("    -> a portal you cannot see costs %.3f ms; one you can costs %.3f\n",
                    from_behind - no_portal, both_ways - no_portal);
    }

    // ---------------------------------------- the target at the portal's size
    //
    // DESIGN.md 4.2's last unimplemented budget item, and the only one left
    // that can move a number. The target is the size of the window, and the
    // opening it feeds is a few hundred pixels wide, so most of that fill rate
    // is spent on pixels that never reach the screen. It is not free: the
    // opening samples the target by SCREEN position, so a smaller target is a
    // softer image inside the hole -- which is why this measures both halves in
    // one place. Choosing a default off the cost alone would be choosing with
    // half the numbers.
    {
        auto set_scale = [&](float scale) {
            g_portal_scale = scale;
            const int w = portal_rt_dim(kWindowWidth), h = portal_rt_dim(kWindowHeight);
            for (int i = 0; i < s.count(); ++i) {
                if (s.at(i).link_ab.rt.id) gfx_rt_destroy(s.at(i).link_ab.rt);
                s.at(i).link_ab.rt = gfx_rt_create(w, h, true);
            }
            // The same depth main() was asked for: rebuilding the pool one
            // level shallower would measure a different renderer.
            s.portal_render.init_recursion(2, g_recursion_depth, w, h);
        };

        const float scales[] = {1.0f, 0.75f, 0.5f, 0.35f, 0.25f};
        constexpr int kCount = (int)(sizeof(scales) / sizeof(scales[0]));

        double ms[kCount] = {};
        for (int i = 0; i < kCount; ++i) {
            set_scale(scales[i]);
            ms[i] = std::min(time_frames(true, true), time_frames(true, true));
        }

        // ------------------------------------------------------ what it costs
        //
        // The denominator that makes the percentage mean something: the pixels
        // the opening actually paints, found by diffing the frame against the
        // same frame with no portal in it rather than by projecting a box.
        set_scale(1.0f);
        g_capture_rt = gfx_rt_create(kWindowWidth, kWindowHeight, true);

        auto capture = [&](bool portals) -> unsigned char * {
            s.act().portals = portals;
            for (int i = 0; i < 8; ++i) render_frame(s, eye, fwd, up, false, nullptr, portals);
            int w = 0, h = 0;
            return read_frame_pixels(&w, &h);
        };

        unsigned char *with_portal = capture(true);
        unsigned char *no_portal   = capture(false);
        s.act().portals            = true;

        const long pixels = (long)kWindowWidth * kWindowHeight;
        std::vector<unsigned char> mask((size_t)pixels, 0);
        long opening = 0;
        if (with_portal && no_portal) {
            for (long i = 0; i < pixels; ++i) {
                for (int c = 0; c < 3; ++c) {
                    if (std::abs((int)with_portal[i * 4 + c] - (int)no_portal[i * 4 + c]) > 8) {
                        mask[(size_t)i] = 1;
                        ++opening;
                        break;
                    }
                }
            }
        }
        gfx_free_pixels(no_portal);

        std::printf("\n  the target at the portal's own size, %d frames a pass, twice over\n",
                    kMeasure);
        std::printf("    the opening paints %ld of %ld pixels (%.1f%% of the screen)\n",
                    opening, pixels, 100.0 * (double)opening / (double)pixels);
        std::printf("    %-7s %-11s %8s %8s %10s %9s\n", "scale", "target", "ms", "saved",
                    "changed", "mean err");

        for (int i = 0; i < kCount; ++i) {
            set_scale(scales[i]);
            unsigned char *shot = capture(true);

            long   changed = 0;
            double sum     = 0.0;
            if (shot && with_portal) {
                for (long px = 0; px < pixels; ++px) {
                    if (!mask[(size_t)px]) continue;
                    int worst = 0;
                    for (int c = 0; c < 3; ++c) {
                        const int moved = std::abs((int)shot[px * 4 + c] -
                                                   (int)with_portal[px * 4 + c]);
                        if (moved > worst) worst = moved;
                    }
                    sum += (double)worst;
                    if (worst > 8) ++changed;
                }
            }
            gfx_free_pixels(shot);

            const int    w = (int)((float)kWindowWidth * scales[i] + 0.5f);
            const int    h = (int)((float)kWindowHeight * scales[i] + 0.5f);
            char         size[24];
            std::snprintf(size, sizeof size, "%dx%d", w, h);
            std::printf("    %-7.2f %-11s %8.3f %8.3f %9.1f%% %9.1f\n", (double)scales[i], size,
                        ms[i], ms[0] - ms[i],
                        opening > 0 ? 100.0 * (double)changed / (double)opening : 0.0,
                        opening > 0 ? sum / (double)opening : 0.0);
        }

        gfx_free_pixels(with_portal);
        gfx_rt_destroy(g_capture_rt);
        g_capture_rt = gfx_rt{0};
        set_scale(1.0f);
        std::printf("    (changed = pixels of the opening that move by more than 8 of 255 in "
                    "any channel; mean err = the average of that move over the whole opening)\n");
    }

    // And the other half of the passive budget: the mesh tick. One chunk a
    // frame is cheap per frame and slow to finish, and "slow" is a number a
    // player waits through, so it is measured rather than described.
    {
        s.at(1).world.mark_all_dirty();
        int          frames = 0;
        const double t0     = plat_time();
        for (; frames < 200000; ++frames) {
            s.at(1).world_render.sync(s.at(1).world, s.at(1).registry,
                                            kPassiveMeshBudget);
            if (s.at(1).world_render.last_meshed() == 0) break;
        }
        const double secs = plat_time() - t0;
        std::printf("  the passive world remeshes whole in %d ticks at %d chunk(s) a tick "
                    "(%.3f s of mesher time, %.1f s of wall clock at 60 frames a second)\n",
                    frames, kPassiveMeshBudget, secs, (double)frames / 60.0);
    }

    gfx_set_present(true);
    return 0;
}

int run_verify(Scene &s)
{
    s.act().world_render.sync(s.act().world, s.act().registry, 100000);

    // Four distances, not one. The first version of this gate measured 6.5
    // blocks out and nothing else, and passed while the image at a block and a
    // half looked wrong to somebody standing in front of it.
    //
    // The last one is measured and reported but not enforced. Closer than about
    // a block the visible aperture of a hole through a block-thick wall stops
    // being a rectangle -- it is the intersection of two mouths seen at very
    // different angles -- and the rectangle this test compares inside starts
    // covering the tunnel wall instead of the opening. That is a limit of the
    // measurement, not a licence: it is printed every run so it cannot be
    // forgotten, and fixing it means comparing against a mask rather than a box.
    struct Distance { float d; bool enforced; };
    const Distance distances[] = {{6.5f, true}, {3.0f, true}, {1.5f, true}, {0.8f, false}};

    // A tenth of a per cent of a region that already excludes the border.
    //
    // It was TWO per cent, on the reasoning written here for four milestones:
    // "anything structural -- a seam, a wrong projection, geometry leaking
    // through -- moves this by whole percentage points, not tenths". That is
    // false, and a measurement disproved it: clipping the destination's image a
    // whole node early moved this to 1.15%, which is structural by any
    // definition and sat comfortably under the old threshold, printed as PASS.
    // See docs/archive/M5_FINDINGS.md 9.8.
    //
    // The new number is not a guess either. Both shapes config/portal.cfg can
    // take -- the image on the far face and the image at the node centre --
    // read exactly 0 differing pixels of 12960, 60726 and 242900, repeatably
    // across runs. A tenth of a per cent still allows 243 pixels at the closest
    // distance, which is more slack than a shared rasterised edge can use, and
    // it is eleven times smaller than the defect that hid here.
    //
    // If this ever goes red on a change that is genuinely correct, the answer
    // is to find out which pixels moved and why -- not to put the 2 back.
    constexpr double kMaxDifferingPct = 0.10;
    bool all_pass = true;

    for (const Distance &entry : distances) {
        const float d = entry.d;
        long total = 0, differing = 0;
        const double pct = continuity_at(s, 40.5f + d, &total, &differing);

        if (pct < 0.0) {
            std::printf("continuity: %s at %.1f blocks -- no usable aperture to compare inside\n",
                        entry.enforced ? "FAIL" : "not measurable", d);
            all_pass = all_pass && !entry.enforced;
            continue;
        }

        const bool pass = pct <= kMaxDifferingPct;
        if (entry.enforced) all_pass = all_pass && pass;
        std::printf("continuity: %s at %.1f blocks  %6ld/%6ld pixels differ (%5.2f%%,"
                    " threshold %.2f%%)%s\n",
                    pass ? "PASS" : "FAIL", d, differing, total, pct, kMaxDifferingPct,
                    entry.enforced ? "" : "   [reported, not enforced]");
    }

    return all_pass ? 0 : 1;
}

// ------------------------------------------------------------------- soak
//
// The other half of the M1 exit test: memory stable over ten minutes of flight.
// A frame-rate average hides exactly the failure this is looking for, so the
// report is percentiles -- a rebuild storm shows up in p99 and max while
// leaving the mean untouched.

// Where the growth actually is. RSS alone cannot tell an allocator that keeps
// freed memory from a driver that caches GPU buffers behind our back, and
// guessing between those two costs more than measuring.
struct MemorySnapshot {
    size_t rss        = 0;   // what the OS says the process occupies
    size_t heap_used  = 0;   // glibc: bytes in live allocations
    size_t heap_total = 0;   // glibc: bytes it has taken from the OS and kept
};

size_t resident_bytes()
{
#if !defined(__linux__)
    return 0;   // statm is Linux's; the soak's memory line reads 0 elsewhere
#else
    // statm reports pages; field two is resident set size.
    std::FILE *f = std::fopen("/proc/self/statm", "r");
    if (!f) return 0;
    long total = 0, rss = 0;
    const int got = std::fscanf(f, "%ld %ld", &total, &rss);
    std::fclose(f);
    if (got != 2) return 0;
    return (size_t)rss * (size_t)sysconf(_SC_PAGESIZE);
#endif
}

MemorySnapshot memory_snapshot()
{
    MemorySnapshot m;
    m.rss = resident_bytes();
#if defined(__GLIBC__)
    const struct mallinfo2 mi = mallinfo2();
    m.heap_used  = (size_t)mi.uordblks + (size_t)mi.hblkhd;
    m.heap_total = (size_t)mi.arena + (size_t)mi.hblkhd;
#endif
    return m;
}

int run_soak(Scene &s, double seconds)
{
    s.act().world_render.sync(s.act().world, s.act().registry, 100000);

    const float mid = (float)g_world_extent * 0.5f;
    const float radius = (float)g_world_extent * 0.28f;

    // Measure the renderer, not the compositor.
    gfx_set_present(false);

    // Frame times go into a fixed histogram rather than an array of samples.
    //
    // Keeping every sample is the obvious thing and it makes the soak lie about
    // the quantity it exists to measure. At a few thousand frames a second the
    // vector crosses two million entries after about twenty minutes and doubles
    // from 16 MB to 32 MB. In the memory report that is a sudden step of live
    // allocation, indistinguishable from the engine leaking -- and it was read
    // as exactly that until the world and GPU columns turned out to be flat
    // across it.
    constexpr int    kBins  = 20000;   // 0.01 ms wide, so up to 200 ms
    constexpr double kBinMs = 0.01;
    std::vector<uint32_t> hist((size_t)kBins + 1, 0);   // final bin catches the rest
    size_t frame_count = 0;
    double worst_ms    = 0.0;

    const MemorySnapshot mem_start = memory_snapshot();
    const size_t rss_start = mem_start.rss;
    const double t_start   = plat_time();
    double next_report     = 20.0;
    long   edits             = 0;
    long   frames_with_remesh = 0;
    double sum_script_ms      = 0.0;
    double sum_sync_ms        = 0.0;
    double sum_render_ms      = 0.0;
    double sum_finish_ms      = 0.0;
    long   chunks_remeshed    = 0;
    double worst_columns_ms   = 0.0;   // the longest a frame spent on columns

    // Where memory is measured from. The start, for a circle over the box the
    // world began as; for --soak-far the first report a third of the way in,
    // because a flight out of the box has to fill memory with the world around
    // it before there is anything to let go of, and growth up to that point is
    // the world arriving rather than the world leaking.
    size_t rss_settled = 0;

    // Line buffering: a soak that gets killed mid-run must still have said
    // everything it managed to measure. Block-buffered stdout loses the lot.
    setvbuf(stdout, nullptr, _IOLBF, 0);
    std::printf("soak: %.0f s, world %d, starting RSS %.1f MB\n",
                seconds, g_world_extent, (double)rss_start / (1024.0 * 1024.0));

    while (true) {
        const double now = plat_time() - t_start;
        if (now >= seconds) break;

        // A circular flight, so the whole world streams through the frustum
        // rather than one lucky corner of it staying resident.
        //
        // Or, with --soak-far, a straight one out of the box and on, at that
        // many blocks a second (TODO.md OPEN-35, OPEN-38): the world ahead has
        // to be made as fast as the camera crosses it, and what it leaves behind
        // has to be let go of or the memory column below climbs for ever.
        const float angle    = (float)now * 0.25f;
        const bool  straight = g_soak_far > 0.0;
        const Vec3  eye      = straight
                                   ? Vec3{mid + (float)(g_soak_far * now), (float)(kGroundTop + 26), mid}
                                   : Vec3{mid + radius * std::cos(angle), (float)(kGroundTop + 26),
                                          mid + radius * std::sin(angle)};
        const Vec3  fwd = dir_from_angles(straight ? 1.5708f : angle + 1.5708f, -0.15f);
        const Vec3  up{0.0f, 1.0f, 0.0f};

        // Edit as we go: placing and breaking is what drives relighting and
        // remeshing, and a soak that only flies never exercises either.
        if ((edits % 4) == 0) {
            const RaycastHit hit = raycast(s.act().world, s.act().registry, eye, fwd, 40.0f);
            if (hit.hit) {
                const bool remove = ((edits / 4) & 1) != 0;
                if (remove) s.act().world.set_block(hit.x, hit.y, hit.z, kAir);
                else        s.act().world.set_block(hit.x + hit.nx, hit.y + hit.ny,
                                              hit.z + hit.nz, kStone);
                relight_around(s.act().world, s.act().registry, hit.x, hit.y, hit.z);
            }
        }
        ++edits;

        const double frame_t0 = plat_time();
        step_script(s, 1.0 / 60.0);
        const double t_columns = plat_time();
        step_columns(s.act(), eye.x, eye.z);
        const double t_script = plat_time();
        worst_columns_ms = std::max(worst_columns_ms, (t_script - t_columns) * 1000.0);
        s.act().world_render.sync(s.act().world, s.act().registry);
        const double t_sync = plat_time();
        render_frame(s, eye, fwd, up, false, nullptr);
        const double t_render = plat_time();
        gfx_finish();   // stop the clock when the GPU is done, not when it is asked
        const double t_end = plat_time();

        // Per-phase totals, so a percentile that moves can be attributed
        // instead of guessed at. The phases are already timed for the STALL
        // line below; keeping the sums costs four additions a frame.
        sum_script_ms += (t_script - frame_t0) * 1000.0;
        sum_sync_ms   += (t_sync - t_script) * 1000.0;
        sum_render_ms += (t_render - t_sync) * 1000.0;
        sum_finish_ms += (t_end - t_render) * 1000.0;

        if (s.act().world_render.last_meshed() > 0) {
            ++frames_with_remesh;
            chunks_remeshed += (long)s.act().world_render.last_meshed();
        }

        const double total_ms = (t_end - frame_t0) * 1000.0;
        const int bin = (total_ms >= (double)kBins * kBinMs) ? kBins : (int)(total_ms / kBinMs);
        ++hist[(size_t)bin];
        ++frame_count;
        if (total_ms > worst_ms) worst_ms = total_ms;
        if (total_ms > 200.0) {
            std::printf("  STALL %.0f ms: script %.1f (systems %.1f, on_step %.1f, modifiers %.1f, "
                        "light %.1f, tick %.1f, columns %.1f)  sync %.1f  render %.1f  "
                        "finish %.1f\n",
                        total_ms, (t_script - frame_t0) * 1000.0, g_script_phase_ms[0],
                        g_script_phase_ms[1], g_script_phase_ms[2], g_script_phase_ms[3],
                        g_script_phase_ms[4], (t_script - t_columns) * 1000.0,
                        (t_sync - t_script) * 1000.0, (t_render - t_sync) * 1000.0,
                        (t_end - t_render) * 1000.0);
        }

        if (now >= next_report) {
            const MemorySnapshot m = memory_snapshot();
            const double mb = 1024.0 * 1024.0;
            std::printf("  t=%5.0fs  RSS %7.1f (%+6.1f)  heap-used %6.1f (%+5.1f)"
                        "  heap-total %6.1f (%+5.1f)  world %.1f  gpu %.1f  MB\n",
                        now,
                        (double)m.rss / mb, ((double)m.rss - (double)mem_start.rss) / mb,
                        (double)m.heap_used / mb,
                        ((double)m.heap_used - (double)mem_start.heap_used) / mb,
                        (double)m.heap_total / mb,
                        ((double)m.heap_total - (double)mem_start.heap_total) / mb,
                        (double)s.act().world.memory_bytes() / mb,
                        (double)s.act().world_render.gpu_bytes() / mb);
            if (const ColumnKeeper *k = s.act().keeper.get())
                std::printf("           at x %.0f: %zu chunks, %zu column(s) in memory (%zu lit), "
                            "%llu made, %llu let go, %zu kept in %.1f KB; %.0f ms filling, "
                            "%.0f ms lighting\n",
                            (double)eye.x, s.act().world.chunk_count(), k->columns().count(),
                            k->columns().lit_count(),
                            (unsigned long long)k->columns().filled_total,
                            (unsigned long long)k->columns().unloaded_total, k->kept_columns(),
                            (double)k->kept_bytes() / 1024.0, k->fill_ms(), k->light_ms());
            if (g_soak_far > 0.0 && rss_settled == 0 && now >= seconds / 3.0) rss_settled = m.rss;
            next_report += 20.0;
        }
    }

    // Percentiles read straight out of the histogram: walk the bins until the
    // running count crosses the quantile.
    auto pct = [&](double q) {
        if (frame_count == 0) return 0.0;
        const size_t target = (size_t)(q * (double)frame_count);
        size_t seen = 0;
        for (int i = 0; i <= kBins; ++i) {
            seen += hist[(size_t)i];
            if (seen >= target) return (i == kBins) ? worst_ms : ((double)i + 0.5) * kBinMs;
        }
        return worst_ms;
    };

    const size_t rss_end  = resident_bytes();
    const size_t rss_from = rss_settled != 0 ? rss_settled : rss_start;
    const double growth   = ((double)rss_end - (double)rss_from) / (1024.0 * 1024.0);

    std::printf("\nsoak result: %zu frames, %ld edits\n", frame_count, edits);
    if (s.act().keeper)
        std::printf("  columns    at most %.1f ms in a frame\n", worst_columns_ms);
    std::printf("  frame ms   p50 %.3f   p95 %.3f   p99 %.3f   max %.3f\n",
                pct(0.50), pct(0.95), pct(0.99), worst_ms);
    std::printf("  per frame  script %.4f  sync %.4f  render %.4f  finish %.4f ms (mean)\n",
                sum_script_ms / (double)frame_count, sum_sync_ms / (double)frame_count,
                sum_render_ms / (double)frame_count, sum_finish_ms / (double)frame_count);
    std::printf("  remesh     %ld frames of %zu (%.1f%%) rebuilt a chunk, %ld chunks total\n",
                frames_with_remesh, frame_count,
                frame_count ? 100.0 * (double)frames_with_remesh / (double)frame_count : 0.0,
                chunks_remeshed);
    std::printf("  fps        p50 %.0f   p99 %.0f (worst frames)\n",
                1000.0 / (pct(0.50) > 0 ? pct(0.50) : 1),
                1000.0 / (pct(0.99) > 0 ? pct(0.99) : 1));
    std::printf("  RSS        %.1f -> %.1f MB  (%+.1f MB%s)\n",
                (double)rss_from / (1024.0 * 1024.0),
                (double)rss_end / (1024.0 * 1024.0), growth,
                rss_settled != 0 ? ", from a third of the way in" : "");

    // Sixty FPS is the M1 bar, and it has to hold for the worst frames, not the
    // median. Memory is allowed to settle upward as chunks mesh for the first
    // time; what it must not do is climb without bound.
    const bool fps_ok = pct(0.99) <= 16.67;
    const bool mem_ok = growth < 64.0;
    std::printf("  %s frame time (p99 under 16.67 ms)\n", fps_ok ? "PASS" : "FAIL");
    std::printf("  %s memory (growth under 64 MB)\n", mem_ok ? "PASS" : "FAIL");
    gfx_set_present(true);
    return (fps_ok && mem_ok) ? 0 : 1;
}

// ------------------------------------------------------------------ network
// The game, with the world coming from a server instead of from this process.
//
// What changes, and each of these is a decision rather than an omission:
//
//   - Nothing is generated locally. The world starts empty and fills in as
//     chunks arrive, which is why the first seconds look like standing in fog.
//   - The player still moves locally, every frame, against whatever has
//     arrived. That is prediction: the server validates claims, it does not
//     simulate the player (core/physics/movement.h), so the client is the one
//     that knows where it is until told otherwise.
//   - A correction is applied by snapping. No smoothing, on purpose: a
//     correction means the server and this client disagree about something
//     basic, and interpolating towards the truth over half a second is how a
//     disagreement gets hidden instead of seen.
//   - Placing and breaking go through the server, since M7 gave them a message
//     (net::Edit). Nothing is drawn on the click: what comes back is what the
//     server decided, because a client that drew its own would be showing a
//     block that may have been refused, for as long as the round trip lasts.
//
// `seconds` above zero turns it into a gate rather than a game: it plays for
// that long, takes no input, and returns non-zero if what should have happened
// did not. A network client nobody can check automatically is a network client
// that quietly stops working, and this one is not reachable by a headless test
// -- it needs the window -- so the check has to live where the window is.
// The openings, as the server says they are.
//
// This is the whole of what a network client knows about a portal. It does not
// decide where one goes, it does not decide whether a placement was allowed,
// and it does not run the crossing -- DESIGN.md 3.4 puts all three on the
// server. What it does is draw what it was told, which is why every field here
// comes off the message and none of them is remembered from the click.
//
// An opening exists in three places in this client: twice in the renderer, as
// one link's source and the other's destination, and once as the Portal a body
// walks through. All three are written here, for the reason host_set_portal
// carries in its own comment -- a pair that drifts is drawn in one place and
// walked into in another.
// One opening, as the wire describes it. The one place that reads a PortalDesc,
// because two readings of one description is exactly the pair that drifts.
Portal portal_from_desc(const net::PortalDesc &d)
{
    Portal p;
    p.center      = Vec3{d.cx, d.cy, d.cz};
    p.axis        = (PortalAxis)d.axis;
    p.normal_sign = d.normal_sign;
    p.rot         = d.rot;
    p.half_w      = d.half_w;
    p.half_h      = d.half_h;
    p.kind        = (PortalKind)d.kind;
    p.colour      = d.colour;
    return p;
}

// The other direction: an opening this client holds, as the wire wants it.
net::PortalDesc desc_from_portal(const Portal &p)
{
    net::PortalDesc d;
    d.cx          = p.center.x;
    d.cy          = p.center.y;
    d.cz          = p.center.z;
    d.axis        = (uint8_t)p.axis;
    d.normal_sign = (int8_t)p.normal_sign;
    d.rot         = (uint8_t)p.rot;
    d.half_w      = p.half_w;
    d.half_h      = p.half_h;
    d.kind        = (uint8_t)p.kind;
    d.colour      = p.colour;
    return d;
}

void apply_server_portals(Scene &s, const net::Portals &ps)
{
    Portal p[2] = {s.act().portal_a, s.act().portal_b};

    for (int i = 0; i < 2; ++i)
        if (ps.has(i)) p[i] = portal_from_desc(ps.opening[i]);

    s.act().portal_a = p[0];
    s.act().portal_b = p[1];

    // And the mouths past the second, unlinked like the two below.
    for (int m = 2; m < net::kMaxMouths; ++m) {
        Session::MoreMouth &mm = s.act().more_mouths[m - 2];
        if (ps.has(m) && ps.away[m].leads_away()) mm.portal = portal_from_desc(ps.opening[m]);
        mm.link    = PortalLink{};
        mm.session = -1;
    }

    // A pair that leads to another server is a MOUTH: one opening here, and
    // what is behind it belongs to a world this function has never heard of.
    // The link is built when that world says where its own opening is --
    // link_to_remote below -- and building a local one here first would draw
    // this world inside its own doorway for a frame or two.
    //
    // Unlinked here, both of them: the openings just moved or just arrived,
    // and a link kept from before is drawn where they used to be. The caller
    // relinks every mouth whose world is up in the same frame.
    s.act().more_links.clear();
    s.act().linked_openings.clear();
    s.act().linked_to.clear();
    // What was there before, to see which openings have just been linked.
    const std::vector<Portal> was_closed = s.act().closed_openings;
    const std::vector<Portal> was_linked = s.act().linked_openings;
    const auto same = [](const Portal &a, const Portal &b) {
        return a.axis == b.axis && length_sq(a.center - b.center) < 1e-4f;
    };
    const auto now = std::chrono::steady_clock::now();
    for (int i = 0; i < net::kMaxOpenings; ++i) {
        if (ps.partner(i) < 0 || (i < net::kMaxMouths && ps.away[i].leads_away())) continue;
        const Portal p = portal_from_desc(ps.opening[i]);
        bool linked_before = false, closed_before = false;
        for (const Portal &q : was_linked) linked_before = linked_before || same(p, q);
        for (const Portal &q : was_closed) closed_before = closed_before || same(p, q);
        if (closed_before || (!linked_before && s.act().portals_heard))
            s.act().opening.push_back({p, now});
    }
    s.act().portals_heard = true;
    s.act().mouths_here   = 0;
    for (int m = 0; m < net::kMaxMouths; ++m)
        if (ps.has(m) && ps.away[m].leads_away()) s.act().mouths_here |= 1u << m;

    s.act().local_links.clear();
    s.act().closed_openings.clear();
    for (int i = 0; i < net::kMaxOpenings; ++i)
        if (ps.has(i) && ps.partner(i) < 0 && !(i < net::kMaxMouths && ps.away[i].leads_away()))
            s.act().closed_openings.push_back(portal_from_desc(ps.opening[i]));

    if (ps.leads_away()) {
        s.act().link_count     = 1;
        s.act().link_session   = -1;
        s.act().link_session_b = -1;
        s.act().portals        = false;
        // The pairs of its own, past the mouths.
        const auto mouth = [&](int k) { return k < net::kMaxMouths && ps.away[k].leads_away(); };
        for (int i = 0; i < net::kMaxOpenings; ++i) {
            const int j = ps.partner(i);
            if (j < 0 || mouth(i) || mouth(j)) continue;
            s.act().local_links.push_back(
                PortalLink{surface_from_portal(portal_from_desc(ps.opening[i])),
                           surface_from_portal(portal_from_desc(ps.opening[j])), gfx_rt{}});
            // And fallen into, as the openings of a world without mouths are:
            // a floor one holds a body up until it is over it (floor shift),
            // and a fall is steered into it (funnel).
            s.act().linked_openings.push_back(portal_from_desc(ps.opening[i]));
            s.act().linked_to.push_back(portal_from_desc(ps.opening[j]));
        }
        return;
    }

    s.act().link_session   = -1;
    s.act().link_session_b = -1;

    // Every opening that leads somewhere, in index order: the first two links
    // are link_ab and link_ba, which own the targets, and the rest go in
    // more_links. With the pair alone that is 0 to 1 and 1 to 0, as it always
    // was.
    //
    // Half a pair is drawn as nothing. An opening whose other end does not
    // exist has no image to show: the render target would be filled with a view
    // from a destination nobody placed, which is a picture of somewhere real
    // and therefore worse than a hole -- a player would read it as a working
    // portal and walk into a wall.
    std::vector<PortalLink> all;
    for (int i = 0; i < net::kMaxOpenings; ++i) {
        const int j = ps.partner(i);
        if (j < 0) continue;
        const Portal from = portal_from_desc(ps.opening[i]);
        all.push_back(PortalLink{surface_from_portal(from),
                                 surface_from_portal(portal_from_desc(ps.opening[j])), gfx_rt{}});
        s.act().linked_openings.push_back(from);
        s.act().linked_to.push_back(portal_from_desc(ps.opening[j]));
    }

    if (all.size() >= 1) s.act().link_ab = PortalLink{all[0].src, all[0].dst, s.act().link_ab.rt};
    if (all.size() >= 2) s.act().link_ba = PortalLink{all[1].src, all[1].dst, s.act().link_ba.rt};
    if (all.size() > 2) s.act().more_links.assign(all.begin() + 2, all.end());
    s.act().link_count = all.size() >= 2 ? 2 : 1;
    s.act().portals    = !all.empty() && s.act().link_ab.rt.id != 0;
}

// The far side of a mouth: the opening in the OTHER world, as that world's own
// server describes it.
//
// This is the whole reason the near server does not carry the far geometry. It
// does not know it, and a copy of it there would go stale the moment somebody
// over there moved their opening. The client asks the world it is looking into,
// which is a question a passive session can already answer.
// `which` is the opening over there this mouth leads to, from the near link's
// PortalAway::portal. It has to be the SAME number the crossing sends, or the
// image in the doorway is of one hole and walking into it comes out of another
// -- which is a portal that lies, and is exactly what happened for one commit:
// the crossing learned to name an opening and this did not.
// `mouth` is which opening HERE: the first is link_ab, the second link_ba, the
// rest Session::more_mouths, each with a target of its own (Scene::mouth_rt).
bool link_to_remote(Scene &s, int mouth, int world, const net::Portals &far, int which)
{
    if (world < 0 || world >= s.count()) return false;
    if (mouth < 0 || mouth >= net::kMaxMouths) return false;
    if (which < 0 || which >= net::kMaxOpenings) return false;
    if (!far.has(which)) return false;

    const Portal there = portal_from_desc(far.opening[which]);
    s.at(world).portal_a  = there;
    s.at(world).view_range = kPassiveViewRange;

    if (mouth >= 2) {
        gfx_rt &rt = s.mouth_rt[mouth - 2];
        if (rt.id == 0)
            rt = gfx_rt_create(portal_rt_dim(render_width()), portal_rt_dim(render_height()), true);
        Session::MoreMouth &mm = s.act().more_mouths[mouth - 2];
        mm.link    = PortalLink{surface_from_portal(mm.portal), surface_from_portal(there), rt};
        mm.session = world;
        s.act().portals = s.act().portals || rt.id != 0;
    } else if (mouth == 0) {
        s.act().link_ab      = PortalLink{surface_from_portal(s.act().portal_a),
                                          surface_from_portal(there), s.act().link_ab.rt};
        s.act().link_count   = s.act().two_mouths() ? 2 : 1;
        s.act().link_session = world;
        s.act().portals      = s.act().link_ab.rt.id != 0;
    } else {
        s.act().link_ba        = PortalLink{surface_from_portal(s.act().portal_b),
                                            surface_from_portal(there), s.act().link_ba.rt};
        s.act().link_count     = 2;
        s.act().link_session_b = world;
        s.act().portals        = s.act().portals || s.act().link_ba.rt.id != 0;
    }
    return true;
}

// Driving through a mouth into a world that knows what the body wears: the
// server keeps it a kart there (take_carried), and so does this machine, from
// the first frame. Otherwise the world arrived in the first time was on foot
// until its message came, and the kart was put down afresh and rose on its
// springs -- a getting-in over again. The same rule as the server's: the body
// drives, and the far world's table has its model by name.
void carry_kart(const Session &from, const std::vector<std::string> &from_models, Session &to,
                const std::vector<std::string> &to_models)
{
    if (!from.movement.drives() || from.own_body_model == kNoModel ||
        (size_t)from.own_body_model > from_models.size())
        return;
    const std::string &name = from_models[(size_t)from.own_body_model - 1];
    for (size_t i = 0; i < to_models.size(); ++i) {
        if (to_models[i] != name) continue;
        to.movement       = from.movement;
        to.own_body_model = (ModelId)(i + 1);
        to.view_distance  = from.view_distance;
        // And the body's motion, so the driver stays sat in it: a world where
        // this body was last on foot would play getting in again.
        if (const auto it = from.motion.find(kOwnBody); it != from.motion.end())
            to.motion[kOwnBody] = it->second;
        std::printf("drove through in '%s', which the world arrived in has too\n", name.c_str());
        return;
    }
}

// Arriving in the world that was behind the opening.
//
// Everything DESIGN.md 4.3 lists as already paid for, on a pair of worlds that
// are two SERVERS rather than two local sessions -- and one thing that only
// happens here: the render target moves. There is one, it belongs to whichever
// world is being stood in, and the world just left has no use for it.
//
// The pose comes from the server on the far side, because the arrival is its
// decision: it owns the opening being come out of.
void arrive_from_portal(Scene &s, Player &player, const net::Teleport &t, int new_active)
{
    // Demote, before the index moves, so it lands on the world being left.
    s.act().draws_own_avatar = false;
    s.act().view_range       = kPassiveViewRange;

    const int left = s.active;

    // Both targets: a world with two mouths draws two images, and the world
    // arrived in may be one.
    gfx_rt rt   = s.act().link_ab.rt;
    gfx_rt rt_b = s.act().link_ba.rt;
    s.act().link_ab.rt     = gfx_rt{};
    s.act().link_ba.rt     = gfx_rt{};
    s.act().link_count     = 0;
    s.act().link_session_b = -1;
    for (Session::MoreMouth &mm : s.act().more_mouths) {
        mm.session = -1;
        mm.link    = PortalLink{};
    }
    s.act().portals        = false;
    s.act().more_links.clear();
    s.act().linked_openings.clear();
    s.act().local_links.clear();
    s.act().closed_openings.clear();
    s.act().opening.clear();
    s.act().portals_heard = false;
    s.act().mouths_here   = 0;
    s.act().mouths_drawn  = 0;

    s.active = new_active;

    s.act().link_ab.rt       = rt;
    s.act().link_ba.rt       = rt_b;
    s.act().draws_own_avatar = true;
    s.act().view_range       = kActiveViewRange;
    s.act().link_session     = left;
    s.act().link_session_b   = -1;
    s.act().link_count       = 1;

    player.pos       = Vec3{t.x, t.y, t.z};
    player.velocity  = Vec3{t.vx, t.vy, t.vz};
    player.yaw       = std::atan2(t.look_x, -t.look_z);
    player.pitch     = std::asin(std::clamp(t.look_y, -1.0f, 1.0f));
    player.vehicle.reset();   // afresh, as the server's copy is
    player.on_ground = false;
    player.cb_shift  = 0.0f;
    s.roll           = t.roll;
    aim_after_crossing(s, player);

    // No camera catch-up. The eye was continuous through the opening -- that is
    // what the image inside it was for -- so there is nothing to absorb.
    s.cam_warp      = Vec3{};
    s.cam_warp_left = 0.0f;

    // Seeded, or the arrival walks straight back through the opening it came
    // out of. Done against the opening this world will report; until that
    // arrives the record is cleared, which is the safe half of the same thing.
    s.act().trigger_a = PortalTriggerState{};
    s.act().trigger_b = PortalTriggerState{};
    for (Session::MoreMouth &mm : s.act().more_mouths) mm.trigger = PortalTriggerState{};
}

// Why a session is not playing, in the words of whoever decided it.
//
// A refusal carries a reason AND a detail, and the detail is where the server
// puts the part a person can act on: which two clocks disagree and by how much,
// which feature was missing, which block table. Two places in this loop printed
// the reason alone -- so a far world that refused a ticket because its clock is
// two minutes off said "no ticket this world will honour", and whoever read
// that went to look at trust and signatures, which were fine.
//
// ClientSession::error() is already "reason: detail" for a refusal and the
// failure text otherwise; the state name is the last resort for a session that
// has stopped without either, which is what a give-up by timeout looks like.
const char *why_not(const net::ClientSession &s)
{
    return s.error().empty() ? net::client_state_text(s.state()) : s.error().c_str();
}

// And the other half of "may I walk in": is there anything to walk into yet.
//
// A window onto another world streams while it is being looked at, and that is
// the whole point of it -- a crossing costs no load because the world is
// already here. On loopback it always is by the time anybody reaches the mouth.
// On a link that drops packets it may not be: measured at 400 ms and 20% loss,
// one run in eight walked through with the far world at ZERO chunks, and the
// world arrived afterwards. That is a loading screen, however short.
//
// Asked about the block the player is going to come out in, not about the world
// in general: the chunk under the arrival is the nearest one to the point the
// observer claims, so it is the first to arrive and the last thing worth
// waiting for.
bool mouth_has_ground(const Scene &s, int world, Vec3 at)
{
    if (world < 0 || world >= s.count()) return false;
    const ChunkPos p = World::chunk_of((int)std::floor(at.x), (int)std::floor(at.y),
                                          (int)std::floor(at.z));
    return s.at(world).world.find_chunk(p) != nullptr;
}

// Every render target shaped like the screen, built again at a new size.
//
// A resizable window makes this necessary and nothing else does: the projection
// already reads gfx_screen_width() every frame, but a portal's image is rendered
// into a target of its own and then sampled in screen space, so a target still
// the shape of the old window puts a stretched world inside the opening. Nothing
// warns about it -- it just looks slightly wrong, which is the kind of thing
// somebody notices six months later and cannot place.
//
// Returns false if a target could not be made, which the caller has to treat as
// fatal for the same reason the first allocation is: half a pool means some
// portals recurse and some show a hole.
//
// Not to be called per frame while a window is being dragged -- see
// ScreenTargets below, which is what the loops actually use.
bool resize_screen_targets(Scene &s, int width, int height)
{
    const int w = portal_rt_dim(width);
    const int h = portal_rt_dim(height);

    for (int i = 0; i < s.count(); ++i) {
        PortalLink *links[2] = {&s.at(i).link_ab, &s.at(i).link_ba};
        for (PortalLink *link : links) {
            if (link->rt.id == 0) continue;   // never had one; not this function's job

            gfx_rt_destroy(link->rt);
            link->rt = gfx_rt_create(w, h, true);
            if (link->rt.id == 0) return false;
        }
    }

    for (gfx_rt &rt : s.more_rt) {
        if (rt.id == 0) continue;
        gfx_rt_destroy(rt);
        rt = gfx_rt_create(w, h, true);
        if (rt.id == 0) return false;
    }

    // The mouths' past the second, and the links holding them: each world's
    // link keeps a copy of the handle it was given.
    for (int k = 0; k < net::kMaxMouths - 2; ++k) {
        gfx_rt &rt = s.mouth_rt[k];
        if (rt.id == 0) continue;
        gfx_rt_destroy(rt);
        rt = gfx_rt_create(w, h, true);
        if (rt.id == 0) return false;
        for (int i = 0; i < s.count(); ++i) s.at(i).more_mouths[k].link.rt = rt;
    }

    // The pool below the first level, at the same depth and for as many links
    // as it was built with -- rebuilding one level shallower would quietly
    // change what is drawn.
    const int links = std::max(2, s.portal_render.pool_links());
    s.portal_render.shutdown_recursion();
    return s.portal_render.init_recursion(links, g_recursion_depth, w, h);
}

// The screen-shaped targets, kept in step with a window somebody is dragging.
//
// It WAITS for the drag to settle, and that is the whole reason this is a type
// and not two lines in each loop. Rebuilding on every size the window passes
// through means destroying and creating several render targets per frame:
// measured on one drag of this window, 195 rebuilds -- a GL object churn nobody
// asked for, in the frames where the machine is already busiest.
//
// The cost of waiting is that the portal image is the shape of the OLD window
// for a fifth of a second after the drag stops, which is a stretched picture
// nobody is looking at while their hand is on the frame.
struct ScreenTargets {
    // How long the size has to hold still before it is worth believing.
    static constexpr double kSettle = 0.2;

    int    built_w = 0, built_h = 0;   // what the targets were made for
    int    want_w  = 0, want_h  = 0;   // what the window last said
    double moved_at = 0.0;

    void start()
    {
        built_w = want_w = gfx_screen_width();
        built_h = want_h = gfx_screen_height();
    }

    // Call once a frame. False only when a target could not be made, which is
    // as fatal as failing to make the first one.
    bool follow(Scene &s, double now)
    {
        const int w = gfx_screen_width();
        const int h = gfx_screen_height();

        if (w != want_w || h != want_h) {
            want_w   = w;
            want_h   = h;
            moved_at = now;
            return true;   // still moving; nothing to build yet
        }

        if ((want_w == built_w && want_h == built_h) || now - moved_at < kSettle) return true;

        if (!resize_screen_targets(s, want_w, want_h)) return false;

        built_w = want_w;
        built_h = want_h;
        std::printf("window: %dx%d\n", built_w, built_h);
        return true;
    }
};

// A scripted body driving to a point, in a world whose bodies drive
// (--walk-path, and the walk into a mouth). The keys a player has and nothing
// else: a wheel towards the point, a throttle that slows for a bend and for
// arriving, and reverse when the point is behind. Setting the yaw outright, as
// the walk on foot does, would turn a body faster than its wheel can -- which
// the server does not check, and a run is only worth what a player could do.
bool g_walk_boost = false;   // --walk-boost: the scripted driver uses the nitro on straights

void drive_towards(const Player &player, const MovementRules &rules, Vec3 target, PlayerInput &in)
{
    const float dx   = target.x - player.pos.x;
    const float dz   = target.z - player.pos.z;
    const float dist = std::sqrt(dx * dx + dz * dz);

    float err = std::atan2(dx, -dz) - player.yaw;
    while (err > 3.14159265f) err -= 6.2831853f;
    while (err < -3.14159265f) err += 6.2831853f;

    const bool reversing = player.drive_speed < -0.5f;
    const bool stopped   = std::fabs(player.drive_speed) < 1.0f;
    if (std::fabs(err) > 2.2f || (reversing && std::fabs(err) > 1.0f) ||
        (stopped && std::fabs(err) > 1.2f)) {
        // Behind: back up, turning the other way round, until the point is far
        // enough ahead to drive at -- a three-point turn, which a road seven
        // nodes wide has room for. Measured without it: a body that had to go
        // back 13 nodes circled against the outer wall until the run ended.
        //
        // And beside, from standing: a wheel turns nothing at rest, so a kart
        // nose to a wall with its point off to one side pressed the throttle
        // into the wall for the rest of the run -- seen with a body 1.6 wide
        // just out of the track's bay, in the same place in two frames four
        // seconds apart.
        in.move_forward = -1.0f;
        in.move_right   = -std::clamp(err * 2.5f, -1.0f, 1.0f);
        return;
    }

    in.move_right = std::clamp(err * 2.5f, -1.0f, 1.0f);

    // The speed the wheel can take what is left of the bend at, and the speed
    // that stops in the distance left. The wheel as it turns at this speed, so a
    // body whose grip fades brakes for the bend; never under 4 m/s, where the
    // wheel of a body standing still would ask for no speed at all.
    const float rate  = drive_turn_rate(rules, std::max(std::fabs(player.drive_speed), 4.0f));
    const float bend  = std::fabs(err) > 0.2f ? rate * 4.0f / std::fabs(err) : rules.sprint;
    const float want  = std::min({rules.sprint, bend, 2.0f + dist * 1.5f});
    in.move_forward   = player.drive_speed < want          ? 1.0f
                        : player.drive_speed > want + 1.5f ? -1.0f
                                                           : 0.0f;

    // Pointing straight at a point far off: shift, the nitro, as a player
    // does on a straight.
    if (g_walk_boost && std::fabs(err) < 0.1f && dist > 30.0f) {
        in.move_forward = 1.0f;
        in.sprint       = true;
    }
}

// Where a link leads: its own address, or -- a room of the server it came
// from, which has none (protocol 56) -- the address that server was dialled
// at. `spec` is how this client names a world: the address and the room,
// since two rooms of one server share an address.
struct LinkDest {
    std::string host;
    uint16_t    port = 0;
    std::string room;
    std::string spec;
};

LinkDest link_dest(const net::ClientSession &from, const std::string &host, uint16_t port,
                   const std::string &room)
{
    LinkDest d;
    d.host = host.empty() ? from.host() : host;
    d.port = host.empty() ? from.port() : port;
    d.room = room;
    d.spec = d.host + ":" + std::to_string((unsigned)d.port) + (room.empty() ? "" : "/" + room);
    return d;
}

int run_network(Scene &s, net::ClientSessionSet &net_sessions, int self,
                const net::ClientConfig &base, const std::string &self_name, double seconds)
{
    // The player's own connection. Named by an id and fetched like any other:
    // the set is what holds every world this process is looking at, and the one
    // the player is standing in has no privileged slot in it.
    // A POINTER and not a reference: crossing to another server makes a
    // different connection the player's, and a reference could not be moved.
    int                 self_conn = self;
    net::ClientSession *conn      = net_sessions.get(self_conn);

    plat_cursor_grab(true);

    // The same spot the probe claims: the middle of the world, two blocks above
    // the plaza, which is air on every seed. It has to be air without having
    // seen the world, because no chunk arrives until a position is claimed --
    // the server streams to where the player says they are.
    Player player;
    s.player = &player;   // world code here asks after this machine's body too (core.player_pos)
    player.pos = Vec3{(float)g_world_extent * 0.5f, (float)kGroundTop + 2.0f,
                      (float)g_world_extent * 0.5f};

    bool   show_debug  = true;
    if (seconds <= 0.0) load_options(show_debug);
    s.play_arms = true;
    LookSettle look_settle;

    // The world the player has been let go in. Until the chunk UNDER them has
    // arrived they are held where they were put; see the hold below for why
    // this is a latch on a session rather than a condition on the world.
    const Session *settled = nullptr;

    long   applied     = 0;
    double last_line   = 0.0;
    int    reported_chunks = -1;

    // Blocks. `held_slot` is the hand, remembered here and sent with each click
    // rather than as a message of its own -- see net::Edit::slot.
    int  held_slot      = 0;
    long blocks_changed = 0;

    // Which PanelShow this client has already turned into a panel. See
    // adopt_panel: rebuilding it every frame would be work, and comparing the
    // panels field by field to avoid the work would be more of it.
    uint64_t panel_rev_seen = 0;
    long edits_refused  = 0;
    long edits_sent     = 0;
    long hits_sent      = 0;

    // The scripted break-and-build-it-back, as a small state machine. A gate
    // and not a demo: what it proves is that the loop closes -- the block that
    // came out of the world went into the bag, and the one that came out of the
    // bag went back into the world -- and that the two refusals a player will
    // actually meet say why.
    // How long one step waits for its answer. An edit is a reliable message and
    // the answer is another one, so this is a round trip plus retransmissions --
    // six seconds is fifteen of them at the 400 ms the bad-link gate runs at.
    // It is a budget and not a measurement: four steps at eight seconds each
    // did not leave a forty-second run enough time to also walk through a
    // portal, and the scripted walk now waits for this sequence to finish.
    constexpr double kEditAnswer = 6.0;

    // The empty-hand step is the one that measures an ABSENCE, so it cannot
    // finish when its answer arrives -- there is no answer. It waits, and that
    // is why it has a window of its own and why it goes LAST.
    //
    // Six seconds here instead would add six to a sequence the scripted walk
    // waits for, and the comment on the walk already records what re-timing
    // this sequence costs: the crossing moves, and three runs in eight came
    // back with the far window in a state the crossing checks did not expect.
    // Measured as exactly that -- bad_link went red the first time this step
    // waited kEditAnswer.
    //
    // Two seconds is five round trips at the 400 ms the bad-link gate runs at,
    // and a refusal is a reliable message the server sends immediately. Too
    // short would be the worse failure: concluding "no refusal came" before one
    // could have.
    constexpr double kUseSilence = 2.0;

    enum class EditStep { Idle, Breaking, Placing, Reach, EmptyHand, Done };

    // Which step ran out of time, in the log. Guessing at which of four steps
    // ate a budget is what this line exists to stop.
    auto gave_up = [](EditStep step, double waited) {
        static const char *name[] = {"finding a block", "breaking", "building it back",
                                     "the empty hand", "the reach"};
        const int i = (int)step;
        std::printf("the hands gave up on %s after %.1f s\n",
                    (i >= 0 && i < 5) ? name[i] : "?", waited);
    };
    EditStep edit_step   = EditStep::Idle;
    int      edit_x = 0, edit_y = 0, edit_z = 0;
    BlockId  edit_was     = kAir;
    double   edit_started = 0.0;
    double   edit_swung   = 0.0;   // when this sequence last swung at the cell
    bool     broke_ok = false, placed_back = false;
    bool     empty_hand_used = false, reach_refused = false;
    bool     hands_ran = false;   // the sequence below sent its first break
    uint32_t carried_after_break = 0;

    // And what it held when the sequence was over.
    //
    // Sampled while the sequence runs rather than read at the end of the run,
    // which is what it used to be: with --roam the player goes on breaking
    // blocks for another twenty-five minutes, so a check about the sequence's
    // own economy was reading a bag that a different scenario had filled. It
    // went red for exactly that, and the check was right about its own claim
    // and wrong about where to look for it.
    uint32_t carried_when_done = 0;
    std::string last_refusal;

    // The version of the openings this client has drawn. Compared rather than
    // taken, because the openings are state and not an event: a client that
    // joins a world which already has a pair is at version 0 and everybody
    // else is at 7, and the same comparison covers both.
    uint32_t drawn_portals = 0;

    // The worlds behind the mouths, when this server's openings lead to other
    // ones -- one for each opening that does, so a sandbox can show an arena
    // through one and a track through the other. Opened on demand from what the
    // server says, rather than from a flag: a portal that names a destination
    // is the client's instruction to go and look at it.
    //
    // Per mouth: which session it looks through, and what has been built and
    // asked for on its behalf. Which world that session streams into is per
    // CONNECTION (conn_world below), because two mouths -- or a mouth and the
    // world just left -- can be the same session.
    struct Mouth {
        int         conn  = net::ClientSessionSet::kNoSession;
        std::string spec;              // host:port, as the link names it
        bool        tried = false;     // a session was opened for it, or could not be

        // The versions of the two ends the link was built from. Rebuilt when
        // EITHER side moves.
        uint32_t drawn_far   = 0;
        uint32_t linked_near = 0;

        // The ticket for the world through this opening, and the one-shot
        // state of having asked for it. A world that issues none is the
        // ordinary case, so "settled" means answered OR given up on, not
        // "granted".
        bool              ticket_asked    = false;
        bool              ticket_settled  = false;
        double            ticket_asked_at = 0.0;
        bool              have_ticket     = false;
        net::PortalTicket held_ticket;
        double            ticket_got_at   = 0.0;     // by this machine's clock
        bool              ticket_renew    = false;   // ask again: see the mouths' loop

        // What has already been said about the far session being unreachable,
        // so a dead neighbour costs a handful of lines rather than one per
        // attempt.
        double said_retry_delay = -1.0;
        bool   said_gave_up     = false;
        int    said_far_flags   = -1;
    };
    Mouth mouths[net::kMaxMouths];

    // How many times an atlas was rebuilt because media arrived. For the
    // gate: zero of them with files still missing is a world drawn from a
    // cache that happened to be warm, which is a different claim.
    //
    // Which media the active world's atlas and model store were built from
    // are per connection (conn_atlas_built, conn_models_read below): both live
    // in that world's Session and outlive a crossing, and one key for "the
    // player's connection" rebuilt them on every way back -- 107-151 ms of
    // models and 17-19 ms of atlas in one frame, each time the player walked
    // back into the sandbox (RX 5700, Release).
    int         world_code_conn      = -1;    // the world whose code this session runs (OPEN-49)
    double      world_code_stepped   = 0.0;
    int         media_atlas_rebuilds = 0;

    // And which connection this world's PACK was kept from. A world serves its
    // pack as a pack as well as as an atlas -- see core/media/texture_pack.h --
    // so a player can put it on another world, or take it off. Keyed like the
    // two above.
    int         pack_conn            = -1;
    unsigned    pack_media_version   = 0;

    // Which server each connection is to. Needed the moment a crossing swaps
    // the roles: the world just left becomes the one behind the opening, and
    // the code that opens a mouth has to recognise the connection it already
    // has rather than making a second one to the same place.
    std::vector<std::string> conn_spec((size_t)net_sessions.count());
    {
        const net::ClientSession *self = net_sessions.get(self_conn);
        const std::string         room = self ? self->room() : base.room;
        conn_spec[(size_t)self_conn] =
            base.host + ":" + std::to_string((unsigned)base.port) + (room.empty() ? "" : "/" + room);
    }

    // And, per connection, which of this client's worlds it streams into (-1
    // for none this loop knows of), whether that world's renderer is up, and
    // which of its media the atlas was built from.
    std::vector<int>      conn_world((size_t)net_sessions.count(), -1);
    std::vector<char>     conn_meshed((size_t)net_sessions.count(), 0);
    std::vector<unsigned> conn_media_version((size_t)net_sessions.count(), 0);
    // The media version + 1 the world's atlas (with the server's item icons)
    // was last built from as the player's world; 0 never. And how far its
    // model store has been read, a few a frame (see where it is read).
    struct ModelsRead {
        unsigned    version = 0;   // media version + 1, 0 never
        size_t      next    = 0;   // the next id to read, minus one
        int         from_wire = 0, from_disk = 0;
        std::string told;
    };
    std::vector<unsigned>   conn_atlas_built((size_t)net_sessions.count(), 0);
    std::vector<ModelsRead> conn_models_read((size_t)net_sessions.count());
    conn_world[(size_t)self_conn] = s.active;

    const auto conn_grew = [&]() {
        const size_t n = (size_t)net_sessions.count();
        conn_spec.resize(n);
        conn_world.resize(n, -1);
        conn_meshed.resize(n, 0);
        conn_media_version.resize(n, 0);
        conn_atlas_built.resize(n, 0);
        conn_models_read.resize(n);
    };
    const auto world_of = [&](int id) {
        return (id >= 0 && (size_t)id < conn_world.size()) ? conn_world[(size_t)id] : -1;
    };
    // Still of use as a window: in, trying again, or not settled yet.
    const auto alive = [&](int id) {
        net::ClientSession *w = net_sessions.get(id);
        return w != nullptr &&
               (w->playing() || w->retrying() || !net::client_join_settled(w->state()));
    };

    // A crossing this client has asked for and not been answered yet.
    bool   crossing_pending = false;
    // Where the feet were the last time the mouths were tested, and in which
    // world: the trigger is walked along the step from there (trigger_along).
    Vec3   trigger_from{};
    int    trigger_world = -1;
    double crossing_sent    = 0.0;
    bool   seed_after_cross = false;
    int    crossed_once     = 0;
    int    crossing_mouth   = 0;   // which of this world's mouths it was asked through
    int    left_conn        = net::ClientSessionSet::kNoSession;   // the world last left
    double arrived_when     = -1.0;   // when the last crossing came out, for --cross-back
    size_t walk_point       = 0;      // the next of --walk-path's points
    double use_sent         = -1.0;   // when --use-at's click went, -1 before

    // And the other answer to it. `denial_delay` is what the gate reads: a
    // refusal that arrives is a number of milliseconds. `grant_delay` is the
    // same number for the answer that says yes. `crossings_lost` counts the
    // requests whose world went away before it answered -- the only way a
    // request ends without an answer (see where it is counted).
    int    crossings_denied = 0;
    int    crossings_lost   = 0;
    double denial_delay     = -1.0;
    double grant_delay      = -1.0;

    // Walking into a mouth, and what came of it. Three numbers because there
    // are three outcomes and only two of them used to exist: the crossing that
    // was asked for, the one that was refused, and the one that was never asked
    // because the world on the other side had already said there was no room.
    int    mouth_triggers      = 0;
    int    crossings_attempted = 0;
    int    crossings_held_back = 0;
    double held_back_at        = -1.0;

    // And the fourth: held not because the answer was no but because there was
    // no world to arrive in yet. Counted apart from the refusals -- one is a
    // decision by the world on the other side, this is this client waiting for
    // a window it opened to finish arriving.
    int    crossings_waited_for_ground = 0;
    double waited_at                   = -1.0;
    bool   ground_was_ready            = false;

    // Times a mouth named a server this client already had open, and the window
    // had to be MOVED onto that session. Usually zero even when the recognition
    // is doing the work: the common case is the window already pointing at the
    // right session and the key merely agreeing where the address did not. The
    // number that shows the recognition working is how many other worlds got a
    // window at all -- see tests/bad_link.sh.
    int    windows_reused              = 0;

    // Which of the arrived world's openings the link named. Kept because the
    // gate below checks the arrival against the RIGHT hole: with two openings
    // twenty nodes apart, "near an opening" is not the same claim as "near the
    // one the link named".
    uint32_t crossed_out_of = 0;

    // Where the image inside the doorway was showing, on the frame the player
    // walked into it. The property nothing measured until a portal was caught
    // showing one opening and arriving out of another: what you looked at is
    // where you come out.
    Vec3 shown_before_cross{};
    bool shown_taken = false;

    // The redirect fallback. A crossing done by reconnecting, for the case
    // where there is no window already open onto the far world -- and the
    // numbers that say what it cost, which is the whole point of calling it a
    // fallback: the seamless path streams nothing on arrival and this one
    // streams a world.
    int    redirects_ignored   = 0;
    int    redirects_taken     = 0;
    int    fallback_conn       = net::ClientSessionSet::kNoSession;
    int    fallback_world      = -1;
    bool   fallback_pending    = false;
    bool   fallback_asked      = false;
    double fallback_started    = 0.0;
    int    fallback_chunks     = -1;
    double fallback_seconds    = -1.0;
    uint32_t        fallback_portal = 0;
    net::PortalDesc fallback_from;
    Vec3            fallback_pos{}, fallback_look{}, fallback_vel{};
    std::string     fallback_spec;

    // The redirect's ticket, kept for the second half of the journey. Looking
    // in and walking in are different permissions (M6_FINDINGS 11), so the
    // ticket that got the connection admitted has to be presented again for
    // the crossing -- measured by its absence: "this world only takes players
    // it has been vouched for", with the ticket sitting unused in the hello.
    bool             fallback_has_ticket = false;
    net::PortalTicket fallback_ticket;

    // Whether this run ever stood in a world whose opening led somewhere else.
    // What makes the fallback gate below a check rather than a condition: with
    // no windows and a mouth in front of the player, a crossing HAS to happen,
    // and a run where none did must go red rather than skip the section.
    // Measured: with the server's redirect deleted the whole block vanished and
    // the run passed.
    bool saw_a_mouth = false;

    // Whether the scripted walk is getting anywhere, sampled twice a second.
    Vec3   walk_was{};
    double walk_checked = 0.0;
    bool   walk_stuck   = false;

    // The long run, for M7's exit test: thirty minutes from the start without a
    // crash and without touching the console.
    //
    // Thirty minutes of a client standing still is a test of a socket and a
    // leak. What the milestone asks about is a GAME, so with --roam the player
    // keeps walking, keeps breaking a block and building one back, and swings
    // at whatever body is in front of it -- through the same messages a click
    // sends, because what is under test is the loop closing over and over and
    // not a second path that only the long run uses.
    //
    // Off by default and named on the command line rather than implied by a
    // long --play-seconds: every other gate in this suite plays for a few
    // seconds and expects the player to stay where the scenario put them.
    struct Roam {
        // Where the walking started, and how far it may get from there.
        //
        // A finite world has an EDGE, and a circle of fifty-odd nodes centred
        // wherever the player happened to be standing walks off it. Measured,
        // and it is what made the first thirty minute run worthless: the player
        // left the map at about eight minutes and spent the other twenty-two
        // falling, at y = -130822 by the end, corrected every six seconds for
        // falling too fast and never landing. Forty nodes keeps a 128 world
        // under its feet from anywhere the spawn can be.
        Vec3    home{};
        bool    have_home = false;

        float   heading = 0.0f;
        double  looked  = 0.0;
        Vec3    was{};
        bool    stuck   = false;

        double  next_edit = 5.0;
        bool    holding   = false;   // a block is out of the world and in the bag
        double  next_hit  = 0.0;

        long    broke = 0, built = 0, swung = 0, turns = 0;
        double  reported     = 0.0;
        long    frames_then  = 0;
        size_t  rss_first    = 0;
        size_t  rss_worst    = 0;
    } roam;

    // What the gate reads afterwards about a crossing between two servers.
    int    chunks_before_arriving = -1;
    int    chunks_after_arriving  = -1;
    Vec3   arrived_at{};

    // The gate below places one opening from this client, exactly the way a
    // click does. Kept here rather than in the loopback probe because the half
    // of this that only exists here is what happens AFTER the answer arrives:
    // three copies of the opening in the renderer's own types, which no
    // headless test links against.
    bool     placement_sent   = false;
    int      placed_x = 0, placed_y = 0, placed_z = 0;
    uint32_t version_before_placement = 0;

    // And the case that used to bury the player: an opening on the block under
    // their own feet. Sent second, once the first has been answered, because
    // what it measures is a crossing and a crossing needs both ends.
    bool underfoot_sent = false;
    long teleports_before_underfoot = 0;
    long applied_at_underfoot = 0;

    // And how many there had been while the player was standing still, for a
    // world with no openings -- see stood_sampled. The claim is about a player
    // who did nothing; counting to the end of a run counts a walk.
    long applied_when_stood = 0;
    long applied_at_crossing  = -1;

    // Where the player was standing before any of this, and how many
    // corrections they had by then. The checks about the streamed world being a
    // world are about a player standing on it, and after the floor experiment
    // the player is somewhere else entirely -- deliberately, since falling
    // through the portal is what is being measured.
    Vec3 stood_at{};
    bool stood_taken = false;

    // And whether it was sampled while the scripted sequence ran, for a world
    // with no openings in it -- where the underfoot test above never happens and
    // this used to fall back to wherever the player was when the run ended.
    //
    // With --roam that is wherever the walk had got to, and the walk digs: the
    // player stands on the rim of a hole it made a second ago, the cell under
    // the middle of the box is the hole, and a check about the world having
    // ARRIVED goes red about a block the player broke themselves. Measured
    // exactly that way -- "block below is 'core:air' (NOT SOLID)" at the end of
    // a run whose every other check passed.
    bool stood_sampled = false;

    const double started  = plat_time();
    const double deadline = seconds > 0.0 ? started + seconds : 0.0;

    // --shot: the same one screenshot the single-process loop takes. Wired into
    // both loops rather than one, because a flag that is accepted and does
    // nothing is worse than one that is refused -- see --play-seconds, which
    // spent a while being ignored by exactly this loop the other way round.
    const double shot_due   = g_shot_at >= 0.0 ? plat_time() + g_shot_at : -1.0;
    bool         shot_taken = false;
    double       press_r_due = g_press_r_at >= 0.0 ? plat_time() + g_press_r_at : -1.0;
    bool         dropped  = false;
    long         frames   = 0;

    // What the screen-shaped targets were last built for, and what the window
    // has become since.
    ScreenTargets screen_targets;
    screen_targets.start();

    while (!plat_should_close()) {
        window_keys(show_debug);
        const float  dt  = plat_delta_time();
        const double now = plat_time();
        s.notice_clock   = now;

        // Which cells the server says somebody has open, and how far their lids
        // have swung by now. The bit is the world's; the swing is this
        // machine's. See advance_lids.
        {
            std::vector<Cell> open_now;
            for (const Cell &at : conn->open_cells()) open_now.push_back(at);
            advance_lids(s, dt, open_now);
            wire_lid_query(s, s.active);
            wire_model_draw(s, s.active);
        }
        if (deadline > 0.0 && now >= deadline) break;
        ++frames;

        const ProfFrameScope prof_frame;

        // Zero timeout: the frame is the clock here, not the socket. Blocking
        // even a millisecond per frame for a quiet connection would cost more
        // than the whole network layer does.
        // Every connection, and the timeout is spent once rather than once per
        // socket. See ClientSessionSet::pump.
        { diag::ProfScope _("net"); net_sessions.pump(now, 0); }

        // Which connection this process's mods talk through, refreshed every
        // frame rather than tracked at each place `conn` is reassigned:
        // crossing to another server makes a different connection the player's,
        // and a pointer set once would go on sending to the world they left.
        s.conn = conn;

        // What arrived on the mods' own channels, handed to them as an event.
        // `player` is 0: on this side the message came from the server, and
        // there is no player id here that would mean anything to a mod.
        //
        // Not before this world's code has been loaded (or refused), below. A
        // world greets a player the moment they walk in -- yafps sends its
        // counter once, on arrival -- and handed over a frame early that
        // greeting went to a world with no events declared yet and was lost:
        // measured, an arena with a crosshair and no counter for the whole
        // visit. The session keeps up to 256 meanwhile.
        if (world_code_conn == self_conn) {
            for (const auto &said : conn->mod_messages()) {
                script_field fields[3] = {
                    {"player", SCRIPT_V_NUMBER, 0.0, nullptr, 0},
                    {"mod", SCRIPT_V_STRING, 0.0, said.mod.c_str(), 0},
                    {"text", SCRIPT_V_STRING, 0.0, said.text.c_str(), 0},
                };
                bool        ignored = false;
                std::string say_err;
                if (!s.act().mods.events().emit("core:net_message", fields, 3, &ignored, say_err,
                                                &said.mod))
                    std::fprintf(stderr, "emit core:net_message: %s\n", say_err.c_str());
            }
            conn->clear_mod_messages();
        }

        if (conn->state() != net::ClientState::Playing) {
            std::printf("disconnected: %s%s%s\n", net::client_state_text(conn->state()),
                        conn->error().empty() ? "" : " -- ", conn->error().c_str());
            // Put out by the server (a kick, a ban): back to the title, saying
            // why, rather than a window that just closes.
            if (conn->state() == net::ClientState::Refused) {
                g_to_title = true;
#ifdef _WIN32
                _putenv_s("CNP_TITLE_ERROR", conn->error().c_str());
#else
                ::setenv("CNP_TITLE_ERROR", conn->error().c_str(), 1);
#endif
                // Restarting: the title joins it again (TitleMenu::rejoin).
                if (conn->refusal().reason == net::RefuseReason::Restarting && !g_rejoin_account.empty()) {
#ifdef _WIN32
                    _putenv_s("CNP_REJOIN", g_rejoin_account.c_str());
#else
                    ::setenv("CNP_REJOIN", g_rejoin_account.c_str(), 1);
#endif
                }
            }
            dropped = true;
            break;
        }

        // What time it is, per world, mirrored from whoever owns that world.
        // Nothing here advances a clock between messages: the sun moves by a
        // tenth of a light level between one telling and the next, and a second
        // clock on this side to smooth that over would be a second clock to
        // disagree with. The far world's hour comes from ITS server, which may
        // be a different machine at a different time of day -- which is the
        // whole reason the clock sits on the session and not on the process.
        // The wind is the exception: ClientSession::wind runs it on between
        // tellings, or the clouds would jump once a second.
        if (conn->has_time()) {
            s.act().time_of_day = conn->time_of_day();
            s.act().wind        = conn->wind();
        }
        for (const Mouth &mo : mouths) {
            const int                 w   = world_of(mo.conn);
            const net::ClientSession *far = net_sessions.get(mo.conn);
            if (w >= 0 && far != nullptr && far->has_time()) {
                s.at(w).time_of_day = far->time_of_day();
                s.at(w).wind        = far->wind();
            }
        }

        // And each world's own sky, if its server gave it one (core.world_sky).
        for (int id = 0; id < net_sessions.count(); ++id) {
            const int                 w   = world_of(id);
            const net::ClientSession *one = net_sessions.get(id);
            if (w < 0 || one == nullptr || !one->playing()) continue;
            Session &at  = s.at(w);
            at.sky_fixed = one->welcome().has_sky;
            for (int c = 0; c < 3; ++c) at.sky_rgb[c] = (float)one->welcome().sky[c] / 255.0f;
            at.storm = one->welcome().has_storm;
            for (int c = 0; c < 3; ++c) at.storm_rgb[c] = (float)one->welcome().storm[c] / 255.0f;
        }

        // The atlas for the world the player is standing in, once everything
        // that world offered has arrived. Keyed by which connection it was
        // built from, not by a flag: a crossing makes a different connection
        // the player's, and an atlas built from the world they left is a world
        // drawn with somebody else's pictures.
        if (conn->media_complete() &&
            conn_atlas_built[(size_t)self_conn] != conn->media_version() + 1) {
            // Into the SERVER's item table: it is the one the hotbar draws
            // (s.hotbar.items), and icons written into this process's own
            // table left every tool of a connected world a word.
            const double atlas_from = plat_time();
            s.act().world_render.rebuild_atlas(s.act().world, s.act().registry, world_media(conn), &conn->items());
            std::printf("atlas: %zu blocks, in %.0f ms\n", s.act().registry.size(),
                        (plat_time() - atlas_from) * 1000.0);
            conn_atlas_built[(size_t)self_conn] = conn->media_version() + 1;
            media_atlas_rebuilds += 1;
        }

        // And this world's models, keyed by the same connection for the same
        // reason: a model id is a position in ONE world's table, so the store
        // is re-read when the world under the player changes. A world that
        // names none clears it, which is what a body wearing 0 already draws.
        // Keyed on the media the same way the atlas above is, and not on the
        // connection alone. A model is several FILES -- the .gltf and whatever
        // it names beside itself -- so asking for them the moment the welcome
        // lands asks for parts that are still on their way: found in play
        // against a 284 KB character, where the .gltf was here, the buffer was
        // not, and the client reported that the world had not served it. A
        // small model hides this completely; the gate's is padded so it cannot.
        // This world's code (TODO.md OPEN-49): what its mods run on a player's
        // machine, served as media like any picture and loaded into this
        // world's own ModHost -- against a host that cannot write the world,
        // with no capability of any kind, and only if this player has not said
        // no. Once per world: a world's code is that world's, and a crossing is
        // another world with code of its own.
        // media_version() > 0: the offer has arrived. Before it, the offer is
        // empty and media_complete() says yes (see its comment), and this ran
        // once against nothing -- no mod loaded, nothing said, never tried
        // again: a sandbox whose offer grew past 2000 files (mods/cards' decks)
        // lost that race on a real join and every table was dead.
        if (conn->media_complete() && conn->media_version() > 0 && world_code_conn != self_conn) {
            world_code_conn    = self_conn;
            world_code_stepped = 0.0;
            Session &here      = s.act();
            if (here.mods.mods().empty()) {
                here.mods.set_host(world_code_host(make_client_host(s)));
                here.mods.set_side(ModSide::Client);
                here.mods.set_world_code(true);
                here.mods.set_vm_gone([&s, &here](script_vm *vm) {
                    s.lua_ecs.drop_vm(vm);
                    s.passes.drop_vm(vm);
                    s.hud_passes.drop_vm(vm);
                    // Its stages dead too: matched by VM address, they would
                    // run in the next VM given the same one (see the server's).
                    for (Session::LuaStage &st : here.mapgen_stages)
                        if (st.vm == vm) st.vm = nullptr;
                });
                define_core_events(s);

                std::vector<std::string> offered;
                for (const net::MediaEntry &entry : conn->media_offer()) offered.push_back(entry.name);

                std::string       refused_why;
                const bool        refused = world_code_refused_for(*conn, refused_why);
                const std::string cache =
                    (g_media_dir.empty() || g_media_dir == "-")
                        ? (std::filesystem::temp_directory_path() / "cnp_world_code").string()
                        : g_media_dir + "/world_code";
                const double      wc_from = plat_time();
                const WorldCodeReport wc =
                    world_code_load(here.mods, offered, world_media(conn), cache, refused);
                if (wc.offered > 0)
                    std::printf("world code: %d offered, %d running, in %.0f ms%s%s\n", wc.offered,
                                wc.running, (plat_time() - wc_from) * 1000.0,
                                refused ? ", refused by this player: " : "",
                                refused ? refused_why.c_str() : "");
                if (!wc.told.empty()) std::fputs(wc.told.c_str(), stdout);
            }
        }
        if (world_code_conn == self_conn && !s.act().mods.mods().empty()) {
            const double step = world_code_stepped > 0.0 ? now - world_code_stepped : 0.0;
            world_code_stepped = now;
            if (step > 0.0) s.act().mods.step(step > 0.25 ? 0.25 : step);
        }

        // Every world this client streams, not only the one under the player,
        // and a few models a frame: read all at once on the way into the
        // sandbox, its 94 took 120-335 ms of one frame the first time. A world
        // seen through an opening has them by the time anyone walks in, and its
        // bodies are drawn with them in the opening meanwhile.
        const double models_until = plat_time() + 0.003;
        for (int id = 0; id < net_sessions.count(); ++id) {
            net::ClientSession *w     = net_sessions.get(id);
            const int           world = world_of(id);
            if (!w || world < 0 || !w->playing() || !w->media_complete()) continue;

            ModelsRead                     &mr    = conn_models_read[(size_t)id];
            ModelStore                     &store = s.at(world).models;
            const std::vector<std::string> &names = w->welcome().models;
            if (mr.version != w->media_version() + 1) {
                mr = ModelsRead{w->media_version() + 1};
                store.clear();
            }
            while (mr.next < names.size() && plat_time() < models_until) {
                // The world's own first, out of the media it served: a client
                // that has never seen this game rebuilds its characters from
                // what arrived. Then the player's folder, for whatever the
                // world did not serve.
                if (!g_media_dir.empty() && g_media_dir != "-")
                    mr.from_wire += store.load_from_media(names, world_media(w),
                                                          g_media_dir + "/models", mr.told,
                                                          mr.next, 1);
                mr.from_disk += store.fill_from_dir(names, g_models_dir, mr.told, mr.next, 1);
                if (++mr.next < names.size()) continue;
                std::printf("models: %zu named by %s, %d from it, %d from this machine, "
                            "%d refused\n",
                            names.size(), conn_spec[(size_t)id].c_str(), mr.from_wire,
                            mr.from_disk, store.refused());
                if (!mr.told.empty()) std::fputs(mr.told.c_str(), stdout);
            }
        }

        // And this world's pack, kept as a PACK.
        //
        // The atlas above was built out of the same files under the ENGINE's
        // names, which is all a world needs to be drawn right. This is the
        // other thing a player wants: the pack itself, in a folder, to put on
        // another world or to take off this one. TODO.md OPEN-25.
        //
        // Keyed on the media the way the two above are, and it needs the cache
        // for the same reason they do -- with no cache directory there is
        // nowhere to put a folder that outlives the session.
        if (conn->media_complete() && !g_media_dir.empty() && g_media_dir != "-" &&
            (pack_conn != self_conn || pack_media_version != conn->media_version())) {
            pack_conn          = self_conn;
            pack_media_version = conn->media_version();

            std::vector<std::string> offered;
            offered.reserve(conn->media_offer().size());
            for (const net::MediaEntry &e : conn->media_offer()) offered.push_back(e.name);

            std::string       why;
            const std::string folder =
                rebuild_pack_folder(offered, world_media(conn), g_media_dir + "/packs", why);

            if (!folder.empty() &&
                std::find(g_pack_dirs.begin(), g_pack_dirs.end(), folder) == g_pack_dirs.end()) {
                TexturePack got;
                std::string load_why;
                if (load_texture_pack(folder, got, load_why)) {
                    g_pack_dirs.push_back(folder);
                    std::printf("pack: this world's '%s' kept -- %s, %zu name(s), F7 to wear "
                                "it here or anywhere\n",
                                got.name.c_str(), why.c_str(), got.alias.size());
                } else {
                    std::printf("pack: this world served one and it will not load: %s\n",
                                load_why.c_str());
                }
            }
        }

        // With the bag open, ESC closes the bag and gives the cursor back to
        // the world. Anything else makes ESC mean two things at once and leaves
        // a player pressing it twice to get out of one panel. Otherwise it is
        // the pause menu's. The server is not paused -- it is not only this
        // player's -- so the menu only stops the hands.
        bool esc = plat_key_pressed(PLAT_KEY_ESC);
        // In a field, ESC lets go of the keyboard and does nothing else.
        if (esc && s.typing.on) {
            esc = false;
            stop_typing(s);
        }
        for (const std::string &line : conn->take_chat()) {
            s.chat.add(line, s.notice_clock);
            std::printf("chat: %s\n", line.c_str());
        }
        if (std::string said; chat_frame(s, look_settle, esc, said)) conn->send_chat(said);
        if (esc && s.hotbar.open) {
            esc            = false;
            s.hotbar.open  = false;
            s.hotbar.press = -1;
            plat_cursor_grab(true);
            look_settle.grabbed();

            // And the chest with it, on the server. See send_close_container.
            // One message for both, because there is one gesture: a player
            // presses escape and expects the screen to go. See the
            // CloseContainer handler in session.cpp.
            if (s.hotbar.chest_open || s.hotbar.craft_w > 2) conn->send_close_container();
        }
        // And a mod's panel: the server takes it off (ui::kPanelEscape).
        if (const ui::Panel *up = s.act().panels.topmost_for(ui::kLocalPlayer);
            esc && up != nullptr && !s.pause.up) {
            esc = false;
            conn->send_ui_click(up->name, ui::kPanelEscape, "");
        }
        if (pause_frame(s, look_settle, show_debug, esc, seconds <= 0.0)) break;
        if (plat_key_pressed(PLAT_KEY_F3))
            show_debug = !show_debug;
        if (plat_key_pressed(PLAT_KEY_F6))
            diag::prof_enable(!diag::prof_enabled());
        if (plat_key_pressed(PLAT_KEY_F2)) g_occlusion = !g_occlusion;
        if (plat_key_pressed(PLAT_KEY_F5))
            s.blit_link = (s.blit_link + 1) % 3;
        if (plat_key_pressed(PLAT_KEY_V) && !iso_engaged(s.act()))
            set_view_distance(s, s.act().view_distance > 0.0f ? 0.0f : kThirdPersonDistance);
        // A track piece in hand, turned before it is put down (the preview
        // shows it; the turn goes with the placement, net::Edit::turn).
        if (plat_key_pressed(PLAT_KEY_Z)) s.piece_turn = (uint8_t)((s.piece_turn + 3) & 3);
        if (plat_key_pressed(PLAT_KEY_X)) s.piece_turn = (uint8_t)((s.piece_turn + 1) & 3);
        s.nitro = s.act().movement.drives() ? player.nitro : -1.0f;

        PlayerInput in;
        if (plat_cursor_grabbed() && seconds <= 0.0) {
            in.move_forward = (plat_key_down(PLAT_KEY_W) ? 1.0f : 0.0f) -
                              (plat_key_down(PLAT_KEY_S) ? 1.0f : 0.0f);
            in.move_right   = (plat_key_down(PLAT_KEY_D) ? 1.0f : 0.0f) -
                              (plat_key_down(PLAT_KEY_A) ? 1.0f : 0.0f);
            in.jump       = plat_key_down(PLAT_KEY_SPACE);
            in.sprint     = plat_key_down(PLAT_KEY_SHIFT);
            in.descend    = plat_key_down(PLAT_KEY_CTRL);
            in.toggle_fly = plat_key_pressed(PLAT_KEY_F);
            plat_mouse_delta(&in.look_dx, &in.look_dy);

            // The first deltas after a grab are not movements. See LookSettle.
            look_settle.apply(in);
            if (in.look_dx != 0.0f || in.look_dy != 0.0f) s.look_settling = false;
            in.look_dx *= g_look_scale;
            in.look_dy *= g_look_scale;

            // Screen axes while the horizon is tipped, as in the local loop.
            look_apply_roll(in, s.roll);
            steer_input(s, player, in);
        }

        // The scripted walk, for the gate. Nobody holds a key down in a
        // headless run, and a crossing nobody walks into is a crossing that
        // never happens.
        //
        // Driven through the INPUT and not by writing a position, which the
        // first version did: a straight line at a fixed height walks through
        // hills, and the server refuses every claim inside one. Turning towards
        // the opening and pressing forward is what a player does, and it gets
        // the collision, the step-up and the gravity for free.
        // With --no-passive there is no window and therefore no drawn portal,
        // and the walk still has to happen: the opening is the SERVER's, the
        // trigger is computed from the position claims, and what comes back is
        // a Redirect instead of an image.
        //
        // Which mouth: in the world the run joined, the one --mouths names for
        // this visit; in a world walked into, its first, which is the way home.
        const bool   at_home    = crossed_once % 2 == 0;
        const size_t visit      = (size_t)(crossed_once / 2);
        int          walk_mouth = at_home && visit < g_walk_mouths.size() ? g_walk_mouths[visit] : 0;
        if (walk_mouth == 1 && !conn->portals().away[1].leads_away()) walk_mouth = 0;
        const bool walk_linked =
            walk_mouth == 0 ? s.act().links_elsewhere() : s.act().two_mouths();

        const bool walking_into_a_mouth =
            (walk_linked && s.act().portals) ||
            (g_no_passive && conn->portals().leads_away() && conn->portals().has(walk_mouth));

        // And not before the hands have finished. The break-and-build-it-back
        // below is about the world the player JOINED, and a crossing halfway
        // through it moves the player to another server with another bag --
        // measured on a 400 ms link losing 20%, where the sequence was still
        // waiting for an answer when the walk started and every check after it
        // was about the wrong world. Two scenarios in one run have to be
        // sequential, or they are one scenario nobody wrote.
        const bool walk_owed =
            at_home ? (visit < g_walk_mouths.size() &&
                       (crossed_once == 0 ||
                        (g_cross_back >= 0.0 && now - arrived_when >= g_cross_back)))
                    : (g_cross_back >= 0.0 && walk_point >= g_walk_path.size() &&
                       now - arrived_when >= g_cross_back);
        if (seconds > 0.0 && walking_into_a_mouth && walk_owed && !crossing_pending &&
            !fallback_pending) {
            // Two phases, and both of them were wrong once.
            //
            // Walking to the CENTRE stops with the feet a tenth of a node in
            // front of the plane: close enough to look like an arrival, and the
            // trigger never fires because nobody crossed anything. Measured:
            // 40.62 44.58 against a plane at 44.50, no crossing in fifteen
            // seconds.
            //
            // Aiming straight at a point PAST the opening is worse: the
            // approach is a diagonal, so the line crosses the plane before it
            // reaches the opening. Measured: the plane crossed at x 42.3 with
            // the opening spanning 40.0 to 41.0 -- through the wall beside it,
            // which is not a portal being walked into.
            //
            // So: to the centre first, and only from close in, straight
            // through. Which is what a person does.
            //
            // And two phases before both. BEHIND the mouth -- the plaza's
            // second opening faces away from where the first one puts an
            // arrival -- round to its front, two nodes out to the side the
            // player is already on, so the way round never passes through the
            // opening from the back. In front but off its axis, onto the axis
            // first, a little nearer than the player already is so a kart
            // meets it at a slant rather than broadside: a kart aimed at the
            // centre from the side passed the plane beside a one-node opening
            // and drove round into the bay's wall.
            const Portal &mouth  = mouth_portal(s.act(), std::clamp(walk_mouth, 0, net::kMaxMouths - 1));
            const Vec3    centre = mouth.center;
            const Vec3    to_centre{centre.x - player.pos.x, 0.0f, centre.z - player.pos.z};
            const Vec3    rel    = player.pos - centre;
            const float   along  = dot(rel, mouth.normal());
            const float   aside  = dot(rel, mouth.right());
            const float   side   = aside < 0.0f ? -1.0f : 1.0f;
            const Vec3    target =
                along < 0.0f               ? centre + mouth.normal() * 3.0f + mouth.right() * (2.0f * side)
                : std::fabs(aside) > 0.35f ? centre + mouth.normal() * std::max(along * 0.6f, 2.5f)
                : length(to_centre) > 0.6f ? centre
                                           : centre - mouth.normal() * 1.5f;
            const Vec3    gap{target.x - player.pos.x, 0.0f, target.z - player.pos.z};
            if (length(gap) > 0.15f) {
                player.pitch = 0.0f;
                if (s.act().movement.drives()) {
                    drive_towards(player, s.act().movement, player.pos + gap, in);
                } else {
                    player.yaw      = std::atan2(gap.x, -gap.z);
                    in.move_forward = 1.0f;
                    in.sprint       = true;
                }

                // Jump when nothing is happening. The ground between the spawn
                // and the opening has hills in it, and pressing forward into
                // one is a walk that never arrives: one run in three ended five
                // nodes short with eighteen seconds spent. A person would have
                // jumped, so this does.
                if (now - walk_checked > 0.5) {
                    const Vec3 moved{player.pos.x - walk_was.x, 0.0f, player.pos.z - walk_was.z};
                    walk_stuck   = length(moved) < 0.25f;
                    walk_was     = player.pos;
                    walk_checked = now;
                }
                // Not in a body that drives: in the air it keeps its velocity and
                // does not steer, and a kart turning slowly out of the track's
                // bay counted as stuck and hopped on the spot for the rest of the
                // run -- 21.1 high over air at the end of two runs of 100 s.
                if (walk_stuck && !s.act().movement.drives()) in.jump = true;
            }
        }

        // --walk-path: one point after another, then stand. Not while a walk
        // into a mouth is owed, and in the world the run joined only once the
        // hands have finished, for the reason the roam below waits for them.
        const bool walk_free = seconds > 0.0 && conn->playing() && !crossing_pending &&
                               !fallback_pending && !(walking_into_a_mouth && walk_owed) &&
                               (edit_step == EditStep::Done || conn->portals().leads_away());

        // --use-at first: up to the block, and one right click on it from
        // beside it, on the face towards the eye -- the Edit a real click sends.
        if (walk_free && g_use_at && use_sent < 0.0) {
            const Vec3 block{g_use_x + 0.5f, g_use_y + 0.5f, g_use_z + 0.5f};
            const Vec3 gap{block.x - player.pos.x, 0.0f, block.z - player.pos.z};
            if (length(gap) > 1.8f) {
                player.pitch    = 0.0f;
                player.yaw      = std::atan2(gap.x, -gap.z);
                in.move_forward = 1.0f;
            } else {
                const Vec3 d = player.eye() - block;
                const float ax = std::fabs(d.x), ay = std::fabs(d.y), az = std::fabs(d.z);
                const int   nx = ax >= ay && ax >= az ? (d.x < 0.0f ? -1 : 1) : 0;
                const int   nz = nx == 0 && az >= ay ? (d.z < 0.0f ? -1 : 1) : 0;
                const int   ny = nx == 0 && nz == 0 ? (d.y < 0.0f ? -1 : 1) : 0;
                if (conn->send_edit(g_use_x, g_use_y, g_use_z, nx, ny, nz, false, held_slot)) {
                    ++edits_sent;
                    use_sent = now;
                    std::printf("used the block at %d %d %d from %.2f %.2f %.2f, %.1f s in\n",
                                g_use_x, g_use_y, g_use_z, (double)player.pos.x,
                                (double)player.pos.y, (double)player.pos.z, now - started);
                }
            }
        }
        // And what the click did arrives before the walk goes on: a body that
        // walked off on foot in the moment it became a kart is a run about the
        // moment, not about the lap.
        const bool used = !g_use_at || (use_sent >= 0.0 && (s.act().movement.drives() ||
                                                            now - use_sent > 2.0));

        if (walk_free && used && walk_point < g_walk_path.size()) {
            const Vec3 &to = g_walk_path[walk_point];
            const Vec3  gap{to.x - player.pos.x, 0.0f, to.z - player.pos.z};
            // Wider for a body driving fast: one that misses a point by two
            // nodes at speed has to turn round for it, which is a lap's worth of
            // seconds spent on a test's bookkeeping. Not while it manoeuvres:
            // three nodes counted a body still inside the bay as out on the
            // road, and it turned into the bay's wall.
            const bool fast = s.act().movement.drives() && player.drive_speed > 8.0f;
            if (length(gap) < (fast ? 3.0f : 1.5f)) {
                // Every point said, with where and when: a walk that stops
                // somewhere says where, instead of ending the run in silence.
                std::printf("walked to point %zu of %zu at %.2f %.2f %.2f, %.1f s in\n",
                            walk_point + 1, g_walk_path.size(), (double)player.pos.x,
                            (double)player.pos.y, (double)player.pos.z, now - started);
                if (++walk_point == g_walk_path.size())
                    std::printf("walked the last of %zu point(s), standing at %.2f %.2f %.2f\n",
                                g_walk_path.size(), (double)player.pos.x, (double)player.pos.y,
                                (double)player.pos.z);
            } else {
                player.pitch = 0.0f;
                if (s.act().movement.drives()) {
                    drive_towards(player, s.act().movement, player.pos + gap, in);
                } else {
                    player.yaw      = std::atan2(gap.x, -gap.z);
                    in.move_forward = 1.0f;
                    in.sprint       = true;
                }
                if (now - walk_checked > 0.5) {
                    const Vec3 moved{player.pos.x - walk_was.x, 0.0f, player.pos.z - walk_was.z};
                    walk_stuck   = length(moved) < 0.25f;
                    walk_was     = player.pos;
                    walk_checked = now;
                }
                // Not in a body that drives: in the air it keeps its velocity and
                // does not steer, and a kart turning slowly out of the track's
                // bay counted as stuck and hopped on the spot for the rest of the
                // run -- 21.1 high over air at the end of two runs of 100 s.
                if (walk_stuck && !s.act().movement.drives()) in.jump = true;
            }
        }

        // And the long walk. A slow turn with forward held, which is a circle:
        // the same shape the M1 soak flies, and for the same reason -- a player
        // who stands still streams one lucky corner of the world and meshes it
        // once.
        // Not before the scripted break-and-build-it-back has finished. That
        // sequence checks the bag is empty at the end, and a roam that has been
        // breaking blocks all along leaves it holding two -- measured, red on
        // exactly that line. Two scenarios in one run are one scenario nobody
        // wrote, and this file already says so about the crossing.
        // `seconds > 0` as well, said rather than relied on: the scripted
        // sequence only runs in a timed run, so edit_step never reaches Done in
        // an interactive one and the roam would never start -- which is the
        // right behaviour arrived at by accident, and an accident is a thing
        // that stops being true when somebody moves a line.
        if (g_roam && seconds > 0.0 && conn->playing() && edit_step == EditStep::Done) {
            // Something to go and meet, if there is one within sight. A circuit
            // that never passes a creature is a long run that never touches the
            // half of M7 with creatures in it -- measured, on the first version
            // of this: seventy seconds of circling, not one body ever in front
            // of the player, not one swing sent.
            // Anything this client has been told about, not just what is
            // nearby: the interest radius already decides what reaches it, and
            // a shorter leash here made a hundred and fifty seconds pass with
            // eight creatures in the world and not one swing. A circle of
            // fifty-odd nodes at walking pace does not happen to cross a circle
            // of five in the middle.
            const net::RemotePlayer *nearest = nullptr;
            float                    nearest_d2 = 128.0f * 128.0f;
            for (const net::RemotePlayer &p : conn->players()) {
                if (p.kind == net::EntityKind::Drop) continue;
                const float dx = p.x - player.pos.x;
                const float dz = p.z - player.pos.z;
                const float d2 = dx * dx + dz * dz;
                if (d2 < nearest_d2) {
                    nearest_d2 = d2;
                    nearest    = &p;
                }
            }

            if (!roam.have_home) {
                roam.home      = player.pos;
                roam.have_home = true;
            }

            // Home first, whatever else is going on: a player who has wandered
            // to the edge of the map has nothing to chase out there and no
            // ground to come back to once they step off it.
            const Vec3  from_home{player.pos.x - roam.home.x, 0.0f, player.pos.z - roam.home.z};
            const bool  too_far = length(from_home) > 40.0f;

            if (too_far) {
                roam.heading = std::atan2(roam.home.x - player.pos.x,
                                          -(roam.home.z - player.pos.z));
                player.pitch = -0.35f;
            } else if (nearest != nullptr) {
                roam.heading = std::atan2(nearest->x - player.pos.x,
                                          -(nearest->z - player.pos.z));

                // And DOWN at it. A creature is 1.4 tall and an eye is at 1.62,
                // so a level ray passes over the head of anything standing on
                // the same ground -- measured as a hundred and fifty seconds of
                // walking straight at eight creatures with not one swing sent.
                const float dy =
                    (nearest->y + ecs::kMobHeight * 0.5f) - (player.pos.y + player.eye_height);
                const float flat = std::sqrt(nearest_d2);
                player.pitch = std::atan2(dy, flat > 0.001f ? flat : 0.001f);
            } else {
                roam.heading += (float)dt * 0.15f;

                // Eyes a little down, or the ray goes over open ground and the
                // hands never find anything: the same seventy seconds produced
                // three edits where the cadence asks for eight. From an eye at
                // 1.62 this puts the ground about four nodes ahead, inside the
                // six the arm reaches.
                player.pitch = -0.35f;
            }
            player.yaw      = roam.heading;
            in.move_forward = 1.0f;

            // Against something: a person would jump and then turn away, so
            // this does. Half a second is the crossing walk's own sampling
            // interval, kept rather than chosen again.
            if (now - roam.looked > 0.5) {
                const Vec3 moved{player.pos.x - roam.was.x, 0.0f, player.pos.z - roam.was.z};
                roam.stuck  = length(moved) < 0.25f;
                roam.was    = player.pos;
                roam.looked = now;
                if (roam.stuck) {
                    roam.heading += 1.2f;
                    ++roam.turns;
                }
            }
            if (roam.stuck) in.jump = true;
        }

        update_floor_shift(s, player, dt);
        update_funnel(s, player, dt);
        settle_roll(s, dt);
        settle_look(s, player, dt);

        // Where the player was before this frame moved them, for the hold
        // below: holding means NOT FALLING, and not being put back at a height
        // this loop guessed.
        const float y_before     = player.pos.y;
        const bool  was_grounded = player.on_ground && !player.vehicle;

        // What a kart's chassis did goes with the next claims, for the server
        // to drive the same kart with the same keys (TODO.md OPEN-47).
        static std::vector<DriveEvent> drive_log;

        for (const net::Sound &snd : conn->take_sounds())
            play_at(s, s.sounds.named(snd.name, world_media(conn)), Vec3{snd.x, snd.y, snd.z}, s.active);

        // Laid down or got up by the server (core.player_lie).
        if (net::Lie lie; conn->take_lie(lie)) {
            player.lying    = lie.on;
            player.lie_at   = Vec3{lie.x, lie.y, lie.z};
            player.lie_head = lie.head;
            // Where the server put the body, now: the correction that said so
            // may be applied after this frame's step, which would find the
            // body off the bed and get it straight back up.
            if (lie.on) {
                player.pos      = player.lie_at;
                player.velocity = Vec3{};
            }
        }

        net::Ride seat;
        if (conn->riding(seat)) {
            // Carried (core.ride): the body goes where the server says the seat
            // is, eased over a tick so the view does not step at its rate.
            const Vec3  to{seat.x, seat.y, seat.z};
            const float k = 1.0f - std::exp(-dt / 0.05f);
            // The mouse still turns the head: a gunner aims with it, a pilot
            // steers by it (player_update, which is skipped here, did this).
            player.yaw += in.look_dx * kLookSpeed;
            player.pitch = std::clamp(player.pitch - in.look_dy * kLookSpeed, -kMaxPitch, kMaxPitch);
            player.pos      = length(to - player.pos) > 4.0f ? to : player.pos + (to - player.pos) * k;
            player.velocity = Vec3{};
            player.on_ground = true;
            player.vehicle.reset();
        } else {
            player_update(player, s.act().world, s.act().registry, s.act().movement, in, dt,
                          &drive_log);
        }
        s.step.note(player, was_grounded, y_before);
        s.step.settle(dt);
        conn->record_drive(drive_log);
        drive_log.clear();

        // Said when the body goes into a liquid and when it comes out, so a run
        // with nobody watching can tell a swim from a walk along the shore.
        {
            static bool was_swimming = false;
            const bool  swimming     = box_in_liquid(s.act().world, s.act().registry,
                                                     body_box(player.pos, s.act().movement.width * 0.5f));
            if (swimming != was_swimming)
                std::printf("%s at %.2f %.2f %.2f\n", swimming ? "swimming" : "out of the water",
                            (double)player.pos.x, (double)player.pos.y, (double)player.pos.z);
            was_swimming = swimming;
        }
        sound_body(s, player, dt);

        // Hold position rather than falling through a world that is not there.
        // Without this the player is at terminal velocity by the time the
        // ground turns up, and the first thing they see is the inside of the
        // plaza.
        //
        // The chunk UNDER the player, and not `chunk_count() == 0`, which is
        // what this waited for first. That is a weaker thing to wait for: the
        // first chunk to arrive is not necessarily the one being stood on, and
        // the player falls through the gap between the two. On loopback the gap
        // is nothing; on a link with loss in it, it is long enough. Measured
        // with mods/ore in the shipped set -- a speckled world compresses worse
        // and streams slower -- where `bad_link` went from about one red in
        // three to six in six, the player at y -14264 with air under them.
        //
        // A latch on the SESSION, so it releases once and stays released. Two
        // things depend on that. A player who later walks off the edge of a
        // finite world still falls, which is TODO OPEN-16 and not this; and a
        // crossing into a world that has not streamed yet is a different
        // session, so it is held again on arrival.
        if (settled != &s.act()) {
            const int fx = (int)std::floor(player.pos.x);
            const int fy = (int)std::floor(player.pos.y) - 1;
            const int fz = (int)std::floor(player.pos.z);

            if (s.act().world.find_chunk(World::chunk_of(fx, fy, fz)) != nullptr) {
                settled = &s.act();
            } else {
                // Put back where the frame started, and not at a height this
                // loop chose. The server is telling this client where it is all
                // the while, and a hold that overwrote that with its own guess
                // would be answered with a correction -- every frame, for as
                // long as the hold lasted, which on a slow link is seconds of
                // them.
                player.pos.y    = y_before;
                player.velocity = Vec3{};
            }
        } else {
            // And past the latch, a column that has not arrived at all. The
            // world has no edge and is made as players go (TODO.md OPEN-35), so
            // a player who outruns it is walking over ground on its way, not
            // off the end of the world. The bottom layer and not the chunk under
            // the feet: every column the server sends has one, and a player
            // flying above the terrain has no chunk under them and must still
            // be able to come down.
            const ChunkPos under = World::chunk_of((int)std::floor(player.pos.x), 0,
                                                   (int)std::floor(player.pos.z));
            static bool held = false;
            const bool  hold = s.act().world.find_chunk(ChunkPos{under.x, 0, under.z}) == nullptr;
            if (hold != held)
                std::printf("%s ground not sent yet at %.2f %.2f %.2f\n",
                            hold ? "held over" : "no longer held over", (double)player.pos.x,
                            (double)player.pos.y, (double)player.pos.z);
            held = hold;
            if (hold) {
                player.pos.y    = y_before;
                player.velocity = Vec3{};
            }
        }

        // The server had the last word. One correction at a time, and taking it
        // clears it: if two arrived in one frame the older one is already wrong.
        net::Correct c;
        if (conn->take_correction(c)) {
            player.pos      = Vec3{c.x, c.y, c.z};
            player.velocity = Vec3{};
            // A kart goes on from where the server's copy of it is, at its speed
            // and heading, its chassis put down afresh as the server's is.
            if (c.flags & net::kCorrectDrive) {
                player.velocity = Vec3{c.vx, c.vy, c.vz};
                player.yaw      = c.yaw;
                if (c.nitro >= 0.0f) player.nitro = c.nitro;   // the server's gauge too
            }
            player.vehicle.reset();
            ++applied;
            std::printf("corrected to %.2f %.2f %.2f: %s\n", c.x, c.y, c.z, c.reason.c_str());
        }

        // Cells the server says have changed, in every world this client is
        // talking to -- the one being stood in and the ones being looked into,
        // because a window streams and a block that changed behind somebody's
        // portal has changed.
        //
        // The session already wrote the block; what is owed here is light and a
        // mesh, and set_block marked the chunk dirty on the way past. The light
        // goes through a LightBatch, as the server's does: a relight is a 33-cubed
        // pass, about a millisecond, and the Inferno seen through an opening sent
        // ninety changes in one frame as its lava spread -- 95 ms, measured with
        // CNP_HITCH_MS. A change that alters no light (lava to lava of another
        // level, a turn) costs nothing, and the rest share boxes.
        for (int id = 0; id < net_sessions.count(); ++id) {
            net::ClientSession *w = net_sessions.get(id);
            if (!w || !w->playing()) continue;

            // Which of this client's worlds that connection is. A change in the
            // world through the opening is heard THROUGH the opening -- it is
            // the one call site in the game where that rule is the difference
            // between a doorway and a sound inside your own head.
            const int changed_world = (id == self_conn) ? s.active : world_of(id);

            net::BlockChange bc;
            LightBatch       relight;
            while (w->next_block_change(bc)) {
                if (!light_unchanged(w->registry(), (BlockId)bc.had, (BlockId)bc.id))
                    relight.add(bc.x, bc.y, bc.z);
                ++blocks_changed;

                if (changed_world >= 0 && bc.why.empty()) {
                    const Vec3 at{(float)bc.x + 0.5f, (float)bc.y + 0.5f, (float)bc.z + 0.5f};
                    play_at(s, (bc.id == kAir) ? s.sounds.broke.any() : s.sounds.placed.any(), at,
                            changed_world);
                }
                if (!bc.why.empty()) {
                    ++edits_refused;
                    last_refusal = bc.why;

                    // On the SCREEN, and not only in the terminal. The server
                    // has been sending this sentence with every refused edit
                    // since blocks went on the wire and nothing ever drew it,
                    // so a mod protecting an area produced a player clicking at
                    // a wall with no idea why nothing happened -- which is what
                    // it looked like from the outside: "I can break blocks near
                    // the portal but not place them".
                    s.notice.say(bc.why, now);

                    if (edits_refused <= 4)
                        std::printf("that edit was refused: %s\n", bc.why.c_str());
                }
            }
            if (!relight.empty()) {
                diag::ProfScope _("relight");
                relight.flush(w->world(), w->registry());
            }

            // And what a mod there wanted this player to read (core.notice),
            // in the same place as a refusal.
            std::string said;
            if (w->take_notice(said)) {
                s.notice.say(said, now);
                std::printf("notice: %s\n", said.c_str());
            }
        }

        // A crossing the server decided. Not a correction with more fields: a
        // correction says the player was wrong and leaves their aim alone,
        // while this replaces where they are, where they look, what they carry
        // and how far the horizon is tipped -- all of it at once, which is why
        // it arrives as one message. See net::Teleport.
        net::Teleport moved;
        if (conn->take_teleport(moved)) {
            const Vec3 eye_before = player.eye(), look_before = player.forward();
            player.pos      = Vec3{moved.x, moved.y, moved.z};
            player.velocity = Vec3{moved.vx, moved.vy, moved.vz};
            player.yaw      = std::atan2(moved.look_x, -moved.look_z);
            player.vehicle.reset();   // afresh, as the server's copy is
            player.pitch    = std::asin(std::clamp(moved.look_y, -1.0f, 1.0f));
            player.on_ground = false;
            player.cb_shift  = 0.0f;
            s.roll           = moved.roll;
            if (moved.reason == "portal") {
                ahead_of_crossing(s, player, eye_before, look_before);
                aim_after_crossing(s, player);
            }

            // And held again until the ground under the new place has arrived.
            // The latch above released at the middle of the world, where a
            // client puts itself before it knows anything, and a server that
            // then places the player somewhere not streamed yet -- a track's
            // spawn in the corner of its world -- let them fall through air
            // until the chunk turned up. Measured: 0.03 of a node, from 20.00
            // to 19.97, with no chunk under the feet.
            settled = nullptr;

            // No camera warp: the server sent where the player IS, and there is
            // no image on this side yet for the eye to have been continuous
            // with. When the passive session arrives, this is where the warp
            // the local crossing uses will go.
            s.cam_warp      = Vec3{};
            s.cam_warp_left = 0.0f;

            if (underfoot_sent && applied_at_crossing < 0) applied_at_crossing = applied;

            std::printf("the server crossed us to %.2f %.2f %.2f, looking %.2f %.2f %.2f\n",
                        (double)moved.x, (double)moved.y, (double)moved.z, (double)moved.look_x,
                        (double)moved.look_y, (double)moved.look_z);
        }

        // Other players are bodies, not outlines: filled, so they read as
        // people-sized solids, and so that when portals work over the network
        // they are cut at one like anything else. The box is the one the
        // server's movement check uses, so what is drawn is exactly the volume
        // the rules are about.
        s.act().bodies.clear();
        std::unordered_map<uint64_t, Body> creatures_now;
        net::Ride  carried;
        const bool is_carried = conn->riding(carried);
        s.seat_on = false;
        // Machines' hulls as drawn this frame (protocol 100), for the body to
        // be kept out of and stand on: the one it stood on is found among
        // last frame's, and carries it as far as it went since.
        std::vector<World::Mover> movers;
        uint64_t stood_on = 0;
        if (player.on_ground && !is_carried) {
            const float h = s.act().movement.width * 0.5f;
            const AABB  under{player.pos - Vec3{h, 0.1f, h}, player.pos + Vec3{h, 0.0f, h}};
            for (const World::Mover &m : s.act().world.movers)
                if (box_meets_mover(m, under)) stood_on = m.id;
        }
        // The machine this player drives, where it is going to be (Scene::
        // Predicted), and the way from where it is drawn to there, which what
        // rides it -- its turret, its gun, whoever sits in it -- goes too.
        bool      predicting = false;
        CraftPose ahead;
        Vec3      drawn;
        float     drawn_yaw = 0.0f, drawn_pitch = 0.0f, drawn_roll = 0.0f;
        float     hull_w = 0.0f, hull_l = 0.0f, hull_top = 0.0f;
        if (s.predicted.sim) {
            const net::RemotePlayer *host = nullptr;
            for (const net::RemotePlayer &p : conn->players())
                if (p.id == s.predicted.id) host = &p;
            if (!is_carried || carried.host != s.predicted.id || host == nullptr) {
                const Scene::Predicted &pr = s.predicted;
                std::printf("predicted machine %llu: %d poses compared, %.2f off on average, %d put back\n",
                            (unsigned long long)pr.id, pr.compared,
                            pr.compared > 0 ? pr.off_sum / pr.compared : 0.0, pr.resets);
                s.predicted = Scene::Predicted{};
            } else {
                predict_machine(s, *host, conn->round_trip(), plat_time(), dt);
                ahead = s.predicted.history.back().second;
                net::RemotePlayer g = *host;
                glide(s.act().motion[g.id], g, plat_time());
                drawn       = Vec3{g.x, g.y, g.z};
                drawn_yaw   = g.yaw;
                drawn_pitch = g.pitch;
                drawn_roll  = g.chassis ? g.body_roll : 0.0f;
                hull_w      = g.hull_w;
                hull_l      = g.hull_l;
                hull_top    = g.hull_ride + g.hull_h;
                predicting  = true;
            }
        }
        for (const net::RemotePlayer &sent : conn->players()) {
            net::RemotePlayer p = sent;
            glide(s.act().motion[p.id], p, plat_time());
            if (predicting && p.id == s.predicted.id) {
                p.x = ahead.base.x, p.y = ahead.base.y, p.z = ahead.base.z;
                p.yaw = ahead.yaw, p.pitch = ahead.pitch, p.body_roll = ahead.roll;
            } else if (predicting) {
                const Vec3 l = craft_local(drawn, drawn_yaw, drawn_pitch, drawn_roll, Vec3{p.x, p.y, p.z});
                if (std::fabs(l.x) < hull_w + 0.5f && std::fabs(l.z) < hull_l + 0.5f && l.y > -0.5f &&
                    l.y < hull_top + 2.5f) {
                    const Vec3 to = craft_point(ahead.base, ahead.yaw, ahead.pitch, ahead.roll, l);
                    p.x = to.x, p.y = to.y, p.z = to.z;
                    p.yaw += ahead.yaw - drawn_yaw;
                }
            }
            if (p.kind == net::EntityKind::Thing && p.hull_w > 0.0f && !(is_carried && p.id == carried.host)) {
                const float mid = p.y + p.hull_ride;
                movers.push_back({p.id, p.x, p.z, p.yaw, p.hull_w, p.hull_l, mid - p.hull_h, mid + p.hull_h});
            }
            // What carries this player, as it will be drawn: the seat on it,
            // so the body and the machine move as one (protocol 99).
            if (is_carried && p.id == carried.host) {
                const float roll = p.kind == net::EntityKind::Thing && p.chassis ? p.body_roll : 0.0f;
                const float tip  = p.kind == net::EntityKind::Thing ? p.pitch : 0.0f;
                player.pos   = craft_point(Vec3{p.x, p.y, p.z}, p.yaw, tip, roll,
                                           Vec3{carried.seat_x, carried.seat_y, carried.seat_z});
                s.seat_on    = true;
                s.seat_yaw   = p.yaw;
                s.seat_pitch = tip;
                s.seat_roll  = roll;
            }
            // In this player's hands, drawn there by its mod: not drawn here,
            // and not gone either -- forgotten, so nothing fades out where it
            // was picked up.
            if (p.held) {
                s.act().creatures_seen.erase(p.id);
                continue;
            }
            Body body;
            body.id = p.id;

            if (p.kind == net::EntityKind::Drop) {
                body = p.falling ? falling_body(p.item, Vec3{p.x, p.y, p.z})
                                 : drop_body(s.act(), conn->items().get(p.item), Vec3{p.x, p.y, p.z}, p.id);
            } else if (p.kind == net::EntityKind::Mob) {
                // Its own box and its own colour, because a creature drawn as a
                // person is a creature a player swings at by mistake -- and a
                // person drawn as a creature is worse. The box is the one the
                // aim uses, so what is drawn is the volume the rule is about.
                body.center = Vec3{p.x, p.y + ecs::kMobHeight * 0.5f, p.z};
                body.half =
                    Vec3{ecs::kMobHalfWidth, ecs::kMobHeight * 0.5f, ecs::kMobHalfWidth};
                body.colour[0] = 0.30f;
                body.colour[1] = 0.70f;
                body.colour[2] = 0.35f;
            } else if (p.kind == net::EntityKind::Thing) {
                // Something a mod made visible (TODO.md OPEN-20). A box of its
                // own size and colour, because the engine does not know what it
                // is -- a pellet, a marker, a crate -- and drawing it as a person
                // or a creature would be a guess that tells the player a lie.
                body.center    = Vec3{p.x, p.y + 0.25f, p.z};
                body.half      = Vec3{0.25f, 0.25f, 0.25f};
                body.colour[0] = 0.55f;
                body.colour[1] = 0.60f;
                body.colour[2] = 0.95f;
            } else {
                body.center    = Vec3{p.x, p.y + kPlayerHeight * 0.5f, p.z};
                const float half = s.act().movement.width * 0.5f;
                body.half        = Vec3{half, kPlayerHeight * 0.5f, half};
                body.colour[0] = 0.95f;
                body.colour[1] = 0.75f;
                body.colour[2] = 0.15f;
            }

            // What it looks like and which way it is facing, straight off the
            // wire. A drop never carries a model -- the decoder refuses one
            // that does -- so this is only ever a person or a creature.
            body.model = p.kind == net::EntityKind::Player ? species_model(s.act().models, p.model, p.livery)
                                                           : p.model;
            if (!body.dropped) body.yaw = p.yaw;
            // What they hold, off the server's item table: the tools shown
            // and a block in the hand, as for this machine's own body.
            if (p.kind == net::EntityKind::Player) {
                const ItemDef &held = conn->items().get((ItemId)p.item);
                body.hand_known = true;
                body.hand       = p.item != 0 ? held.name : std::string();
                body.hand_block = p.item != 0 ? held.places : kAir;
                body.hand_tag   = (uint32_t)p.item_tint << kTagIdBits;
                body.livery     = p.livery;
                for (int k = 0; k < 5; ++k) {
                    body.worn[k] = p.worn[k] != 0 ? conn->items().get(p.worn[k]).name : std::string();
                    body.worn_tint[k] = p.worn_tint[k];
                }
                for (int k = 0; k < 2; ++k)
                    body.arm[k] = p.arm[k] != 0 ? conn->items().get(p.arm[k]).name : std::string();
                std::copy(std::begin(p.dress), std::end(p.dress), std::begin(body.dress));
                std::copy(std::begin(p.dress_tint), std::end(p.dress_tint), std::begin(body.dress_tint));
                body.arms_working = p.working;
            }
            // An arrow's nose, which the server turns along its flight.
            if (p.kind == net::EntityKind::Thing) body.pitch = p.pitch;
            // A machine, rolled (98).
            if (p.kind == net::EntityKind::Thing && p.chassis) body.roll = p.body_roll;
            // Somebody in a kart: tipped as its chassis sits and stood on
            // the bottom of it, not on the wheel that touches highest.
            if (p.kind == net::EntityKind::Player && p.chassis) {
                body.boosting = p.boosting;
                body.sliding  = p.sliding;
                body.pitch  = p.body_pitch;
                body.roll   = p.body_roll;
                body.center = Vec3{p.x + p.base_dx, p.y + p.base_dy + body.half.y, p.z + p.base_dz};
            }
            // Somebody lying down (75): their yaw is the way their head points.
            if (p.kind == net::EntityKind::Player && p.lying) lay(body, Vec3{p.x, p.y, p.z}, p.yaw);

            // What a body at work plays: a person's is their tool's; a
            // creature's is the one its model has for fighting, a blow or a
            // draw (protocol 52).
            const char *work = nullptr;
            // A creature's hand is always known (81): empty, every tool_* bone
            // of its model is hidden, and holding a tool shows only that one.
            if (p.kind == net::EntityKind::Mob) {
                const ItemDef &held = conn->items().get((ItemId)p.item);
                body.hand_known = true;
                body.hand       = p.item != 0 ? held.name : std::string();
                body.hand_block = p.item != 0 ? held.places : kAir;
            }
            if (!p.clip.empty()) {   // a worker's, a thing's, or a person's a mod named (92)
                work = p.clip.c_str();
            } else if (p.working && p.kind == net::EntityKind::Mob) {
                const ModelStore::Entry *me = s.act().models.entry(p.model);
                work = !me                                 ? nullptr
                     : me->model.clip("attack") >= 0       ? "attack"
                     : me->model.clip("sit") >= 0          ? "sit"   // a pet told to sit
                                                           : "shoot";
            } else if (p.working) {
                work = work_clip(body.hand, "");
            }
            if (p.kind != net::EntityKind::Drop && p.model != 0)
                animate_body(s.act(), p.id, body,
                             Vec3{p.x, body.center.y - body.half.y, p.z},
                             species_model(s.act().models, conn->welcome().player_model, p.livery),
                             nullptr, work, p.crouching,
                             p.kind == net::EntityKind::Player && p.lying,
                             p.kind == net::EntityKind::Player && p.flying);

            s.act().bodies.push_back(body);
            if (p.kind == net::EntityKind::Mob && p.model != 0) creatures_now[p.id] = body;
            if (p.burning) add_flames(s.act().bodies, Vec3{p.x, p.y, p.z}, p.id);
        }
        fade_the_gone(s.act(), creatures_now, plat_time());
        {
            World &w = s.act().world;
            const std::vector<World::Mover> before = std::exchange(w.movers, std::move(movers));
            for (const World::Mover &now : w.movers) {
                if (now.id != stood_on) continue;
                for (const World::Mover &was : before) {
                    if (was.id != now.id) continue;
                    // Where the feet are on it, in its own frame, put back
                    // in the world's as it is now: turned with it as well.
                    const float dx = player.pos.x - was.x, dz = player.pos.z - was.z;
                    const float lx = dx * std::cos(was.yaw) + dz * std::sin(was.yaw);
                    const float lz = dx * std::sin(was.yaw) - dz * std::cos(was.yaw);
                    const Vec3  to{now.x + lx * std::cos(now.yaw) + lz * std::sin(now.yaw),
                                  player.pos.y + (now.top - was.top),
                                  now.z + lx * std::sin(now.yaw) - lz * std::cos(now.yaw)};
                    player.pos = move_body(w, s.act().registry, player.pos, to - player.pos).pos;
                    player.yaw += std::remainder(now.yaw - was.yaw, 6.2831853f);
                }
            }
        }

        {
            // The server shows everybody what is in this square, so it is told
            // when the square changes -- and again on every new connection.
            s.working = s.dig_progress > 0.0f ||
                        (plat_cursor_grabbed() && plat_mouse_down(PLAT_MOUSE_LEFT));
            s.arms_clip = body_arms_clip(s.act(), player);
            hold_arms(s, player);
            static const net::ClientSession *told_conn    = nullptr;
            static int                       told_slot    = -1;
            static bool                      told_working = false;
            static bool                      told_crouch  = false;
            if ((told_conn != conn || told_slot != held_slot || told_working != s.working ||
                 told_crouch != player.crouching) &&
                conn->playing() && conn->send_hold(held_slot, s.working, player.crouching)) {
                told_conn    = conn;
                told_slot    = held_slot;
                told_working = s.working;
                told_crouch  = player.crouching;
            }

            const net::InventoryState &carried = conn->carried();
            s.hand.clear();
            s.hand_block = kAir;
            s.hand_tag   = 0;
            if (held_slot >= 0 && (size_t)held_slot < carried.slots.size() &&
                carried.slots[(size_t)held_slot].count > 0) {
                const ItemDef &def =
                    conn->items().get((ItemId)carried.slots[(size_t)held_slot].item);
                s.hand       = def.name;
                s.hand_block = def.places;
                s.hand_tag   = carried.slots[(size_t)held_slot].tag;
            }
        }
        s.act().player_model_id = conn->welcome().player_model;
        add_own_body(s, player, conn->welcome().player_model);

        const Vec3 fwd = player.forward();
        conn->send_move(player.pos.x, player.pos.y, player.pos.z, fwd.x, fwd.y, fwd.z, now,
                        player.flying, player.velocity);

        // And a viewpoint for every world being looked into, or nothing is
        // streamed: an observer's chunks follow its claim like anybody's.
        //
        // The claim is the PLAYER's position, and that is a placeholder said
        // out loud. What it should be is where the opening comes out in that
        // world, and a mouth is where that point comes from.
        for (int id = 0; id < net_sessions.count(); ++id) {
            // The connection the player is on NOW. After a crossing that is not
            // the one they joined on, and skipping the original would send a
            // viewpoint claim down the player's own connection -- dragging
            // their interest centre to the far opening every tick.
            if (id == self_conn) continue;
            net::ClientSession *w = net_sessions.get(id);
            if (!w || !w->playing()) continue;

            // And not to one that is on its way to being a body. The moment a
            // crossing is asked for, that session stops being a window: going
            // on claiming the far opening as a viewpoint means the last claims
            // before the answer describe a portal centre, and the server that
            // has just granted the crossing reads them as a position. The wire
            // catches the ones already in flight; this stops making more.
            if (w->scope() != net::Scope::Observe) continue;

            // For the world behind a mouth, the viewpoint is where the opening
            // comes out over there -- which is the point DESIGN.md 4.4 wants an
            // observer's chunks kept in a sphere around. For a --watch world
            // there is no such point, and the player's own position stands in
            // for it.
            // Kept for a window no mouth is using right now -- the arena while
            // the player is on the track -- so it is still streamed around its
            // opening when the way back to it opens again.
            // A world behind two mouths here -- its gate and its way out on
            // the plaza -- is streamed round the far end of the nearer one.
            const int shown_world = world_of(id);
            Vec3      at = (id != self_conn && shown_world >= 0) ? s.at(shown_world).portal_a.center
                                                                 : player.pos;
            float best = 1e30f;
            for (int m = 0; m < net::kMaxMouths && shown_world >= 0; ++m) {
                if (mouth_world(s.act(), m) != shown_world) continue;
                const float d = length_sq(mouth_portal(s.act(), m).center - player.pos);
                if (d < best) best = d, at = mouth_link(s.act(), m).dst.center;
            }
            w->send_move(at.x, at.y, at.z, fwd.x, fwd.y, fwd.z, now);
        }

        // The window may have been dragged. The projection follows on its own --
        // it reads the screen every frame -- but the portal targets do not, and
        // they wait for the drag to settle. See ScreenTargets.
        if (!screen_targets.follow(s, plat_time())) {
            std::fprintf(stderr, "could not rebuild the portal targets at %dx%d\n",
                         gfx_screen_width(), gfx_screen_height());
            return 1;
        }

        // Aim, at blocks and at bodies both. Whichever is nearer is what the
        // click is about: a player swinging at a mob standing against a wall is
        // aiming at both, and the one in front is the one they mean.
        constexpr float kReach = 6.0f;
        s.target = raycast(s.act().world, s.act().registry, player.eye(), player.forward(), kReach);
        // Looking at a point (core.fixed_view) the eye aims at nothing: the
        // clicks are the world code's, and no block is outlined under them.
        if (s.act().iso_fixed && iso_engaged(s.act())) s.target = RaycastHit{};

        float          aimed_at   = kReach;
        const uint64_t aimed_at_body =
            aimed_body(*conn, s.act().models, player.eye(), player.forward(), kReach, aimed_at);

        // The panel this world's mods put up, as the server last described it.
        // Rebuilt only when a PanelShow actually arrived -- see
        // ClientSession::panel_revision.
        if (conn->panel_revision() != panel_rev_seen) {
            panel_rev_seen = conn->panel_revision();

            std::string perr;
            if (!adopt_panel(s.act().panels, conn->panel(), perr)) {
                std::fprintf(stderr, "the server sent a panel this client will not draw: %s\n",
                             perr.c_str());
            } else if (const net::PanelShow *ps = conn->panel()) {
                std::printf("a mod put a panel up: '%s' from '%s', %zu element(s)\n",
                            ps->name.c_str(), ps->owner.c_str(), ps->elements.size());
            } else {
                std::printf("the mod's panel closed\n");
            }
        }

        // A mod's panel takes the cursor and the clicks while it is up. The same
        // rule and the same order as the single-process loop: checked before the
        // bag, because it is drawn over it.
        //
        // What goes out is the panel's name and the button's id, never the
        // point -- the server checks the pair against the panel it has up. See
        // net::UiClick.
        const bool panel_up = s.act().panels.topmost_for(ui::kLocalPlayer) != nullptr;
        {
            const ui::CursorWish w = ui::panel_cursor(panel_up, s.hotbar.open,
                                                      plat_cursor_grabbed(),
                                                      s.hotbar.panel_took_cursor);
            s.hotbar.panel_took_cursor = w.took;
            if (w.change) {
                plat_cursor_grab(w.grab);
                if (w.grab) look_settle.grabbed();
            }
        }

        // Not with the pause menu up: it is drawn over the panel, and a click
        // on Resume must not also land on whatever button is underneath.
        const ui::Panel *mod_panel = s.act().panels.topmost_for(ui::kLocalPlayer);
        if (mod_panel && !s.pause.up) {

            if (plat_mouse_pressed(PLAT_MOUSE_LEFT)) {
                float mx = 0.0f, my = 0.0f;
                plat_mouse_position(&mx, &my);

                const std::string pressed = click_panel(s, *mod_panel, mx, my);
                if (!pressed.empty()) conn->send_ui_click(mod_panel->name, pressed);
            }

            std::string field, typed;
            if (type_in_field(s, mod_panel, field, typed))
                conn->send_ui_click(mod_panel->name, field, typed);
        } else if (s.typing.on && s.typing.panel != "core:chest") {
            stop_typing(s);
        }
        if (std::string q; chest_search(s, q)) conn->send_ui_click("core:chest", "search", q);

        // The bag, over the world. E opens it, E or ESC closes it, and while it
        // is open the cursor is free and nothing the player clicks reaches the
        // world -- which is what the dim sheet behind the panel is saying.
        if (seconds <= 0.0 && !s.pause.up && !s.typing.on && plat_key_pressed(PLAT_KEY_E)) {
            s.hotbar.open  = !s.hotbar.open;
            s.hotbar.press = -1;
            plat_cursor_grab(!s.hotbar.open);

            // Shutting the panel shuts the chest, and it has to be SAID: the
            // chest is the server's, and hiding the picture of one is not
            // closing it. Without this the server went on holding it open and
            // the next ContainerState put it straight back on screen.
            if (!s.hotbar.open && (s.hotbar.chest_open || s.hotbar.craft_w > 2))
                conn->send_close_container();
        }

        // Press, then release. A drag never touches this side's copy of the
        // bag: it turns into a MoveItem, and what the slots show next is
        // whatever the server sends back.
        if (s.hotbar.open) {
            float mx = 0.0f, my = 0.0f;
            plat_mouse_position(&mx, &my);

            if (plat_mouse_pressed(PLAT_MOUSE_LEFT)) {
                const ui::BagPoint at = s.hotbar.point_at(mx, my);
                s.hotbar.press        = at.slot;
                s.hotbar.press_grid   = at.grid;
                s.hotbar.press_right  = false;
            }

            // The right button means two different things, and which one is
            // decided by whether a drag is already running. Part way through
            // one it puts ONE item down and leaves the drag going; on its own
            // it STARTS a drag that carries half. See ui::bag_tap and
            // ui::bag_half.
            if (plat_mouse_pressed(PLAT_MOUSE_RIGHT)) {
                if (plat_mouse_down(PLAT_MOUSE_LEFT)) {
                    ui::BagPoint press;
                    press.slot = s.hotbar.press;
                    press.grid = s.hotbar.press_grid;

                    const ui::BagAction one =
                        ui::bag_tap(press, s.hotbar.point_at(mx, my), s.hotbar.press_has_item());

                    if (one.kind == ui::BagAction::Move)
                        conn->send_move_item(one.from, one.to, one.count, side_of(one.from_grid),
                                             side_of(one.to_grid));
                } else {
                    const ui::BagPoint at = s.hotbar.point_at(mx, my);
                    if (ui::half_drag_starts(at.grid)) {
                        s.hotbar.press       = at.slot;
                        s.hotbar.press_grid  = at.grid;
                        s.hotbar.press_right = true;
                    }
                }
            }

            if (plat_mouse_released(PLAT_MOUSE_RIGHT) && s.hotbar.press_right) {
                ui::BagPoint press;
                press.slot = s.hotbar.press;
                press.grid = s.hotbar.press_grid;

                const ui::BagAction act =
                    ui::bag_half(press, s.hotbar.point_at(mx, my), s.hotbar.press_count());

                if (act.kind == ui::BagAction::Move)
                    conn->send_move_item(act.from, act.to, act.count, side_of(act.from_grid),
                                         side_of(act.to_grid));

                s.hotbar.press       = -1;
                s.hotbar.press_grid  = ui::Grid::Bag;
                s.hotbar.press_right = false;
            }

            if (plat_mouse_released(PLAT_MOUSE_LEFT)) {
                ui::BagPoint press;
                press.slot = s.hotbar.press;
                press.grid = s.hotbar.press_grid;

                // Shift makes a click mean something again -- the stack goes
                // to the other grid without being carried there. Two functions
                // and one decision here, because they are two rules and not one
                // rule with a flag in it.
                const ui::BagPoint at = s.hotbar.point_at(mx, my);
                const ui::BagAction act =
                    plat_key_down(PLAT_KEY_SHIFT)
                        ? ui::bag_shift_click(press, at, s.hotbar.press_has_item())
                        : ui::bag_click(press, at, s.hotbar.press_has_item());

                if (act.kind == ui::BagAction::Move) {
                    // Taking the result is its own request: it is not a move
                    // between two squares, it is "make that and pay for it out
                    // of the grid", and the server has to do both or neither.
                    if (act.from_grid == ui::Grid::Result) conn->send_take_craft();
                    else
                        conn->send_move_item(act.from, act.to, act.count, side_of(act.from_grid),
                                             side_of(act.to_grid));
                }
                if (act.kind == ui::BagAction::Quick) {
                    // No destination goes with it. Where it lands is the
                    // server's rule -- net::quick_target -- and a client that
                    // worked it out first would be a second copy of it.
                    if (act.from_grid == ui::Grid::Result) conn->send_quick_craft();
                    else conn->send_quick_move(act.from, side_of(act.from_grid));
                }
                if (act.kind == ui::BagAction::Drop) conn->send_drop_item(act.from);

                // Whatever it was, the gesture is over and the square fills
                // back in. If the move is going to happen the server's
                // InventoryState says so a round trip later -- and until it
                // does, the stack is where it always was, because that is where
                // the server still has it.
                s.hotbar.press       = -1;
                s.hotbar.press_grid  = ui::Grid::Bag;
                s.hotbar.press_right = false;
            }
        } else {
            s.hotbar.press       = -1;
            s.hotbar.press_grid  = ui::Grid::Bag;
            s.hotbar.press_right = false;
        }

        // Nothing is drawn on the click, for blocks or for openings. What goes
        // out is which block was hit and which face of it; what comes back is
        // what the server decided. A client that drew its own would be showing
        // a block that may have been refused -- and over a link with a round
        // trip in it, showing it for as long as the round trip lasts.
        //
        // Ctrl for an opening, plain for a block. The two used to share the
        // buttons because blocks did not cross the wire at all; now they do,
        // and a portal is the rarer of the two.
        // Every frame that is NOT a dig says so, before anything asks whether
        // this one is. Without this the bar under the crosshair kept whatever
        // it was last given: letting go, aiming at the sky or opening the bag
        // half way through a dig left a half-full bar on screen until the next
        // dig started -- seen in play.
        const bool playing_input = plat_cursor_grabbed() && !s.hotbar.open && seconds <= 0.0;
        const bool claimed_right = claim_buttons(s, playing_input);
        // The world's code on this machine hears time pass, as the server's
        // mods do: a clock it shows, a first word once there is a server to
        // say it to. core:tick only; on_step and the rest stay the server's.
        {
            script_field dt_field{"dt", SCRIPT_V_NUMBER, (double)dt, nullptr, 0};
            bool         cancelled = false;
            std::string  err;
            if (!s.act().mods.events().emit("core:tick", &dt_field, 1, &cancelled, err))
                std::fprintf(stderr, "emit core:tick: %s\n", err.c_str());
        }
        if (press_r_due >= 0.0 && plat_time() >= press_r_due) {
            std::printf("pressed R, %.1f s into the run\n", g_press_r_at);
            emit_input(s, "key_r");
            press_r_due = -1.0;
        }
        const bool left_down     = plat_mouse_down(PLAT_MOUSE_LEFT) && !s.left_claimed;

        if (!dig_frame(left_down, s.target.hit, playing_input, plat_key_down(PLAT_KEY_CTRL)))
            dig_idle(s.dig, &s.dig_progress);

        if (playing_input) {
            const bool right = plat_mouse_pressed(PLAT_MOUSE_RIGHT) && !claimed_right;

            // Breaking is HELD here too, and the dig is timed by the same
            // function the single-process loop uses -- so the bar fills at the
            // rate the block actually takes.
            //
            // What goes on the wire is two Edits and not sixty: the one on the
            // press, which starts the dig on the server, and the one this
            // sends when the seconds are up. The server times it with its own
            // clock (ServerSessions::plan_edit), so a client that hurried this
            // would send its second Edit early and simply be told nothing.
            bool left = plat_mouse_pressed(PLAT_MOUSE_LEFT) && !s.left_claimed;
            if (left_down && s.target.hit && !plat_key_down(PLAT_KEY_CTRL)) {
                const BlockId under =
                    s.act().world.get_block(s.target.x, s.target.y, s.target.z);

                // The hand, off the inventory the SERVER sent: this loop has no
                // bag of its own, and `held_slot` is which square of it the
                // player is on. An empty or unknown slot is a bare hand, which
                // is the same thing the server reads for it.
                const net::InventoryState &carried = conn->carried();
                ItemStack                  held;
                if (held_slot >= 0 && (size_t)held_slot < carried.slots.size()) {
                    held.item  = (ItemId)carried.slots[(size_t)held_slot].item;
                    held.count = carried.slots[(size_t)held_slot].count;
                    held.ench  = carried.slots[(size_t)held_slot].ench;
                    held.tag   = carried.slots[(size_t)held_slot].tag;
                }

                // The SERVER's item table, which the ids in `carried` are
                // positions in -- the one the hotbar names things from. This
                // process's own table is its own mods', and in it the server's
                // pick was some other item or none: reported from play, a pick
                // and a shovel that dug no faster than a hand.
                // Or an arm's tool, when it digs this better (net::best_dig_slot).
                {
                    std::vector<ItemStack> bag(carried.slots.size());
                    for (size_t i = 0; i < bag.size(); ++i)
                        bag[i] = ItemStack{(ItemId)carried.slots[i].item, carried.slots[i].count,
                                           carried.slots[i].used, carried.slots[i].ench, carried.slots[i].tag};
                    const int best = net::best_dig_slot(conn->items(), bag, held_slot,
                                                        s.act().registry.get(under).material);
                    if (best != held_slot) held = bag[(size_t)best];
                }
                const double need = dig_need(s.act().registry, conn->items(), under, held, conn->dig_factor());

                // A dig that starts on a cell other than the last one asks the
                // server too, pressed or not. Holding the button from one block
                // to the next starts the next without a press, and the server
                // starts its clock on the first Edit it gets for a cell: that was
                // the Edit at the end of the bar, so the bar filled twice for
                // every block after the first -- reported from play, digging a
                // column. The cell just finished is not a new one: its Edit has
                // gone, and a second would reach a cell that is already air.
                const bool starting = s.dig.x != s.target.x || s.dig.y != s.target.y ||
                                      s.dig.z != s.target.z || s.dig.block != under;
                if (dig_step(s.dig, left, true, true, s.target.x, s.target.y, s.target.z, under,
                             need, now, &s.dig_progress))
                    left = true;
                else if (need > 0.0)
                    left = left || starting;   // otherwise still digging: the start has asked
            }

            // The body first, when it is the nearer of the two. The left
            // button strikes it; the right uses it -- a pet tamed with what is
            // in the hand, or told to sit (ecs::use_pet) -- and is building
            // against a block only when no body is nearer.
            //
            // Nothing is drawn here either. What the swing did -- whether it
            // landed, whether it killed -- comes back as the body being gone
            // from the next entity list, the same way a broken block comes back
            // as a BlockChange.
            // Except a door with somebody in its doorway: the right button
            // opens or shuts the door, not the body standing in it.
            const bool body_in_door =
                right && s.target.hit && s.target.distance - aimed_at < 1.0f &&
                s.act().registry.get(s.act().world.get_block(s.target.x, s.target.y, s.target.z)).door != 0;
            const bool body_first = aimed_at_body != 0 && !body_in_door &&
                                    (!s.target.hit || aimed_at <= s.target.distance);

            if (left && body_first && !plat_key_down(PLAT_KEY_CTRL)) {
                if (conn->send_hit(aimed_at_body, held_slot)) ++hits_sent;
            } else if (right && body_first && !plat_key_down(PLAT_KEY_CTRL)) {
                conn->send_hit(aimed_at_body, held_slot, true);
            } else if ((left || right) && s.target.hit) {
                if (plat_key_down(PLAT_KEY_CTRL))
                    conn->send_place_portal(left ? 0 : 1, s.target.x, s.target.y, s.target.z,
                                            s.target.nx, s.target.ny, s.target.nz);
                else if (conn->send_edit(s.target.x, s.target.y, s.target.z, s.target.nx,
                                         s.target.ny, s.target.nz, left, held_slot, s.piece_turn,
                                         plat_key_down(PLAT_KEY_SHIFT)))
                    ++edits_sent;
            } else if (right && !plat_key_down(PLAT_KEY_CTRL)) {
                // Nothing solid within reach, but maybe water: a bucket held over
                // a lake whose bed is further than a hand. Sent as a use of the
                // water cell; the server builds nothing against it.
                const RaycastHit wet = raycast(s.act().world, s.act().registry, player.eye(),
                                               player.forward(), kReach, true);
                if (wet.hit && conn->send_edit(wet.x, wet.y, wet.z, wet.nx, wet.ny, wet.nz, false,
                                               held_slot))
                    ++edits_sent;
            }
        }

        // And the hands, in the long run. The same two messages a click sends,
        // because what is under test is the loop closing over and over -- a
        // second path that only the long run uses would be a second path to get
        // wrong.
        if (g_roam && seconds > 0.0 && conn->playing() && edit_step == EditStep::Done) {
            if (now >= roam.next_hit && aimed_at_body != 0) {
                if (conn->send_hit(aimed_at_body, held_slot)) {
                    ++roam.swung;
                    ++hits_sent;
                }
                // A little past the server's own rate, so this is not sending
                // blows that are refused for being early. The refusal is
                // already gated elsewhere; here it would only be noise.
                roam.next_hit = now + ecs::kHitCooldown + 0.05;
            }

            if (now >= roam.next_edit && s.target.hit) {
                // Break, then a few seconds later build something back with
                // whatever is in the hand. Not necessarily into the same cell:
                // what closes is the LOOP -- a block came out of the world and
                // into the bag, and one came out of the bag and into the world
                // -- and insisting on the same cell would make the walk stand
                // still while it waited for an answer.
                const bool breaking = !roam.holding;
                if (conn->send_edit(s.target.x, s.target.y, s.target.z, s.target.nx, s.target.ny,
                                    s.target.nz, breaking, held_slot)) {
                    ++edits_sent;
                    if (breaking) ++roam.broke;
                    else          ++roam.built;
                    roam.holding = breaking;
                }
                roam.next_edit = now + (breaking ? 3.0 : 5.0);
            }

            // A line a minute, so a run that is killed at the wall clock still
            // says how it was going -- and so that a leak is a shape rather
            // than two numbers at the ends.
            if (roam.rss_first == 0) roam.rss_first = resident_bytes();
            if (now - roam.reported >= 60.0) {
                const size_t rss = resident_bytes();
                if (rss > roam.rss_worst) roam.rss_worst = rss;
                std::printf("roam %5.0f s: %6.0f fps, RSS %.1f MB, %ld broke / %ld built, "
                            "%ld swung, %ld turned, %ld corrections, %02d:%02d\n",
                            now, (double)(frames - roam.frames_then) / (now - roam.reported),
                            (double)rss / (1024.0 * 1024.0), roam.broke, roam.built, roam.swung,
                            roam.turns, applied,
                            (int)(conn->time_of_day() * 24.0),
                            (int)(conn->time_of_day() * 1440.0) % 60);
                std::fflush(stdout);
                roam.reported    = now;
                roam.frames_then = frames;
            }
        }

        // Q throws what is in the hand on the ground. One of it, or the whole
        // stack with CTRL held -- the same two the panel offers, from a key,
        // because a player with a bag closed should not have to open it to get
        // rid of something.
        if (seconds <= 0.0 && !s.hotbar.open && !panel_up && !s.pause.up &&
            plat_key_pressed(PLAT_KEY_Q))
            conn->send_drop_item(held_slot, plat_key_down(PLAT_KEY_CTRL) ? 0 : 1);

        // The wheel walks the bar, which is the other half of 1-9 and the one
        // a hand already knows. Only with the bag closed: while a panel is up
        // the wheel belongs to the panel, and one that quietly moved the hand
        // behind it would be a player putting down a block they did not choose.
        if (seconds <= 0.0 && !s.hotbar.open && !panel_up) {
            const float notches = plat_mouse_wheel();
            if (notches != 0.0f)
                held_slot = ui::hotbar_scroll(held_slot, notches, Inventory::kHotbar);
        } else if (seconds <= 0.0 && s.hotbar.open && s.hotbar.chest_open) {
            // Over an open chest it is the mods': a catalogue turns its rows.
            const float notches = plat_mouse_wheel();
            if (notches != 0.0f) emit_input(s, notches > 0.0f ? "wheel_up" : "wheel_down");
        }

        // The hotbar. The slot travels WITH the click (net::Edit) rather than
        // as a message of its own, so this only has to remember it -- see the
        // comment on Edit::slot: a select that arrives after the click that
        // meant it is how a player pays for a block they did not put down.
        for (int n = 0; n < Inventory::kHotbar; ++n) {
            if (plat_key_pressed((plat_key)(PLAT_KEY_1 + n)) && !s.digit_claimed && !s.typing.on) held_slot = n;
        }

        // The scripted break-and-build-it-back. Three nodes out, so the hole
        // it makes is nowhere near the block the portal placement below picks
        // and nowhere near the player's feet.
        //
        // Nothing is gifted to this player. A server hands out no starting
        // inventory -- the only way to have a block is to have broken one --
        // so the order here is the order the game actually imposes: break
        // first, build second, and the third click is refused because the bag
        // is empty again.
        // Not on a world whose pair leads somewhere else. That run's scenario is
        // the crossing, and two scenarios in one run are one scenario nobody
        // wrote: the sequence is about the world the player JOINED, and a
        // crossing moves them to another server with another bag.
        //
        // Making the walk wait for the hands to finish was tried first and is
        // worse -- it pushes the crossing eight seconds later, which changes
        // the timing of the very thing bad_link measures, and three runs in
        // eight came back with the far window in a state the crossing checks
        // did not expect. A gate that has to be re-timed to fit a second
        // scenario is a gate that is now measuring the second one.
        const bool hands_here = !conn->portals().leads_away();

        // The bag as the sequence leaves it, and the ground it stood on. Every
        // frame while it is still running, so the last value written is the one
        // the sequence finished with -- and the sequence stands still, which is
        // the state both of these claims are about.
        if (edit_step != EditStep::Done && s.act().world.chunk_count() > 0) {
            if (conn->has_inventory()) {
                carried_when_done = 0;
                for (const auto &sl : conn->carried().slots) carried_when_done += sl.count;
            }
            if (!stood_taken) {
                stood_at           = player.pos;
                applied_when_stood = applied;
                stood_sampled      = true;
            }
        }

        if (seconds > 0.0 && hands_here && edit_step != EditStep::Done && conn->playing() &&
            s.act().world.chunk_count() > 0 && now - started > 1.2) {
            const int reach_x = (int)std::floor(player.pos.x) + 40;   // far out of reach

            switch (edit_step) {
            case EditStep::Idle: {
                // A deadline here too. Every other step has one; without one
                // on this one a world that never offers a block of the right
                // shape leaves the sequence pending forever -- and the scripted
                // walk now waits for it, so "no suitable block" would turn into
                // "no crossing", which is a red that names the wrong thing.
                // Solid, with air above it, and something solid under it to
                // build against afterwards: this is the shape a player aiming
                // at a floor gets, and picking anything else would be measuring
                // a case nobody meets. The first such cell two to four nodes
                // out, from +3 +3 round: a single fixed cell went silent the
                // day the spawn's chess board was put on it.
                const auto fits = [&](int x, int y, int z) {
                    const auto &reg = s.act().registry;
                    const auto &w   = s.act().world;
                    return reg.is_solid(w.get_block(x, y, z)) &&
                           !reg.is_solid(w.get_block(x, y + 1, z)) &&
                           reg.is_solid(w.get_block(x, y - 1, z));
                };
                const int px = (int)std::floor(player.pos.x);
                const int by = (int)std::floor(player.pos.y) - 1;
                const int pz = (int)std::floor(player.pos.z);
                int  bx = px + 3, bz = pz + 3;
                bool found = fits(bx, by, bz);
                for (int dx = -4; dx <= 4 && !found; ++dx)
                    for (int dz = -4; dz <= 4 && !found; ++dz)
                        if (dx * dx + dz * dz >= 4 && dx * dx + dz * dz <= 18 &&
                            fits(px + dx, by, pz + dz)) {
                            bx = px + dx; bz = pz + dz; found = true;
                        }

                if (!found && now - started > 8.0) {
                    gave_up(edit_step, now - started);
                    // Which cell, and what was in it: "no block" says nothing a
                    // reader can check.
                    const auto name = [&](int y) {
                        return s.act().registry.get(s.act().world.get_block(bx, y, bz)).name;
                    };
                    std::printf("        none round it; at +3 +3, %d %d %d: '%s' over '%s', under '%s'\n",
                                bx, by, bz, name(by).c_str(), name(by - 1).c_str(),
                                name(by + 1).c_str());
                    edit_step = EditStep::Done;
                    break;
                }

                if (found) {
                    edit_x = bx; edit_y = by; edit_z = bz;
                    edit_was = s.act().world.get_block(bx, by, bz);
                    if (conn->send_edit(bx, by, bz, 0, 1, 0, true, 0)) {
                        ++edits_sent;
                        hands_ran    = true;
                        edit_started = now;
                        edit_swung   = now;
                        edit_step    = EditStep::Breaking;
                    }
                }
                break;
            }

            case EditStep::Breaking:
                if (s.act().world.get_block(edit_x, edit_y, edit_z) == kAir &&
                    conn->has_inventory()) {
                    broke_ok = true;
                    for (const auto &sl : conn->carried().slots) carried_after_break += sl.count;

                    // Back into the same cell, aimed at the block underneath
                    // it. The cell is derived by the server from that block and
                    // its top face -- this client never names it.
                    held_slot = 0;
                    if (conn->send_edit(edit_x, edit_y - 1, edit_z, 0, 1, 0, false, held_slot)) {
                        ++edits_sent;
                        edit_started = now;
                        edit_step    = EditStep::Placing;
                    }
                } else if (now - edit_started > kEditAnswer) {
                    gave_up(edit_step, now - edit_started);
                    edit_step = EditStep::Done;
                } else if (now - edit_swung > 0.15) {
                    // HELD, not tapped. Blocks declare seconds since OPEN-31,
                    // and one Edit only starts the server's clock -- a
                    // sequence that clicked once and waited would wait for
                    // ever. This is the same thing a player's finger does and
                    // the same thing dig_step does for the frame loop.
                    if (conn->send_edit(edit_x, edit_y, edit_z, 0, 1, 0, true, 0)) ++edits_sent;
                    edit_swung = now;
                }
                break;

            case EditStep::Placing:
                if (s.act().world.get_block(edit_x, edit_y, edit_z) != kAir) {
                    // The SAME block, not merely something. An item that placed
                    // whatever happened to be first in the table would fill the
                    // cell and pass a check that only asked whether it was
                    // solid again.
                    placed_back = s.act().world.get_block(edit_x, edit_y, edit_z) == edit_was;

                    // Reaching too far, which answers with a refusal and so
                    // finishes as soon as it arrives. Before the empty-hand
                    // step, which does not -- see kUseSilence.
                    if (conn->send_edit(reach_x, edit_y, edit_z, 0, 1, 0, true, 0)) {
                        ++edits_sent;
                        edit_started = now;
                        edit_step    = EditStep::Reach;
                    }
                } else if (now - edit_started > kEditAnswer) {
                    gave_up(edit_step, now - edit_started);
                    edit_step = EditStep::Done;
                }
                break;

            case EditStep::Reach:
                if (last_refusal.find("further than") != std::string::npos) reach_refused = true;
                if (!reach_refused && now - edit_started > kEditAnswer)
                    gave_up(edit_step, now - edit_started);
                if (reach_refused || now - edit_started > kEditAnswer) {
                    last_refusal.clear();

                    // And last, with nothing left in the hand. Aimed at the
                    // block that was put back, so the cell the server would
                    // derive is the free one above it -- otherwise a refusal
                    // saying "there is already something there" would be true
                    // and would not be the thing being measured.
                    if (conn->send_edit(edit_x, edit_y, edit_z, 0, 1, 0, false, held_slot)) {
                        ++edits_sent;
                        edit_started = now;
                        edit_step    = EditStep::EmptyHand;
                    } else {
                        edit_step = EditStep::Done;
                    }
                }
                break;

            case EditStep::EmptyHand:
                // An ABSENCE, waited out rather than seen: a right click with
                // an empty hand is no longer a refused placement, it is
                // core:used, and the server answers a use with nothing. What is
                // measured is that no refusal came and nothing was built. The
                // positive half -- that the mod host gets the event -- is
                // net_loopback's, where there is a mod host to ask.
                if (now - edit_started > kUseSilence) {
                    empty_hand_used = last_refusal.find("hand is empty") == std::string::npos &&
                                      s.act().world.get_block(edit_x, edit_y + 1, edit_z) == kAir;
                    edit_step = EditStep::Done;
                }
                break;

            case EditStep::Done:
                break;
            }
        }

        // The scripted placement, once the world has arrived and the server has
        // said what it holds. Aimed down at a block NEXT TO the player rather
        // than the one under their feet, and the difference is a finding rather
        // than a preference: an opening placed in the cell the player is
        // standing in engages the floor shift, the player sinks into the solid
        // block below it, and the server refuses every claim after that as a
        // position inside a block. See docs/archive/M6_FINDINGS.md 3.5 -- it is a real
        // thing a player can do, and it is not what this gate is measuring.
        // Not before the mouth's link exists, when there is one. Otherwise the
        // placement races the far world's first Portals message, and a check
        // that the link follows the NEAR opening passes because the FAR one
        // happened to arrive afterwards -- which is how it passed with that
        // rebuild sabotaged.
        const bool mouth_ready = !conn->portals().leads_away() || mouths[0].drawn_far > 0;

        // Not on a world whose pair leads away: there the scripted walk below
        // is what the gate measures, and moving the mouth out from under it
        // would measure nothing.
        if (seconds > 0.0 && !conn->portals().leads_away() && !placement_sent &&
            conn->has_portals() && mouth_ready &&
            s.act().world.chunk_count() > 0 && now - started > 1.0) {
            const int px = (int)std::floor(player.pos.x);
            const int py = (int)std::floor(player.pos.y) - 1;
            const int pz = (int)std::floor(player.pos.z);

            // The nearest block within reach with room above it -- which is the
            // question a raycast answers when a player aims at a floor.
            const int offsets[4][2] = {{2, 0}, {-2, 0}, {0, 2}, {0, -2}};
            for (const auto &o : offsets) {
                const int bx = px + o[0], bz = pz + o[1];
                if (!s.act().registry.is_solid(s.act().world.get_block(bx, py, bz))) continue;
                if (s.act().registry.is_solid(s.act().world.get_block(bx, py + 1, bz))) continue;

                placed_x = bx;
                placed_y = py;
                placed_z = bz;
                version_before_placement = conn->portals_version();
                placement_sent = conn->send_place_portal(0, bx, py, bz, 0, 1, 0);
                break;
            }
        }

        // The floor portal under the player's own feet. This is OPEN-5's case,
        // and it is in the gate rather than avoided by it: the opening is the
        // node the player is standing on, the client's floor shift sinks them a
        // tenth of a node into it, and the server has to accept that claim or
        // the player is corrected once per claim forever.
        if (seconds > 0.0 && placement_sent && !underfoot_sent &&
            !conn->portals().leads_away() &&
            conn->portals_version() > version_before_placement && now - started > 2.5) {
            // Standing on the node, not on the corner of four of them. The
            // spawn is on a node boundary, so the 0.6-wide box straddles two
            // nodes and half of it is over solid ground beside a one-node hole
            // -- the player cannot fall through it and should not. Measured as
            // 106 corrections before the move was added, all of them for the
            // node NEXT to the opening.
            const int bx = (int)std::floor(player.pos.x);
            const int bz = (int)std::floor(player.pos.z);
            player.pos.x = (float)bx + 0.5f;
            player.pos.z = (float)bz + 0.5f;

            teleports_before_underfoot = conn->teleports();
            applied_at_underfoot       = applied;
            stood_at                   = player.pos;
            stood_taken                = true;
            underfoot_sent = conn->send_place_portal(1, bx, (int)std::floor(player.pos.y) - 1,
                                                       bz, 0, 1, 0);
        }

        // Walking into a mouth.
        //
        // This is the one crossing the server on THIS side cannot decide: the
        // destination is a world it has no blocks for. So the client asks the
        // world through the opening to take it, sending the opening it walked
        // into and its pose in it -- and that world computes the arrival
        // against its own opening. See ChangeScope.
        //
        // Either mouth, one at a time: a player may walk into whichever door
        // they like, and only one crossing is ever asked for.
        // From last frame's feet, unless that was another world, or somewhere
        // the body did not run from (a correction, a respawn).
        if (trigger_world != s.active || length(player.pos - trigger_from) > 6.0f)
            trigger_from = player.pos;
        s.act().sealed = conn->sealed();
        for (int m = 0; m < net::kMaxMouths && !crossing_pending; ++m) {
            const bool linked = mouth_world(s.act(), m) >= 0;
            if (!linked || !s.act().portals || (conn->sealed() >> m & 1u)) continue;

            Mouth              &mo   = mouths[m];
            net::ClientSession *far  = net_sessions.get(mo.conn);
            const Portal       &near = mouth_portal(s.act(), m);
            PortalTriggerState &trig = mouth_trigger(s.act(), m);
            const PortalLink   &link = mouth_link(s.act(), m);

            // Or caught in its block by the eye, armed or not, as the server
            // takes a body in a pair's (portal_swallows).
            if (far && far->playing() &&
                (trigger_along(near, trig, trigger_from, player.pos, player.velocity,
                               player.eye().y - player.pos.y) ||
                 portal_swallows(near, player.pos, player.eye().y - player.pos.y, player.velocity))) {
                ++mouth_triggers;
                bool held_this_step = false;

                // What the world on the other side last said about itself.
                //
                // Until this, the first news that a world was full arrived as a
                // refused crossing -- a round trip after the player had already
                // walked into the opening. The heartbeat has been carrying the
                // answer since it existed and nothing read it: the mouth knows,
                // and the crossing does not have to be attempted to find out.
                if (!mouth_is_passable(far)) {
                    ++crossings_held_back;
                    if (held_back_at < 0.0) held_back_at = now;
                    const net::Heartbeat &b = far->heartbeat();
                    std::printf("did not walk into the opening: %s says %u/%u players, no room "
                                "to walk in\n", mo.spec.c_str(), b.players, b.max_players);
                    held_this_step = true;
                } else if (!mouth_has_ground(s, world_of(mo.conn), link.dst.center)) {
                    // Held for a different reason, and it is worth keeping the
                    // two apart: a full world is an answer and this is a wait.
                    // The window is streaming; the block the player would come
                    // out in has not arrived yet, and crossing now would put
                    // them in a world that is still turning up.
                    ++crossings_waited_for_ground;
                    if (waited_at < 0.0) waited_at = now;
                    if (crossings_waited_for_ground == 1)
                        std::printf("waited at the opening: %s has %d chunk(s) so far and none of "
                                    "them is the one to come out in\n", mo.spec.c_str(),
                                    far->chunks_received());
                    held_this_step = true;
                } else if ((mo.ticket_asked && !mo.ticket_settled) || (mo.ticket_renew && !g_no_passive)) {
                    // And a third: the ticket for this crossing is on its way,
                    // or a new one is about to be asked for (a stale one is
                    // refused).
                    // Walking in without it is walking in with none, or with
                    // the last one -- which after a crossing names the world
                    // being stood in, and the world through the opening refuses
                    // it. Bounded: the ask gives up after a second.
                    held_this_step = true;
                }

                // Which opening over there. Not this client's choice: it is on
                // the link, sent by the world being left, and copied through.
                //
                // Recorded only when the crossing is actually asked for: these
                // are what the arrival is checked against, and filling them in
                // for a crossing that never happened would leave the gate
                // comparing an arrival with a mouth from a different approach.
                if (!held_this_step) {
                    const Vec3     f      = player.forward();
                    const uint32_t out_of = conn->portals().away[m].portal;
                    crossed_out_of        = out_of;
                    shown_before_cross    = link.dst.center;
                    shown_taken           = true;
                    crossing_mouth        = m;

                    ++crossings_attempted;
                    far->send_enter(out_of, desc_from_portal(near), player.pos.x, player.pos.y,
                                    player.pos.z, f.x, f.y, f.z, player.velocity.x,
                                    player.velocity.y, player.velocity.z,
                                    mo.have_ticket ? &mo.held_ticket : nullptr);
                    crossing_pending = true;
                    crossing_sent    = now;
                    std::printf("walked into the opening; asking %s to take us out of its "
                                "opening %u (round trip %.0f ms)\n", mo.spec.c_str(), out_of,
                                far->round_trip() * 1e3);
                }
            }
        }

        trigger_from  = player.pos;
        trigger_world = s.active;

        // "Your session continues over there."
        //
        // The server decided it: it owns the opening and it saw the player step
        // into it. What this client does with it depends on what it already
        // has. A live window onto that world means the seamless crossing is
        // available and this is not needed -- taken and dropped, and counted,
        // because the two paths are worth telling apart.
        {
            net::Redirect rd;
            if (conn->take_redirect(rd)) {
                // Which of this world's mouths the address is for.
                const LinkDest    rd_to   = link_dest(*conn, rd.host, rd.port, rd.room);
                const std::string rd_spec = rd_to.spec;
                int               rd_mouth = 0;
                const net::PortalAway *rd_away = nullptr;
                for (int m = 0; m < net::kMaxMouths; ++m) {
                    const net::PortalAway &a = conn->portals().away[m];
                    if (a.leads_away() && link_dest(*conn, a.host, a.port, a.room).spec == rd_spec) {
                        rd_mouth = m;
                        rd_away  = &a;
                        break;
                    }
                }
                const int           window  = mouths[rd_mouth].conn;
                net::ClientSession *far_now = net_sessions.get(window);
                const bool seamless_available =
                    far_now != nullptr && far_now->playing() &&
                    (size_t)window < conn_spec.size() && conn_spec[(size_t)window] == rd_spec;

                if (seamless_available || crossing_pending) {
                    ++redirects_ignored;
                } else if (!fallback_pending) {
                    // The fallback. Not a different arrival: the SAME messages
                    // the seamless path uses, against a connection that has not
                    // been streamed anything yet. That difference is the
                    // loading screen, and it is the only difference.
                    fallback_spec   = rd_spec;
                    fallback_portal = rd.portal_id;
                    fallback_from   = desc_from_portal(mouth_portal(s.act(), rd_mouth));
                    fallback_pos    = player.pos;
                    fallback_look   = player.forward();
                    fallback_vel    = player.velocity;

                    net::ClientConfig fcfg = base;
                    fcfg.host       = rd_to.host;
                    fcfg.port       = rd_to.port;
                    fcfg.room       = rd_to.room;
                    fcfg.pin_server = false;
                    fcfg.scope      = net::Scope::Observe;
                    fcfg.has_ticket = rd.has_ticket;
                    if (rd.has_ticket) fcfg.ticket = rd.ticket;
                    // The key the link names, as the window would check it:
                    // a redirect is the same crossing, the slow way.
                    fcfg.expect_signer = rd_away && rd_away->has_key;
                    if (fcfg.expect_signer) std::memcpy(fcfg.signer, rd_away->key, net::kSignPublicBytes);
                    fallback_has_ticket = rd.has_ticket;
                    fallback_ticket     = rd.ticket;

                    fallback_world = s.add();
                    std::string ferr;
                    fallback_conn = net_sessions.open(fcfg, self_name, now, ferr);
                    conn_grew();
                    if (fallback_conn == net::ClientSessionSet::kNoSession) {
                        std::printf("redirected to %s and cannot reach it: %s\n",
                                    fallback_spec.c_str(), ferr.c_str());
                    } else {
                        conn_spec[(size_t)fallback_conn] = fallback_spec;
                        net_sessions.get(fallback_conn)->use(s.at(fallback_world).world,
                                                             s.at(fallback_world).registry,
                                                             s.at(fallback_world).movement);
                        fallback_pending = true;
                        fallback_asked   = false;   // a second redirect asks again
                        fallback_started = now;
                        ++redirects_taken;
                        std::printf("redirected to %s opening %u: %s%s\n",
                                    fallback_spec.c_str(), rd.portal_id, rd.reason.c_str(),
                                    rd.has_ticket ? " (with a ticket)" : "");
                    }
                }
            }
        }

        // The fallback, once that connection is in: the same request the
        // seamless path sends, from the pose the player had when they stepped
        // into the opening.
        if (fallback_pending && !fallback_asked) {
            net::ClientSession *fb = net_sessions.get(fallback_conn);
            if (fb && fb->playing()) {
                fallback_asked = fb->send_enter(fallback_portal, fallback_from, fallback_pos.x,
                                                fallback_pos.y, fallback_pos.z, fallback_look.x,
                                                fallback_look.y, fallback_look.z,
                                                fallback_vel.x, fallback_vel.y, fallback_vel.z,
                                                fallback_has_ticket ? &fallback_ticket : nullptr);
            } else if (fb && (fb->given_up() || now - fallback_started > 15.0)) {
                fallback_pending = false;
                std::printf("the redirect went nowhere: %s\n", why_not(*fb));
            }
        }

        if (fallback_pending && fallback_asked) {
            net::ClientSession *fb = net_sessions.get(fallback_conn);
            net::Teleport       arrived;
            net::ScopeDenied    no;
            if (fb && fb->take_teleport(arrived)) {
                fallback_pending = false;
                // Chunks AT THE MOMENT OF ARRIVAL, which is the number that
                // says the world was not there yet -- the gate reads what the
                // session has by the end of the run for the rest of it.
                // The first arrival's: a way back may find a few streamed
                // while the reconnect waited for its answer.
                if (crossed_once == 0) fallback_chunks = fb->chunks_received();
                fallback_seconds = now - fallback_started;
                ++crossed_once;
                chunks_before_arriving = 0;   // nothing had been streamed: that IS the cost

                carry_kart(s.act(), conn->welcome().models, s.at(fallback_world),
                           fb->welcome().models);
                conn->close();                // the world being left is not watched: no window
                arrive_from_portal(s, player, arrived, fallback_world);

                left_conn = net::ClientSessionSet::kNoSession;   // closed: nothing behind
                self_conn = fallback_conn;
                conn      = net_sessions.get(self_conn);
                s.act().bodies.clear();
                s.act().player_model_id = conn->welcome().player_model;
                add_own_body(s, player, conn->welcome().player_model);
                conn_world[(size_t)self_conn] = fallback_world;
                s.act().link_session = -1;
                s.act().portals      = false;

                drawn_portals  = 0;
                for (Mouth &mo : mouths) mo = Mouth{};
                crossed_out_of = fallback_portal;
                arrived_at          = player.pos;

                std::printf("came out at %.2f %.2f %.2f on %s after a reconnect: %.0f ms from "
                            "the opening, with %d chunks of that world in hand\n",
                            (double)player.pos.x, (double)player.pos.y, (double)player.pos.z,
                            fallback_spec.c_str(), fallback_seconds * 1e3, fallback_chunks);
            } else if (fb && fb->take_scope_denial(no)) {
                fallback_pending = false;
                std::printf("the world we were redirected to refused us: %s\n",
                            no.detail.c_str());
            }
        }

        // And the answer to that, which is the arrival.
        if (crossing_pending) {
            net::Teleport       arrived;
            const int           far_conn  = mouths[crossing_mouth].conn;
            const int           far_world = world_of(far_conn);
            net::ClientSession *far       = net_sessions.get(far_conn);

            if (far && far->take_teleport(arrived)) {
                crossing_pending = false;
                ++crossed_once;
                grant_delay = now - crossing_sent;
                chunks_before_arriving = far->chunks_received();

                // Asked here and not at the end of the run: by then everything
                // has arrived and the answer is yes whatever happened. What the
                // gate wants to know is whether the block the server just named
                // was already in hand at the moment of the crossing.
                ground_was_ready = mouth_has_ground(s, far_world,
                                                    Vec3{arrived.x, arrived.y, arrived.z});

                // The world being left is told, on the connection that is
                // already open: a player who has gone through is a window onto
                // the world they left. No disconnect, so the way back needs no
                // handshake.
                conn->send_observe();

                carry_kart(s.act(), conn->welcome().models, s.at(far_world),
                           far->welcome().models);
                arrive_from_portal(s, player, arrived, far_world);

                left_conn = self_conn;
                self_conn = far_conn;
                conn      = net_sessions.get(self_conn);
                // Only the world played in has its bodies gathered every frame
                // (from its connection's players); the one left keeps whatever
                // the last frame there put in -- this body among them, which
                // then stood in the window home as a robot left behind (from
                // play). A window shows no bodies, as it did before it was
                // walked through.
                if (const int was = conn_world[(size_t)left_conn]; was >= 0) s.at(was).bodies.clear();

                // This frame's bodies were gathered in the world just left:
                // the one arrived in shows its own again, or the crossing
                // frame draws no player at all (a kart vanished for a frame).
                s.act().bodies.clear();
                s.act().player_model_id = conn->welcome().player_model;
                add_own_body(s, player, conn->welcome().player_model);

                // The world just left is already streamed and meshed, which is
                // the whole point: it is the window home, and nothing is built
                // again for it.
                conn_meshed[(size_t)left_conn] = 1;
                if (net::ClientSession *was = net_sessions.get(left_conn))
                    conn_media_version[(size_t)left_conn] = was->media_version();

                // Everything that was about the old world's mouths. The new
                // active world has not had its openings applied yet, and they
                // are relinked from what it says -- the one leading back finds
                // the session just left rather than opening another.
                //
                // The ticket in hand was the pass INTO this world, signed by the
                // one just left. Shown back there it is a ticket for a different
                // server -- measured, fifteen refusals in a row on the way home.
                // The way back needs one from here, and a fresh mouth asks.
                drawn_portals    = 0;
                for (Mouth &mo : mouths) mo = Mouth{};
                seed_after_cross = true;

                arrived_at   = player.pos;
                arrived_when = now;

                std::printf("came out at %.2f %.2f %.2f in the other world; now playing on %s, "
                            "watching %s\n", (double)player.pos.x, (double)player.pos.y,
                            (double)player.pos.z, conn_spec[(size_t)self_conn].c_str(),
                            conn_spec[(size_t)left_conn].c_str());
            } else if (net::ScopeDenied no; far && far->take_scope_denial(no)) {
                // Told, rather than waited out. The reason is what the far
                // world decided, and it arrives on the same tick it decided it.
                crossing_pending = false;
                ++crossings_denied;
                denial_delay = now - crossing_sent;
                std::printf("the world through the opening refused us after %.1f ms: %s (%s)\n",
                            denial_delay * 1e3, no.detail.c_str(),
                            net::refuse_reason_text(no.reason));
                // Refused for the ticket: the next step in carries a new one
                // rather than the same one again, which was refused forever.
                if (no.reason == net::RefuseReason::BadTicket) mouths[crossing_mouth].ticket_renew = true;
            } else if (!far || !far->playing()) {
                // No timer, on purpose (TODO.md OPEN-14). The request and its
                // answer are both reliable, and the far world answers every
                // request it can read, yes or no -- so while that session lives
                // the answer is on its way. A wait of its own was a guess at
                // how long ENet retransmits: 5 s on loopback against up to 72 s
                // the far server's ENet may spend on the answer, and a client
                // that gave up on a yes stood in a world the server had just
                // moved it out of.
                crossing_pending = false;
                ++crossings_lost;
                std::printf("the world through the opening went away before it answered "
                            "(after %.1f s); staying here\n", now - crossing_sent);
            }
        }

        // And the answer, whenever it changes. Applied here rather than inside
        // the session because it writes three copies of the opening in the
        // renderer's own types, which the network layer has no business
        // knowing about.
        if (conn->portals_version() != drawn_portals) {
            drawn_portals = conn->portals_version();
            apply_server_portals(s, conn->portals());

            const net::Portals &ps = conn->portals();
            std::printf("the server's openings, version %u:%s%s\n", ps.version,
                        ps.has(0) ? "" : " (first not placed)",
                        ps.has(1) ? "" : " (second not placed)");
            for (int i = 0; i < 2; ++i) {
                if (!ps.has(i)) continue;
                std::printf("        %d at %.2f %.2f %.2f, axis %u, facing %+d\n", i + 1,
                            (double)ps.opening[i].cx, (double)ps.opening[i].cy,
                            (double)ps.opening[i].cz, (unsigned)ps.opening[i].axis,
                            (int)ps.opening[i].normal_sign);
            }
        }

        // The worlds on the other side of the mouths.
        //
        // AFTER the openings above have been applied, and that ordering is a
        // defect this had: the link's near end is read from portal_a, so
        // building it before the new portal_a lands draws the mouth where it
        // used to be, and the version test then says nothing has changed.
        //
        // Three things in order for each mouth, each waiting for the one before
        // it: a connection, a renderer for the world it streams into, and the
        // link once that world says where its own opening is. None of them can
        // be done at startup -- the destination arrives from the server, the
        // block table arrives from the join, and the far opening arrives from
        // the far server.
        if (conn->portals().leads_away()) saw_a_mouth = true;

        // Tickets are asked for one mouth at a time: the answer does not say
        // which opening it was for, so two in flight would be two answers
        // nobody could tell apart.
        bool ticket_in_flight = false;
        for (const Mouth &mo : mouths)
            ticket_in_flight = ticket_in_flight || (mo.ticket_asked && !mo.ticket_settled);

        for (int m = 0; m < net::kMaxMouths; ++m) {
            const net::PortalAway &away = conn->portals().away[m];
            Mouth                 &mo   = mouths[m];
            if (!away.leads_away()) {
                if (!mo.spec.empty()) mo = Mouth{};
                continue;
            }
            const LinkDest    to   = link_dest(*conn, away.host, away.port, away.room);
            const std::string spec = to.spec;
            if (mo.spec != spec) {
                mo      = Mouth{};
                mo.spec = spec;
            }

            // Is the world through this opening one this client already has a
            // session to -- the world just left, or the other mouth's?
            //
            // By KEY when both ends have one, and by address only when they do
            // not. An address is a route: measured in two containers behind
            // NAT, the far server told this client "that world is at
            // 172.31.8.1:25891" about the very server it was already talking to
            // as 127.0.0.1:25891. Comparing the strings said no, so the client
            // opened a second session to a machine it was already connected to
            // -- and that one was refused for want of a ticket. On loopback the
            // two strings are always equal and none of it can be seen.
            if (mo.conn == net::ClientSessionSet::kNoSession) {
                int found = net::ClientSessionSet::kNoSession;
                for (int pass = 0; pass < 2 && found == net::ClientSessionSet::kNoSession; ++pass) {
                    for (int id = 0; id < net_sessions.count(); ++id) {
                        if (id == self_conn || world_of(id) < 0 || !alive(id)) continue;
                        net::ClientSession *w = net_sessions.get(id);
                        const bool same =
                            pass == 0 ? (away.has_key && w->playing() && w->same_server_as(away.key) &&
                                         w->room() == away.room)
                                      : ((size_t)id < conn_spec.size() &&
                                         conn_spec[(size_t)id] == spec);
                        if (same) {
                            found = id;
                            break;
                        }
                    }
                }
                if (found != net::ClientSessionSet::kNoSession) {
                    // Said when that server answers under another address:
                    // opening a second session to it is what this exists to stop.
                    if (conn_spec[(size_t)found] != spec) {
                        std::printf("the world through the opening is %s, which this client "
                                    "already has open as %s -- reusing it\n", spec.c_str(),
                                    conn_spec[(size_t)found].c_str());
                        ++windows_reused;
                    }
                    mo.conn  = found;
                    mo.tried = true;
                }
            }

            // The ticket, before the connection. DESIGN.md 4.4: the world being
            // stood in signs a statement about this player, and the world
            // through the opening admits them because it trusts the signer.
            //
            // Asked for once and waited for briefly, because a world that
            // issues none is the ordinary case and must not cost a hang: if
            // nothing comes back the connection is opened without one, and it
            // works wherever the door is open to anybody. Both scopes in one
            // ticket -- watching and walking through are one journey, and
            // asking twice would put a round trip in the middle of a crossing.
            // Only a mouth among the nearest (kMouthsShown) opens its window:
            // a sandbox with a door to every world would otherwise stream them
            // all at once. Nearest to where the server put the body (a join is
            // placed by a Teleport), not to where it stood before. ponytail: a
            // window once open stays open; closing the far ones wants a Scene
            // that can drop a world.
            const bool near_here = conn->teleports() > 0 && mouth_near(m, player.pos, [&](int k, Vec3 &c) {
                const net::Portals &ps = conn->portals();
                if (!ps.away[k].leads_away() || !ps.has(k)) return false;
                c = Vec3{ps.opening[k].cx, ps.opening[k].cy, ps.opening[k].cz};
                return true;
            });
            const bool opening_new =
                mo.conn == net::ClientSessionSet::kNoSession && !mo.tried && near_here;
            // Another room of the server already being played on needs no
            // ticket: it is the same server, it knows this player, and a
            // ticket from itself to itself is a check that can only go stale
            // -- one held across the arena's fight was refused at its way out.
            // Settled as "none": nothing asked, nothing presented. Tickets are
            // for a world on ANOTHER server, which has no reason to trust us.
            // ponytail: a server run with --require-ticket and several rooms
            // would refuse this; none is (it is for crossings between servers),
            // and if one ever is, its rooms should admit a player of their own.
            if (to.host == conn->host() && to.port == conn->port()) {
                mo.ticket_asked   = true;
                mo.ticket_settled = true;
                mo.have_ticket    = false;
                mo.ticket_renew   = false;
            }
            // A ticket lives kTicketLifetime and was asked for once, when the
            // window opened. Held across a long stay -- an arena whose way out
            // opens only once it is won -- it was presented after it ran out,
            // and the world through the opening refused every step into it.
            // Asked again halfway through its life, by this machine's clock so
            // two clocks apart cannot make it look fresh.
            if (mo.have_ticket && mo.ticket_settled &&
                now - mo.ticket_got_at > 0.5 * (double)(mo.held_ticket.expires - mo.held_ticket.issued))
                mo.ticket_renew = true;
            if (mo.ticket_renew && mo.ticket_settled) {
                mo.ticket_asked   = false;
                mo.ticket_settled = false;
            }
            if (g_no_passive) {
                // No window behind the mouth. The server's Redirect is the only
                // way through it, which is the fallback being measured.
            } else if (!mo.ticket_asked && !ticket_in_flight &&
                       (opening_new || crossed_once > 0 || mo.ticket_renew)) {
                // After a crossing the window usually exists already -- it is
                // the world just left -- and the way back still needs its own
                // ticket.
                mo.ticket_asked    = conn->send_ticket_request(
                    (uint8_t)(net::kTicketObserve | net::kTicketEnter), (uint8_t)m);
                mo.ticket_asked_at = now;
                ticket_in_flight   = mo.ticket_asked;
            }
            if (mo.ticket_asked && !mo.ticket_settled) {
                net::Ticket answer;
                if (conn->take_ticket(answer)) {
                    mo.ticket_settled = true;
                    mo.ticket_renew   = false;
                    ticket_in_flight  = false;
                    if (answer.granted) {
                        mo.have_ticket   = true;
                        mo.held_ticket   = answer.ticket;
                        mo.ticket_got_at = now;
                        std::printf("this world vouched for us at %s, opening %u, for %llu "
                                    "seconds\n", spec.c_str(),
                                    (unsigned)answer.ticket.portal_id,
                                    (unsigned long long)(answer.ticket.expires -
                                                          answer.ticket.issued));
                    } else {
                        std::printf("this world issues no ticket for %s: %s\n", spec.c_str(),
                                    answer.detail.c_str());
                    }
                } else if (now - mo.ticket_asked_at > 1.0) {
                    mo.ticket_settled = true;
                    mo.ticket_renew   = false;
                    ticket_in_flight  = false;
                    std::printf("no answer about a ticket for %s; going without one\n",
                                spec.c_str());
                }
            }

            if (!g_no_passive && opening_new && mo.ticket_settled) {
                mo.tried = true;

                // The same identity and the same encryption as the player's own
                // join. Not the same PIN: that key belongs to the server this
                // client is standing in, and the world through the opening is a
                // different one.
                net::ClientConfig wcfg = base;
                wcfg.host       = to.host;
                wcfg.port       = to.port;
                wcfg.room       = to.room;
                wcfg.pin_server = false;
                // But WHO it is, when the link names it: a crossing lands only
                // on the server the link was made to.
                wcfg.expect_signer = away.has_key;
                if (away.has_key) std::memcpy(wcfg.signer, away.key, net::kSignPublicBytes);
                wcfg.scope      = net::Scope::Observe;
                wcfg.has_ticket = mo.have_ticket;
                if (mo.have_ticket) wcfg.ticket = mo.held_ticket;

                // A window onto somebody else's world keeps trying, at a rate
                // that goes down. Not because outages are expected -- because
                // the alternative that is also quiet is giving up, and then the
                // portal never comes back when that server does.
                wcfg.retry_first = 2.0;
                wcfg.retry_max   = 60.0;

                const int   world = s.add();
                std::string werr;
                const int   id    = net_sessions.open(wcfg, self_name, now, werr);
                conn_grew();
                if (id == net::ClientSessionSet::kNoSession) {
                    std::printf("the opening leads to %s and this client cannot reach it: %s\n",
                                spec.c_str(), werr.c_str());
                } else {
                    conn_spec[(size_t)id]  = spec;
                    conn_world[(size_t)id] = world;
                    mo.conn                = id;
                    net_sessions.get(id)->use(s.at(world).world, s.at(world).registry,
                                              s.at(world).movement);
                    std::printf("the opening leads to %s -- looking into it as world %d\n",
                                spec.c_str(), world);
                }
            }

            net::ClientSession *far   = net_sessions.get(mo.conn);
            const int           world = world_of(mo.conn);

            // The rule that keeps a dead neighbour out of the log: say
            // something while the wait is still growing, once when it stops
            // growing, and nothing after that.
            if (far && far->retrying() && far->retry_delay() != mo.said_retry_delay) {
                mo.said_retry_delay = far->retry_delay();
                std::printf("nothing behind the opening at %s (%s); trying again in %.0f s\n",
                            spec.c_str(), net::client_state_text(far->state()),
                            mo.said_retry_delay);
            } else if (far && far->given_up() && !mo.said_gave_up) {
                mo.said_gave_up = true;
                std::printf("the world through the opening will not have us: %s\n",
                            why_not(*far));
            } else if (far && far->playing() && mo.said_retry_delay >= 0.0) {
                mo.said_retry_delay = -1.0;
                std::printf("the world through the opening is back\n");
            }

            // What that world says it currently is. Worth a line when it
            // changes, because it is the difference between a player learning
            // the far world is full now and learning it while standing in the
            // opening with a refusal in their face.
            if (far && far->has_heartbeat() && far->heartbeat().flags != mo.said_far_flags) {
                mo.said_far_flags = far->heartbeat().flags;
                const net::Heartbeat &b = far->heartbeat();
                std::printf("the world through the opening at %s: %u/%u players, %u/%u "
                            "windows, %s, %s\n", spec.c_str(), b.players, b.max_players,
                            b.observers, b.max_observers,
                            b.watchable() ? "open to portals" : "closed to portals",
                            b.enterable() ? "room to walk in" : "no room to walk in");
            }

            if (far && far->playing() && world >= 0) {
                const size_t id = (size_t)mo.conn;

                // The renderer for that world, once its block table exists.
                // Built here and not at startup because the atlas comes from
                // the registry and the registry comes from the welcome.
                if (!conn_meshed[id]) {
                    conn_meshed[id] =
                        s.at(world).world_render.init(s.at(world).registry, world_media(far)) ? 1
                                                                                              : 0;
                    if (!conn_meshed[id])
                        std::printf("could not build a renderer for world %d\n", world);
                    else
                        wire_model_draw(s, world);
                    conn_media_version[id] = far->media_version();
                }

                // The files arrive after the welcome, so the first atlas is
                // built out of whatever had already been cached and the rest
                // land over the next few frames. Rebuilt when the media this
                // world offered have changed and settled, not once: a one-shot
                // "when complete" fires before the offer has even arrived,
                // because an offer nobody has sent yet is an offer with nothing
                // outstanding in it.
                if (far->media_complete() && conn_media_version[id] != far->media_version()) {
                    s.at(world).world_render.rebuild_atlas(s.at(world).world, s.at(world).registry,
                                                           world_media(far), &s.at(world).items);
                    conn_media_version[id] = far->media_version();
                    media_atlas_rebuilds += 1;
                }

                // Rebuilt when EITHER side moves. The far opening changing is
                // obvious; the near one is the case that was missed -- a player
                // who moves the mouth leaves the link drawn where the mouth
                // used to be, which is the same drift the local pair has its
                // own check for.
                const bool far_moved  = far->portals_version() != mo.drawn_far;
                const bool near_moved = conn->portals_version() != mo.linked_near;
                if (conn_meshed[id] && (far_moved || near_moved)) {
                    mo.drawn_far   = far->portals_version();
                    mo.linked_near = conn->portals_version();

                    // The first link after arriving in this world: seed the
                    // trigger against the opening we just came out of, or the
                    // next claim walks straight back into it.
                    if (seed_after_cross) {
                        seed_after_cross = false;
                        // Out of a mouth here; one a mod opened is the server's.
                        if (crossed_out_of < (uint32_t)net::kMaxMouths) {
                            const int out = (int)crossed_out_of;
                            portal_seed_arrival(mouth_portal(s.act(), out),
                                                mouth_trigger(s.act(), out), player.pos);
                        }
                    }

                    if (link_to_remote(s, m, world, far->portals(), (int)away.portal) && far_moved)
                        std::printf("world %d shows its opening at %.2f %.2f %.2f\n", world,
                                    (double)s.at(world).portal_a.center.x,
                                    (double)s.at(world).portal_a.center.y,
                                    (double)s.at(world).portal_a.center.z);
                }
            }
        }

        {
            char line[256];
            std::snprintf(line, sizeof(line),
                          "net: %s  %d chunks  %zu players  %ld corrections  "
                          "%.2f MB in / %.2f MB out",
                          net::client_state_text(conn->state()), conn->chunks_received(),
                          conn->players().size(), applied,
                          (double)conn->bytes_received() / (1024.0 * 1024.0),
                          (double)conn->bytes_sent() / (1024.0 * 1024.0));
            s.net_status = line;
        }

        // A line on stdout when the world stops arriving, so a run that is
        // headless or being watched from a terminal says something.
        if (reported_chunks != conn->chunks_received() && now - last_line > 1.0) {
            last_line       = now;
            reported_chunks = conn->chunks_received();
            std::printf("streaming: %d chunks, %d let go, %zu in the world, %.2f MB in\n",
                        conn->chunks_received(), conn->chunks_dropped(),
                        s.act().world.chunk_count(),
                        (double)conn->bytes_received() / (1024.0 * 1024.0));
        }

        // Both worlds, through the one function that knows there may be two --
        // the world being walked in at full speed, the world inside the opening
        // at the passive budget. This used to be a bare sync of the active
        // world, which meant a mouth onto another server showed a hole full of
        // sky: the far world had chunks and no geometry. Caught by the gate
        // reading 0 vertices, in exactly the words play reported it in for the
        // local case.
        sync_sessions(s);

        // The bar, from what the server last said. Never from anything this
        // client worked out for itself: DESIGN.md 3.4 puts the inventory on the
        // server, so the only honest thing to draw is the last thing it sent.
        if (conn->has_inventory()) {
            const net::InventoryState &st = conn->carried();
            s.hotbar.slots.resize(st.slots.size());
            for (size_t i = 0; i < st.slots.size(); ++i) {
                s.hotbar.slots[i].item  = st.slots[i].item;
                s.hotbar.slots[i].count = st.slots[i].count;
                s.hotbar.slots[i].used  = st.slots[i].used;
                s.hotbar.slots[i].ench  = st.slots[i].ench;
                s.hotbar.slots[i].tag   = st.slots[i].tag;
            }
            // The hand is this client's, not the server's: the slot travels
            // with the click (net::Edit) and the server only ever reports where
            // it last landed, which is one round trip behind the key that was
            // pressed.
            s.hotbar.selected = held_slot;
            s.hotbar.items    = &conn->items();
        }

        // And how much of this player is left, which is the server's number and
        // nothing this side works out. A maximum of zero until the first
        // message, which is what says "nobody has told us yet" -- drawn as
        // empty it would tell everybody who just joined that they are dying.
        // A blow's knock, into the body's speed; its physics spends it. Not
        // into a kart, whose chassis the server steps from its keys alone.
        if (net::Push push; conn->take_push(push) && !player.vehicle && !player.hanging) {
            // A launch sets the speed: a second launch plate throws as hard
            // as the first, whatever the body brought onto it.
            if (push.set) player.velocity.x = 0.0f, player.velocity.z = 0.0f, player.velocity.y = 0.0f;
            player.velocity.x += push.x;
            player.velocity.z += push.z;
            player.velocity.y = std::max(player.velocity.y, push.y);
            if (push.y > 0.0f) player.on_ground = false;
        }

        if (conn->has_vitals()) {
            s.hotbar.health     = conn->vitals().health;
            s.hotbar.health_max = conn->vitals().max;
        } else {
            s.hotbar.health     = 0;
            s.hotbar.health_max = 0;
        }

        // And the chest, from the same place and for the same reason. A
        // container is one Inventory on the SERVER with any number of people
        // looking into it, so a client that worked out its own contents would
        // be the one copy that is wrong the moment somebody else moves a stack.
        if (const net::ContainerState *cs = conn->container()) {
            if (!s.hotbar.chest_open) {
                // Opened from under the player: the bag comes up with it, or
                // they are standing in the world with their hands in a box
                // they cannot see.
                s.hotbar.open  = true;
                s.hotbar.press = -1;
                plat_cursor_grab(false);
                std::printf("a chest is open: %d slot(s) at %d %d %d\n", (int)cs->slots.size(),
                            cs->x, cs->y, cs->z);
                play_at(s, s.sounds.chest,
                        Vec3{(float)cs->x + 0.5f, (float)cs->y + 0.5f, (float)cs->z + 0.5f}, s.active);
            }
            s.hotbar.chest_open = true;
            s.hotbar.chest_roles       = cs->roles;
            s.hotbar.chest_scroll      = cs->scroll;
            s.hotbar.chest_scroll_size = cs->scroll_size;
            s.hotbar.chest_search      = cs->search;
            if (!(s.typing.on && s.typing.panel == "core:chest")) s.hotbar.chest_query = cs->query;
            if (cs->progress != s.hotbar.chest_progress || cs->rate != s.hotbar.chest_rate) {
                s.hotbar.chest_progress = cs->progress;
                s.hotbar.chest_rate     = cs->rate;
                s.hotbar.chest_since    = plat_time();
            }
            s.hotbar.chest.resize(cs->slots.size());
            for (size_t i = 0; i < cs->slots.size(); ++i) {
                s.hotbar.chest[i].item  = cs->slots[i].item;
                s.hotbar.chest[i].count = cs->slots[i].count;
                s.hotbar.chest[i].used  = cs->slots[i].used;
                s.hotbar.chest[i].ench  = cs->slots[i].ench;
                s.hotbar.chest[i].tag   = cs->slots[i].tag;
            }
        } else {
            if (s.hotbar.chest_open) std::printf("the chest closed\n");
            s.hotbar.chest_open = false;
            s.hotbar.chest.clear();
        }

        // And the craft grid, from the same place. There is no "open" in it:
        // the SIZE is the state, so a grid that came back three by three means
        // this player is standing at a table -- and the bag comes up with it,
        // the same as a chest.
        if (conn->has_craft()) {
            const net::CraftState &cs = conn->crafting();

            if (cs.w > 2 && s.hotbar.craft_w <= 2 && !s.hotbar.open) {
                s.hotbar.open  = true;
                s.hotbar.press = -1;
                plat_cursor_grab(false);
                std::printf("a crafting table is open: %dx%d at %d %d %d\n", (int)cs.w,
                            (int)cs.h, cs.x, cs.y, cs.z);
            }

            s.hotbar.craft_w = cs.w;
            s.hotbar.craft_h = cs.h;
            s.hotbar.craft.resize(cs.slots.size());
            for (size_t i = 0; i < cs.slots.size(); ++i) {
                s.hotbar.craft[i].item  = cs.slots[i].item;
                s.hotbar.craft[i].count = cs.slots[i].count;
                s.hotbar.craft[i].tag   = cs.slots[i].tag;
            }
            s.hotbar.craft_result.item  = cs.result.item;
            s.hotbar.craft_result.count = cs.result.count;
            s.hotbar.craft_result.tag   = cs.result.tag;
        }

        camera_follow(s, player, dt);
        const Vec3 cam = camera_eye(s, player);
        render_frame(s, cam, camera_forward(s, player, cam), camera_up(s, player), show_debug,
                     "WASD move  F fly  LMB break  RMB place  1-9 slot  E bag  "
                     "CTRL+LMB/RMB portal end  ESC pause  F3 hud  F6 profiler  V view  Z/X turn piece");

        if (shot_due > 0.0 && !shot_taken && plat_time() >= shot_due) {
            shot_taken = true;
            shot_write("play.png");
            std::printf("screenshot: bench/play.png\n");
        }
    }
    if (seconds <= 0.0) save_options(show_debug);

    const double played = plat_time() - started;
    std::printf("left after %.1fs, %ld frames, %d chunks, %ld corrections applied, "
                "%.2f MB in / %.2f MB out\n",
                played, frames, conn->chunks_received(), applied,
                (double)conn->bytes_received() / (1024.0 * 1024.0),
                (double)conn->bytes_sent() / (1024.0 * 1024.0));

    // Bodies other than this player's. Printed rather than left on the status
    // line, because the status line is only readable by somebody watching: a
    // server started with --mobs is otherwise a server whose entities nothing
    // outside the loopback test can see.
    if (!conn->players().empty())
        std::printf("        %zu other bod%s in view at the end\n", conn->players().size(),
                    conn->players().size() == 1 ? "y" : "ies");

    if (seconds <= 0.0) {
        conn->close();
        return 0;
    }

    // The gate. Everything here is something a person watching the window would
    // notice immediately and no headless test can see at all.
    int failures = 0;
    auto check = [&](bool ok, const char *what) {
        std::printf(ok ? "  ok    %s\n" : "  FAIL  %s\n", what);
        if (!ok) ++failures;
    };

    check(!dropped, "stayed connected for the whole run");
    check(conn->chunks_received() > 0, "the server's world arrived");
    check(s.act().world.chunk_count() > 0, "and went into the world this client renders");
    check(conn->chunk_error().empty(), "every chunk decoded");

    // Standing still is not a violation of anything. A correction here means the
    // server refused a player who did nothing at all, which is the failure that
    // makes a game unplayable for everybody who is not cheating.
    if (!stood_taken && !stood_sampled) stood_at = player.pos;
    const long applied_standing = stood_taken    ? applied_at_underfoot
                                  : stood_sampled ? applied_when_stood
                                                  : applied;
    check(applied_standing == 0, "standing still was never corrected");

    // The one that says the streamed world is a world and not a pile of bytes:
    // the player is standing on something, is not buried in it, and has not
    // fallen through a world that never arrived.
    //
    // Not player.on_ground, which was the first version of this and passed for
    // a reason that does not hold. With nothing to draw this client runs at six
    // thousand frames a second, so dt is 160 microseconds and one frame of
    // gravity moves the player about a millionth of a block -- less than a
    // float can represent at y = 20. The downward step rounds to nothing, no
    // collision is ever resolved, and on_ground keeps whatever value it had.
    // The flag was reporting the frame rate, not the ground.
    //
    // These three are timing-independent: they ask where the player is and what
    // is there, and inside_solid is the server's own predicate -- the same call
    // that decides whether a claim gets refused.
    const int  fx = (int)std::floor(stood_at.x), fz = (int)std::floor(stood_at.z);
    const int  fy = (int)std::floor(stood_at.y) - 1;
    const bool solid_below = s.act().registry.is_solid(s.act().world.get_block(fx, fy, fz));
    const bool buried = inside_solid(s.act().world, s.act().registry, stood_at.x, stood_at.y,
                                     stood_at.z);
    std::printf("        stood at %.2f %.2f %.2f, block below is '%s'%s%s\n", stood_at.x,
                stood_at.y, stood_at.z,
                s.act().registry.get(s.act().world.get_block(fx, fy, fz)).name.c_str(),
                solid_below ? "" : " (NOT SOLID)", buried ? " (BURIED)" : "");
    check(solid_below, "the block under the player came from the server and is solid");
    check(!buried, "the player is not inside it");
    check(stood_at.y > -8.0f, "the player did not fall through a world that never arrived");

    // The other worlds, if this client was told to look into any.
    //
    // Nothing is drawn from them yet, so what is checked is what exists: a
    // connection of its own, a world of its own, and chunks arriving into it.
    // The renderer is the next piece and this is what it will draw.
    if (net_sessions.count() > 1) {
        int connected = 0, streaming = 0, own_world = 0;
        int own_table = 0, different_game = 0;
        int watched_worlds = net_sessions.count() - 1;
        for (int id = 0; id < net_sessions.count(); ++id) {
            // The connection the player is on NOW, which after a crossing is
            // not the one they joined on. Skipping the original id instead is
            // how this check came to count the player's own world as a watched
            // one and fail.
            if (id == self_conn) continue;
            const net::ClientSession *w = net_sessions.get(id);
            if (!w) continue;

            // A session this client shut on purpose is not a world being
            // looked into. The redirect fallback closes the world it left --
            // there was no window onto it to keep -- and counting that as a
            // watched world that went down measures the wrong thing.
            if (w->given_up() && !w->playing()) {
                --watched_worlds;
                continue;
            }

            if (w->playing()) ++connected;
            if (w->chunks_received() > 0) ++streaming;
            if (&w->world() != &conn->world()) ++own_world;

            // Ids are read through the table of the session they arrived on and
            // through no other. Checked as a loop over the whole table rather
            // than as a spot check: a shared table would agree on the core
            // blocks and disagree on exactly the ones a mod added.
            bool table_is_its_own = w->registry().size() == w->welcome().nodes.size();
            for (size_t i = 0; table_is_its_own && i < w->welcome().nodes.size(); ++i)
                if (w->registry().get((BlockId)i).name != w->welcome().nodes[i])
                    table_is_its_own = false;
            if (table_is_its_own) ++own_table;

            if (w->welcome().content_hash != conn->welcome().content_hash) ++different_game;

            std::printf("        world %d: %s, %d chunks, %zu blocks in its table\n", id,
                        net::client_state_text(w->state()), w->chunks_received(),
                        w->welcome().nodes.size());
        }

        const int watched = watched_worlds;
        check(connected == watched, "every world being looked into is still connected");
        check(streaming == watched, "and streaming into this process");
        check(own_world == watched, "each into a world and a block table of its own");
        // The check that used to be missing. The one above compares POINTERS:
        // two worlds with byte-identical tables pass it just as happily, which
        // is not the property -- the property is that every id in a world's
        // table is the name THAT server sent for it.
        check(own_table == watched,
              "and every id in it is the name that server sent for it, all the way down");

        // "Due server con game diversi" is one clause of DESIGN.md's exit test
        // for M6, and whether a given run is one is decided by how the servers
        // were started. Reported always, so a run meant to be that and not
        // being it is one line rather than a silence; asserted when the run
        // says it is.
        std::printf("        %d of %d world(s) are a different game from this one (block table "
                    "%016llx here)\n", different_game, watched,
                    (unsigned long long)conn->welcome().content_hash);
        if (g_different_games) {
            check(different_game == watched,
                  "every world looked into is a different game, as this run said it would be");

            // And the half that only means anything when they differ: one id,
            // two blocks, at the same moment, in one process.
            const net::ClientSession *other = nullptr;
            for (int id = 0; id < net_sessions.count(); ++id) {
                if (id == self_conn) continue;
                const net::ClientSession *w = net_sessions.get(id);
                if (w && w->playing()) { other = w; break; }
            }
            const size_t smallest =
                other ? (other->registry().size() < conn->registry().size()
                             ? other->registry().size() : conn->registry().size())
                      : 0;
            int disagreements = 0;
            for (size_t i = 0; i < smallest; ++i)
                if (conn->registry().get((BlockId)i).name != other->registry().get((BlockId)i).name)
                    ++disagreements;

            // Blocks one world has and the other has never heard of. Two games
            // can differ in two shapes: the same id naming different blocks
            // (which needs their tables to diverge INSIDE the shared range) and
            // one table simply having blocks the other lacks. Real mod sets
            // usually produce the second -- the core nodes register first and
            // in the same order on both sides -- so this is what a run against
            // two real servers can assert. The harder shape is built on purpose
            // and measured in net_loopback: id 1 as core:stone on one side and
            // test:only-here on the other.
            // Counted in BOTH directions, which the first version was not: a
            // crossing swaps which connection is "here", so a run that walked
            // from the world with the mods into the world without them found
            // every block it had in the other table and reported no difference
            // at all.
            auto missing_from = [](const net::ClientSession &from,
                                   const net::ClientSession &in) {
                int n = 0;
                for (size_t i = 0; i < from.registry().size(); ++i) {
                    const std::string &name = from.registry().get((BlockId)i).name;
                    if (name != "core:air" && in.registry().find(name) == kAir) ++n;
                }
                return n;
            };
            const int only_here  = other ? missing_from(*conn, *other) : 0;
            const int only_there = other ? missing_from(*other, *conn) : 0;

            std::printf("        of the %zu ids both tables have, %d name different blocks; "
                        "%d block(s) here are unknown there and %d there are unknown here\n",
                        smallest, disagreements, only_here, only_there);
            check(other != nullptr && (disagreements > 0 || only_here > 0 || only_there > 0),
                  "and the two tables are two tables, not one shared between the worlds");
        }
        check(s.count() == net_sessions.count(),
              "and the scene holds one world per connection, however many that is");
    }

    // The crossing between two servers, when the gate walked into one.
    //
    // What is checked is what could not be checked before there were two of
    // everything: that the player is on the OTHER connection, that both are
    // still up -- neither reconnected -- and that the world they arrived in was
    // not sent to them again. That last one is DESIGN.md's test for M6 in a
    // number: a crossing without a loading screen is a crossing where nothing
    // is downloaded.
    // The other end of the same question: a crossing that was refused. Only
    // measured when one happened, because the ordinary run crosses -- the two
    // are different server configurations, not two paths through one run.
    // On EITHER outcome, and that is the point: conditioning it on a refusal
    // having arrived would make the case where none arrives -- the two-second
    // timeout, which is the thing this replaced -- skip the check instead of
    // failing it. Measured: with the server's reply removed the run printed
    // nothing at all here until this condition was widened.
    // Walking into a mouth, and the three things that can come of it.
    //
    // The third one is new: a crossing that was never asked for, because the
    // world on the other side had already said there was no room. The heartbeat
    // has carried that answer since it existed and nothing read it -- so the
    // first news of a full world was a refused crossing, a round trip after the
    // player had walked in.
    // What the link did to the liveness signal. The silence window is a
    // constant chosen on loopback; this is the number it should have been
    // chosen against.
    {
        // Over every session, and only once each: `conn` is one of them, and
        // seeding the total with it counted the session the player is on twice.
        // Harmless while the count was always zero, which is what a reliable
        // heartbeat made it -- and wrong the moment the number could move.
        double worst = 0.0;
        long   lost  = 0;
        for (int id = 0; id < net_sessions.count(); ++id) {
            const net::ClientSession *w = net_sessions.get(id);
            if (!w || !w->has_heartbeat()) continue;
            if (w->heartbeat_gap_max() > worst) worst = w->heartbeat_gap_max();
            lost += w->heartbeats_lost();
        }
        std::printf("        heartbeats: longest gap %.2f s across %d session(s), %ld lost; "
                    "round trip %.0f ms, so the silence window is %.1f s\n",
                    worst, net_sessions.count(), lost, conn->round_trip() * 1000.0,
                    conn->heartbeat_window());
    }

    // Servers this client was handed a second address for, and recognised.
    //
    // Zero on loopback, where every route to a server spells the same, and the
    // reason this is printed even then: a run that says nothing here has not
    // shown that the recognition works, it has shown that nothing needed it.
    {
        int named = 0;
        for (int id = 0; id < net_sessions.count(); ++id) {
            const net::ClientSession *w = net_sessions.get(id);
            if (w && w->playing() && w->server_is_named()) ++named;
        }
        std::printf("        %d of %d session(s) told this client what server they are, "
                    "not just where; %d window(s) had to be repointed at a session already "
                    "open under another address\n",
                    named, net_sessions.count(), windows_reused);
    }

    if (mouth_triggers > 0) {
        const net::ClientSession *far = net_sessions.get(mouths[0].conn);
        std::printf("        walked into a mouth %d time(s): %d crossing(s) asked for, "
                    "%d held back before anything went out, %d waited for the world of "
                    "arrival\n", mouth_triggers, crossings_attempted, crossings_held_back,
                    crossings_waited_for_ground);
        if (far != nullptr && far->has_heartbeat()) {
            const net::Heartbeat &b = far->heartbeat();
            std::printf("        the world through it last said %u/%u players, %s\n",
                        b.players, b.max_players,
                        b.enterable() ? "room to walk in" : "no room to walk in");
        }
    }

    if (g_far_world_full) {
        const net::ClientSession *far = net_sessions.get(mouths[0].conn);
        check(far != nullptr && far->has_heartbeat() && !far->heartbeat().enterable(),
              "the world through the opening says it has no room, which is what this run set up");

        // The point, as a number: nothing went out. Not "the refusal was
        // handled well" -- there was no refusal, because there was no request.
        check(mouth_triggers > 0, "and the player walked into the mouth anyway");
        check(crossings_held_back == mouth_triggers && crossings_attempted == 0,
              "and every one of those was held back before a byte went out");
        check(crossings_denied == 0 && crossed_once == 0,
              "so there was nothing to refuse, and nobody crossed");
    }

    // What an answer to a crossing actually cost, whichever way it went. Printed
    // for every run that asked for one, because it is the number the wait above
    // has to be chosen against and for a long time nothing measured it.
    if (crossings_attempted > 0) {
        char yes[32], no_[32];
        if (grant_delay  >= 0.0) std::snprintf(yes, sizeof(yes), "%.0f ms", grant_delay * 1e3);
        else                     std::snprintf(yes, sizeof(yes), "not asked");
        if (denial_delay >= 0.0) std::snprintf(no_, sizeof(no_), "%.0f ms", denial_delay * 1e3);
        else                     std::snprintf(no_, sizeof(no_), "not asked");
        std::printf("        the crossing was answered yes in %s, no in %s\n", yes, no_);
    }

    if (crossings_denied > 0 || crossings_lost > 0) {
        char answered_in[32];
        if (denial_delay >= 0.0) std::snprintf(answered_in, sizeof(answered_in), "%.1f ms",
                                               denial_delay * 1e3);
        else                     std::snprintf(answered_in, sizeof(answered_in), "never");
        std::printf("        %d crossing(s) refused, answered in %s; %d lost with the world "
                    "they asked\n", crossings_denied, answered_in, crossings_lost);
        check(crossings_lost == 0, "a refused crossing is answered, not lost");

        // The number this replaces, and why it is no longer a constant. A
        // refusal is decided the moment the request lands, so what it costs is
        // one round trip and a tick -- not a fixed half second, which on a
        // 400 ms link is less than the trip itself and would fail a server that
        // answered as fast as the link allows. Anything near the wait above is
        // the timeout doing the work, whatever the message says.
        const net::ClientSession *asked = net_sessions.get(mouths[crossing_mouth].conn);
        const double rtt   = asked ? asked->round_trip() : 0.0;
        const double prompt = rtt * 3.0 > 0.5 ? rtt * 3.0 : 0.5;
        std::printf("        a refusal that is decided rather than waited out costs about a "
                    "round trip; at %.0f ms that is under %.1f s\n", rtt * 1e3, prompt);
        check(denial_delay >= 0.0 && denial_delay < prompt,
              "and the answer arrives in a round trip, not at the end of the wait");
        check(crossed_once == 0 && s.active == self,
              "and the player is still in the world they were standing in");
    }

    // The fallback, when the crossing went that way. Different checks, because
    // it is a different journey with the same destination: one connection
    // instead of two, a world streamed instead of a world already there, and no
    // window onto the place left behind.
    if (g_no_passive && saw_a_mouth) {
        char crossed_in[32];
        if (fallback_seconds >= 0.0)
            std::snprintf(crossed_in, sizeof(crossed_in), "%.0f ms", fallback_seconds * 1e3);
        else
            std::snprintf(crossed_in, sizeof(crossed_in), "never");
        std::printf("        the redirect fallback: %d chunk(s) in hand when the player arrived, "
                    "%d by the end; %s from the opening to standing there; %d redirect(s) taken, "
                    "%d ignored\n", fallback_chunks, conn->chunks_received(), crossed_in,
                    redirects_taken, redirects_ignored);

        // Printed whether it went well or not: a red here is otherwise a
        // mystery, and the first thing worth knowing is whether the player ever
        // reached the opening. One run in several ended with no redirect at all
        // and nothing in the output said why.
        std::printf("        the player ended at %.2f %.2f %.2f, and the mouth this run walked "
                    "at was %.2f %.2f %.2f\n", (double)player.pos.x, (double)player.pos.y,
                    (double)player.pos.z, (double)s.act().portal_a.center.x,
                    (double)s.act().portal_a.center.y, (double)s.act().portal_a.center.z);

        // One redirect a crossing: --cross-back walks back the same way.
        check(redirects_taken >= 1 && redirects_taken == crossed_once,
              "the server's redirect is what carried the player across");
        check(fallback_seconds >= 0.0,
              "and the player is standing in the world on the other side");
        check(conn->playing() && conn->scope() == net::Scope::Enter,
              "and the connection it opened is the one being played on");
        // The number that makes it a FALLBACK. The seamless path arrives with
        // the world already streamed; this one pays for it, and saying how much
        // is the difference between a trade-off and a claim.
        // The number that makes it a FALLBACK. The seamless path arrives with
        // the world already in hand; this one arrives with nothing and has the
        // whole world sent to it afterwards. Saying how much is the difference
        // between a trade-off and a claim.
        check(fallback_chunks == 0,
              "the player arrived in a world this client had not been sent yet");
        check(conn->chunks_received() > 0,
              "and it was streamed afterwards -- the loading screen the name is for");
        check(net_sessions.get(mouths[0].conn) == nullptr,
              "with nothing left watching the world behind, because there was no window to keep");
    }

    // There and back (--cross-back). The checks below are about a player who
    // is still on the far side, and every one of them would read the way home
    // as a failure; these are the ones that hold after it.
    // Only --cross-back walks back the way it came: a kart lap crosses on
    // into a third world, and the way home is not where it ends.
    if (crossed_once >= 2 && fallback_seconds < 0.0 && g_cross_back < 0.0)
        std::printf("        crossed %d times without --cross-back: the way home was not "
                    "measured here\n", crossed_once);
    if (crossed_once >= 2 && fallback_seconds < 0.0 && g_cross_back >= 0.0) {
        const net::ClientSession *behind = net_sessions.get(left_conn);
        std::printf("        crossed there and back: last came out at %.2f %.2f %.2f, playing on "
                    "%s and watching %s\n",
                    (double)arrived_at.x, (double)arrived_at.y, (double)arrived_at.z,
                    conn_spec[(size_t)self_conn].c_str(),
                    left_conn >= 0 ? conn_spec[(size_t)left_conn].c_str() : "nothing");
        check(self_conn == self, "the player is back on the connection the run joined on");
        std::printf("        the doorway home was showing %.2f %.2f %.2f\n",
                    (double)shown_before_cross.x, (double)shown_before_cross.y,
                    (double)shown_before_cross.z);
        check(shown_taken && length(arrived_at - shown_before_cross) < 4.0f,
              "and came home out of the opening the doorway was showing");
        check(conn->playing() && conn->scope() == net::Scope::Enter && behind &&
                  behind->playing() && behind->scope() == net::Scope::Observe,
              "playing here, watching there, and neither connection reconnected");
    }

    if (crossed_once == 1 && fallback_seconds < 0.0) {
        chunks_after_arriving = conn->chunks_received();

        std::printf("        crossed once: arrived at %.2f %.2f %.2f, playing on %s and watching "
                    "%s; that world had %d chunks before the crossing and %d after, %d of "
                    "which it already had\n",
                    (double)arrived_at.x, (double)arrived_at.y, (double)arrived_at.z,
                    conn_spec[(size_t)self_conn].c_str(),
                    left_conn >= 0 ? conn_spec[(size_t)left_conn].c_str() : "nothing",
                    chunks_before_arriving, chunks_after_arriving, conn->chunks_repeated());

        const net::ClientSession *behind = net_sessions.get(left_conn);
        check(self_conn != self, "the player is on the connection that was behind the opening");
        check(conn->playing() && behind && behind->playing(),
              "and both connections are still up -- neither of them reconnected");
        check(behind && behind->scope() == net::Scope::Observe &&
                  conn->scope() == net::Scope::Enter,
              "the world left behind was told it is being watched, not played in");
        check(s.active != s.act().link_session,
              "the world being stood in is not the world being looked at");
        // Not "few chunks arrived after", which is what this used to say. On a
        // link that drops packets the first stream can still be running when
        // the player walks in, and nine more chunks of a world that was at 23
        // of 32 is that stream finishing, not a second one starting. What a
        // second stream looks like is a chunk this world already held arriving
        // again, and that is a thing the session can count exactly.
        check(chunks_before_arriving > 0 && conn->chunks_repeated() == 0,
              "and the world arrived in was not streamed a second time");

        // The block the player came out in, specifically. "Some of the world
        // had arrived" is not the claim a seamless crossing makes -- one run in
        // eight at 20% loss crossed with the far world at zero chunks and the
        // world turned up afterwards, which is a loading screen with the lights
        // left on. Now the client waits at the mouth instead, and this is what
        // says the wait was long enough.
        check(ground_was_ready,
              "and the block it came out in was already here before it crossed");
        if (crossings_waited_for_ground > 0)
            std::printf("        waited %d frame(s) at the opening for that block, from %.1f s "
                        "into the run\n", crossings_waited_for_ground, waited_at - started);
        const int     out_m = crossed_out_of < (uint32_t)net::kMaxMouths ? (int)crossed_out_of : 0;
        const Portal &came_out_of = mouth_portal(s.act(), out_m);
        const Portal &other_one =
            crossed_out_of == 0 ? s.act().portal_b : s.act().portal_a;
        std::printf("        the link named opening %u, and that world's openings are at "
                    "%.2f %.2f %.2f and %.2f %.2f %.2f\n", crossed_out_of,
                    (double)s.act().portal_a.center.x, (double)s.act().portal_a.center.y,
                    (double)s.act().portal_a.center.z, (double)s.act().portal_b.center.x,
                    (double)s.act().portal_b.center.y, (double)s.act().portal_b.center.z);
        check(length(arrived_at - came_out_of.center) < 4.0f,
              "the player came out at the opening the link named, in the world they arrived in");
        // The one that catches a portal that lies. The image in the doorway and
        // the arrival are computed by two different pieces of code from two
        // different messages, and for one commit they disagreed by 26 nodes
        // with every check still green.
        std::printf("        the doorway was showing %.2f %.2f %.2f, and the player came out at "
                    "%.2f %.2f %.2f\n", (double)shown_before_cross.x,
                    (double)shown_before_cross.y, (double)shown_before_cross.z,
                    (double)arrived_at.x, (double)arrived_at.y, (double)arrived_at.z);
        check(shown_taken && length(arrived_at - shown_before_cross) < 4.0f,
              "and out of the opening the doorway was SHOWING, not a different one");
        // Nearer the one named than the other. "More than 8 from the other"
        // failed an arrival 0.6 from the right opening once the track's bay put
        // the two 6.1 apart: far enough to tell, not far enough for that.
        check(length(came_out_of.center - other_one.center) < 4.0f ||
                  length(arrived_at - came_out_of.center) < length(arrived_at - other_one.center),
              "and not at the other one, when the two are far enough apart to tell");

        // The two paths, side by side. The server decides the crossing either
        // way and says so; a client with a window already open onto that world
        // does not need the address it was handed.
        std::printf("        the server sent %d redirect(s) as well; %d were taken\n",
                    redirects_ignored + redirects_taken, redirects_taken);
        check(redirects_taken == 0,
              "and the seamless path was the one taken, not the reconnect");
    }

    // The mouth, when this world's pair leads to another server. Skipped for a
    // client that opens no passive sessions: there is no window to check, which
    // is the configuration and not a failure.
    //
    // Three things, and they are three because each one failed on its own at
    // some point: the connection exists, the link's far end is where the OTHER
    // server says its opening is, and the world drawn inside the opening is the
    // one that connection streams into.
    for (int m = 0; m < net::kMaxMouths && !g_no_passive; ++m) {
        const net::PortalAway &away = conn->portals().away[m];
        if (!away.leads_away()) continue;

        const net::ClientSession *far         = net_sessions.get(mouths[m].conn);
        const int                 world       = world_of(mouths[m].conn);
        const int                 far_opening = (int)away.portal;
        const Vec3 shown  = mouth_link(s.act(), m).dst.center;
        const Vec3 theirs = far && far->portals().has(far_opening)
                                ? Vec3{far->portals().opening[far_opening].cx,
                                       far->portals().opening[far_opening].cy,
                                       far->portals().opening[far_opening].cz}
                                : Vec3{};

        std::printf("        opening %d leads to %s opening %d; its far end is drawn at "
                    "%.2f %.2f %.2f and that server says %.2f %.2f %.2f\n", m + 1,
                    link_dest(*conn, away.host, away.port, away.room).spec.c_str(), far_opening,
                    (double)shown.x,
                    (double)shown.y, (double)shown.z, (double)theirs.x, (double)theirs.y,
                    (double)theirs.z);

        check(far != nullptr && far->playing(),
              "a pair that leads away opens a session to the world it leads to");
        check(far != nullptr && far->portals().has(far_opening) && length(shown - theirs) < 0.01f,
              "and the far end of the link is where THAT server says the opening it names is");
        check(world >= 0 && mouth_world(s.act(), m) == world,
              "and the world drawn inside the opening is the one that session streams into");
        check(s.act().portals && s.act().link_count == std::min(conn->portals().mouths(), 2),
              "one link per mouth, drawn, because the other ends are not in this world");

        // And that there is something in it. A link pointing at an empty world
        // draws a hole full of sky, which is exactly what a broken one looks
        // like: the far world has to have been MESHED, and the portal pass has
        // to have run for it.
        std::printf("        world %d meshed %zu vertices; the portal pass %s an oblique near "
                    "plane on the last frame\n", world,
                    world >= 0 ? s.at(world).world_render.vertex_count() : (size_t)0,
                    (m == 0 ? s.last_oblique : s.last_oblique_b) ? "applied" : "did not apply");
        check(world >= 0 && s.at(world).world_render.vertex_count() > 0,
              "and that world was meshed, so the opening is not a hole full of sky");
    }

    // The openings, placed from here and answered from there.
    //
    // The loopback probe measures the server's half of this -- what it refuses,
    // where it puts the hole, what it sends back. What that probe cannot see is
    // this client: the click going out, and the answer landing in the three
    // places an opening lives in a renderer. A pair that is drawn in one place
    // and walked into in another is the drift that takes longest to diagnose,
    // so all three are read back here rather than one of them.
    if (placement_sent) {
        // The clicked node itself for a floor opening: something has to hold it
        // up, and that something is the node whose top face was clicked.
        const Vec3 want{(float)placed_x + 0.5f, (float)placed_y + 0.5f, (float)placed_z + 0.5f};
        const Vec3 got = s.act().portal_a.center;
        std::printf("        placed an opening on top of %d %d %d: the server put it at "
                    "%.2f %.2f %.2f, version %u -> %u\n", placed_x, placed_y, placed_z,
                    (double)got.x, (double)got.y, (double)got.z, version_before_placement,
                    conn->portals_version());

        check(conn->portals_version() > version_before_placement,
              "a placement sent from this client comes back as a new version");
        check(length(got - want) < 0.01f,
              "and a floor opening is the block that was clicked");
        // A local pair has the opening twice in the renderer, as one link's
        // source and the other's destination. A mouth has it once: the second
        // link would be a hole leading back out of a world that is not here.
        if (conn->portals().leads_away())
            check(length(s.act().link_ab.src.center - got) < 1e-4f,
                  "and the mouth is drawn where the server just moved it");
        else
            check(length(s.act().link_ab.src.center - got) < 1e-4f &&
                      length(s.act().link_ba.dst.center - got) < 1e-4f,
                  "and the drawn opening and the walked one are one opening");
        check(s.act().portals, "and this client is drawing the pair");

        // The floor portal placed under the player's own feet.
        //
        // Before the node holding a floor portal up was exempt from the "inside
        // a solid block" rule, this produced 97 corrections in five seconds and
        // no crossing at all: the client sinks on purpose, the server refuses on
        // purpose, and the player is pinned. Both halves are read here -- that
        // the sink is accepted, and that it ends in a crossing rather than in
        // standing a tenth of a node inside the ground.
        if (underfoot_sent) {
            const long crossings = conn->teleports() - teleports_before_underfoot;
            std::printf("        placed one under the player's feet: %ld crossings, %ld "
                        "corrections while sinking into it, and %ld more afterwards; ended at "
                        "%.2f\n", crossings,
                        applied_at_crossing < 0 ? -1 : applied_at_crossing - applied_at_underfoot,
                        applied_at_crossing < 0 ? 0 : applied - applied_at_crossing,
                        (double)player.pos.y);

            check(crossings > 0, "the player falls through the portal they put at their feet");
            check(applied_at_crossing >= 0 && applied_at_crossing == applied_at_underfoot,
                  "and sinking into the node that holds it up is not a cheat");

            // Said rather than checked, because it is a shape a player can
            // build and this gate is not the place to pretend otherwise: with
            // BOTH openings lying flat and facing up, the exit is underneath
            // the second one -- the map puts the arrival behind the
            // destination plane, and behind a floor is inside the ground. The
            // player then sinks out of the exempt node and is corrected. A
            // floor paired with a wall, which is the shape this is for, comes
            // out standing. See docs/archive/M6_FINDINGS.md 4.3.
            if (applied_at_crossing >= 0 && applied > applied_at_crossing)
                std::printf("        (and then fell out of the bottom of the second flat "
                            "opening, which is what a flat-to-flat pair does)\n");
        } else if (conn->portals().leads_away()) {
            // Not a failure and not skipped quietly: with the pair leading to
            // another server there is no local crossing to measure, because
            // this server refuses to decide one -- the destination is a world
            // it has no blocks for.
            std::printf("        no crossing to measure: this world's pair leads to %s:%u\n",
                        conn->portals().away[0].host.c_str(),
                        (unsigned)conn->portals().away[0].port);
        } else {
            std::printf("        the second placement never went out\n");
        }
    } else {
        std::printf("        no placement was sent: the server has no openings\n");
    }

    // Adoption, checked directly rather than inferred. Every block the terrain
    // is made of is a core block with the same id on both sides, so a client
    // that skipped adoption entirely would walk around on grass and pass every
    // check above. What it would not have is the rest of the server's table.
    const size_t table = conn->welcome().nodes.size();
    std::printf("        block table: %zu names from the server, %zu in this registry, "
                "%zu unknown to this build\n",
                table, s.act().registry.size(), conn->unknown_blocks());

    // ------------------------------------------------------------- media
    //
    // DESIGN.md 4.5, as numbers. Each world says what its blocks look like; the
    // client answers with what it does not already hold; the files that are
    // left travel once and land in a cache every session shares.
    //
    // The claim worth checking is the one about the SECOND world, and it is
    // stated as a total rather than as "world 1 downloaded nothing": a run
    // against a warm cache downloads nothing anywhere, and a gate reading world
    // 1 alone would call that a pass while proving nothing. What holds either
    // way is that the union of what was offered bounds what was fetched.
    {
        std::vector<ContentHash> distinct;
        int    offered = 0, had = 0, arrived = 0, refused = 0;
        size_t wire_bytes = 0;

        for (int id = 0; id < net_sessions.count(); ++id) {
            const net::ClientSession *w = net_sessions.get(id);
            if (!w) continue;

            offered += w->media_offered();
            had     += w->media_had();
            arrived += w->media_received();
            refused += w->media_rejected();
            wire_bytes += w->media_bytes();

            for (const net::MediaEntry &e : w->media_offer()) {
                bool seen = false;
                for (const ContentHash &h : distinct)
                    if (h == e.hash) { seen = true; break; }
                if (!seen) distinct.push_back(e.hash);
            }

            std::printf("        media: world %d offered %d file(s), %d already here, "
                        "%d arrived (%.1f KB)\n", id, w->media_offered(), w->media_had(),
                        w->media_received(), (double)w->media_bytes() / 1024.0);
        }

        std::printf("        media: %d offer(s) across %d world(s) name %zu distinct file(s); "
                    "%d were fetched, %d were already on disk, %d refused\n",
                    offered, net_sessions.count(), distinct.size(), arrived, had, refused);
        // Where a duplicate came from, when there is one. A claim taken over is
        // one session deciding another had died; anything else is a fetch that
        // was never claimed at all.
        std::printf("        %zu claim(s) still held, %ld taken over from a session that "
                    "looked dead\n", g_media_cache.claims_held(),
                    g_media_cache.claims_taken_over());

        check(offered > 0, "the server said what its blocks look like");

        // The deduplication, which is the whole point of addressing media by
        // content: N worlds offering the same texture cost one copy of it.
        check(arrived <= (int)distinct.size(),
              "and nothing was downloaded twice, however many worlds offered it");

        // A file whose bytes did not match the hash they arrived under, or that
        // this world never offered. Zero is the only acceptable number and it
        // is checked rather than assumed: this cache is shared with every other
        // world the player has ever looked into.
        check(refused == 0, "and every file that arrived was one this world had offered");

        // The atlas was rebuilt from what this world offered.
        //
        // The "unless they were all already here" escape this used to carry was
        // green while the atlas held no media at all: everything WAS already on
        // disk, and the rebuild had fired before the offer arrived and never
        // again. On loopback the offer always beat the renderer and the escape
        // never had to hold anything up; over 120 ms it did, and it lied.
        check(media_atlas_rebuilds > 0,
              "and the atlas was rebuilt from the media this world offered");

        // And what the whole pipeline is for, on the world through the opening:
        // the blocks this build has never heard of.
        //
        // Their names arrive in the block table and their colours do not, so
        // until media travelled there was nothing a client could draw for them
        // but the magenta placeholder adopt_node_table() invents. Now the
        // server that owns them bakes their pictures and sends them, and a
        // cross-game portal shows the other game.
        for (const Mouth &mo : mouths) {
            const int                 world = world_of(mo.conn);
            const net::ClientSession *far   = net_sessions.get(mo.conn);
            if (world >= 0 && far != nullptr && far->playing()) {
                const int drawn = s.at(world).world_render.textures_supplied();
                std::printf("        the world through %s: %zu block(s) this build has "
                            "never heard of; its atlas is %d from its own media, %d generated\n",
                            mo.spec.c_str(), far->unknown_blocks(), drawn,
                            s.at(world).world_render.textures_generated());
                // Tiles, not blocks: the stairs, slabs and colours that wear one
                // picture share its tile, so a world with more blocks than
                // pictures is normal. Magenta is a tile with no file behind it.
                check(drawn > 0 && s.at(world).world_render.textures_generated() == 0,
                      "and the blocks it has that this build does not are drawn from ITS "
                      "pictures, not from magenta");
            }
        }
    }

    // Where the pixels came from. Before there were any files this was not a
    // question anybody could ask: the renderer generated every tile, so "the
    // block is drawn" and "the block is drawn from the texture somebody made
    // for it" were the same sentence.
    std::printf("        atlas: %d block(s) from media this world sent, %d from a generated "
                "placeholder\n", s.act().world_render.textures_supplied(),
                s.act().world_render.textures_generated());
    check(s.act().world_render.textures_supplied() > 0,
          "the world is drawn from what the server sent, not from a fallback that hides "
          "its absence");
    check(table > 0 && s.act().registry.size() == table,
          "this client's block table is the same size as the server's");

    // And the ids are the positions, which is the whole mechanism: the client
    // rebuilds the table in arrival order, so name number seven must be block
    // number seven here too. Checking the sizes alone would pass for a registry
    // holding the right names in the wrong order.
    size_t misplaced = 0;
    for (size_t i = 0; i < table; ++i)
        if (s.act().registry.find(conn->welcome().nodes[i]) != (BlockId)i) ++misplaced;
    check(misplaced == 0, "and every name sits at the id the server gave it");

    // Breaking and building, over the wire. The loop has to CLOSE: the block
    // that came out of the world went into the bag, and the one that came out
    // of the bag went back into the world. Either half on its own passes for a
    // server that hands out blocks nobody paid for.
    std::printf("        edits: %ld sent, %ld cell(s) changed, %ld refused; the item table has "
                "%zu entr%s\n", edits_sent, blocks_changed, edits_refused,
                conn->item_table().size(), conn->item_table().size() == 1 ? "y" : "ies");
    std::printf("        %ld hit(s) sent at bodies\n", hits_sent);

    // The long run's own verdict. DESIGN.md 8: thirty minutes from the start,
    // no crash, no touching the console.
    //
    // Getting to the end without crashing is most of it and it is not all of
    // it: a client that stops doing anything after five minutes also reaches
    // the end. So the two things checked are that the loop went on CLOSING --
    // blocks out of the world and back into it, over and over -- and that
    // memory did not climb without bound while it did.
    if (g_roam) {
        const size_t rss_end = resident_bytes();
        const double grew =
            ((double)rss_end - (double)roam.rss_first) / (1024.0 * 1024.0);

        std::printf("        roam: %ld broke, %ld built, %ld swung, %ld turned away from "
                    "something\n", roam.broke, roam.built, roam.swung, roam.turns);
        // From the roam's FIRST frame and not from the start of the process:
        // the first seconds are the world arriving and meshing, which is growth
        // that has to happen and settles. What this window asks is whether it
        // goes on growing after that, which is the question a long run is for.
        std::printf("        RSS from the first step %.1f -> %.1f MB (%+.1f), worst seen "
                    "%.1f MB\n",
                    (double)roam.rss_first / (1024.0 * 1024.0),
                    (double)rss_end / (1024.0 * 1024.0), grew,
                    (double)roam.rss_worst / (1024.0 * 1024.0));

        check(roam.broke > 0 && roam.built > 0,
              "the loop kept closing: blocks came out of the world and went back into it");

        // The same rule and the same number the M1 soak uses, rather than a
        // second one chosen beside it. Memory is allowed to settle upward as
        // chunks mesh for the first time; what it must not do is climb without
        // bound.
        check(grew < 64.0, "and memory did not climb without bound over the whole run");

        // A run too short to mean anything must say so rather than pass. Five
        // minutes is where the report lines start being a shape instead of two
        // points; below that this is a smoke test wearing the exit test's name.
        if (played < 300.0)
            std::printf("        NOTE: %.0f s is not the thirty minutes DESIGN.md 8 asks "
                        "for\n", played);
    }

    // Said out loud when it did not run, because the alternative is a block of
    // checks that pass by not happening -- which this suite has been bitten by
    // often enough to have a rule about it. net_client.sh asserts the line
    // below is the one that ran, so the skip cannot quietly spread.
    // On the sequence and not on edits_sent: --use-at's click is an edit too,
    // and a kart run that got in would read as hands that never broke a block.
    if (!hands_ran) {
        std::printf("        the hands were not measured here: this world's openings lead "
                    "somewhere else, and that run's scenario is the crossing\n");
    } else {
    check(edits_sent > 0, "the client asked the server to change a block");
    check(broke_ok, "and the block it broke is gone from the world it is drawing");
    std::printf("        the bag held %u item(s) after the break\n", carried_after_break);
    check(carried_after_break == 1,
          "and turned up in the bag, which is where a broken block goes");
    check(placed_back, "and the block it put back is in the world again, and is the same "
                       "block that came out of it");

    // Nothing is gifted to a player here, so this is the whole economy: one
    // block was broken and one was placed, and the second could only be the
    // first.
    {
        check(carried_when_done == 0,
              "and the bag paid for it: nothing left, having started empty");
    }

    check(empty_hand_used,
          "a right click with an empty hand is not refused and builds nothing: it is a use, "
          "which is the gesture a container is opened with");
    check(reach_refused, "and so is reaching forty nodes for a block, in a sentence with the "
                         "distance in it");
    check(conn->item_table().size() > 1,
          "and this client adopted the server's item table, so it can name what it carries");
    }

    // Frames, not ticks: this is the graphical client, and a network client that
    // arrives at one frame a second is not a client anybody can play.
    check(frames > (long)(played * 30.0), "rendered faster than 30 FPS while connected");

    std::printf(failures == 0 ? "network client passed\n" : "NETWORK CLIENT FAILED\n");
    conn->close();
    return failures == 0 ? 0 : 1;
}

// Steps both portals for one frame and teleports if either fires.
//
// One place rather than two, and it takes the pair rather than one portal,
// because the arrival has to be seeded the moment the departure fires: a player
// who lands in front of the destination and is not told they have already used
// it goes straight back where they came from, once per frame, for ever.
// The one place a crossing is written to the player.
//
// One site, and it stays one site: PORTAL_SYSTEM 4.1 sends position, view and
// velocity in a single packet because sending them apart leaves a frame where
// the player is in the new place with the old orientation. There is no packet
// here, so what has to hold is that nothing writes half of this -- which is why
// the cross-session branch below calls this rather than repeating it.
void apply_traversal(Scene &s, Player &player, const Traversal &t)
{
    player.pos       = t.feet;
    player.yaw       = t.yaw;
    player.pitch     = t.pitch;
    player.velocity  = player.velocity + t.velocity_delta;
    player.on_ground = false;
    // Cleared with the move, not after it. The exit was computed from the real
    // eye height, and a box still missing its bottom on the far side would let
    // the arriving player sink through whatever they came out on.
    player.cb_shift  = 0.0f;
    s.roll           = t.roll;

    s.cam_warp      = t.stepped_up ? t.warp_offset : Vec3{};
    s.cam_warp_time = cam_warp_time(length(t.warp_offset));
    s.cam_warp_left = t.stepped_up ? s.cam_warp_time : 0.0f;

    // Before the session swap, so `out of world` says the world being left.
    view_trace_crossing(s, t);
}

bool step_portals(Scene &s, Player &player, float dt)
{
    bool crossed = false;

    if (s.act().links_elsewhere()) {
        // Two worlds, one opening each. The destination lives in the OTHER
        // session, and so do the world and the block table the arrival is
        // checked against -- a step up must be resolved against the blocks the
        // player is landing among, not the ones they are leaving.
        const Portal &here  = s.act().portal_a;
        Session      &dest  = s.linked();
        const Portal &there = dest.portal_a;

        if (portal_step_trigger(here, s.act().trigger_a, player.pos, player.eye(),
                                player.velocity)) {
            const Traversal t = portal_traverse(here, there, player.pos, player.forward(),
                                                player.velocity, &dest.world, &dest.registry, player.eye_height);
            if (t.blocked) {
                ++s.refused_crossings;
                std::printf("portal: the destination world is blocked, not crossing\n");
            } else {
                apply_traversal(s, player, t);
                swap_sessions(s);

                // Seeded on what is NOW the active session, after the swap, so
                // the player does not arrive and immediately go back.
                portal_seed_arrival(s.act().portal_a, s.act().trigger_a, player.pos);
                aim_after_crossing(s, player);
                crossed = true;
            }
        }
    } else {
        struct Crossing {
            const Portal       *from;
            const Portal       *to;
            PortalTriggerState *state;
            PortalTriggerState *other;
        };
        const Crossing pair[2] = {
            {&s.act().portal_a, &s.act().portal_b, &s.act().trigger_a, &s.act().trigger_b},
            {&s.act().portal_b, &s.act().portal_a, &s.act().trigger_b, &s.act().trigger_a},
        };

        for (const Crossing &c : pair) {
            if (!portal_step_trigger(*c.from, *c.state, player.pos, player.eye(),
                                     player.velocity))
                continue;

            const Traversal t = portal_traverse(*c.from, *c.to, player.pos, player.forward(),
                                                player.velocity, &s.act().world,
                                                &s.act().registry, player.eye_height);
            if (t.blocked) {
                ++s.refused_crossings;
                std::printf("portal: the destination is blocked, not crossing\n");
                continue;
            }

            apply_traversal(s, player, t);
            portal_seed_arrival(*c.to, *c.other, player.pos);
            aim_after_crossing(s, player);
            crossed = true;
            break;   // one crossing per frame; the second would use a stale position
        }
    }

    if (!crossed) {
        s.cam_warp_left = std::max(0.0f, s.cam_warp_left - dt);
        settle_roll(s, dt);
        settle_look(s, player, dt);
    }

    return crossed;
}

int run_interactive(Scene &s, double seconds)
{
    plat_cursor_grab(true);

    // How long this run lasts, when somebody asked for a bounded one.
    //
    // --play-seconds went only to run_network until now, which meant that on
    // the single-process loop it did NOTHING: `cnp_client --play-seconds 20`
    // ran until somebody closed the window. A gate that started one and waited
    // for it waited for ever, and this is not a hypothesis -- it is how a
    // sabotage of the crash handler came to hang for twenty minutes.
    //
    // A flag that is accepted and ignored is worse than one that is refused.
    const double deadline = seconds > 0.0 ? plat_time() + seconds : 0.0;

    // --shot: the one screenshot, and when it is due.
    const double shot_due = g_shot_at >= 0.0 ? plat_time() + g_shot_at : -1.0;
    bool         shot_taken = false;

    // Six and a half nodes in front of the first opening, looking at it.
    //
    // Derived from the portal rather than written out, because there is more
    // than one scene now: --facing puts the pair somewhere else entirely, and a
    // spawn point spelled 40.5 47.0 in three places would leave a player
    // standing in an empty field wondering where the portals went. For the
    // spike pair this comes out at exactly the coordinates it used to be.
    // Do the two openings look at each other? Read off the geometry rather than
    // passed in as a flag, because it is the geometry that decides what the
    // scene wants, and a flag would let the two disagree.
    const bool pair_faces =
        dot(s.act().portal_a.normal(), s.act().portal_b.normal()) < -0.9f &&
        dot(normalize(s.act().portal_b.center - s.act().portal_a.center), s.act().portal_a.normal()) > 0.9f;

    // Closer for a facing pair: the aperture is the thing being looked at, and
    // at six and a half nodes a one-node opening is a postage stamp with three
    // levels of corridor inside it.
    const float stand_off = pair_faces ? 3.5f : 6.5f;

    const Vec3 spawn_n = s.act().portal_a.normal();
    const Vec3 spawn   = spawn_in_front(s.act().world, s.act().registry, s.act().portal_a,
                                        stand_off);

    // Facing the opening. dir_from_angles is (cos p sin y, sin p, -cos p cos y),
    // so forward == -normal means sin y = -n.x and cos y = n.z.
    const float spawn_yaw = std::atan2(-spawn_n.x, spawn_n.z);

    Player player;
    player.pos = spawn;
    player.yaw = spawn_yaw;

    std::printf("spawn: %.1f %.1f %.1f looking at the first opening\n", (double)spawn.x,
                (double)spawn.y, (double)spawn.z);

    // --model: on the ground between the player and the opening, turned three
    // quarters so its shape shows. It stood at a fixed spot beside the middle
    // of the world once, which was in front of the spawn until the spawn moved
    // to the opening -- and then the viewer showed nothing at all.
    if (s.model_ready) {
        s.model_pos = Vec3{spawn.x - spawn_n.x * 2.5f, spawn.y, spawn.z - spawn_n.z * 2.5f};
        s.model_yaw = spawn_yaw + 2.4f;
        std::printf("model: standing at %.1f %.1f %.1f\n", (double)s.model_pos.x,
                    (double)s.model_pos.y, (double)s.model_pos.z);
    }

    // The bag, and something in it. A world where the only way to get a block
    // is to break one is the right rule and a bad first minute: the plaza is
    // there to be built in, so the hand starts with a stack of the two blocks
    // the demo scene is made of.
    //
    // Held next to the player rather than inside Session: a bag belongs to
    // whoever is carrying it, and a player who walks through an opening does
    // not leave it in the world they came from.
    // On an ECS entity and not in a local, so that a stack thrown out of it has
    // somewhere to land: core/ecs/drop.h collects into "entities with a place
    // and an inventory", and this process needs to satisfy that query with the
    // ONE bag it has rather than with a copy kept in step by hand. It is the
    // same shape the server holds a player in, which is the point -- the ECS
    // was never a server thing.
    flecs::world demo_ecs;
    flecs::entity me = demo_ecs.entity().set<Inventory>(Inventory{}).set<ecs::Position>({});

    // And a mod can drop into it. Before the loop rather than inside it: a mod
    // handler runs on the same thread as this loop, so anything it drops has to
    // have somewhere to land from the first tick and not from the second.
    s.drops    = &demo_ecs;
    s.drops_me = me.id();

    // The grid this player crafts in. A local and not a component, because in
    // this process there is exactly one player and no session table to hang it
    // on -- and it is deliberately NOT in the bag, the same as on the server: a
    // bag is saved and a grid is not.
    CraftGrid demo_craft;
    s.craft = &demo_craft;
    s.bag   = &me.get_mut<Inventory>();

    // And how much of this player is left. Locals for the reason the grid is
    // one: one player, no session table. The rules are core's, so what happens
    // here to a fall and to a wound is what happens on the server.
    ecs::Health demo_health;
    ecs::Regen  demo_regen;
    FallWatch   demo_fall;
    double      demo_last_hit = -1.0;   // the server's cooldown on a hand, here
    double      demo_mobs_at  = 0.0;    // when the dark next fills a little
    s.health = &demo_health;
    s.regen  = &demo_regen;
    s.fall   = &demo_fall;
    s.player = &player;

    // --fly-down N: the scripted flight. Nobody holds a key down in a timed
    // run, so this loop presses them itself -- climb N nodes, come back down,
    // say what the landing cost. Phase 0 is the climb, 1 the way down, 2 done.
    const float scripted_top = spawn.y + g_fly_down;
    int scripted_phase = (g_fly_down > 0.0f && seconds > 0.0) ? 0 : -1;
    double scripted_landed_at = 0.0;
    bool   scripted_died      = false;

    {
        Inventory &bag = me.get_mut<Inventory>();
        for (const char *what : {"core:stone", "core:frame"}) {
            const ItemId id = s.act().items.find(what);
            if (id != kNoItem) bag.add(s.act().items, id, 64);
        }
        std::printf("carrying: %u x '%s' and %u x '%s', keys 1-9 pick a slot, Q drops one\n",
                    bag.at(0).count, s.act().items.get(bag.at(0).item).name.c_str(),
                    bag.at(1).count, s.act().items.get(bag.at(1).item).name.c_str());
    }

    bool  show_debug  = true;
    if (seconds <= 0.0) load_options(show_debug);
    s.play_arms = true;
    LookSettle look_settle;
    float watch_timer = 0.0f;

    // A crate sliding back and forth through the first opening. It is a demo
    // prop and it is named as one -- there is no item system and nothing here
    // is pickable -- but a cut is a thing to see, and until there is a second
    // player to watch there is nothing else in this world that goes through a
    // portal while you look at it.
    //
    // Not in the facing scene. There it slides along the line between the
    // player and the opening they came to look into, so it is in the way at
    // every point of its travel; and putting it anywhere else in a corridor
    // eleven nodes long puts it in the portal image instead. That scene counts
    // its levels with the block on the corridor floor and the two wall colours,
    // neither of which moves.
    if (!pair_faces) {
        s.act().bodies.assign(1, Body{});
        s.act().bodies[0].half   = Vec3{0.4f, 0.45f, 0.4f};
        s.act().bodies[0].colour[0] = 0.95f;
        s.act().bodies[0].colour[1] = 0.55f;
        s.act().bodies[0].colour[2] = 0.15f;
    }

    double crate_clock = 0.0;

    // How many bodies belong to the demo props, so the drops can be appended
    // after them and cleared each frame without taking the crate with them.
    // What the screen-shaped targets were last built for, and what the window
    // has become since.
    ScreenTargets screen_targets;
    screen_targets.start();

    const size_t prop_bodies = s.act().bodies.size();
    double       demo_clock  = 0.0;

    // Out of the bag and in front of the player. The same arithmetic the server
    // does for a net::DropItem -- flattened look, a fixed distance ahead, eye
    // height -- because a stack thrown here should land where a stack thrown
    // there does, and two versions of "in front of" is how they stop agreeing.
    auto throw_stack = [&](int slot, uint16_t count) {
        const ItemStack took = me.get_mut<Inventory>().take_from(slot, count);
        if (took.empty()) return;

        const Vec3  fwd = player.forward();
        const float len = std::sqrt(fwd.x * fwd.x + fwd.z * fwd.z);
        const float lx  = len > 1e-3f ? fwd.x / len : 0.0f;
        const float lz  = len > 1e-3f ? fwd.z / len : -1.0f;

        ecs::spawn_drop(demo_ecs, took, player.pos.x + lx * ecs::kThrowAhead,
                        player.pos.y + player.eye_height, player.pos.z + lz * ecs::kThrowAhead,
                        demo_clock);
    };

    while (!plat_should_close()) {
        window_keys(show_debug);
        if (deadline > 0.0 && plat_time() >= deadline) break;

        const ProfFrameScope prof_frame;

        // Nothing moves while the pause menu is up. Every clock, body and
        // script in this loop steps by dt, and each of them already takes a
        // zero as "no time passed".
        const float dt = s.pause.up ? 0.0f : plat_delta_time();

        // The same two lines the networked loop runs, from the same functions:
        // this loop is its own authority, so what it knows is its own open
        // panel rather than a message.
        {
            std::vector<Cell> open_now;
            if (s.hotbar.chest_open) open_now.push_back(s.hotbar.chest_at);
            advance_lids(s, dt, open_now);
            wire_lid_query(s, s.active);
            wire_model_draw(s, s.active);
        }

        // Fetched every frame rather than held across them: adding entities can
        // move a component's row, and a reference kept from last frame would be
        // pointing at where the bag used to be.
        Inventory &bag = me.get_mut<Inventory>();

        // Back and forth THROUGH the opening, which means along the portal's
        // own normal. Sliding it along z regardless was right for the spike
        // pair and wrong the moment there was a second scene: against a portal
        // facing x it slid the crate sideways into the wall.
        crate_clock += dt;
        if (!s.act().bodies.empty()) {
            const float d = 2.2f * std::sin((float)crate_clock * 0.55f);
            s.act().bodies[0].center = Vec3{s.act().portal_a.center.x + spawn_n.x * d, s.act().portal_a.center.y,
                                      s.act().portal_a.center.z + spawn_n.z * d};
        }

        // The bag, over the world. The same two keys as the networked client,
        // and the same rule while it is open: the cursor is free and nothing
        // reaches the world.
        if (std::string q; chest_search(s, q)) emit_ui_click(s, "core:chest", "search", q);
        if (!s.pause.up && !s.typing.on && plat_key_pressed(PLAT_KEY_E)) {
            // In this process there is no message to send -- the container set
            // and the grid are right here -- but the rule is the networked one:
            // shutting the panel shuts what was open behind it. See shut_bag.
            if (s.hotbar.open) {
                shut_bag(&s);
            } else {
                s.hotbar.open  = true;
                s.hotbar.press = -1;
            }
            plat_cursor_grab(!s.hotbar.open);
        }

        // A mod's panel takes the cursor and the clicks while it is up. Checked
        // before the bag, because it is drawn over it: whatever is on top is
        // what a click belongs to, and asking in the other order would send a
        // click to something the player cannot see.
        {
            const ui::CursorWish w =
                ui::panel_cursor(s.act().panels.topmost_for(ui::kLocalPlayer) != nullptr,
                                 s.hotbar.open, plat_cursor_grabbed(),
                                 s.hotbar.panel_took_cursor);
            s.hotbar.panel_took_cursor = w.took;
            if (w.change) {
                plat_cursor_grab(w.grab);
                if (w.grab) look_settle.grabbed();
            }
        }

        // Not with the pause menu up, which is drawn over it. See the
        // networked loop.
        const ui::Panel *panel = s.act().panels.topmost_for(ui::kLocalPlayer);
        if (panel && !s.pause.up) {

            // Copied before emitting: a handler can redeclare the panel,
            // which moves the vector this is pointing into.
            const std::string name = panel->name;
            if (plat_mouse_pressed(PLAT_MOUSE_LEFT)) {
                float mx = 0.0f, my = 0.0f;
                plat_mouse_position(&mx, &my);

                const std::string pressed = click_panel(s, *panel, mx, my);
                if (!pressed.empty()) emit_ui_click(s, name, pressed);
            }

            std::string field, typed;
            if (type_in_field(s, s.act().panels.topmost_for(ui::kLocalPlayer), field, typed))
                emit_ui_click(s, name, field, typed);
        } else if (s.typing.on && s.typing.panel != "core:chest") {
            stop_typing(s);
        }

        // Press, then release: one gesture with two ends, and ui::bag_click is
        // the whole rule. Both loops call the same function -- see the comment
        // on it.
        if (s.hotbar.open) {
            float mx = 0.0f, my = 0.0f;
            plat_mouse_position(&mx, &my);

            if (plat_mouse_pressed(PLAT_MOUSE_LEFT)) {
                const ui::BagPoint at = s.hotbar.point_at(mx, my);
                s.hotbar.press        = at.slot;
                s.hotbar.press_grid   = at.grid;
                s.hotbar.press_right  = false;
            }

            // What a Move means in this process: straight into the
            // inventories, because here they ARE the authority -- there is no
            // server to ask. Written once because two gestures produce one now,
            // the release of a drag and the right button part way through it,
            // and the second would have been the fifth copy of this rule.
            const auto apply_move = [&](const ui::BagAction &act) {
                // The same move_between the server calls for a networked
                // player, against the same two objects, and the same
                // craft_take.
                if (act.from_grid == ui::Grid::Result) {
                    craft_take(s.act().items, s.act().recipes, demo_craft, bag);
                    return;
                }

                Inventory *chest = s.hotbar.chest_open
                                       ? s.act().containers.find(s.hotbar.chest_at)
                                       : nullptr;
                const auto pick = [&](ui::Grid g) -> Inventory * {
                    switch (g) {
                    case ui::Grid::Chest: return chest;
                    case ui::Grid::Craft: return &demo_craft.slots;
                    case ui::Grid::Result: return nullptr;
                    case ui::Grid::Bag: break;
                    }
                    return &bag;
                };
                Inventory *from = pick(act.from_grid);
                Inventory *to   = pick(act.to_grid);

                // A square past the end of the CURRENT size is refused and not
                // clamped, the same rule the server applies: it would take a
                // stack out of sight, where nothing draws it and no recipe
                // reads it.
                const bool in_grid =
                    (act.from_grid != ui::Grid::Craft || act.from < demo_craft.used()) &&
                    (act.to_grid != ui::Grid::Craft || act.to < demo_craft.used());

                if (from != nullptr && to != nullptr && in_grid)
                    move_between(s.act().items, *from, act.from, *to, act.to, act.count);
            };

            // And what a Quick means: to the other grid, wherever it fits. The
            // destination is net::quick_target's answer and not this loop's --
            // the networked server decides it there, and a second copy here is
            // the sixth time that mistake would have been made in this file.
            const auto apply_quick = [&](const ui::BagAction &act) {
                if (act.from_grid == ui::Grid::Result) {
                    craft_take_all(s.act().items, s.act().recipes, demo_craft, bag);
                    return;
                }
                if (act.from_grid == ui::Grid::Craft && act.from >= demo_craft.used()) return;

                // The server's rule: with nothing open, armour goes on or off.
                if (side_of(act.from_grid) == net::Side::Bag && !s.hotbar.chest_open) {
                    bag.swap_worn(s.act().items, act.from);
                    return;
                }
                net::Side to = net::Side::Bag;
                if (!net::quick_target(side_of(act.from_grid), s.hotbar.chest_open, to)) return;

                Inventory *chest = s.hotbar.chest_open
                                       ? s.act().containers.find(s.hotbar.chest_at)
                                       : nullptr;
                const auto pick = [&](net::Side which) -> Inventory * {
                    switch (which) {
                    case net::Side::Container: return chest;
                    case net::Side::Craft: return &demo_craft.slots;
                    case net::Side::Bag: break;
                    }
                    return &bag;
                };
                Inventory *from   = pick(side_of(act.from_grid));
                Inventory *to_bag = pick(to);
                if (from == nullptr || to_bag == nullptr) return;

                if (quick_move(s.act().items, *from, act.from, *to_bag) && chest != nullptr &&
                    (from == chest || to_bag == chest))
                    s.act().containers.touched(s.hotbar.chest_at);
            };

            // The right button, and which of its two meanings this press has.
            // Part way through a drag it puts one item down; on its own it
            // starts a drag that carries half. See ui::bag_tap and ui::bag_half.
            if (plat_mouse_pressed(PLAT_MOUSE_RIGHT)) {
                if (plat_mouse_down(PLAT_MOUSE_LEFT)) {
                    ui::BagPoint press;
                    press.slot = s.hotbar.press;
                    press.grid = s.hotbar.press_grid;

                    const ui::BagAction one =
                        ui::bag_tap(press, s.hotbar.point_at(mx, my), s.hotbar.press_has_item());
                    if (one.kind == ui::BagAction::Move) apply_move(one);
                } else {
                    const ui::BagPoint at = s.hotbar.point_at(mx, my);
                    if (ui::half_drag_starts(at.grid)) {
                        s.hotbar.press       = at.slot;
                        s.hotbar.press_grid  = at.grid;
                        s.hotbar.press_right = true;
                    }
                }
            }

            if (plat_mouse_released(PLAT_MOUSE_RIGHT) && s.hotbar.press_right) {
                ui::BagPoint press;
                press.slot = s.hotbar.press;
                press.grid = s.hotbar.press_grid;

                const ui::BagAction act =
                    ui::bag_half(press, s.hotbar.point_at(mx, my), s.hotbar.press_count());
                if (act.kind == ui::BagAction::Move) apply_move(act);

                s.hotbar.press       = -1;
                s.hotbar.press_grid  = ui::Grid::Bag;
                s.hotbar.press_right = false;
            }

            if (plat_mouse_released(PLAT_MOUSE_LEFT)) {
                ui::BagPoint press;
                press.slot = s.hotbar.press;
                press.grid = s.hotbar.press_grid;

                const ui::BagPoint at = s.hotbar.point_at(mx, my);
                const ui::BagAction act =
                    plat_key_down(PLAT_KEY_SHIFT)
                        ? ui::bag_shift_click(press, at, s.hotbar.press_has_item())
                        : ui::bag_click(press, at, s.hotbar.press_has_item());

                if (act.kind == ui::BagAction::Move) apply_move(act);
                if (act.kind == ui::BagAction::Quick) apply_quick(act);
                if (act.kind == ui::BagAction::Drop) throw_stack(act.from, 0);

                s.hotbar.press       = -1;
                s.hotbar.press_grid  = ui::Grid::Bag;
                s.hotbar.press_right = false;
            }
        } else {
            s.hotbar.press       = -1;
            s.hotbar.press_grid  = ui::Grid::Bag;
            s.hotbar.press_right = false;
        }

        // Q throws what is in the hand on the ground: one of it, or the whole
        // stack with CTRL held. The same two the panel offers, from a key,
        // because a player with a bag closed should not have to open it to get
        // rid of something.
        if (!s.hotbar.open && !s.pause.up && plat_key_pressed(PLAT_KEY_Q))
            throw_stack(bag.selected(), plat_key_down(PLAT_KEY_CTRL) ? 0 : 1);

        // The wheel walks the bar, the same rule as the networked loop and the
        // same function: see ui::hotbar_scroll.
        if (!s.hotbar.open) {
            const float notches = plat_mouse_wheel();
            if (notches != 0.0f)
                bag.select(ui::hotbar_scroll(bag.selected(), notches, Inventory::kHotbar));
        } else if (s.hotbar.chest_open) {
            const float notches = plat_mouse_wheel();
            if (notches != 0.0f) emit_input(s, notches > 0.0f ? "wheel_up" : "wheel_down");
        }

        // The hotbar. Contiguous key ids, so this is a loop and not nine cases
        // -- see the comment on PLAT_KEY_1 in platform.h, which is why they are
        // in order.
        const int slot_before = bag.selected();
        for (int n = 0; n < Inventory::kHotbar; ++n) {
            if (plat_key_pressed((plat_key)(PLAT_KEY_1 + n)) && !s.typing.on) {
                bag.select(n);
                const ItemStack &now = bag.held();
                std::printf("slot %d: %s\n", n + 1,
                            now.empty() ? "empty"
                                        : s.act().items.get(now.item).name.c_str());
            }
        }

        // With the bag open, ESC closes the bag and gives the cursor back to
        // the world. Anything else makes ESC mean two things at once and leaves
        // a player pressing it twice to get out of one panel. Otherwise it is
        // the pause menu's, which here stops the world too: see `dt`.
        bool esc = plat_key_pressed(PLAT_KEY_ESC);
        // In a field, ESC lets go of the keyboard and does nothing else.
        if (esc && s.typing.on) {
            esc = false;
            stop_typing(s);
        }
        if (std::string said; chat_frame(s, look_settle, esc, said)) local_chat(s, said);
        if (esc && s.hotbar.open) {
            esc = false;
            shut_bag(&s);
            plat_cursor_grab(true);
            look_settle.grabbed();
        }
        // And a mod's panel, the server's rule here: off the screen, and its
        // mod hears ui::kPanelEscape.
        if (const ui::Panel *up = s.act().panels.topmost_for(ui::kLocalPlayer);
            esc && up != nullptr && !s.pause.up) {
            esc = false;
            const std::string name = up->name;
            std::string       perr;
            s.act().panels.show(name, up->owner, false, ui::kLocalPlayer, perr);
            emit_ui_click(s, name, ui::kPanelEscape);
        }
        if (pause_frame(s, look_settle, show_debug, esc, seconds <= 0.0)) break;
        if (plat_key_pressed(PLAT_KEY_F3))
            show_debug = !show_debug;
        if (plat_key_pressed(PLAT_KEY_F6))
            diag::prof_enable(!diag::prof_enabled());
        if (plat_key_pressed(PLAT_KEY_F2)) g_occlusion = !g_occlusion;

        // The next texture pack. Local worlds only: a world somebody else is
        // serving is drawn from what THAT server sent, and dressing it in a
        // pack from this machine would be a picture of the wrong game -- see
        // world_media().
        if (plat_key_pressed(PLAT_KEY_F7) && !g_pack_dirs.empty()) {
            select_pack(g_pack_index + 1);
            // Every mesh goes stale with the numbering, and rebuild_atlas is
            // what says so -- see its comment.
            s.act().world_render.rebuild_atlas(s.act().world, s.act().registry, read_texture_file, &s.act().items);
        }

        if (!s.pause.up && plat_key_pressed(PLAT_KEY_R)) {
            player.pos      = spawn;
            player.yaw      = spawn_yaw;
            player.velocity = Vec3{};
            // Said out loud because it is one of only three things in this loop
            // that write the player's aim, and a silent one would be a suspect
            // in every report about the view moving on its own.
            std::printf("R: back to the spawn, looking at the first opening\n");
        }

        // Editing a mod while the game runs is a reload, not a restart. F5
        // forces one; otherwise the files are polled twice a second, which is
        // often enough to feel immediate and rare enough to cost nothing.
        if (plat_key_pressed(PLAT_KEY_V) && !iso_engaged(s.act()))
            set_view_distance(s, s.act().view_distance > 0.0f ? 0.0f : kThirdPersonDistance);
        // A track piece in hand, turned before it is put down (the preview
        // shows it; the turn goes with the placement, net::Edit::turn).
        if (plat_key_pressed(PLAT_KEY_Z)) s.piece_turn = (uint8_t)((s.piece_turn + 3) & 3);
        if (plat_key_pressed(PLAT_KEY_X)) s.piece_turn = (uint8_t)((s.piece_turn + 1) & 3);
        s.nitro = s.act().movement.drives() ? player.nitro : -1.0f;

        if (plat_key_pressed(PLAT_KEY_F5)) {
            for (const Mod &m : s.act().mods.mods()) {
                std::string err;
                const std::string name = m.manifest.name;
                if (!s.act().mods.reload(name, err))
                    std::fprintf(stderr, "mod '%s' reload failed: %s\n", name.c_str(), err.c_str());
                else
                    std::printf("mod '%s' reloaded\n", name.c_str());
            }
        }

        watch_timer += dt;
        if (watch_timer >= 0.5f) {
            watch_timer = 0.0f;
            s.act().mods.reload_changed();
        }

        PlayerInput in;
        PlayerInput raw_look;
        if (plat_cursor_grabbed()) {
            in.move_forward = (plat_key_down(PLAT_KEY_W) ? 1.0f : 0.0f) -
                              (plat_key_down(PLAT_KEY_S) ? 1.0f : 0.0f);
            in.move_right   = (plat_key_down(PLAT_KEY_D) ? 1.0f : 0.0f) -
                              (plat_key_down(PLAT_KEY_A) ? 1.0f : 0.0f);
            in.jump       = plat_key_down(PLAT_KEY_SPACE);
            in.sprint     = plat_key_down(PLAT_KEY_SHIFT);
            in.descend    = plat_key_down(PLAT_KEY_CTRL);
            in.toggle_fly = plat_key_pressed(PLAT_KEY_F);
            plat_mouse_delta(&in.look_dx, &in.look_dy);

            // The first deltas after a grab are not movements. See LookSettle.
            look_settle.apply(in);
            if (in.look_dx != 0.0f || in.look_dy != 0.0f) s.look_settling = false;
            in.look_dx *= g_look_scale;
            in.look_dy *= g_look_scale;

            // What the hand did, before the horizon has its say. Kept so the
            // trace can print both: a view that goes somewhere the hand did not
            // ask for would otherwise be credited to the mouse.
            raw_look = in;

            // While the horizon is still tipped from a crossing, the mouse
            // works in screen axes and yaw and pitch do not. See
            // look_apply_roll.
            look_apply_roll(in, s.roll);
            steer_input(s, player, in);
        }
        // The scripted flight's keys: F to fly, space to climb, ctrl to come
        // down -- or, with --walked-down, F again at the top and gravity for
        // the rest of the way, which is a fall and has to hurt.
        if (scripted_phase == 0) {
            in = PlayerInput{};
            if (!player.flying) {
                in.toggle_fly = true;
            } else if (player.pos.y < scripted_top) {
                in.jump = true;
            } else {
                scripted_phase = 1;
                if (!g_fly_flown) in.toggle_fly = true;
                std::printf("scripted flight: %.1f nodes up, coming down %s\n",
                            (double)(player.pos.y - spawn.y),
                            g_fly_flown ? "flying" : "having let the flight go");
            }
        } else if (scripted_phase == 1) {
            in         = PlayerInput{};
            in.descend = player.flying;
        }

        update_floor_shift(s, player, dt);
        update_funnel(s, player, dt);
        const float y_before     = player.pos.y;
        const bool  was_grounded = player.on_ground && !player.vehicle;
        player_update(player, s.act().world, s.act().registry, s.act().movement, in, dt);
        s.step.note(player, was_grounded, y_before);
        s.step.settle(dt);
        sound_body(s, player, dt);

        // After the move and before the aim: a crossing changes where the
        // player is looking, and the outline should point at what they are
        // looking at now rather than at what they were looking at on the other
        // side of the portal.
        const bool crossed = step_portals(s, player, dt);

        // The camera as it now is, which is the camera render_frame is about to
        // build at the bottom of this loop. Here rather than there because only
        // here is it known whether this frame crossed, and only here is the
        // mouse input that produced it still in hand.
        view_trace_frame(s, player, raw_look, in, dt, crossed);

        // What the ground cost, and the net under the world.
        //
        // Bedrock closes the ordinary way out, but a portal put somewhere silly
        // will find another, and a body below the world is in a fall with no
        // bottom -- so that is a death rather than something to wait out. The
        // same two rules the server runs, through the same functions: see
        // core/physics/fall.h, which is in core for exactly this reason.
        if (player.pos.y < kBelowWorld) {
            hurt_local(&s, demo_health.max, false);
        } else if (player.flying) {
            // The server's rule: a flight down to the ground is not a fall.
            fall_reset(demo_fall, player.pos.y);
        } else if (s.act().registry.is_liquid(s.act().world.get_block(
                       (int)std::floor(player.pos.x), (int)std::floor(player.pos.y),
                       (int)std::floor(player.pos.z)))) {
            // And the server's other rule: water breaks a fall.
            fall_reset(demo_fall, player.pos.y);
        } else {
            const float fell = fall_step(demo_fall, player.pos.y);
            if (fell > 0.0f) {
                const uint16_t cost = fall_damage(fell);
                if (cost > 0)
                    std::printf("fell %.1f nodes, which cost %u\n", (double)fell,
                                (unsigned)cost);
                hurt_local(&s, cost, false);
            }
        }

        // The scripted flight is over the moment it is standing on something,
        // and what it costs is read before the death below puts it back at the
        // spawn with a full bar.
        if (scripted_phase == 1 && player.on_ground && scripted_landed_at == 0.0)
            scripted_landed_at = plat_time();

        // A quarter of a second after touching down, not the instant it does: a
        // fall is charged by the sample that stops going down (fall_step), so
        // the frame that lands is a frame too early to read the bar.
        if (scripted_landed_at > 0.0 && plat_time() >= scripted_landed_at + 0.25) {
            scripted_phase = 2;
            std::printf("scripted flight: landed at %.1f with %u/%u health%s\n",
                        (double)player.pos.y, (unsigned)demo_health.hp,
                        (unsigned)demo_health.max,
                        scripted_died ? ", having died on the way down" : "");
            break;
        }

        // And whatever killed them said so rather than moving them: this is the
        // only thing here that owns a position. The same split the server makes
        // between kill_body and deaths_to_serve.
        if (s.died) {
            s.died          = false;
            if (scripted_phase == 1) scripted_died = true;
            player.pos      = s.has_bed ? s.bed : spawn;
            player.yaw      = spawn_yaw;
            player.velocity = Vec3{};

            // Or the drop that killed them is charged a second time the moment
            // they land at the spawn.
            fall_reset(demo_fall, player.pos.y);
            std::printf("died, and was put back at the spawn\n");
        }

        {
            const uint16_t back = s.no_regen ? 0 : ecs::regen_step(demo_regen, demo_health, dt);
            if (back > 0) ecs::heal(demo_health, back);
        }

        // The window may have been dragged. The projection follows on its own --
        // it reads the screen every frame -- but the portal targets do not, and
        // they wait for the drag to settle. See ScreenTargets.
        if (!screen_targets.follow(s, plat_time())) {
            std::fprintf(stderr, "could not rebuild the portal targets at %dx%d\n",
                         gfx_screen_width(), gfx_screen_height());
            return 1;
        }

        // Aim once per frame; the outline, breaking and placing all read this,
        // so the block that lights up is exactly the block that gets edited.
        constexpr float kReach = 6.0f;
        s.target = raycast(s.act().world, s.act().registry, player.eye(), player.forward(), kReach);
        // Looking at a point (core.fixed_view) the eye aims at nothing: the
        // clicks are the world code's, and no block is outlined under them.
        if (s.act().iso_fixed && iso_engaged(s.act())) s.target = RaycastHit{};

        // The same line as the networked loop, for the same reason and from the
        // same function: a frame that is not a dig has to SAY it is not, or the
        // bar goes on showing the last dig that was interrupted.
        const bool claimed_right = claim_buttons(s, plat_cursor_grabbed() && !s.hotbar.open);
        if (s.digit_claimed) bag.select(slot_before);   // the bar has been walked above
        bool       left_down     = plat_mouse_down(PLAT_MOUSE_LEFT) && !s.left_claimed;
        bool       used_body     = false;   // a right click that went to a creature

        // A creature nearer than the block it stands in front of is struck and
        // not dug through: ServerSessions::strike's rules for the one player
        // here -- the cooldown, the hand's damage, the knock.
        if (plat_cursor_grabbed() && !s.hotbar.open) {
            flecs::entity_t aimed = 0;
            float           near  = kReach;
            demo_ecs.each([&](flecs::entity e, const ecs::Position &p, const ecs::Brain &) {
                const ecs::Wears *wears = e.try_get<ecs::Wears>();
                float t = 0.0f;
                if (ray_hits_box(creature_box(s.act().models, wears ? wears->model : 0,
                                              Vec3{p.x, p.y, p.z}),
                                 player.eye(), player.forward(), near, t) &&
                    t < near) {
                    near  = t;
                    aimed = e.id();
                }
            });
            // A door with somebody in its doorway: the right button is the door's.
            const bool body_in_door =
                plat_mouse_pressed(PLAT_MOUSE_RIGHT) && s.target.hit && s.target.distance - near < 1.0f &&
                s.act().registry.get(s.act().world.get_block(s.target.x, s.target.y, s.target.z)).door != 0;
            if (aimed != 0 && !body_in_door && (!s.target.hit || near <= s.target.distance)) {
                if (plat_mouse_pressed(PLAT_MOUSE_LEFT) && !s.left_claimed &&
                    demo_clock - demo_last_hit >= ecs::kHitCooldown) {
                    flecs::entity    e    = demo_ecs.entity(aimed);
                    const ecs::Position &p = e.get<ecs::Position>();
                    const uint16_t before  = e.get<ecs::Health>().hp;
                    ecs::hurt(e.get_mut<ecs::Health>(), net::hit_damage(s.act().items, bag.slots(), bag.selected()));
                    e.modified<ecs::Health>();
                    bag.wear_slot(s.act().items, bag.selected(), 1);
                    {
                        const uint16_t after = e.get<ecs::Health>().hp;
                        script_field   hf[4] = {
                            {"player", SCRIPT_V_NUMBER, (double)ui::kLocalPlayer, nullptr, 0},
                            {"target", SCRIPT_V_NUMBER, (double)aimed, nullptr, 0},
                            {"damage", SCRIPT_V_NUMBER, (double)(before - after), nullptr, 0},
                            {"killed", SCRIPT_V_BOOL, 0.0, nullptr, after == 0 ? 1 : 0},
                        };
                        std::string hit_err;
                        if (!s.act().mods.events().emit("core:hit", hf, 4, nullptr, hit_err))
                            std::fprintf(stderr, "emit core:hit: %s\n", hit_err.c_str());
                    }
                    ecs::knock_back(e.get_mut<ecs::Brain>(), player.pos.x, player.pos.z, p.x, p.z);
                    demo_last_hit = demo_clock;
                }
                left_down = false;   // and the block behind it is not dug

                // A right click uses it: a pet tamed with what is in the hand,
                // or told to sit -- use_pet, as the server does it. The block
                // behind it is not built against.
                if (plat_mouse_pressed(PLAT_MOUSE_RIGHT) && !claimed_right &&
                    demo_ecs.entity(aimed).has<ecs::Act>()) {
                    const ItemStack  &held = bag.held();
                    const std::string name = held.empty() ? std::string() : s.act().items.get(held.item).name;
                    script_field uf[3] = {
                        {"player", SCRIPT_V_NUMBER, (double)ui::kLocalPlayer, nullptr, 0},
                        {"target", SCRIPT_V_NUMBER, (double)(uint32_t)aimed, nullptr, 0},
                        {"held", SCRIPT_V_STRING, 0.0, name.c_str(), 0},
                    };
                    std::string use_err;
                    if (!s.act().mods.events().emit("core:used_mob", uf, 3, nullptr, use_err))
                        std::fprintf(stderr, "emit core:used_mob: %s\n", use_err.c_str());
                    used_body = true;
                } else if (plat_mouse_pressed(PLAT_MOUSE_RIGHT) && !claimed_right) {
                    const ItemStack &held = bag.held();
                    bool             took = false;
                    std::string      why;
                    const ItemId hand  = held.empty() ? kNoItem : held.item;
                    const bool   shorn = hand != kNoItem &&
                                       ecs::shear_mob(demo_ecs, demo_ecs.entity(aimed), hand, demo_clock);
                    const bool   fed   = !shorn && hand != kNoItem &&
                                       ecs::feed_mob(demo_ecs.entity(aimed), hand, demo_clock);
                    if (shorn) bag.wear_slot(s.act().items, bag.selected(), 1);
                    if (fed) bag.take_from(bag.selected(), 1);
                    if (!shorn && !fed && ecs::use_pet(demo_ecs.entity(aimed), g_self_name,
                                               held.empty() ? kNoItem : held.item, took, why) && took)
                        bag.take_from(bag.selected(), 1);
                    used_body = true;
                }
            }
        }

        if (!dig_frame(left_down, s.target.hit, plat_cursor_grabbed() && !s.hotbar.open, false))
            dig_idle(s.dig, &s.dig_progress);

        // The use of water, with nothing solid in reach: the networked loop's
        // rule, in the same place in the frame.
        if (plat_cursor_grabbed() && !s.hotbar.open && !s.target.hit && !used_body &&
            plat_mouse_pressed(PLAT_MOUSE_RIGHT) && !claimed_right) {
            const RaycastHit wet = raycast(s.act().world, s.act().registry, player.eye(),
                                           player.forward(), kReach, true);
            if (wet.hit)
                emit_block_event(s, "core:used", wet.x, wet.y, wet.z,
                                 (int)s.act().world.get_block(wet.x, wet.y, wet.z));
        }

        if (plat_cursor_grabbed() && !s.hotbar.open && s.target.hit) {
            if (left_down) {
                // Held, not pressed. A block with a hardness takes seconds and
                // the button is what pays for them; a block with none still
                // comes away on the press frame alone, which is what keeps one
                // click to one block over a floor of them -- see dig_step.
                const BlockId under =
                    s.act().world.get_block(s.target.x, s.target.y, s.target.z);
                const double need = dig_need(
                    s.act().registry, s.act().items, under,
                    bag.at(net::best_dig_slot(s.act().items, bag.slots(), bag.selected(),
                                              s.act().registry.get(under).material)));

                if (dig_step(s.dig, plat_mouse_pressed(PLAT_MOUSE_LEFT), true, s.target.hit,
                             s.target.x, s.target.y, s.target.z, under, need, demo_clock,
                             &s.dig_progress)) {
                    // The sound AFTER the break and not before it. It used to
                    // be unconditional, which was harmless while every block
                    // came out for every hand: now a dig can be refused, and a
                    // break the player hears but does not get is the worst way
                    // to be told no.
                    std::string why;
                    if (break_into_bag(s, bag, s.target.x, s.target.y, s.target.z, &why))
                        play_at(s, s.sounds.broke.any(),
                                Vec3{(float)s.target.x + 0.5f, (float)s.target.y + 0.5f,
                                     (float)s.target.z + 0.5f},
                                s.active);
                    s.notice.say(why, demo_clock);
                }
            } else if (plat_mouse_pressed(PLAT_MOUSE_RIGHT) && !claimed_right && !used_body) {
                // A use FIRST, whatever is in the hand, and cancellable: a mod
                // that cancels has claimed the click and the placement does not
                // happen. The same order the server keeps -- see
                // net::EditPlan::use -- and what lets a chest open with a block
                // in hand without also building on top of it.
                // Shift with a block in hand only places, as on the server.
                const bool place_only = plat_key_down(PLAT_KEY_SHIFT) &&
                                        s.act().items.get(bag.held().item).places != kAir;
                const bool claimed =
                    !place_only &&
                    !emit_block_event(s, "core:used", s.target.x, s.target.y, s.target.z,
                                      (int)s.act().world.get_block(s.target.x, s.target.y,
                                                                   s.target.z));

                // Into the empty cell in front of the face that was hit, and
                // only if the player is not standing there.
                const int px = s.target.x + s.target.nx;
                const int py = s.target.y + s.target.ny;
                const int pz = s.target.z + s.target.nz;

                const AABB box = body_box(player.pos, s.act().movement.width * 0.5f);
                const bool inside_player =
                    (float)px < box.max.x && (float)(px + 1) > box.min.x &&
                    (float)py < box.max.y && (float)(py + 1) > box.min.y &&
                    (float)pz < box.max.z && (float)(pz + 1) > box.min.z;

                // A mod can refuse the placement, or change what gets placed.
                // What gets placed at all is whatever is in the hand, and the
                // hand can be empty -- which is the first thing in this engine
                // that a player can run out of.
                if (!claimed && !inside_player) {
                    std::string why;
                    if (place_from_bag(s, bag, px, py, pz, player.pos, &why))
                        play_at(s, s.sounds.placed.any(),
                                Vec3{(float)px + 0.5f, (float)py + 0.5f, (float)pz + 0.5f},
                                s.active);
                    s.notice.say(why, demo_clock);
                }
            }
        }

        step_script(s, dt);

        // The world around the player, when it has no edge.
        step_columns(s.act(), player.pos.x, player.pos.z);

        advance_clocks(s, dt);
        sync_sessions(s);

        // Stacks on the ground: the same two calls the server's tick makes, on
        // the same code. The avatar's place is written first because that is
        // what "standing on it" is measured against.
        demo_clock    += (double)dt;
        s.drop_clock   = demo_clock;
        s.notice_clock = demo_clock;
        s.player_x     = player.pos.x;
        s.player_y     = player.pos.y;
        s.player_z     = player.pos.z;

        // A chest walked away from, closed in whatever state it is in. The same
        // function the networked server calls, given this loop's own eye: the
        // last three defects around chests were one rule written twice, and
        // this is the fourth place it would have been.
        if (s.hotbar.chest_open &&
            !container_in_reach(s.hotbar.chest_at, player.pos.x, player.pos.y + player.eye_height,
                                player.pos.z, MovementLimits{}.place_reach)) {
            s.hotbar.chest_open = false;
        }

        // And a table walked away from, which shuts the GRID and not the panel:
        // the 3x3 becomes the 2x2 everybody carries, and the bag stays open
        // around it. The same rule the server runs in close_far_crafts, through
        // the same function -- and this loop had no rule at all, so a player
        // could walk to the far side of the world still arranging a table they
        // had left behind.
        if (craft_out_of_reach(demo_craft, player.pos.x, player.pos.y + player.eye_height,
                               player.pos.z, MovementLimits{}.place_reach))
            host_craft_close(&s, (int)ui::kLocalPlayer);
        {
            ecs::Position &at = me.get_mut<ecs::Position>();
            at.x     = player.pos.x;
            at.y     = player.pos.y;
            at.z     = player.pos.z;
            at.valid = true;
        }
        // The creatures, as the server's tick runs them (core/ecs/mob.h), with
        // this player the one they go for; and every two seconds the dark
        // fills a little and the far ones go.
        if (!s.act().mob_kinds.empty()) {
            Session &w = s.act();
            find_mob_drops(w);
            std::vector<ecs::Target> targets;
            targets.push_back({me.id(), 1, player.pos.x, player.pos.y, player.pos.z, g_self_name});
            if (!demo_ecs.has<ecs::MobVoice>())
                demo_ecs.set<ecs::MobVoice>({[&s](uint16_t kind, const char *what, Vec3 at) {
                    const std::vector<ecs::MobKind> &kinds = s.act().mob_kinds;
                    if (kind < kinds.size())
                        play_at(s, s.sounds.named(kinds[kind].name + "_" + what), at, s.active);
                }});
            ecs::step_mobs(
                demo_ecs, w.world, w.registry, w.mob_kinds, targets, dt, demo_clock,
                daylight_at(w.time_of_day),
                [&](uint32_t, uint16_t amount, uint16_t, Vec3 push) {
                    hurt_local(&s, amount);
                    if (!player.vehicle && !player.hanging) {   // the networked client's rule
                        player.velocity.x += push.x;
                        player.velocity.z += push.z;
                        player.velocity.y = std::max(player.velocity.y, push.y);
                        if (push.y > 0.0f) player.on_ground = false;
                    }
                },
                [&](int x, int y, int z) {
                    ecs::swap_door(w.world, w.registry, x, y, z,
                                   [&](int bx, int by, int bz, BlockId b, uint8_t f) {
                                       host_set_block(&s, bx, by, bz, (int)b);
                                       host_set_facing(&s, bx, by, bz, (int)f);
                                   });
                });
            if (demo_clock >= demo_mobs_at) {
                demo_mobs_at = demo_clock + 2.0;
                const ecs::SpawnResult sr = ecs::spawn_and_despawn(
                    demo_ecs, w.world, w.registry, w.mob_kinds, targets,
                    daylight_at(w.time_of_day), 6, (uint32_t)(demo_clock * 20.0), demo_clock);
                if (sr.came || sr.went)
                    std::printf("creatures: %d came out of the dark, %d went\n", sr.came, sr.went);
            }
        }
        if (const size_t died = ecs::reap_dead(demo_ecs, demo_clock); died > 0)
            std::printf("%zu creature%s died\n", died, died == 1 ? "" : "s");

        ecs::step_drops(demo_ecs, s.act().world, s.act().registry, dt);
        ecs::collect_drops(demo_ecs, s.act().items, demo_clock);
        ecs::step_falling(demo_ecs, s.act().world, s.act().registry, dt,
                          [&](int x, int y, int z, BlockId block, uint8_t facing, int fell) {
                              Session      &w     = s.act();
                              const BlockId there = w.world.get_block(x, y, z);
                              if (there == kAir || !w.registry.is_solid(there)) {
                                  host_set_block(&s, x, y, z, block);
                                  w.world.set_facing(x, y, z, facing);
                              } else if (const ItemId item = w.items.of_block(block); item != kNoItem) {
                                  ItemStack stack;
                                  stack.item  = item;
                                  stack.count = 1;
                                  ecs::spawn_drop(demo_ecs, stack, (float)x + 0.5f, (float)y + 1.2f,
                                                  (float)z + 0.5f, demo_clock);
                              }
                              script_field f[5] = {
                                  {"x", SCRIPT_V_NUMBER, (double)x, nullptr, 0},
                                  {"y", SCRIPT_V_NUMBER, (double)y, nullptr, 0},
                                  {"z", SCRIPT_V_NUMBER, (double)z, nullptr, 0},
                                  {"id", SCRIPT_V_NUMBER, (double)block, nullptr, 0},
                                  {"fell", SCRIPT_V_NUMBER, (double)fell, nullptr, 0},
                              };
                              bool        ignored = false;
                              std::string err;
                              w.mods.events().emit("core:landed", f, 5, &ignored, err);
                          });
        ecs::expire_drops(demo_ecs, demo_clock);

        // And they are drawn as the networked client draws them.
        s.act().bodies.resize(prop_bodies);
        demo_ecs.each([&](flecs::entity e, const ecs::Position &p, const ecs::Dropped &d) {
            s.act().bodies.push_back(
                drop_body(s.act(), s.act().items.get(d.stack.item), Vec3{p.x, p.y, p.z}, e.id()));
        });
        demo_ecs.each([&](flecs::entity, const ecs::Position &p, const ecs::Falling &f) {
            s.act().bodies.push_back(falling_body(f.block, Vec3{p.x, p.y, p.z}));
        });

        // And the creatures and their arrows, as the networked client draws
        // what the server streams of them: the same boxes, clips and colours.
        std::unordered_map<uint64_t, Body> creatures_now;
        demo_ecs.each([&](flecs::entity e, const ecs::Position &p, const ecs::Look &look,
                          const ecs::Brain &) {
            Body b;
            b.center    = Vec3{p.x, p.y + ecs::kMobHeight * 0.5f, p.z};
            b.half      = Vec3{ecs::kMobHalfWidth, ecs::kMobHeight * 0.5f, ecs::kMobHalfWidth};
            b.colour[0] = 0.30f;
            b.colour[1] = 0.70f;
            b.colour[2] = 0.35f;
            b.yaw       = look.yaw;
            if (const ecs::Wears *wears = e.try_get<ecs::Wears>()) b.model = wears->model;
            const char *work = nullptr;
            if (const ecs::Act *act = e.try_get<ecs::Act>()) {   // a worker's, as its mod said
                // Known even when empty: no tool_* bone shows then.
                b.hand_known = true;
                if (act->held != kNoItem) {
                    const ItemDef &held = s.act().items.get(act->held);
                    b.hand       = held.name;
                    b.hand_block = held.places;
                }
                if (!act->clip.empty()) work = act->clip.c_str();
            } else if (e.has<ecs::Working>()) {
                const ModelStore::Entry *me = s.act().models.entry(b.model);
                work = !me                                 ? nullptr
                     : me->model.clip("attack") >= 0       ? "attack"
                     : me->model.clip("sit") >= 0          ? "sit"   // a pet told to sit
                                                           : "shoot";
            }
            if (b.model != 0)
                animate_body(s.act(), e.id(), b, Vec3{p.x, p.y, p.z},
                             s.act().declared_player_model, nullptr, work);
            s.act().bodies.push_back(b);
            if (b.model != 0) creatures_now[e.id()] = b;
            if (e.has<ecs::Burning>()) add_flames(s.act().bodies, Vec3{p.x, p.y, p.z}, e.id());
        });
        fade_the_gone(s.act(), creatures_now, plat_time());
        demo_ecs.each([&](flecs::entity e, const ecs::Position &p, const ecs::Look &look,
                          const ecs::Arrow &) {
            Body b;
            b.center    = Vec3{p.x, p.y + 0.25f, p.z};
            b.half      = Vec3{0.25f, 0.25f, 0.25f};
            b.colour[0] = 0.55f;
            b.colour[1] = 0.60f;
            b.colour[2] = 0.95f;
            b.yaw       = look.yaw;
            b.pitch     = look.pitch;
            if (const ecs::Wears *wears = e.try_get<ecs::Wears>()) b.model = wears->model;
            s.act().bodies.push_back(b);
        });

        // What this world's mods made visible (core.ecs.visible), drawn as the
        // networked client draws the server's: the same box, or the model it
        // wears (TODO.md OPEN-20).
        s.mod_ecs.each([&](flecs::entity e, const ecs::Position &p, const ecs::Look &look,
                           ecs::Thing) {
            Body b;
            b.center    = Vec3{p.x, p.y + 0.25f, p.z};
            b.half      = Vec3{0.25f, 0.25f, 0.25f};
            b.colour[0] = 0.55f;
            b.colour[1] = 0.60f;
            b.colour[2] = 0.95f;
            b.yaw       = look.yaw;
            b.pitch     = look.pitch;
            if (const ecs::Wears *wears = e.try_get<ecs::Wears>()) b.model = wears->model;
            const ecs::Act *act = e.try_get<ecs::Act>();   // core.ecs.clip
            if (b.model != 0)
                animate_body(s.act(), e.id() | (1ull << 62), b, Vec3{p.x, p.y, p.z},   // not a creature's key
                             s.act().declared_player_model, nullptr, act ? act->clip.c_str() : nullptr);
            s.act().bodies.push_back(b);
        });

        // A local world's models, the first frame there is somebody to wear
        // them: its mods declared them (model_defs) and nothing else loads
        // them here. A model that will not load is said once and left a box.
        if (s.act().models.size() < s.act().model_defs.size()) {
            for (ModelId id = 1; id <= (ModelId)s.act().model_defs.size(); ++id) {
                if (s.act().models.has(id)) continue;
                const ModelRegistry::ModelDef &d = s.act().model_defs.get(id);
                std::string why;
                if (!s.act().models.put(id, d.name, d.file, why))
                    std::fprintf(stderr, "model '%s': %s\n", d.name.c_str(), why.c_str());
            }
        }
        s.act().player_model_id = s.act().declared_player_model;

        const bool holding = s.bag != nullptr && !s.bag->held().empty();
        s.hand       = holding ? s.act().items.get(s.bag->held().item).name : std::string();
        s.hand_block = holding ? s.act().items.get(s.bag->held().item).places : kAir;
        s.hand_tag   = holding ? s.bag->held().tag : 0;
        s.working    = s.dig_progress > 0.0f ||
                    (plat_cursor_grabbed() && plat_mouse_down(PLAT_MOUSE_LEFT));
        s.arms_clip  = body_arms_clip(s.act(), player);
        hold_arms(s, player);
        add_own_body(s, player, s.act().declared_player_model);

        // The bar reads the bag, every frame. Not a copy kept in step by hand:
        // the bag is the only inventory in this process, and anything that
        // changed it since the last frame has to show.
        s.hotbar.slots      = bag.slots();
        s.hotbar.selected   = bag.selected();
        s.hotbar.items      = &s.act().items;
        s.hotbar.health     = demo_health.hp;
        s.hotbar.health_max = demo_health.max;

        // And the chest, from the same place. Read rather than kept in step,
        // because a mod's handler can have changed it since the last frame.
        if (s.hotbar.chest_open) {
            const Inventory *chest = s.act().containers.find(s.hotbar.chest_at);
            if (chest != nullptr) {
                s.hotbar.chest = chest->slots();
                s.hotbar.chest_roles.resize(s.hotbar.chest.size());
                for (int i = 0; i < chest->size(); ++i) s.hotbar.chest_roles[(size_t)i] = chest->role(i);
                float done = 0.0f, rate = 0.0f;
                const bool going = s.act().containers.progress(s.hotbar.chest_at, s.drop_clock, done, rate);
                s.hotbar.chest_progress = going ? done : -1.0f;
                s.hotbar.chest_rate     = 0.0f;   // read afresh every frame here
                if (!s.act().containers.scroll(s.hotbar.chest_at, s.hotbar.chest_scroll,
                                               s.hotbar.chest_scroll_size))
                    s.hotbar.chest_scroll = s.hotbar.chest_scroll_size = 0.0f;
                std::string q;
                s.hotbar.chest_search = s.act().containers.search(s.hotbar.chest_at, q);
                if (!(s.typing.on && s.typing.panel == "core:chest")) s.hotbar.chest_query = q;
                s.hotbar.chest_since    = plat_time();
            } else {
                // The block stopped being a chest under the player's hands.
                s.hotbar.chest_open = false;
                s.hotbar.chest.clear();
            }
        } else {
            s.hotbar.chest.clear();
        }

        // And the craft grid, read every frame for the same reason: a mod's
        // handler can have opened a table since the last one.
        s.hotbar.craft_w = demo_craft.w;
        s.hotbar.craft_h = demo_craft.h;
        s.hotbar.craft.assign(demo_craft.slots.slots().begin(),
                              demo_craft.slots.slots().begin() + demo_craft.used());
        s.hotbar.craft_result =
            s.act().recipes.craft(s.act().items, demo_craft.slots, demo_craft.w, demo_craft.h);

        camera_follow(s, player, dt);
        const Vec3 cam = camera_eye(s, player);
        render_frame(s, cam, camera_forward(s, player, cam), camera_up(s, player), show_debug,
                     "WASD move  F fly  LMB break  RMB place  1-9 slot  E bag  ESC pause  "
                     "F3 hud  F6 profiler  "
                     "F5 portal target  V view  Z/X turn piece");

        // --shot: after the frame, so what lands on disk is the frame that was
        // just drawn and not the one before it.
        if (shot_due > 0.0 && !shot_taken && plat_time() >= shot_due) {
            shot_taken = true;
            shot_write("play.png");
            std::printf("screenshot: bench/play.png\n");
        }
    }
    if (seconds <= 0.0) save_options(show_debug);
    return 0;
}

// Which side a mod in this process runs on (TODO.md OPEN-49). A process connected
// to a world runs that world's code; one that is not is its own server and runs
// both halves of everything.
const char *host_side(void *ud)
{
    const Scene *s = static_cast<Scene *>(ud);
    return s->conn != nullptr ? "client" : "both";
}

// Everything this process lets a mod reach, in one table. One function because
// two things use it now: a single-process world's own mods get all of it, and a
// connected world's code gets it through world_code_host(), which takes away
// whatever writes the world (TODO.md OPEN-49).
script_host make_client_host(Scene &scene)
{
    script_host host{};
    host.set_block     = host_set_block;
    host.set_facing    = host_set_facing;
    host.get_facing    = host_get_facing;
    host.get_block     = host_get_block;
    host.set_portal    = host_set_portal;
    host.get_portal    = host_get_portal;
    host.portal_open   = host_portal_open;
    host.portal_close  = host_portal_close;
    host.log           = host_log;
    host.register_node  = host_register_node;
    host.register_mapgen = host_register_mapgen;
    host.world_sky       = host_world_sky;
    host.world_storm     = host_world_storm;
    host.storm_funnel    = host_storm_funnel;
    host.mapgen_core     = host_mapgen_core;
    host.portal_place    = host_portal_place;
    host.register_abm    = host_register_abm;
    host.register_lbm    = host_register_lbm;
    host.terrain_height  = host_terrain_height;
    host.water_level     = host_water_level;
    host.biome           = host_biome;
    host.node_liquid     = host_node_liquid;
    host.register_model = host_register_model;
    host.player_model   = host_player_model;
    host.body_model     = host_body_model;
    host.model_id       = host_model_id;
    host.view           = host_view;
    host.iso_view       = host_iso_view;
    host.fixed_view     = host_fixed_view;
    host.cursor_ray     = host_cursor_ray;
    host.walk           = host_walk;
    host.mobs_near      = host_mobs_near;
    host.mob_ids        = host_mob_ids;
    host.mob_pos        = host_mob_pos;
    host.mob_goto       = host_mob_goto;
    host.mob_going      = host_mob_going;
    host.mob_act        = host_mob_act;
    host.mob_face       = host_mob_face;
    host.mob_remove     = host_mob_remove;
    host.mob_place      = host_mob_place;
    host.slow_mobs      = host_slow_mobs;
    host.engine_hud     = host_engine_hud;
    host.engine_hud_on  = host_engine_hud_on;
    host.mob_model      = host_mob_model;
    host.register_mob   = host_register_mob;
    host.spawn_mob      = host_spawn_mob;
    host.mob_tame       = host_mob_tame;
    host.mob_shear      = host_mob_shear;
    host.mob_lays       = host_mob_lays;
    host.mob_drop       = host_mob_drop;
    host.push_mobs      = host_push_mobs;
    host.monsters_near  = host_monsters_near;
    host.mob_sight      = host_mob_sight;
    host.arms_clip      = host_arms_clip;
    host.zoom           = host_zoom;
    host.rope           = host_rope;
    host.held_model     = host_held_model;
    host.mob_breed      = host_mob_breed;
    host.players       = host_players;
    host.find_nodes    = host_find_nodes;
    host.find_path     = host_find_path;
    host.ecs_component = host_ecs_component;
    host.ecs_phase     = host_ecs_phase;
    host.ecs_system    = host_ecs_system;
    host.ecs_spawn     = host_ecs_spawn;
    host.ecs_set       = host_ecs_set;
    host.ecs_get       = host_ecs_get;
    host.ecs_kill      = host_ecs_kill;
    host.ecs_visible   = host_ecs_visible;
    host.ecs_model     = host_ecs_model;
    host.ecs_clip      = host_ecs_clip;
    host.ecs_box       = host_ecs_box;
    host.ecs_health    = host_ecs_health;
    host.ecs_count     = host_ecs_count;
    host.net_open      = host_net_open;
    host.net_send      = host_net_send;
    host.render_pass   = host_render_pass;
    host.render_box    = host_render_box;
    host.render_fx     = host_render_fx;
    host.render_model  = host_render_model;
    host.hud_pass      = host_hud_pass;
    host.hud_text      = host_hud_text;
    host.hud_rect      = host_hud_rect;
    host.hud_image     = host_hud_image;
    host.hud_item      = host_hud_item;
    host.hud_mouse     = host_hud_mouse;
    host.hud_aspect    = host_hud_aspect;
    host.player_look   = host_player_look;
    host.craft_make    = host_craft_make_here;
    host.craft_keys    = host_craft_keys_here;
    host.craft_pose    = host_craft_pose_here;
    host.player_place  = host_player_place;
    host.player_lie    = host_player_lie;
    host.player_clip   = host_player_clip;
    host.player_wear   = host_player_wear;
    host.play_sound    = host_play_sound;
    host.raycast       = host_raycast;
    host.movement      = host_movement;
    host.body_movement = host_body_movement;
    host.body_drive    = host_body_drive;
    host.notice        = host_notice;
    host.chat          = host_chat;
    host.command       = host_command;
    host.has_perm      = host_has_perm;
    host.role          = host_role;
    host.is_muted      = host_is_muted;
    host.page_register = host_page_register;
    host.page_show     = host_page_show;
    host.storage_set   = host_storage_set;
    host.storage_get   = host_storage_get;
    host.shared_storage_set = host_storage_set;   // one world here: the same store
    host.shared_storage_get = host_storage_get;
    host.drive         = host_drive;
    host.player_pos    = host_player_pos;
    host.take_item     = host_take_item;
    host.bag_count     = host_bag_count;
    host.shoot_arrow   = host_shoot_arrow;
    host.fall          = host_fall;
    host.replace_nodes = host_replace_nodes;
    host.repair_held   = host_repair_held;
    host.node_falls    = host_node_falls;
    host.node_glass    = host_node_glass;
    host.give_item     = host_give_item;
    host.meta_set      = host_meta_set;
    host.meta_get      = host_meta_get;
    host.register_item = host_register_item;
    host.item_id       = host_item_id;
    host.item_count    = host_item_count;
    host.item_name     = host_item_name;
    host.item_stack    = host_item_stack;
    host.ui_define     = host_ui_define;
    host.ui_label      = host_ui_label;
    host.ui_button     = host_ui_button;
    host.ui_item = host_ui_item;
    host.ui_field      = host_ui_field;
    host.ui_box        = host_ui_box;
    host.ui_model      = host_ui_model;
    host.ui_show       = host_ui_show;
    host.recipe_shaped    = host_recipe_shaped;
    host.recipe_shapeless = host_recipe_shapeless;
    host.recipe_dye       = host_recipe_dye;
    host.item_dye         = host_item_dye;
    host.worn_id          = host_worn_id;
    host.worn_set_id      = host_worn_set_id;
    host.craft_show      = host_craft_show;
    host.craft_close     = host_craft_close;
    host.damage          = host_damage;
    host.heal            = host_heal;
    host.health          = host_health;
    host.container_spill = host_container_spill;
    host.drop_item       = host_drop_item;
    host.container_make  = host_container_make;
    host.container_add   = host_container_add;
    host.container_show  = host_container_show;
    host.container_close = host_container_close;
    host.container_count = host_container_count;
    host.container_fill  = host_container_fill;
    host.container_revision = host_container_revision;
    host.container_move  = host_container_move;
    host.collect_drops   = host_collect_drops;
    host.drop_at = host_drop_at;
    host.drop_send = host_drop_send;
    host.container_drop = host_container_drop;
    host.container_item = host_container_item;
    host.container_take  = host_container_take;
    host.container_clear = host_container_clear;
    host.node_id       = host_node_id;
    host.node_box      = host_node_box;
    host.node_model    = host_node_model;
    host.node_drop     = host_node_drop;
    host.node_drop_item = host_node_drop_item;
    host.node_alias     = host_node_alias;
    host.node_solid     = host_node_solid;
    host.light          = host_light;
    host.block_light    = host_block_light;
    host.node_turns     = host_node_turns;
    host.loaded         = host_loaded;
    host.watch_cell     = host_watch_cell;
    host.enchant_held   = host_enchant_held;
    host.held_enchant   = host_held_enchant;
    host.wear_held      = host_wear_held;
    host.item_alias     = host_item_alias;
    host.player_regen   = host_player_regen;
    host.player_max_health = host_player_max_health;
    host.time_of_day    = host_time_of_day;
    host.set_time       = host_set_time;
    host.player_spawn   = host_player_spawn;
    host.node_climbable = host_node_climbable;
    host.node_conveyor = host_node_conveyor;
    host.container_roles = host_container_roles;
    host.node_keeps_drops = host_node_keeps_drops;
    host.container_accepts = host_container_accepts;
    host.container_progress = host_container_progress;
    host.container_scroll   = host_container_scroll;
    host.container_search   = host_container_search;
    host.node_door     = host_node_door;
    host.node_piece    = host_node_piece;
    host.place_piece   = host_place_piece;
    host.piece_entry   = host_piece_entry;
    host.take_piece    = host_take_piece;
    host.node_yield = host_node_yield;
    host.node_name     = host_node_name;
    host.event_define  = host_event_define;
    host.event_on      = host_event_on;
    host.event_emit    = host_event_emit;
    host.ud            = &scene;
    host.side          = host_side;
    host.player_name   = host_player_name;
    host.held_item     = host_held_item;
    host.worn_item     = host_worn_item;
    host.player_fly    = host_player_fly;
    host.body_width    = host_body_width;
    host.eye_height    = host_eye_height;
    host.parkour       = host_parkour;
    host.ledges        = host_ledges;
    return host;
}

// Whether this player has said no to a world's code (TODO.md OPEN-49): for
// every world, with --world-code refuse, or for this one, by the key it signs
// with, listed one per line in world_code_refused.txt in the media cache. A
// world with no key cannot be listed by it, and is refused only by the flag --
// an address is a route, and the same world under another is not the same line.
bool world_code_refused_for(const net::ClientSession &conn, std::string &why)
{
    if (g_world_code_refused) {
        why = "--world-code refuse";
        return true;
    }
    if (g_media_dir.empty() || g_media_dir == "-" || !conn.welcome().has_signing_key) return false;

    char hex[2 * net::kSignPublicBytes + 1] = {0};
    for (size_t i = 0; i < net::kSignPublicBytes; ++i)
        std::snprintf(hex + 2 * i, 3, "%02x", conn.welcome().signing_key[i]);

    const std::string list = g_media_dir + "/world_code_refused.txt";
    std::ifstream     in(list);
    std::string       line;
    while (std::getline(in, line)) {
        if (line == hex) {
            why = "this world is listed in " + list;
            return true;
        }
    }
    return false;
}

// Handing the identity to the next run, so going back to the title screen
// does not ask for the password again: the key and the name it was made from,
// down a pipe the new program inherits and reads once. Never a file or an
// argument, where somebody else could read it.
constexpr const char *kIdentityFdEnv = "CNP_IDENTITY_FD";

#ifdef _WIN32
// ponytail: no inherited pipe on Windows yet, so back to the title screen asks
// for the password again. Upgrade: an inheritable anonymous pipe handle.
void pass_identity(const TitleMenu &) {}
void take_identity(TitleMenu &) {}
#else
void pass_identity(const TitleMenu &m)
{
    int fd[2];
    if (!m.has_identity || ::pipe(fd) != 0) return;
    const std::string out = m.identity_for + '\n' +
                            std::string((const char *)&m.identity, sizeof(m.identity)) + m.player;
    if (::write(fd[1], out.data(), out.size()) == (ssize_t)out.size())
        ::setenv(kIdentityFdEnv, std::to_string(fd[0]).c_str(), 1);
    else
        ::close(fd[0]);
    ::close(fd[1]);
}

void take_identity(TitleMenu &m)
{
    const char *env = std::getenv(kIdentityFdEnv);
    if (env == nullptr) return;
    const int fd = std::atoi(env);
    ::unsetenv(kIdentityFdEnv);

    std::string in;
    char        buf[256];
    for (ssize_t n; (n = ::read(fd, buf, sizeof(buf))) > 0;) in.append(buf, (size_t)n);
    ::close(fd);
    const size_t at = in.find('\n') + 1;
    if (at == 0 || in.size() <= at + sizeof(m.identity)) return;

    m.identity_for = in.substr(0, at - 1);
    std::memcpy(&m.identity, in.data() + at, sizeof(m.identity));
    m.player       = in.substr(at + sizeof(m.identity));
    m.has_identity = true;
}
#endif

int client_main(int argc, char **argv)
{
    bool   bench  = false;
    bool   quick  = false;   // straight into the single-process world, no title screen

    // Bisecting a wrong portal image: turn one mechanism off at a time.
    uint32_t world_seed = 1;
    // How many portals deep the image goes. One means a portal seen through
    // another portal shows its own view; two is what DESIGN.md 8 asks M4 to
    // prove. Each level costs one full-size target per portal AND multiplies
    // the number of world passes, so the default is the cheapest one that
    // stops a portal looking like a hole.
    int    recursion   = 1;
    bool   use_oblique = true;
    bool   use_clip    = true;
    bool   verify = false;
    std::string model_path;
    bool   textures_off = false;

    // Where this process's mods come from. A gate points it at a fixture; a
    // player never passes it and gets the ones that ship.
    std::string mods_dir = data_dir("mods", CNP_SOURCE_ROOT "/mods");
    bool   facing = false;
    bool   dual_bench = false;
    bool   dual_scene = false;
    const char *portal_config = nullptr;
    bool   oriented_scene = false;
    double soak   = 0.0;

    // Empty means the world is generated here, which is what every mode before
    // this one did. A host means it comes from there instead.
    const char *connect_host  = nullptr;
    uint16_t    connect_port  = 25301;
    std::string connect_room;            // --room: which of the server's worlds
    const char *player_name   = "player";
    double      play_seconds  = 0.0;   // above zero: play for a while, then check
    const char *server_key    = nullptr;   // the server key this player insists on
    const char *player_key    = nullptr;   // where this player's own identity lives

    // Other worlds to look into, one connection each. A list and not a second
    // host: there is nothing about a portal that stops at two worlds.
    std::vector<std::string> watch;
    bool        encrypt       = true;

    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--bench") == 0)  bench = true;
        if (std::strcmp(argv[i], "--quick") == 0)  quick = true;
        if (std::strcmp(argv[i], "--classic-light") == 0) g_classic_light = true;
        if (std::strcmp(argv[i], "--no-oblique") == 0) use_oblique = false;
        if (std::strcmp(argv[i], "--no-clip") == 0)    use_clip = false;
        if (std::strcmp(argv[i], "--verify-shots") == 0) g_verify_shots = true;
        if (std::strcmp(argv[i], "--verify") == 0) verify = true;
        if (std::strcmp(argv[i], "--model") == 0 && i + 1 < argc) model_path = argv[++i];
        if (std::strcmp(argv[i], "--mods") == 0 && i + 1 < argc) mods_dir = argv[++i];
        if (std::strcmp(argv[i], "--facing") == 0) facing = true;
        if (std::strcmp(argv[i], "--dual-bench") == 0) dual_bench = true;
        if (std::strcmp(argv[i], "--dual") == 0) dual_scene = true;
        if (std::strcmp(argv[i], "--oriented") == 0) oriented_scene = true;
        if (std::strcmp(argv[i], "--portal-scale") == 0 && i + 1 < argc)
            g_portal_scale = std::clamp((float)std::atof(argv[++i]), 0.1f, 1.0f);
        if (std::strcmp(argv[i], "--log-view") == 0) {
            g_view_trace.on = true;
            std::printf("view trace on: %.1f seconds of lines after every crossing and the\n"
                        "frames before it, sampled every %.0f ms. Also, wherever it happens\n"
                        "and whether or not a crossing is anywhere near, EVERY frame whose\n"
                        "look direction moves more than %.1f degree nobody asked for.\n"
                        "  turn      degrees the look direction moved since the frame before\n"
                        "  asked     degrees the mouse asked it to move\n"
                        "  unexplned degrees it moved that nothing asked for -- the defect\n"
                        "  clamp     degrees the pitch limit took off the top\n"
                        "  horizon   degrees the horizon rolled; half a turn a second is the\n"
                        "            crossing animation and not a fault\n"
                        "  crate     the sliding crate's distance from the opening's plane,\n"
                        "            along its normal; CUT while it is in two worlds at once\n",
                        (double)kViewTraceSeconds, (double)(kViewTraceSampleSeconds * 1000.0f),
                        (double)kViewSnapDegrees);
        }
        if (std::strcmp(argv[i], "--recursion") == 0 && i + 1 < argc) {
            recursion = std::max(0, std::min(3, atoi(argv[++i])));
            g_recursion_depth = recursion;
        }
        if (std::strcmp(argv[i], "--soak") == 0 && i + 1 < argc)
            soak = atof(argv[++i]);
        if (std::strcmp(argv[i], "--seed") == 0 && i + 1 < argc)
            world_seed = (uint32_t)std::strtoul(argv[++i], nullptr, 10);
        if (std::strcmp(argv[i], "--world") == 0 && i + 1 < argc)
            g_world_extent = std::max(64, atoi(argv[++i]));
        if (std::strcmp(argv[i], "--day-length") == 0 && i + 1 < argc) {
            const double d = atof(argv[++i]);
            if (d > 0.0) g_day_length = d;
        }
        if (std::strcmp(argv[i], "--time") == 0 && i + 1 < argc)
            g_start_time = clock_wrap(atof(argv[++i]));
        if (std::strcmp(argv[i], "--room") == 0 && i + 1 < argc)
            connect_room = argv[++i];
        if (std::strcmp(argv[i], "--connect") == 0 && i + 1 < argc)
            connect_host = argv[++i];
        if (std::strcmp(argv[i], "--port") == 0 && i + 1 < argc)
            connect_port = (uint16_t)std::strtoul(argv[++i], nullptr, 10);
        if (std::strcmp(argv[i], "--name") == 0 && i + 1 < argc)
            player_name = argv[++i], g_self_name = player_name;
        if (std::strcmp(argv[i], "--play-seconds") == 0 && i + 1 < argc)
            play_seconds = atof(argv[++i]);
        if (std::strcmp(argv[i], "--server-key") == 0 && i + 1 < argc)
            server_key = argv[++i];
        if (std::strcmp(argv[i], "--player-key") == 0 && i + 1 < argc)
            player_key = argv[++i];
        if (std::strcmp(argv[i], "--watch") == 0 && i + 1 < argc)
            watch.push_back(argv[++i]);
        if (std::strcmp(argv[i], "--no-passive") == 0)
            g_no_passive = true;
        if (std::strcmp(argv[i], "--roam") == 0)
            g_roam = true;
        if (std::strcmp(argv[i], "--soak-far") == 0 && i + 1 < argc)
            g_soak_far = std::atof(argv[++i]);
        if (std::strcmp(argv[i], "--cross-back") == 0 && i + 1 < argc)
            g_cross_back = std::atof(argv[++i]);
        // "none" is an empty list: a run that stands where --walk-path left it
        // and walks into nothing, to look at the mouths rather than through them.
        if (std::strcmp(argv[i], "--mouths") == 0 && i + 1 < argc) {
            g_walk_mouths.clear();
            for (const char *p = argv[++i]; *p != '\0'; ++p)
                if (*p >= '0' && *p < '0' + net::kMaxMouths) g_walk_mouths.push_back(*p - '0');
        }
        if (std::strcmp(argv[i], "--use-at") == 0 && i + 1 < argc)
            g_use_at = std::sscanf(argv[++i], "%d,%d,%d", &g_use_x, &g_use_y, &g_use_z) == 3;
        if (std::strcmp(argv[i], "--first-person") == 0) {
            g_first_person = true;
            continue;
        }
        if (std::strcmp(argv[i], "--walk-boost") == 0) {
            g_walk_boost = true;
            continue;
        }
        if (std::strcmp(argv[i], "--walk-path") == 0 && i + 1 < argc) {
            g_walk_path.clear();
            for (const char *p = argv[++i]; *p != '\0';) {
                float x = 0.0f, z = 0.0f;
                int   used = 0;
                if (std::sscanf(p, "%f,%f%n", &x, &z, &used) != 2) break;
                g_walk_path.push_back(Vec3{x, 0.0f, z});
                p += used;
                if (*p == ';') ++p;
            }
        }
        // Given more than once it is a list, and the list is what F7 walks:
        // "selectable" means a player can put a pack on, look, and take it off,
        // which is not something a startup flag alone can do.
        if (std::strcmp(argv[i], "--fly-down") == 0 && i + 1 < argc)
            g_fly_down = (float)atof(argv[++i]);
        if (std::strcmp(argv[i], "--walked-down") == 0) g_fly_flown = false;
        if (std::strcmp(argv[i], "--shot") == 0 && i + 1 < argc)
            g_shot_at = atof(argv[++i]);
        // The look picked on the title screen, for a join that skips it.
        if (std::strcmp(argv[i], "--livery") == 0 && i + 1 < argc)
            net::g_client_livery = (uint32_t)std::strtoul(argv[++i], nullptr, 10) % net::kLiveryMax;
        if (std::strcmp(argv[i], "--press-r-at") == 0 && i + 1 < argc)
            g_press_r_at = atof(argv[++i]);
        if (std::strcmp(argv[i], "--textures") == 0 && i + 1 < argc)
            g_pack_dirs.push_back(argv[++i]);
        if (std::strcmp(argv[i], "--no-textures") == 0) textures_off = true;
        if (std::strcmp(argv[i], "--models") == 0 && i + 1 < argc) g_models_dir = argv[++i];
        if (std::strcmp(argv[i], "--no-models") == 0) g_models_dir.clear();
        if (std::strcmp(argv[i], "--no-update-check") == 0) g_no_update_flag = true;
        if (std::strcmp(argv[i], "--media-cache") == 0 && i + 1 < argc)
            g_media_dir = argv[++i];
        if (std::strcmp(argv[i], "--no-media-cache") == 0)
            g_media_dir = "-";
        if (std::strcmp(argv[i], "--world-code") == 0 && i + 1 < argc)
            g_world_code_refused = std::strcmp(argv[++i], "refuse") == 0;
        if (std::strcmp(argv[i], "--media-cache-size") == 0 && i + 1 < argc)
            g_media_budget = (size_t)std::strtoull(argv[++i], nullptr, 10) * 1024u * 1024u;
        if (std::strcmp(argv[i], "--watchdog") == 0 && i + 1 < argc)
            g_watchdog_seconds = std::atof(argv[++i]);
        if (std::strcmp(argv[i], "--far-world-full") == 0)
            g_far_world_full = true;
        if (std::strcmp(argv[i], "--different-games") == 0)
            g_different_games = true;
        if (std::strcmp(argv[i], "--no-encryption") == 0)
            encrypt = false;
        if (std::strcmp(argv[i], "--portal-config") == 0 && i + 1 < argc)
            portal_config = argv[++i];
    }

    // Which pack the world wears, before anything builds an atlas out of it.
    //
    // Nothing said means the folder this build ships, which is a pack like any
    // other -- it just has no manifest and needs none. --no-textures means no
    // pack at all, which is not the same as an empty one: it is the state where
    // every block is the generated placeholder, and it is what the pack gate
    // measures everything else against.
    if (textures_off) {
        g_pack_dirs.clear();
        g_pack         = TexturePack{};
        g_textures_dir.clear();
    } else {
        if (g_pack_dirs.empty()) g_pack_dirs.push_back(g_textures_dir);
        select_pack(0);
    }

    // The shape of a portal, before anything builds one. A bad config is fatal
    // rather than a warning: every gate below measures a shape, and measuring
    // the built-in defaults while the file says something else is the kind of
    // green that costs a day.
    {
        std::string err;
        bool        ok = false;
        if (portal_config) {
            bool found = false;
            ok = portal_tuning_load(portal_config, &found, err);
            if (ok && !found) {
                err = std::string(portal_config) + ": no such file";
                ok  = false;
            }
        } else {
            ok = portal_tuning_load_default(err);
        }
        if (!ok) {
            std::fprintf(stderr, "portal config: %s\n", err.c_str());
            return 1;
        }
    }

    // The continuity gate compares what a portal SHOWS against the plain world
    // seen from the virtual camera, and a reference frame with no portals in it
    // cannot contain the other portal's opening. With the recursion on, the
    // portal's image legitimately does contain it, and the comparison fails at
    // ninety per cent of pixels -- correctly, against the wrong question.
    //
    // So --verify measures the transform at depth zero, where the recursion is
    // by construction the two render_target calls it replaced. Whether the
    // recursion puts an image inside that opening is a different property with
    // its own gate.
    if (verify) recursion = 0;

    // The modes that measure something are single player by construction: a
    // frame rate measured against a server is a measurement of the server's
    // upload, and a continuity reference needs a world that is all here.
    if (connect_host && (bench || verify || dual_bench || soak > 0.0)) {
        std::fprintf(stderr,
                     "--connect cannot be combined with --bench, --dual-bench, --verify or "
                     "--soak\n");
        return 1;
    }

    // --verify derives its viewpoints by hand for the default pair, so pointing
    // it at another scene measures nothing.
    if ((facing || oriented_scene || dual_scene) && verify) {
        std::fprintf(stderr,
                     "--facing, --oriented and --dual are scenes to look at, not inputs to "
                     "--verify.\n");
        return 1;
    }

    // A stack out of a process that has stopped answering. Costs nothing while
    // nothing is wrong: SIGQUIT prints one and carries on, and the watchdog is
    // off unless somebody asked for it. TODO.md OPEN-6.
    install_stack_dumper();
    install_crash_handler();
    if (g_watchdog_seconds > 0.0) {
        watchdog_start(g_watchdog_seconds);
        std::printf("watchdog: %.0f s without a frame and this process writes its stack and "
                    "gives up\n", g_watchdog_seconds);
    }

    // A window somebody can resize, unless this run is a measurement.
    //
    // Every gate here renders into a target of its own at a size it chose, so a
    // window of another shape does not change what they measure -- but the
    // frame-rate ones are measured against the SCREEN, and a smaller window
    // makes them cheaper. So a run that is measuring anything asks for the shape
    // it intends to measure and is not allowed to be dragged into another one.
    const bool window_is_measured =
        bench || dual_bench || verify || soak > 0.0 || play_seconds > 0.0;

    gfx_desc desc{};
    desc.width     = kWindowWidth;
    desc.height    = kWindowHeight;
    desc.title     = "Cubes and Portals";
    desc.vsync     = false;   // criterion 3 measures frame cost, not refresh rate
    desc.msaa      = 0;
    desc.resizable = !window_is_measured;
    g_window_measured = window_is_measured;

    if (!gfx_init(&desc)) {
        std::fprintf(stderr, "failed to create window\n");
        return 1;
    }
    plat_disable_default_exit_key();


    // What the window actually IS, which is not always what was asked for: a
    // resizable one is shrunk to fit the monitor it opened on, because asking
    // for the monitor's own resolution puts the bottom of the game behind the
    // desktop's panel. Printed because a window that is not the size the flags
    // implied is the first thing to check when a measurement moves.
    std::printf("window: %dx%d%s\n", gfx_screen_width(), gfx_screen_height(),
                desc.resizable ? ", resizable" : ", fixed for measurement");

    Scene scene;

    // Sound is for the loops a person plays, and not for the ones that measure.
    const bool wants_sound = !window_is_measured;
    struct AudioUp {
        ~AudioUp() { snd_shutdown(); }
    } audio_up;
    if (wants_sound && snd_init()) {
        scene.sounds.mods_dir = mods_dir;
        scene.sounds.load();
        std::printf("audio: %d sound(s)\n", snd_loaded());
    }


    // The title screen, unless this run already says where it goes: --quick
    // for the single-process world, --connect, a scene or a measurement. What
    // it chooses is joined like any server. Declared before the sessions so
    // it outlives them: a world's server is stopped once its player has left.
    LocalServer local_server;
    TitleMenu   menu;
    if (const char *again = std::getenv("CNP_REJOIN"); again && *again) {
        menu.rejoin = again;
#ifdef _WIN32
        _putenv_s("CNP_REJOIN", "");
#else
        ::unsetenv("CNP_REJOIN");
#endif
    }
    if (const char *why = std::getenv("CNP_TITLE_ERROR"); why && *why) {
        menu.error = why;
#ifdef _WIN32
        _putenv_s("CNP_TITLE_ERROR", "");
#else
        ::unsetenv("CNP_TITLE_ERROR");
#endif
    }
    bool        menu_debug = false;
    uint16_t    menu_port  = 0;
    std::string local_name, local_server_key, local_host;

    g_robot_dir = mods_dir + "/robot";   // the look page's preview on the title screen
    const bool menu_up = !quick && !connect_host && !window_is_measured && !facing &&
                         !dual_scene && !oriented_scene;
    // A join straight from the command line (tools/track.sh) skips the menu, and
    // its hello still says what the player picked to look like.
    if (!menu_up && !window_is_measured && net::g_client_livery == 0) {
        bool unused = false;
        load_options(unused);
    }
    if (menu_up) {
        // Where this player's identity, their worlds and their list of servers
        // live. Made here, because the first thing chosen may be a server, and
        // a player.key that cannot be written stops the client.
        std::error_code data_ec;
        std::filesystem::create_directories(user_data_dir(), data_ec);

        load_options(menu_debug);
        if (g_update_check && !g_no_update_flag) update::start();
        menu.player = saved_player_name();
        take_identity(menu);
        g_from_title = true;
        if (!title_choose(scene, menu, menu_debug, local_server, local_host, menu_port,
                          local_server_key)) {
            gfx_shutdown();
            return 0;
        }

        local_name       = menu.player;
        connect_host     = local_host.c_str();
        connect_port     = menu_port;
        player_name      = local_name.c_str();

        // Only a world this client started is pinned: its key is the one that
        // server just printed. Somebody else's is trusted the way --connect
        // trusts it, and the client says so at the join.
        server_key = local_server_key.empty() ? nullptr : local_server_key.c_str();
    }

    // Every connection this process holds. One when a player joins a server,
    // one more for each world they are looking into. The player's own is named
    // by an id like any other -- there is no privileged slot, because the day
    // there is, "the connection" becomes a singleton again.
    net::ClientSessionSet net_sessions;
    int                   self_session = net::ClientSessionSet::kNoSession;

    // What the player's own join was made with. Kept because a world behind a
    // mouth is joined with the same identity and the same encryption: a portal
    // that quietly dropped to a weaker connection would be a portal that
    // teaches players not to look at the padlock.
    net::ClientConfig     net_cfg;

    const bool networked = connect_host != nullptr;

    // Both openings sit one node clear of the floor, and that is a finding
    // rather than a preference.
    //
    // With the bottom edge at floor level the two are exactly coplanar, and the
    // sight line grazing that edge maps to a ray leaving the destination's
    // inner face travelling exactly along the floor plane. Nothing decides
    // whether it hits the ground or skims over it except rounding, so the lower
    // part of the opening fills with clear colour. It is a degenerate grazing
    // case, not a fault in the projection -- raising the pair by one node makes
    // the portal image match the reference view. M4 owns the real fix; the fork
    // this replaces carries floor_portal_cb_shift and a separate horizontal
    // portal path for related reasons. See docs/archive/M0_FINDINGS.md.
    PortalSurface a;
    a.center = Vec3{40.5f, 22.0f, 40.5f};
    a.normal = Vec3{0.0f, 0.0f, 1.0f};
    a.up     = Vec3{0.0f, 1.0f, 0.0f};
    a.half_w = 0.5f;
    a.half_h = 1.0f;

    PortalSurface b;
    b.center = Vec3{79.5f, 22.0f, 60.5f};
    b.normal = Vec3{1.0f, 0.0f, 0.0f};
    b.up     = Vec3{0.0f, 1.0f, 0.0f};
    b.half_w = 0.5f;
    b.half_h = 1.0f;

    // --facing: a corridor with a portal at each end, looking into each other.
    //
    // The spike pair above cannot show recursion at all -- they face different
    // ways, so neither appears in the other's image and every extra level of
    // depth renders the same picture. The recursion gate has always built its
    // own pair for that reason, inside the gate where nobody can stand and
    // look. This is that pair, in the world, walkable.
    //
    // Eight nodes apart with the spawn exactly between them, so the first thing
    // on screen at startup is one of the two. Eight rather than eleven because
    // what matters is how big the far opening is inside the near one, and that
    // falls off with the square of the gap. Both openings are FLUSH
    // WITH THE FLOOR, which the spike pair is not: that was a defect until
    // M4_FINDINGS 7 and the workaround was to raise them a node, which is why
    // the spike ones have to be jumped into. This pair is walked into, and it
    // is the demonstration that the workaround is no longer needed.
    if (facing) {
        a.center = Vec3{60.5f, (float)kGroundTop + 1.0f, 64.5f};
        a.normal = Vec3{1.0f, 0.0f, 0.0f};        // faces B

        b.center = Vec3{68.5f, (float)kGroundTop + 1.0f, 64.5f};
        b.normal = Vec3{-1.0f, 0.0f, 0.0f};       // faces A
    }

    // --oriented: an ordinary opening you walk into, and one lying on its side
    // you come out of. The pair the orientation gate measures, in the world,
    // walkable -- because a quarter turn of roll settling over half a second is
    // a thing to WATCH, and a percentage in a log does not say whether it
    // looks like a landing or like a glitch.
    //
    // B's centre is placed by its own basis, not by eye. Its right is (0,-1,0)
    // and the opening is one node across that way, so the y sits at a node
    // centre; its up is (0,0,-1) and the opening is two nodes along it, so the
    // z sits on a node boundary. Get those the other way round and the hole is
    // half in the slab. The result is a hole two nodes wide and one tall, which
    // is exactly the shape a standing player cannot walk through -- and does
    // not have to, because they arrive through it rather than entering it.
    if (oriented_scene) {
        a.center = Vec3{40.5f, (float)kGroundTop + 1.0f, 44.5f};
        a.normal = Vec3{0.0f, 0.0f, 1.0f};
        a.up     = Vec3{0.0f, 1.0f, 0.0f};

        b.center = Vec3{60.5f, (float)kGroundTop + 1.5f, 60.0f};
        b.normal = Vec3{1.0f, 0.0f, 0.0f};
        b.up     = Vec3{0.0f, 0.0f, -1.0f};
    }

    if (networked) {
        // The world is somebody else's. Nothing is generated here, the block
        // table is adopted rather than registered, and the join has to finish
        // before the renderers are built: the atlas is made from the registry,
        // and the registry does not exist until the welcome lands.
        net::ClientConfig &cfg = net_cfg;
        cfg.host    = connect_host;
        cfg.port    = connect_port;
        cfg.room    = connect_room;
        cfg.encrypt = encrypt;

        // The cache every session on this client shares. Opened once, here,
        // because a per-session one would satisfy every sentence of DESIGN.md
        // 4.5 except the one it is for.
        if (g_media_dir == "-") {
            std::printf("media: no cache, every world's files are downloaded every time\n");
        } else {
            if (g_media_dir.empty()) g_media_dir = default_media_dir();
            std::string cache_err;
            if (!g_media_cache.open(g_media_dir, cache_err, g_media_budget)) {
                // Not fatal: a client with no cache works and is slow, and
                // saying so beats a portal that silently never draws anything.
                std::printf("media: no cache (%s): every world's files are downloaded "
                            "every time\n", cache_err.c_str());
            } else {
                cfg.media_cache = &g_media_cache;
                std::printf("media: cache at %s -- %zu file(s), %.1f MB of %.0f MB, "
                            "counted in %.2f ms%s\n",
                            g_media_dir.c_str(), g_media_cache.files_held(),
                            (double)g_media_cache.bytes_held() / (1024.0 * 1024.0),
                            (double)g_media_cache.budget() / (1024.0 * 1024.0),
                            g_media_cache.scan_seconds() * 1e3,
                            g_media_cache.evicted_files() > 0
                                ? " (over budget on arrival, oldest thrown away)" : "");
            }
        }

        if (server_key) {
            if (!net::from_hex(server_key, cfg.server_key, net::kKeyBytes)) {
                std::fprintf(stderr, "--server-key wants the 64 hex characters the server "
                                     "printed at startup\n");
                gfx_shutdown();
                return 1;
            }
            cfg.pin_server = true;
        }

        // Who the player is. From the title screen that is the name and the
        // password typed into it, which ARE the key (sign_key_from_password),
        // so nothing is kept on disk and the same two words are the same
        // player on any machine.
        if (menu_up) {
            cfg.identity     = menu.identity;
            cfg.has_identity = true;
        } else

        // DESIGN.md 4.4: the identity is the key and the name is a label, so
        // this is generated once and kept, while the name stays whatever they
        // typed today.
        //
        // Created on first use rather than asked for. A game that makes a
        // person run a keygen before they can play is a game they do not play,
        // and there is nothing to choose here anyway -- the file either exists
        // or it is 32 random bytes.
        {
            const std::string path = player_key ? std::string(player_key) : "player.key";
            bool              created = false;
            std::string       key_err;
            if (!net::load_or_create_sign_key(path, cfg.identity, created, key_err)) {
                std::fprintf(stderr, "player identity: %s\n", key_err.c_str());
                gfx_shutdown();
                return 1;
            }
            if (!key_err.empty()) std::printf("player identity: %s\n", key_err.c_str());
            cfg.has_identity = true;

            std::printf("playing as '%s', identity %s (%s %s)\n", player_name,
                        net::key_fingerprint(cfg.identity.public_key).c_str(),
                        created ? "created in" : "read from", path.c_str());
        }

        // Joined here, and tried again from the title screen when it does not
        // work: a server that has moved, or is not up, sends the player back to
        // the list of servers. A run told where to go on the command line has
        // no list to go back to, so for that one a failure is still the end.
        std::string     err;
        net::ClientState why = net::ClientState::Idle;
        for (;;) {
            cfg.host = connect_host;
            cfg.port = connect_port;
            if (menu_up)
                g_rejoin_account = menu.identity_for != kLocalAccount && menu.identity_for != kHostAccount
                                       ? menu.identity_for
                                       : "";

            self_session = net_sessions.open(cfg, player_name, plat_time(), err);
            if (self_session != net::ClientSessionSet::kNoSession) {
                // The world is somebody else's. Nothing is generated here, the
                // block table is adopted rather than registered, and the join
                // has to finish before the renderers are built: the atlas is
                // made from the registry, and the registry does not exist
                // until the welcome lands.
                net::ClientSession &opened = *net_sessions.get(self_session);
                opened.use(scene.act().world, scene.act().registry, scene.act().movement);

                std::printf("connecting to %s:%u as '%s'\n", connect_host,
                            (unsigned)connect_port, player_name);

                while (!net::client_join_settled(opened.state()))
                    net_sessions.pump(plat_time(), 10);

                if (opened.state() == net::ClientState::Playing) break;

                why = opened.state();
                err = std::string(net::client_state_text(why)) +
                      (opened.error().empty() ? "" : " -- " + opened.error());
                net_sessions.close(self_session);
            }

            std::fprintf(stderr, "could not join %s:%u: %s\n", connect_host,
                         (unsigned)connect_port, err.c_str());
            if (!menu_up) {
                gfx_shutdown();
                // The same codes the probe uses, for the same reason: "it did
                // not work" is not enough for a script to check anything.
                return why == net::ClientState::Refused  ? 2
                       : why == net::ClientState::Failed ? 3
                                                         : 1;
            }

            menu.error = err.find("not the one that was pinned") == std::string::npos
                             ? err
                             : "its key changed: x it and add it again to trust it";
            // Asked again on the next try: a server that refused the name
            // wants another account, or the same one typed right.
            menu.identity_for.clear();
            menu.page  = MenuPage::Server;
            if (!title_choose(scene, menu, menu_debug, local_server, local_host, menu_port,
                              local_server_key)) {
                gfx_shutdown();
                return 0;
            }
            connect_host   = local_host.c_str();
            connect_port   = menu_port;
            cfg.pin_server = false;
            if (!local_server_key.empty() &&
                net::from_hex(local_server_key.c_str(), cfg.server_key, net::kKeyBytes))
                cfg.pin_server = true;
        }

        net::ClientSession &session = *net_sessions.get(self_session);

        // Who this client is on that server, kept beside its address: the
        // server has the identity, this end has the name that goes with it.
        // And the key it showed the first time, pinned on every join after:
        // trust on first use, as ssh does, so a server that is later somebody
        // else is refused rather than joined.
        if (menu_up && menu.chose_row < menu.servers.size()) {
            ServerRow &row = menu.servers[menu.chose_row];
            if (row.player != menu.player || row.key.empty()) {
                row.player = menu.player;
                if (row.key.empty()) row.key = session.server_key_hex();   // empty unencrypted
                save_servers(menu.servers);
            }
        }

        const net::Welcome &w = session.welcome();
        std::printf("joined: server '%s', protocol %u, seed %u, %zu blocks (%zu unknown here)\n",
                    w.engine_version.c_str(), (unsigned)w.protocol, w.seed, w.nodes.size(),
                    session.unknown_blocks());
        // What the join cost, against what it was allowed. The number the
        // window has to be chosen against, and nothing printed it before.
        std::printf("        took %.1f s of the %.1f s allowed (round trip %.0f ms)\n",
                    session.joined_in(), session.handshake_window(),
                    session.round_trip() * 1e3);

        // Said out loud, every time. A player who does not know whether their
        // session is authenticated will assume it is.
        if (!encrypt)
            std::printf("        UNENCRYPTED -- anybody on the path can read and change this\n");
        else if (session.authenticated())
            std::printf("        encrypted, server key pinned and matched\n");
        else
            std::printf("        encrypted, server key %s\n"
                        "        NOT pinned: safe from listening, not from interception. Pass\n"
                        "        --server-key <that key> to insist on this server.\n",
                        session.server_key_hex().c_str());

        // The ids are the server's now. Looking them up by name is the only way
        // that stays true when the server runs mods this build has never seen.
        kStone   = scene.act().registry.find("core:stone");
        kGrass   = scene.act().registry.find("core:grass");
        kBedrock = scene.act().registry.find("core:bedrock");
        kFrame   = scene.act().registry.find("core:frame");
        kGlass   = scene.act().registry.find("core:glass");

        scene.act().portals = false;

        // And any world this client was told to look into. One connection and
        // one world each, as many as were asked for: this is the shape a portal
        // onto another server needs, built before there is a portal to hang it
        // on, and it is measurable now rather than when the renderer catches up.
        //
        // They join as observers, which is what DESIGN.md 4.4 says a passive
        // session is: chunks and the people in that world, no avatar, nothing
        // they can touch. Nothing is drawn from them yet -- said plainly rather
        // than implied by a black rectangle.
        for (const std::string &spec : watch) {
            const size_t colon = spec.rfind(':');
            if (colon == std::string::npos) {
                std::fprintf(stderr, "--watch wants host:port, got '%s'\n", spec.c_str());
                gfx_shutdown();
                return 1;
            }

            net::ClientConfig wcfg = cfg;
            wcfg.host       = spec.substr(0, colon);
            wcfg.port       = (uint16_t)std::strtoul(spec.c_str() + colon + 1, nullptr, 10);
            wcfg.scope      = net::Scope::Observe;
            wcfg.pin_server = false;   // a different server, so not this server's key

            const int world = scene.add();
            std::string werr;
            const int   id = net_sessions.open(wcfg, player_name, plat_time(), werr);
            if (id == net::ClientSessionSet::kNoSession) {
                std::fprintf(stderr, "could not look into %s: %s\n", spec.c_str(), werr.c_str());
                gfx_shutdown();
                return 1;
            }

            net::ClientSession &watcher = *net_sessions.get(id);
            watcher.use(scene.at(world).world, scene.at(world).registry, scene.at(world).movement);

            while (!net::client_join_settled(watcher.state()))
                net_sessions.pump(plat_time(), 10);

            std::printf("looking into %s as world %d: %s%s%s\n", spec.c_str(), world,
                        net::client_state_text(watcher.state()),
                        watcher.error().empty() ? "" : " -- ", watcher.error().c_str());
        }
    } else if (!dual_scene) {
        // --dual builds its own two worlds further down. Skipped here rather
        // than built and overwritten: this path registers the block table and
        // generates terrain into session 0, and build_local_session would then
        // register the same names into the same registry a second time.
        register_client_core_nodes(scene.act().registry);

        // Terrain comes from the same generator the server runs -- the client no
        // longer has an opinion about what the ground looks like, which is the
        // only way the two can agree once there is a network between them. The
        // scene this client needs on top of it is registered as two more stages,
        // in the order they must run.
        {
            // Kept on the session: the world has no edge, and a column the
            // player reaches an hour from now is made by these same stages.
            scene.act().mapgen.reset(new Mapgen());
            Mapgen &mapgen = *scene.act().mapgen;
            mapgen.add_stage("client:portal_walls", [&](MapgenContext &ctx) {
                // Different materials for a facing pair, and that is the
                // whole device for reading depth off the screen. Looking into
                // A shows the corridor from behind B, so the wall at the far
                // end of that image is A's; inside ITS opening the far wall is
                // B's again. The colours alternate with the level, so counting
                // levels is counting stripes rather than guessing.
                build_slab_with_opening(ctx,a, 3, 3, kFrame);
                build_slab_with_opening(ctx,b, 3, 3, facing ? kStone : kFrame);

                if (!oriented_scene) return;

                // Somewhere to land, and the reason is the limitation this
                // scene exists to show rather than an oversight.
                //
                // The first run of --oriented refused the crossing outright and
                // said so: "the destination is blocked, not crossing". B is
                // lying on its side, so its hole is two nodes wide and ONE
                // tall, and the arriving body is 1.8 tall -- this engine rolls
                // the camera, not the body, so there is no orientation that
                // fits. Worse, the exit is 0.75 past the plane and the body is
                // about 0.3 wide, so five centimetres of it are still inside
                // the slab's own node; those five centimetres are air only when
                // the body lines up with the aperture, and here it cannot.
                //
                // So the hole is opened one node downward, to the height a
                // standing player actually occupies, and the ground in front of
                // it is cleared. The seam this leaves in the wall is honest:
                // a sideways portal is not a doorway.
                for (int z = 59; z <= 60; ++z) ctx.set_block(60, 20, z, kAir);
                for (int x = 61; x <= 63; ++x)
                    for (int y = 20; y <= 23; ++y)
                        for (int z = 58; z <= 61; ++z) ctx.set_block(x, y, z, kAir);
            });
            mapgen.add_stage("client:landmarks", [&](MapgenContext &ctx) {
                // A tall stone spike behind A, a squat frame-coloured block
                // behind B. Telling the two apart is how continuity gets judged
                // by eye.
                build_landmark(ctx,36, kGroundTop, 36, kStone, 7);
                build_landmark(ctx,84, kGroundTop, 64, kFrame, 4);

                if (!facing) return;

                // Nothing else in the corridor. A marker down the middle was
                // tried and it is the wrong idea twice over: on the sight line
                // it blocks the very thing being looked at, and off the sight
                // line it is outside the narrow cone that reaches through a
                // one-node opening, so it only ever shows when walking past.
                // The two wall colours do the counting instead, and they are
                // the one thing that cannot get in the way.
            });

            // The box around the spawn and a ring of columns round it, now,
            // before the mods load -- as the server does it, and for its two
            // reasons: a mod builds its scene into this while it loads, and
            // the box's light reaches into the ring. It is lit once the mods'
            // stages have run, below. Past the ring the world is made as the
            // player goes (TODO.md OPEN-35).
            Session     &sess    = scene.act();
            const int    columns = columns_across(g_world_extent);
            const double t_gen   = plat_time();
            sess.keeper.reset(new ColumnKeeper(sess.world, sess.registry, kWorldHeight));
            for (int cz = -1; cz <= columns; ++cz) {
                for (int cx = -1; cx <= columns; ++cx) {
                    mapgen.generate_column(sess.world, sess.registry, world_seed,
                                           ColumnPos{cx, cz}, kWorldHeight);
                    sess.keeper->mark_generated(ColumnPos{cx, cz}, 0);
                }
            }
            std::printf("world: no edge; %d x %d x %d around the spawn made now, %zu chunks, "
                        "%.1f MB, in %.2f s\n",
                        g_world_extent, kWorldHeight, g_world_extent, sess.world.chunk_count(),
                        (double)sess.world.memory_bytes() / (1024.0 * 1024.0),
                        plat_time() - t_gen);

            // What a column coming in, lit or waited for does in this process:
            // the same stages, the loading modifiers, and a write through the
            // host functions a mod calls. Nothing to tell when one leaves -- the
            // renderer lets go of the meshes of chunks the world has let go of.
            Session *here = &sess;
            Scene   *sp   = &scene;
            sess.keeper->set_hooks(ColumnKeeper::Hooks{
                [here](ColumnPos c) {
                    here->mapgen->generate_column(here->world, here->registry,
                                                  here->mapgen_ctx->seed, c, kWorldHeight);
                    run_mod_stages(*here, c);
                },
                [here](const std::vector<ChunkPos> &chunks) {
                    if (!here->modifiers.registration_closed() || here->modifiers.lbms().empty())
                        return;
                    for (const ChunkPos p : chunks) here->modifiers.chunk_loaded(p);
                },
                {},
                [sp](const PendingWrite &w) {
                    if (w.facing_only) host_set_facing(sp, w.x, w.y, w.z, w.facing);
                    else host_set_block(sp, w.x, w.y, w.z, (int)w.id);
                },
            });

            // Kept for the mods, which load later. See Session::mapgen_biomes
            // for why the biomes are copied.
            scene.act().mapgen_biomes = mapgen.biomes();
            scene.act().mapgen_ctx.reset(new MapgenContext(mapgen_make_context(
                scene.act().world, scene.act().registry, scene.act().mapgen_biomes, world_seed,
                kWorldHeight)));
        }
        scene.act().world.mark_all_dirty();
    }

    scene.portal_render.set_debug(use_oblique, use_clip);

    // --dual: the M5 scene, walkable. Two local worlds, one opening each, and
    // the passive one rendered inside the active one's portal.
    //
    // Built here instead of the single-world path above rather than on top of
    // it: that path generates terrain, registers blocks and creates targets for
    // ONE world, and running it first would leave the second session holding a
    // half-built copy of the first.
    if (dual_scene) {
        // Noon and dusk, for the same reason as in run_dual: the two skies are
        // the two hours, and crossing is supposed to change the time of day.
        const double time_a = kNoon;
        const double time_b = kDusk;

        PortalSurface oa;
        oa.center = Vec3{40.5f, (float)kGroundTop + 1.0f, 44.5f};
        oa.normal = Vec3{0.0f, 0.0f, 1.0f};
        oa.up     = Vec3{0.0f, 1.0f, 0.0f};

        PortalSurface ob;
        ob.center = Vec3{70.5f, (float)kGroundTop + 1.0f, 24.5f};
        ob.normal = Vec3{1.0f, 0.0f, 0.0f};
        ob.up     = Vec3{0.0f, 1.0f, 0.0f};

        BlockId fa = 0, fb = 0;
        while (scene.count() < 2) scene.add();   // the scene starts with one world
        build_local_session(scene.at(0), 1, world_seed, oa, ob, time_a, kActiveViewRange, 3, fa);
        build_local_session(scene.at(1), 0, world_seed + 6u, ob, oa, time_b, kPassiveViewRange,
                            8, fb);
        scene.active = 0;
        scene.at(0).draws_own_avatar = true;

        if (!scene.at(0).world_render.init(scene.at(0).registry, read_texture_file) ||
            !scene.at(1).world_render.init(scene.at(1).registry, read_texture_file) ||
            !scene.portal_render.init() || !scene.selection.init() ||
            !scene.body_render.init()) {
            gfx_shutdown();
            return 1;
        }
        wire_model_draw(scene, 0);
        wire_model_draw(scene, 1);
        if (!scene.portal_render.init_recursion(2, recursion, portal_rt_dim(kWindowWidth),
                                                portal_rt_dim(kWindowHeight))) {
            std::fprintf(stderr, "failed to create the portal recursion pool\n");
            gfx_shutdown();
            return 1;
        }

        // The core events, per session. Declared even with no mods loaded,
        // because the engine EMITS them on every block placed or broken and an
        // undeclared emit is an error message per swing of the pick.
        //
        // Both sessions, and that is the point of the loop rather than a call:
        // an event table is per world like everything else in a Session, so
        // declaring them only on the active one leaves the second world
        // complaining as soon as a player digs there.
        for (int i = 0; i < scene.count(); ++i) {
            scene.active = i;
            define_core_events(scene);
        }
        scene.active = 0;

        // Both worlds meshed before the first frame. The per-frame tick above
        // would get there on its own, at a chunk a frame for the passive one --
        // several seconds of watching the far world appear through the opening.
        // A one-off cost at startup buys a spawn that is already correct.
        for (int i = 0; i < scene.count(); ++i) {
            scene.active = i;
            settle_world(scene);
        }
        scene.active = 0;

        // No mods. The spike mod moves portal B every tick by index, and in this
        // scene portal B is the OTHER world's opening -- it would drag the far
        // end of the link around. Said rather than left to be discovered.
        std::printf("dual: two local worlds, seeds %u and %u, one opening each\n", world_seed,
                    world_seed + 6u);
        std::printf("      no mods in this scene: core.set_portal moves the far world's opening\n");

        const int rc_dual = run_interactive(scene, 0.0);

        for (int i = 0; i < scene.count(); ++i)
            if (scene.at(i).link_ab.rt.id) gfx_rt_destroy(scene.at(i).link_ab.rt);
        scene.body_render.shutdown();
        scene.selection.shutdown();
        scene.portal_render.shutdown_recursion();
        scene.portal_render.shutdown();
        for (int i = 0; i < scene.count(); ++i) scene.at(i).world_render.shutdown();
        gfx_shutdown();
        return rc_dual;
    }

    // The world the player is standing in. Its pictures come from the server
    // when there is one, and from this build's own folder when this build is
    // the one that made the world.
    if (!scene.act().world_render.init(
            scene.act().registry,
            world_media(networked ? net_sessions.get(self_session) : nullptr)) ||
        !scene.portal_render.init() ||
        !scene.selection.init() || !scene.body_render.init()) {
        gfx_shutdown();
        return 1;
    }

    // How a cell wearing a model gets drawn. Here rather than at the first
    // frame: a renderer without this hook draws such a cell as nothing at all,
    // which looks exactly like a missing model and is not one.
    //
    // This world only. The others are wired where they are built -- a second
    // session exists in the dual scene and when a portal opens one, and asking
    // for one that is not there is a walk off the end of the table.
    wire_model_draw(scene, scene.active);

    // A character, when one was named. Loaded here and not inside the frame
    // loop: uploading a mesh mid-frame is a stall, and a model that cannot be
    // read is something to say now rather than a hole in the world later.
    if (!model_path.empty()) {
        SkinnedModel model;
        std::string  why;
        if (!load_gltf(model_path, model, why)) {
            std::fprintf(stderr, "model: %s\n", why.c_str());
        } else if (!scene.model_render.init() || !scene.model_render.upload(model, why)) {
            std::fprintf(stderr, "model: %s\n", why.c_str());
        } else {
            scene.model_bones = model.bind_pose;
            scene.model_ready = true;

            // Where it stands is decided with the spawn, in run_interactive.
            std::printf("model: %s -- %zu verts, %zu tris, %d joints, %zu material(s), "
                        "%.2f x %.2f x %.2f blocks\n",
                        model_path.c_str(), model.vertices.size(), model.indices.size() / 3,
                        model.joint_count, model.materials.size(),
                        (double)(model.max.x - model.min.x), (double)(model.max.y - model.min.y),
                        (double)(model.max.z - model.min.z));
        }
    }

    // The same two openings again, in the form a body needs. Derived from a and
    // b above rather than written out a second time: two representations of one
    // opening is exactly the kind of pair that drifts, and the renderer showing
    // a portal somewhere the player cannot walk through is the drift that would
    // take longest to diagnose.
    scene.act().portal_a = portal_from_surface(a);
    scene.act().portal_b = portal_from_surface(b);

    // Two render targets at 1080p each.
    //
    // A network client used to skip these, because there was nothing for them
    // to show. Now there is: the server owns a pair of openings, the player can
    // put them somewhere, and what goes inside them is the world this client is
    // already streaming. So they are built here and stay empty until the server
    // says where the openings are -- see apply_server_portals, which is the one
    // place that turns drawing on.
    if (scene.act().portals || networked) {
        scene.act().link_ab = PortalLink{a, b, gfx_rt_create(portal_rt_dim(kWindowWidth),
                                                             portal_rt_dim(kWindowHeight), true)};
        scene.act().link_ba = PortalLink{b, a, gfx_rt_create(portal_rt_dim(kWindowWidth),
                                                             portal_rt_dim(kWindowHeight), true)};
        if (scene.act().link_ab.rt.id == 0 || scene.act().link_ba.rt.id == 0) {
            std::fprintf(stderr, "failed to create portal render targets\n");
            gfx_shutdown();
            return 1;
        }

        // And the pool for the levels below the first. Refused rather than
        // silently reduced: half a pool means some portals recurse and some
        // show a hole, which reads as a bug in the transform rather than as
        // running out of video memory.
        // Said out loud rather than left to two defaults that happen to agree:
        // resize_screen_targets rebuilds this pool from the global, and a pool
        // rebuilt one level shallower than it was built draws something else.
        g_recursion_depth = recursion;

        if (!scene.portal_render.init_recursion(2, recursion, portal_rt_dim(kWindowWidth),
                                                portal_rt_dim(kWindowHeight))) {
            std::fprintf(stderr, "failed to create the portal recursion pool at depth %d\n",
                         recursion);
            gfx_shutdown();
            return 1;
        }
        std::printf("portals: recursion depth %d, %d extra render targets\n", recursion,
                    2 * recursion);

        // With a facing pair the depth is the thing being looked at, so the
        // numbers that decide what is on screen get printed rather than left to
        // be worked out from the source.
        if (facing) {
            // Two portals, each spawning both again one level down, so the pass
            // count doubles per level: 2, 6, 14, 30. Same closed form the
            // recursion gate checks against measured counts.
            int passes = 0;
            for (int d = 0; d <= recursion; ++d) passes += 2 << d;

            std::printf("        facing pair: A at %.1f %.1f %.1f facing +X, B at %.1f %.1f %.1f "
                        "facing -X, %.0f apart\n"
                        "        %d world passes a frame; %d nested image%s deep, and then the\n"
                        "        innermost opening is not drawn at all -- it is a hole through\n"
                        "        to whatever stands behind the wall\n",
                        (double)a.center.x, (double)a.center.y, (double)a.center.z,
                        (double)b.center.x, (double)b.center.y, (double)b.center.z,
                        (double)(b.center.x - a.center.x), passes, recursion,
                        recursion == 1 ? "" : "s");
        }

        // So that a body halfway through a portal appears inside the portal too.
        scene.portal_render.set_extra_draw(draw_bodies_cb, &scene);
    }

    // ------------------------------------------------------------- scripting
    // One VM per mod, each with the heap ceiling and interrupt budget its own
    // manifest asked for. Both are runtime primitives in Luau rather than
    // conventions, which is why it is the primary backend (DESIGN.md 5.2), and
    // the isolation they buy is a build gate: tests/script_isolation_test.cpp.
    //
    // Not on a network client, and that is the honest position rather than a
    // shortcut. These mods write blocks and place portals; run here they would
    // edit a copy of the server's world that the server knows nothing about,
    // and every edit would be a desync this client manufactured for itself.
    // Mods belong to whoever owns the world. A client-side mod API -- one that
    // can draw and listen but not write -- is a separate thing, and it is not
    // this one.
    if (!networked) {
        script_host host = make_client_host(scene);

        ModHostCallbacks cb{};
        cb.before_reload = on_before_reload;
        cb.after_reload  = on_after_reload;
        cb.ud            = &scene;

        scene.act().mods.set_host(host);
        scene.act().mods.set_callbacks(cb);

        // A mod's systems, wired the same way the server wires them: who is
        // running while one runs, what a system that errors costs, and letting
        // go of a VM that goes away. The world is this process's own -- see
        // Scene::mod_ecs.
        scene.lua_ecs.set_world(&scene.mod_ecs);
        scene.lua_ecs.set_scope([&scene](script_vm *vm) { scene.act().mods.enter_vm(vm); },
                                [&scene](script_vm *vm) { scene.act().mods.leave_vm(vm); });
        scene.lua_ecs.set_error([&scene](const std::string &, script_vm *vm,
                                         const std::string &system, const std::string &error) {
            scene.act().mods.disable_vm(vm, "system '" + system + "': " + error);
        });
        scene.act().mods.set_vm_gone([&scene, sess = &scene.act()](script_vm *vm) {
            scene.lua_ecs.drop_vm(vm);
            scene.passes.drop_vm(vm);
            scene.hud_passes.drop_vm(vm);
            for (Session::LuaStage &st : sess->mapgen_stages)
                if (st.vm == vm) st.vm = nullptr;
        });

        // Before the mods load, or the first mod to ask for core:pre_place
        // would be told no such event exists.
        define_core_events(scene);

        // Which mods, and why those. The same line the server prints, for the
        // same reason: a build reading somebody else's checkout must say so.
        std::printf("mods from %s (%s)\n", mods_dir.c_str(),
                    data_dir_reason(mods_dir, CNP_SOURCE_ROOT "/mods").c_str());

        const int running = scene.act().mods.load_dir(mods_dir);
        const size_t adopted = scene.act().items.adopt_blocks(scene.act().registry);
        std::printf("scripting: %s, %d mods running, %zu block(s) can be held, %zu item(s) in all\n",
                    script_backend_name(), running, adopted, scene.act().items.size() - 1);

        // The mods' own stages of world generation, in the order they were
        // asked for, after the whole directory has loaded. The server does this
        // too and says why there, at length; what matters here is that it is
        // the SAME rule -- a stage runs once, before anything is drawn, and a
        // mod that asks afterwards is refused.
        // A world whose mods make all of it: the engine's ground goes, and
        // the engine makes none from here on -- the server's rule.
        if (Session &sess = scene.act(); !sess.core_mapgen) {
            const int columns = columns_across(g_world_extent);
            const int lo = sess.keeper ? -1 : 0, hi = sess.keeper ? columns : columns - 1;
            for (int y = 0; y < kWorldHeight; ++y)
                for (int z = lo * kChunkSize; z < (hi + 1) * kChunkSize; ++z)
                    for (int x = lo * kChunkSize; x < (hi + 1) * kChunkSize; ++x)
                        sess.world.set_block(x, y, z, kAir);
            if (sess.mapgen) sess.mapgen->clear_stages();
            std::printf("mapgen: no engine terrain, the mods make this world\n");
        }

        if (!scene.act().mapgen_stages.empty()) {
            Session &sess   = scene.act();
            size_t   failed = 0;

            // A column at a time, like the server: the box, and the ring round
            // it when this world has no edge. The rest run from the keeper's
            // hook, as the player goes.
            const int columns = columns_across(g_world_extent);
            const int lo = sess.keeper ? -1 : 0, hi = sess.keeper ? columns : columns - 1;
            for (int cz = lo; cz <= hi; ++cz)
                for (int cx = lo; cx <= hi; ++cx) failed += run_mod_stages(sess, ColumnPos{cx, cz});

            // A box with no keeper was lit before the mods loaded, so whatever a
            // stage wrote is sitting in the dark. A keeper lights its box below.
            if (!sess.keeper)
                compute_lighting(sess.world, sess.registry, 0, 0, 0, g_world_extent - 1,
                                 kWorldHeight - 1, g_world_extent - 1);
            std::printf("mapgen: %zu stage(s) from mods over %d column(s), %zu failure(s)\n",
                        sess.mapgen_stages.size(), (hi - lo + 1) * (hi - lo + 1), failed);
        }
        scene.act().mapgen_finished = true;

        // And the modifiers close with the stages, for the same reason: what an
        // ABM names may be registered by a mod that loads later.
        {
            scene.act().modifiers.set_seed(world_seed);

            std::vector<std::string> errors;
            scene.act().modifiers.close_registration(scene.act().registry, errors);
            for (const std::string &e : errors)
                std::fprintf(stderr, "modifier disabled: %s\n", e.c_str());

            if (!scene.act().modifiers.abms().empty() || !scene.act().modifiers.lbms().empty()) {
                std::printf("modifiers: %zu active, %zu loading\n",
                            scene.act().modifiers.abms().size(),
                            scene.act().modifiers.lbms().size());
                // A keeper queues a column's loading modifiers when it is lit.
                if (!scene.act().keeper)
                    for (const ChunkPos p : scene.act().world.chunk_positions_sorted())
                        scene.act().modifiers.chunk_loaded(p);
            }
            if (ColumnKeeper *k = scene.act().keeper.get())
                scene.act().modifiers.set_active([k](ChunkPos p) { return k->lit_chunk(p); });
        }

        // The box's light, now that every stage has written into it -- and the
        // box's columns lit, which is what a column has to be before a mod
        // writes into it again or a modifier sweeps it.
        if (Session &sess = scene.act(); sess.keeper) {
            const double t0      = plat_time();
            const int    columns = columns_across(g_world_extent);
            for (int cz = 0; cz < columns; ++cz)
                for (int cx = 0; cx < columns; ++cx) sess.keeper->make_lit(ColumnPos{cx, cz}, 0);
            sess.keeper->set_loading(false);
            sess.keeper->apply_ready_writes();
            sess.world.mark_all_dirty();
            std::printf("world: %d column(s) lit in %.2f s; the rest is made as the player goes\n",
                        columns * columns, plat_time() - t0);
        }

        // Mods register blocks while they load, so the atlas cannot be built
        // before they have run. Rebuilding here rather than at init is also
        // exactly what a reload will need.
        scene.act().world_render.rebuild_atlas(scene.act().world, scene.act().registry,
                                               read_texture_file, &scene.act().items);
        std::printf("registry: %zu nodes\n", scene.act().registry.size());

        // What the pack actually did, in numbers. "19 names mapped" is what the
        // manifest CLAIMED; this is what reached the atlas -- and the two differ
        // whenever a file is missing, is not square, or is at a size the rest of
        // the pack does not use.
        std::printf("atlas: %d x %d tiles at %dpx (%dpx square), %d from the pack, %d "
                    "generated\n",
                    scene.act().world_render.atlas_tiles(), scene.act().world_render.atlas_tiles(),
                    scene.act().world_render.tile_pixels(),
                    scene.act().world_render.atlas_pixels(),
                    scene.act().world_render.textures_supplied(),
                    scene.act().world_render.textures_generated());

        // And how many ITEMS got a square of their own, which is a different
        // question: an item form of a block wears the block's picture and needs
        // nothing drawn for it, while a tool has nothing to fall back on and is
        // a grey square until somebody draws one.
        {
            int drawn = 0, tools = 0;
            for (size_t i = 1; i < scene.act().items.size(); ++i) {
                const ItemDef &d = scene.act().items.get((ItemId)i);
                if (d.tex != 0) ++drawn;
                if (d.places == kAir) ++tools;
            }
            std::printf("items: %zu in all, %d with a picture of their own, %d that place no "
                        "block\n", scene.act().items.size() - 1, drawn, tools);

            // And WHICH of those, by name, because they are the ones a pack has
            // to name itself: everything else falls back to the block it puts
            // down, and a tool has nothing to fall back to.
            for (size_t i = 1; i < scene.act().items.size(); ++i) {
                const ItemDef &d = scene.act().items.get((ItemId)i);
                if (d.places != kAir) continue;
                std::printf("        %s: %s\n", d.name.c_str(),
                            d.tex != 0 ? "drawn" : "a blank square");
            }
        }

        // The same one sentence the server says, in the same place in the
        // order: after every mod has loaded and the world is what it is going
        // to be. A mod that waits for it has to be able to wait for it here.
        bool        ignored = false;
        std::string ready_err;
        if (!scene.act().mods.events().emit("core:world_ready", nullptr, 0, &ignored, ready_err))
            std::fprintf(stderr, "emit core:world_ready: %s\n", ready_err.c_str());
    }

    // Clear the corridor, AFTER the mods have run.
    //
    // Mods place blocks when they load -- the spike mod puts down fourteen and
    // the beacons mod raises three pillars -- and the world generator drops
    // stone columns of its own. Any of them landing between the two openings
    // stands exactly where the point of the scene is, and no amount of choosing
    // coordinates avoids it, because the mods choose theirs after this code has
    // chosen its. So the corridor is emptied at the last moment instead.
    //
    // Only the space strictly between the walls, so the walls themselves and
    // the ground survive. Relit block by block rather than with the bulk pass:
    // this is a handful of nodes and relight_around is what an edit uses.
    if (facing && !networked) {
        const int x0 = (int)std::floor(a.center.x) + 1;
        const int x1 = (int)std::floor(b.center.x) - 1;
        const int z0 = (int)std::floor(a.center.z) - 3;
        const int z1 = (int)std::floor(a.center.z) + 3;

        int cleared = 0;
        for (int x = x0; x <= x1; ++x)
            for (int z = z0; z <= z1; ++z)
                for (int y = kGroundTop; y <= kGroundTop + 5; ++y)
                    if (scene.act().world.get_block(x, y, z) != kAir) {
                        scene.act().world.set_block(x, y, z, kAir);
                        relight_around(scene.act().world, scene.act().registry, x, y, z);
                        ++cleared;
                    }

        std::printf("        corridor cleared: %d blocks removed between the openings\n",
                    cleared);
        scene.act().world.mark_all_dirty();
    }

    // The continuity check reads pixels; interactive play does not, so a player
    // never pays for a second full-size target.
    if (verify) {
        g_capture_rt = gfx_rt_create(kWindowWidth, kWindowHeight, true);
        if (g_capture_rt.id == 0) {
            std::fprintf(stderr, "could not create the capture target\n");
            gfx_shutdown();
            return 1;
        }
    }

    const int rc = networked  ? run_network(scene, net_sessions, self_session, net_cfg,
                                            player_name, play_seconds)
                 : verify     ? run_verify(scene)
                 : dual_bench ? run_dual_bench(scene)
                 : soak > 0.0 ? run_soak(scene, soak)
                 : bench      ? run_bench(scene)
                              : run_interactive(scene, play_seconds);
    if (g_to_title) pass_identity(menu);

    if (g_capture_rt.id) gfx_rt_destroy(g_capture_rt);
    scene.act().mods.shutdown();
    if (scene.act().link_ab.rt.id) gfx_rt_destroy(scene.act().link_ab.rt);
    if (scene.act().link_ba.rt.id) gfx_rt_destroy(scene.act().link_ba.rt);
    for (gfx_rt &rt : scene.more_rt)
        if (rt.id) gfx_rt_destroy(rt);
    for (gfx_rt &rt : scene.mouth_rt)
        if (rt.id) gfx_rt_destroy(rt);
    scene.body_render.shutdown();
    scene.fx_render.shutdown();
    scene.selection.shutdown();
    scene.portal_render.shutdown_recursion();
    scene.portal_render.shutdown();
    scene.act().world_render.shutdown();
    gfx_shutdown();
    return rc;
}

} // namespace

// Back to the title screen is this program started again, with the same
// arguments, once client_main has shut everything down -- the world's server
// saved and stopped, the connection closed, the window gone.
// ponytail: the window closes and opens again; a menu that tears the game down
// in place would keep it, once main() stops being one long function.
int main(int argc, char **argv)
{
    const int rc = client_main(argc, argv);
    if (!g_to_title) return rc;

    // By its real path: exec'd as /proc/self/exe it would be called "exe" in
    // every list of processes.
    const std::string self = exe_path();
    std::fflush(nullptr);
#ifdef _WIN32
    // A new process and this one ends: Windows has no exec that keeps the pid.
    _execv(self.c_str(), argv);
#else
    ::execv(self.c_str(), argv);
#endif
    std::perror("back to the title screen");
    return rc;
}

