// Placeholder textures, baked to files.
//
// Until now blocks had no media at all: the renderer generated a tile per node
// from `colour`, `checker` and `opacity` every time it built an atlas. That is
// why DESIGN.md 4.5 -- media addressed by hash, a cache shared between
// sessions, a delta handshake -- had nothing to carry. This makes the payload.
//
// It generates rather than downloads, and that is a choice with a reason: what
// the media pipeline needs is FILES WITH A HASH, not art. Any bytes exercise
// the addressing, the cache and the delta, and generated ones cost no licence,
// no download and no dependency. Art replaces them file by file when the look
// starts to matter, and the transport underneath does not change.
//
// It loads the mods, because the blocks worth having textures for are the ones
// a mod registered -- a run with the four mods in this tree registers 26 of
// them. The host it gives them is the smallest one that lets a mod finish
// loading: it registers nodes, answers questions about them, and writes blocks
// into a world nobody will ever look at.

#include "core/media/media_cache.h"
#include "core/media/png.h"
#include "core/media/tile_bake.h"
#include "core/registry/core_nodes.h"
#include "core/registry/node_registry.h"
#include "core/voxel/chunk.h"
#include "script/mod_host.h"

#include <cstdio>
#include <cstring>
#include <string>
#include <sys/stat.h>
#include <vector>

namespace {

using namespace cnp;

struct BakeState {
    World        world;      // somewhere for a mod that places blocks while loading
    NodeRegistry registry;
    ModHost      mods;
};

void   host_set_block(void *ud, int x, int y, int z, int id)
{
    static_cast<BakeState *>(ud)->world.set_block(x, y, z, (BlockId)id);
}
int    host_get_block(void *ud, int x, int y, int z)
{
    return (int)static_cast<BakeState *>(ud)->world.get_block(x, y, z);
}
void   host_set_portal(void *, int, float, float, float) {}
void   host_log(void *, const char *) {}   // a bake is not a place for a mod's chatter

int host_register_node(void *ud, const char *name, int r, int g, int b, int solid,
                       int light_source, const char *turn, int dig_level,
                       double hardness, const char *material)
{
    BakeState *s = static_cast<BakeState *>(ud);

    NodeDef d;
    d.name = name ? name : "unnamed";
    if (const ModManifest *m = mod_host_current()) d.owner = m->name;
    d.colour[0] = (uint8_t)r;
    d.colour[1] = (uint8_t)g;
    d.colour[2] = (uint8_t)b;
    d.solid  = solid != 0;
    d.opaque = solid != 0;
    d.light_source = (uint8_t)light_source;
    d.turn         = node_turn_from_name(turn);
    d.dig_level    = (uint8_t)(dig_level < 0 ? 0 : (dig_level > 255 ? 255 : dig_level));
    d.hardness     = (float)(hardness < 0.0 ? 0.0 : hardness);
    d.material     = material ? material : "";
    return (int)s->registry.register_node(d);
}

int host_node_id(void *ud, const char *name)
{
    return (int)static_cast<BakeState *>(ud)->registry.find(name ? name : "");
}

const char *host_node_name(void *ud, int id)
{
    BakeState *s = static_cast<BakeState *>(ud);
    if (id < 0 || (size_t)id >= s->registry.size()) return "";
    return s->registry.get((BlockId)id).name.c_str();
}

int host_event_define(void *ud, const char *event, const script_field *fields, int count,
                      int cancellable, char *err, size_t err_size)
{
    BakeState *s = static_cast<BakeState *>(ud);

    EventSchema schema;
    schema.name        = event ? event : "";
    schema.cancellable = cancellable != 0;
    schema.owner       = mod_host_current() ? mod_host_current()->name : "core";
    for (int i = 0; i < count; ++i)
        schema.fields.push_back(EventField{fields[i].name, fields[i].type});

    std::string e;
    if (!s->mods.events().define(schema, e)) {
        std::snprintf(err, err_size, "%s", e.c_str());
        return 0;
    }
    return 1;
}

int host_event_on(void *ud, const char *event, int priority, int handler_ref, char *err,
                  size_t err_size)
{
    BakeState        *s     = static_cast<BakeState *>(ud);
    const std::string owner = mod_host_current() ? mod_host_current()->name : "core";

    std::string e;
    if (!s->mods.events().on(event ? event : "", priority, mod_host_current_vm(), handler_ref,
                             owner, e)) {
        std::snprintf(err, err_size, "%s", e.c_str());
        return 0;
    }
    return 1;
}

int host_event_emit(void *ud, const char *event, script_field *fields, int count,
                    int *out_cancelled, char *err, size_t err_size)
{
    BakeState *s = static_cast<BakeState *>(ud);

    bool        cancelled = false;
    std::string e;
    if (!s->mods.events().emit(event ? event : "", fields, count, &cancelled, e)) {
        std::snprintf(err, err_size, "%s", e.c_str());
        return 0;
    }
    if (out_cancelled) *out_cancelled = cancelled ? 1 : 0;
    return 1;
}

bool file_exists(const std::string &path)
{
    struct stat st{};
    return ::stat(path.c_str(), &st) == 0;
}

} // namespace

int main(int argc, char **argv)
{
    const char *mods_dir = CNP_SOURCE_ROOT "/mods";
    const char *out_dir  = CNP_SOURCE_ROOT "/assets/textures";
    bool        force    = false;

    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--mods") == 0 && i + 1 < argc)      mods_dir = argv[++i];
        else if (std::strcmp(argv[i], "--out") == 0 && i + 1 < argc)  out_dir  = argv[++i];
        else if (std::strcmp(argv[i], "--no-mods") == 0)              mods_dir = nullptr;
        else if (std::strcmp(argv[i], "--force") == 0)                force    = true;
        else {
            std::printf("bake: unknown argument '%s'\n", argv[i]);
            return 1;
        }
    }

    BakeState s;
    register_core_nodes(s.registry);

    if (mods_dir) {
        script_host host{};
        host.set_block     = host_set_block;
        host.get_block     = host_get_block;
        host.set_portal    = host_set_portal;
        host.log           = host_log;
        host.register_node = host_register_node;
        host.node_id       = host_node_id;
        host.node_name     = host_node_name;
        host.event_define  = host_event_define;
        host.event_on      = host_event_on;
        host.event_emit    = host_event_emit;
        host.ud            = &s;
        s.mods.set_host(host);

        // The engine's own events, or the first mod to listen for one is told
        // it does not exist and stops loading before it registers anything.
        const std::vector<EventField> block_fields = {
            {"x", SCRIPT_V_NUMBER}, {"y", SCRIPT_V_NUMBER}, {"z", SCRIPT_V_NUMBER},
            {"id", SCRIPT_V_NUMBER},
        };
        const struct { const char *name; bool cancellable; } defs[] = {
            {"core:pre_place", true}, {"core:placed", false},
            {"core:pre_break", true}, {"core:broken", false},
        };
        std::string err;
        for (const auto &d : defs) {
            EventSchema schema;
            schema.name        = d.name;
            schema.owner       = "core";
            schema.fields      = block_fields;
            schema.cancellable = d.cancellable;
            s.mods.events().define(schema, err);
        }
        EventSchema tick;
        tick.name   = "core:tick";
        tick.owner  = "core";
        tick.fields = {{"dt", SCRIPT_V_NUMBER}};
        s.mods.events().define(tick, err);

        const int running = s.mods.load_dir(mods_dir);
        std::printf("bake: %d mods loaded from %s\n", running, mods_dir);
    }

    // 0777 masked by the process umask, which is the ordinary thing; an
    // existing directory is not an error.
    // Every component, not just the last one -- and the same function the media
    // cache uses, because "assets/textures under a tree that has no assets"
    // and "a cache directory two levels down" are one problem.
    {
        std::string dir_err;
        if (!make_dirs(out_dir, dir_err)) {
            std::printf("bake: %s\n", dir_err.c_str());
            return 1;
        }
    }

    int written = 0, kept = 0, bytes = 0;
    for (size_t i = 1; i < s.registry.size(); ++i) {   // air has no face and needs no texture
        const NodeDef    &d    = s.registry.get((BlockId)i);
        const std::string path = std::string(out_dir) + "/" + texture_file_name(d.name);

        // Kept, not overwritten. The whole point of files is that a real one
        // can replace a generated one; a baker that clobbers what it finds
        // would delete the art the first time somebody ran it.
        if (!force && file_exists(path)) {
            ++kept;
            continue;
        }

        const std::vector<uint8_t> px = bake_tile(d);
        std::string                err;
        if (!png_write_rgba(path, px.data(), kTilePixels, kTilePixels, err)) {
            std::printf("bake: %s\n", err.c_str());
            return 1;
        }
        ++written;
        bytes += (int)png_encode_rgba(px.data(), kTilePixels, kTilePixels).size();
    }

    // And a MARKED face for a block whose faces are not interchangeable. A
    // placeholder is the same on all six, so a block that turns about anything
    // but the vertical has nothing to show for it -- a log lying down looks
    // exactly like a log standing up, and the orientation is measurable in a
    // gate and invisible in the game.
    //
    // Only the kinds that need it: an upright block's top and bottom do not
    // move however it is turned, so marking a chest would cost two tiles to
    // say nothing. The word is asked of face_texture_name so that the file
    // lands where the atlas looks for it.
    int faces_written = 0;
    for (size_t i = 1; i < s.registry.size(); ++i) {
        const NodeDef &d = s.registry.get((BlockId)i);
        if (d.turn != NodeTurn::Facedir && d.turn != NodeTurn::Wallmounted) continue;

        const char *word = (d.turn == NodeTurn::Facedir) ? "top" : "front";
        const std::string path = std::string(out_dir) + "/" + face_texture_name(d.name, word);
        if (!force && file_exists(path)) {
            ++kept;
            continue;
        }

        const std::vector<uint8_t> px = bake_face_tile(d, word);
        std::string                err;
        if (!png_write_rgba(path, px.data(), kTilePixels, kTilePixels, err)) {
            std::printf("bake: %s\n", err.c_str());
            return 1;
        }
        ++faces_written;
        bytes += (int)png_encode_rgba(px.data(), kTilePixels, kTilePixels).size();
    }
    written += faces_written;

    std::printf("bake: %zu blocks; %d written, %d already there, %.1f KB into %s\n",
                s.registry.size() - 1, written, kept, (double)bytes / 1024.0, out_dir);
    if (!force && kept > 0)
        std::printf("      (--force rewrites the ones that are already there)\n");
    return 0;
}
