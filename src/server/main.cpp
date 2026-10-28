// The headless server.
//
// No window, no OpenGL, no raylib: it links cnp_core and cnp_script and nothing
// else, which is DESIGN.md 3.1 made checkable rather than promised. The day this
// stops linking, the server has grown a graphics dependency.
//
// What it does today is the CI half of the M3 exit test -- generate a world from
// a seed, run a fixed number of ticks with the mods loaded, print a hash of the
// world -- and that is deliberately the first thing built. Networking arrives
// against a server that is already deterministic, instead of determinism being
// chased through a protocol later.

#include <array>
#include "core/net/chat.h"
#include "core/net/chunk_codec.h"
#include "core/storage/world_db.h"
#include "core/media/media_set.h"
#include "core/ecs/machine.h"
#include "core/ecs/mob.h"
#include "core/item/inventory.h"
#include "core/item/item_codec.h"
#include "core/item/item_registry.h"
#include "core/item/recipe.h"
#include "core/ui/panel.h"
#include "core/ui/page.h"
#include "core/net/session.h"
#include "core/net/transport.h"
#include "core/registry/core_nodes.h"
#include "core/registry/model_registry.h"
#include "core/registry/node_registry.h"
#include "core/os.h"
#include "core/paths.h"
#include "core/physics/aabb.h"
#include "core/version.h"
#include "core/voxel/chunk.h"
#include "core/voxel/light.h"
#include "core/voxel/raycast.h"
#include "core/storage/column_keeper.h"
#include "core/world/columns.h"
#include "core/world/daylight.h"
#include "core/world/modifiers.h"
#include "core/world/node_meta.h"
#include "core/worldgen/spawn.h"
#include "core/worldgen/worldgen.h"
#include "core/diag/stack_dump.h"
#include "core/ecs/lua_ecs.h"
#include "core/voxel/find.h"
#include "core/voxel/pathfind.h"
#include "core/track/piece.h"
#include "script/mod_host.h"
#include "script/world_code.h"
#include "server/admin.h"
#include "core/update_check.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <atomic>
#include <deque>
#include <fstream>
#include <mutex>
#include <condition_variable>
#include <thread>

#include <cerrno>

using namespace cnp;

namespace {

// Everything a mod can reach on a server. The same script_host the client
// fills in, minus what has no meaning without a screen: there is no portal to
// move here yet, and set_portal is a client-side concept until M4 gives the
// server portals of its own.
struct ServerState {
    World        world;
    NodeRegistry registry;
    ItemRegistry items;
    ui::PanelSet panels;
    // Each square's role is the block's at the container's cell.
    // A carried one has no cell: its z names the block whose roles it takes.
    ContainerSet containers{[this](const ContainerAt &a) {
        if (a.y <= kCarriedBelow) return &registry.get((BlockId)(a.z < 0 ? 0 : a.z)).slot_rules;
        return &registry.get(world.get_block(a.x, a.y, a.z)).slot_rules;
    }};

    // What the bodies here look like, and who wears what. The two ids are held
    // on the state and not on the session table because a mod declares them
    // while it loads, which is before the socket exists -- see `sessions`
    // below, which is null for exactly the same reason.
    ModelRegistry models;
    ModelId       player_model = kNoModel;
    ModelId       mob_model    = kNoModel;

    // core.world_sky: one colour and no sun. core.mapgen_core: the engine's
    // ground or none.
    bool    has_sky = false;
    uint8_t sky[3]{};
    // core.world_storm: a tinted sky that keeps its sun, its clouds whirling.
    bool    has_storm = false;
    uint8_t storm[3]{};
    bool    core_mapgen = true;
    // core.mapgen_only: the mods whose stages make the ground; empty, all.
    std::set<std::string> mapgen_owners;

    // The kinds of creature the mods registered (core.register_mob), in the
    // order they did: a creature's Brain names one by its place here.
    std::vector<cnp::ecs::MobKind> mob_kinds;
    size_t                         pets_kept = 0;   // "pet:N" keys the store holds (keep_pets)

    // What this world's mods say turns some things into another thing.
    RecipeSet    recipes;
    ModHost      mods;

    // Things that happen TO cells on their own: grass spreading, a furnace
    // cooking, a world fixed up as it loads (core/world/modifiers.h).
    Modifiers    modifiers;
    double       modifier_clock   = 0.0;   // the world's seconds, not the wall's

    // The light a run of mod writes owes, done once at the end of that run
    // (TODO OPEN-29). `batching` is on exactly while mod code can be writing:
    // a player's edit is one cell and relights on the spot, as it always has.
    LightBatch   light_batch;
    bool         light_batching = false;
    uint64_t     light_batch_cells = 0;   // cells the batched passes covered
    uint64_t     light_writes_skipped = 0;   // writes whose light could not have changed
    int          light_batch_flushes = 0;
    uint64_t     modifier_actions = 0;
    uint64_t     modifier_cells   = 0;
    double       modifier_seconds = 0.0;

    // Entities a mod owns, and the systems that step them. The world they live
    // in belongs to the session table, which is built when the socket is -- so
    // a mod declares its components while it loads and they are minted when
    // there is somewhere to put them. An offline run (--ticks with no --listen)
    // has no session table and therefore no entities: it is a mapgen hash run
    // and not a game, and this says so rather than leaving it to be found.
    ecs::LuaEcs  lua_ecs;

    // The mods that asked for a channel, in the order they asked. The NUMBER a
    // mod gets is its place in this list plus kModChannelBase, and the same list
    // goes into the welcome -- so every client on this server is told the same
    // numbers, and a mod that computed its own would be computing the same
    // thing twice.
    std::vector<std::string> mod_channels;

    // How a body moves in this world, as its mods declared it (TODO.md
    // OPEN-44), and which mod said so -- so a second mod that says otherwise
    // is told whose numbers it replaced. Goes into the welcome, and the
    // validator's limits are worked out from it when the socket opens.
    MovementRules movement;
    std::string   movement_by;

    // Where the pair of openings stands when the server is started with
    // --portals: these unless a mod says otherwise while it loads
    // (core.set_portal). A world laid out around its own openings -- an arena
    // with walls -- cannot live with a pair placed for somebody else's plaza.
    // Past the pair, where a third to eighth --portal-to stands until a mod
    // says: along the plaza, facing +X as the second does.
    Vec3 openings[net::kMaxMouths] = {Vec3{40.5f, (float)kGroundTop + 1.0f, 44.5f},
                                      Vec3{60.5f, (float)kGroundTop + 1.0f, 60.5f},
                                      Vec3{80.5f, (float)kGroundTop + 1.0f, 60.5f},
                                      Vec3{100.5f, (float)kGroundTop + 1.0f, 60.5f},
                                      Vec3{40.5f, (float)kGroundTop + 1.0f, 80.5f},
                                      Vec3{60.5f, (float)kGroundTop + 1.0f, 80.5f},
                                      Vec3{80.5f, (float)kGroundTop + 1.0f, 80.5f},
                                      Vec3{100.5f, (float)kGroundTop + 1.0f, 80.5f}};
    // The room each of them leads to (--portal-to @ROOM), "" for none or for
    // another server: core.portal_room, while the mods load.
    std::string mouth_rooms[net::kMaxMouths];
    bool openings_late_told = false;

    // What mods want said on those channels, drained by the tick loop. Queued
    // and not sent here for the reason panels are: a host function has no
    // transport, and giving it one would put the socket inside the mod API.
    struct ModSay {
        int         player = 0;   // 0 means everybody in this world
        uint8_t     channel = 0;
        std::string text;
        bool        notice = false;   // core.notice: a net::Notice, not the mod's channel
        bool        chat   = false;   // core.chat: a net::Chat
    };
    std::vector<ModSay> mod_says;
    // A muted player whose /command is running (run_chat_line): what its mod
    // says to anybody else is held, so no mod's command is a way round /mute.
    int muted_caller = 0;
    // When each player last spoke, for the chat's flood limit (run_chat_line).
    std::unordered_map<net::PeerId, std::deque<double>> said_at;
    std::string muted_why;

    // The /commands this world's mods declared (core.command).
    net::ChatCommands commands;

    // The page each player has up (src/core/ui/page.h): what it says, so a
    // click can be read against the links it had, and a command run from it
    // can show it again with what changed.
    struct PageUp {
        std::string    name, owner, title, markup;   // owner "core" for the engine's
        int            screen = 0;
        ui::PageLayout layout;
    };
    std::unordered_map<net::PeerId, PageUp> pages_up;

    // The pages this world's mods offer (core.page_register), by name.
    struct PageDecl {
        std::string owner, title, perm;
    };
    std::map<std::string, PageDecl> pages;

    // Bodies a mod moved (core.player_place), sent after the tick's block
    // changes: a body put where a mod just took a block away has to find it
    // gone, and the two are on different channels.
    struct ModPlace {
        net::PeerId player = 0;
        Vec3        at, look;   // a look of length 0 keeps the one they have
    };
    std::vector<ModPlace> mod_places;

    // Workers carried on a mod's thing (core.mob_ride): put on their place in
    // its frame every tick right after the machines move, so a soldier in a
    // jeep goes with it the same tick and not a tick behind.
    struct MobRide {
        double thing = 0.0;
        float  x = 0.0f, y = 0.0f, z = 0.0f;
    };
    std::map<uint32_t, MobRide> mob_rides;

    // Bodies a mod launched (core.player_launch): sent with the placements.
    struct ModLaunch {
        net::PeerId player = 0;
        Vec3        vel;
        double      seconds = 0.0;
    };
    std::vector<ModLaunch> mod_launches;

    // When each body on foot was last shoved out of a machine's hull.
    std::unordered_map<net::PeerId, double> shoved;

    // Mouths sealed to one player (core.portal_seal): the mask to send them.
    std::vector<std::pair<net::PeerId, uint32_t>> mod_seals;

    // The portals mods open (core.portal_open): as many as they like. A world
    // has kMaxOpenings openings on the wire, so these are VIRTUAL: every half
    // second the ones within kPortalNear of a player, and whatever each leads
    // to, nearest first, are given an opening, and the rest give theirs back
    // (schedule_portals). The limit is what is in use near players at once,
    // not what a world holds. An id is the lowest one free, and leads to
    // id xor 1 until linked -- what core.portal_open always returned.
    struct VirtualPortal {
        Portal p;
        bool   alive = false;
        int    link  = -1;   // another id, or -1
        int    slot  = -1;   // the opening it has now, or -1
    };
    std::vector<VirtualPortal> vportals;
    double                     vportal_next = 0.0;

    // Seats taken or left by a mod (core.ride), decided at the call and sent
    // with the placements, in the order they were asked for.
    std::vector<std::pair<net::PeerId, net::SessionAction>> mod_rides;

    // Sounds a mod played this tick (core.play_sound), encoded, for everybody.
    std::vector<std::vector<uint8_t>> mod_sounds;

    // What the cells of this world remember. M8: three of the four classic portal-puzzle
    // blocks are a cell that holds a value.
    NodeMetaSet  node_meta;

    // Where a meta_get's answer lives until the next call into the host. The
    // alternative is an allocation per read, on a path a mod runs every tick.
    std::string  meta_scratch;

    // What mods keep across a restart (core.storage_set), and where a read's
    // answer lives until the next call, as meta_scratch.
    ModStore     mod_store;
    std::string  storage_scratch;
    std::string  name_scratch;   // and a player_name's, for the same reason

    // core.player_fly: the players who may not fly, and the flying speed each
    // gets back when they may again.
    std::unordered_map<int, float> fly_held;

    // Stages of world generation a mod registered, in the order they asked for
    // them. Order is the contract here exactly as it is for the C++ stages: a
    // stage sees what the ones before it wrote.
    struct LuaStage {
        std::string name;
        std::string owner;
        script_vm  *vm  = nullptr;
        int         ref = 0;
    };
    std::vector<LuaStage> mapgen_stages;

    // The context those stages are run against, kept for the life of the
    // process because core.terrain_height answers from it and a mod may ask
    // outside a stage -- the terrain is a function of the seed and the column,
    // so the answer does not depend on when it is asked.
    std::unique_ptr<MapgenContext> mapgen_ctx;

    // True only while the mods' stages are running. Two things read it:
    // core.register_mapgen refuses once the pass is over, because a stage
    // registered afterwards is a stage that never runs; and write_block skips
    // the per-cell relight, because the whole world is lit once when the pass
    // is done. The second is TODO OPEN-29's own advice taken in the one place
    // where a mod writes cells in the thousands.
    bool generating      = false;
    bool mapgen_finished = false;

    // The sessions, once there are any. A pointer because the mods load before
    // the socket does: a mod that opens a container at load time is opening it
    // for nobody, and this being null is what says so.
    net::ServerSessions *sessions = nullptr;
    // core.owned_area said before there were sessions (at core:world_ready),
    // handed to them when they come.
    std::vector<net::ServerSessions::OwnedArea> owned_early;
    std::vector<std::array<int, 3>>             owned_cells_early;   // x, z, tag

    // The side of the region generated before the mods load, from 0: the one
    // the welcome's hash is taken over and the spawn stands in. Not the size of
    // the world, which has none.
    int      extent = 128;
    int      height = kWorldHeight;
    uint32_t seed   = 1;

    // Which columns are in memory, and what coming in, being lit and being let
    // go of does to them (TODO.md OPEN-35, OPEN-38; core/storage/column_keeper.h).
    // The world has no edge: a column is read back or generated when somebody is
    // near enough to want it, and let go when nobody has been for a while. A
    // write into a column that is not lit waits for it.
    ColumnKeeper  keeper{world, registry, kWorldHeight};
    const Mapgen *mapgen = nullptr;   // the engine's stages; main() owns them

    uint64_t column_clock  = 0;   // ticks, which is when a column was last used
    // Columns a mod asked to keep (core.keep_loaded), to the tick they may go.
    std::unordered_map<ColumnPos, uint64_t, ColumnPosHash> kept;
    uint64_t chunks_let_go = 0;   // named in a ChunksGone, over every session

    // Server seconds, the same clock the sessions are told about. Here because
    // a stack a mod drops has to be armed and to expire against the clock
    // everything else on the ground uses, and a host function has no other way
    // to reach it. Zero while nothing is running, which is the right answer for
    // a mod dropping something before the socket exists: nobody is there to
    // pick it up and it will be gone five minutes into the run.
    double now = 0.0;

    long block_writes = 0;

    // Cells a mod has changed this tick.
    //
    // POSITIONS and not values: the id goes out when the message does, read
    // from the world, so a cell written three times in one tick is one message
    // carrying what is actually there rather than three carrying a history.
    // Deduplicated for the same reason -- mods/beacons rewrites the same cell
    // every frame.
    std::vector<uint64_t>        mod_changed;
    std::unordered_set<uint64_t> mod_changed_seen;

    // And what each session has not been told yet. Per session, because whether
    // a change is worth sending is a question about ONE session: whether it
    // holds the chunk, and whether it is still being sent the world.
    //
    // A session receiving chunks is a session whose world has not arrived, and
    // the world arriving comes first. Measured, not assumed: with these going
    // out to everybody every tick, bad_link went red eight times in eight, and
    // the failures were the far world never streaming and a player corrected
    // against a position claimed while it was still on its way.
    struct Owed {
        std::vector<uint64_t>        order;
        std::unordered_set<uint64_t> seen;

        void add(uint64_t key)
        {
            if (seen.insert(key).second) order.push_back(key);
        }
        void clear()
        {
            order.clear();
            seen.clear();
        }
    };
    std::unordered_map<uint32_t, Owed> owed;
    bool owed_overflowed = false;

    // Two numbers rather than one, because they are the two halves of the rule
    // and a gate has to see both: a run where nothing was ever held back has
    // not exercised "the world comes first", and a run where something was held
    // and never sent has lost it.
    long mod_changes_told = 0;
    long mod_changes_held = 0;

    // And the third: chunks dropped from a session's interest because a mod
    // changed so much of one that the chunk is the cheaper message. A gate has
    // to see this number beside the other two, or "told 3 changes" reads as a
    // mod that did almost nothing when it actually rewrote a chunk.
    long mod_chunks_resent = 0;

    // Whose bag a mod has changed and who has not been told yet. The same
    // reason mod_changed exists: a host function has no transport, and a player
    // whose bag was emptied by a mod goes on being drawn holding what they had.
    std::unordered_set<uint32_t> bags_touched;

    // Cells a mod asked to hear about (core.watch_cell), and the ones written
    // this tick, told as core:watched before the next core:tick.
    std::unordered_set<uint64_t> watch_cells;
    std::vector<uint64_t>        watch_hits;
    std::unordered_set<uint64_t> watch_hit_seen;

    // Packed so the set has something cheap to hash. The world is at most
    // 2^21 on a side in each direction, which is four thousand times the
    // largest one anybody has generated here.
    static uint64_t packed(int x, int y, int z)
    {
        const uint64_t ux = (uint64_t)(uint32_t)x & 0x1FFFFFull;
        const uint64_t uy = (uint64_t)(uint32_t)y & 0x1FFFFFull;
        const uint64_t uz = (uint64_t)(uint32_t)z & 0x1FFFFFull;
        return (ux << 42) | (uy << 21) | uz;
    }
    static void unpacked(uint64_t k, int &x, int &y, int &z)
    {
        const auto sign21 = [](uint64_t v) {
            return (int)((v & 0x100000ull) ? (int64_t)v - 0x200000 : (int64_t)v);
        };
        x = sign21((k >> 42) & 0x1FFFFFull);
        y = sign21((k >> 21) & 0x1FFFFFull);
        z = sign21(k & 0x1FFFFFull);
    }
};

// One place where a block changes on this server, whoever asked for it.
//
// A player's edit relit the world around the cell; a mod's write did neither
// that nor anything else, so a mod's lamp was dark and a mod's block reached
// nobody. TODO OPEN-17: mods/beacons has been lighting a beacon every frame
// since M2 and no connected player has ever seen one.
// Returns whether anything actually changed.
//
// A write of the block that is already there is not a change, and saying so is
// the difference between six messages a second and eighty. mods/spike calls
// set_block on the same cell every tick and only alters it twice a second --
// which is normal, because a mod says what it WANTS a cell to be rather than
// keeping a diff -- so the engine is the side that has to know.
// `facing` is which way the cell ends up turned, 0..3. Not defaulted, and not
// left alone: a cell keeps its turn until something says otherwise, so a write
// that did not say would put a new block into the turn the last one was left
// in -- a chest built where a furnace was, facing whichever way the furnace
// faced.
bool column_takes_writes(const ServerState &s, int x, int z)
{
    // While the mods load, the region generated before them has blocks and no
    // light yet, and a mod building its scene writes straight into it -- which
    // is what a stage run after it and a world file read after that are laid
    // over. Once they have loaded, only a lit column.
    return s.keeper.takes_writes(x, z);
}

int host_fall(void *ud, int x, int y, int z);

bool write_block(ServerState &s, int x, int y, int z, BlockId id, uint8_t facing)
{
    // A stage writes only into the column it is generating.
    if (s.generating) {
        if (s.mapgen_ctx && !s.mapgen_ctx->owns(x, z)) return false;
    } else if (!column_takes_writes(s, x, z)) {
        s.keeper.defer(PendingWrite{x, y, z, id, facing});
        return false;
    } else {
        s.keeper.wrote(x, z, s.column_clock);
    }

    // The facing counts as a change. Placing the same block turned the other way
    // is not "nothing happened": it is what a player does when they put a chest
    // down the wrong way round and put it down again.
    const BlockId had = s.world.get_block(x, y, z);
    if (had == id && s.world.get_facing(x, y, z) == facing) return false;

    s.world.set_block(x, y, z, id);
    s.world.set_facing(x, y, z, facing);
    s.block_writes++;

    // Watched (core.watch_cell): every write comes through here -- a mod's, a
    // player's edit, a door a creature opened -- so a watch sees them all.
    if (!s.watch_cells.empty()) {
        const uint64_t w = ServerState::packed(x, y, z);
        if (s.watch_cells.count(w) && s.watch_hit_seen.insert(w).second) s.watch_hits.push_back(w);
    }

    // Not while the world is being generated. A mapgen stage writes cells by
    // the thousand and the whole world is lit once when the pass is over, so a
    // relight per cell here is a lighting pass per block -- TODO OPEN-29's
    // measurement, which is 1.9 ms a cell and makes a stage that builds
    // anything unusable.
    if (!s.generating) {
        // Two ways out of the 1.9 ms a cell OPEN-29 measured, and they are
        // independent. The first is exact: the pass reads whether a block is
        // opaque and what it emits, so a cell whose old and new blocks agree
        // about both is a relight that can find nothing.
        if (light_unchanged(s.registry, had, id)) {
            ++s.light_writes_skipped;
        } else if (s.light_batching) {
            // And the second is the run: a mod writes a thousand cells and the
            // light for all of them is done once, when it has stopped writing.
            s.light_batch.add(x, y, z);
        } else {
            relight_around(s.world, s.registry, x, y, z);
        }
    }

    // A cell that has become air remembers nothing. Here and not on every id
    // change, and the difference is a real one: a button that goes from up to
    // down is the same cell holding the same state, and clearing on any change
    // would take it away. What breaking a block has to take away is the state,
    // so that the next block placed there does not inherit it.
    if (id == kAir) s.node_meta.clear(Cell{x, y, z});

    // A block that falls (core.node_falls), written over nothing, lets go; and
    // one standing on this cell does when this cell stops holding it up.
    if (!s.generating && s.sessions) {
        const auto loose = [&](int cy) {
            const BlockId b = s.world.get_block(x, cy, z);
            return b < s.registry.size() && s.registry.get(b).falls && cy > 0 &&
                   !s.registry.is_solid(s.world.get_block(x, cy - 1, z));
        };
        if (loose(y)) host_fall(&s, x, y, z);
        else if (loose(y + 1)) host_fall(&s, x, y + 1, z);
    }
    return true;
}

// Whether this session has been sent the chunk that cell is in.
//
// A cell inside a chunk somebody has not received yet is a cell that will
// arrive WITH that chunk -- the chunk is encoded from the world as it is now,
// so the change is already in it. Telling them anyway is a message that costs
// the stream it is competing with, and on a bad link that stream is the world
// arriving.
//
// The comment on the edit broadcast used to say a filter by interest "would
// cost more than it saves". That was written when a block change happened when
// a player clicked. Once a mod's writes started going out too (TODO CLOSED-17)
// the same filter stopped being an optimisation and became the thing that keeps
// a joining client's world from arriving late: bad_link went red eight times in
// eight, every failure a correction against a position claimed while the world
// was still on its way.
// How many changed cells one session may be owed before the engine gives up on
// telling it cell by cell.
//
// Reachable only for a session whose world is still arriving: everything else
// is drained every tick, and a chunk with more than kCellsWorthAChunk changes in
// it is re-sent rather than listed. Past this the queue is emptied AND the
// chunks it named are dropped from that session's interest, so nothing is lost
// -- which is the half of TODO OPEN-19 that was a hole and not a cost.
constexpr size_t kMaxOwedCells = 8192;

bool already_has(const net::ServerSessions &sessions, net::PeerId peer, int x, int y, int z)
{
    const net::Session *sess = sessions.find(peer);
    return sess && sess->interest.was_sent(World::chunk_of(x, y, z));
}

void host_set_block(void *ud, int x, int y, int z, int id)
{
    ServerState *s = static_cast<ServerState *>(ud);
    // Facing 0: a mod's set_block says which block, not which way round. A cell
    // it writes over keeps no turn of the block that was there -- and giving a
    // mod the turn as well is core.set_facing, which OPEN-27 has still to add.
    if (!write_block(*s, x, y, z, (BlockId)id, 0)) return;


    // Only while there is somebody who could be told. Before the socket exists
    // every mod in the directory is building its scene, and those writes are in
    // the chunk whoever joins is sent -- queueing them would be a list of
    // thousands that nobody is waiting for.
    if (!s->sessions) return;

    const uint64_t key = ServerState::packed(x, y, z);
    if (s->mod_changed_seen.insert(key).second) s->mod_changed.push_back(key);
}

// A track piece's cells, all of them or none: written as a mod's writes are,
// and queued to be told the same way (core/track/piece.h).
void write_told(ServerState &s, int x, int y, int z, BlockId id, uint8_t facing)
{
    if (!write_block(s, x, y, z, id, facing) || !s.sessions) return;
    const uint64_t key = ServerState::packed(x, y, z);
    if (s.mod_changed_seen.insert(key).second) s.mod_changed.push_back(key);
}

bool put_piece(ServerState &s, BlockId id, uint8_t facing, int ox, int oy, int oz)
{
    const NodeDef &d = s.registry.get(id);
    if (d.piece.plan == 0 || !node_turn_allows(d.turn, facing)) return false;
    std::vector<PieceCell> cells;
    piece_cells(d.piece, id, facing, ox, oy, oz, cells);
    if (!piece_has_room(s.world, s.registry, cells)) return false;
    piece_headroom(s.world, s.registry, cells, cells);
    for (const PieceCell &c : cells) write_told(s, c.x, c.y, c.z, c.id, c.facing);
    return true;
}

// Takes away the piece whose cell (x, y, z) is, and says which piece it was.
bool take_piece(ServerState &s, int x, int y, int z, BlockId &piece)
{
    int ox, oy, oz;
    if (!piece_find(s.world, s.registry, x, y, z, ox, oy, oz)) return false;
    piece = s.world.get_block(ox, oy, oz);
    std::vector<PieceCell> cells;
    piece_cells(s.registry.get(piece).piece, piece, s.world.get_facing(ox, oy, oz), ox, oy, oz, cells);
    for (const PieceCell &c : cells)
        if (s.world.get_block(c.x, c.y, c.z) == c.id) write_told(s, c.x, c.y, c.z, kAir, 0);
    return true;
}

int host_node_piece(void *ud, int id, int fill, int plan, int size, int climb, int grade0,
                    int grade1, int bank0, int bank1, int curbs, int wall, int width)
{
    ServerState *s = static_cast<ServerState *>(ud);
    const auto   b = [](int v) { return (int8_t)std::clamp(v, -127, 127); };
    PieceShape   p;
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
    if (id < 0 || fill < 0 || !piece_declare(s->registry, (BlockId)id, (BlockId)fill, p)) {
        std::printf("[mod] node_piece: not a piece -- block %d, fill %d\n", id, fill);
        return 0;
    }
    return 1;
}

int host_place_piece(void *ud, int x, int y, int z, int id, int facing)
{
    ServerState *s = static_cast<ServerState *>(ud);
    if (id <= 0 || (size_t)id >= s->registry.size() || facing < 0 || facing > 3) return 0;
    int ox, oz;
    piece_origin(s->registry.get((BlockId)id).piece, (uint8_t)facing, x, z, ox, oz);
    return put_piece(*s, (BlockId)id, (uint8_t)facing, ox, y, oz) ? 1 : 0;
}

int host_piece_entry(void *ud, int x, int y, int z, int *ex, int *ey, int *ez, int *id, int *facing)
{
    ServerState *s = static_cast<ServerState *>(ud);
    int          ox, oy, oz;
    if (!ex || !ey || !ez || !id || !facing || !piece_find(s->world, s->registry, x, y, z, ox, oy, oz))
        return 0;
    const BlockId piece = s->world.get_block(ox, oy, oz);
    *facing             = s->world.get_facing(ox, oy, oz) & 3;
    piece_entry(s->registry.get(piece).piece, (uint8_t)*facing, ox, oz, *ex, *ez);
    *ey = oy;
    *id = (int)piece;
    return 1;
}

int host_take_piece(void *ud, int x, int y, int z)
{
    BlockId piece = kAir;
    return take_piece(*static_cast<ServerState *>(ud), x, y, z, piece) ? (int)piece : 0;
}

// Turning a cell is a change like any other: it has to reach the people who can
// see it, or a mod's furnace faces one way on the server and another on every
// screen. Same queue as a block write, and no relight -- which way a block is
// turned does not change what light it stops or emits.
int host_watch_cell(void *ud, int x, int y, int z, int on)
{
    ServerState *s = static_cast<ServerState *>(ud);
    const uint64_t k = ServerState::packed(x, y, z);
    if (on) s->watch_cells.insert(k);
    else s->watch_cells.erase(k);
    return 1;
}

void host_set_facing(void *ud, int x, int y, int z, int facing)
{
    ServerState *s = static_cast<ServerState *>(ud);
    if (facing < 0) return;

    // What the byte may be is the BLOCK's business: four turns for a chest,
    // twenty-four for a log, six faces for a ladder (registry/node_registry.h).
    // Refused rather than masked into range, and said out loud with the mod's
    // name on it -- a turn that quietly became a different turn is a mod
    // author looking at the wrong block.
    const NodeDef &def = s->registry.get(s->world.get_block(x, y, z));
    if (!node_turn_allows(def.turn, (unsigned)facing)) {
        const ModManifest *m = mod_host_current();
        std::fprintf(stderr, "mod '%s': core.set_facing %d is not a turn '%s' has\n",
                     m ? m->name.c_str() : "?", facing, def.name.c_str());
        return;
    }

    if (!s->generating && !column_takes_writes(*s, x, z)) {
        PendingWrite w{x, y, z, kAir, (uint8_t)facing};
        w.facing_only = true;
        s->keeper.defer(w);
        return;
    }
    if (s->generating && s->mapgen_ctx && !s->mapgen_ctx->owns(x, z)) return;

    if (s->world.get_facing(x, y, z) == (uint8_t)facing) return;

    s->world.set_facing(x, y, z, (uint8_t)facing);
    if (!s->generating) s->keeper.wrote(x, z, s->column_clock);
    if (!s->sessions) return;

    const uint64_t key = ServerState::packed(x, y, z);
    if (s->mod_changed_seen.insert(key).second) s->mod_changed.push_back(key);
}

int host_get_facing(void *ud, int x, int y, int z)
{
    return (int)static_cast<ServerState *>(ud)->world.get_facing(x, y, z);
}

int host_get_block(void *ud, int x, int y, int z)
{
    return (int)static_cast<ServerState *>(ud)->world.get_block(x, y, z);
}

// Who is in this world, and where. The two questions M8's button and pedestal
// could not ask (docs/archive/M8_FINDINGS.md 1.2).
//
// Observers are left out on purpose: a portal looking in has no avatar and no
// inventory (DESIGN.md 4.4), so there is nobody there to stand on a pad or be
// handed anything. A mod that saw them would be told a body is present that
// nothing can touch.
// A mod reaching into a player's bag.
//
// Both go through the ONE thing that owns it -- Session::bag(), the same
// Inventory the placement path spends from -- rather than a copy: two ways to
// take something out of a bag are two bags that agree until one of them is
// edited.
int host_take_item(void *ud, int player, const char *item, int count)
{
    ServerState *s = static_cast<ServerState *>(ud);
    if (!s->sessions || !item || count <= 0) return 0;

    net::Session *sess = s->sessions->find_mutable((net::PeerId)player);
    if (!sess || sess->scope == net::Scope::Observe) return 0;

    const ItemId id = s->items.find(item);
    if (id == kNoItem) return 0;

    const int taken = sess->bag().take_by_item(id, count);

    if (taken > 0) s->bags_touched.insert((uint32_t)player);
    return taken;
}

// core.bag_count: the same bag core.take_item spends from, only counted.
int host_bag_count(void *ud, int player, const char *item)
{
    ServerState *s = static_cast<ServerState *>(ud);
    if (!s->sessions || !item) return 0;
    net::Session *sess = s->sessions->find_mutable((net::PeerId)player);
    if (!sess || sess->scope == net::Scope::Observe) return 0;
    const ItemId id = s->items.find(item);
    if (id == kNoItem) return 0;
    int n = 0;
    for (int i = 0; i < sess->bag().size(); ++i)
        if (sess->bag().at(i).item == id) n += sess->bag().at(i).count;
    return n;
}

// core.swap_item: one `from` out of the bag, `n` of `to` in, with the stack's
// tag kept -- a stolen stone broken to cobblestone is stolen cobblestone. A
// tagged stack goes first. What does not fit is the mod's (returned short).
int host_swap_item(void *ud, int player, const char *from, const char *to, int n)
{
    ServerState *s = static_cast<ServerState *>(ud);
    if (!s->sessions || !from || !to || n <= 0 || n > 64) return -1;
    net::Session *sess = s->sessions->find_mutable((net::PeerId)player);
    if (!sess || sess->scope == net::Scope::Observe) return -1;
    const ItemId a = s->items.find(from), b = s->items.find(to);
    if (a == kNoItem || b == kNoItem) return -1;
    Inventory &bag = sess->bag();
    int pick = -1;
    for (int i = 0; i < bag.size(); ++i) {
        const ItemStack &st = bag.at(i);
        if (st.empty() || st.item != a) continue;
        if (pick < 0 || ((st.tag & kTagIdMask) != 0 && (bag.at(pick).tag & kTagIdMask) == 0)) pick = i;
    }
    if (pick < 0) return -1;
    const uint32_t tag = bag.at(pick).tag & kTagIdMask;
    bag.take_from(pick, 1);
    const uint16_t left = bag.add(s->items, b, (uint16_t)n, -1, 0, 0, tag);
    s->bags_touched.insert((uint32_t)player);
    return n - left;
}

// core.owned_area, core.bag_tagged and core.take_tagged: a village's goods.
int host_owned_area(void *ud, int x1, int z1, int x2, int z2, int tag)
{
    ServerState *s = static_cast<ServerState *>(ud);
    if (tag < 0 || (uint32_t)tag > kTagIdMask) return 0;
    if (s->sessions) s->sessions->set_owned(x1, z1, x2, z2, (uint32_t)tag);
    else s->owned_early.push_back({x1, z1, x2, z2, (uint32_t)tag});
    return 1;
}

int host_owned_cell(void *ud, int x, int z, int tag)
{
    ServerState *s = static_cast<ServerState *>(ud);
    if (tag < 0 || (uint32_t)tag > kTagIdMask) return 0;
    if (s->sessions) s->sessions->set_owned_cell(x, z, (uint32_t)tag);
    else s->owned_cells_early.push_back({x, z, tag});
    return 1;
}

int host_owned_at(void *ud, int x, int z)
{
    ServerState *s = static_cast<ServerState *>(ud);
    if (s->sessions) return (int)s->sessions->owned_at(x, z);
    // Before the sessions: what was said so far, the last word for a column.
    for (auto it = s->owned_cells_early.rbegin(); it != s->owned_cells_early.rend(); ++it)
        if ((*it)[0] == x && (*it)[1] == z) return (*it)[2];
    for (const net::ServerSessions::OwnedArea &a : s->owned_early)
        if (x >= std::min(a.x1, a.x2) && x <= std::max(a.x1, a.x2) && z >= std::min(a.z1, a.z2) &&
            z <= std::max(a.z1, a.z2))
            return (int)a.tag;
    return 0;
}

int host_owned_member(void *ud, int player, int tag, int on)
{
    ServerState *s = static_cast<ServerState *>(ud);
    if (!s->sessions || tag <= 0 || (uint32_t)tag > kTagIdMask) return 0;
    net::Session *sess = s->sessions->find_mutable((net::PeerId)player);
    if (!sess) return 0;
    std::vector<uint32_t> &of = sess->owner_of;
    of.erase(std::remove(of.begin(), of.end(), (uint32_t)tag), of.end());
    if (on) of.push_back((uint32_t)tag);
    return 1;
}

// core.player_factors. Walk is folded into the body's own numbers, so its
// client predicts by them and the validator checks by them; core.body_movement
// scales what it is given by the factor in force (host_body_movement).
int host_player_factors(void *ud, int player, double walk, double dig, double hurt, double regen,
                        const char *source)
{
    ServerState *s = static_cast<ServerState *>(ud);
    if (!(walk >= 0.25 && walk <= 4.0) || !(dig >= 0.1 && dig <= 10.0) ||
        !(hurt >= 0.0 && hurt <= 10.0) || !(regen >= 0.0 && regen <= 10.0))
        return 0;
    if (!s->sessions || player <= 0) return 0;
    net::Session *sess = s->sessions->find_mutable((net::PeerId)player);
    if (!sess || sess->scope == net::Scope::Observe) return 0;

    // This source's four, then the product of them all, in the same ranges.
    const std::string from = source ? source : "";
    if (walk == 1.0 && dig == 1.0 && hurt == 1.0 && regen == 1.0)
        sess->factor_sources.erase(from);
    else
        sess->factor_sources[from] = {(float)walk, (float)dig, (float)hurt, (float)regen};
    walk = dig = hurt = regen = 1.0;
    for (const auto &[_, f] : sess->factor_sources) {
        walk *= f[0];
        dig *= f[1];
        hurt *= f[2];
        regen *= f[3];
    }
    walk  = std::clamp(walk, 0.25, 4.0);
    dig   = std::clamp(dig, 0.1, 10.0);
    hurt  = std::clamp(hurt, 0.0, 10.0);
    regen = std::clamp(regen, 0.0, 10.0);
    sess->hurt_factor  = (float)hurt;
    sess->regen_factor = (float)regen;
    if (sess->walk_factor == (float)walk && sess->dig_factor == (float)dig) return 1;

    MovementRules rules = sess->movement;
    const float   by    = (float)walk / sess->walk_factor;
    rules.walk   *= by;
    rules.sprint *= by;
    if (auto held = s->fly_held.find(player); held != s->fly_held.end()) rules.fly = 0.0f;
    const float walk_was = sess->walk_factor, dig_was = sess->dig_factor;
    sess->walk_factor = (float)walk;
    sess->dig_before  = dig_was;
    sess->dig_factor  = (float)dig;
    if (!s->sessions->set_body_movement((net::PeerId)player, rules, s->now)) {
        sess->walk_factor = walk_was;
        sess->dig_factor  = dig_was;
        return 0;
    }
    return 1;
}

int host_bag_tagged(void *ud, int player, int tag, int *out, int max)
{
    ServerState *s = static_cast<ServerState *>(ud);
    if (!s->sessions || tag <= 0) return 0;
    const net::Session *sess = s->sessions->find((net::PeerId)player);
    if (!sess || sess->scope == net::Scope::Observe) return 0;
    int n = 0;
    for (const ItemStack &st : sess->bag().slots())
        if (!st.empty() && (st.tag & kTagIdMask) == (uint32_t)tag && n + 2 <= max) {
            out[n++] = (int)st.item;
            out[n++] = (int)st.count;
        }
    return n;
}

int host_take_tagged(void *ud, int player, int tag)
{
    ServerState *s = static_cast<ServerState *>(ud);
    if (!s->sessions || tag <= 0) return 0;
    net::Session *sess = s->sessions->find_mutable((net::PeerId)player);
    if (!sess || sess->scope == net::Scope::Observe) return 0;
    int taken = 0;
    for (int i = 0; i < sess->bag().size(); ++i) {
        const ItemStack &st = sess->bag().at(i);
        if (!st.empty() && (st.tag & kTagIdMask) == (uint32_t)tag) taken += sess->bag().take_from(i).count;
    }
    if (taken > 0) s->bags_touched.insert((uint32_t)player);
    return taken;
}

// core.held_item: the square the player chose (their Hold), by name.
const char *host_held_item(void *ud, int player)
{
    ServerState *s = static_cast<ServerState *>(ud);
    s->name_scratch.clear();
    if (!s->sessions) return s->name_scratch.c_str();
    net::Session *sess = s->sessions->find_mutable((net::PeerId)player);
    if (sess && sess->scope != net::Scope::Observe && !sess->bag().held().empty())
        s->name_scratch = s->items.get(sess->bag().held().item).name;
    return s->name_scratch.c_str();
}

// core.worn_item: one of the worn squares after the 36, by name.
const char *host_worn_item(void *ud, int player, const char *square)
{
    ServerState *s = static_cast<ServerState *>(ud);
    s->name_scratch.clear();
    const int w = wear_slot(square);
    if (!s->sessions || w == 0) return s->name_scratch.c_str();
    net::Session *sess = s->sessions->find_mutable((net::PeerId)player);
    if (sess && sess->scope != net::Scope::Observe) {
        const ItemStack &st = sess->bag().at(Inventory::kDefaultSlots + w - 1);
        if (!st.empty()) s->name_scratch = s->items.get(st.item).name;
    }
    return s->name_scratch.c_str();
}

// core.worn_id and core.worn_set_id: the low bits of a worn stack's tag.
ItemStack *worn_stack(ServerState *s, int player, const char *square)
{
    const int w = wear_slot(square);
    if (!s->sessions || w == 0) return nullptr;
    net::Session *sess = s->sessions->find_mutable((net::PeerId)player);
    if (!sess || sess->scope == net::Scope::Observe) return nullptr;
    ItemStack &st = sess->bag().slot(Inventory::kDefaultSlots + w - 1);
    return st.empty() ? nullptr : &st;
}

int host_worn_id(void *ud, int player, const char *square)
{
    const ItemStack *st = worn_stack(static_cast<ServerState *>(ud), player, square);
    return st ? (int)(st->tag & kTagIdMask) : 0;
}

int host_worn_set_id(void *ud, int player, const char *square, int id)
{
    ServerState *s  = static_cast<ServerState *>(ud);
    ItemStack   *st = worn_stack(s, player, square);
    if (st == nullptr || id < 0 || (uint32_t)id > kTagIdMask) return 0;
    st->tag = (st->tag & ~kTagIdMask) | (uint32_t)id;
    s->bags_touched.insert((uint32_t)player);
    return 1;
}

// core.enchant_held and core.held_enchant: the hand's stack, and only one
// thing -- a stack of 64 enchanted at once would be 64 enchantments for one.
int host_held_id(void *ud, int player)
{
    ServerState *s = static_cast<ServerState *>(ud);
    if (!s->sessions) return 0;
    net::Session *sess = s->sessions->find_mutable((net::PeerId)player);
    if (!sess || sess->scope == net::Scope::Observe) return 0;
    const ItemStack &st = sess->bag().at(sess->bag().selected());
    return st.empty() ? 0 : (int)(st.tag & kTagIdMask);
}

int host_held_set_id(void *ud, int player, int id)
{
    ServerState *s = static_cast<ServerState *>(ud);
    if (!s->sessions || id < 0 || (uint32_t)id > kTagIdMask) return 0;
    net::Session *sess = s->sessions->find_mutable((net::PeerId)player);
    if (!sess || sess->scope == net::Scope::Observe) return 0;
    ItemStack &st = sess->bag().slot(sess->bag().selected());
    if (st.empty()) return 0;
    st.tag = (st.tag & ~kTagIdMask) | (uint32_t)id;
    s->bags_touched.insert((uint32_t)player);
    return 1;
}

int host_enchant_held(void *ud, int player, int level, int kind)
{
    ServerState *s = static_cast<ServerState *>(ud);
    if (!s->sessions) return -1;
    net::Session *sess = s->sessions->find_mutable((net::PeerId)player);
    if (!sess || sess->scope == net::Scope::Observe) return -1;
    ItemStack &held = sess->bag().slot(sess->bag().selected());
    if (held.empty() || s->items.max_stack(held.item) != 1) return -1;
    if (kind < 1 || kind > 4) return -1;
    held.ench = enchant_with(held.ench, (Enchant)kind, std::clamp(level, 0, 3));
    s->bags_touched.insert((uint32_t)player);
    return enchant_level_for(held.ench, (Enchant)kind);
}

// core.wear_held: the hand's stack spends a use, as a dig does (EditPlan::wears).
int host_wear_held(void *ud, int player, int amount)
{
    ServerState *s = static_cast<ServerState *>(ud);
    if (!s->sessions || amount <= 0) return 0;
    net::Session *sess = s->sessions->find_mutable((net::PeerId)player);
    if (!sess || sess->scope == net::Scope::Observe) return 0;
    if (!sess->bag().wear_slot(s->items, sess->bag().selected(), (uint16_t)std::min(amount, 65535))) return 0;
    s->bags_touched.insert((uint32_t)player);
    return 1;
}

int host_held_enchant(void *ud, int player)
{
    ServerState *s = static_cast<ServerState *>(ud);
    if (!s->sessions) return -1;
    const net::Session *sess = s->sessions->find((net::PeerId)player);
    if (!sess || sess->scope == net::Scope::Observe) return -1;
    const ItemStack &held = sess->bag().held();
    return held.empty() ? -1 : held.ench;
}

int host_give_item(void *ud, int player, const char *item, int count, int tag)
{
    ServerState *s = static_cast<ServerState *>(ud);
    if (!s->sessions || !item || count <= 0) return 0;

    net::Session *sess = s->sessions->find_mutable((net::PeerId)player);
    if (!sess || sess->scope == net::Scope::Observe) return 0;

    const ItemId id = s->items.find(item);
    if (id == kNoItem) return 0;

    const uint16_t asked = count > 65535 ? (uint16_t)65535 : (uint16_t)count;
    const uint16_t left  = sess->bag().give_to_hand(s->items, id, asked, (uint32_t)std::clamp(tag, 0, (int)kTagIdMask));
    const int      given = (int)asked - (int)left;

    if (given > 0) s->bags_touched.insert((uint32_t)player);
    return given;
}

int host_players(void *ud, int *out, int max)
{
    ServerState *s = static_cast<ServerState *>(ud);
    if (!s->sessions || !out || max <= 0) return 0;

    int n = 0;
    for (const net::PeerId peer : s->sessions->ready_peers()) {
        if (n >= max) break;
        const net::Session *sess = s->sessions->find(peer);
        if (!sess || sess->scope == net::Scope::Observe) continue;
        out[n++] = (int)peer;
    }
    return n;
}

const char *host_side(void *ud)
{
    (void)ud;
    return "server";
}

const char *host_player_name(void *ud, int player)
{
    ServerState *s = static_cast<ServerState *>(ud);
    s->name_scratch.clear();
    if (!s->sessions) return s->name_scratch.c_str();

    const net::Session *sess = s->sessions->find((net::PeerId)player);
    if (sess && sess->scope != net::Scope::Observe) s->name_scratch = sess->name;
    return s->name_scratch.c_str();
}

const char *host_player_key(void *ud, int player)
{
    ServerState *s = static_cast<ServerState *>(ud);
    s->name_scratch.clear();
    if (!s->sessions) return s->name_scratch.c_str();
    const net::Session *sess = s->sessions->find((net::PeerId)player);
    if (sess && sess->scope != net::Scope::Observe && sess->has_identity)
        s->name_scratch = net::key_fingerprint(sess->identity);
    return s->name_scratch.c_str();
}

int host_world_spawn(void *ud, float *x, float *y, float *z)
{
    ServerState *s = static_cast<ServerState *>(ud);
    if (!s->sessions || !x || !y || !z) return 0;
    const net::SpawnPoint &at = s->sessions->spawn();
    *x = at.x; *y = at.y; *z = at.z;
    return 1;
}

// A mod's line about a body (core.player_carry): kept on the session while the
// body is here, and taken with it by take_carried when it drives elsewhere.
int host_player_carry(void *ud, int player, const char *text)
{
    ServerState *s = static_cast<ServerState *>(ud);
    if (!s->sessions || !text || std::strlen(text) > 256) return 0;

    net::Session *sess = s->sessions->find_mutable((net::PeerId)player);
    if (!sess || sess->scope == net::Scope::Observe || !sess->holds_bag) return 0;
    sess->carried = text;
    return 1;
}

int host_body_taken(void *ud, int player)
{
    ServerState *s = static_cast<ServerState *>(ud);
    const net::Session *sess = s->sessions ? s->sessions->find((net::PeerId)player) : nullptr;
    return sess && sess->body_taken ? 1 : 0;
}

const char *host_player_carried(void *ud, int player)
{
    ServerState *s = static_cast<ServerState *>(ud);
    s->name_scratch.clear();
    if (!s->sessions) return s->name_scratch.c_str();

    const net::Session *sess = s->sessions->find((net::PeerId)player);
    if (sess && sess->scope != net::Scope::Observe) s->name_scratch = sess->carried;
    return s->name_scratch.c_str();
}

int host_player_pos(void *ud, int player, float *x, float *y, float *z)
{
    ServerState *s = static_cast<ServerState *>(ud);
    if (!s->sessions || !x || !y || !z) return 0;

    const net::Session *sess = s->sessions->find((net::PeerId)player);
    if (!sess || sess->scope == net::Scope::Observe) return 0;

    // The position the movement authority ACCEPTED, not the last claim that
    // arrived. A mod asking where somebody is has to be told the same thing the
    // reach check and the fall are working from -- otherwise a modified client
    // could stand on a button from across the world by claiming to.
    const MovementState &at = sess->authority();
    if (!at.valid) return 0;   // joined, never moved: there is no position yet

    *x = at.x;
    *y = at.y;
    *z = at.z;
    return 1;
}

int host_player_look(void *ud, int player, float *x, float *y, float *z)
{
    ServerState *s = static_cast<ServerState *>(ud);
    if (!s->sessions || !x || !y || !z) return 0;

    const net::Session *sess = s->sessions->find((net::PeerId)player);
    if (!sess || sess->scope == net::Scope::Observe || !sess->has_moved) return 0;

    // The look the interest was last moved with, which is the one that came
    // with the last ACCEPTED claim: a refused claim moves neither.
    float lx, ly, lz;
    sess->interest.look(lx, ly, lz);
    const Vec3 dir{lx, ly, lz};
    if (length(dir) < 1e-6f) return 0;

    const Vec3 unit = normalize(dir);
    *x = unit.x;
    *y = unit.y;
    *z = unit.z;
    return 1;
}

int host_raycast(void *ud, double x, double y, double z, double dx, double dy, double dz,
                 double max, int *hx, int *hy, int *hz, float *distance)
{
    ServerState *s = static_cast<ServerState *>(ud);
    const Vec3   dir{(float)dx, (float)dy, (float)dz};
    if (length(dir) < 1e-6f || !hx || !hy || !hz || !distance) return 0;

    // Capped, because this runs inside the tick and a mod's interrupt budget
    // counts VM safepoints, not the cells the engine walks on its behalf.
    const RaycastHit hit = raycast(s->world, s->registry, Vec3{(float)x, (float)y, (float)z},
                                   normalize(dir), (float)std::clamp(max, 0.0, 256.0));
    if (!hit.hit) return 0;
    *hx       = hit.x;
    *hy       = hit.y;
    *hz       = hit.z;
    *distance = hit.distance;
    return 1;
}

int host_meta_set(void *ud, int x, int y, int z, const char *key, const char *value)
{
    ServerState *s = static_cast<ServerState *>(ud);
    std::string  why;
    if (s->node_meta.set(Cell{x, y, z}, key ? key : "", value ? value : "", why)) return 1;
    std::printf("[mod] meta at %d %d %d: %s\n", x, y, z, why.c_str());
    return 0;
}

const char *host_meta_get(void *ud, int x, int y, int z, const char *key)
{
    ServerState *s  = static_cast<ServerState *>(ud);
    s->meta_scratch = s->node_meta.get(Cell{x, y, z}, key ? key : "");
    return s->meta_scratch.c_str();
}

// Where a world's openings stand. While the mods load, and not after: the pair
// is installed in the session table when the socket opens, and it goes to every
// client in a Portals message -- an opening that moved afterwards would teleport
// from one place and be drawn in another. Later calls are ignored, said once:
// mods/spike moves its portal on every step, which is a client's scene.
void host_set_portal(void *ud, int index, float x, float y, float z)
{
    ServerState *s = static_cast<ServerState *>(ud);
    if (index < 0 || index >= net::kMaxMouths) return;

    if (s->mapgen_finished) {
        if (!s->openings_late_told) {
            s->openings_late_told = true;
            const ModManifest *m = mod_host_current();
            std::printf("[mod] %s: set_portal: too late -- a world places its openings while "
                        "its mods load; this call and any after it are ignored\n",
                        m ? m->name.c_str() : "?");
        }
        return;
    }
    s->openings[index] = Vec3{x, y, z};
}

const char *host_portal_room(void *ud, int index)
{
    const ServerState *s = static_cast<ServerState *>(ud);
    return index >= 0 && index < net::kMaxMouths ? s->mouth_rooms[index].c_str() : "";
}

// Tells the room an opening leads to that it moved (core:far_moved there).
// Defined beside the Router, which holds the rooms.
void tell_far_room(const std::string &room, int portal, const Portal &p);

// One of the two openings moved while the world runs (core.portal_place).
int host_portal_place(void *ud, int index, float x, float y, float z, int axis, int sign,
                      float half_w, float half_h)
{
    ServerState *s = static_cast<ServerState *>(ud);
    if (!s->sessions || index < 0 || index >= net::kMaxMouths || axis < 0 || axis > 1 ||
        (sign != 1 && sign != -1) || !std::isfinite(x) || !std::isfinite(y) || !std::isfinite(z) ||
        !(half_w > 0.0f && half_w <= 16.0f) || !(half_h > 0.0f && half_h <= 16.0f))
        return 0;
    if (s->sessions->opening(index) == nullptr) return 0;   // no openings here (--portals)
    // Past the pair only a mouth's: an opening a mod opened there is its own.
    if (index >= 2 && !s->sessions->away(index).leads_away()) return 0;
    Portal p;
    p.center      = Vec3{x, y, z};
    p.axis        = (PortalAxis)axis;
    p.normal_sign = sign;
    p.half_w      = half_w;
    p.half_h      = half_h;
    s->sessions->move_opening(index, p);
    s->openings[index] = p.center;

    // A room of this same server over there hears it, so a mod there can move
    // its end to match. ponytail: rooms only; a far server on another address
    // is not told, which wants a message on the wire.
    const net::PortalAway &away = s->sessions->away(index);
    if (away.host.empty() && away.port == 0 && !away.room.empty())
        tell_far_room(away.room, away.portal, p);
    return 1;
}

float host_get_portal(void *ud, int index, int axis)
{
    const ServerState *s = static_cast<ServerState *>(ud);
    if (index < 0 || index >= net::kMaxMouths || axis < 0 || axis > 2) return 0.0f;
    const Vec3 &at = s->openings[index];
    return axis == 0 ? at.x : axis == 1 ? at.y : at.z;
}

// An opening a mod opens while the world runs: a frame somebody built. Unlike
// set_portal this is late on purpose -- it goes through the session table,
// which sends every client the new pair, so there is no copy left behind to lie.
// The table is built when the socket opens, after core:world_ready: before it,
// and on a server that never listens, there is nowhere to open anything.
int host_portal_open(void *ud, float x, float y, float z, int axis, int sign, int rot,
                     float half_w, float half_h, int colour)
{
    ServerState *s = static_cast<ServerState *>(ud);
    if (!s->sessions) return -1;

    const bool sane = std::isfinite(x) && std::isfinite(y) && std::isfinite(z) &&
                      axis >= 0 && axis <= 2 && (sign == 1 || sign == -1) && rot >= 0 &&
                      rot <= 3 && half_w > 0.0f && half_w <= 16.0f && half_h > 0.0f &&
                      half_h <= 16.0f;
    if (!sane) {
        const ModManifest *m = mod_host_current();
        std::printf("[mod] %s: portal_open: not an opening (axis %d, sign %d, rot %d, "
                    "half %.2f x %.2f)\n", m ? m->name.c_str() : "?", axis, sign, rot,
                    (double)half_w, (double)half_h);
        return -1;
    }

    Portal p;
    p.center      = Vec3{x, y, z};
    p.axis        = (PortalAxis)axis;
    p.normal_sign = sign;
    p.rot         = rot;
    p.half_w      = half_w;
    p.half_h      = half_h;
    p.kind        = PortalKind::Frame;
    p.colour      = (uint8_t)(colour >= 0 && colour <= 2 ? colour : 0);

    // The one already at this centre in this plane takes the new shape -- on
    // its opening too, if it has one; anything else the lowest free id.
    auto &v = s->vportals;
    int   id = -1;
    for (int i = 0; i < (int)v.size() && id < 0; ++i)
        if (v[i].alive && v[i].p.axis == p.axis && length(v[i].p.center - p.center) < 0.01f) id = i;
    if (id < 0) {
        for (int i = 0; i < (int)v.size() && id < 0; ++i)
            if (!v[i].alive) id = i;
        if (id < 0) {
            id = (int)v.size();
            v.emplace_back();
        }
        v[id] = ServerState::VirtualPortal{};
        v[id].alive = true;
        // Leads to id xor 1 until linked, as the openings always did -- unless
        // that one is linked elsewhere.
        const int pair = id ^ 1;
        if (pair < (int)v.size() && v[pair].alive && v[pair].link < 0) {
            v[id].link   = pair;
            v[pair].link = id;
        } else if (pair >= (int)v.size() || !v[pair].alive) {
            v[id].link = pair;   // a partner opened later finds it waiting
        }
    }
    v[id].p = p;
    if (v[id].slot >= 0) s->sessions->open_portal(p);   // reshaped where it stands
    s->vportal_next = 0.0;   // given an opening on the next tick, not in half a second
    return id;
}

int host_portal_close(void *ud, float x, float y, float z)
{
    ServerState *s = static_cast<ServerState *>(ud);
    if (!s->sessions) return -1;
    auto &v = s->vportals;
    for (int i = 0; i < (int)v.size(); ++i) {
        if (!v[i].alive || length(v[i].p.center - Vec3{x, y, z}) >= 0.01f) continue;
        if (v[i].slot >= 0) s->sessions->close_opening(v[i].slot);
        v[i] = ServerState::VirtualPortal{};
        // Its partner keeps the link, as a closed opening's did: it leads to
        // an id with nothing in it, and draws nothing, until linked again.
        return i;
    }
    return -1;
}

// Every half second: which virtual portals have an opening. Wanted are the
// ones within kPortalNear of a player and whatever those lead to, each keyed
// by the nearer of the two ends, nearest first, as many as there are openings
// to hand out; the others give theirs back. Then every portal with an
// opening is linked to its partner's opening, or to nothing.
constexpr float kPortalNear = 160.0f;
void schedule_portals(ServerState &s, double now)
{
    if (!s.sessions || now < s.vportal_next) return;
    s.vportal_next = now + 0.5;
    auto &v = s.vportals;
    if (v.empty()) return;

    std::vector<Vec3> bodies;
    for (const net::PeerId peer : s.sessions->ready_peers()) {
        const net::Session *sess = s.sessions->find(peer);
        if (!sess || sess->scope == net::Scope::Observe) continue;
        const MovementState &at = sess->authority();
        if (at.valid) bodies.push_back(Vec3{at.x, at.y, at.z});
    }
    std::vector<float> near(v.size(), 1e30f);
    for (size_t i = 0; i < v.size(); ++i)
        if (v[i].alive)
            for (const Vec3 &b : bodies) near[i] = std::min(near[i], length(v[i].p.center - b));
    std::vector<std::pair<float, int>> wanted;
    int held = 0;
    for (size_t i = 0; i < v.size(); ++i) {
        if (!v[i].alive) continue;
        if (v[i].slot >= 0) ++held;
        const int   l   = v[i].link;
        const float key = std::min(near[i], l >= 0 && l < (int)v.size() && v[l].alive ? near[l] : 1e30f);
        if (key <= kPortalNear) wanted.emplace_back(key, (int)i);
    }
    std::sort(wanted.begin(), wanted.end());
    const size_t room = (size_t)(s.sessions->free_openings() + held);
    if (wanted.size() > room) wanted.resize(room);
    std::vector<char> want(v.size(), 0);
    for (const auto &w : wanted) want[w.second] = 1;

    int taken = 0, given = 0, alive = 0;
    for (size_t i = 0; i < v.size(); ++i) {
        alive += v[i].alive ? 1 : 0;
        if (v[i].slot >= 0 && !want[i]) {
            s.sessions->close_opening(v[i].slot);
            v[i].slot = -1;
            ++taken;
        }
    }
    for (const auto &w : wanted)
        if (v[w.second].slot < 0) {
            v[w.second].slot = s.sessions->open_portal(v[w.second].p);
            given += v[w.second].slot >= 0 ? 1 : 0;
        }
    if (taken + given > 0)
        std::printf("portals: %d given an opening, %d gave one back; %d of %d live\n", given, taken,
                    held - taken + given, alive);
    for (size_t i = 0; i < v.size(); ++i) {
        if (v[i].slot < 0) continue;
        const int l = v[i].link;
        s.sessions->link_portals(v[i].slot, l >= 0 && l < (int)v.size() && v[l].alive ? v[l].slot : -1);
    }
}

// Sent with the placements: a seal is the player's own and not the world's,
// so it does not wait for the openings' version to move.
int host_portal_seal(void *ud, int player, int index, bool sealed)
{
    ServerState  *s    = static_cast<ServerState *>(ud);
    net::Session *sess = s->sessions ? s->sessions->find_mutable((net::PeerId)player) : nullptr;
    if (!sess || index < 0 || index >= net::kMaxMouths ||
        s->mouth_rooms[index].empty())
        return 0;
    const uint32_t was = sess->sealed;
    sess->sealed = sealed ? was | (1u << index) : was & ~(1u << index);
    if (sess->sealed != was) s->mod_seals.emplace_back((net::PeerId)player, sess->sealed);
    return 1;
}

// Between virtual portals (core.portal_open's ids), as link_portals is
// between openings: pairs, and whatever either led to leads nowhere now. The
// openings follow on the next tick.
int host_portal_link(void *ud, int a, int b)
{
    ServerState *s = static_cast<ServerState *>(ud);
    if (!s->sessions || a < 0 || a >= 1 << 20 || b < -1 || b >= 1 << 20) return 0;
    auto &v = s->vportals;
    const int most = std::max(a, b);
    if (most >= (int)v.size()) v.resize((size_t)most + 1);
    if (v[a].link >= 0 && v[a].link < (int)v.size() && v[v[a].link].link == a) v[v[a].link].link = -1;
    v[a].link = -1;
    if (b >= 0 && b != a) {
        if (v[b].link >= 0 && v[b].link < (int)v.size() && v[v[b].link].link == b) v[v[b].link].link = -1;
        v[b].link = a;
        v[a].link = b;
    }
    s->vportal_next = 0.0;
    return 1;
}

void host_log(void *ud, const char *message)
{
    (void)ud;
    std::printf("[mod] %s\n", message);
}

int host_register_node(void *ud, const char *name, int r, int g, int b, int solid,
                       int light_source, const char *turn, int dig_level,
                       double hardness, const char *material)
{
    ServerState *s = static_cast<ServerState *>(ud);

    NodeDef d;
    d.name = name ? name : "unnamed";
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
        // A misspelled kind is a block that quietly does not turn, which is the
        // same quiet failure the manifest's unknown-key refusal exists to stop.
        std::fprintf(stderr, "register_node('%s'): '%s' is not a turn kind\n",
                     name ? name : "?", turn ? turn : "");
        return 0;
    }
    d.dig_level    = (uint8_t)(dig_level < 0 ? 0 : (dig_level > 255 ? 255 : dig_level));
    d.hardness     = (float)(hardness < 0.0 ? 0.0 : hardness);
    d.material     = material ? material : "";
    return (int)s->registry.register_node(d);
}

// A box on a block's shape. The server does not draw, and keeps it anyway: the
// block table is one table, a reload replaces a definition whole, and a shape
// that lived only on the client would be a second place for what a block IS.
int host_node_model(void *ud, int id_in, int model_in)
{
    ServerState *s  = static_cast<ServerState *>(ud);
    const BlockId id = (BlockId)id_in;
    if (id == kAir || id >= s->registry.size()) return 0;

    // A model this world does not have is refused rather than stored: a block
    // pointing at an id nobody registered is a block that draws nothing, and it
    // would be found by looking at it rather than by being told.
    const ModelId model = (ModelId)(model_in < 0 ? 0 : model_in);
    if (model != kNoModel && model > s->models.size()) return 0;

    NodeDef d = s->registry.get(id);
    d.model   = model;
    s->registry.register_node(d);   // same name, same id, new definition
    return (int)model;
}

int host_node_climbable(void *ud, int id_in, int on)
{
    ServerState *s  = static_cast<ServerState *>(ud);
    NodeRegistry &R = s->registry;
    const BlockId id = (BlockId)id_in;
    if (id == kAir || id >= R.size()) return 0;
    NodeDef d   = R.get(id);
    d.climbable = on != 0;
    R.register_node(d);
    return 1;
}

int host_container_roles(void *ud, int id_in, const char *roles)
{
    NodeRegistry &R = static_cast<ServerState *>(ud)->registry;
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
    NodeRegistry &R = static_cast<ServerState *>(ud)->registry;
    const BlockId id = (BlockId)id_in;
    if (id == kAir || id >= R.size()) return 0;
    NodeDef d     = R.get(id);
    d.keeps_drops = on != 0;
    R.register_node(d);
    return 1;
}

int host_container_accepts(void *ud, int id_in, const char *role, const char *items)
{
    NodeRegistry &R = static_cast<ServerState *>(ud)->registry;
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
    ServerState *s = static_cast<ServerState *>(ud);
    if (s->containers.find(ContainerAt{x, y, z}) == nullptr) return 0;
    s->containers.set_progress(ContainerAt{x, y, z}, s->now, seconds);
    return 1;
}

int host_container_scroll(void *ud, int x, int y, int z, float pos, float size)
{
    ServerState *s = static_cast<ServerState *>(ud);
    if (s->containers.find(ContainerAt{x, y, z}) == nullptr) return 0;
    s->containers.set_scroll(ContainerAt{x, y, z}, pos, size);
    return 1;
}

int host_container_search(void *ud, int x, int y, int z, const char *text)
{
    ServerState *s = static_cast<ServerState *>(ud);
    if (s->containers.find(ContainerAt{x, y, z}) == nullptr) return 0;
    s->containers.set_search(ContainerAt{x, y, z}, text ? std::string(text).substr(0, ui::kMaxPanelText) : "");
    return 1;
}

int host_node_conveyor(void *ud, int id_in, float speed, float height, float rise, int rail)
{
    NodeRegistry &R = static_cast<ServerState *>(ud)->registry;
    const BlockId id = (BlockId)id_in;
    if (id == kAir || id >= R.size()) return 0;
    NodeDef d  = R.get(id);
    d.conveyor        = std::max(speed, 0.0f);
    d.conveyor_height = height;
    d.conveyor_rise   = rise;
    d.rail            = rail != 0;
    R.register_node(d);
    return 1;
}

// Two blocks that are one door: `shut` and `open`, each the other's other.
int host_node_door(void *ud, int shut_in, int open_in)
{
    ServerState *s  = static_cast<ServerState *>(ud);
    NodeRegistry &R = s->registry;
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
    ServerState *s  = static_cast<ServerState *>(ud);
    const BlockId id = (BlockId)id_in, as = (BlockId)(as_in < 0 ? 0 : as_in);
    if (id == kAir || id >= s->registry.size() || as >= s->registry.size() || as == id) return 0;

    NodeDef d  = s->registry.get(id);
    d.drops_as = as;
    s->registry.register_node(d);
    return 1;
}

int host_node_drop_item(void *ud, int id_in, const char *item)
{
    ServerState *s = static_cast<ServerState *>(ud);
    const BlockId id = (BlockId)id_in;
    if (id == kAir || id >= s->registry.size() || item == nullptr) return 0;

    NodeDef d    = s->registry.get(id);
    d.drops_item = item;
    s->registry.register_node(d);
    return 1;
}

int host_node_solid(void *ud, int id)
{
    return id > 0 && static_cast<ServerState *>(ud)->registry.is_solid((BlockId)id) ? 1 : 0;
}

int host_node_colour(void *ud, int id)
{
    const NodeRegistry &reg = static_cast<ServerState *>(ud)->registry;
    if (id <= 0 || (size_t)id >= reg.size()) return 0;
    const uint8_t *c = reg.get((BlockId)id).colour;
    return (c[0] << 16) | (c[1] << 8) | c[2];
}

int host_loaded(void *ud, int x, int y, int z)
{
    return static_cast<ServerState *>(ud)->world.find_chunk(World::chunk_of(x, y, z)) ? 1 : 0;
}

int host_keep_loaded(void *ud, int x, int z, int r)
{
    ServerState *s = static_cast<ServerState *>(ud);
    r              = std::clamp(r, 0, 48);
    const ColumnPos lo = column_of_cell(x - r, z - r), hi = column_of_cell(x + r, z + r);
    int n = 0;
    // Thirty seconds, as long as a column nobody touches stays anyway.
    for (int cx = lo.x; cx <= hi.x; ++cx)
        for (int cz = lo.z; cz <= hi.z; ++cz, ++n) s->kept[ColumnPos{cx, cz}] = s->column_clock + 600;
    return n;
}

int host_node_turns(void *ud, int id)
{
    const NodeRegistry &reg = static_cast<ServerState *>(ud)->registry;
    return id > 0 && (size_t)id < reg.size() ? node_turn_values(reg.get((BlockId)id).turn) : 0;
}

int host_node_alias(void *ud, const char *old_name, const char *name)
{
    ServerState *s = static_cast<ServerState *>(ud);
    if (old_name == nullptr || name == nullptr) return 0;
    s->registry.alias(old_name, name);
    return 1;
}

int host_item_alias(void *ud, const char *old_name, const char *name)
{
    if (old_name == nullptr || name == nullptr) return 0;
    static_cast<ServerState *>(ud)->items.alias(old_name, name);
    return 1;
}

int host_player_regen(void *ud, int player, int on)
{
    ServerState *s = static_cast<ServerState *>(ud);
    if (player <= 0 || s->sessions == nullptr) return 0;
    net::Session *sess = s->sessions->find_mutable((net::PeerId)player);
    if (sess == nullptr) return 0;
    sess->no_regen = on == 0;
    return 1;
}

int host_player_max_health(void *ud, int player, int max)
{
    ServerState *s = static_cast<ServerState *>(ud);
    if (player <= 0 || s->sessions == nullptr || max < 1 || max > 1000) return 0;
    return s->sessions->set_max_health((net::PeerId)player, (uint16_t)max) ? 1 : 0;
}

float host_time_of_day(void *ud)
{
    ServerState *s = static_cast<ServerState *>(ud);
    return s->sessions ? (float)s->sessions->time_of_day() : 0.0f;
}

int host_light(void *ud, int x, int y, int z)
{
    ServerState  *s     = static_cast<ServerState *>(ud);
    const uint8_t l     = s->world.get_light(x, y, z);
    const float   day   = s->has_sky ? 0.0f : daylight_at(host_time_of_day(ud));
    const float   sky   = (float)sun_light(l) * day;
    return std::max((int)block_light(l), (int)std::lround(sky));
}

int host_block_light(void *ud, int x, int y, int z)
{
    return (int)block_light(static_cast<ServerState *>(ud)->world.get_light(x, y, z));
}

int host_set_time(void *ud, double t)
{
    ServerState *s = static_cast<ServerState *>(ud);
    if (s->sessions == nullptr || !std::isfinite(t)) return 0;
    s->sessions->set_time(t);
    return 1;
}

int host_player_spawn(void *ud, int player, double x, double y, double z, int on)
{
    ServerState *s = static_cast<ServerState *>(ud);
    if (player <= 0 || s->sessions == nullptr) return 0;
    net::Session *sess = s->sessions->find_mutable((net::PeerId)player);
    if (sess == nullptr || !std::isfinite(x) || !std::isfinite(y) || !std::isfinite(z)) return 0;
    sess->has_bed = on != 0;
    sess->bed[0]  = (float)x;
    sess->bed[1]  = (float)y;
    sess->bed[2]  = (float)z;
    return 1;
}

int host_node_box(void *ud, int id_in, double x0, double y0, double z0, double x1, double y1,
                  double z1, const char *part)
{
    ServerState  *s  = static_cast<ServerState *>(ud);
    const BlockId id = (BlockId)id_in;
    if (id == kAir || id >= s->registry.size()) return 0;

    NodeDef d = s->registry.get(id);
    NodeDef::Box box;
    box.min[0] = (float)x0; box.min[1] = (float)y0; box.min[2] = (float)z0;
    box.max[0] = (float)x1; box.max[1] = (float)y1; box.max[2] = (float)z1;
    box.part   = part ? part : "";
    d.parts.push_back(box);
    s->registry.register_node(d);   // same name, same id, new definition
    return (int)d.parts.size();
}

int host_register_item(void *ud, const char *name, int max_stack, const char *places,
                       int dig_level, double dig_speed, const char *digs, int damage,
                       const char *wear, int armor, int durability, int arms)
{
    ServerState *s = static_cast<ServerState *>(ud);

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

    // By name, and looked up now: an item that names a block nobody registered
    // places air, which is the honest answer to "put down a thing that does not
    // exist". A mod that means it should declare `depends`.
    d.places = (places && *places) ? s->registry.find(places) : kAir;

    return (int)s->items.register_item(d);
}

int host_item_id(void *ud, const char *name)
{
    return (int)static_cast<ServerState *>(ud)->items.find(name ? name : "");
}

int host_item_count(void *ud)
{
    return (int)static_cast<ServerState *>(ud)->items.size();
}

// Empty for an id nobody registered AND for an orphan -- an item whose mod is
// gone. A mod walking the table is asking what a player can be handed, and an
// orphan cannot be handed to anybody (ItemRegistry::end_reload).
const char *host_item_name(void *ud, int id)
{
    ServerState *s = static_cast<ServerState *>(ud);
    if (id <= 0 || (size_t)id >= s->items.size() || s->items.is_orphan((ItemId)id)) return "";
    return s->items.get((ItemId)id).name.c_str();
}

int host_item_stack(void *ud, int id)
{
    ServerState *s = static_cast<ServerState *>(ud);
    if (id <= 0 || (size_t)id >= s->items.size()) return 0;
    return (int)s->items.max_stack((ItemId)id);
}

// --- panels, from a mod's side ----------------------------------------------
//
// The same five the client gives a mod, against this world's PanelSet. What a
// panel here does that one in the single-process client does not is cross a
// socket: net::PanelShow carries it to every player, and net::UiClick brings a
// press back. See M7_FINDINGS 11 for what it does NOT do -- a panel belongs to
// the world and not to a player, so everybody in it sees the same one.
const char *asking_mod()
{
    const ModManifest *m = mod_host_current();
    return m ? m->name.c_str() : "";
}

// What muted players handed the mods: a command's words, a page's field, a
// panel's text, a line from their own mod (hold_if_muted). A mod that says
// them again -- now, or later from a tick: a letter delivered, a note pinned
// on a board -- says them to nobody but their author: in chat, in a notice,
// on a page. Every room's, since a mod may pass words on through the shared
// storage; kept while the author is muted and for an hour after the words.
struct MutedWords {
    admin::Who               who;
    std::vector<std::string> words;   // lowercased
    int64_t                  at = 0;
};
std::deque<MutedWords> g_muted_words;

std::vector<std::string> lower_words(const std::string &text)
{
    std::vector<std::string> out;
    std::string              w;
    for (const char c : text + " ") {
        if (std::isspace((unsigned char)c)) {
            if (!w.empty()) out.push_back(std::move(w));
            w.clear();
        } else {
            w += (char)std::tolower((unsigned char)c);
        }
    }
    return out;
}

void note_muted_words(const admin::Who &who, const std::string &text)
{
    std::vector<std::string> w = lower_words(text);
    if (w.empty()) return;
    g_muted_words.push_back({who, std::move(w), (int64_t)std::time(nullptr)});
    if (g_muted_words.size() > 1000) g_muted_words.pop_front();
}

// Whose muted words `text` carries, "" for nobody's (or only the reader's own).
// Three of their words in a row, twelve letters or more -- or all they said,
// when that is less -- are theirs. ponytail: what they said in under six
// letters passes, and somebody else repeating their phrase within the hour
// is held too; a mod that relays knows better and asks core.is_muted.
std::string muted_author(const std::string &text, const std::string &reader)
{
    if (g_muted_words.empty()) return "";
    const int64_t     now = (int64_t)std::time(nullptr);
    std::string       out;
    for (const std::string &w : lower_words(text)) out += " " + w;
    out += " ";
    std::string why;
    for (auto it = g_muted_words.begin(); it != g_muted_words.end();) {
        const bool live = now - it->at < 3600 || admin::muted(it->who, why);
        if (!live) {
            it = g_muted_words.erase(it);
            continue;
        }
        const MutedWords &m = *it++;
        if (m.who.name == reader) continue;
        size_t total = 0;
        for (const std::string &w : m.words) total += w.size();
        if (total < 6) continue;
        const size_t want_letters = std::min<size_t>(12, total), want_words = std::min<size_t>(3, m.words.size());
        for (size_t i = 0; i < m.words.size(); ++i) {
            std::string run = m.words[i];
            size_t      letters = run.size(), j = i + 1;
            while ((letters < want_letters || j - i < want_words) && j < m.words.size()) {
                run += " " + m.words[j];
                letters += m.words[j++].size();
            }
            if (letters < want_letters || j - i < want_words) break;
            if (out.find(" " + run + " ") != std::string::npos) return m.who.name;
        }
    }
    return "";
}

// A line for `player` (0: everybody) carrying a muted player's words goes to
// its author alone, if they are here. True when it was held.
bool hold_muted_line(ServerState *s, int player, const std::string &text, bool notice)
{
    const net::Session *to     = player > 0 ? s->sessions->find((net::PeerId)player) : nullptr;
    const std::string   author = muted_author(text, to ? to->name : "");
    if (author.empty()) return false;
    if (player == 0)
        for (const net::PeerId peer : s->sessions->ready_peers())
            if (const net::Session *p = s->sessions->find(peer); p && p->name == author)
                s->mod_says.push_back(ServerState::ModSay{(int)peer, 0, text, notice, !notice});
    std::printf("        held a mod's line with %s's words (muted)\n", author.c_str());
    return true;
}

// A line in a player's chat (core.chat), queued with the notices.
int host_chat(void *ud, int player, const char *text)
{
    ServerState *s = static_cast<ServerState *>(ud);
    if (!text || s->sessions == nullptr || std::strlen(text) > net::kMaxWireModText) return 0;
    if (player < 0 || (player > 0 && !s->sessions->find((net::PeerId)player))) return 0;
    if (s->muted_caller && player != s->muted_caller) {
        if (!s->muted_why.empty())
            s->mod_says.push_back(ServerState::ModSay{s->muted_caller, 0, s->muted_why, false, true});
        s->muted_why.clear();   // said once
        return 1;
    }
    if (hold_muted_line(s, player, text, false)) return 1;

    s->mod_says.push_back(ServerState::ModSay{player, 0, text, false, true});
    return 1;
}

admin::Who admin_who(const net::Session &p, const std::string *room = nullptr);   // below the rooms

int host_command(void *ud, const char *name, const char *help, const char *perm)
{
    ServerState *s = static_cast<ServerState *>(ud);
    if (!name || !net::chat_command_name_ok(name) || net::chat_builtin(name)) return 0;
    if (admin::is_command(name) || std::strcmp(name, "page") == 0 || std::strcmp(name, "admin") == 0)
        return 0;
    const std::string owner = asking_mod();
    const auto        it    = s->commands.find(name);
    if (it != s->commands.end() && it->second.owner != owner) return 0;
    admin::Role need;
    if (perm && *perm && !admin::perm_role(perm, need)) {
        std::printf("[mod] %s: command /%s: no permission called '%s'\n", owner.c_str(), name,
                    perm);
        return 0;
    }

    s->commands[name] = net::ChatCommand{owner, help ? help : "", perm ? perm : ""};
    return 1;
}

// core.has_perm and core.role: what a player here may do (src/server/admin.h).
int host_has_perm(void *ud, int player, const char *perm)
{
    ServerState *s = static_cast<ServerState *>(ud);
    const net::Session *p = s->sessions ? s->sessions->find((net::PeerId)player) : nullptr;
    return p && perm && admin::may(admin_who(*p), perm) ? 1 : 0;
}

const char *host_role(void *ud, int player)
{
    ServerState *s = static_cast<ServerState *>(ud);
    const net::Session *p = s->sessions ? s->sessions->find((net::PeerId)player) : nullptr;
    return p ? admin::role_name(admin::role_of(admin_who(*p))) : "";
}

// A body moved and turned by a mod (core.player_place): queued, and sent with
// the tick's other news once its block changes have gone.
int host_ride(void *ud, int player, int host, double x, double y, double z)
{
    ServerState *s = static_cast<ServerState *>(ud);
    if (!s->sessions) return 0;
    net::SessionAction a = s->sessions->ride((net::PeerId)player, (net::PeerId)std::max(host, 0),
                                             Vec3{(float)x, (float)y, (float)z},
                                             s->now);
    if (a.reply.empty()) {
        const std::string name = asking_mod();
        std::printf("[mod] %s: ride: refused -- player %d cannot be carried on %d there\n",
                    name.empty() ? "?" : name.c_str(), player, host);
        return 0;
    }
    s->mod_rides.emplace_back((net::PeerId)player, std::move(a));
    return 1;
}

int host_player_launch(void *ud, int player, double vx, double vy, double vz, double seconds)
{
    ServerState      *s    = static_cast<ServerState *>(ud);
    const std::string name = asking_mod();
    const char       *who  = name.empty() ? "?" : name.c_str();

    const net::Session *sess = s->sessions ? s->sessions->find((net::PeerId)player) : nullptr;
    if (!sess || sess->state != net::SessionState::Ready || sess->scope != net::Scope::Enter) {
        std::printf("[mod] %s: player_launch: refused -- there is no player %d with a body in "
                    "this world\n", who, player);
        return 0;
    }
    const float m = net::Push::kMax;
    if (!std::isfinite(vx) || !std::isfinite(vy) || !std::isfinite(vz) || !std::isfinite(seconds) ||
        std::fabs(vx) > m || std::fabs(vy) > m || std::fabs(vz) > m || seconds < 0.0 || seconds > 10.0) {
        std::printf("[mod] %s: player_launch: refused -- a speed over %.0f m/s on an axis, or a "
                    "grace outside 0..10 s\n", who, (double)m);
        return 0;
    }
    s->mod_launches.push_back({(net::PeerId)player, Vec3{(float)vx, (float)vy, (float)vz}, seconds});
    return 1;
}

int host_mob_velocity(void *ud, int id, double vx, double vy, double vz)
{
    ServerState *s = static_cast<ServerState *>(ud);
    if (!std::isfinite(vx) || !std::isfinite(vy) || !std::isfinite(vz)) return 0;
    return s->sessions ? cnp::ecs::mob_velocity(s->sessions->ecs(), (uint32_t)id, (float)vx, (float)vy,
                                                (float)vz)
                       : 0;
}

int host_mob_unseen(void *ud, int id, int player)
{
    ServerState *s = static_cast<ServerState *>(ud);
    if (player < 0) return 0;
    return s->sessions ? cnp::ecs::mob_unseen(s->sessions->ecs(), (uint32_t)id, (uint32_t)player) : 0;
}

int host_player_place(void *ud, int player, double x, double y, double z, double lx, double ly,
                      double lz)
{
    ServerState      *s    = static_cast<ServerState *>(ud);
    const std::string name = asking_mod();
    const char       *who  = name.empty() ? "?" : name.c_str();

    const net::Session *sess = s->sessions ? s->sessions->find((net::PeerId)player) : nullptr;
    if (!sess || sess->state != net::SessionState::Ready || sess->scope != net::Scope::Enter) {
        std::printf("[mod] %s: player_place: refused -- there is no player %d with a body in "
                    "this world\n", who, player);
        return 0;
    }
    if (!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(z) || !std::isfinite(lx) ||
        !std::isfinite(ly) || !std::isfinite(lz)) {
        std::printf("[mod] %s: player_place: refused -- not a number\n", who);
        return 0;
    }

    ServerState::ModPlace p;
    p.player = (net::PeerId)player;
    p.at     = Vec3{(float)x, (float)y, (float)z};
    p.look   = Vec3{(float)lx, (float)ly, (float)lz};
    if (length(p.look) > 1e-6f) p.look = normalize(p.look);
    s->mod_places.push_back(p);
    return 1;
}

// A body laid down or got up by a mod (core.player_lie): put there and told,
// with the tick's other seats.
int host_player_lie(void *ud, int player, int on, double x, double y, double z, double hx,
                    double hz)
{
    ServerState *s = static_cast<ServerState *>(ud);
    if (!s->sessions) return 0;
    const float head = std::atan2((float)hx, (float)-hz);
    net::SessionAction a = s->sessions->lie((net::PeerId)player, on != 0,
                                            Vec3{(float)x, (float)y, (float)z}, head, s->now);
    if (a.reply.empty()) {
        const std::string name = asking_mod();
        std::printf("[mod] %s: player_lie: refused -- there is no player %d with a body here "
                    "to lay down\n", name.empty() ? "?" : name.c_str(), player);
        return 0;
    }
    s->mod_rides.emplace_back((net::PeerId)player, std::move(a));
    return 1;
}

// A clip a body plays for everybody else (core.player_clip), sent with it.
int host_player_clip(void *ud, int player, const char *clip, double dx, double dz)
{
    ServerState *s    = static_cast<ServerState *>(ud);
    net::Session *sess = s->sessions ? s->sessions->find_mutable((net::PeerId)player) : nullptr;
    const std::string c = clip ? clip : "";
    if (!sess || !sess->avatar.is_alive() || c.size() > net::kMaxClipName) {
        const std::string name = asking_mod();
        std::printf("[mod] %s: player_clip: refused -- no player %d with a body here, or a name "
                    "over %zu bytes\n", name.empty() ? "?" : name.c_str(), player, net::kMaxClipName);
        return 0;
    }
    const bool faced = std::isfinite(dx) && std::isfinite(dz) && (dx != 0.0 || dz != 0.0);
    if (c.empty()) sess->avatar.remove<ecs::Posed>();
    else sess->avatar.set<ecs::Posed>({c, faced ? (float)std::atan2(dx, -dz) : 0.0f, faced});
    sess->pose_owed = true;
    return 1;
}

// A model of a mod's own a body wears (core.player_wear), sent with it.
int host_player_wear(void *ud, int player, int slot, int model, int tint)
{
    ServerState *s    = static_cast<ServerState *>(ud);
    net::Session *sess = s->sessions ? s->sessions->find_mutable((net::PeerId)player) : nullptr;
    if (!sess || !sess->avatar.is_alive() || slot < 1 || slot > ecs::kDressSlots || model < 0 ||
        (size_t)model > s->models.size() || tint < 0 ||
        tint > 0x7FFF) {
        const std::string name = asking_mod();
        std::printf("[mod] %s: player_wear: refused -- no player %d with a body here, a slot "
                    "out of 1..%d or a model nobody registered\n",
                    name.empty() ? "?" : name.c_str(), player, ecs::kDressSlots);
        return 0;
    }
    ecs::Dressed d = sess->avatar.has<ecs::Dressed>() ? sess->avatar.get<ecs::Dressed>() : ecs::Dressed{};
    d.model[slot - 1] = (uint16_t)model;
    d.tint[slot - 1]  = model != 0 ? (uint16_t)tint : 0;
    sess->avatar.set<ecs::Dressed>(d);
    sess->pose_owed = true;
    return 1;
}

// A sound for everybody here (core.play_sound): queued, sent with the seats.
int host_play_sound(void *ud, const char *name, double x, double y, double z)
{
    ServerState *s = static_cast<ServerState *>(ud);
    net::Sound   snd;
    snd.name = name ? name : "";
    snd.x = (float)x; snd.y = (float)y; snd.z = (float)z;
    if (!net::sound_name_ok(snd.name) || !std::isfinite(snd.x) || !std::isfinite(snd.y) ||
        !std::isfinite(snd.z)) {
        const std::string mod = asking_mod();
        std::printf("[mod] %s: play_sound: refused -- '%s' is not \"mod:thing\" at a place\n",
                    mod.empty() ? "?" : mod.c_str(), snd.name.c_str());
        return 0;
    }
    if (s->mod_sounds.size() < 256) s->mod_sounds.push_back(snd.encode());
    return 1;
}

bool panel_ok(bool ok, const std::string &err)
{
    if (!ok) std::printf("[mod] ui: %s\n", err.c_str());
    return ok;
}

// A channel of this mod's own (DESIGN.md 6.5).
//
// Asked for while the mod loads, because the number is handed out in the
// handshake and the handshake happens before any player exists. A mod that
// asked afterwards would get a number the clients already connected were never
// told about, so this refuses once the socket is up and says why.
int host_net_open(void *ud)
{
    ServerState      *s    = static_cast<ServerState *>(ud);
    const std::string name = asking_mod();
    if (name.empty()) return 0;

    for (size_t i = 0; i < s->mod_channels.size(); ++i)
        if (s->mod_channels[i] == name) return (int)(net::kModChannelBase + i);

    if (s->sessions) {
        std::printf("[mod] %s: net.open: too late -- this world is already listening, and the "
                    "clients on it were told the channels at the handshake\n", name.c_str());
        return 0;
    }
    if (s->mod_channels.size() >= (size_t)(net::kMaxChannels - net::kModChannelBase)) {
        std::printf("[mod] %s: net.open: this build has room for %d mod channels\n", name.c_str(),
                    (int)(net::kMaxChannels - net::kModChannelBase));
        return 0;
    }

    s->mod_channels.push_back(name);
    return (int)(net::kModChannelBase + s->mod_channels.size() - 1);
}

// How a body moves in this world, from a mod (TODO.md OPEN-44). The client's
// copy, with the same refusals, is host_movement in src/client/main.cpp.
//
// Too late is the moment the mod directory has finished loading, which is the
// moment a stage is too late as well -- and not the moment the socket opens, as
// it is for a channel: an offline run never opens one, and a rule that holds
// only on a server that listens is a rule a mod author meets in play.
int host_movement(void *ud, double walk, double sprint, double fly, double jump, double gravity)
{
    ServerState      *s    = static_cast<ServerState *>(ud);
    const std::string name = asking_mod();
    const char       *who  = name.empty() ? "?" : name.c_str();

    if (s->mapgen_finished) {
        std::printf("[mod] %s: movement: too late -- a world says how a body moves while its "
                    "mods load, and the numbers go out in the welcome\n", who);
        return 0;
    }

    MovementRules rules;
    rules.walk    = (float)walk;
    rules.sprint  = (float)sprint;
    rules.fly     = (float)fly;
    rules.jump    = (float)jump;
    rules.gravity = (float)gravity;
    keep_body(rules, s->movement);

    const std::string why = movement_rules_problem(rules);
    if (!why.empty()) {
        std::printf("[mod] %s: movement: refused -- %s\n", who, why.c_str());
        return 0;
    }

    if (!s->movement_by.empty() && s->movement_by != name)
        std::printf("[mod] %s: movement: replaces what %s declared\n", who,
                    s->movement_by.c_str());

    s->movement    = rules;
    s->movement_by = name;
    std::printf("[mod] %s: movement: walk %.2f, sprint %.2f, fly %.2f, jump %.2f, gravity %.2f\n",
                who, (double)rules.walk, (double)rules.sprint, (double)rules.fly,
                (double)rules.jump, (double)rules.gravity);
    return 1;
}

// Whether bodies in this world drive, from a mod (core.drive). The same rule
// and the same moment as core.movement, into the same numbers, out in the same
// welcome. The client's copy is host_drive in src/client/main.cpp.
int host_drive(void *ud, double accel, double brake, double turn, double drag)
{
    ServerState      *s    = static_cast<ServerState *>(ud);
    const std::string name = asking_mod();
    const char       *who  = name.empty() ? "?" : name.c_str();

    if (s->mapgen_finished) {
        std::printf("[mod] %s: drive: too late -- a world says how a body moves while its mods "
                    "load, and the numbers go out in the welcome\n", who);
        return 0;
    }

    MovementRules rules = s->movement;
    rules.accel = (float)accel;
    rules.brake = (float)brake;
    rules.turn  = (float)turn;
    rules.drag  = (float)drag;

    const std::string why = movement_rules_problem(rules);
    if (!why.empty()) {
        std::printf("[mod] %s: drive: refused -- %s\n", who, why.c_str());
        return 0;
    }

    s->movement = rules;
    std::printf("[mod] %s: drive: accel %.2f, brake %.2f, turn %.2f, drag %.2f\n", who,
                (double)rules.accel, (double)rules.brake, (double)rules.turn, (double)rules.drag);
    return 1;
}

// How high a body's eye is in this world, from a mod (core.eye_height), with
// the refusals core.body_width has: while the mods load, and in the body.
int host_eye_height(void *ud, double height)
{
    ServerState      *s    = static_cast<ServerState *>(ud);
    const std::string name = asking_mod();
    const char       *who  = name.empty() ? "?" : name.c_str();

    if (s->mapgen_finished) {
        std::printf("[mod] %s: eye_height: too late -- a world says how a body moves while its "
                    "mods load, and the numbers go out in the welcome\n", who);
        return 0;
    }
    MovementRules rules = s->movement;
    rules.eye           = (float)height;
    const std::string why = movement_rules_problem(rules);
    if (!why.empty()) {
        std::printf("[mod] %s: eye_height: refused -- %s\n", who, why.c_str());
        return 0;
    }
    s->movement = rules;
    std::printf("[mod] %s: eye_height: %.2f\n", who, (double)rules.eye);
    return 1;
}

// Whether bodies in this world do parkour, from a mod (core.parkour): the
// moment core.eye_height has.
int host_ledges(void *ud, int on)
{
    ServerState *s = static_cast<ServerState *>(ud);
    if (s->mapgen_finished) return 0;
    s->movement.ledges = on != 0;
    return 1;
}

int host_parkour(void *ud, int on)
{
    ServerState      *s    = static_cast<ServerState *>(ud);
    const std::string name = asking_mod();
    const char       *who  = name.empty() ? "?" : name.c_str();

    if (s->mapgen_finished) {
        std::printf("[mod] %s: parkour: too late -- a world says how a body moves while its "
                    "mods load, and the numbers go out in the welcome\n", who);
        return 0;
    }
    s->movement.parkour = on != 0;
    std::printf("[mod] %s: parkour: %s\n", who, on ? "on" : "off");
    return 1;
}

// How wide a body is in this world, from a mod (core.body_width). The same rule
// and the same moment as core.drive. The client's copy is host_body_width in
// src/client/main.cpp.
int host_body_width(void *ud, double width, double length)
{
    ServerState      *s    = static_cast<ServerState *>(ud);
    const std::string name = asking_mod();
    const char       *who  = name.empty() ? "?" : name.c_str();

    if (s->mapgen_finished) {
        std::printf("[mod] %s: body_width: too late -- a world says how a body moves while its "
                    "mods load, and the numbers go out in the welcome\n", who);
        return 0;
    }

    MovementRules rules = s->movement;
    rules.width  = (float)width;
    rules.length = (float)length;

    const std::string why = movement_rules_problem(rules);
    if (!why.empty()) {
        std::printf("[mod] %s: body_width: refused -- %s\n", who, why.c_str());
        return 0;
    }

    s->movement = rules;
    std::printf("[mod] %s: body_width: %.2f, length %.2f\n", who, (double)rules.width,
                (double)rules.length);
    return 1;
}

// One body's movement, from a mod (TODO.md OPEN-47). Any time, unlike
// core.movement: it is one body and not the welcome, and that body is told by a
// message of its own. Refused: numbers this engine cannot run, a player who is
// not here, and somebody with no body -- an observer has nothing to move.
int host_body_movement(void *ud, int player, double walk, double sprint, double fly,
                       double jump, double gravity, double rise)
{
    ServerState      *s    = static_cast<ServerState *>(ud);
    const std::string name = asking_mod();
    const char       *who  = name.empty() ? "?" : name.c_str();

    MovementRules rules;
    rules.walk    = (float)walk;
    rules.sprint  = (float)sprint;
    rules.fly     = (float)fly;
    rules.jump    = (float)jump;
    rules.gravity = (float)gravity;
    rules.rise    = (float)rise;

    // What this body IS stays what it was: the world's, or the kart
    // core.body_drive gave this one. Kept from the SESSION and not from the
    // world, or a hold put on a driver -- five zeroes for a countdown -- would
    // take the kart away from them and hand back a body on foot.
    const net::Session *body =
        (player > 0 && s->sessions != nullptr) ? s->sessions->find((net::PeerId)player) : nullptr;
    keep_body(rules, body != nullptr ? body->movement : s->movement);
    if (body != nullptr) {   // and a territory's factor on top (core.player_factors)
        rules.walk   *= body->walk_factor;
        rules.sprint *= body->walk_factor;
    }

    const std::string why = movement_rules_problem(rules);
    if (!why.empty()) {
        std::printf("[mod] %s: body_movement: refused -- %s\n", who, why.c_str());
        return 0;
    }
    // Somebody who may not fly (core.player_fly) keeps this speed aside.
    if (auto held = s->fly_held.find(player); held != s->fly_held.end()) {
        held->second = rules.fly;
        rules.fly    = 0.0f;
    }
    if (player <= 0 || s->sessions == nullptr ||
        !s->sessions->set_body_movement((net::PeerId)player, rules, s->now)) {
        std::printf("[mod] %s: body_movement: refused -- there is no player %d with a body in "
                    "this world\n", who, player);
        return 0;
    }

    std::printf("[mod] %s: body_movement: player %d walk %.2f, sprint %.2f, fly %.2f, jump %.2f, "
                "gravity %.2f, rise %.2f\n",
                who, player, (double)rules.walk, (double)rules.sprint, (double)rules.fly,
                (double)rules.jump, (double)rules.gravity, (double)limits_for(rules).rise);
    return 1;
}

// core.player_fly: a player's flying speed set aside, or given back.
int host_player_fly(void *ud, int player, int allowed)
{
    ServerState *s = static_cast<ServerState *>(ud);
    const net::Session *body =
        (player > 0 && s->sessions != nullptr) ? s->sessions->find((net::PeerId)player) : nullptr;
    if (body == nullptr) return 0;
    MovementRules rules = body->movement;
    auto held = s->fly_held.find(player);
    if (allowed) {
        if (held == s->fly_held.end()) return 1;
        rules.fly = held->second;
        s->fly_held.erase(held);
    } else {
        // Kept aside once; a body that flies again all the same (an id a
        // new connection was given) is grounded again.
        if (held == s->fly_held.end()) s->fly_held[player] = rules.fly;
        else if (rules.fly <= 0.0f) return 1;
        rules.fly = 0.0f;
    }
    return s->sessions->set_body_movement((net::PeerId)player, rules, s->now) ? 1 : 0;
}

// One body in a kart, or back on foot (core.body_drive). core.drive and
// core.body_width for a single player, at any time: the body keeps its speeds
// and changes what it is, and the client is told by the message
// core.body_movement sends. The client's copy is host_body_drive in
// src/client/main.cpp.
int host_body_drive(void *ud, int player, double accel, double brake, double turn, double drag,
                    double width, double length)
{
    ServerState      *s    = static_cast<ServerState *>(ud);
    const std::string name = asking_mod();
    const char       *who  = name.empty() ? "?" : name.c_str();

    const net::Session *body =
        (player > 0 && s->sessions != nullptr) ? s->sessions->find((net::PeerId)player) : nullptr;
    if (body == nullptr) {
        std::printf("[mod] %s: body_drive: refused -- there is no player %d with a body in this "
                    "world\n", who, player);
        return 0;
    }

    MovementRules rules = body->movement;   // its speeds are its own and stay
    rules.accel  = (float)accel;
    rules.brake  = (float)brake;
    rules.turn   = (float)turn;
    rules.drag   = (float)drag;
    rules.width  = (float)width;
    rules.length = (float)length;

    const std::string why = movement_rules_problem(rules);
    if (!why.empty()) {
        std::printf("[mod] %s: body_drive: refused -- %s\n", who, why.c_str());
        return 0;
    }
    if (!s->sessions->set_body_movement((net::PeerId)player, rules, s->now)) {
        std::printf("[mod] %s: body_drive: refused -- player %d has no body to move\n", who,
                    player);
        return 0;
    }

    std::printf("[mod] %s: body_drive: player %d accel %.2f, brake %.2f, turn %.2f, drag %.2f, "
                "%.2f by %.2f\n",
                who, player, (double)rules.accel, (double)rules.brake, (double)rules.turn,
                (double)rules.drag, (double)rules.width, (double)rules.length);
    return 1;
}

// One body in a boat, or out of it (core.body_boat). The client's copy is
// host_body_boat in src/client/main.cpp.
int host_body_boat(void *ud, int player, int on)
{
    ServerState        *s    = static_cast<ServerState *>(ud);
    const net::Session *body =
        (player > 0 && s->sessions != nullptr) ? s->sessions->find((net::PeerId)player) : nullptr;
    if (body == nullptr) return 0;
    MovementRules rules = body->movement;
    rules.boat          = on != 0;
    return s->sessions->set_body_movement((net::PeerId)player, rules, s->now) ? 1 : 0;
}

// One body in a cart, or out of it (core.body_cart), as host_body_boat.
int host_body_cart(void *ud, int player, int on)
{
    ServerState        *s    = static_cast<ServerState *>(ud);
    const net::Session *body =
        (player > 0 && s->sessions != nullptr) ? s->sessions->find((net::PeerId)player) : nullptr;
    if (body == nullptr) return 0;
    MovementRules rules = body->movement;
    rules.cart          = on != 0;
    return s->sessions->set_body_movement((net::PeerId)player, rules, s->now) ? 1 : 0;
}

int host_net_send(void *ud, int player, const char *text)
{
    ServerState      *s    = static_cast<ServerState *>(ud);
    const std::string name = asking_mod();
    if (name.empty() || !text) return 0;

    int channel = -1;
    for (size_t i = 0; i < s->mod_channels.size(); ++i)
        if (s->mod_channels[i] == name) channel = (int)(net::kModChannelBase + i);
    if (channel < 0) {
        std::printf("[mod] %s: net.send: this mod has no channel -- call core.net.open() while "
                    "it loads\n", name.c_str());
        return 0;
    }
    if (std::strlen(text) > net::kMaxWireModText) {
        std::printf("[mod] %s: net.send: %zu bytes, more than the %zu one message carries\n",
                    name.c_str(), std::strlen(text), net::kMaxWireModText);
        return 0;
    }

    s->mod_says.push_back(ServerState::ModSay{player, (uint8_t)channel, text});
    return 1;
}

int host_storage_set(void *ud, const char *key, const char *value)
{
    ServerState      *s    = static_cast<ServerState *>(ud);
    const std::string name = asking_mod();
    std::string       why;
    if (s->mod_store.set(name, key ? key : "", value ? value : "", why)) return 1;
    std::printf("[mod] %s: storage_set: refused -- %s\n", name.c_str(), why.c_str());
    return 0;
}

const char *host_storage_get(void *ud, const char *key)
{
    ServerState *s     = static_cast<ServerState *>(ud);
    s->storage_scratch = s->mod_store.get(asking_mod(), key ? key : "");
    return s->storage_scratch.c_str();
}

// core.shared_storage_*: the first room's store, which that room saves; on a
// server of one world, this world's. One room runs at a time (TurnLock), so
// another room's store is safe to touch from here.
ModStore &shared_store(ServerState *s);   // below the rooms

int host_shared_storage_set(void *ud, const char *key, const char *value)
{
    ServerState      *s    = static_cast<ServerState *>(ud);
    const std::string name = asking_mod();
    std::string       why;
    if (shared_store(s).set(name, key ? key : "", value ? value : "", why)) return 1;
    std::printf("[mod] %s: shared_storage_set: refused -- %s\n", name.c_str(), why.c_str());
    return 0;
}

const char *host_shared_storage_get(void *ud, const char *key)
{
    ServerState *s     = static_cast<ServerState *>(ud);
    s->storage_scratch = shared_store(s).get(asking_mod(), key ? key : "");
    return s->storage_scratch.c_str();
}

// A sentence on a player's screen (TODO.md OPEN-11). Queued with what mods say
// on their channels, for the reason those are: a host function has no socket.
int host_notice(void *ud, int player, const char *text)
{
    ServerState      *s    = static_cast<ServerState *>(ud);
    const std::string name = asking_mod();
    if (!text || s->sessions == nullptr) return 0;
    if (std::strlen(text) > net::kMaxWireModText) {
        std::printf("[mod] %s: notice: %zu bytes, more than the %zu one carries\n", name.c_str(),
                    std::strlen(text), net::kMaxWireModText);
        return 0;
    }
    if (player < 0 || (player > 0 && !s->sessions->find((net::PeerId)player))) return 0;
    if (hold_muted_line(s, player, text, true)) return 1;

    s->mod_says.push_back(ServerState::ModSay{player, 0, text, true});
    return 1;
}

int host_ecs_component(void *ud, const char *name, const char *const *fields, int count,
                       char *err, size_t err_size)
{
    ServerState             *s = static_cast<ServerState *>(ud);
    std::vector<std::string> f;
    for (int i = 0; i < count; ++i) f.push_back(fields && fields[i] ? fields[i] : "");

    std::string why;
    if (s->lua_ecs.component(name ? name : "", f, asking_mod(), why)) return 1;
    std::snprintf(err, err_size, "%s", why.c_str());
    return 0;
}

int host_ecs_phase(void *ud, const char *name, int order, char *err, size_t err_size)
{
    ServerState *s = static_cast<ServerState *>(ud);
    std::string  why;
    if (s->lua_ecs.phase(name ? name : "", order, asking_mod(), why)) return 1;
    std::snprintf(err, err_size, "%s", why.c_str());
    return 0;
}

int host_ecs_system(void *ud, const char *name, const char *phase, const char *component,
                    int handler_ref, char *err, size_t err_size)
{
    ServerState *s = static_cast<ServerState *>(ud);
    std::string  why;
    if (s->lua_ecs.system(name ? name : "", phase ? phase : "", component ? component : "",
                          mod_host_current_vm(), handler_ref, asking_mod(), why))
        return 1;
    std::snprintf(err, err_size, "%s", why.c_str());
    return 0;
}

// A stage of world generation, from a mod (TODO OPEN-35; DESIGN.md 8, M3 asked
// for "stage registrabili" and only C++ could register one).
//
// Refused after the pass has run, and the sentence says which of the two orders
// went wrong: a mod asking while it loads is in time, a mod asking from a tick
// handler is not. The alternative -- taking the stage and never running it --
// is a mod that does nothing and is never told why.
// A modifier a mod registers. Both kinds go through one function because the
// refusals are the same ones and the Modifiers class is what tells them apart:
// what differs is which of its two lists the thing lands in.
int register_modifier(void *ud, const char *name, const char *const *nodes, int count,
                      double interval, int chance, int handler_ref, bool is_abm, char *err,
                      size_t err_size)
{
    ServerState *s = static_cast<ServerState *>(ud);

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
    const bool  ok = is_abm ? s->modifiers.register_abm(std::move(m), error)
                            : s->modifiers.register_lbm(std::move(m), error);
    if (!ok) {
        std::snprintf(err, err_size, "%s", error.c_str());
        return 0;
    }
    return 1;
}

int host_register_abm(void *ud, const char *name, const char *const *nodes, int count,
                      double interval, int chance, int handler_ref, char *err, size_t err_size)
{
    return register_modifier(ud, name, nodes, count, interval, chance, handler_ref, true, err,
                             err_size);
}

int host_register_lbm(void *ud, const char *name, const char *const *nodes, int count,
                      int handler_ref, char *err, size_t err_size)
{
    return register_modifier(ud, name, nodes, count, 0.0, 1, handler_ref, false, err, err_size);
}

int host_register_mapgen(void *ud, const char *name, int handler_ref, char *err, size_t err_size)
{
    ServerState      *s     = static_cast<ServerState *>(ud);
    const std::string stage = name ? name : "";

    if (stage.empty()) {
        std::snprintf(err, err_size, "a mapgen stage needs a name");
        return 0;
    }
    if (s->mapgen_finished) {
        std::snprintf(err, err_size,
                      "'%s' asked for a mapgen stage after the world was generated; a stage "
                      "has to be registered while the mod loads",
                      stage.c_str());
        return 0;
    }

    // By name, because the name is what a log and a profile call it, and two
    // stages sharing one would be two lines nobody can tell apart.
    for (const ServerState::LuaStage &st : s->mapgen_stages) {
        if (st.name == stage) {
            std::snprintf(err, err_size, "a mapgen stage called '%s' is already registered by %s",
                          stage.c_str(), st.owner.empty() ? "the engine" : st.owner.c_str());
            return 0;
        }
    }

    ServerState::LuaStage st;
    st.name  = stage;
    st.owner = asking_mod();
    st.vm    = mod_host_current_vm();
    st.ref   = handler_ref;
    s->mapgen_stages.push_back(st);
    return 1;
}

// Where the generator put the ground in that column: the y of the first cell of
// AIR above it. Answered from the context the core stages were run against, so
// a mod and the terrain cannot hold two opinions about it.
int host_terrain_height(void *ud, int x, int z)
{
    ServerState *s = static_cast<ServerState *>(ud);
    if (!s->mapgen_ctx) return 0;
    return mapgen_terrain_height(*s->mapgen_ctx, x, z);
}

// Where the water stands over a column (core.water_level), from the same context.
int host_water_level(void *ud, int x, int z)
{
    ServerState *s = static_cast<ServerState *>(ud);
    if (!s->mapgen_ctx) return 0;
    return mapgen_water_level(*s->mapgen_ctx, x, z);
}

// Which climate the generator gave that column, by name. From the same context,
// for the same reason core.terrain_height is.
const char *host_biome(void *ud, int x, int z)
{
    ServerState *s = static_cast<ServerState *>(ud);
    if (!s->mapgen_ctx || !s->mapgen_ctx->biomes) return "";
    const BiomeSet &set = *s->mapgen_ctx->biomes;
    return set.at(mapgen_biome_at(set, s->mapgen_ctx->seed, x, z)).name.c_str();
}

// A block a body swims in. Re-registered under its own name, which keeps its id,
// the way node_model changes what a block is drawn as.
int host_node_liquid(void *ud, int id_in, int opacity, int fill)
{
    ServerState  *s  = static_cast<ServerState *>(ud);
    const BlockId id = (BlockId)id_in;
    if (id == kAir || id >= s->registry.size()) return 0;

    NodeDef d   = s->registry.get(id);
    d.solid     = false;
    d.opaque    = false;
    d.liquid    = true;
    d.opacity   = (uint8_t)std::clamp(opacity, 0, 254);
    d.liquid_fill = (uint8_t)std::clamp(fill, 1, 8);
    d.breakable = false;   // there is nothing to dig out of a lake
    s->registry.register_node(d);
    return 1;
}

// core.node_glass: seen through, like glass or ice, and light passes.
int host_node_glass(void *ud, int id_in, int opacity)
{
    ServerState  *s  = static_cast<ServerState *>(ud);
    const BlockId id = (BlockId)id_in;
    if (id == kAir || id >= s->registry.size()) return 0;
    NodeDef d = s->registry.get(id);
    d.opaque  = false;
    d.opacity = (uint8_t)std::clamp(opacity, 0, 254);   // 255 would be drawn by nobody
    s->registry.register_node(d);
    return 1;
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
    ServerState *s = static_cast<ServerState *>(ud);
    if (to < 0 || (size_t)to >= s->registry.size()) return 0;
    return replace_in_box(
        x1, y1, z1, x2, y2, z2, from, to, round != 0,
        [&](int x, int y, int z) { return (int)s->world.get_block(x, y, z); },
        [&](int x, int y, int z) { host_set_block(ud, x, y, z, to); });
}

// core.repair_held: an anvil's work on the hand's stack.
int host_repair_held(void *ud, int player, int points)
{
    ServerState *s = static_cast<ServerState *>(ud);
    if (!s->sessions) return -1;
    net::Session *sess = s->sessions->find_mutable((net::PeerId)player);
    if (!sess || sess->scope == net::Scope::Observe) return -1;
    ItemStack &held = sess->bag().slot(sess->bag().selected());
    const int  was  = held.empty() ? 0 : held.used;
    held.used       = (uint16_t)std::max(0, was - std::max(0, points));
    if (was > 0) s->bags_touched.insert((uint32_t)player);
    return was;
}

// core.node_falls: a block that falls when nothing solid is under it.
int host_node_falls(void *ud, int id_in, int on)
{
    ServerState  *s  = static_cast<ServerState *>(ud);
    const BlockId id = (BlockId)id_in;
    if (id == kAir || id >= s->registry.size()) return 0;
    NodeDef d = s->registry.get(id);
    d.falls   = on != 0;
    s->registry.register_node(d);
    return 1;
}

// core.fall: the block in that cell comes out of it and falls (ecs::Falling).
int host_fall(void *ud, int x, int y, int z)
{
    ServerState *s = static_cast<ServerState *>(ud);
    if (!s->sessions) return 0;
    const BlockId id = s->world.get_block(x, y, z);
    if (id == kAir || id >= s->registry.size() || !s->registry.get(id).breakable) return 0;
    const uint8_t facing = s->world.get_facing(x, y, z);
    if (s->world.get_block(x, y, z) != id) return 0;
    write_told(*s, x, y, z, kAir, 0);
    if (s->world.get_block(x, y, z) != kAir) return 0;   // the column is not taking writes
    cnp::ecs::spawn_falling(s->sessions->ecs(), id, facing, x, y, z);
    return 1;
}

double host_ecs_spawn(void *ud, const char *component, double x, double y, double z)
{
    ServerState *s = static_cast<ServerState *>(ud);
    return s->lua_ecs.spawn(component ? component : "", x, y, z);
}

int host_ecs_set(void *ud, double id, const char *field, double value)
{
    ServerState *s = static_cast<ServerState *>(ud);
    return s->lua_ecs.set(id, field ? field : "", value) ? 1 : 0;
}

int host_ecs_get(void *ud, double id, const char *field, double *out)
{
    ServerState *s = static_cast<ServerState *>(ud);
    if (!out) return 0;
    return s->lua_ecs.get(id, field ? field : "", *out) ? 1 : 0;
}

int host_ecs_kill(void *ud, double id)
{
    ServerState *s = static_cast<ServerState *>(ud);
    return s->lua_ecs.kill(id) ? 1 : 0;
}

int host_ecs_visible(void *ud, double id, int on)
{
    ServerState *s = static_cast<ServerState *>(ud);
    return s->lua_ecs.visible(id, on != 0) ? 1 : 0;
}

int host_ecs_model(void *ud, double id, int model)
{
    ServerState *s = static_cast<ServerState *>(ud);
    if (model < 0 || (size_t)model > s->models.size()) return 0;
    return s->lua_ecs.model(id, (uint16_t)model) ? 1 : 0;
}

int host_ecs_health(void *ud, double id, int set)
{
    return static_cast<ServerState *>(ud)->lua_ecs.health(id, set);
}

int host_ecs_clip(void *ud, double id, const char *name)
{
    return static_cast<ServerState *>(ud)->lua_ecs.clip(id, name ? name : "") ? 1 : 0;
}

int host_ecs_box(void *ud, double id, double half, double height)
{
    return static_cast<ServerState *>(ud)->lua_ecs.box(id, half, height) ? 1 : 0;
}

// core.craft: a mod's visible thing made a machine (core/ecs/machine.h).
flecs::entity craft_thing(ServerState *s, double id)
{
    if (!s->sessions || id <= 0.0) return flecs::entity();
    flecs::entity e = s->sessions->ecs().get_alive((flecs::entity_t)id);
    return e.is_valid() && e.has<cnp::ecs::Thing>() ? e : flecs::entity();
}

int host_craft_make(void *ud, double id, const char *kind, double mass, double half_w, double half_h,
                    double half_l, double accel, double top, double turn)
{
    ServerState  *s = static_cast<ServerState *>(ud);
    flecs::entity e = craft_thing(s, id);
    const std::string k = kind ? kind : "";
    cnp::CraftSpec spec;
    if (k == "wheeled") spec.kind = cnp::CraftKind::Wheeled;
    else if (k == "tracked") spec.kind = cnp::CraftKind::Tracked;
    else if (k == "rotor") spec.kind = cnp::CraftKind::Rotor;
    else if (k == "wing") spec.kind = cnp::CraftKind::Wing;
    else e = flecs::entity();
    for (const double v : {mass, half_w, half_h, half_l, accel, top, turn})
        if (!std::isfinite(v) || v <= 0.0) e = flecs::entity();
    if (!e.is_valid()) {
        const std::string name = asking_mod();
        std::printf("[mod] %s: craft.make: refused -- not a visible thing of this mod's, or kind '%s' "
                    "is none of wheeled, tracked, rotor, wing, or a number not above 0\n",
                    name.empty() ? "?" : name.c_str(), k.c_str());
        return 0;
    }
    spec.mass   = (float)mass;
    spec.half_w = (float)half_w;
    spec.half_h = (float)half_h;
    spec.half_l = (float)half_l;
    spec.accel  = (float)accel;
    spec.top    = (float)top;
    spec.turn   = (float)turn;
    spec.gravity = s->movement.gravity;
    const cnp::ecs::Position p = e.get<cnp::ecs::Position>();
    if (!e.has<cnp::ecs::Look>()) e.set<cnp::ecs::Look>({});
    // Now and not on its first step: step_machines sets it while it walks
    // every machine, and adding it there moved the entity out of the table
    // being walked (two machines made in one tick, SIGABRT in flecs).
    if (!e.has<cnp::ecs::Roll>()) e.set<cnp::ecs::Roll>({});
    const float yaw = e.get<cnp::ecs::Look>().yaw;
    cnp::ecs::Machine m;
    m.sim  = cnp::craft_make(spec, Vec3{p.x, p.y, p.z}, yaw, s->world, s->registry);
    m.pose = cnp::craft_pose(*m.sim);
    e.set<cnp::ecs::Machine>(std::move(m));
    e.remove<cnp::ecs::Mounted>();
    return 1;
}

int host_craft_keys(void *ud, double id, double forward, double right, double up, double yaw)
{
    flecs::entity e = craft_thing(static_cast<ServerState *>(ud), id);
    cnp::ecs::Machine *m = e.is_valid() ? e.try_get_mut<cnp::ecs::Machine>() : nullptr;
    if (!m) return 0;
    m->keys = cnp::CraftKeys{(float)forward, (float)right, (float)up, (float)yaw};
    return 1;
}

int host_craft_pose(void *ud, double id, double *out)
{
    flecs::entity e = craft_thing(static_cast<ServerState *>(ud), id);
    cnp::ecs::Machine *m = e.is_valid() ? e.try_get_mut<cnp::ecs::Machine>() : nullptr;
    if (!m) return 0;
    const cnp::CraftPose &p = m->pose;
    const double v[13] = {p.base.x, p.base.y, p.base.z, p.yaw, p.pitch, p.roll, p.velocity.x,
                          p.velocity.y, p.velocity.z, p.speed, p.grounded ? 1.0 : 0.0, m->impact,
                          p.throttle};
    for (int i = 0; i < 13; ++i) out[i] = v[i];
    m->impact = 0.0f;
    return 1;
}

int host_craft_push(void *ud, double id, double vx, double vy, double vz)
{
    flecs::entity e = craft_thing(static_cast<ServerState *>(ud), id);
    cnp::ecs::Machine *m = e.is_valid() ? e.try_get_mut<cnp::ecs::Machine>() : nullptr;
    if (!m || !m->sim) return 0;
    cnp::craft_push(*m->sim, Vec3{(float)vx, (float)vy, (float)vz});
    return 1;
}

int host_craft_mount(void *ud, double id, double on, double x, double y, double z)
{
    ServerState  *s = static_cast<ServerState *>(ud);
    flecs::entity e = craft_thing(s, id);
    if (!e.is_valid() || e.has<cnp::ecs::Machine>()) return 0;
    if (on <= 0.0) {
        e.remove<cnp::ecs::Mounted>();
        return 1;
    }
    flecs::entity carrier = craft_thing(s, on);
    if (!carrier.is_valid() || carrier.id() == e.id()) return 0;
    for (const double v : {x, y, z})
        if (!std::isfinite(v) || std::fabs(v) > 16.0) return 0;
    cnp::ecs::Mounted m;
    if (const cnp::ecs::Mounted *was = e.try_get<cnp::ecs::Mounted>()) m = *was;
    m.on = carrier.id();
    m.at = Vec3{(float)x, (float)y, (float)z};
    e.set<cnp::ecs::Mounted>(m);
    if (!e.has<cnp::ecs::Look>()) e.set<cnp::ecs::Look>({});
    return 1;
}

int host_craft_aim(void *ud, double id, double yaw, double pitch)
{
    flecs::entity e = craft_thing(static_cast<ServerState *>(ud), id);
    if (!e.is_valid() || e.has<cnp::ecs::Machine>() || !std::isfinite(yaw) || !std::isfinite(pitch))
        return 0;
    if (cnp::ecs::Mounted *m = e.try_get_mut<cnp::ecs::Mounted>()) {
        m->yaw   = (float)yaw;
        m->pitch = (float)pitch;
        return 1;
    }
    cnp::ecs::Look l{(float)yaw, (float)pitch};
    e.set<cnp::ecs::Look>(l);
    return 1;
}

int host_craft_seat(void *ud, int player, double id, double x, double y, double z)
{
    ServerState *s = static_cast<ServerState *>(ud);
    if (!s->sessions || !craft_thing(s, id).is_valid()) return 0;
    net::SessionAction a = s->sessions->ride_thing((net::PeerId)player, (uint64_t)id,
                                                   Vec3{(float)x, (float)y, (float)z}, s->now);
    if (a.reply.empty()) {
        const std::string name = asking_mod();
        std::printf("[mod] %s: craft.seat: refused -- player %d cannot be carried there\n",
                    name.empty() ? "?" : name.c_str(), player);
        return 0;
    }
    s->mod_rides.emplace_back((net::PeerId)player, std::move(a));
    return 1;
}

int host_ecs_count(void *ud, const char *component)
{
    ServerState *s = static_cast<ServerState *>(ud);
    return s->lua_ecs.count(component ? component : "");
}

int host_find_nodes(void *ud, int x1, int y1, int z1, int x2, int y2, int z2, const char *name,
                    int *out, int max)
{
    ServerState *s = static_cast<ServerState *>(ud);
    std::string  why;
    const int    n = find_nodes(s->world, s->registry, x1, y1, z1, x2, y2, z2, name, out, max, why);

    // A refusal says why in the LOG and not back in Lua, the same way a panel's
    // does: the generated binding hands back the list and there is nowhere in
    // it for a sentence. Named rather than hidden -- a mod that searched a box
    // too big gets an empty list, which is also what a box with nothing in it
    // gives, and the difference is only in this line.
    if (!why.empty()) std::printf("[mod] %s: find_nodes: %s\n", asking_mod(), why.c_str());
    return n;
}

// The way a body of that size would walk, for a mod: see core/voxel/pathfind.h.
int host_find_path(void *ud, int x1, int y1, int z1, int x2, int y2, int z2, int height,
                   int step_up, int drop, int swim, int max_nodes, double hurry, int smooth,
                   int *out, int max)
{
    ServerState *s = static_cast<ServerState *>(ud);
    PathBody body;
    body.height    = height < 1 ? 1 : height > 8 ? 8 : height;
    body.step_up   = step_up < 0 ? 0 : step_up > 4 ? 4 : step_up;
    body.drop      = drop < 0 ? 0 : drop > 32 ? 32 : drop;
    body.swim      = swim != 0;
    body.max_nodes = max_nodes < 1 ? 1 : max_nodes > 20000 ? 20000 : max_nodes;
    body.hurry     = (float)(hurry < 1.0 ? 1.0 : hurry > 4.0 ? 4.0 : hurry);
    const PathResult r = find_path(s->world, s->registry, PathCell{x1, y1, z1}, PathCell{x2, y2, z2}, body);
    int gx = x1, gy = y1, gz = z1;   // where the way starts, on the ground as find_path took it
    path_ground(s->world, s->registry, gx, gy, gz, body);
    const std::vector<PathCell> cells =
        smooth ? path_smooth(s->world, s->registry, PathCell{gx, gy, gz}, r.cells, body) : r.cells;
    int n = 0;
    for (const PathCell &c : cells) {
        if (n + 3 > max) break;
        out[n++] = c.x;
        out[n++] = c.y;
        out[n++] = c.z;
    }
    return n;
}

int host_ui_define(void *ud, const char *name, float w, float h)
{
    ServerState *s = static_cast<ServerState *>(ud);
    std::string  err;
    return panel_ok(s->panels.define(name ? name : "", asking_mod(), w, h, err), err) ? 1 : 0;
}

int host_ui_add(void *ud, ui::Element e, const char *panel)
{
    ServerState *s = static_cast<ServerState *>(ud);
    std::string  err;
    return panel_ok(s->panels.add(panel ? panel : "", asking_mod(), e, err), err) ? 1 : 0;
}

int host_ui_label(void *ud, const char *panel, float x, float y, const char *text)
{
    ui::Element e;
    e.kind = ui::ElemKind::Label;
    e.text = text ? text : "";
    e.x = x; e.y = y; e.h = 1.0f;
    return host_ui_add(ud, e, panel);
}

int host_ui_button(void *ud, const char *panel, const char *id, float x, float y, float w,
                   float h, const char *text)
{
    ui::Element e;
    e.kind = ui::ElemKind::Button;
    e.id   = id ? id : "";
    e.text = text ? text : "";
    e.x = x; e.y = y; e.w = w; e.h = h;
    return host_ui_add(ud, e, panel);
}

int host_ui_item(void *ud, const char *panel, const char *id, float x, float y, float w,
                 float h, const char *item)
{
    ui::Element e;
    e.kind = ui::ElemKind::Item;
    e.id   = id ? id : "";
    e.text = item ? item : "";
    e.x = x; e.y = y; e.w = w; e.h = h;
    return host_ui_add(ud, e, panel);
}

int host_ui_field(void *ud, const char *panel, const char *id, float x, float y, float w,
                  float h, const char *text)
{
    ui::Element e;
    e.kind = ui::ElemKind::Field;
    e.id   = id ? id : "";
    e.text = text ? text : "";
    e.x = x; e.y = y; e.w = w; e.h = h;
    return host_ui_add(ud, e, panel);
}

int host_ui_box(void *ud, const char *panel, float x, float y, float w, float h, int r, int g,
                int b)
{
    ui::Element e;
    e.kind = ui::ElemKind::Box;
    e.x = x; e.y = y; e.w = w; e.h = h;
    e.r = (uint8_t)(r < 0 ? 0 : (r > 255 ? 255 : r));
    e.g = (uint8_t)(g < 0 ? 0 : (g > 255 ? 255 : g));
    e.b = (uint8_t)(b < 0 ? 0 : (b > 255 ? 255 : b));
    return host_ui_add(ud, e, panel);
}

int host_ui_model(void *ud, const char *panel, float x, float y, float w, float h, int model, int tint,
                  const char *pieces)
{
    return host_ui_add(ud, ui::model_element(x, y, w, h, model, tint, pieces), panel);
}

int host_ui_show(void *ud, const char *name, int open, int player)
{
    ServerState *s = static_cast<ServerState *>(ud);
    std::string  err;

    // Nobody named means EVERY player in this world -- a notice, not a chest.
    // A mod that means one person passes the id the events gave it.
    const ui::PlayerId viewer = player <= 0 ? ui::kAllPlayers : (ui::PlayerId)player;

    return panel_ok(s->panels.show(name ? name : "", asking_mod(), open != 0, viewer, err), err)
               ? 1
               : 0;
}

// An edit the session layer agreed to, actually happening.
//
// Split from net::ServerSessions::plan_edit on purpose -- see net::EditPlan.
// That decides WHETHER: scope, reach, what is there, what is in the hand. This
// is the doing, and it lives here because it needs the two things core/net has
// no business holding: a writable world and a mod host.
//
// The order matters and it is the same one the single-process client uses: the
// cancellable event first, the write second, the notification third. A mod that
// cancels costs the player nothing, because nothing has been spent yet -- the
// caller only calls edit_done() if this returns true.
//
// `plan.want` is updated to what actually went down: a pre_place handler may
// rewrite the id, and the block the world got is the block everybody has to be
// told about.
// The node meta field that says a player put this block on owned ground.
constexpr const char *kPlacedKey = "core:placed";

bool apply_edit(ServerState &s, net::PeerId who, net::EditPlan &plan, std::string &why)
{
    script_field fields[5] = {
        {"x",  SCRIPT_V_NUMBER, (double)plan.x, nullptr, 0},
        {"y",  SCRIPT_V_NUMBER, (double)plan.y, nullptr, 0},
        {"z",  SCRIPT_V_NUMBER, (double)plan.z, nullptr, 0},
        {"id", SCRIPT_V_NUMBER, (double)(plan.breaking ? plan.had : plan.want), nullptr, 0},
        {"player", SCRIPT_V_NUMBER, (double)who, nullptr, 0},
    };

    bool        cancelled = false;
    std::string err;
    const char *before = plan.breaking ? "core:pre_break" : "core:pre_place";
    if (!s.mods.events().emit(before, fields, 5, &cancelled, err)) {
        // A broken emit must not silently block the player -- the same rule the
        // client's try_place follows, and for the same reason: a mod that
        // errors is a mod's problem and not a world nobody can build in.
        std::fprintf(stderr, "emit %s: %s\n", before, err.c_str());
        cancelled = false;
    }
    if (cancelled) {
        why = net::refused_by_mod(plan.breaking);
        return false;
    }

    if (!plan.breaking) plan.want = (BlockId)fields[3].num;

    // On somebody's ground (core.owned_area) a block a player put there is
    // theirs: marked in the cell, read before breaking takes the mark away.
    const Cell here{plan.x, plan.y, plan.z};
    const bool owned_ground = s.sessions && s.sessions->owned_tag(plan.x, plan.y, plan.z) != 0;
    if (plan.breaking) plan.own = !s.node_meta.get(here, kPlacedKey).empty();

    // A track piece is all its cells or none: put down on the grid with its
    // entry over the cell aimed at, and taken away whole by breaking any one of
    // them -- which is what the bag gets.
    BlockId        piece  = kAir;
    const NodeDef &placed = s.registry.get(plan.want);
    if (!plan.breaking && placed.piece.plan != 0) {
        int ox, oz;
        piece_origin(placed.piece, plan.facing, plan.x, plan.z, ox, oz);
        if (!put_piece(s, plan.want, plan.facing, ox, plan.y, oz)) {
            why = "a track piece cannot go over another one";
            return false;
        }
    } else if (plan.breaking && take_piece(s, plan.x, plan.y, plan.z, piece)) {
        plan.had  = piece;
        plan.drop = s.items.of_block(piece);
    } else {
        // The same write a mod's set_block makes, and it has to be: two ways
        // to change a block are two sets of consequences that agree until one
        // of them is edited. What differs is only who is told and when, below.
        write_block(s, plan.x, plan.y, plan.z, plan.want, plan.breaking ? (uint8_t)0 : plan.facing);
    }

    if (!plan.breaking && owned_ground) {
        std::string why_not;
        s.node_meta.set(here, kPlacedKey, "1", why_not);
    }

    script_field after_fields[5] = {
        {"x",  SCRIPT_V_NUMBER, (double)plan.x, nullptr, 0},
        {"y",  SCRIPT_V_NUMBER, (double)plan.y, nullptr, 0},
        {"z",  SCRIPT_V_NUMBER, (double)plan.z, nullptr, 0},
        {"id", SCRIPT_V_NUMBER, (double)(plan.breaking ? plan.had : plan.want), nullptr, 0},
        {"player", SCRIPT_V_NUMBER, (double)who, nullptr, 0},
    };
    bool ignored = false;
    s.mods.events().emit(plan.breaking ? "core:broken" : "core:placed", after_fields, 5,
                         &ignored, err);
    return true;
}

// --- containers, from a mod's side ------------------------------------------
//
// A chest belongs to a CELL and not to a player, so all four of these take a
// position. Only the showing takes a player, and that is the whole distinction:
// where the chest is is world state, who is looking into it is not.

int host_recipe_shaped(void *ud, const char *makes, int count, const char *row1,
                       const char *row2, const char *row3)
{
    ServerState *s = static_cast<ServerState *>(ud);
    std::string  err;
    if (s->recipes.add_shaped(asking_mod(), makes ? makes : "",
                              (uint16_t)(count < 0 ? 0 : count > 65535 ? 65535 : count),
                              row1 ? row1 : "", row2 ? row2 : "", row3 ? row3 : "", err)) {
        return 1;
    }
    std::printf("[mod] recipe: %s\n", err.c_str());
    return 0;
}

int host_recipe_shapeless(void *ud, const char *makes, int count, const char *items)
{
    ServerState *s = static_cast<ServerState *>(ud);
    std::string  err;
    if (s->recipes.add_shapeless(asking_mod(), makes ? makes : "",
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
    if (static_cast<ServerState *>(ud)->recipes.add_dyeable(item ? item : "", err)) return 1;
    std::printf("[mod] recipe_dye: %s\n", err.c_str());
    return 0;
}

int host_item_dye(void *ud, const char *item, int dye)
{
    std::string err;
    if (static_cast<ServerState *>(ud)->recipes.add_dye(item ? item : "", dye, err)) return 1;
    std::printf("[mod] item_dye: %s\n", err.c_str());
    return 0;
}

int host_item_returns(void *ud, const char *item, const char *back)
{
    if (!item || !back || !*item || !*back) return 0;
    static_cast<ServerState *>(ud)->recipes.add_returns(item, back);
    return 1;
}

int host_drop_item(void *ud, int x, int y, int z, int item, int count)
{
    ServerState *s = static_cast<ServerState *>(ud);
    if (s->sessions == nullptr) return 0;
    if ((ItemId)item == kNoItem || count <= 0 || count > 65535) return 0;

    ItemStack stack;
    stack.item  = (ItemId)item;
    stack.count = (uint16_t)count;

    cnp::ecs::spawn_drop(s->sessions->ecs(), stack, (float)x + 0.5f, (float)y + 0.5f,
                         (float)z + 0.5f, s->now);
    return 1;
}

int host_container_spill(void *ud, int x, int y, int z, int from)
{
    ServerState      *s = static_cast<ServerState *>(ud);
    const ContainerAt at{x, y, z};

    Inventory *bag = s->containers.find(at);
    if (bag == nullptr) return 0;

    // Each stack as it was -- its wear, enchantments, colours and number --
    // and, on somebody's ground, theirs (core.owned_area).
    const uint32_t owner = s->sessions ? s->sessions->owned_tag(x, y, z) : 0;
    int fell = 0;
    for (int i = from < 0 ? 0 : from; i < bag->size(); ++i) {
        ItemStack out = bag->take_from(i);
        if (out.empty() || s->sessions == nullptr) continue;
        if ((out.tag & kTagIdMask) == 0) out.tag |= owner;
        cnp::ecs::spawn_drop(s->sessions->ecs(), out, (float)x + 0.5f, (float)y + 0.5f,
                             (float)z + 0.5f, s->now);
        ++fell;
    }

    // From a slot on: those slots are empty now, and the container ends there.
    if (from > 0 && from < bag->size()) {
        bag->resize(from);
        s->containers.touched(at);
    }

    // Emptied, and still there. Taking the chest away is container_clear, a
    // separate act -- and separate because a mod may want to empty something it
    // is not destroying.
    if (fell > 0) s->containers.touched(at);
    return fell;
}

int host_craft_show(void *ud, int x, int y, int z, int player)
{
    ServerState *s = static_cast<ServerState *>(ud);

    // A table for nobody is not a table, the same as a chest: two people may
    // stand at one, and each of them gets their own grid -- what is in it
    // belongs to the player and not to the block.
    if (player <= 0 || s->sessions == nullptr) return 0;

    return s->sessions->open_craft((net::PeerId)player, ContainerAt{x, y, z}, s->now) ? 1 : 0;
}

int host_craft_close(void *ud, int player)
{
    ServerState *s = static_cast<ServerState *>(ud);
    if (player <= 0 || s->sessions == nullptr) return 0;

    s->sessions->close_craft((net::PeerId)player, s->now);
    return 1;
}

int host_damage(void *ud, int player, int amount)
{
    ServerState *s = static_cast<ServerState *>(ud);
    if (player <= 0 || s->sessions == nullptr) return 0;
    if (amount <= 0) return (int)s->sessions->health_of((net::PeerId)player);

    const int asked = amount > 65535 ? 65535 : amount;
    return (int)s->sessions->hurt((net::PeerId)player, (uint16_t)asked, s->now);
}

int host_heal(void *ud, int player, int amount)
{
    ServerState *s = static_cast<ServerState *>(ud);
    if (player <= 0 || s->sessions == nullptr) return 0;
    if (amount <= 0) return (int)s->sessions->health_of((net::PeerId)player);

    const int asked = amount > 65535 ? 65535 : amount;
    return (int)s->sessions->heal((net::PeerId)player, (uint16_t)asked);
}

int host_health(void *ud, int player)
{
    ServerState *s = static_cast<ServerState *>(ud);
    if (player <= 0 || s->sessions == nullptr) return 0;

    return (int)s->sessions->health_of((net::PeerId)player);
}

int host_container_make(void *ud, int x, int y, int z, int slots)
{
    ServerState      *s = static_cast<ServerState *>(ud);
    const ContainerAt at{x, y, z};
    std::string       err;
    if (s->containers.open(at, slots, err) == nullptr) {
        std::printf("[mod] container: %s\n", err.c_str());
        return 0;
    }
    return 1;
}

int host_container_add(void *ud, int x, int y, int z, int item, int count, const char *role)
{
    ServerState *s = static_cast<ServerState *>(ud);
    if (count <= 0 || count > 65535) return count < 0 ? 0 : count;

    // Nothing is not a thing that can be put in a chest, and Inventory::add
    // answers "none of it was left over" for it -- true, and the opposite of
    // what a caller asking what did not fit needs to hear.
    if ((ItemId)item == kNoItem) return count;

    const ContainerAt at{x, y, z};
    Inventory        *bag = s->containers.find(at);
    if (bag == nullptr) return count;   // no chest there: none of it fitted

    const unsigned roles = role_mask(role);
    if (roles == 0) return count;
    const uint16_t left = bag->add(s->items, (ItemId)item, (uint16_t)count, -1, 0, 0, 0, roles);
    if (left != (uint16_t)count) s->containers.touched(at);
    return (int)left;
}

int host_container_show(void *ud, int x, int y, int z, int slots, int player)
{
    ServerState *s = static_cast<ServerState *>(ud);

    // A chest for nobody is not a chest. Unlike a panel, where "nobody in
    // particular" means the world, there is no container the world is looking
    // into: two people reaching into one chest is the feature, and the world
    // reaching into one is not a thing that can happen.
    if (player <= 0 || s->sessions == nullptr) return 0;
    if (!host_container_make(ud, x, y, z, slots)) return 0;

    return s->sessions->open_container((net::PeerId)player, ContainerAt{x, y, z}) ? 1 : 0;
}

int host_container_close(void *ud, int player)
{
    ServerState *s = static_cast<ServerState *>(ud);
    if (player <= 0 || s->sessions == nullptr) return 0;

    s->sessions->close_container((net::PeerId)player);
    return 1;
}

int host_container_revision(void *ud, int x, int y, int z)
{
    ServerState *s = static_cast<ServerState *>(ud);
    return (int)(s->containers.revision(ContainerAt{x, y, z}) & 0x7fffffff);
}

int host_container_move(void *ud, int fx, int fy, int fz, int tx, int ty, int tz, int count)
{
    ServerState *s = static_cast<ServerState *>(ud);
    const ContainerAt from{fx, fy, fz}, to{tx, ty, tz};
    Inventory *a = s->containers.find(from), *b = s->containers.find(to);
    if (a == nullptr || b == nullptr || a == b || count <= 0) return 0;
    for (int slot = 0; slot < a->size(); ++slot) {
        if (a->at(slot).empty() || !((kRolesOut >> a->role(slot)) & 1u)) continue;   // what may come out
        const ItemStack out  = a->take_from(slot, (uint16_t)std::min(count, 65535));
        const uint16_t  left = b->add(s->items, out.item, out.count, -1, out.used, out.ench, out.tag);
        if (left > 0) {
            ItemStack &back = a->slot(slot);
            if (back.empty()) back = ItemStack{out.item, 0, out.used, out.ench, out.tag};
            back.count = (uint16_t)(back.count + left);
        }
        const int moved = (int)out.count - (int)left;
        if (moved > 0) {
            s->containers.touched(from);
            s->containers.touched(to);
        }
        return moved;
    }
    return 0;
}

int host_drop_at(void *ud, int x, int y, int z)
{
    ServerState *s = static_cast<ServerState *>(ud);
    return s->sessions ? (int)cnp::ecs::drop_in_cell(s->sessions->ecs(), x, y, z) : 0;
}

int host_drop_send(void *ud, int x, int y, int z, float tx, float ty, float tz)
{
    ServerState *s = static_cast<ServerState *>(ud);
    return s->sessions ? (int)cnp::ecs::send_drop(s->sessions->ecs(), x, y, z, tx, ty, tz) : 0;
}

int host_container_drop(void *ud, int x, int y, int z, float tx, float ty, float tz, int count)
{
    ServerState *s   = static_cast<ServerState *>(ud);
    Inventory   *bag = s->containers.find(ContainerAt{x, y, z});
    if (bag == nullptr || s->sessions == nullptr) return 0;
    const int n = cnp::ecs::drop_from(s->sessions->ecs(), *bag, count, tx, ty, tz, s->now);
    if (n > 0) s->containers.touched(ContainerAt{x, y, z});
    return n;
}

int host_container_item(void *ud, int x, int y, int z, int slot)
{
    const Inventory *bag = static_cast<ServerState *>(ud)->containers.find(ContainerAt{x, y, z});
    if (bag == nullptr || slot < 1 || slot > bag->size()) return 0;
    return (int)bag->at(slot - 1).item;
}

int host_collect_drops(void *ud, int x, int y, int z, int tx, int ty, int tz)
{
    ServerState *s = static_cast<ServerState *>(ud);
    Inventory *into = s->containers.find(ContainerAt{tx, ty, tz});
    if (into == nullptr || (s->sessions ? &s->sessions->ecs() : nullptr) == nullptr) return 0;
    int taken = 0;
    std::vector<flecs::entity> gone;
    (s->sessions ? &s->sessions->ecs() : nullptr)->each([&](flecs::entity e, const ecs::Position &p, ecs::Dropped &d) {
        if ((int)std::floor(p.x) != x || (int)std::floor(p.y) != y || (int)std::floor(p.z) != z)
            return;
        const uint16_t left = into->add(s->items, d.stack.item, d.stack.count, -1, d.stack.used,
                                        d.stack.ench, d.stack.tag);
        taken += (int)d.stack.count - (int)left;
        d.stack.count = left;
        if (left == 0) gone.push_back(e);
    });
    for (flecs::entity e : gone) e.destruct();
    if (taken > 0) s->containers.touched(ContainerAt{tx, ty, tz});
    return taken;
}

float host_container_fill(void *ud, int x, int y, int z)
{
    ServerState     *s = static_cast<ServerState *>(ud);
    const Inventory *c = s->containers.find(ContainerAt{x, y, z});
    if (c == nullptr || c->size() == 0) return -1.0f;
    double sum = 0.0;
    for (int slot = 0; slot < c->size(); ++slot) {
        const ItemStack &st = c->at(slot);
        if (!st.empty()) sum += (double)st.count / (double)s->items.max_stack(st.item);
    }
    return (float)(sum / (double)c->size());
}

int host_container_count(void *ud, int x, int y, int z, int item, const char *role)
{
    ServerState     *s = static_cast<ServerState *>(ud);
    const Inventory *c = s->containers.find(ContainerAt{x, y, z});
    if (c == nullptr) return 0;
    const unsigned roles = role_mask_any(role);
    int n = 0;
    for (int i = 0; i < c->size(); ++i)
        if (c->at(i).item == (ItemId)item && ((roles >> c->role(i)) & 1u)) n += c->at(i).count;
    return n;
}

int host_container_take(void *ud, int x, int y, int z, int item, int count, const char *role)
{
    ServerState      *s = static_cast<ServerState *>(ud);
    const ContainerAt at{x, y, z};
    Inventory        *c = s->containers.find(at);
    if (c == nullptr || count <= 0 || (ItemId)item == kNoItem) return 0;

    int taken = 0;
    for (int slot = 0; slot < c->size() && taken < count; ++slot) {
        const ItemStack &in = c->at(slot);
        if (in.empty() || in.item != (ItemId)item || !((role_mask_any(role) >> c->role(slot)) & 1u)) continue;

        const int want = count - taken;
        const int here = (int)in.count < want ? (int)in.count : want;
        if (c->take(slot, (uint16_t)here)) taken += here;
    }

    // Changed, so whoever is looking into it is told and the file keeps it.
    if (taken > 0) s->containers.touched(at);
    return taken;
}

int host_container_clear(void *ud, int x, int y, int z)
{
    ServerState      *s = static_cast<ServerState *>(ud);
    const ContainerAt at{x, y, z};

    const bool had = s->containers.find(at) != nullptr;

    // Through the session layer and not through the set: erasing the container
    // alone leaves everybody who was looking into it still pointing at that
    // cell, and the next chest built there opens by itself on their screen.
    if (s->sessions) s->sessions->erase_container(at);
    else s->containers.erase(at);

    return had ? 1 : 0;
}

int host_node_id(void *ud, const char *name)
{
    return (int)static_cast<ServerState *>(ud)->registry.find(name ? name : "");
}

// A model, and the file it is in. The file is resolved INSIDE the mod's own
// directory and nowhere else: a mod names "persona.gltf" and gets the one
// beside its init.luau, and a name with a slash or a leading dot in it is
// refused rather than followed. The same rule the texture pack manifest and the
// client's model store live by, because a mod is content too.
int host_register_model(void *ud, const char *name, const char *file)
{
    ServerState *s = static_cast<ServerState *>(ud);

    const std::string want = file ? file : "";
    if (!model_file_is_plain(want)) return 0;

    const ModManifest *m = mod_host_current();
    const std::string  path = m ? m->dir + "/" + want : want;
    return (int)s->models.add(name ? name : "", path);
}

int host_model_id(void *ud, const char *name)
{
    return (int)static_cast<ServerState *>(ud)->models.find(name ? name : "");
}

// What ONE body wears (core.body_model): the world's model for a single player,
// put on and taken off while they play. The client's copy is host_body_model in
// src/client/main.cpp.
int host_body_model(void *ud, int player, int model)
{
    ServerState      *s    = static_cast<ServerState *>(ud);
    const std::string name = asking_mod();
    const char       *who  = name.empty() ? "?" : name.c_str();

    // A model nobody registered would go out in an entity message the client
    // refuses -- the whole message, not the costume -- so it is refused here.
    if (model < 0 || (size_t)model > s->models.size()) {
        std::printf("[mod] %s: body_model: refused -- this world has no model %d\n", who, model);
        return 0;
    }
    if (player <= 0 || s->sessions == nullptr ||
        !s->sessions->set_body_model((net::PeerId)player, (uint16_t)model)) {
        std::printf("[mod] %s: body_model: refused -- there is no player %d with a body in this "
                    "world\n", who, player);
        return 0;
    }
    return 1;
}

void host_player_model(void *ud, int model)
{
    ServerState *s  = static_cast<ServerState *>(ud);
    s->player_model = (ModelId)(model < 0 ? 0 : model);

    // And the sessions, when there are any. A mod that says this while people
    // are standing in the world dresses them too, rather than only whoever
    // knocks next.
    if (s->sessions) s->sessions->set_player_model(s->player_model);
}

void host_world_sky(void *ud, int r, int g, int b)
{
    ServerState *s = static_cast<ServerState *>(ud);
    s->has_sky = true;
    s->sky[0]  = (uint8_t)std::clamp(r, 0, 255);
    s->sky[1]  = (uint8_t)std::clamp(g, 0, 255);
    s->sky[2]  = (uint8_t)std::clamp(b, 0, 255);
}

void host_world_storm(void *ud, int r, int g, int b)
{
    ServerState *s = static_cast<ServerState *>(ud);
    s->has_storm = true;
    s->storm[0]  = (uint8_t)std::clamp(r, 0, 255);
    s->storm[1]  = (uint8_t)std::clamp(g, 0, 255);
    s->storm[2]  = (uint8_t)std::clamp(b, 0, 255);
}

void host_mapgen_core(void *ud, int on)
{
    ServerState *s = static_cast<ServerState *>(ud);
    if (s->mapgen_finished) {
        std::printf("[mod] mapgen_core: too late -- the world is already made; only while the "
                    "mods load\n");
        return;
    }
    s->core_mapgen = on != 0;
}

void host_mapgen_only(void *ud, const char *mod)
{
    ServerState *s = static_cast<ServerState *>(ud);
    if (s->mapgen_finished) {
        std::printf("[mod] mapgen_only: too late -- the world is already made; only while the "
                    "mods load\n");
        return;
    }
    if (mod && *mod) s->mapgen_owners.insert(mod);
}

void host_mob_model(void *ud, int model)
{
    static_cast<ServerState *>(ud)->mob_model = (ModelId)(model < 0 ? 0 : model);
}

// A kind of creature. Registered again under the same name, it is replaced in
// its place, so a reloaded mod changes the creatures already walking about.
int host_register_mob(void *ud, const char *name, int model, int health, double speed,
                      const char *fight, int damage, double reach, double every,
                      const char *drop, int drop_count, int shot_model, int opens_doors,
                      double hover, int fireball, const char *spawn_on)
{
    ServerState *s = static_cast<ServerState *>(ud);
    cnp::ecs::MobKind k;
    std::string       why;
    if (!cnp::ecs::make_mob_kind(k, why, name, model, health, speed, fight, damage, reach, every,
                                 drop, drop_count, shot_model, opens_doors, hover, fireball, spawn_on, s->models.size())) {
        std::printf("[mod] register_mob('%s'): %s\n", name ? name : "?", why.c_str());
        return 0;
    }
    if (k.model != 0 && !cnp::ecs::size_mob_kind(k, s->models.get(k.model).file))
        std::printf("[mod] register_mob('%s'): its model has no \"death\" clip; it will fade out "
                    "standing (docs/MODDING.md, Creature)\n", k.name.c_str());
    return cnp::ecs::add_mob_kind(s->mob_kinds, k);
}

// What tames a pet (core.mob_tame); the item is found once every mod has loaded.
int host_mob_tame(void *ud, const char *name, const char *item, int tamed_model)
{
    ServerState *s = static_cast<ServerState *>(ud);
    for (cnp::ecs::MobKind &k : s->mob_kinds)
        if (k.name == (name ? name : "") && k.fight == cnp::ecs::MobKind::Fight::Pet) {
            k.tame_name  = item ? item : "";
            k.tame_model = (uint16_t)(tamed_model < 0 || (size_t)tamed_model > s->models.size() ? 0 : tamed_model);
            return 1;
        }
    return 0;
}

// What shears a creature, and what it lays (core.mob_shear, core.mob_lays).
int host_mob_shear(void *ud, const char *name, const char *item, const char *drop, int count,
                   int shorn_model, double regrow)
{
    ServerState *s = static_cast<ServerState *>(ud);
    return cnp::ecs::mob_shear(s->mob_kinds, name, item, drop, count,
                               (size_t)shorn_model > s->models.size() ? 0 : shorn_model, regrow);
}

int host_mob_lays(void *ud, const char *name, const char *item, double min_s, double max_s)
{
    return cnp::ecs::mob_lays(static_cast<ServerState *>(ud)->mob_kinds, name, item, min_s, max_s);
}

int host_push_mobs(void *ud, int x, int y, int z, int dx, int dy, int dz)
{
    ServerState *s = static_cast<ServerState *>(ud);
    return s->sessions ? cnp::ecs::push_mobs(s->sessions->ecs(), x, y, z, dx, dy, dz) : 0;
}

int host_monsters_near(void *ud, double x, double y, double z, double r)
{
    ServerState *s = static_cast<ServerState *>(ud);
    return s->sessions ? cnp::ecs::monsters_near(s->sessions->ecs(), s->mob_kinds, (float)x,
                                                 (float)y, (float)z, (float)r)
                       : 0;
}

int host_mobs_near(void *ud, double x, double y, double z, double r, float *out, int max)
{
    ServerState *s = static_cast<ServerState *>(ud);
    return s->sessions ? cnp::ecs::mobs_near(s->sessions->ecs(), s->mob_kinds, (float)x, (float)y,
                                             (float)z, (float)r, out, max)
                       : 0;
}

int host_mob_ids(void *ud, double x, double y, double z, double r, int all, int *out, int max)
{
    ServerState *s = static_cast<ServerState *>(ud);
    return s->sessions ? cnp::ecs::mob_ids(s->sessions->ecs(), s->mob_kinds, (float)x, (float)y,
                                           (float)z, (float)r, all != 0, out, max)
                       : 0;
}

int host_mob_pos(void *ud, int id, float *x, float *y, float *z)
{
    ServerState *s = static_cast<ServerState *>(ud);
    return s->sessions && cnp::ecs::mob_pos(s->sessions->ecs(), (uint32_t)id, x, y, z) ? 1 : 0;
}

int host_mob_blow_every(void *ud, int id, double factor)
{
    ServerState *s = static_cast<ServerState *>(ud);
    if (!std::isfinite(factor)) return 0;
    return s->sessions ? cnp::ecs::mob_blow_every(s->sessions->ecs(), (uint32_t)id, (float)factor) : 0;
}

int host_slow_mobs(void *ud, double x, double y, double z, double r, double factor, double seconds)
{
    ServerState *s = static_cast<ServerState *>(ud);
    return s->sessions ? cnp::ecs::slow_mobs(s->sessions->ecs(), s->mob_kinds, (float)x, (float)y,
                                             (float)z, (float)r, (float)factor, seconds, s->now)
                       : 0;
}

int host_mob_drop(void *ud, const char *name, const char *item, int max)
{
    return cnp::ecs::mob_drop(static_cast<ServerState *>(ud)->mob_kinds, name, item, max);
}

int host_mob_sight(void *ud, const char *name, double nodes)
{
    return cnp::ecs::mob_sight(static_cast<ServerState *>(ud)->mob_kinds, name, nodes);
}

// A mod's bullet (core.shoot): the wall first, then any body nearer than it.
int host_shoot(void *ud, double x, double y, double z, double dx, double dy, double dz,
               double max, int damage, float *distance, int *killed)
{
    ServerState *s = static_cast<ServerState *>(ud);
    const Vec3   dir{(float)dx, (float)dy, (float)dz};
    if (!s->sessions || length(dir) < 1e-6f) return 0;
    const Vec3   from{(float)x, (float)y, (float)z};
    const Vec3   d   = normalize(dir);
    float        far = (float)std::clamp(max, 0.0, 256.0);
    const RaycastHit wall = raycast(s->world, s->registry, from, d, far);
    if (wall.hit) far = wall.distance;
    cnp::ecs::ShotHit hit;
    if (!cnp::ecs::shoot_bodies(s->sessions->ecs(), from, d, far,
                                (uint16_t)std::clamp(damage, 1, 1000), hit))
        return 0;
    *distance = hit.distance;
    *killed   = hit.killed ? 1 : 0;
    return 1;
}

// core.shoot_arrow: from the eye the movement authority accepted, along the
// look that came with it, and a use of what is in the hand.
int host_shoot_arrow(void *ud, int player, double speed, int damage, int model, const char *item)
{
    ServerState *s = static_cast<ServerState *>(ud);
    if (!s->sessions) return 0;
    net::Session *sess = s->sessions->find_mutable((net::PeerId)player);
    if (!sess || sess->scope != net::Scope::Enter || !sess->has_moved) return 0;
    const MovementState &at = sess->authority();
    float lx, ly, lz;
    sess->interest.look(lx, ly, lz);
    const Vec3 look{lx, ly, lz};
    if (!at.valid || length(look) < 1e-6f) return 0;

    const Vec3   from{at.x, at.y + s->movement.eye, at.z};
    const ItemId picked = item && *item ? s->items.find(item) : kNoItem;
    cnp::ecs::loose_player_arrow(s->sessions->ecs(), from + normalize(look) * 0.3f,
                                 normalize(look) * (float)std::clamp(speed, 0.0, 100.0),
                                 (uint16_t)std::clamp(damage, 1, 1000),
                                 (uint16_t)(model < 0 || (size_t)model > s->models.size() ? 0 : model),
                                 sess->avatar.id(), picked == kNoItem ? 0 : picked, s->now);
    if (sess->bag().wear_slot(s->items, sess->bag().selected(), 1))
        s->bags_touched.insert((uint32_t)player);
    return 1;
}

int host_blast(void *ud, double x, double y, double z, double r, int damage)
{
    ServerState *s = static_cast<ServerState *>(ud);
    return s->sessions ? cnp::ecs::blast_bodies(s->sessions->ecs(),
                                                Vec3{(float)x, (float)y, (float)z},
                                                (float)std::clamp(r, 0.0, 32.0),
                                                (uint16_t)std::clamp(damage, 1, 1000))
                       : 0;
}

int host_mob_breed(void *ud, const char *name, const char *item, int baby_model, double grow,
                   int hearts_model)
{
    ServerState *s = static_cast<ServerState *>(ud);
    const auto model = [&](int m) { return (size_t)m > s->models.size() ? 0 : m; };
    return cnp::ecs::mob_breed(s->mob_kinds, name, item, model(baby_model), grow, model(hearts_model));
}

// One creature of a registered kind, standing at x y z.
int host_spawn_mob(void *ud, const char *name, double x, double y, double z)
{
    ServerState *s = static_cast<ServerState *>(ud);
    if (s->sessions == nullptr) return 0;
    for (size_t i = 0; i < s->mob_kinds.size(); ++i)
        if (s->mob_kinds[i].name == (name ? name : "")) {
            const flecs::entity e = cnp::ecs::spawn_mob(s->sessions->ecs(), s->mob_kinds,
                                                        (uint16_t)i, (float)x, (float)y, (float)z, s->now);
            return e.is_valid() ? (int)(uint32_t)e.id() : 0;
        }
    return 0;
}

int host_mob_goto(void *ud, int id, int x, int y, int z)
{
    ServerState *s = static_cast<ServerState *>(ud);
    return s->sessions ? cnp::ecs::mob_goto(s->sessions->ecs(), s->mob_kinds, (uint32_t)id, x, y, z) : 0;
}

int host_mob_going(void *ud, int id)
{
    ServerState *s = static_cast<ServerState *>(ud);
    return s->sessions ? cnp::ecs::mob_going(s->sessions->ecs(), (uint32_t)id) : 0;
}

int host_mob_act(void *ud, int id, const char *clip, const char *item)
{
    ServerState *s = static_cast<ServerState *>(ud);
    if (!s->sessions) return 0;
    const ItemId held = item && *item ? s->items.find(item) : kNoItem;
    return cnp::ecs::mob_act(s->sessions->ecs(), s->mob_kinds, (uint32_t)id, clip, held);
}

int host_mob_place(void *ud, int id, double x, double y, double z)
{
    ServerState *s = static_cast<ServerState *>(ud);
    return s->sessions ? cnp::ecs::mob_place(s->sessions->ecs(), (uint32_t)id, (float)x, (float)y, (float)z) : 0;
}

int host_mob_ride(void *ud, int id, double thing, double x, double y, double z)
{
    ServerState *s = static_cast<ServerState *>(ud);
    if (id <= 0) return 0;
    if (thing <= 0.0) return s->mob_rides.erase((uint32_t)id) > 0 ? 1 : 0;
    double probe = 0.0;
    if (!s->lua_ecs.get(thing, "x", probe)) return 0;
    s->mob_rides[(uint32_t)id] = ServerState::MobRide{thing, (float)x, (float)y, (float)z};
    return 1;
}

int host_mob_hunt(void *ud, int id, int player)
{
    ServerState *s = static_cast<ServerState *>(ud);
    return s->sessions && player >= 0 ? cnp::ecs::mob_hunt(s->sessions->ecs(), (uint32_t)id, (uint32_t)player) : 0;
}

int host_mob_attack(void *ud, int id, int mob)
{
    ServerState *s = static_cast<ServerState *>(ud);
    return s->sessions && mob >= 0 ? cnp::ecs::mob_attack(s->sessions->ecs(), (uint32_t)id, (uint32_t)mob) : 0;
}

int host_mob_pierce(void *ud, const char *name, double part)
{
    return cnp::ecs::mob_pierce(static_cast<ServerState *>(ud)->mob_kinds, name, part);
}

int host_mob_hurt(void *ud, int id, int damage, double from_x, double from_z)
{
    ServerState *s = static_cast<ServerState *>(ud);
    return s->sessions ? cnp::ecs::mob_hurt(s->sessions->ecs(), (uint32_t)id,
                                            (uint16_t)std::clamp(damage, 1, 1000), (float)from_x,
                                            (float)from_z)
                       : 0;
}

int host_mob_remove(void *ud, int id)
{
    ServerState *s = static_cast<ServerState *>(ud);
    return s->sessions ? cnp::ecs::mob_remove(s->sessions->ecs(), (uint32_t)id) : 0;
}

int host_body_player(void *ud, int id)
{
    ServerState *s = static_cast<ServerState *>(ud);
    if (!s->sessions) return 0;
    for (const net::PeerId peer : s->sessions->ready_peers())
        if (const net::Session *sess = s->sessions->find(peer);
            sess && sess->avatar.is_alive() && (uint32_t)sess->avatar.id() == (uint32_t)id)
            return (int)peer;
    return 0;
}

const char *host_mob_kind(void *ud, int id)
{
    ServerState *s = static_cast<ServerState *>(ud);
    static std::string name;
    name = s->sessions ? cnp::ecs::mob_kind_name(s->sessions->ecs(), s->mob_kinds, (uint32_t)id) : "";
    return name.c_str();
}

int host_mob_love(void *ud, int id)
{
    ServerState *s = static_cast<ServerState *>(ud);
    return s->sessions ? cnp::ecs::mob_love(s->sessions->ecs(), (uint32_t)id, s->now) : 0;
}

int host_mob_keep(void *ud, int id, int on)
{
    ServerState *s = static_cast<ServerState *>(ud);
    return s->sessions ? cnp::ecs::mob_keep(s->sessions->ecs(), (uint32_t)id, on != 0) : 0;
}

int host_mob_shear_now(void *ud, int id, int drop)
{
    ServerState *s = static_cast<ServerState *>(ud);
    return s->sessions ? cnp::ecs::mob_shear_now(s->sessions->ecs(), (uint32_t)id, s->now, drop != 0) : 0;
}

int host_mob_become(void *ud, int id, const char *kind)
{
    ServerState *s = static_cast<ServerState *>(ud);
    return s->sessions && kind ? cnp::ecs::mob_become(s->sessions->ecs(), s->mob_kinds, (uint32_t)id, kind) : 0;
}

int host_mob_voice(void *ud, const char *name, const char *voice)
{
    return name && voice ? cnp::ecs::mob_voice(static_cast<ServerState *>(ud)->mob_kinds, name, voice) : 0;
}

int host_mob_face(void *ud, int id, double x, double z)
{
    ServerState *s = static_cast<ServerState *>(ud);
    return s->sessions ? cnp::ecs::mob_face(s->sessions->ecs(), (uint32_t)id, (float)x, (float)z) : 0;
}

const char *host_node_yield(void *ud, int id)
{
    ServerState *s = static_cast<ServerState *>(ud);
    if (id <= 0 || (size_t)id >= s->registry.size()) return "";
    const ItemId item = s->items.of_block((BlockId)id);
    return item != kNoItem ? s->items.get(item).name.c_str() : "";
}

const char *host_node_name(void *ud, int id)
{
    ServerState *s = static_cast<ServerState *>(ud);
    if (id < 0 || (size_t)id >= s->registry.size()) return "";
    return s->registry.get((BlockId)id).name.c_str();
}

int host_event_define(void *ud, const char *event, const script_field *fields, int count,
                      int cancellable, char *err, size_t err_size)
{
    ServerState *s = static_cast<ServerState *>(ud);

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

int host_event_on(void *ud, const char *event, int priority, int handler_ref,
                  char *err, size_t err_size)
{
    ServerState *s = static_cast<ServerState *>(ud);
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
    ServerState *s = static_cast<ServerState *>(ud);

    bool        cancelled = false;
    std::string e;
    if (!s->mods.events().emit(event ? event : "", fields, count, &cancelled, e)) {
        std::snprintf(err, err_size, "%s", e.c_str());
        return 0;
    }
    if (out_cancelled) *out_cancelled = cancelled ? 1 : 0;
    return 1;
}

// How far away another player is still worth sending. Four chunks, the same
// distance chunks stream to: a player you cannot see the ground under is a
// player you do not need the position of.
constexpr float kEntityRadius = 4.0f * (float)kChunkSize;

double seconds_since(std::chrono::steady_clock::time_point t0)
{
    return std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
}

// What a tick may spend on the modifiers, of the fifty milliseconds it has.
// Two, which is the same shape of answer M9 gave the mesher: a budget the work
// is cut to rather than a hope that the work is small. A mod that asks for more
// than this gets its sweeps spread over more ticks, and the server stays on
// time -- which is the property a player feels.
constexpr double kModifierBudgetMs = 2.0;

// Opens a run of writes whose light is done at the end of it, and closes it.
//
// Around every place mod code can write cells, and nowhere else: what makes
// this safe is that the light is right again before anything reads it, and the
// things that read it are the chunk encoder and the mesher -- both of which run
// after the tick, never inside it.
struct LightRun {
    ServerState &s;

    explicit LightRun(ServerState &state) : s(state) { s.light_batching = true; }

    ~LightRun()
    {
        s.light_batching = false;
        if (s.light_batch.empty()) return;

        ++s.light_batch_flushes;
        s.light_batch_cells += (uint64_t)s.light_batch.flush(s.world, s.registry);
    }
};

// ---------------------------------------------------------- the world's columns
//
// TODO.md OPEN-35 and OPEN-38: how a column comes into memory, gets its light,
// and leaves it. core/world/columns.h decides when.

// Spent on columns in one tick, at most. A tick is 50 ms; a column costs about
// 5 to 20 to generate and light, so a player arriving somewhere new waits a few
// ticks for the ground rather than the server waiting for the player.
constexpr double kColumnBudgetMs = 20.0;

// Thirty seconds untouched before a column may be let go, and a look every
// second. A mod that writes into one keeps it: a beacon that blinks is a beacon
// somebody put there to be seen.
constexpr uint64_t kColumnIdleTicks   = 600;
constexpr uint64_t kColumnUnloadEvery = 20;

// A mod's stages, over one column. Returns how many failed.
size_t run_mod_stages(ServerState &s, ColumnPos column)
{
    if (s.mapgen_stages.empty() || !s.mapgen_ctx) return 0;

    mapgen_set_column(*s.mapgen_ctx, column);
    const MapgenContext &c = *s.mapgen_ctx;

    size_t failed = 0;
    s.generating  = true;
    for (const ServerState::LuaStage &st : s.mapgen_stages) {
        // A stage whose mod an earlier column stopped does not run again: its
        // VM is gone.
        bool alive = false;
        for (const Mod &m : s.mods.mods())
            if (st.vm && m.vm == st.vm) alive = m.running;
        if (!alive) continue;

        // Read-only, and there is no way to say otherwise: a stage writes with
        // core.set_block and reads the ground with core.terrain_height, and
        // nothing it puts back in this table is looked at.
        script_field fields[6] = {
            {"seed",   SCRIPT_V_NUMBER, (double)s.seed,   nullptr, 0},
            {"height", SCRIPT_V_NUMBER, (double)s.height, nullptr, 0},
            {"min_x",  SCRIPT_V_NUMBER, (double)c.min_x,  nullptr, 0},
            {"min_z",  SCRIPT_V_NUMBER, (double)c.min_z,  nullptr, 0},
            {"max_x",  SCRIPT_V_NUMBER, (double)c.max_x,  nullptr, 0},
            {"max_z",  SCRIPT_V_NUMBER, (double)c.max_z,  nullptr, 0},
        };

        char stage_err[256] = {0};
        s.mods.enter_vm(st.vm);
        const bool ok =
            script_call_handler(st.vm, st.ref, fields, 6, 0, nullptr, stage_err, sizeof(stage_err));
        s.mods.leave_vm(st.vm);

        // A stage that errors costs its own mod and nobody else, which is the
        // rule every other call into a mod already keeps. The world keeps
        // whatever that stage had written before it threw.
        if (!ok) {
            ++failed;
            s.mods.disable_vm(st.vm, "mapgen stage '" + st.name + "': " + stage_err);
        }
    }
    s.generating = false;
    return failed;
}

// What the keeper's hooks do on a server: generate with the engine's stages and
// the mods', queue the loading modifiers of a column just lit, tell every session
// holding a chunk that is leaving, and apply a write that waited through the same
// host functions a mod calls -- so the change reaches the players who can see it.
void keep_columns(ServerState &s)
{
    s.keeper.set_hooks(ColumnKeeper::Hooks{
        [&s](ColumnPos c) {
            if (s.mapgen != nullptr) s.mapgen->generate_column(s.world, s.registry, s.seed, c, s.height);
            run_mod_stages(s, c);
        },
        [&s](const std::vector<ChunkPos> &chunks) {
            // A lit column is a column that has just loaded, as far as anything
            // that looks at the world can tell, and that is the event the
            // loading modifiers exist for.
            if (!s.modifiers.registration_closed() || s.modifiers.lbms().empty()) return;
            for (const ChunkPos p : chunks) s.modifiers.chunk_loaded(p);
        },
        [&s](const std::vector<ChunkPos> &chunks) {
            if (s.sessions == nullptr) return;
            for (const net::PeerId peer : s.sessions->ready_peers())
                if (net::Session *sess = s.sessions->find_mutable(peer))
                    for (const ChunkPos p : chunks) sess->interest.drop(p);
        },
        [&s](const PendingWrite &w) {
            // With the turn it was asked with: a block and its facing are one
            // write (write_block), and the replay of a waiting one dropped the
            // facing -- a track piece put down where the world was not made
            // yet came back unturned, its cells under the road all empty.
            if (w.facing_only) host_set_facing(&s, w.x, w.y, w.z, w.facing);
            else write_told(s, w.x, w.y, w.z, w.id, w.facing);
        },
    });
}

// Towards every column somebody is owed, nearest first; every column a mod has
// written into; and the columns `anchors` stand in. `budget_ms` is what one tick
// may spend; an offline run passes none and makes every one of them now, because
// what a mod's write into a column does has to happen on the same tick on every
// machine and a wall-clock budget would move it.
// `windows` are the far ends of openings somebody is near (net::PlayerInterest::
// set_windows), made after everybody's own columns and only as wide as what is
// streamed round them.
void advance_columns(ServerState &s, const std::vector<ColumnPos> &centres,
                     const std::vector<ColumnPos> &anchors = {},
                     double                        budget_ms = kColumnBudgetMs,
                     const std::vector<ColumnPos> &windows = {})
{
    std::vector<ColumnPos> want = columns_near(centres, net::InterestConfig{}.radius);
    const std::vector<ColumnPos> far = columns_near(windows, net::InterestConfig{}.window_radius);
    want.insert(want.end(), far.begin(), far.end());
    const std::vector<ColumnPos> written = s.keeper.pending_columns();
    want.insert(want.end(), written.begin(), written.end());
    want.insert(want.end(), anchors.begin(), anchors.end());
    s.keeper.advance(want, budget_ms, s.column_clock);
}

// Lets go of what nobody has been near: further from every session than the
// radius a session forgets its chunks at, and two more, so no session can
// still be holding one; and untouched for thirty seconds.
void unload_idle(ServerState &s, const std::vector<ColumnPos> &centres)
{
    const net::InterestConfig interest;
    s.keeper.unload_idle(centres, interest.radius + interest.forget_margin + 2, kColumnIdleTicks,
                         s.column_clock);
}

// One tick of the modifiers. The action is called in its own mod's VM, and an
// action that errors costs that mod and nobody else -- the same bargain an
// event handler makes, because it is the same kind of call.
void step_modifiers(ServerState &s, double dt)
{
    if (!s.modifiers.registration_closed()) return;

    // The world's own clock and not the wall's: two runs of the same seed have
    // to fire the same cells, and a sweep due "one second later" has to mean
    // twenty ticks later on every machine.
    s.modifier_clock += dt;

    const ModifierStepStats st = s.modifiers.step(
        s.world, s.modifier_clock, kModifierBudgetMs,
        [&](const Modifier &m, int x, int y, int z, BlockId id) {
            script_field fields[4] = {
                {"x",  SCRIPT_V_NUMBER, (double)x,        nullptr, 0},
                {"y",  SCRIPT_V_NUMBER, (double)y,        nullptr, 0},
                {"z",  SCRIPT_V_NUMBER, (double)z,        nullptr, 0},
                {"id", SCRIPT_V_NUMBER, (double)id,       nullptr, 0},
            };

            char action_err[256] = {0};
            script_vm *vm = static_cast<script_vm *>(m.vm);
            s.mods.enter_vm(vm);
            const bool ok = script_call_handler(vm, m.handler, fields, 4, 0, nullptr, action_err,
                                                sizeof(action_err));
            s.mods.leave_vm(vm);

            if (!ok) s.mods.disable_vm(vm, "modifier '" + m.name + "': " + action_err);
            return ok;
        });

    s.modifier_actions += (uint64_t)st.actions_run;
    s.modifier_cells   += (uint64_t)st.cells_tested;
    s.modifier_seconds += st.seconds;
}

// What this build of the server can do beyond the base protocol. Empty, and it
// stays empty until something is actually implemented: a feature list is a
// promise the client is entitled to act on, and the one failure mode worth
// avoiding here is a server that advertises compression it does not have.
std::vector<std::string> server_features()
{
    // One, and it is implemented: a client that declares it is handed the
    // server's block table and adopts it, which is what lets somebody join a
    // server whose mods they do not have. Everything else this server might
    // one day do stays off the list until it does it.
    return {net::kFeatureContentAdoption};
}

// The mod channel table (DESIGN.md 6.5), in the order the mods asked for one
// while they loaded. It goes into every welcome, so every client on this server
// is told the same numbers -- and it is the SAME list host_net_open counts, so
// the number a mod was given and the number a client is told cannot disagree.
std::vector<std::string> server_mod_channels(const ServerState &s)
{
    return s.mod_channels;
}

} // namespace

namespace {

// Set by SIGINT or SIGTERM and read by the tick loops. A server told to stop
// finishes the tick it is in and leaves the way a --ticks run does, which is
// the way that saves the world. Before this, Ctrl+C was the default action and
// took every change since the start with it.
volatile std::sig_atomic_t g_stop = 0;

void install_stop_handler() { install_stop_flag(&g_stop); }

// One server per world file.
//
// SQLite lets a second process open the same database, and two servers on one
// world would each keep the chunks it has in memory and write them over the
// other's -- a world that loses whatever the player who joined the wrong one
// did. The lock is a separate file beside the world's, held for as long as this
// process runs; the kernel drops it however the server ends, including a kill.
bool lock_world_file(const std::string &world_file, std::string &err)
{
    const std::string path = world_file + ".lock";

    const int held = hold_lock_file(path);
    if (held < 0) {
        err = "cannot open " + path + ": " + std::strerror(errno);
        return false;
    }
    if (held > 0) {
        // The file's name and not its path: this line is read in a terminal
        // that has the path on its command line, and in the client's menu,
        // where a path fills the panel and pushes the reason off it.
        const size_t slash = world_file.find_last_of('/');
        err = "another server is already running on " +
              (slash == std::string::npos ? world_file : world_file.substr(slash + 1));
        return false;
    }
    return true;
}

} // namespace

// ------------------------------------------------------------------- rooms
//
// A server may hold several worlds, its ROOMS (protocol 56): one port, one
// identity, and a player who walks through an opening into another room stays
// on the same server. Each room is what this program always was -- serve_room
// below, with its own mods, world file and openings -- run on a thread of its
// own. They share the socket through a Router, which reads the room a Hello
// names and hands that peer's events to that room from then on.
//
// One room runs at a time. A room holds `big` for everything it does and lets
// go only while it waits for the network, so the rooms never touch the socket,
// the mod host's globals or each other's players at once. That costs nothing
// against one room per process -- a tick is a few milliseconds of fifty -- and
// is what lets a room reach into another to take a player's bag.
// ponytail: one lock for every room; per-room locks and a queue for the
// socket if rooms ever need more than one core between them.
namespace {

// The rooms' lock, handed on in the order it was asked for. A std::mutex is
// not: a room behind on its ticks let go and took it back before a room that
// had been waiting woke up, over and over, and the waiting room -- the track,
// while the inferno made columns -- waited up to 1.1 s for its turn, which a
// kart's claims do not survive. Tickets give every room that asks a turn
// before any takes a second one.
class TurnLock {
public:
    void lock()
    {
        std::unique_lock<std::mutex> g(m_m);
        const uint64_t mine = m_next++;
        m_cv.wait(g, [&] { return m_serving == mine; });
    }
    void unlock()
    {
        {
            std::lock_guard<std::mutex> g(m_m);
            ++m_serving;
        }
        m_cv.notify_all();
    }

private:
    std::mutex              m_m;
    std::condition_variable m_cv;
    uint64_t                m_next = 0, m_serving = 0;
};

// What one room lets the others see, set once it is running.
struct RoomShared {
    std::string          name;
    net::ServerSessions *sessions = nullptr;
    ItemRegistry        *items    = nullptr;
    const NodeRegistry  *registry = nullptr;   // block names, for /inspect
    const ModelRegistry *models   = nullptr;
    WorldDb             *db       = nullptr;
    ContainerSet        *containers = nullptr;   // bring_carried_containers
    ModStore            *mod_store  = nullptr;   // the first room's: core.shared_storage_*
    std::unordered_set<uint32_t> *bags_touched = nullptr;   // a bag a friend server took or sent
    // The longest tick of the last minute, for /metrics; and when this room
    // last finished one, for --watchdog.
    std::atomic<int>     worst_tick_ms{0};
    std::atomic<int64_t> beat{0};
    // Its links out (--portal-to) and the issuers it trusts (--trust), for
    // /portals.
    std::vector<std::string> portal_to, trusted;

    explicit RoomShared(std::string n) : name(std::move(n)) {}
    RoomShared(RoomShared &&o) noexcept
        : name(std::move(o.name)), sessions(o.sessions), items(o.items), registry(o.registry), models(o.models), db(o.db),
          containers(o.containers), mod_store(o.mod_store), bags_touched(o.bags_touched)
    {
    }
};

const auto g_started = std::chrono::steady_clock::now();
bool       g_update_told = false;    // the newer release said once (update::newer)

std::string update_line()
{
    return "A newer release, " + update::newer() + ", is out; this server runs " + kEngineVersion + ".";
}
int        g_exit_code = 0;          // 75 for a restart: tools/serve.sh starts it again
int        g_max_per_address = 0;    // --max-per-address, 0 for no limit

struct Router {
    TurnLock                                 big;
    net::Transport                           transport;
    std::vector<RoomShared>                  rooms;
    std::vector<std::deque<net::NetEvent>>   queues;
    std::unordered_map<net::PeerId, int>     room_of;

    // What each room is to hear as core:far_moved on its next tick: one of its
    // openings is where an opening that leads to it moved (tell_far_room).
    struct FarMoved {
        int    portal;
        Portal where;
    };
    std::vector<std::vector<FarMoved>> far_moved;
    // Blocks /rollback puts back, for each room's next tick.
    std::vector<std::vector<admin::Restore>> restores;

    // Connected, and not yet said which room: held until the Hello, or dropped
    // after a while like any peer that never says hello.
    struct Waiting {
        net::NetEvent connected;
        std::chrono::steady_clock::time_point since;
    };
    std::unordered_map<net::PeerId, Waiting> waiting;

    bool identity_set = false;
    bool listening    = false;

    int find(const std::string &name) const
    {
        for (size_t i = 0; i < rooms.size(); ++i)
            if (rooms[i].name == name) return (int)i;
        return name.empty() && !rooms.empty() ? 0 : -1;
    }

    void route(net::NetEvent &ev)
    {
        switch (ev.type) {
        case net::NetEvent::Type::Connected:
            waiting[ev.peer] = Waiting{ev, std::chrono::steady_clock::now()};
            return;
        case net::NetEvent::Type::Disconnected: {
            if (waiting.erase(ev.peer) > 0) return;
            const auto it = room_of.find(ev.peer);
            if (it == room_of.end()) return;
            queues[(size_t)it->second].push_back(std::move(ev));
            room_of.erase(it);
            return;
        }
        case net::NetEvent::Type::Received: {
            const auto it = room_of.find(ev.peer);
            if (it != room_of.end()) {
                queues[(size_t)it->second].push_back(std::move(ev));
                return;
            }
            const auto w = waiting.find(ev.peer);
            if (w == waiting.end()) return;

            // The list of servers asking who is here: answered and let go.
            if (net::peek_type(ev.data.data(), ev.data.size()) == net::MsgType::StatusQuery) {
                net::Status st;
                st.version = kEngineVersion;
                st.rooms   = (uint16_t)rooms.size();
                for (const RoomShared &r : rooms)
                    if (r.sessions)
                        for (const net::PeerId p : r.sessions->ready_peers())
                            if (const net::Session *s = r.sessions->find(p); s && s->scope == net::Scope::Enter)
                                ++st.players;
                st.motd        = admin::motd();
                st.maintenance = admin::maintenance();
                transport.send(ev.peer, net::Channel::Handshake, st.encode(), true);
                transport.flush();
                transport.disconnect(ev.peer);
                waiting.erase(w);
                return;
            }

            // The first word decides the room. Anything that is not a Hello
            // goes to the first room, whose sessions refuse it as they would.
            int room = 0;
            net::Hello hello;
            ByteReader r(ev.data);
            if (net::peek_type(ev.data.data(), ev.data.size()) == net::MsgType::Hello &&
                net::Hello::decode(r, hello)) {
                room = find(hello.room);
                if (room < 0) {
                    net::Refuse no;
                    no.reason = net::RefuseReason::NoSuchRoom;
                    no.detail = "this server holds no world called '" + hello.room + "'";
                    transport.send(ev.peer, net::Channel::Handshake, no.encode(), true);
                    transport.flush();
                    transport.disconnect(ev.peer);
                    std::printf("server: a peer asked for room '%s', which there is not\n",
                                hello.room.c_str());
                    waiting.erase(w);
                    return;
                }
            }
            room_of[ev.peer] = room;
            queues[(size_t)room].push_back(w->second.connected);
            queues[(size_t)room].push_back(std::move(ev));
            waiting.erase(w);
            return;
        }
        case net::NetEvent::Type::None:
            return;
        }
    }

    // Everything the socket has, into the rooms' queues; and the peers that
    // never said which room, let go after the time a session gets to say hello.
    void pump(int timeout_ms)
    {
        net::NetEvent ev;
        while (transport.poll(ev, timeout_ms)) {
            route(ev);
            timeout_ms = 0;
        }
        const auto now = std::chrono::steady_clock::now();
        for (auto it = waiting.begin(); it != waiting.end();) {
            if (now - it->second.since > std::chrono::seconds(10)) {
                transport.disconnect(it->first);
                it = waiting.erase(it);
            } else {
                ++it;
            }
        }
    }
};

Router g_router;

// Which room this thread runs.
thread_local int g_room = 0;

ModStore &shared_store(ServerState *s)
{
    if (!g_router.rooms.empty() && g_router.rooms[0].mod_store != nullptr)
        return *g_router.rooms[0].mod_store;
    return s->mod_store;
}

// Bags between friend servers, below the rooms (admin::bag_ask).
bool bag_give_here(const uint8_t *identity, bool from_file, std::string &out, std::string &packs);
void bag_take_here(const uint8_t *identity, const std::string &bag, const std::string &hidden,
                   const std::string &packs, const std::string &from);
void bag_late_here(const uint8_t *identity);
int64_t bag_seen_here(const uint8_t *identity);
net::Session *bag_holder(const uint8_t *identity, int &room);
WorldDb      *newest_bag_file(const uint8_t *identity);

// A player in this process as the admin store sees them (src/server/admin.h).
admin::Who admin_who(const net::Session &p, const std::string *room)
{
    admin::Who w;
    w.room         = room ? *room : g_router.rooms[(size_t)g_room].name;
    w.name         = p.name;
    w.has_identity = p.has_identity;
    if (p.has_identity) std::memcpy(w.identity, p.identity, net::kSignPublicBytes);
    w.address = g_router.transport.address(p.id);
    return w;
}

// What an admin command reaches: the players of every room, a portal window
// included, and the way out -- the refusal first, flushed, then the cut, so
// they read why. The room hears the Disconnected and lets go of the body.
admin::Hooks admin_hooks()
{
    admin::Hooks h;
    h.online = [] {
        std::vector<admin::Online> out;
        for (const RoomShared &r : g_router.rooms) {
            if (!r.sessions) continue;
            for (const net::PeerId peer : r.sessions->ready_peers())
                if (const net::Session *p = r.sessions->find(peer))
                    out.push_back({peer, admin_who(*p, &r.name), r.name});
        }
        return out;
    };
    h.remove = [](net::PeerId peer, net::RefuseReason reason, const std::string &text) {
        net::Refuse r;
        r.reason = reason;
        r.detail = text;
        g_router.transport.send(peer, net::Channel::Handshake, r.encode(), true);
        g_router.transport.flush();
        g_router.transport.disconnect(peer);
    };
    h.notice = [](const std::string &text) {
        const std::vector<uint8_t> bytes = net::Notice{text}.encode();
        for (const RoomShared &r : g_router.rooms)
            if (r.sessions)
                for (const net::PeerId peer : r.sessions->ready_peers())
                    if (const net::Session *p = r.sessions->find(peer); p && p->scope == net::Scope::Enter)
                        g_router.transport.send(peer, net::Channel::Event, bytes, true);
        std::printf("        notice: %s\n", text.c_str());
    };
    h.block_name = [](const std::string &room, int id) {
        const int r = g_router.find(room);
        return r < 0 || g_router.rooms[(size_t)r].registry == nullptr || id < 0 ||
                       (size_t)id >= g_router.rooms[(size_t)r].registry->size()
                   ? std::string()
                   : g_router.rooms[(size_t)r].registry->get((BlockId)id).name;
    };
    h.restore = [](const std::vector<admin::Restore> &list) {
        for (const admin::Restore &r : list) {
            const int room = g_router.find(r.room);
            if (room >= 0 && (size_t)room < g_router.restores.size()) g_router.restores[(size_t)room].push_back(r);
        }
    };
    h.bag_lines = [](const admin::Who &w) {
        std::vector<std::string> out;
        if (!w.has_identity) return out;
        int                  room = 0;
        Inventory            file_bag;
        const Inventory     *bag  = nullptr;
        if (const net::Session *sess = bag_holder(w.identity, room)) {
            bag = &sess->bag();
        } else if (WorldDb *db = newest_bag_file(w.identity); db && g_router.rooms[0].items) {
            std::string err;
            if (db->load_player_bag(w.identity, *g_router.rooms[0].items, file_bag, err)) bag = &file_bag;
            out.push_back("Not online; the bag kept here:");
        }
        if (bag == nullptr) return out;
        const ItemRegistry &items = *g_router.rooms[(size_t)room].items;
        for (int i = 0; i < bag->size(); ++i) {
            const ItemStack &st = bag->at(i);
            if (st.empty()) continue;
            out.push_back("square " + std::to_string(i + 1) + ": " + std::to_string(st.count) + " " +
                          items.get(st.item).name);
            // And what a backpack holds (mods/backpack), wherever its room is.
            const uint32_t pack = st.tag & kTagIdMask;
            if (pack == 0 || items.get(st.item).wear != kWornBack) continue;
            for (const RoomShared &o : g_router.rooms)
                if (const Inventory *c = o.containers ? o.containers->find(ContainerAt{(int)pack, kCarriedBelow, 0})
                                                      : nullptr)
                    for (const ItemStack &in : c->slots())
                        if (!in.empty())
                            out.push_back("  in it (#" + std::to_string(pack) + "): " + std::to_string(in.count) +
                                          " " + o.items->get(in.item).name);
        }
        return out;
    };
    h.give = [](net::PeerId peer, const std::string &item, int count) -> std::string {
        const auto r = g_router.room_of.find(peer);
        if (r == g_router.room_of.end()) return "not in a room";
        RoomShared   &room = g_router.rooms[(size_t)r->second];
        net::Session *sess = room.sessions ? room.sessions->find_mutable(peer) : nullptr;
        if (!sess || !sess->holds_bag) return "they hold no bag here (watching, or arriving)";
        const ItemId id = room.items->find(item);
        if (id == kNoItem || room.items->is_orphan(id)) return "no item '" + item + "' in that world";
        const uint16_t left = sess->bag().add(*room.items, id, (uint16_t)count);
        if (room.bags_touched) room.bags_touched->insert((uint32_t)peer);
        return left > 0 ? "only " + std::to_string(count - left) + " fitted" : "";
    };
    h.bag_give = bag_give_here;
    h.bag_take = bag_take_here;
    h.bag_late = bag_late_here;
    h.bag_seen = bag_seen_here;
    h.say = [](net::PeerId peer, const std::string &line) {
        g_router.transport.send(peer, net::Channel::Event, net::Chat{line}.encode(), true);
    };
    h.portals = [] {
        std::vector<std::string> out;
        for (const RoomShared &r : g_router.rooms) {
            const std::string room = r.name.empty() ? std::string("this world") : r.name;
            for (size_t i = 0; i < r.portal_to.size(); ++i)
                out.push_back(room + ", opening " + std::to_string(i) + ": " + r.portal_to[i]);
            if (!r.trusted.empty())
                out.push_back(room + " takes tickets from " + std::to_string(r.trusted.size()) + " server(s).");
        }
        return out;
    };
    h.metrics = [] {
        std::vector<std::string> out;
        const int64_t up = (int64_t)std::chrono::duration_cast<std::chrono::seconds>(
                               std::chrono::steady_clock::now() - g_started).count();
        out.push_back("Up " + std::to_string(up / 3600) + "h " + std::to_string(up % 3600 / 60) + "m, " +
                      std::string(kEngineVersion) + ".");
        for (const RoomShared &r : g_router.rooms) {
            size_t n = 0, watching = 0;
            if (r.sessions)
                for (const net::PeerId peer : r.sessions->ready_peers())
                    if (const net::Session *p = r.sessions->find(peer))
                        ++(p->scope == net::Scope::Enter ? n : watching);
            out.push_back((r.name.empty() ? std::string("the world") : r.name) + ": " + std::to_string(n) +
                          " playing, " + std::to_string(watching) + " looking in, longest gap between ticks " +
                          std::to_string(r.worst_tick_ms.load()) + " ms this minute (50 is on time).");
        }
#ifdef __linux__ // /proc is Linux only
        if (FILE *f = std::fopen("/proc/self/statm", "r")) {
            long pages = 0, rss = 0;
            if (std::fscanf(f, "%ld %ld", &pages, &rss) == 2)
                out.push_back("Memory " + std::to_string(rss * (sysconf(_SC_PAGESIZE) / 1024) / 1024) + " MB.");
            std::fclose(f);
        }
#endif
        return out;
    };
    return h;
}

// The pages: the engine's own and the mods' (src/core/ui/page.h), each player's
// on a panel of its own that the engine owns.
constexpr const char *kPageOwner = "core:page";

std::string page_panel(net::PeerId peer) { return "page:" + std::to_string(peer); }

// Names the engine answers itself, which no mod may register.
bool core_page_name(const std::string &n)
{
    for (const char *c : {"pages", "admin", "bans", "whitelist", "roles", "perms", "server", "running", "players", "reports", "log",
                          "mods", "here", "rules"})
        if (n == c) return true;
    return false;
}

void show_page(ServerState &s, net::PeerId peer, ServerState::PageUp up)
{
    const std::string name = page_panel(peer);
    up.layout = ui::layout_page(name, kPageOwner, up.title, up.markup, up.screen);
    up.screen = up.layout.screen;
    std::string err;
    bool ok = s.panels.define(name, kPageOwner, up.layout.panel.w, up.layout.panel.h, err);
    for (const ui::Element &e : up.layout.panel.elements)
        ok = ok && s.panels.add(name, kPageOwner, e, err);
    ok = ok && s.panels.show(name, kPageOwner, true, (ui::PlayerId)peer, err);
    if (!ok) {
        std::printf("        page '%s' for %u: %s\n", up.name.c_str(), (unsigned)peer, err.c_str());
        return;
    }
    s.pages_up[peer] = std::move(up);
}

void close_page(ServerState &s, net::PeerId peer)
{
    s.pages_up.erase(peer);
    s.panels.drop_owner(kPageOwner, page_panel(peer));
}

// A player gone from this room, however they went: the visit ends
// (admin::left), the page goes.
void gone(ServerState &s, net::PeerId peer, const char *why)
{
    if (const net::Session *p = s.sessions ? s.sessions->find(peer) : nullptr;
        p && p->scope == net::Scope::Enter && !p->name.empty())
        admin::left(admin_who(*p), g_router.rooms[(size_t)g_room].name, why);
    s.said_at.erase(peer);
    close_page(s, peer);
}

void say_to(ServerState &s, net::PeerId peer, const std::string &line)
{
    s.mod_says.push_back(ServerState::ModSay{(int)peer, 0, line, false, true});
}

// The engine's pages that need the world rather than the admin store.
bool world_page(ServerState &s, const admin::Who &who, const net::Session &me,
                const std::string &name, std::string &title, std::string &m)
{
    using ui::page_text;
    if (name == "pages") {
        title = "Pages";
        if (admin::may(who, "pages")) m += "[Administration](page:admin)\n";
        m += "[Server](page:server) [Mods](page:mods) [Here](page:here)\n";
        bool any = false;
        for (const auto &[n, d] : s.pages)
            if (d.perm.empty() || admin::may(who, d.perm)) {
                if (!any) m += "## From the mods\n";
                any = true;
                m += "[" + page_text(d.title) + "](page:" + n + ") ";
            }
        m += "\n";
        return true;
    }
    if (name == "server") {
        title = "Server";
        m += std::string("Cubes and portals ") + kEngineVersion + ".\n";
        m += "## Rooms\n| room | players | |\n";
        for (const RoomShared &r : g_router.rooms) {
            size_t n = 0;
            if (r.sessions)
                for (const net::PeerId peer : r.sessions->ready_peers())
                    if (const net::Session *p = r.sessions->find(peer); p && p->scope == net::Scope::Enter)
                        ++n;
            m += "| " + page_text(r.name.empty() ? "this world" : r.name) + " | " +
                 std::to_string(n) + " | |\n";
        }
        if (admin::may(who, "pages")) m += "\n[Administration](page:admin)\n";
        return true;
    }
    if (name == "mods") {
        title = "Mods in this world";
        m += "| mod | version | |\n";
        for (const Mod &mod : s.mods.mods())
            m += "| " + page_text(mod.manifest.name) + " | " + mod.manifest.version.str() + " | " +
                 (mod.running ? "running" : page_text("stopped: " + mod.error)) + " |\n";
        return true;
    }
    if (name == "here") {
        title = "Here";
        const ecs::Position p = me.avatar.is_alive() && me.avatar.has<ecs::Position>()
                                    ? me.avatar.get<ecs::Position>()
                                    : ecs::Position{};
        const int x = (int)std::floor(p.x), z = (int)std::floor(p.z);
        const uint32_t tag = s.sessions ? s.sessions->owned_at(x, z) : 0;
        m += "Room: " + page_text(g_router.rooms[(size_t)g_room].name.empty()
                                      ? "this world"
                                      : g_router.rooms[(size_t)g_room].name) + "\n";
        m += "Standing at " + std::to_string(x) + " " + std::to_string((int)std::floor(p.y)) + " " +
             std::to_string(z) + ".\n";
        m += tag ? "This ground belongs to owner " + std::to_string(tag) +
                       " (core.owned_area): what is taken here comes out marked.\n"
                 : "This ground belongs to nobody.\n";
        m += "> A mod that keeps zones can offer its own page: /page lists them.\n";
        return true;
    }
    return false;
}

// A page for a player: the engine's, or a mod's -- whose mod hears
// core:page_open and answers with core.page_show. False for none they may see.
bool open_page(ServerState &s, net::PeerId peer, const std::string &name, int screen = 0)
{
    const net::Session *me = s.sessions ? s.sessions->find(peer) : nullptr;
    if (!me) return false;
    const admin::Who     who = admin_who(*me);
    ServerState::PageUp up;
    up.name   = name;
    up.owner  = "core";
    up.screen = screen;
    if (admin::page(name, who, admin_hooks(), up.title, up.markup) ||
        world_page(s, who, *me, name, up.title, up.markup)) {
        show_page(s, peer, std::move(up));
        return true;
    }
    const auto it = s.pages.find(name);
    if (it == s.pages.end() || (!it->second.perm.empty() && !admin::may(who, it->second.perm)))
        return false;
    // Where they were on it, for the mod's answer (host_page_show).
    if (const auto was = s.pages_up.find(peer); was == s.pages_up.end() || was->second.name != name)
        s.pages_up.erase(peer);
    script_field fields[2] = {
        {"player", SCRIPT_V_NUMBER, (double)peer, nullptr, 0},
        {"page", SCRIPT_V_STRING, 0.0, name.c_str(), 0},
    };
    std::string err;
    if (!s.mods.events().emit("core:page_open", fields, 2, nullptr, err, &it->second.owner))
        std::fprintf(stderr, "emit core:page_open: %s\n", err.c_str());
    return true;
}

// While a muted player's command, page action or click runs, what its mod
// core.chats to anybody else is held (host_chat); cleared after the emit.
// And what they handed it is noted (g_muted_words), for what the mod says
// with it later.
void hold_if_muted(ServerState &s, net::PeerId peer, const std::string &words)
{
    const net::Session *p = s.sessions ? s.sessions->find(peer) : nullptr;
    if (!p) return;
    const admin::Who who = admin_who(*p);
    if (!admin::muted(who, s.muted_why)) return;
    s.muted_caller = (int)peer;
    note_muted_words(who, words);
}

// A line a player typed or a page ran for them: a /command answered here or by
// its mod, or something said to everybody in this world. Answers go out with
// the mods' chat, at the end of the tick.
void run_chat_line(ServerState &s, net::PeerId peer, const std::string &line)
{
    const net::Session *from = s.sessions ? s.sessions->find(peer) : nullptr;
    const std::string   who  = from ? from->name : "?";
    const admin::Who    caller = from ? admin_who(*from) : admin::Who{};
    std::string         name, args;
    std::vector<std::string> said;
    std::string              held;
    const bool               is_command = net::chat_split(line, name, args);
    // Rules not accepted: only what reads, accepts or leaves them.
    if (!admin::accepted(caller) && !(is_command && (name == "rules" || name == "accept" ||
                                                     name == "leave" || name == "help"))) {
        say_to(s, peer, "Accept the rules first: /rules, then /accept (or /leave).");
        open_page(s, peer, "rules");
        return;
    }
    if (!is_command) {
        if (admin::muted(caller, held)) {
            say_to(s, peer, held);
            return;
        }
        // Five lines in five seconds, and the sixth waits.
        std::deque<double> &said_at = s.said_at[peer];
        const double        t       = std::chrono::duration<double>(std::chrono::steady_clock::now() - g_started).count();
        while (!said_at.empty() && t - said_at.front() > 5.0) said_at.pop_front();
        if (said_at.size() >= 5) {
            say_to(s, peer, "Slow down: five lines in five seconds at most.");
            return;
        }
        said_at.push_back(t);
        const std::string out = "<" + who + "> " + line;
        std::printf("        chat: %s\n", out.c_str());
        s.mod_says.push_back(ServerState::ModSay{0, 0, out, false, true});
    } else if (admin::run(caller, name, args, admin_hooks(), said)) {
        for (const std::string &l : said) say_to(s, peer, l);
    } else if (name == "page" || name == "admin") {
        std::string want = name == "admin" ? "admin" : args.substr(0, args.find(' '));
        if (want.empty()) want = "pages";
        if (!open_page(s, peer, want)) say_to(s, peer, "No page '" + want + "' for you -- /page lists them");
    } else if (name == "help") {
        for (const std::string &l : net::chat_help(s.commands)) say_to(s, peer, l);
        say_to(s, peer, "/page [NAME] -- the pages: the server's, the mods'");
        if (admin::may(caller, "pages")) say_to(s, peer, "/admin -- the administration page");
        for (const std::string &l : admin::help(caller)) say_to(s, peer, l);
    } else if (name == "list") {
        std::string names;
        int         n = 0;
        for (const net::PeerId p : s.sessions->ready_peers())
            if (const net::Session *o = s.sessions->find(p); o && o->scope == net::Scope::Enter)
                names += (n++ ? ", " : "") + o->name;
        say_to(s, peer, std::to_string(n) + " in this world: " + names);
    } else if (const auto c = s.commands.find(name);
               c != s.commands.end() && !c->second.perm.empty() &&
               !admin::may(caller, c->second.perm)) {
        say_to(s, peer, "You may not use /" + name + ".");
    } else if (c != s.commands.end()) {
        std::printf("        '%s' ran /%s\n", who.c_str(), name.c_str());
        script_field fields[3] = {
            {"player", SCRIPT_V_NUMBER, (double)peer, nullptr, 0},
            {"name", SCRIPT_V_STRING, 0.0, name.c_str(), 0},
            {"args", SCRIPT_V_STRING, 0.0, args.c_str(), 0},
        };
        std::string cmd_err;
        hold_if_muted(s, peer, args);
        if (!s.mods.events().emit("core:command", fields, 3, nullptr, cmd_err, &c->second.owner))
            std::fprintf(stderr, "emit core:command: %s\n", cmd_err.c_str());
        s.muted_caller = 0;
        s.muted_why.clear();
    } else {
        say_to(s, peer, "Unknown command /" + name + " -- /help lists them");
    }
}

// A click on a player's page: its own buttons, or a link.
void page_click(ServerState &s, net::PeerId peer, const std::string &id, const std::string &text)
{
    const auto it = s.pages_up.find(peer);
    if (it == s.pages_up.end()) return;
    ServerState::PageUp up = it->second;   // a copy: what runs below may replace it
    if (id == ui::kPanelEscape || id == ui::kPageClose) {
        close_page(s, peer);
        return;
    }
    if (id == ui::kPagePrev || id == ui::kPageNext) {
        up.screen += id == ui::kPagePrev ? -1 : 1;
        show_page(s, peer, std::move(up));
        return;
    }
    const ui::PageLink *l = ui::page_link(up.layout, id);
    if (!l) return;
    std::string target = l->target;
    if (l->field)
        for (size_t at; (at = target.find("{text}")) != std::string::npos;)
            target.replace(at, 6, text);

    if (target.rfind("page:", 0) == 0) {
        if (!open_page(s, peer, target.substr(5))) say_to(s, peer, "No page '" + target.substr(5) + "' for you");
    } else if (target.rfind("cmd:", 0) == 0) {
        run_chat_line(s, peer, target.substr(4));
        // And the page again, with what the command changed.
        if (const auto now = s.pages_up.find(peer); now != s.pages_up.end() && now->second.name == up.name)
            open_page(s, peer, up.name, up.screen);
    } else if (target.rfind("act:", 0) == 0 && up.owner != "core") {
        const std::string act = target.substr(4);
        script_field fields[4] = {
            {"player", SCRIPT_V_NUMBER, (double)peer, nullptr, 0},
            {"page", SCRIPT_V_STRING, 0.0, up.name.c_str(), 0},
            {"id", SCRIPT_V_STRING, 0.0, act.c_str(), 0},
            {"text", SCRIPT_V_STRING, 0.0, l->field ? text.c_str() : "", 0},
        };
        std::string err;
        hold_if_muted(s, peer, l->field ? text : "");
        if (!s.mods.events().emit("core:page_action", fields, 4, nullptr, err, &up.owner))
            std::fprintf(stderr, "emit core:page_action: %s\n", err.c_str());
        s.muted_caller = 0;
        s.muted_why.clear();
    }
}

// core.page_register, core.page_show, core.is_muted.
int host_page_register(void *ud, const char *name, const char *title, const char *perm)
{
    ServerState      *s     = static_cast<ServerState *>(ud);
    const std::string owner = asking_mod();
    admin::Role       need;
    if (!name || !net::chat_command_name_ok(name) || core_page_name(name) ||
        (perm && *perm && !admin::perm_role(perm, need)))
        return 0;
    const auto it = s->pages.find(name);
    if (it != s->pages.end() && it->second.owner != owner) return 0;
    s->pages[name] = ServerState::PageDecl{owner, title && *title ? title : name, perm ? perm : ""};
    return 1;
}

int host_page_show(void *ud, int player, const char *name, const char *markup)
{
    ServerState *s = static_cast<ServerState *>(ud);
    if (!s->sessions || !name || !*name || !markup || std::strlen(markup) > 16384 ||
        !s->sessions->find((net::PeerId)player))
        return 0;
    ServerState::PageUp up;
    up.name   = name;
    up.owner  = asking_mod();
    // A line of the page with a muted player's words is held for everybody
    // but them (a board's note).
    const std::string reader = s->sessions->find((net::PeerId)player)->name;
    std::istringstream lines(markup);
    for (std::string l; std::getline(lines, l);) {
        const std::string author = muted_author(l, reader);
        static std::set<std::string> told;   // once for each page, author and reader
        if (!author.empty() && told.insert(std::string(name) + "|" + author + "|" + reader).second)
            std::printf("        held a line of page '%s' with %s's words (muted) from '%s'\n", name,
                        author.c_str(), reader.c_str());
        up.markup += (author.empty() ? l : "(held: " + ui::page_text(author) + " is muted)") + "\n";
    }
    const auto decl = s->pages.find(name);
    up.title  = decl != s->pages.end() && decl->second.owner == up.owner ? decl->second.title : name;
    if (const auto was = s->pages_up.find((net::PeerId)player);
        was != s->pages_up.end() && was->second.name == up.name)
        up.screen = was->second.screen;
    show_page(*s, (net::PeerId)player, std::move(up));
    return 1;
}

int host_is_muted(void *ud, int player)
{
    ServerState        *s = static_cast<ServerState *>(ud);
    const net::Session *p = s->sessions ? s->sessions->find((net::PeerId)player) : nullptr;
    std::string         why;
    return p && admin::muted(admin_who(*p), why) ? 1 : 0;
}

// Called holding the rooms' lock, as every host call is, so the room it is for
// reads it on its own turn.
void tell_far_room(const std::string &room, int portal, const Portal &p)
{
    const int r = g_router.find(room);
    if (r < 0 || (size_t)r >= g_router.far_moved.size()) return;
    g_router.far_moved[(size_t)r].push_back({portal, p});
}

// How long this room has held the rooms' lock without letting go, at most,
// once it is ticking: a room that holds it for a second is a second every other
// room's players hear nothing.
thread_local std::chrono::steady_clock::time_point g_held_since;

// Ticks that ran over (see the main loop): the worst, how many, what the mods
// and the columns took, how far behind the clock the server got. Printed every
// ten seconds when anything ran over, and reset.
struct TickStats {
    double started = -1.0, last = 0.0, worst = 0.0, behind = 0.0, last_took = 0.0;
    double mods = 0.0, mods_worst = 0.0, cols = 0.0, cols_tick = 0.0, cols_worst = 0.0;
    int    ticks = 0, over = 0;
    void add_mods(double secs)
    {
        mods += secs;
        mods_worst = std::max(mods_worst, secs);
    }
    void end_tick(double now, bool quiet)
    {
        const double took = started >= 0.0 && last > 0.0 ? now - last : 0.0;
        last = now;
        last_took = took;
        if (started < 0.0) started = now;
        ++ticks;
        worst = std::max(worst, took);
        if (took > 0.06) ++over;   // 50 a tick, with what the socket takes
        cols_worst = std::max(cols_worst, cols - cols_tick);
        cols_tick  = cols;
        if (now - started < 10.0) return;
        if (over > 0 && !quiet)
            std::printf("        ticks: %d over 60 ms of %d, the worst %.0f ms, %.0f ms behind at most; mods "
                        "%.1f ms a tick (worst %.0f), columns %.1f (worst %.0f)\n",
                        over, ticks, worst * 1000.0, behind * 1000.0, mods * 1000.0 / ticks,
                        mods_worst * 1000.0, cols * 1000.0 / ticks, cols_worst * 1000.0);
        *this  = TickStats{};
        started = last = now;
    }
};
thread_local TickStats g_tick_stats;
thread_local double                                g_longest_hold = 0.0;

void let_go_of_rooms()
{
    const double held = std::chrono::duration<double>(std::chrono::steady_clock::now() -
                                                      g_held_since).count();
    g_longest_hold = std::max(g_longest_hold, held);
    g_router.big.unlock();
}

void take_rooms()
{
    g_router.big.lock();
    g_held_since = std::chrono::steady_clock::now();
}

// The transport as one room sees it: the Router's, with the room's own events.
// The calls the room makes once for the whole server -- the identity, the
// socket -- are made by the first room to make them; the others' are nothing.
class RoomTransport {
public:
    void set_encryption(bool on)
    {
        if (!g_router.identity_set) g_router.transport.set_encryption(on);
    }
    void set_identity(const net::KeyPair &k)
    {
        if (g_router.identity_set) return;
        g_router.transport.set_identity(k);
        g_router.identity_set = true;
    }
    bool listen(uint16_t port, int max_peers, std::string &err, const std::string &bind = {})
    {
        if (g_router.listening) return true;
        g_router.listening = g_router.transport.listen(
            port, max_peers * (int)g_router.rooms.size(), err, bind);
        return g_router.listening;
    }

    // Waits WITHOUT the lock, so the other rooms run meanwhile. Alone on the
    // server, a room waits in the socket itself as it always did.
    bool poll(net::NetEvent &out, int timeout_ms)
    {
        std::deque<net::NetEvent> &q = g_router.queues[(size_t)g_room];
        const auto until = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout_ms);
        for (;;) {
            if (!q.empty()) {
                out = std::move(q.front());
                q.pop_front();
                return true;
            }
            const int left = (int)std::chrono::duration_cast<std::chrono::milliseconds>(
                                 until - std::chrono::steady_clock::now())
                                 .count();
            if (!g_router.listening) {
                // Offline, or before the first room has opened the socket.
                if (g_router.rooms.size() == 1 || left <= 0) return false;
                let_go_of_rooms();
                std::this_thread::sleep_for(std::chrono::milliseconds(std::min(left, 1)));
                take_rooms();
                continue;
            }
            if (g_router.rooms.size() == 1) {
                g_router.pump(std::max(0, left));
                if (q.empty() && left <= 0) return false;
                if (q.empty() && std::chrono::steady_clock::now() >= until) return false;
                continue;
            }
            g_router.pump(0);
            if (!q.empty()) continue;
            if (left <= 0) {
                // Behind, a room asks with no time to wait and would return
                // holding the lock -- and a room behind runs its ticks back to
                // back, so it held it for as long as it stayed behind: a room
                // making columns of the inferno starved the others for seconds,
                // and their players gave the server up as silent. Let go once
                // on the way out, so every room gets a turn between two ticks.
                let_go_of_rooms();
                std::this_thread::yield();
                take_rooms();
                return false;
            }
            let_go_of_rooms();
            std::this_thread::sleep_for(std::chrono::milliseconds(std::min(left, 1)));
            take_rooms();
        }
    }

    bool channel_binding(net::PeerId p, uint8_t out[net::kKeyBytes]) const
    {
        return g_router.transport.channel_binding(p, out);
    }
    bool send(net::PeerId p, net::Channel c, const std::vector<uint8_t> &d, bool reliable = true)
    {
        return g_router.transport.send(p, c, d, reliable);
    }
    void   flush() { g_router.transport.flush(); }
    void   disconnect(net::PeerId p, bool now = false) { g_router.transport.disconnect(p, now); }
    size_t owed_bytes(net::PeerId p) const { return g_router.transport.owed_bytes(p); }
    double round_trip(net::PeerId p) const { return g_router.transport.round_trip(p); }
    void   hold_peer(net::PeerId p, double seconds) { g_router.transport.hold_peer(p, seconds); }
    uint64_t rejected_frames() const { return g_router.transport.rejected_frames(); }
    void     close() {}   // the Router's, closed when every room has stopped
};

using IdentityKey = std::array<uint8_t, net::kSignPublicBytes>;

IdentityKey identity_key(const uint8_t *id)
{
    IdentityKey k{};
    std::memcpy(k.data(), id, k.size());
    return k;
}

// The room each player's bag was last kept by, this run: a later join reads
// it from that room's file. Across a restart, the file that kept it last.
std::map<IdentityKey, int> g_bag_kept_by;

// A body that drives, taken along: how it moves, and what it wears by name.
struct CarriedBody {
    bool          drives = false;
    MovementRules movement;
    std::string   model;
    std::string   carried;   // core.player_carry's line about it
};

// What a player carries, taken from the room where they are a body right now
// -- the one they just walked out of -- as bytes by name, which every room can
// read whatever items it knows (core/item/item_codec.h). That room's copy is
// emptied and marked as elsewhere, so it is neither used nor saved there.
//
// And a body that drives (core.body_drive: a kart) with it, when `models` --
// the room it arrives in -- knows what it wears: that room's mods put karts
// there, and a kart driven into a world with none arrives on foot as it always
// did. The mod's line about it (core.player_carry) goes too, on foot or not:
// a world says where somebody left it by (a way out that is not the way in).
bool take_carried(const uint8_t *identity, std::vector<uint8_t> &bag, ecs::Health &health,
                  const ModelRegistry &models, CarriedBody &body)
{
    for (size_t i = 0; i < g_router.rooms.size(); ++i) {
        if ((int)i == g_room) continue;
        RoomShared &r = g_router.rooms[i];
        if (r.sessions == nullptr || r.items == nullptr) continue;
        for (const net::PeerId peer : r.sessions->ready_peers()) {
            net::Session *sess = r.sessions->find_mutable(peer);
            if (!sess || !sess->has_identity || !sess->holds_bag ||
                std::memcmp(sess->identity, identity, net::kSignPublicBytes) != 0)
                continue;
            bag    = encode_inventory(*r.items, sess->bag());
            health = sess->vitals();
            sess->bag()     = Inventory{};
            sess->holds_bag = false;

            body = CarriedBody{};
            const ecs::Wears *w = sess->avatar.try_get<ecs::Wears>();
            if (sess->movement.drives() && w && r.models) {
                const std::string name = r.models->get(w->model).name;
                if (!name.empty() && models.find(name) != kNoModel) {
                    body.drives   = true;
                    body.movement = sess->movement;
                    body.model    = name;
                    sess->body_taken = true;
                }
            }
            body.carried = sess->carried;
            sess->carried.clear();
            return true;
        }
    }
    return false;
}

// What a player's numbered back pieces hold -- mods/backpack's carried
// container at {its number, kCarriedBelow, 0} -- brought here from whichever
// room has it, by name like the bag: a pack's contents go where the pack goes.
// Only things worn on a back: a number on anything else may be a territory's
// mark (core.territory), not a pack's.
void bring_carried_containers(const Inventory &bag, ItemRegistry &items, ContainerSet &here)
{
    for (int i = 0; i < bag.size(); ++i) {
        const ItemStack &st = bag.at(i);
        if (st.empty() || items.get(st.item).wear != kWornBack) continue;
        const uint32_t id = st.tag & kTagIdMask;
        const ContainerAt at{(int)id, kCarriedBelow, 0};
        if (id == 0 || here.find(at) != nullptr) continue;
        for (size_t r = 0; r < g_router.rooms.size(); ++r) {
            RoomShared &o = g_router.rooms[r];
            if ((int)r == g_room || o.containers == nullptr || o.items == nullptr) continue;
            const Inventory *c = o.containers->find(at);
            if (c == nullptr) continue;
            const std::vector<uint8_t> bytes = encode_inventory(*o.items, *c);
            std::string err;
            Inventory  *into = here.open(at, c->size(), err);
            if (into == nullptr || !decode_inventory(items, bytes, *into, err)) {
                std::printf("server: could not bring pack %u from room '%s': %s\n", id,
                            o.name.c_str(), err.c_str());
                if (into != nullptr) here.erase(at);
                break;
            }
            here.touched(at);
            o.containers->erase(at);
            break;
        }
    }
}

// The file with this player's newest bag: the room that kept it last this run,
// or the file that says it was kept last. Null for no room with a file.
WorldDb *newest_bag_file(const uint8_t *identity)
{
    const auto it = g_bag_kept_by.find(identity_key(identity));
    if (it != g_bag_kept_by.end() && g_router.rooms[(size_t)it->second].db)
        return g_router.rooms[(size_t)it->second].db;
    WorldDb *best      = nullptr;
    int64_t  best_seen = -1;
    for (RoomShared &r : g_router.rooms) {
        if (r.db == nullptr || !r.db->is_open()) continue;
        const int64_t seen = r.db->bag_seen(identity);
        if (seen > best_seen) {
            best_seen = seen;
            best      = r.db;
        }
    }
    return best;
}

// Bags asked of a friend server and not here yet (admin::bag_ask). Such a
// player holds an empty bag, which nothing saves over the one in the file.
std::set<IdentityKey> g_bag_waiting;
// The bag kept here that a player was given when a friend did not answer in
// time (bag_late_here), as it was then: what came later is told apart by it.
std::map<IdentityKey, std::vector<uint8_t>> g_bag_fallback;

// The session in any room of this process that holds this player's bag.
net::Session *bag_holder(const uint8_t *identity, int &room)
{
    for (size_t i = 0; i < g_router.rooms.size(); ++i) {
        RoomShared &r = g_router.rooms[i];
        if (r.sessions == nullptr) continue;
        for (const net::PeerId peer : r.sessions->ready_peers()) {
            net::Session *sess = r.sessions->find_mutable(peer);
            if (sess && sess->has_identity && sess->holds_bag &&
                std::memcmp(sess->identity, identity, net::kSignPublicBytes) == 0) {
                room = (int)i;
                return sess;
            }
        }
    }
    return nullptr;
}

// What the backpacks in a bag hold (mods/backpack: a carried container
// numbered by the stack's tag), taken out of every room here to go with the
// bag to a friend server: a line each, NUMBER:HEX.
std::string take_packs(const Inventory &bag, const ItemRegistry &items)
{
    std::string out;
    for (const ItemStack &st : bag.slots()) {
        const uint32_t id = st.tag & kTagIdMask;
        if (st.empty() || id == 0 || items.get(st.item).wear != kWornBack) continue;
        const ContainerAt at{(int)id, kCarriedBelow, 0};
        for (RoomShared &o : g_router.rooms) {
            const Inventory *c = o.containers ? o.containers->find(at) : nullptr;
            if (c == nullptr || o.items == nullptr) continue;
            const std::vector<uint8_t> b = encode_inventory(*o.items, *c);
            out += std::to_string(id) + ":" + net::to_hex(b.data(), b.size()) + "\n";
            o.containers->erase(at);
            break;
        }
    }
    return out;
}

bool pack_here(uint32_t id)
{
    for (const RoomShared &o : g_router.rooms)
        if (o.containers && o.containers->find(ContainerAt{(int)id, kCarriedBelow, 0})) return true;
    return false;
}

// Numbers for backpacks that came from a friend, from where the mod, which
// counts from 1, does not reach: two servers' counters know nothing of each
// other.
constexpr uint32_t kFriendPacksFrom = 100000;

// A friend server wants this player's bag: the session holding it here lets
// go of it, as take_carried does for a room of this process, so it is neither
// used nor saved here any more.
bool bag_give_here(const uint8_t *identity, bool from_file, std::string &out, std::string &packs)
{
    if (g_bag_waiting.count(identity_key(identity))) return false;
    int                  room = 0;
    std::vector<uint8_t> bytes;
    if (net::Session *sess = bag_holder(identity, room)) {
        RoomShared &r   = g_router.rooms[(size_t)room];
        bytes           = encode_inventory(*r.items, sess->bag());
        packs           = take_packs(sess->bag(), *r.items);
        sess->bag()     = Inventory{};
        sess->holds_bag = false;
        if (r.bags_touched) r.bags_touched->insert((uint32_t)sess->id);
    } else {
        WorldDb    *db = from_file ? newest_bag_file(identity) : nullptr;
        Inventory   bag;
        std::string err;
        if (db == nullptr || g_router.rooms[0].items == nullptr ||
            !db->load_player_bag(identity, *g_router.rooms[0].items, bag, err))
            return false;
        bytes = encode_inventory(*g_router.rooms[0].items, bag);
        packs = take_packs(bag, *g_router.rooms[0].items);
    }
    out.assign(bytes.begin(), bytes.end());
    packs += admin::hidden(identity, "packs");   // those of backpacks this world does not know
    admin::set_hidden(identity, "", "packs");
    return true;
}

// The bag a friend sent. What this world does not know (an orphan here,
// core/item/item_codec.h) is kept aside, and what was kept aside and is known
// here comes out into the free squares.
void bag_take_here(const uint8_t *identity, const std::string &bag, const std::string &hidden,
                   const std::string &packs, const std::string &from)
{
    const IdentityKey k       = identity_key(identity);
    const bool        waiting = g_bag_waiting.erase(k) > 0;
    int               room    = 0;
    net::Session     *sess    = bag_holder(identity, room);
    const auto        late    = g_bag_fallback.find(k);
    if (sess && !waiting && late == g_bag_fallback.end()) {
        std::printf("server: '%s' has a bag here already; the one from %s is not taken\n",
                    sess->name.c_str(), from.c_str());
        return;
    }
    RoomShared   &r     = g_router.rooms[(size_t)room];
    ItemRegistry &items = *r.items;
    Inventory     got;
    std::string   err;
    if (!decode_inventory(items, (const uint8_t *)bag.data(), bag.size(), got, err)) {
        std::printf("server: could not read a bag from %s: %s\n", from.c_str(), err.c_str());
        got = Inventory{};
    }
    // Each with the square it had: a thing kept aside goes back there when
    // that square is free -- a backpack onto the back -- or into a free one.
    // Aside, it sits at that same square if it can.
    std::vector<std::pair<int, ItemStack>> extra;
    for (const std::string &h : {admin::hidden(identity), hidden}) {
        Inventory aside(kMaxStoredSlots);
        if (h.empty() || !decode_inventory(items, (const uint8_t *)h.data(), h.size(), aside, err)) continue;
        for (int i = 0; i < aside.size(); ++i)
            if (!aside.at(i).empty()) extra.push_back({i, aside.at(i)});
    }
    for (int i = 0; i < got.size(); ++i)
        if (!got.at(i).empty() && items.is_orphan(got.at(i).item)) {
            extra.push_back({i, got.at(i)});
            got.slot(i) = ItemStack{};
        }
    Inventory aside(kMaxStoredSlots);
    int       kept = 0;
    for (const auto &[was, st] : extra) {
        bool placed = false;
        if (!items.is_orphan(st.item)) {
            if (was < got.size() && got.at(was).empty() &&
                (was < Inventory::kDefaultSlots || items.get(st.item).wear != 0)) {
                got.slot(was) = st;
                placed        = true;
            }
            for (int i = 0; i < Inventory::kDefaultSlots && !placed; ++i)
                if (got.at(i).empty()) {
                    got.slot(i) = st;
                    placed      = true;
                }
        }
        if (placed) continue;
        int at = was < kMaxStoredSlots && aside.at(was).empty() ? was : -1;
        for (int i = 0; i < kMaxStoredSlots && at < 0; ++i)
            if (aside.at(i).empty()) at = i;
        if (at >= 0) {
            aside.slot(at) = st;
            ++kept;
        }
    }
    const std::vector<uint8_t> aside_bytes = kept > 0 ? encode_inventory(items, aside) : std::vector<uint8_t>{};
    admin::set_hidden(identity, std::string(aside_bytes.begin(), aside_bytes.end()));

    // Later than the wait: the player has been using the bag kept here since
    // (bag_late_here). What they picked up meanwhile goes into the one that
    // came; the rest of the copy here was the old bag.
    if (late != g_bag_fallback.end()) {
        Inventory before, now_bag;
        decode_inventory(items, late->second.data(), late->second.size(), before, err);
        g_bag_fallback.erase(late);
        if (sess)
            now_bag = sess->bag();
        else if (WorldDb *db = newest_bag_file(identity))
            db->load_player_bag(identity, items, now_bag, err);
        std::map<ItemId, int> gained;
        for (const ItemStack &st : now_bag.slots())
            if (!st.empty()) gained[st.item] += st.count;
        for (const ItemStack &st : before.slots())
            if (!st.empty()) gained[st.item] -= st.count;
        int added = 0;
        for (const auto &[item, n] : gained)
            if (n > 0) added += n - got.add(items, item, (uint16_t)std::min(n, 65535));
        std::printf("        '%s' bag from %s came late; %d thing(s) picked up meanwhile go into it\n",
                    sess ? sess->name.c_str() : "?", from.c_str(), added);
    }
    // The backpacks' contents, under a number of this server's: the one they
    // had if it is in the friends' range and free here, else the next free.
    // Those of a backpack kept aside wait with it.
    std::map<uint32_t, std::string> packs_in;
    for (const std::string &list : {packs, admin::hidden(identity, "packs")}) {
        std::istringstream lines(list);
        for (std::string l; std::getline(lines, l);)
            if (const size_t c = l.find(':'); c != std::string::npos)
                packs_in[(uint32_t)std::strtoul(l.c_str(), nullptr, 10)] = l.substr(c + 1);
    }
    if (r.containers != nullptr)
        for (int i = 0; i < got.size(); ++i) {
            ItemStack     &st = got.slot(i);
            const uint32_t id = st.tag & kTagIdMask;
            const auto     p  = packs_in.find(id);
            if (st.empty() || id == 0 || items.get(st.item).wear != kWornBack || p == packs_in.end()) continue;
            uint32_t to = id;
            if (to < kFriendPacksFrom || pack_here(to))
                for (to = kFriendPacksFrom; to < kTagIdMask && pack_here(to);) ++to;
            std::vector<uint8_t> b(p->second.size() / 2);
            const ContainerAt    at{(int)to, kCarriedBelow, 0};
            Inventory           *into = net::from_hex(p->second, b.data(), b.size())
                                            ? r.containers->open(at, stored_slots(b), err)
                                            : nullptr;
            if (into == nullptr || !decode_inventory(items, b, *into, err)) {
                std::printf("server: a backpack from %s did not fit here: %s\n", from.c_str(), err.c_str());
                if (into != nullptr) r.containers->erase(at);
                continue;
            }
            r.containers->touched(at);
            st.tag = (st.tag & ~kTagIdMask) | to;
            packs_in.erase(p);
        }
    std::string still;
    for (const auto &[id, hex] : packs_in) still += std::to_string(id) + ":" + hex + "\n";
    admin::set_hidden(identity, still, "packs");

    if (sess) {
        sess->bag() = got;
        if (r.bags_touched) r.bags_touched->insert((uint32_t)sess->id);
        std::printf("        '%s' bag comes from %s (%d thing(s) this world does not know, kept aside)\n",
                    sess->name.c_str(), from.c_str(), kept);
    } else if (r.db != nullptr && r.db->is_open()) {
        // Gone while it came: it waits in the file for their next visit.
        if (r.db->save_player_bag(identity, "", items, got, (int64_t)std::time(nullptr), err))
            g_bag_kept_by[identity_key(identity)] = room;
    }
}

// When this player's bag was last kept here: now, for one held this moment.
int64_t bag_seen_here(const uint8_t *identity)
{
    int room = 0;
    // Waiting: asking for it itself, and the file is what it has.
    if (!g_bag_waiting.count(identity_key(identity)) && bag_holder(identity, room))
        return (int64_t)std::time(nullptr);
    WorldDb *db = newest_bag_file(identity);
    return db ? db->bag_seen(identity) : -1;
}

// A player becoming a body here whose bag may be on a friend server
// (admin::bag_ask). Waiting from before the asking, so that bag_seen_here
// reads this server's file rather than the empty bag they hold now.
bool ask_friend_for_bag(const net::Session &sess)
{
    const IdentityKey k = identity_key(sess.identity);
    g_bag_fallback.erase(k);   // an old wait's, not this one's
    g_bag_waiting.insert(k);
    if (admin::bag_ask(admin_who(sess), sess.admitted_by_ticket ? sess.ticket_issuer : nullptr,
                       admin_hooks()))
        return true;
    g_bag_waiting.erase(k);
    return false;
}

// The friend did not send it: the bag here, as if nobody had been asked.
void bag_late_here(const uint8_t *identity)
{
    if (g_bag_waiting.erase(identity_key(identity)) == 0) return;
    int           room = 0;
    net::Session *sess = bag_holder(identity, room);
    if (sess == nullptr) return;
    RoomShared &r = g_router.rooms[(size_t)room];
    std::string err;
    if (WorldDb *from = newest_bag_file(identity)) from->load_player_bag(identity, *r.items, sess->bag(), err);
    g_bag_fallback[identity_key(identity)] = encode_inventory(*r.items, sess->bag());
    if (r.bags_touched) r.bags_touched->insert((uint32_t)sess->id);
}

} // namespace

int serve_room(int argc, char **argv)
{

    // Before anything else, so a crash while the world is being generated says
    // so too. The server had neither this nor the SIGQUIT dumper: a server that
    // segfaulted at three in the morning left an exit status, and the exit
    // status of a crash is the same whatever crashed.
    uint32_t seed   = 1;
    int      ticks  = 1000;
    int      ticks_run = 0;   // what the stats say: --ticks 0 runs until stopped
    int      extent = 128;
    bool     quiet  = false;
    int      port   = 0;   // 0 means the offline run: generate, tick, print a hash
    const char *world_file = nullptr;   // and no file means the world is not kept

    // How often a running server writes what changed, in seconds. 0 means only
    // when it stops, which a kill -9, a crash or a power cut never reaches.
    double save_every = 10.0;   // 2-13 ms a save on a played world (2026-10-09)

    // The server's long-term identity. Created on first run and kept, because a
    // key that changes every restart is a key nobody can pin.
    const char *key_file = nullptr;
    bool        encrypt  = true;

    // Whether this world has a pair of openings the server decides crossings
    // for. Off by default: a portal that appears in a world nobody asked for is
    // a hole in somebody's plaza.
    bool        portals  = false;

    // Whether a player has to say who they are. DESIGN.md 4.4. Off by default,
    // which is the LAN case; on, nobody joins without a key they hold.
    bool        require_identity = false;

    // Whether a portal in somebody else's world may look into this one.
    // DESIGN.md 4.4's second way in, and the one that needs no infrastructure.
    bool        allow_observers = true;

    // Where the openings lead, first one first. host:port[/N], or nothing for a
    // pair that leads back into this world. A second one makes the second
    // opening a mouth onto another world too.
    std::vector<const char *> portal_to;

    // How many bodies this world holds. DESIGN.md says 2-8; zero is allowed and
    // means a world that can be looked into and not entered, which is a thing
    // somebody may want and is also the only way to see a full world refuse a
    // crossing without eight clients on the machine.
    int max_players = 8;

    // Bodies in this world that nobody is connected to. Off by default and not
    // a game feature yet: the first entity that is not a player, so the code
    // that streams bodies can be shown streaming one. See core/ecs/mob.h.
    int mobs = 0;
    bool natural_mobs = true;   // creatures come out of the dark near players

    // Tickets, DESIGN.md 4.4. The key this world signs with, the key of the
    // world its opening leads to (the audience of what it signs), whose tickets
    // it honours, and whether a portal needs one at all.
    const char              *ticket_key_file = nullptr;
    std::vector<const char *> portal_peer_key;   // in the order of --portal-to
    std::vector<std::string> trusted;
    bool                     require_ticket = false;
    // Not the source tree any more, or not only: what is next to the program
    // wins, so a copied build reads the mods it was shipped with. core/paths.h
    // has the order and the reason.
    std::string mods_dir = data_dir("mods", CNP_SOURCE_ROOT "/mods");
    bool        use_mods = true;

    // Where the art is. A world serves its own media, so this is the server's
    // business now and not only the client's; empty means every block goes out
    // as a generated placeholder, which is a perfectly good world to look at
    // and a way to measure what the files are worth.
    std::string textures_dir = data_dir("assets/textures", CNP_SOURCE_ROOT "/assets/textures");
    std::string fallback_textures;   // where a file textures_dir has not got is looked for next

    // A day is twenty minutes unless somebody says otherwise. --day-length 20
    // is how the cycle gets looked at without booking the afternoon, and the
    // rate the time is sent at follows it on its own -- see kTellEvery.
    double day_length = cnp::kDayLength;
    double start_time = cnp::kNoon;

    // The address to listen on: every one this machine has, unless a world is
    // meant for this machine only -- the client's title screen serves its
    // worlds on 127.0.0.1.
    std::string bind;
    bool        list_names = false;   // --names: who owns what, then exit
    const char *free_name  = nullptr; // --free-name NAME: let it go, then exit

    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--seed") == 0 && i + 1 < argc)
            seed = (uint32_t)std::strtoul(argv[++i], nullptr, 10);
        else if (std::strcmp(argv[i], "--ticks") == 0 && i + 1 < argc)
            ticks = std::atoi(argv[++i]);
        else if (std::strcmp(argv[i], "--world") == 0 && i + 1 < argc)
            extent = std::atoi(argv[++i]);
        else if (std::strcmp(argv[i], "--mods") == 0 && i + 1 < argc)
            mods_dir = argv[++i];
        else if (std::strcmp(argv[i], "--no-mods") == 0)
            use_mods = false;
        else if (std::strcmp(argv[i], "--quiet") == 0)
            quiet = true;
        else if (std::strcmp(argv[i], "--listen") == 0 && i + 1 < argc)
            port = std::atoi(argv[++i]);
        else if (std::strcmp(argv[i], "--world-file") == 0 && i + 1 < argc)
            world_file = argv[++i];
        else if (std::strcmp(argv[i], "--save-every") == 0 && i + 1 < argc)
            save_every = std::atof(argv[++i]);
        else if (std::strcmp(argv[i], "--day-length") == 0 && i + 1 < argc)
            day_length = std::atof(argv[++i]);
        else if (std::strcmp(argv[i], "--time") == 0 && i + 1 < argc)
            start_time = std::atof(argv[++i]);
        else if (std::strcmp(argv[i], "--key-file") == 0 && i + 1 < argc)
            key_file = argv[++i];
        else if (std::strcmp(argv[i], "--portals") == 0)
            portals = true;
        else if (std::strcmp(argv[i], "--require-identity") == 0)
            require_identity = true;
        else if (std::strcmp(argv[i], "--no-observers") == 0)
            allow_observers = false;
        else if (std::strcmp(argv[i], "--max-players") == 0 && i + 1 < argc)
            max_players = std::atoi(argv[++i]);
        else if (std::strcmp(argv[i], "--mobs") == 0 && i + 1 < argc)
            mobs = std::atoi(argv[++i]);
        else if (std::strcmp(argv[i], "--no-natural-mobs") == 0)
            natural_mobs = false;
        else if (std::strcmp(argv[i], "--ticket-key") == 0 && i + 1 < argc)
            ticket_key_file = argv[++i];
        else if (std::strcmp(argv[i], "--portal-peer-key") == 0 && i + 1 < argc)
            portal_peer_key.push_back(argv[++i]);
        else if (std::strcmp(argv[i], "--trust") == 0 && i + 1 < argc)
            trusted.push_back(argv[++i]);
        else if (std::strcmp(argv[i], "--require-ticket") == 0)
            require_ticket = true;
        else if (std::strcmp(argv[i], "--portal-to") == 0 && i + 1 < argc)
            portal_to.push_back(argv[++i]);
        else if (std::strcmp(argv[i], "--textures") == 0 && i + 1 < argc)
            textures_dir = argv[++i];
        else if (std::strcmp(argv[i], "--no-textures") == 0)
            textures_dir.clear();
        else if (std::strcmp(argv[i], "--fallback-textures") == 0 && i + 1 < argc)
            fallback_textures = argv[++i];
        else if (std::strcmp(argv[i], "--no-encryption") == 0)
            encrypt = false;
        else if (std::strcmp(argv[i], "--bind") == 0 && i + 1 < argc)
            bind = argv[++i];
        else if (std::strcmp(argv[i], "--names") == 0)
            list_names = true;
        else if (std::strcmp(argv[i], "--free-name") == 0 && i + 1 < argc)
            free_name = argv[++i];
    }

    // Before a single column is made: a second server on this world file would
    // write over the first one's chunks, and finding that out after a minute of
    // world generation helps nobody.
    if (world_file != nullptr) {
        std::string lock_err;
        if (!lock_world_file(world_file, lock_err)) {
            std::printf("server: %s\n", lock_err.c_str());
            return 1;
        }
    }

    // The owner's two by hand, under the lock so no running server has the
    // file, and nothing generated.
    if (list_names || free_name) {
        if (world_file == nullptr) {
            std::printf("server: --names and --free-name want --world-file\n");
            return 1;
        }
        std::string err;
        if (free_name) {
            if (!WorldDb::free_name(world_file, free_name, err)) {
                std::printf("server: %s\n", err.c_str());
                return 1;
            }
            std::printf("server: '%s' is free; whoever joins under it next takes over its bag "
                        "and place\n",
                        free_name);
            return 0;
        }
        std::vector<WorldDb::NameRow> rows;
        if (!WorldDb::list_names(world_file, rows, err)) {
            std::printf("server: %s\n", err.c_str());
            return 1;
        }
        for (const WorldDb::NameRow &r : rows) {
            char when[32] = "?";
            const std::time_t t = (std::time_t)r.first_seen;
            std::strftime(when, sizeof(when), "%Y-%m-%d %H:%M", std::localtime(&t));
            std::printf("%-24s %s  since %s\n", r.name.c_str(),
                        net::key_fingerprint(r.identity).c_str(), when);
        }
        std::printf("%zu name(s)\n", rows.size());
        return 0;
    }

    // Line buffered, always. A server's stdout is nearly always a pipe or a
    // file, and C makes that fully buffered: "listening on UDP 25301" then sits
    // in a 4 KB buffer until the process exits or logs its way past it. A log
    // that arrives after the run is not a log.
    std::setvbuf(stdout, nullptr, _IOLBF, 0);

    ServerState s;
    s.extent = extent;
    s.seed   = seed;
    for (size_t k = 0; k < portal_to.size() && k < (size_t)net::kMaxMouths; ++k) {
        const std::string spec = portal_to[k];
        if (!spec.empty() && spec[0] == '@') s.mouth_rooms[k] = spec.substr(1, spec.rfind('/') - 1);
    }
    register_core_nodes(s.registry);

    // The region around the plaza and a ring of columns round it, generated
    // now, before the mods load: a mod building its scene while it loads writes
    // into it, and the welcome's hash is taken over it before any mod has. The
    // ring is there for the region's light, which reaches fifteen blocks past
    // its edge. Everything past the ring is made when somebody goes there
    // (TODO.md OPEN-35).
    const int  region       = columns_across(s.extent);
    const int  startup_cols = (region + 2) * (region + 2);
    const auto each_startup = [&](const std::function<void(ColumnPos)> &fn) {
        for (int cz = -1; cz <= region; ++cz)
            for (int cx = -1; cx <= region; ++cx) fn(ColumnPos{cx, cz});
    };

    const auto t_gen = std::chrono::steady_clock::now();
    Mapgen mapgen;
    s.mapgen = &mapgen;
    each_startup([&](ColumnPos c) {
        mapgen.generate_column(s.world, s.registry, seed, c, s.height);
        s.keeper.mark_generated(c, 0);
    });
    const double gen_seconds = seconds_since(t_gen);

    keep_columns(s);

    // The same context the core stages were handed, kept. A mod's stage runs
    // against it below, and core.terrain_height answers out of it -- so what a
    // mod is told about the ground is what the generator did, and not a second
    // derivation of the same noise.
    s.mapgen_ctx.reset(new MapgenContext(
        mapgen_make_context(s.world, s.registry, mapgen.biomes(), seed, s.height)));

    // Taken here, before the mods load, and this line is the whole fix for a
    // desync that was reported at every join. A client can reproduce the world
    // the generator made from a seed; it cannot reproduce the fourteen blocks
    // `spike` places at load, or the three pillars `beacons` raises. Hashing
    // after the mods have run advertises a number no honest client can match.
    const uint64_t mapgen_hash = world_hash(s.world, s.extent, s.height);

    if (!quiet) {
        std::printf("server: seed %u, a world with no edge; %d x %d x %d around the spawn made "
                    "now, %zu chunks, %.1f MB\n",
                    seed, s.extent, s.height, s.extent, s.world.chunk_count(),
                    (double)s.world.memory_bytes() / (1024.0 * 1024.0));
        std::printf("        generated in %.2f s (%zu stages over %d columns)\n", gen_seconds,
                    mapgen.stages().size(), startup_cols);
    }

    if (use_mods) {
        script_host host{};
        host.set_block     = host_set_block;
        host.set_facing    = host_set_facing;
        host.get_facing    = host_get_facing;
        host.get_block     = host_get_block;
        host.set_portal    = host_set_portal;
        host.log           = host_log;
        host.register_node = host_register_node;
        host.register_mapgen = host_register_mapgen;
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
        host.mob_model      = host_mob_model;
        host.world_sky      = host_world_sky;
        host.world_storm    = host_world_storm;
        host.mapgen_core    = host_mapgen_core;
        host.mapgen_only    = host_mapgen_only;
        host.portal_place   = host_portal_place;
        host.register_mob   = host_register_mob;
        host.spawn_mob      = host_spawn_mob;
        host.mob_tame       = host_mob_tame;
        host.mob_shear      = host_mob_shear;
        host.mob_lays       = host_mob_lays;
        host.mob_drop       = host_mob_drop;
        host.push_mobs      = host_push_mobs;
        host.monsters_near  = host_monsters_near;
        host.mobs_near      = host_mobs_near;
        host.mob_ids        = host_mob_ids;
        host.mob_pos        = host_mob_pos;
        host.mob_goto       = host_mob_goto;
        host.mob_going      = host_mob_going;
        host.mob_act        = host_mob_act;
        host.mob_face       = host_mob_face;
    host.mob_remove     = host_mob_remove;
    host.mob_kind       = host_mob_kind;
    host.body_player    = host_body_player;
    host.mob_love       = host_mob_love;
    host.mob_keep       = host_mob_keep;
    host.mob_shear_now  = host_mob_shear_now;
    host.mob_become     = host_mob_become;
    host.mob_voice      = host_mob_voice;
    host.mob_hurt       = host_mob_hurt;
    host.mob_place      = host_mob_place;
    host.mob_ride       = host_mob_ride;
    host.mob_velocity   = host_mob_velocity;
    host.mob_unseen     = host_mob_unseen;
    host.player_launch  = host_player_launch;
    host.mob_hunt       = host_mob_hunt;
    host.mob_attack     = host_mob_attack;
    host.mob_pierce     = host_mob_pierce;
        host.slow_mobs      = host_slow_mobs;
        host.mob_blow_every = host_mob_blow_every;
        host.mob_sight      = host_mob_sight;
        host.shoot          = host_shoot;
        host.shoot_arrow    = host_shoot_arrow;
        host.fall           = host_fall;
        host.replace_nodes  = host_replace_nodes;
        host.repair_held    = host_repair_held;
        host.node_falls     = host_node_falls;
        host.node_glass     = host_node_glass;
        host.blast          = host_blast;
        host.mob_breed      = host_mob_breed;
        host.players       = host_players;
        host.find_path     = host_find_path;
        host.find_nodes    = host_find_nodes;
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
        host.movement      = host_movement;
        host.body_movement = host_body_movement;
        host.drive         = host_drive;
        host.body_drive    = host_body_drive;
        host.body_boat     = host_body_boat;
        host.body_cart     = host_body_cart;
        host.side          = host_side;
        host.player_name   = host_player_name;
        host.player_key    = host_player_key;
        host.world_spawn   = host_world_spawn;
        host.player_carry   = host_player_carry;
        host.player_carried = host_player_carried;
        host.body_taken     = host_body_taken;
        host.held_item     = host_held_item;
        host.worn_item     = host_worn_item;
        host.player_fly    = host_player_fly;
        host.body_width    = host_body_width;
        host.eye_height    = host_eye_height;
        host.parkour       = host_parkour;
        host.ledges        = host_ledges;
        host.player_pos    = host_player_pos;
        host.player_look   = host_player_look;
        host.player_place  = host_player_place;
        host.player_lie    = host_player_lie;
        host.player_clip   = host_player_clip;
        host.player_wear   = host_player_wear;
        host.play_sound    = host_play_sound;
        host.raycast       = host_raycast;
        host.get_portal    = host_get_portal;
        host.portal_room   = host_portal_room;
        host.portal_open   = host_portal_open;
        host.portal_close  = host_portal_close;
        host.portal_seal   = host_portal_seal;
        host.portal_link   = host_portal_link;
        host.take_item     = host_take_item;
        host.bag_count     = host_bag_count;
        host.owned_area    = host_owned_area;
        host.bag_tagged    = host_bag_tagged;
        host.swap_item     = host_swap_item;
        host.owned_cell    = host_owned_cell;
        host.owned_at      = host_owned_at;
        host.owned_member  = host_owned_member;
        host.player_factors = host_player_factors;
        host.take_tagged   = host_take_tagged;
        host.give_item     = host_give_item;
        host.meta_set      = host_meta_set;
        host.meta_get      = host_meta_get;
        host.notice        = host_notice;
        host.chat          = host_chat;
        host.command       = host_command;
        host.has_perm      = host_has_perm;
        host.role          = host_role;
        host.page_register = host_page_register;
        host.page_show     = host_page_show;
        host.is_muted      = host_is_muted;
        host.storage_set   = host_storage_set;
        host.storage_get   = host_storage_get;
        host.shared_storage_set = host_shared_storage_set;
        host.shared_storage_get = host_shared_storage_get;
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
        host.item_returns     = host_item_returns;
        host.worn_id          = host_worn_id;
        host.worn_set_id      = host_worn_set_id;
        host.held_id          = host_held_id;
        host.held_set_id      = host_held_set_id;
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
        host.node_colour  = host_node_colour;
        host.light          = host_light;
        host.block_light    = host_block_light;
        host.node_turns     = host_node_turns;
        host.loaded         = host_loaded;
        host.keep_loaded    = host_keep_loaded;
        host.ride           = host_ride;
        host.craft_make     = host_craft_make;
        host.craft_keys     = host_craft_keys;
        host.craft_pose     = host_craft_pose;
        host.craft_push     = host_craft_push;
        host.craft_mount    = host_craft_mount;
        host.craft_aim      = host_craft_aim;
        host.craft_seat     = host_craft_seat;
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
        host.ud            = &s;

        s.mods.set_host(host);
        s.mods.set_side(ModSide::Server);   // a client's mod is offered, not run (OPEN-49)

        // A mod's systems, wired to the one place that knows what a mod is: who
        // is running while a system runs, what to do with a system that errors,
        // and letting go of a VM that goes away. Each of these is the event
        // bus's rule said again for systems, and said HERE rather than inside
        // either layer -- src/script must not know what a flecs world is, and
        // core/ must not know what a mod is.
        s.lua_ecs.set_scope([&s](script_vm *vm) { s.mods.enter_vm(vm); },
                            [&s](script_vm *vm) { s.mods.leave_vm(vm); });
        s.lua_ecs.set_error([&s](const std::string &, script_vm *vm, const std::string &system,
                                 const std::string &error) {
            s.mods.disable_vm(vm, "system '" + system + "': " + error);
        });
        // A VM that went -- a mod disabled, or one that failed halfway through
        // loading after it asked for a stage -- leaves its stages dead. They
        // stay listed, their names still taken, but point at no VM: matched by
        // address they would run in the next mod's VM, which can be given the
        // same one, as whatever function sits at that ref.
        s.mods.set_vm_gone([&s](script_vm *vm) {
            s.lua_ecs.drop_vm(vm);
            for (ServerState::LuaStage &st : s.mapgen_stages)
                if (st.vm == vm) st.vm = nullptr;
        });

        // The engine's own events, declared before the mods load or the first
        // mod to listen for one would be told it does not exist.
        // `player` is who did it. Without it a mod can react to a block being
        // broken and has no way to answer the person who broke it -- which is
        // what ui_show needs to put a panel on one screen. TODO OPEN-8.
        const std::vector<EventField> block_fields = {
            {"x", SCRIPT_V_NUMBER}, {"y", SCRIPT_V_NUMBER}, {"z", SCRIPT_V_NUMBER},
            {"id", SCRIPT_V_NUMBER}, {"player", SCRIPT_V_NUMBER},
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
        {
            // A block core.fall let go of, come to rest: `fell` nodes down.
            EventSchema landed;
            landed.name   = "core:landed";
            landed.owner  = "core";
            landed.fields = {{"x", SCRIPT_V_NUMBER}, {"y", SCRIPT_V_NUMBER}, {"z", SCRIPT_V_NUMBER},
                             {"id", SCRIPT_V_NUMBER}, {"fell", SCRIPT_V_NUMBER}};
            s.mods.events().define(landed, err);
        }

        // A right click on a block within reach, whatever is in the hand.
        // Cancellable, and that is what makes it useful: a mod that cancels has
        // CLAIMED the click, so the placement behind it does not happen.
        // Without that, opening a chest with a block in hand would open the
        // chest and build a block on top of it.
        //
        // One field more than the others, and it is the one M8 needed: `held`
        // is what the clicker was HOLDING, by name, empty for an empty hand.
        // `id` is the block that was clicked -- a pedestal reading it to find
        // out what it was being offered was reading itself. The click is the
        // only moment the engine knows the hand at all (net::EditPlan::
        // use_holding says why), so this is where it says so.
        EventSchema used;
        used.name        = "core:used";
        used.owner       = "core";
        used.fields      = block_fields;
        used.fields.push_back({"held", SCRIPT_V_STRING});
        used.cancellable = true;
        s.mods.events().define(used, err);
        // The world is as it is going to be. Emitted once, after the world file
        // has been read, and it exists because of the order this has to happen
        // in: the file is opened AFTER the mods register their blocks, since
        // the content hash it is checked against is the final table -- so a mod
        // running at load looks at the GENERATED world and never at the stored
        // one. A clock that read its own count while loading read zero every
        // time, and a mod checking whether it had already built found nothing
        // and built again on top of what was about to be loaded over it.
        //
        // No fields: what a mod does here is look at the world, which it
        // already has calls for. It fires even when there is no world file at
        // all, so a mod may rely on it happening exactly once.
        EventSchema ready;
        ready.name  = "core:world_ready";
        ready.owner = "core";
        s.mods.events().define(ready, err);

        // What arrived on a mod's own channel. `mod` is who it is for, which is
        // the channel it came in on read back as a name: a mod hears its own
        // messages and nobody else's, and the field is there so a mod running
        // beside another can say so in a log without guessing.
        EventSchema said;
        said.name   = "core:net_message";
        said.owner  = "core";
        said.fields = {{"player", SCRIPT_V_NUMBER},
                       {"mod", SCRIPT_V_STRING},
                       {"text", SCRIPT_V_STRING}};
        s.mods.events().define(said, err);

        // An opening that leads to one of this world's moved, in another room
        // of this server (core.portal_place there): `index` is which of this
        // world's openings it comes out of, the rest is its new shape.
        EventSchema far;
        far.name   = "core:far_moved";
        far.owner  = "core";
        far.fields = {{"index", SCRIPT_V_NUMBER},  {"x", SCRIPT_V_NUMBER},
                      {"y", SCRIPT_V_NUMBER},      {"z", SCRIPT_V_NUMBER},
                      {"axis", SCRIPT_V_NUMBER},   {"sign", SCRIPT_V_NUMBER},
                      {"half_w", SCRIPT_V_NUMBER}, {"half_h", SCRIPT_V_NUMBER}};
        s.mods.events().define(far, err);

        EventSchema tick;
        tick.name   = "core:tick";
        tick.owner  = "core";
        tick.fields = {{"dt", SCRIPT_V_NUMBER}};
        s.mods.events().define(tick, err);

        // A click on a mod's panel. Declared here even though nothing on a
        // server can emit one yet -- panels do not cross the wire -- because a
        // mod that listens for an event nobody declared does not load at all,
        // and a mod that runs on a client and not on a server is two games.
        EventSchema click;
        click.name   = "core:ui_click";
        click.owner  = "core";
        click.fields = {{"panel", SCRIPT_V_STRING},
                        {"id", SCRIPT_V_STRING},
                        {"player", SCRIPT_V_NUMBER},
                        {"text", SCRIPT_V_STRING}};   // a field's, on Enter; "" for a button
        s.mods.events().define(click, err);

        // A blow a player landed on a body. After the fact: the points are off.
        EventSchema hit;
        hit.name   = "core:hit";
        hit.owner  = "core";
        hit.fields = {{"player", SCRIPT_V_NUMBER},
                      {"target", SCRIPT_V_NUMBER},
                      {"damage", SCRIPT_V_NUMBER},
                      {"killed", SCRIPT_V_BOOL}};
        s.mods.events().define(hit, err);

        // A right click on a worker creature (core.mob_goto's kind): which one,
        // by the id core.spawn_mob gave, and what was in the hand, by name.
        EventSchema used_mob;
        used_mob.name   = "core:used_mob";
        used_mob.owner  = "core";
        used_mob.fields = {{"player", SCRIPT_V_NUMBER},
                           {"target", SCRIPT_V_NUMBER},
                           {"held", SCRIPT_V_STRING}};
        s.mods.events().define(used_mob, err);

        // A /command a player typed, to the mod that declared it
        // (core.command) and nobody else. `args` is the rest of the line.
        EventSchema command;
        command.name   = "core:command";
        command.owner  = "core";
        command.fields = {{"player", SCRIPT_V_NUMBER},
                          {"name", SCRIPT_V_STRING},
                          {"args", SCRIPT_V_STRING}};
        s.mods.events().define(command, err);

        // A player opened a page this mod registered (core.page_register):
        // the mod answers with core.page_show. To that mod alone.
        EventSchema page_open;
        page_open.name   = "core:page_open";
        page_open.owner  = "core";
        page_open.fields = {{"player", SCRIPT_V_NUMBER}, {"page", SCRIPT_V_STRING}};
        s.mods.events().define(page_open, err);

        // An act:ID link on a page this mod showed, with a field's text.
        EventSchema page_action;
        page_action.name   = "core:page_action";
        page_action.owner  = "core";
        page_action.fields = {{"player", SCRIPT_V_NUMBER},
                              {"page", SCRIPT_V_STRING},
                              {"id", SCRIPT_V_STRING},
                              {"text", SCRIPT_V_STRING}};
        s.mods.events().define(page_action, err);

        EventSchema watched;
        watched.name   = "core:watched";
        watched.owner  = "core";
        watched.fields = {{"x", SCRIPT_V_NUMBER}, {"y", SCRIPT_V_NUMBER}, {"z", SCRIPT_V_NUMBER}};
        s.mods.events().define(watched, err);

        // Which directory, and why that one. A build that reads somebody else's
        // mods has to be able to say so in its own log rather than under an
        // strace: "next to the program" and "the tree it was built from" are
        // two different machines' worth of difference.
        std::printf("mods from %s (%s)\n", mods_dir.c_str(),
                    data_dir_reason(mods_dir, CNP_SOURCE_ROOT "/mods").c_str());

        // A mod builds its scene while it loads -- mods/palette writes twenty
        // cells, and somebody's structure mod would write ten thousand -- so
        // this is the other place the light is owed once rather than per cell.
        int running = 0;
        {
            LightRun lights(s);
            running = s.mods.load_dir(mods_dir);
        }

        // core.mapgen_only: the stages of the mods loaded here for what they
        // hold, not for the ground they would make.
        if (!s.mapgen_owners.empty()) {
            std::erase_if(s.mapgen_stages, [&](const ServerState::LuaStage &st) {
                if (s.mapgen_owners.count(st.owner)) return false;
                if (!quiet)
                    std::printf("        mapgen stage '%s' of %s dropped: not this world's ground\n",
                                st.name.c_str(), st.owner.c_str());
                return true;
            });
        }

        // Every block gets an item form now that the mods have had their say.
        // Here and not inside register_node: a mod may register a block and
        // then override it in the same pass, and adopting each one as it
        // arrives would mint an item for a definition that no longer exists.
        const size_t adopted = s.items.adopt_blocks(s.registry);
        if (!quiet)
            std::printf("        %d mods running, %zu block(s) can be held, %zu item(s) in all, "
                        "%zu recipe(s)\n",
                        running, adopted, s.items.size() - 1, s.recipes.size());

        // Recipes naming something nothing registered. Said once, here, because
        // the alternative is a recipe that quietly never works and a mod author
        // reading their own arithmetic looking for a bug that is not there.
        for (const Recipe *r : s.recipes.unresolved(s.items))
            std::printf("server: %s's recipe for '%s' names something no mod registered\n",
                        r->owner.empty() ? "the engine" : r->owner.c_str(), r->result.c_str());

        // A world whose mods make all of it: the engine's ground made before
        // they loaded goes, and nothing of the engine's is made from here on.
        if (!s.core_mapgen) {
            each_startup([&](ColumnPos c) {
                for (int y = 0; y < s.height; ++y)
                    for (int z = c.z * kChunkSize; z < (c.z + 1) * kChunkSize; ++z)
                        for (int x = c.x * kChunkSize; x < (c.x + 1) * kChunkSize; ++x)
                            s.world.set_block(x, y, z, kAir);
            });
            mapgen.clear_stages();
            if (!quiet) std::printf("        no engine terrain: the mods make this world\n");
        }

        // The mods' own stages of world generation, now that every mod has had
        // its chance to register one. Three things about WHEN, each of them a
        // different way of getting it wrong:
        //
        //   - After the whole directory has loaded, not as each stage arrives.
        //     What orders these is the order they were ASKED for, and a stage
        //     that ran on registration would run before a mod later in the
        //     directory had said anything at all.
        //   - Before the world file below, because a stored chunk wins over a
        //     generated one. A stage is part of generating the world, not part
        //     of starting the server: a world with a file gets its stage run
        //     and then written over, which is what happens to the core stages
        //     too.
        //   - Before core:world_ready, which is the event for a mod that wants
        //     to look at the finished world. A stage is not that -- it is what
        //     the world is made of.
        if (!s.mapgen_stages.empty()) {
            const auto t_stage = std::chrono::steady_clock::now();

            // Counted, and said out loud below. "Some stages ran" is not the
            // claim worth making -- a pass that stopped at the first one that
            // threw would report exactly the same line as one that ran them
            // all. The light for everything they wrote comes when the region
            // is lit, below the world file.
            size_t failed = 0;
            each_startup([&](ColumnPos c) { failed += run_mod_stages(s, c); });

            if (!quiet)
                std::printf("        %zu mapgen stage(s) from mods over %d column(s), %zu "
                            "failure(s), in %.2f s\n",
                            s.mapgen_stages.size(), startup_cols, failed, seconds_since(t_stage));
        }

        // Whether any ran or not: from here on a mod asking for one is asking
        // too late, and is told so rather than left with a stage that never
        // runs.
        s.mapgen_finished = true;
    }

    // And the modifiers close with them, for the same reason and at the same
    // moment: what an ABM names may be registered by a mod that loads later, so
    // the names are resolved once the whole directory has had its say.
    {
        // The seed a chance is drawn against, so two runs of one world fire
        // the same cells.
        s.modifiers.set_seed(seed);

        std::vector<std::string> errors;
        s.modifiers.close_registration(s.registry, errors);
        for (const std::string &e : errors)
            std::fprintf(stderr, "modifier disabled: %s\n", e.c_str());

        if (!quiet && (!s.modifiers.abms().empty() || !s.modifiers.lbms().empty()))
            std::printf("        %zu active and %zu loading modifier(s) from mods\n",
                        s.modifiers.abms().size(), s.modifiers.lbms().size());

        // A column's loading modifiers are owed their visit when it is lit,
        // which is when it has loaded as far as anything can tell (light_column).
        // And the active ones sweep lit columns only.
        s.modifiers.set_active([&s](ChunkPos p) { return s.keeper.lit_chunk(p); });
        s.modifiers.set_near([&s]() {
            std::vector<ChunkPos> near;
            if (!s.sessions) return near;
            for (const net::PeerId peer : s.sessions->ready_peers()) {
                const net::Session *sess = s.sessions->find(peer);
                if (!sess || !sess->authority().valid) continue;
                const MovementState &at = sess->authority();
                near.push_back(World::chunk_of((int)std::floor(at.x), (int)std::floor(at.y),
                                               (int)std::floor(at.z)));
            }
            return near;
        });
    }

    // From here a write goes into a lit column or waits for one, mods or no mods.
    s.keeper.set_loading(false);

    // The world file, opened after the mods have registered their blocks: the
    // table its chunks are carried into has to be the final one, or a server
    // would write a file under a table it then does not have.
    WorldDb db;
    if (world_file) {
        std::string err;
        if (!db.open(world_file, seed, s.registry, err)) {
            std::printf("server: %s\n", err.c_str());
            return 1;
        }
        if (db.chunks_remapped() > 0)
            std::printf("server: the block table changed since the world was saved; %lld stored "
                        "chunk(s) carried into this one by name\n",
                        (long long)db.chunks_remapped());
        if (!db.placeholders().empty()) {
            std::printf("server: the world holds blocks no loaded mod registers; they are magenta "
                        "until their mod is back:");
            for (const std::string &name : db.placeholders()) std::printf(" %s", name.c_str());
            std::printf("\n");
        }

        // Stored columns win over generated ones, and that is the whole point
        // of a world file -- what happened in it is not what the generator
        // says. The region and its ring only: the rest of the file is read a
        // column at a time, when somebody goes there (fill_column). Installed
        // with install_chunk, the one place that copies a chunk into a world,
        // facings and all.
        s.keeper.set_db(&db);
        std::string load_err;
        int64_t     n = 0;
        each_startup([&](ColumnPos c) { n += (int64_t)s.keeper.lay_stored(c, load_err); });
        if (!load_err.empty()) std::printf("server: %s\n", load_err.c_str());

        // The chests, after the item table is final. A stored stack names an
        // item and a name this build has never heard of is minted here as an
        // orphan -- so this has to run after adopt_blocks, or every block form
        // in every chest would be minted as an orphan a moment before the
        // registry learned the real one.
        std::string   chest_err;
        const int64_t chests = db.load_containers(s.items, s.containers, chest_err);
        if (!chest_err.empty()) std::printf("server: %s\n", chest_err.c_str());

        // And what the cells remember. Before core:world_ready below, which is
        // the whole point of that event: a mod reads this the moment it fires
        // and not while it is loading, when none of it is here yet.
        std::string   meta_err;
        const int64_t remembered = db.load_node_meta(s.node_meta, meta_err);
        if (!meta_err.empty()) std::printf("server: %s\n", meta_err.c_str());
        if (remembered > 0 && !quiet)
            std::printf("        %lld thing(s) remembered by %zu cell(s)\n",
                        (long long)remembered, s.node_meta.cells());

        // And what the mods kept, for the same event to find.
        std::string   store_err;
        const int64_t kept = db.load_mod_store(s.mod_store, store_err);
        if (!store_err.empty()) std::printf("server: %s\n", store_err.c_str());
        if (kept > 0 && !quiet) std::printf("        %lld key(s) kept by mods\n", (long long)kept);

        const WorldDbStats st = db.stats();
        std::printf("server: world file %s, %lld chunks stored (%lld loaded), %lld players, "
                    "%lld containers (%lld loaded), %.1f KB\n",
                    world_file, (long long)st.chunks, (long long)n, (long long)st.players,
                    (long long)st.containers, (long long)chests,
                    (double)st.bytes_on_disk / 1024.0);

    }

    // The region's light, now that its blocks are final: generated, written
    // over by the mods as they loaded, and by the file. The ring round it has
    // blocks, so this is the light and nothing else -- and it is what makes the
    // region's columns lit, which a column has to be before it is sent, kept or
    // swept.
    {
        const auto t_light = std::chrono::steady_clock::now();
        for (int cz = 0; cz < region; ++cz)
            for (int cx = 0; cx < region; ++cx) s.keeper.make_lit(ColumnPos{cx, cz}, 0);
        s.keeper.apply_ready_writes();
        if (!quiet)
            std::printf("        lit %d column(s) in %.2f s\n", region * region,
                        seconds_since(t_light));
    }

    // The region once, now, and from here on only what changes. What the
    // generator made and the file did not have goes in with it, so that
    // "unsaved" means changed since the file matched the world -- and not
    // "never written", which is every chunk of a new world and would make the
    // first save during play the size of the region.
    if (db.is_open()) {
        std::string           first_err;
        const auto            t_first = std::chrono::steady_clock::now();
        std::vector<ChunkPos> first   = s.keeper.take_unsaved_lit();
        if (!db.save_chunks(s.world, first, first_err)) {
            std::printf("server: could not save the world: %s\n", first_err.c_str());
            for (const ChunkPos p : first) s.world.mark_dirty(p);
        } else if (!quiet) {
            std::printf("        wrote %zu chunks in %.0f ms, and from here on what changes\n",
                        first.size(), seconds_since(t_first) * 1000.0);
        }
    }

    // The world is now what it is going to be: generated, then whatever the
    // file had written over it, then what the cells remember. Outside the
    // `if (world_file)` above on purpose -- a mod that waits for this must not
    // wait for ever on a server started without one.
    // In a LightRun: the track and the chambers are built here, thousands of
    // cells, and a relight per cell was half a minute of a world's start.
    {
        LightRun    lights(s);
        bool        ignored = false;
        std::string ready_err;
        if (!s.mods.events().emit("core:world_ready", nullptr, 0, &ignored, ready_err))
            std::fprintf(stderr, "emit core:world_ready: %s\n", ready_err.c_str());
    }

    // The tick. Fixed step, because a world hash after a thousand ticks means
    // nothing if the ticks were different lengths on different machines. It is
    // also the authority rate of DESIGN.md 3.3, decoupled from any client's
    // frame rate -- including when nobody is connected, as below.
    constexpr double kTickSeconds = 1.0 / 20.0;

    auto tick_once = [&]() {
        // Everything below can write cells, so the light for all of it is done
        // once, when this scope ends (TODO OPEN-29).
        LightRun lights(s);

        // The mods' own systems, before their on_step: a mod that reads what
        // its systems counted should read this tick's numbers and not the
        // previous tick's.
        s.lua_ecs.step(kTickSeconds, script_call_system);

        s.mods.step(kTickSeconds);

        // What happens to cells on their own. After the mods' own step so that
        // a modifier's action and a mod's tick are not in the middle of each
        // other, and with a budget: a mod that asks for more than this runs
        // slower than it asked, and the server stays on time.
        step_modifiers(s, kTickSeconds);

        script_field dt_field{"dt", SCRIPT_V_NUMBER, kTickSeconds, nullptr, 0};
        bool         cancelled = false;
        std::string  err;

        // What watched cells were written since the last tick; a handler that
        // writes one again is heard next tick, not in this loop.
        std::vector<uint64_t> hits;
        hits.swap(s.watch_hits);
        s.watch_hit_seen.clear();
        for (const uint64_t k : hits) {
            int wx = 0, wy = 0, wz = 0;
            ServerState::unpacked(k, wx, wy, wz);
            script_field wf[3] = {{"x", SCRIPT_V_NUMBER, (double)wx, nullptr, 0},
                                  {"y", SCRIPT_V_NUMBER, (double)wy, nullptr, 0},
                                  {"z", SCRIPT_V_NUMBER, (double)wz, nullptr, 0}};
            s.mods.events().emit("core:watched", wf, 3, nullptr, err);
        }

        if ((size_t)g_room < g_router.far_moved.size()) {
            std::vector<Router::FarMoved> heard;
            heard.swap(g_router.far_moved[(size_t)g_room]);
            for (const Router::FarMoved &f : heard) {
                const Portal &p   = f.where;
                script_field  ff[8] = {
                    {"index", SCRIPT_V_NUMBER, (double)f.portal, nullptr, 0},
                    {"x", SCRIPT_V_NUMBER, (double)p.center.x, nullptr, 0},
                    {"y", SCRIPT_V_NUMBER, (double)p.center.y, nullptr, 0},
                    {"z", SCRIPT_V_NUMBER, (double)p.center.z, nullptr, 0},
                    {"axis", SCRIPT_V_NUMBER, (double)(int)p.axis, nullptr, 0},
                    {"sign", SCRIPT_V_NUMBER, (double)p.normal_sign, nullptr, 0},
                    {"half_w", SCRIPT_V_NUMBER, (double)p.half_w, nullptr, 0},
                    {"half_h", SCRIPT_V_NUMBER, (double)p.half_h, nullptr, 0}};
                s.mods.events().emit("core:far_moved", ff, 8, nullptr, err);
            }
        }

        s.mods.events().emit("core:tick", &dt_field, 1, &cancelled, err);
    };

    const auto t_tick = std::chrono::steady_clock::now();

    // What changed since the file last had it: the chunks that did, and the
    // chests and the cells' memory whole, because those are written whole (see
    // core/storage/world_db.h). `loud` is the stop, which says what it wrote
    // even when it wrote nothing; a save during play speaks only when it wrote
    // a chunk, or the log is a line every thirty seconds for ever.
    //
    // Counted in ticks, not seconds, so an offline run saves on the same ticks
    // every time and can be checked against itself.
    //
    // One transaction: a crash leaves the chunks, the chests, what the cells
    // remember and what the mods keep all from the same save.
    const int save_ticks =
        save_every > 0.0 ? std::max(1, (int)std::lround(save_every / kTickSeconds)) : 0;

    // The tamed pets, into the store the mods keep theirs in, under "core":
    // one key a pet, and the keys past the last one emptied, which is how the
    // store forgets. Saved with it, below.
    const auto keep_pets = [&]() {
        if (s.sessions == nullptr) return;
        const std::vector<std::string> lines = cnp::ecs::pets_to_lines(s.sessions->ecs(), s.mob_kinds);
        std::string why;
        size_t      i = 0;
        for (; i < lines.size() && i < 1024; ++i)
            s.mod_store.set("core", "pet:" + std::to_string(i), lines[i], why);
        for (size_t k = i; k < s.pets_kept; ++k) s.mod_store.set("core", "pet:" + std::to_string(k), "", why);
        s.pets_kept = i;
    };

    const auto save_world = [&](bool loud) {
        if (!db.is_open()) return;
        keep_pets();

        const auto            t_save  = std::chrono::steady_clock::now();
        // Lit columns only: see ColumnKeeper::take_unsaved_lit.
        std::vector<ChunkPos> changed = s.keeper.take_unsaved_lit();

        std::string err;
        const bool  together = db.begin(err);
        if (!together) std::printf("server: saving without one transaction: %s\n", err.c_str());
        if (!db.save_chunks(s.world, changed, err)) {
            std::printf("server: could not save the world: %s\n", err.c_str());
            // Owed again, or a chunk whose write failed once is never tried again.
            for (const ChunkPos p : changed) s.world.mark_dirty(p);
        }

        std::string chest_err;
        if (!db.save_containers(s.items, s.containers, chest_err))
            std::printf("server: could not save the containers: %s\n", chest_err.c_str());

        std::string meta_err;
        if (!db.save_node_meta(s.node_meta, meta_err))
            std::printf("server: could not save what the cells remember: %s\n",
                        meta_err.c_str());

        std::string                 store_err;
        const std::vector<ModStore::Row> kept = s.mod_store.take_touched();
        if (!db.save_mod_store(kept, store_err)) {
            std::printf("server: could not save what the mods keep: %s\n", store_err.c_str());
            for (const ModStore::Row &r : kept) s.mod_store.touch(r);   // owed again
        }

        std::string commit_err;
        if (together && !db.commit(commit_err)) {
            std::printf("server: could not save the world: %s\n", commit_err.c_str());
            for (const ChunkPos p : changed) s.world.mark_dirty(p);
            for (const ModStore::Row &r : kept) s.mod_store.touch(r);
        }

        if (quiet || (!loud && changed.empty())) return;
        std::printf("        saved %zu changed chunk(s) of %zu, %zu container(s), %zu thing(s) "
                    "across %zu cell(s), in %.1f ms\n",
                    changed.size(), s.world.chunk_count(), s.containers.size(),
                    s.node_meta.fields(), s.node_meta.cells(), seconds_since(t_save) * 1000.0);
    };

    if (port <= 0) {
        // Offline: no socket, run the ticks as fast as they will go and print
        // the hash. This is the CI shape, and it stays the default.
        //
        // No session table either, and the entities' world is the session
        // table's: a mod's entities get one of their own here, or
        // core.ecs.spawn answers with nothing (TODO.md OPEN-21).
        flecs::world offline_ecs;
        s.lua_ecs.set_world(&offline_ecs);
        for (int i = 0; i < ticks && !g_stop; ++i) {
            tick_once();
            ++ticks_run;

            // Nobody is here to want a column, and nothing is let go; a mod that
            // wrote past the region still gets its column made.
            ++s.column_clock;
            if (s.keeper.pending_writes() > 0) {
                LightRun lights(s);
                advance_columns(s, {}, {}, std::numeric_limits<double>::infinity());
            }

            if (save_ticks > 0 && (i + 1) % save_ticks == 0) save_world(false);
        }
        s.lua_ecs.set_world(nullptr);
    } else {
        net::ServerConfig config;
        config.seed        = seed;
        config.mapgen_hash = mapgen_hash;
        // Taken from the registry after the mods have registered theirs, so a
        // server with an extra mod is a different table and says so at the
        // handshake instead of at the first wrongly drawn chunk.
        config.engine_version = kEngineVersion;
        config.content_hash   = registry_content_hash(s.registry);
        config.nodes          = node_table(s.registry);
        config.models         = s.models.table();
        config.movement       = s.movement;   // TODO.md OPEN-44: what the mods declared

        // The shape of every block that has one, and of every block that turns.
        // Built here rather than by a helper beside node_table(), for the reason
        // the item table below gives: there is nothing to invent, only to copy.
        //
        // A plain cube that does not turn says nothing, which is nearly every
        // block: the list is a handful of entries in a table of thousands.
        for (size_t id = 0; id < s.registry.size(); ++id) {
            const NodeDef &d = s.registry.get((BlockId)id);
            if (d.parts.empty() && d.turn == NodeTurn::None && d.model == kNoModel && d.piece.plan == 0)
                continue;

            net::NodeShapeDesc desc;
            desc.node  = (uint16_t)id;
            desc.turn  = (uint8_t)d.turn;
            desc.model = (uint16_t)d.model;
            desc.piece = d.piece;
            desc.boxes.reserve(d.parts.size());
            for (const NodeDef::Box &b : d.parts) {
                net::NodeShapeDesc::Box w;
                for (int a = 0; a < 3; ++a) {
                    w.min[a] = b.min[a];
                    w.max[a] = b.max[a];
                }
                w.part = b.part;
                desc.boxes.push_back(std::move(w));
            }
            config.shapes.push_back(std::move(desc));
        }


        // The item table, in id order like the block table. Built here rather
        // than by a helper next to node_table(): there is no placeholder logic
        // to share -- an item a client has never heard of still names a block
        // in a table it HAS adopted, so there is nothing to invent.
        config.items.reserve(s.items.size());
        for (size_t id = 0; id < s.items.size(); ++id) {
            const ItemDef &d = s.items.get((ItemId)id);
            net::ItemDesc  w;
            w.name      = d.name;
            w.places    = (uint16_t)d.places;
            w.max_stack = d.max_stack;
            w.dig_level = d.dig_level;
            w.dig_speed = d.dig_speed;
            w.digs      = d.digs;
            w.wear       = d.wear;
            w.armor      = d.armor;
            w.durability = d.durability;
            w.arms       = d.arms;
            config.items.push_back(std::move(w));
        }

        // And how each block digs, in the block table's order: the client times
        // the bar under the crosshair with these (protocol 36).
        config.node_digs.reserve(config.nodes.size());
        for (size_t id = 0; id < config.nodes.size() && id < s.registry.size(); ++id) {
            const NodeDef   &d = s.registry.get((BlockId)id);
            net::NodeDigDesc w;
            w.hardness  = d.hardness;
            w.dig_level = d.dig_level;
            w.material  = d.material;
            config.node_digs.push_back(std::move(w));
        }

        // And what each block does to a body and to light (protocol 38).
        config.node_bodies.reserve(config.nodes.size());
        for (size_t id = 0; id < config.nodes.size() && id < s.registry.size(); ++id) {
            const NodeDef     &d = s.registry.get((BlockId)id);
            net::NodeBodyDesc b;
            b.solid   = d.solid;
            b.opaque  = d.opaque;
            b.opacity = d.opacity;
            b.liquid  = d.liquid;
            b.fill    = d.liquid_fill;
            b.climbable = d.climbable;
            b.light     = d.light_source;
            b.conveyor        = d.conveyor;
            b.conveyor_height = d.conveyor_height;
            b.conveyor_rise   = d.conveyor_rise;
            b.rail            = d.rail;
            config.node_bodies.push_back(b);
        }
        config.features    = server_features();
        config.mod_channels = server_mod_channels(s);
        config.max_players = max_players;   // DESIGN.md: 2-8 per world, decided
        config.require_identity = require_identity;
        config.allow_observers  = allow_observers;
        config.require_ticket   = require_ticket;
        config.room             = g_router.rooms[(size_t)g_room].name;
        config.has_sky          = s.has_sky;
        std::memcpy(config.sky, s.sky, 3);
        config.has_storm        = s.has_storm;
        std::memcpy(config.storm, s.storm, 3);

        // Whose word this world takes. Read before the sessions are built, so a
        // hex string somebody mistyped stops the server instead of quietly
        // making it trust nobody -- a trust list that silently lost an entry is
        // a world that refuses the players it meant to admit.
        for (const std::string &hex : trusted) {
            std::array<uint8_t, net::kSignPublicBytes> key{};
            if (!net::from_hex(hex, key.data(), key.size())) {
                std::printf("server: --trust wants the 64 hex characters of an issuer's signing "
                            "key, got '%s'\n", hex.c_str());
                return 1;
            }
            config.trusted_issuers.push_back(key);
        }

        // A world that requires a ticket and trusts nobody admits no portal at
        // all. That is a coherent thing to want and almost never the thing
        // somebody meant, so it is said out loud at startup rather than
        // discovered as every crossing being refused.
        if (require_ticket && config.trusted_issuers.empty())
            std::printf("server: --require-ticket with no --trust: no portal can come in, "
                        "because there is nobody whose ticket this world would take\n");

        // Where the dead come back, worked out BEFORE the table because the
        // table cannot be built without it.
        //
        // It used to be a setter called two hundred lines below, and a world
        // started without --portals never called it: kill_body then made a body
        // whole and returned, leaving it where it died. A player who walked off
        // the edge of a finite world fell for ever, killed and healed on every
        // claim and corrected every few seconds for falling too fast. Found by
        // the thirty minute run at y = -130822. Giving every world a spawn
        // closed it; taking the setter out of the path closes the seat it sat
        // in -- see net::SpawnPoint.
        //
        // In front of the first opening when there is one -- a player should
        // come back looking at the thing they walked into -- and in the middle
        // of the plaza when there is not. The opening is built HERE, above both
        // the table and the `if` that installs it, so the spawn and the portal
        // are the same object rather than two descriptions of one place.
        Portal first_opening;
        first_opening.center      = s.openings[0];
        first_opening.axis        = PortalAxis::XY;
        first_opening.normal_sign = 1;
        first_opening.half_w      = 0.5f;
        first_opening.half_h      = 1.0f;

        const Vec3 spawn_at = portals
                                  ? spawn_in_front(s.world, s.registry, first_opening, 4.0f)
                                  : spawn_in_plaza(s.world, s.registry, extent);

        RoomTransport       transport;
        net::ServerSessions sessions(config, net::SpawnPoint{spawn_at.x, spawn_at.y, spawn_at.z});

        // A name belongs to the identity that first joined under it here, for
        // as long as this world file exists. Without a file there is nothing to
        // remember it in, and the only rule left is that two players cannot
        // wear one name at once.
        sessions.set_name_check([&db](const std::string &name, const uint8_t *identity) {
            if (!db.is_open()) return true;

            uint8_t owner[net::kSignPublicBytes];
            if (db.name_owner(name, owner))
                return identity != nullptr &&
                       std::memcmp(owner, identity, net::kSignPublicBytes) == 0;

            // Free, and taken by whoever is at the door -- if they brought a
            // key. An anonymous player is let in under it and owns nothing.
            if (identity != nullptr) {
                std::string name_err;
                if (!db.claim_name(name, identity, (int64_t)std::time(nullptr), name_err))
                    std::printf("server: could not keep the name '%s': %s\n", name.c_str(),
                                name_err.c_str());
            }
            return true;
        });

        // Bans and the whitelist, the same for every room (src/server/admin.h).
        sessions.set_admit_check([](const std::string &name, const uint8_t *identity,
                                    net::PeerId peer, net::RefuseReason &reason,
                                    std::string &why) {
            admin::Who w;
            w.room         = g_router.rooms[(size_t)g_room].name;
            w.name         = name;
            w.has_identity = identity != nullptr;
            if (identity) std::memcpy(w.identity, identity, net::kSignPublicBytes);
            w.address = g_router.transport.address(peer);
            if (!admin::admits(w, reason, why)) {
                admin::refused(w, net::refuse_reason_text(reason) + std::string(", ") + why);
                return false;
            }
            // --max-per-address: names already in from there, this one aside.
            if (g_max_per_address > 0 && !w.address.empty()) {
                std::set<std::string> names;
                for (const admin::Online &o : admin_hooks().online())
                    if (o.who.address == w.address && o.who.name != name) names.insert(o.who.name);
                if ((int)names.size() >= g_max_per_address) {
                    reason = net::RefuseReason::ServerFull;
                    why    = "too many players from your address";
                    admin::refused(w, "too many from one address");
                    return false;
                }
            }
            return true;
        });

        // What a position is checked against: the fastest this world's own
        // numbers push a body (TODO.md OPEN-44). The same numbers the welcome
        // just told every client, from the same field, so the two cannot be
        // two opinions.
        sessions.limits() = limits_for(config.movement);

        // Read back out of the table rather than printed from `spawn_at`, so
        // the line cannot say where the dead come back beside a table that was
        // handed something else. The gates read this line.
        if (!quiet)
            std::printf("    the dead come back at %.1f %.1f %.1f\n", (double)sessions.spawn().x,
                        (double)sessions.spawn().y, (double)sessions.spawn().z);

        // And now the mods' entities have somewhere to live. The components
        // were declared while the mods loaded, which was before this world
        // existed; they are minted here.
        s.lua_ecs.set_world(&sessions.ecs());

        // What a creature drops is named by the mod and exists only now: the
        // item form of a block is minted after every mod has loaded.
        // And what tames, shears and is laid, the same way.
        cnp::ecs::find_mob_items(s.mob_kinds, s.items);

        // The pets somebody tamed, back where they were saved (keep_pets).
        {
            std::vector<std::string> lines;
            for (std::string l; !(l = s.mod_store.get("core", "pet:" + std::to_string(lines.size()))).empty();)
                lines.push_back(l);
            s.pets_kept = lines.size();
            const int back = cnp::ecs::pets_from_lines(sessions.ecs(), s.mob_kinds, lines,
                                                       0.0);
            if (back > 0 && !quiet) std::printf("        %d tamed pet(s) back\n", back);
        }

        // --mobs N: N creatures of the first kind a mod registered, round the
        // spawn, dropped from a node above it onto whatever is there. With no
        // kind registered, a box that hits for one, so a bare server still has
        // something to strike.
        if (mobs > 0) {
            if (s.mob_kinds.empty()) {
                cnp::ecs::MobKind k;
                k.name   = "core:creature";
                k.model  = s.mob_model;
                k.health = 6;
                k.speed  = 1.5f;
                k.damage = 1;
                s.mob_kinds.push_back(k);
            }
            const auto &at = sessions.spawn();
            for (int i = 0; i < mobs; ++i) {
                const float a = 6.2831853f * (float)i / (float)mobs;
                const float r = 4.0f + 2.0f * (float)(i % 3);
                cnp::ecs::spawn_mob(sessions.ecs(), s.mob_kinds, 0, at.x + std::cos(a) * r,
                                    at.y + 1.0f, at.z + std::sin(a) * r, 0.0);
            }
            std::printf("server: %d %s round the spawn at %.1f %.1f %.1f\n", mobs,
                        s.mob_kinds[0].name.c_str(), (double)at.x, (double)at.y, (double)at.z);
        }

        // The key this world signs tickets with. Kept apart from the X25519
        // identity above on purpose: one does key exchange and the other does
        // signatures, and DESIGN.md 4.4's ticket is a signature.
        uint8_t own_sign[net::kSignPublicBytes]{};   // for a mouth onto a room of this server
        {
            const std::string path =
                ticket_key_file
                    ? std::string(ticket_key_file)
                    : (world_file ? std::string(world_file) + ".sign"
                                  : std::string("server.sign.key"));
            net::SignKeyPair issuer;
            bool             created = false;
            std::string      key_err;
            if (!net::load_or_create_sign_key(path, issuer, created, key_err)) {
                std::printf("server: %s\n", key_err.c_str());
                return 1;
            }
            if (!key_err.empty()) std::printf("server: %s\n", key_err.c_str());
            sessions.set_ticket_key(issuer);
            std::memcpy(own_sign, issuer.public_key, sizeof(own_sign));
            std::printf("server: signs tickets as %s (%s)\n",
                        net::key_fingerprint(issuer.public_key).c_str(),
                        (created ? "created, kept in " + path : "read from " + path).c_str());
            std::printf("        other servers take them with --trust %s\n",
                        net::to_hex(issuer.public_key, net::kSignPublicBytes).c_str());
        }

        // One per --portal-to, in the same order: the first key is the audience
        // of tickets for the first opening's world, the second for the second's.
        if (portal_peer_key.size() > 2) {
            std::printf("server: --portal-peer-key given %zu times; a world has two openings\n",
                        portal_peer_key.size());
            return 1;
        }
        for (size_t k = 0; k < portal_peer_key.size(); ++k) {
            uint8_t peer[net::kSignPublicBytes];
            if (!net::from_hex(portal_peer_key[k], peer, sizeof(peer))) {
                std::printf("server: --portal-peer-key wants the 64 hex characters of the far "
                            "server's signing key, got '%s'\n", portal_peer_key[k]);
                return 1;
            }
            sessions.set_destination_key((int)k, peer);
        }

        // The world claimed positions are checked against. Without this call
        // there is no movement authority at all, which is why it is next to the
        // construction rather than somewhere further down.
        sessions.set_world(&s.world, &s.registry);

        // And the item table it validates a placement against. Without it a
        // placement is refused with a sentence that says so, rather than the
        // server quietly accepting whatever a client claims to be holding.
        sessions.set_items(&s.items);

        // And the other rooms may reach it from now on (take_carried).
        g_router.rooms[(size_t)g_room].sessions  = &sessions;
        g_router.rooms[(size_t)g_room].portal_to.assign(portal_to.begin(), portal_to.end());
        g_router.rooms[(size_t)g_room].trusted.assign(trusted.begin(), trusted.end());
        // Who sends players here through a portal (/portals, --report).
        sessions.set_crossing_seen([](const uint8_t *issuer, net::PeerId who) {
            admin::crossed_in(issuer, g_router.transport.address(who));
        });
        g_router.rooms[(size_t)g_room].items    = &s.items;
        g_router.rooms[(size_t)g_room].models   = &s.models;
        g_router.rooms[(size_t)g_room].registry = &s.registry;
        g_router.rooms[(size_t)g_room].db       = db.is_open() ? &db : nullptr;
        g_router.rooms[(size_t)g_room].containers = &s.containers;
        g_router.rooms[(size_t)g_room].mod_store  = &s.mod_store;
        g_router.rooms[(size_t)g_room].bags_touched = &s.bags_touched;

        // What a body here looks like, from whatever the mods said while they
        // loaded -- which was before this table existed, so it is applied now
        // rather than at the call.
        sessions.set_player_model(s.player_model);

        // Said out loud, and read back off the session table rather than off
        // the state it came from: a body's `model` is a number, the table that
        // gives it a meaning is sent once, and this line is the evidence that
        // the world told the thing that dresses the bodies. A world with no
        // models prints nothing and every body goes out as a box, which is the
        // world this engine had until protocol 22.
        if (!s.models.table().empty())
            std::printf("server: %zu model(s) declared, players wear %s, creatures wear %s\n",
                        s.models.size(),
                        sessions.player_model() == kNoModel
                            ? "nothing"
                            : s.models.get(sessions.player_model()).name.c_str(),
                        s.mob_model == kNoModel ? "nothing"
                                                : s.models.get(s.mob_model).name.c_str());

        sessions.set_panels(&s.panels);
        sessions.set_containers(&s.containers);
        sessions.set_recipes(&s.recipes);

        // The world's clock. One of these per world and none on any client:
        // core/world/daylight.h says why there is nothing to interpolate.
        sessions.set_day_length(day_length);
        sessions.set_time(start_time);
        s.sessions = &sessions;
        sessions.set_own_cell([&s](int x, int y, int z) {
            return !s.node_meta.get(Cell{x, y, z}, kPlacedKey).empty();
        });
        for (const net::ServerSessions::OwnedArea &a : s.owned_early)
            sessions.set_owned(a.x1, a.z1, a.x2, a.z2, a.tag);
        s.owned_early.clear();
        for (const std::array<int, 3> &c : s.owned_cells_early)
            sessions.set_owned_cell(c[0], c[1], (uint32_t)c[2]);
        s.owned_cells_early.clear();

        // A player's bag, read off the disk and written back to it. One pair of
        // lambdas because three moments need them -- joining, leaving, and a server
        // stopping with people still in it -- and three copies of "which sessions
        // have a bag worth keeping" is three chances to disagree about it.
        //
        // Keyed by identity and not by name: DESIGN.md 4.4 says the key is the
        // identity and the name is a label, so a bag keyed by name is a bag anybody
        // takes by typing yours. An anonymous session has no key at all and so has
        // no saved bag; it starts empty every time and that is stated rather than
        // papered over.
        //
        // An observer has no bag to keep either -- DESIGN.md 4.4, "scope OBSERVE =
        // niente inventario" -- and asking for one would write a row of nothing
        // under somebody's key.
        const auto bag_is_keepable = [](const net::Session *sess) {
            return sess != nullptr && sess->has_identity && sess->holds_bag;
        };

        // And where they were. Read at the join with the bag, but handed back
        // only after their first accepted claim -- see the loop below `++tick`.
        // Until then the place waits here, and nothing may save over it: the
        // player is standing wherever their client put itself, which is the
        // middle of the world, waiting to be moved.
        //
        // `spawn_owed` marks the places that are the world's spawn and not a
        // place of the player's own -- see the join.
        std::map<net::PeerId, StoredPlayer> places_owed;
        // How many have been handed the spawn: each next one stands a node
        // further along x, alternating sides, so players joining together do
        // not stand inside one another (seen with three clients, 2026-10-09).
        // ponytail: a counter, not who is standing there now; it cycles every 9.
        int spawns_handed = 0;
        std::set<net::PeerId>               spawn_owed;

        // `arrived`: through an opening, which places them itself.
        //
        // What they carry comes from the room of this server they are a body
        // in right now, when there is one -- they are walking out of it -- and
        // otherwise from whichever room's file kept it last: one bag and one
        // health for a player, whichever room they are in.
        const auto restore_player = [&](net::PeerId peer, bool arrived = false) {
            net::Session *sess = sessions.find_mutable(peer);
            if (!sess || !sess->has_identity || sess->scope != net::Scope::Enter ||
                sess->holds_bag)
                return;
            sess->holds_bag  = true;
            sess->body_taken = false;

            std::string          err;
            std::vector<uint8_t> carried;
            ecs::Health          health;
            CarriedBody          body;
            if (take_carried(sess->identity, carried, health, s.models, body)) {
                sess->bag() = Inventory{};
                if (!decode_inventory(s.items, carried, sess->bag(), err))
                    std::printf("server: could not take '%s' bag from the other room: %s\n",
                                sess->name.c_str(), err.c_str());
                sess->avatar.set<ecs::Health>(health);
                sess->vitals_owed = true;
                if (!quiet)
                    std::printf("        '%s' brings what they carry from another room\n",
                                sess->name.c_str());

                // Through an opening in a kart: still in it, at the speed it
                // came in at, with nothing between for the client to walk.
                if (arrived) sess->carried = body.carried;   // core.player_carried, on foot too
                if (arrived && body.drives &&
                    sessions.adopt_body_movement(peer, body.movement) &&
                    sessions.set_body_model(peer, (uint16_t)s.models.find(body.model))) {
                    if (!quiet)
                        std::printf("        and drives in, in '%s'\n", body.model.c_str());
                } else if (arrived && sess->movement.drives()) {
                    // On foot, and this session was last a kart here: the
                    // window it has been since kept what it was when it left.
                    sessions.set_body_movement(peer, s.movement, s.now);
                    sessions.set_body_model(peer, (uint16_t)s.player_model);
                }
            } else if (ask_friend_for_bag(*sess)) {
                // On a friend server: it comes from there (bag_take_here).
            } else if (WorldDb *from = newest_bag_file(sess->identity)) {
                if (from->load_player_bag(sess->identity, s.items, sess->bag(), err)) {
                    if (!quiet)
                        std::printf("        '%s' picks up what they left\n", sess->name.c_str());
                } else if (!err.empty()) {
                    std::printf("server: could not read '%s' bag: %s\n", sess->name.c_str(),
                                err.c_str());
                }
            }
            bring_carried_containers(sess->bag(), s.items, s.containers);
            s.bags_touched.insert((uint32_t)peer);

            if (!db.is_open() || arrived) return;

            // Somebody who came through a portal arrives at the portal. The
            // place they left from last time is for walking back in.
            if (sess->admitted_by_ticket) return;

            StoredPlayer was;
            std::string  place_err;
            if (db.load_player(sess->identity, was, place_err))
                places_owed[peer] = was;
            else if (!place_err.empty())
                std::printf("server: could not read where '%s' was: %s\n", sess->name.c_str(),
                            place_err.c_str());
        };

        // `leaving` is false only for the save during play, which must not
        // drop a place still owed to a player who is staying.
        const auto keep_player = [&](net::PeerId peer, bool leaving = true) {
            const bool owed =
                leaving ? places_owed.erase(peer) > 0 : places_owed.count(peer) > 0;
            if (leaving) spawn_owed.erase(peer);

            if (!db.is_open()) return;
            const net::Session *sess = sessions.find(peer);
            if (!bag_is_keepable(sess) || g_bag_waiting.count(identity_key(sess->identity))) return;

            const int64_t when = (int64_t)std::time(nullptr);
            std::string   err;
            if (!db.save_player_bag(sess->identity, sess->name, s.items, sess->bag(), when, err))
                std::printf("server: could not keep '%s' bag: %s\n", sess->name.c_str(), err.c_str());
            else
                g_bag_kept_by[identity_key(sess->identity)] = g_room;

            // A body that stepped out through an opening keeps its bag, and
            // has no place here any more.
            if (sess->scope != net::Scope::Enter) return;

            // Not before the first claim, when they are nowhere, and not while
            // their old place is owed, when they are at the spawn only because
            // the move back has not happened yet.
            if (!sess->has_moved || owed) return;

            StoredPlayer here;
            std::memcpy(here.identity, sess->identity, sizeof(here.identity));
            here.name      = sess->name;
            here.x         = sess->authority().x;
            here.y         = sess->authority().y;
            here.z         = sess->authority().z;
            here.last_seen = when;
            sess->interest.look(here.look_x, here.look_y, here.look_z);

            std::string place_err;
            if (!db.save_player(here, place_err))
                std::printf("server: could not keep where '%s' was: %s\n", sess->name.c_str(),
                            place_err.c_str());
        };


        // What this world's blocks look like, built once and read by every
        // session. The placeholder for a block nobody drew art for is baked
        // HERE and not on the client, which is the change that makes a portal
        // into somebody else's game show their blocks instead of magenta: the
        // wire carries names, and a name is not enough to bake anything from.
        MediaSet media;
        {
            const MediaBuildStats st = build_media_set(s.registry, textures_dir, media, fallback_textures);

            // And the item squares, into the same set. A tool is not a block
            // and has no colour to bake a placeholder from, so this serves what
            // somebody drew and nothing for the rest -- see add_items_to_media.
            const ItemMediaStats ist = add_items_to_media(s.items, textures_dir, media, fallback_textures);

            // And the models, into the same set. A model is not a file -- the
            // .gltf names its buffer and its images beside itself -- so this
            // puts in the .gltf and everything it names, under names the other
            // side derives from the model's NAME and never from a path.
            std::string      model_trouble;
            const ModelMediaStats mst = add_models_to_media(s.models, media, model_trouble);
            if (!model_trouble.empty()) std::fputs(model_trouble.c_str(), stdout);

            // And the mods' sounds, so a creature's voice is heard on a machine
            // that does not have the mod (SoundBank::named looks here first).
            int sounds = 0;
            for (const Mod &mod : s.mods.mods())
                sounds += add_mod_sounds_to_media(mod.manifest.name, mod.manifest.dir, media);
            if (sounds > 0) std::printf("        sounds: %d file(s) served\n", sounds);

            // And this world's code for its players' machines (TODO.md OPEN-49):
            // every mod that says it runs on a client, packed whole and served as
            // a media file like any picture. Whether it RUNS is the player's
            // machine's to decide.
            {
                int    offered = 0;
                size_t bytes   = 0;
                for (const Mod &mod : s.mods.mods()) {
                    if (mod.manifest.side == ModSide::Server) continue;
                    std::vector<uint8_t> pkg;
                    std::string          why;
                    if (!world_code_pack(mod.manifest.dir, pkg, why)) {
                        std::printf("        world code '%s': not offered -- %s\n",
                                    mod.manifest.name.c_str(), why.c_str());
                        continue;
                    }
                    const size_t size = pkg.size();
                    if (!media.add(world_code_media_name(mod.manifest.name), std::move(pkg))) {
                        std::printf("        world code '%s': not offered -- %zu bytes, more than "
                                    "one media file may be\n",
                                    mod.manifest.name.c_str(), size);
                        continue;
                    }
                    ++offered;
                    bytes += size;
                }
                if (offered > 0)
                    std::printf("        world code: %d mod(s) offered to clients, %.1f KB\n",
                                offered, (double)bytes / 1024.0);
            }

            sessions.set_media(&media);
            std::printf("        media: %zu file(s), %zu distinct, %.1f KB (%d drawn, %d "
                        "generated); the offer a join carries is %zu bytes\n",
                        media.names(), media.blobs(), (double)media.total_bytes() / 1024.0,
                        st.from_file, st.generated,
                        sessions.media_offer_message().encode().size());

            if (ist.files > 0)
                std::printf("        items: %d square(s) drawn of %zu\n", ist.files,
                            s.items.size());

            if (s.models.size() > 0)
                std::printf("        models: %d of %zu served whole, %d file(s), %d skipped, "
                            "%.1f KB\n",
                            mst.models, s.models.size(), mst.files, mst.skipped,
                            (double)mst.bytes / 1024.0);

            // The pack as a pack, beside the same pictures under the engine's
            // names. Said out loud because it is the difference between a
            // player being able to keep this world's art and not.
            if (st.pack_served)
                std::printf("        pack: served as a pack too, %d file(s), %d named and not "
                            "here\n",
                            st.pack_files, st.pack_missing);
        }

        // A pair of openings the SERVER owns, if asked for. DESIGN.md 3.4: the
        // crossing is decided here and drawn by the client.
        //
        // Free-standing, in the flat part of the plaza, and nothing is built
        // around them: this server has no renderer and the client keeps its own
        // portals off while connected, so what a player sees today is nothing
        // at all until they walk into one. That is the honest state of it --
        // the authority is here, the picture is the next piece.
        if (portals) {
            Portal b;
            b.center      = s.openings[1];
            b.axis        = PortalAxis::YZ;
            b.normal_sign = 1;
            b.half_w      = 0.5f;
            b.half_h      = 1.0f;

            sessions.set_portals(first_opening, b);

            // And where they lead, if they lead away. The pair is still
            // installed: with one destination the second opening is where a
            // local crossing would go, and nobody crosses locally at all -- the
            // mouth becomes something to look through. With two, each opening
            // is a mouth onto a world of its own.
            if (portal_to.size() > (size_t)net::kMaxMouths) {
                std::printf("server: --portal-to given %zu times; a world has %d mouths at "
                            "most\n", portal_to.size(), net::kMaxMouths);
                return 1;
            }
            // Past the pair, each mouth's own opening, where core.set_portal
            // put it (a mod moves it with core.portal_place).
            for (size_t k = 2; k < portal_to.size(); ++k) {
                Portal m = b;
                m.center = s.openings[k];
                sessions.move_opening((int)k, m);
            }
            for (size_t k = 0; k < portal_to.size(); ++k) {
                // host:port, and optionally /N for which of the far world's
                // openings this mouth comes out of. Without the N it is the
                // first one, which is what every mouth did before there was a
                // way to say otherwise. Past 1 it is one a mod of that world
                // opens (core.portal_open): a way out there and not a way in,
                // which is how a track leaves a world by one frame and comes
                // back by another.
                const auto far_opening = [](const std::string &t, uint16_t &id) {
                    char         *end = nullptr;
                    const long    n   = std::strtol(t.c_str(), &end, 10);
                    if (t.empty() || *end != '\0' || n < 0 || n >= net::kMaxOpenings) return false;
                    id = (uint16_t)n;
                    return true;
                };
                std::string spec   = portal_to[k];
                uint16_t    far_id = 0;

                // @ROOM[/N]: a room of this same server. Its tickets are signed
                // with the key every room here shares (main), so the audience
                // is this room's own key.
                if (!spec.empty() && spec[0] == '@') {
                    const size_t      at_slash = spec.rfind('/');
                    const std::string room     = spec.substr(1, at_slash == std::string::npos
                                                                    ? std::string::npos
                                                                    : at_slash - 1);
                    const std::string tail =
                        at_slash == std::string::npos ? "0" : spec.substr(at_slash + 1);
                    if (g_router.find(room) < 0 || room.empty() || !far_opening(tail, far_id)) {
                        std::printf("server: --portal-to wants @ROOM[/N] with a room this server "
                                    "holds, got '%s'\n", portal_to[k]);
                        return 1;
                    }
                    sessions.set_destination((int)k, "", 0, far_id, room);
                    sessions.set_destination_key((int)k, own_sign);
                    std::printf("        and opening %zu leads to room '%s' opening %u -- a "
                                "mouth, not a pair\n", k, room.c_str(), (unsigned)far_id);
                    continue;
                }

                const size_t slash = spec.rfind('/');
                if (slash != std::string::npos) {
                    const std::string tail = spec.substr(slash + 1);
                    if (!far_opening(tail, far_id)) {
                        std::printf("server: --portal-to wants /0 to /%d for the far opening, "
                                    "got '%s'\n", net::kMaxOpenings - 1, tail.c_str());
                        return 1;
                    }
                    spec   = spec.substr(0, slash);
                }

                const size_t colon = spec.rfind(':');
                if (colon == std::string::npos) {
                    std::printf("server: --portal-to wants host:port[/N], got '%s'\n",
                                portal_to[k]);
                    return 1;
                }
                const std::string host = spec.substr(0, colon);
                const uint16_t    p    = (uint16_t)std::strtoul(spec.c_str() + colon + 1,
                                                                nullptr, 10);
                if (host.empty() || p == 0) {
                    std::printf("server: --portal-to wants host:port[/N], got '%s'\n",
                                portal_to[k]);
                    return 1;
                }
                sessions.set_destination((int)k, host, p, far_id);
                std::printf("        and opening %zu leads to %s:%u opening %u -- a mouth, "
                            "not a pair\n", k, host.c_str(), (unsigned)p, (unsigned)far_id);
            }
            std::printf("portals: %.1f %.1f %.1f facing +Z and %.1f %.1f %.1f facing +X, "
                        "crossings decided here\n", (double)first_opening.center.x,
                        (double)first_opening.center.y, (double)first_opening.center.z, (double)b.center.x, (double)b.center.y,
                        (double)b.center.z);
        }

        long chunks_sent      = 0;
        long chunk_bytes_sent = 0;

        std::string err;

        // The identity, before the socket. A server that cannot produce a key
        // does not open a port: starting anyway would mean starting in the
        // clear, and the one thing worse than no encryption is encryption
        // people believe they have.
        // An identity is proven against the channel binding, and there is no
        // binding without a key exchange. Refused at startup rather than
        // discovered by the first player who is let in without proving
        // anything: a requirement that silently does not apply is worse than no
        // requirement at all.
        if (require_identity && !encrypt) {
            std::printf("server: --require-identity needs encryption; it is proven against the\n"
                        "        session key exchange, and without one there is nothing to "
                        "sign.\n");
            return 1;
        }

        transport.set_encryption(encrypt);
        if (encrypt) {
            const std::string path =
                key_file ? std::string(key_file)
                         : (world_file ? std::string(world_file) + ".key"
                                       : std::string("server.key"));
            net::KeyPair identity;
            bool         created = false;
            std::string  key_err;
            if (!net::load_or_create_key(path, identity, created, key_err)) {
                std::printf("server: %s\n", key_err.c_str());
                return 1;
            }
            if (!key_err.empty()) std::printf("server: %s\n", key_err.c_str());
            transport.set_identity(identity);

            const std::string origin =
                (created ? "created, kept in " : "read from ") + path;
            std::printf("server: identity %s (%s)\n",
                        net::key_fingerprint(identity.public_key).c_str(), origin.c_str());
            std::printf("        clients pin it with --server-key %s\n",
                        net::to_hex(identity.public_key, net::kKeyBytes).c_str());
        } else {
            std::printf("server: ENCRYPTION OFF -- every packet is readable by anybody on the "
                        "path\n");
        }

        // Sockets for everybody who may be here at once, which is not the
        // player cap: an observer is a peer too, and it is counted against a
        // different number on purpose. With the cap as the peer capacity, a
        // world full of players could not be looked into at all -- the ninth
        // peer never gets a socket, so it never gets as far as the handshake
        // that would have admitted it as a window.
        const int peer_capacity =
            config.max_players + (allow_observers ? config.max_observers : 0);
        if (!transport.listen((uint16_t)port, peer_capacity, err, bind)) {
            std::printf("server: %s\n", err.c_str());
            return 1;
        }
        std::printf("server: listening on UDP %d, up to %d players%s, %s\n", port,
                    config.max_players,
                    require_identity ? ", identity required" : ", anonymous joins allowed",
                    allow_observers
                        ? ("up to " + std::to_string(config.max_observers) + " portals may look in")
                              .c_str()
                        : (config.trusted_issuers.empty()
                               ? "no portal may look in"
                               : ("only portals from " +
                                  std::to_string(config.trusted_issuers.size()) +
                                  " trusted server(s) may look in").c_str()));
        std::printf("        engine %s, protocol %u, block table %016llx (%zu nodes, sent to "
                    "clients that adopt it)\n",
                    config.engine_version.c_str(), (unsigned)config.protocol,
                    (unsigned long long)config.content_hash, s.registry.size());
        std::printf("        mapgen hash %016llx (what a joining client must reproduce)\n",
                    (unsigned long long)config.mapgen_hash);

        // The tick is a wall-clock deadline rather than a sleep, so time spent
        // servicing the socket comes out of the tick rather than adding to it:
        // a server that is busy falls behind on purpose instead of drifting.
        const auto  t_start   = std::chrono::steady_clock::now();
        g_longest_hold        = 0.0;   // making the world first held it, by design
        g_held_since          = t_start;
        int         tick      = 0;
        const bool  forever   = ticks <= 0;


        while ((forever || tick < ticks) && !g_stop) {
            const double next_tick_at = (double)(tick + 1) * kTickSeconds;

            for (int drained = 0;;) {
                const double now       = seconds_since(t_start);
                const double remaining = next_tick_at - now;

                // Behind, the socket is still read -- what is waiting and no
                // more, without waiting for anything else. A tick that ran long
                // making columns (TODO.md OPEN-35) used to be followed at once by
                // the next, and the next, and the claim that would have told the
                // server where the player is waited behind the very work that
                // was being done for them: a joining client was sent nothing.
                const bool behind = remaining <= 0.0;
                if (behind && ++drained > 256) break;

                const int timeout_ms = behind ? 0 : std::max(1, (int)(remaining * 1000.0));
                net::NetEvent ev;
                if (!transport.poll(ev, timeout_ms)) {
                    if (behind) break;
                    continue;
                }

                switch (ev.type) {
                case net::NetEvent::Type::Connected: {
                    sessions.on_connect(ev.peer, seconds_since(t_start));

                    // What this peer's identity proof will have to be signed
                    // over. Handed down here because the transport owns it and
                    // the session layer must not: a session that could compute
                    // its own binding would be a session that could accept a
                    // proof it made up.
                    uint8_t binding[net::kKeyBytes];
                    sessions.set_binding(ev.peer, transport.channel_binding(ev.peer, binding)
                                                      ? binding
                                                      : nullptr);
                    break;
                }

                case net::NetEvent::Type::Disconnected:
                    // Before on_disconnect, which destroys the session and the
                    // body its bag is a component of.
                    keep_player(ev.peer);
                    if (const net::Session *sess = sessions.find(ev.peer)) {
                        if (!sess->name.empty()) std::printf("        '%s' left\n",
                                                             sess->name.c_str());
                    }
                    gone(s, ev.peer, "left");
                    sessions.on_disconnect(ev.peer);
                    break;

                case net::NetEvent::Type::Received: {
                    // A mod's own channel, and the engine does not parse what
                    // is in it. The CHANNEL says which mod: that is what the
                    // handshake handed out, and a name in the payload would be
                    // a second answer to the same question.
                    if (ev.channel >= net::kModChannelBase) {
                        const size_t which = (size_t)ev.channel - net::kModChannelBase;
                        const net::Session *from = sessions.find(ev.peer);
                        if (which >= s.mod_channels.size() || !from ||
                            from->state != net::SessionState::Ready)
                            break;

                        ByteReader r(ev.data);
                        net::ModData    said;
                        if (!net::ModData::decode(r, said)) break;

                        // Only the mod whose channel it is hears it. A handler
                        // belonging to another mod would be reading somebody
                        // else's post.
                        script_field fields[3] = {
                            {"player", SCRIPT_V_NUMBER, (double)ev.peer, nullptr, 0},
                            {"mod", SCRIPT_V_STRING, 0.0, s.mod_channels[which].c_str(), 0},
                            {"text", SCRIPT_V_STRING, 0.0, said.text.c_str(), 0},
                        };
                        bool        ignored = false;
                        std::string emit_err;
                        hold_if_muted(s, ev.peer, said.text);
                        if (!s.mods.events().emit("core:net_message", fields, 3, &ignored,
                                                  emit_err, &s.mod_channels[which]))
                            std::printf("        emit core:net_message: %s\n", emit_err.c_str());
                        s.muted_caller = 0;
                        s.muted_why.clear();
                        break;
                    }

                    const net::SessionAction a =
                        sessions.on_message(ev.peer, ev.channel, ev.data.data(), ev.data.size(),
                                            seconds_since(t_start));
                    if (!a.log.empty()) std::printf("        %s\n", a.log.c_str());

                    if (a.has_hit) {
                        script_field fields[4] = {
                            {"player", SCRIPT_V_NUMBER, (double)ev.peer, nullptr, 0},
                            {"target", SCRIPT_V_NUMBER, (double)a.hit_target, nullptr, 0},
                            {"damage", SCRIPT_V_NUMBER, (double)a.hit_taken, nullptr, 0},
                            {"killed", SCRIPT_V_BOOL, 0.0, nullptr, a.hit_killed ? 1 : 0},
                        };
                        std::string hit_err;
                        if (!s.mods.events().emit("core:hit", fields, 4, nullptr, hit_err))
                            std::fprintf(stderr, "emit core:hit: %s\n", hit_err.c_str());
                    }
                    if (a.has_used_mob) {
                        const std::string held =
                            a.used_held != kNoItem ? s.items.get(a.used_held).name : std::string();
                        script_field fields[3] = {
                            {"player", SCRIPT_V_NUMBER, (double)ev.peer, nullptr, 0},
                            {"target", SCRIPT_V_NUMBER, (double)a.used_mob, nullptr, 0},
                            {"held", SCRIPT_V_STRING, 0.0, held.c_str(), 0},
                        };
                        std::string use_err;
                        if (!s.mods.events().emit("core:used_mob", fields, 3, nullptr, use_err))
                            std::fprintf(stderr, "emit core:used_mob: %s\n", use_err.c_str());
                    }

                    // Before the welcome goes out, and well before the first
                    // InventoryState: a bag read a tick later would reach the
                    // player as an empty bag followed by the real one.
                    if (a.has_joined) restore_player(ev.peer);

                    // Through an opening from somewhere else: what they carry
                    // comes with them, and the bar and the bag say so.
                    if (a.has_entered) {
                        restore_player(ev.peer, true);
                        if (db.is_open()) keep_player(ev.peer, false);
                    }

                    // And a player with no place of their own is owed the
                    // world's spawn, handed over the same way. A client that
                    // joins puts itself in the middle of the world: the plaza
                    // in the sandbox, and in games/track the walled infield --
                    // measured, a player who joined the track stood at 64 64
                    // with the spawn at 30.5 17.5 on the road. Only a join that
                    // asks to PLAY: one coming through a portal joins as a
                    // window, and its crossing places it.
                    // A visit begins (src/server/admin.h), and the message of
                    // the day for whoever comes in through the door.
                    if (const net::Session *in = sessions.find(ev.peer);
                        in && in->scope == net::Scope::Enter && (a.has_joined || a.has_entered)) {
                        const admin::Who who = admin_who(*in);
                        admin::joined(who, g_router.rooms[(size_t)g_room].name);
                        if (const std::string m = admin::motd(); a.has_joined && !m.empty())
                            say_to(s, ev.peer, m);
                        admin::check_alts(who, admin_hooks());
                        if (!update::newer().empty() && admin::may(who, "restart")) say_to(s, ev.peer, update_line());
                        // Rules not accepted yet: on the screen before anything else.
                        if (!admin::accepted(who)) open_page(s, ev.peer, "rules");
                    }

                    if (a.has_joined) {
                        const net::Session *joined = sessions.find(ev.peer);
                        if (joined && joined->scope == net::Scope::Enter &&
                            places_owed.count(ev.peer) == 0) {
                            StoredPlayer at;
                            at.x = sessions.spawn().x;
                            at.y = sessions.spawn().y;
                            at.z = sessions.spawn().z;
                            const int   k   = spawns_handed++ % 9;
                            const float off = (float)((k + 1) / 2) * ((k % 2) ? 1.0f : -1.0f);
                            if (!inside_solid(s.world, s.registry, at.x + off, at.y, at.z) &&
                                inside_solid(s.world, s.registry, at.x + off, at.y - 0.5f, at.z))
                                at.x += off;
                            places_owed[ev.peer] = at;
                            spawn_owed.insert(ev.peer);
                        }
                    }

                    if (!a.reply.empty())
                        transport.send(ev.peer, (net::Channel)a.reply_channel, a.reply);
                    // What follows the reply, on the SAME channel and in
                    // order. The channel matters: ENet orders reliable messages
                    // within a channel and not across them, so an offer sent on
                    // the event channel can overtake the welcome it belongs
                    // behind -- which is a client refusing a media offer that
                    // arrived before it had joined, once in every run.
                    for (const auto &m : a.more)
                        transport.send(ev.peer, (net::Channel)a.reply_channel, m, true);

                    // A block right-clicked, whatever was in the hand. Emitted
                    // BEFORE the placement and cancellable: a mod that cancels
                    // has claimed the click, and the placement behind it does
                    // not happen. That is what lets a chest open with a block
                    // in hand -- and what stops the same click also building on
                    // top of the chest.
                    bool claimed = false;
                    if (a.has_use) {
                        // By NAME, like everything else a mod is handed: an id
                        // is assigned in registration order, so a mod that
                        // compared one against a number it wrote down would be
                        // comparing against a different item the day somebody
                        // adds a mod.
                        const std::string &holding =
                            s.items.get(a.use.use_holding).name;

                        script_field fields[6] = {
                            {"x", SCRIPT_V_NUMBER, (double)a.use.use_x, nullptr, 0},
                            {"y", SCRIPT_V_NUMBER, (double)a.use.use_y, nullptr, 0},
                            {"z", SCRIPT_V_NUMBER, (double)a.use.use_z, nullptr, 0},
                            {"id", SCRIPT_V_NUMBER, (double)a.use.use_had, nullptr, 0},
                            {"player", SCRIPT_V_NUMBER, (double)ev.peer, nullptr, 0},
                            {"held", SCRIPT_V_STRING, 0.0, holding.c_str(), 0},
                        };
                        std::string use_err;
                        if (!s.mods.events().emit("core:used", fields, 6, &claimed, use_err)) {
                            std::fprintf(stderr, "emit core:used: %s\n", use_err.c_str());
                            claimed = false;
                        }
                    }

                    // The refusal, only if nobody claimed the click. A player
                    // told "there is already something there" a moment after
                    // successfully opening a chest is being answered about a
                    // question they did not ask.
                    if (!claimed && !a.edit_refusal.empty()) {
                        transport.send(ev.peer, net::Channel::Block,
                                       net::ServerSessions::block_change_message(
                                           a.refused_x, a.refused_y, a.refused_z,
                                           s.world.get_block(a.refused_x, a.refused_y,
                                                             a.refused_z),
                                           s.world.get_facing(a.refused_x, a.refused_y,
                                                              a.refused_z),
                                           a.edit_refusal),
                                       true);
                    }

                    // An edit the session layer allowed. Applied here because
                    // this is where the world and the mods are -- see
                    // net::EditPlan -- and answered either way, because every
                    // request gets a reply (M6_FINDINGS 25).
                    if (a.has_edit && !claimed) {
                        net::EditPlan plan = a.edit;
                        std::string   why;

                        // The light for the whole edit at once, the way a tick
                        // already does it: an edit is one cell, but what a mod
                        // does about it is not -- mods/lakes pours water into a
                        // hole dug beside a lake, up to a couple of hundred
                        // cells, and a relight each would be OPEN-29's 1.9 ms a
                        // cell in the middle of answering a player.
                        bool wrote = false;
                        const net::Session *editor = sessions.find(ev.peer);
                        const int old_block  = (int)s.world.get_block(plan.x, plan.y, plan.z);
                        const int old_facing = (int)s.world.get_facing(plan.x, plan.y, plan.z);
                        if (editor && !admin::accepted(admin_who(*editor))) {
                            why = "accept the rules first: /rules";
                        } else {
                            LightRun lights(s);
                            wrote = apply_edit(s, ev.peer, plan, why);
                        }
                        // Who did it, for /inspect and /rollback.
                        if (wrote)
                            if (const net::Session *by = sessions.find(ev.peer))
                                admin::edited(admin_who(*by), g_router.rooms[(size_t)g_room].name, plan.x,
                                              plan.y, plan.z, old_block, old_facing,
                                              (int)s.world.get_block(plan.x, plan.y, plan.z));

                        if (!wrote) {
                            transport.send(ev.peer, net::Channel::Block,
                                           net::ServerSessions::block_change_message(
                                               plan.x, plan.y, plan.z,
                                               s.world.get_block(plan.x, plan.y, plan.z),
                                               s.world.get_facing(plan.x, plan.y, plan.z), why),
                                           true);
                        } else {
                            sessions.edit_done(ev.peer, plan, now);

                            // Everybody Ready who has the chunk it is in,
                            // observers included: a window onto this world is
                            // streaming it, and a cell that changed behind
                            // somebody's portal has changed. See already_has
                            // for why the filter is there, and why the comment
                            // that used to be here -- "the filter would cost
                            // more than it saves" -- stopped being true.
                            //
                            // On Channel::Block, which is what keeps a stream
                            // of these from holding up the heartbeat. Read the
                            // comment on that channel before moving either of
                            // the two senders off it: they are on ONE channel
                            // so that two changes to one cell keep their
                            // order.
                            const std::vector<uint8_t> msg =
                                net::ServerSessions::block_change_message(
                                    plan.x, plan.y, plan.z, plan.want,
                                    s.world.get_facing(plan.x, plan.y, plan.z));
                            for (net::PeerId other : sessions.ready_peers())
                                if (already_has(sessions, other, plan.x, plan.y, plan.z))
                                    transport.send(other, net::Channel::Block, msg, true);

                            transport.send(ev.peer, net::Channel::Event,
                                           sessions.inventory_message(ev.peer), true);
                        }
                    }

                    // A button press the session layer checked against the
                    // panel that is actually up. Emitted here for the same
                    // reason an edit is applied here: this is where the mod
                    // host lives.
                    if (a.has_ui_click && a.ui_click.panel == page_panel(ev.peer)) {
                        page_click(s, ev.peer, a.ui_click.id, a.ui_click.text);
                    } else if (a.has_ui_click) {
                        // ESC: off this player's screen first, whatever the mod says.
                        if (a.ui_click.id == ui::kPanelEscape) {
                            std::string perr;
                            if (const ui::Panel *p = s.panels.find(a.ui_click.panel))
                                s.panels.show(p->name, p->owner, false, (ui::PlayerId)ev.peer, perr);
                        }
                        script_field fields[4] = {
                            {"panel",  SCRIPT_V_STRING, 0.0, a.ui_click.panel.c_str(), 0},
                            {"id",     SCRIPT_V_STRING, 0.0, a.ui_click.id.c_str(),    0},
                            {"player", SCRIPT_V_NUMBER, (double)ev.peer, nullptr,      0},
                            {"text",   SCRIPT_V_STRING, 0.0, a.ui_click.text.c_str(),  0},
                        };
                        bool        ignored = false;
                        std::string click_err;
                        hold_if_muted(s, ev.peer, a.ui_click.text);
                        if (!s.mods.events().emit("core:ui_click", fields, 4, &ignored, click_err))
                            std::fprintf(stderr, "emit core:ui_click: %s\n", click_err.c_str());
                        s.muted_caller = 0;
                        s.muted_why.clear();
                    }

                    // A line of chat (run_chat_line).
                    if (a.has_chat) run_chat_line(s, ev.peer, a.chat);

                    if (a.close) {
                        // Flushed first, or the refusal races the disconnect
                        // and the player is cut off without being told why.
                        transport.flush();
                        transport.disconnect(ev.peer);
                    }
                    break;
                }

                case net::NetEvent::Type::None:
                    break;
                }
            }

            for (const net::PeerId p : sessions.expired(seconds_since(t_start))) {
                std::printf("        dropped a peer that connected and never said hello\n");
                keep_player(p);
                gone(s, p, "never said hello");
                sessions.on_disconnect(p);
                transport.disconnect(p);
            }

            // The seat of a player nothing has been heard from. This is the
            // server's own liveness rule and it is new: until now the only
            // thing deciding a peer was gone was ENet's peer timeout, which
            // this code now holds open on purpose -- see Transport::hold_peer.
            // Holding it open without freeing seats some other way would trade
            // a session dropped too early for a seat held too long.
            // What this end still owes, told before the rule below reads it.
            // A session cannot be counted silent across a stretch in which its
            // own answer was still on the wire: it had nothing to reply with.
            const double liveness_now = seconds_since(t_start);
            for (const net::PeerId peer : sessions.ready_peers())
                if (transport.owed_bytes(peer) > 0) sessions.note_owed(peer, liveness_now);

            for (const net::PeerId p : sessions.silent(liveness_now)) {
                const net::Session *idle_sess = sessions.find(p);
                // Both numbers from the two functions the decision itself used.
                // This line used to print the constant kSessionSilence, which
                // stopped being this session's patience the day join_patience
                // arrived -- a log that reports a rule it is not reading.
                std::printf("        dropped '%s': nothing heard for %.0f s of the %.0f s it had\n",
                            idle_sess ? idle_sess->name.c_str() : "?",
                            idle_sess ? net::quiet_for(*idle_sess, liveness_now) : 0.0,
                            idle_sess ? net::join_patience(*idle_sess) : 0.0);
                keep_player(p);
                gone(s, p, "went silent");
                sessions.on_disconnect(p);
                transport.disconnect(p);
            }

            // And ENet's patience. Wider than the rule above, and the two
            // divide the ways a peer can stop answering rather than racing for
            // the same one: with nothing outstanding ENet never times out at
            // all and the rule above decides, and while something IS
            // outstanding the rule above stands aside and this is what
            // eventually lets go. Per peer because the round trip is per peer.
            for (const net::PeerId peer : sessions.ready_peers()) {
                const double rtt = transport.round_trip(peer);
                const double own = net::kSessionSilence > rtt * net::kHeartbeatSilenceRtts
                                       ? net::kSessionSilence
                                       : rtt * net::kHeartbeatSilenceRtts;
                transport.hold_peer(peer, 2.0 * own);
            }

            // The clock the mods drop things against, before they run: a stack
            // a handler drops this tick has to be armed and to expire from
            // where this tick is, not from where the last one was.
            s.now = seconds_since(t_start);

            // How long the mods and the columns take, and how far behind the
            // clock this tick starts: said every ten seconds when a tick ran
            // over its 50 ms, so a world that cannot keep up says why.
            const double t_mods = seconds_since(t_start);
            g_tick_stats.behind = std::max(g_tick_stats.behind, t_mods - (double)(tick + 1) * kTickSeconds);
            tick_once();
            g_tick_stats.add_mods(seconds_since(t_start) - t_mods);

            // A chest somebody walked away from. In whatever state it is in --
            // what is inside stays inside, and anybody still standing at it
            // goes on looking at it. Reach and not a distance of its own, so
            // what this says is "a chest you can no longer reach is a chest you
            // are no longer in".
            for (const net::PeerId peer : sessions.close_far_containers()) {
                const net::Session *walked = sessions.find(peer);
                if (!quiet)
                    std::printf("        '%s' walked away from a chest\n",
                                walked ? walked->name.c_str() : "?");
            }

            // Which cells have somebody looking into them, and who has to be
            // told that it changed.
            //
            // Diffed once a tick against what it was, rather than sent from
            // each of the places a container opens or closes: those are the
            // click, the close message, walking out of reach, the container
            // being erased under somebody, a session being dropped and a
            // session being redirected, and the one that forgot would leave a
            // chest standing open with nobody inside it. One place that asks
            // cannot forget.
            {
                // To whoever has the chunk, which is the same rule a block
                // change follows: a lid nobody can see is a lid nobody needs
                // told about.
                for (const net::CellOpen &msg : sessions.open_cell_changes()) {
                    const std::vector<uint8_t> bytes = msg.encode();
                    for (const net::PeerId peer : sessions.ready_peers())
                        if (already_has(sessions, peer, msg.x, msg.y, msg.z))
                            transport.send(peer, net::Channel::Block, bytes, true);
                }
            }

            // And from a table. Its own call and not a line inside the one
            // above, because closing a table EMPTIES a grid -- a thing that
            // moves stacks has no business hiding inside a loop that closes
            // screens.
            for (const net::PeerId peer : sessions.close_far_crafts(seconds_since(t_start))) {
                const net::Session *walked = sessions.find(peer);
                if (!quiet)
                    std::printf("        '%s' walked away from a crafting table\n",
                                walked ? walked->name.c_str() : "?");
            }

            // The entities, moved by their own systems rather than by this
            // loop. One call, and everything anybody registers runs inside it
            // -- which is the shape M8's Lua-declared systems need to slot
            // into, so it is worth having the C++ side already be that shape.
            sessions.ecs().progress((float)kTickSeconds);

            // The creatures: who they can go for, then a tick of them and of
            // their arrows, and every two seconds the dark fills a little and
            // the far ones go. A player being put back after dying is not
            // somebody to go for.
            if (!s.mob_kinds.empty()) {
                const double mob_now = seconds_since(t_start);
                std::vector<cnp::ecs::Target> targets;
                for (const net::PeerId peer : sessions.ready_peers()) {
                    const net::Session *who = sessions.find(peer);
                    if (who == nullptr || who->scope != net::Scope::Enter || !who->avatar.is_alive())
                        continue;
                    const MovementState &at = who->authority();
                    if (!at.valid) continue;
                    targets.push_back({who->avatar.id(), (uint32_t)peer, at.x, at.y, at.z, who->name});
                }
                // The creatures' sounds go out as a mod's would (core.play_sound):
                // "mobs:zombie" hurt is "mobs:zombie_hurt", for each client to find.
                if (!sessions.ecs().has<cnp::ecs::MobVoice>())
                    sessions.ecs().set<cnp::ecs::MobVoice>({[&s](uint16_t kind, const char *what, Vec3 at) {
                        if (kind >= s.mob_kinds.size() || s.mod_sounds.size() >= 256) return;
                        net::Sound snd;
                        snd.name = s.mob_kinds[kind].voice_name() + "_" + what;
                        snd.x = at.x; snd.y = at.y; snd.z = at.z;
                        if (net::sound_name_ok(snd.name)) s.mod_sounds.push_back(snd.encode());
                    }});
                cnp::ecs::step_mobs(sessions.ecs(), s.world, s.registry, s.mob_kinds, targets,
                                    (float)kTickSeconds, mob_now,
                                    (s.has_sky ? 0.0f : daylight_at(sessions.time_of_day())),
                                    [&](uint32_t peer, uint16_t amount, uint16_t bare, Vec3 push) {
                                        sessions.hurt((net::PeerId)peer, amount, mob_now, bare);
                                        transport.send((net::PeerId)peer, net::Channel::Event,
                                                       net::Push{push.x, push.y, push.z}.encode(),
                                                       true);
                                    },
                                    [&](int x, int y, int z) {
                                        cnp::ecs::swap_door(s.world, s.registry, x, y, z,
                                                            [&](int bx, int by, int bz, BlockId b,
                                                                uint8_t f) {
                                                                write_block(s, bx, by, bz, b, f);
                                                            });
                                    });
                // And through the world's own openings: a cube thrown into a portal.
                sessions.cross_mobs(mob_now);
                if (natural_mobs && tick % 40 == 0) {
                    const cnp::ecs::SpawnResult sr = cnp::ecs::spawn_and_despawn(
                        sessions.ecs(), s.world, s.registry, s.mob_kinds, targets,
                        (s.has_sky ? 0.0f : daylight_at(sessions.time_of_day())), 6, (uint32_t)tick, mob_now);
                    if ((sr.came || sr.went) && !quiet)
                        std::printf("        creatures: %d came out of the dark, %d went\n",
                                    sr.came, sr.went);
                }
            }

            // Stacks on the ground: they fall, and whoever is standing on one
            // picks it up. Two explicit calls rather than systems, because both
            // need this world's blocks and the second has an answer -- see
            // core/ecs/drop.h.
            // How much of everybody is left. Before the drops and the sending,
            // because a body that healed this tick should say so in the same
            // tick rather than one later.
            sessions.step_health(kTickSeconds);
            sessions.step_time(kTickSeconds);

            for (const net::ServerSessions::Death &d : sessions.deaths_to_serve()) {
                transport.send(d.peer, net::Channel::Event, d.back.encode(), true);
                const net::Session *who = sessions.find(d.peer);
                if (!quiet)
                    std::printf("        '%s' died and was put back at %.1f %.1f %.1f\n",
                                who ? who->name.c_str() : "?", (double)d.back.x,
                                (double)d.back.y, (double)d.back.z);
            }

            // A body with nothing left leaves what it was carrying and goes.
            // Before the drops are stepped, so the stack it left falls on the
            // tick it appeared instead of hanging in the air for one.
            if (const size_t died = cnp::ecs::reap_dead(sessions.ecs(), seconds_since(t_start));
                died > 0 && !quiet)
                std::printf("        %zu bod%s died and left what %s carrying\n", died,
                            died == 1 ? "y" : "ies", died == 1 ? "it was" : "they were");

            cnp::ecs::step_drops(sessions.ecs(), s.world, s.registry, (float)kTickSeconds);
            // Blocks falling (core.fall): each put down where it lands, or left
            // as its item when that cell is taken, and core:landed said.
            cnp::ecs::step_falling(
                sessions.ecs(), s.world, s.registry, (float)kTickSeconds,
                [&](int x, int y, int z, BlockId block, uint8_t facing, int fell) {
                    const BlockId there = s.world.get_block(x, y, z);
                    if (there == kAir || !s.registry.is_solid(there)) {
                        write_told(s, x, y, z, block, facing);
                    } else if (const ItemId item = s.items.of_block(block); item != kNoItem) {
                        ItemStack stack;
                        stack.item  = item;
                        stack.count = 1;
                        cnp::ecs::spawn_drop(sessions.ecs(), stack, (float)x + 0.5f, (float)y + 1.2f,
                                             (float)z + 0.5f, seconds_since(t_start));
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
                    s.mods.events().emit("core:landed", f, 5, &ignored, err);
                });
            cnp::ecs::expire_drops(sessions.ecs(), seconds_since(t_start));
            {
                const double drop_now = seconds_since(t_start);
                for (const flecs::entity_t who :
                     cnp::ecs::collect_drops(sessions.ecs(), s.items, drop_now)) {
                    // A bag that changed has to reach its owner. collect_drops
                    // names the avatar and not the peer on purpose: which
                    // connection an avatar belongs to is the session table's
                    // business, and a stack on the ground does not have one.
                    const net::Session *owner = sessions.driver_of((uint64_t)who);
                    if (owner == nullptr) continue;
                    transport.send(owner->id, net::Channel::Event,
                                   sessions.inventory_message(owner->id), true);
                }
            }

            ++tick;
            g_tick_stats.end_tick(seconds_since(t_start), quiet);
            {
                // For /metrics and --watchdog.
                RoomShared   &me  = g_router.rooms[(size_t)g_room];
                const int64_t sec = (int64_t)std::chrono::duration_cast<std::chrono::seconds>(
                                        std::chrono::steady_clock::now() - g_started).count();
                const int     ms  = (int)(g_tick_stats.last_took * 1e3);
                if (sec / 60 != me.beat.load() / 60) me.worst_tick_ms = ms;
                else if (ms > me.worst_tick_ms) me.worst_tick_ms = ms;
                me.beat = sec;
            }
            ++ticks_run;

            // Where a returning player left off, once they are standing
            // somewhere. Not at the join: the correction goes out on the event
            // channel and the welcome on the handshake channel, ENet orders
            // neither against the other, and a client that has not joined has
            // no body to move. After its first accepted claim it is in the
            // world, and it applies this like any other correction.
            for (auto it = places_owed.begin(); it != places_owed.end();) {
                const net::Session *sess = sessions.find(it->first);
                if (!sess || sess->state != net::SessionState::Ready) {
                    spawn_owed.erase(it->first);
                    it = places_owed.erase(it);
                    continue;
                }
                // A refused claim says the same: it came from a body. A client
                // that puts itself inside rock -- the middle of a world with
                // no plaza -- is refused every claim, and waiting for one to
                // be accepted left it there for good.
                if (!sess->has_moved && sess->corrections == 0) {
                    ++it;
                    continue;
                }

                const StoredPlayer &was   = it->second;
                const bool          fresh = spawn_owed.erase(it->first) > 0;
                const bool finite = std::isfinite(was.x) && std::isfinite(was.y) &&
                                    std::isfinite(was.z);
                // The world changed under where they stood: the first place with
                // room straight above it, up to 32 nodes, on the top of whatever
                // was built there. None, or a row that is nonsense: the spawn.
                const auto no_room = [&](float y) {
                    return box_hits_world(s.world, s.registry,
                                          body_box(Vec3{was.x, y, was.z}, s.movement.width * 0.5f));
                };
                float at_y    = was.y;
                bool  blocked = !finite;
                if (finite && no_room(at_y)) {
                    blocked = true;
                    for (int up = 1; up <= 32 && blocked; ++up)
                        if (!no_room(std::floor(was.y) + (float)up)) {
                            at_y    = std::floor(was.y) + (float)up;
                            blocked = false;
                        }
                    if (!blocked && !quiet)
                        std::printf("        '%s' left off where there is no room now, and starts "
                                    "%.0f above it\n",
                                    sess->name.c_str(), (double)(at_y - was.y));
                }
                if (blocked)
                    std::printf("        '%s' left off where there is no room now, and starts "
                                "at the spawn\n",
                                sess->name.c_str());

                // SENT, and not assumed. This used to say "the spawn they are
                // already at", which was never true: a client puts itself in
                // the middle of the world, and a blocked player stayed there.
                //
                // As a Teleport either way -- the server moved them -- and not a
                // Correct, which a client reads as a claim refused from somebody
                // standing still: measured, a returning player's run went red on
                // "where you left off". The spawn turns them to face the first
                // opening, which is what it stands in front of; a place of their
                // own turns them back to where they looked when they left, or,
                // for a row that kept no look, keeps the one their first claim
                // came with.
                const bool             to_spawn = fresh || blocked;
                const net::SpawnPoint &sp       = sessions.spawn();
                const Vec3             n        = first_opening.normal();
                const Vec3 face = portals ? Vec3{-n.x, 0.0f, -n.z} : Vec3{0.0f, 0.0f, -1.0f};
                Vec3       look{was.look_x, was.look_y, was.look_z};
                if (!(length(look) > 1e-6f)) sess->interest.look(look.x, look.y, look.z);
                const Vec3 kept = length(look) > 1e-6f ? normalize(look) : face;
                const net::SessionAction back = sessions.teleport(
                    it->first, blocked ? sp.x : was.x, blocked ? sp.y : at_y,
                    blocked ? sp.z : was.z, seconds_since(t_start),
                    to_spawn ? "the spawn" : "where you left off", to_spawn ? &face : &kept);
                if (!back.reply.empty())
                    transport.send(it->first, (net::Channel)back.reply_channel, back.reply, true);
                if (!back.log.empty() && !quiet) std::printf("        %s\n", back.log.c_str());
                it = places_owed.erase(it);
            }

            if (save_ticks > 0 && tick % save_ticks == 0) {
                for (const net::PeerId peer : sessions.ready_peers()) keep_player(peer, false);
                save_world(false);
            }

            // The world around everybody (TODO.md OPEN-35, OPEN-38): what a
            // session is owed is made or read back before the streaming below
            // looks for it, and what nobody has been near for a while is let
            // go of.
            {
                ++s.column_clock;
                std::vector<ColumnPos> centres, windows;
                for (const net::PeerId peer : sessions.ready_peers())
                    if (const net::Session *sess = sessions.find(peer)) {
                        centres.push_back(column_of(sess->interest.centre()));
                        for (const ChunkPos w : sess->interest.window_centres())
                            windows.push_back(column_of(w));
                    }

                // And wherever something stands that is not a session: a stack
                // on the ground, a creature, a mod's thing. Its column stays in
                // memory while it is there, and is made if it walks into one
                // that is not -- a world let go of under a stack is a stack
                // falling for five minutes.
                std::vector<ColumnPos> anchors;
                sessions.ecs().each([&](flecs::entity, const ecs::Position &p) {
                    anchors.push_back(
                        column_of_cell((int)std::floor(p.x), (int)std::floor(p.z)));
                });
                // And the openings: the far end of a portal is a world somebody
                // arrives in, made before they do and not only once a player on
                // this side is near.
                // All of it and a few cells round, which is where a mod builds
                // its frame; the steps are half a column, so none is skipped.
                for (int i = 0; i < 2; ++i)
                    if (const Portal *o = sessions.opening(i)) {
                        const int r = (int)std::max(o->half_w, o->half_h) + 3;
                        const int cx = (int)std::floor(o->center.x), cz = (int)std::floor(o->center.z);
                        for (int dx = -r; dx <= r + 7; dx += 8)
                            for (int dz = -r; dz <= r + 7; dz += 8)
                                anchors.push_back(column_of_cell(cx + std::min(dx, r),
                                                                 cz + std::min(dz, r)));
                    }
                // And what a mod keeps: a robot working far from everybody.
                for (auto it = s.kept.begin(); it != s.kept.end();) {
                    if (it->second < s.column_clock) {
                        it = s.kept.erase(it);
                    } else {
                        anchors.push_back(it->first);
                        ++it;
                    }
                }
                std::sort(anchors.begin(), anchors.end(), [](const ColumnPos &a, const ColumnPos &b) {
                    return (a.z != b.z) ? a.z < b.z : a.x < b.x;
                });
                anchors.erase(std::unique(anchors.begin(), anchors.end()), anchors.end());
                for (const ColumnPos c : anchors) s.keeper.columns().touch(c, s.column_clock);

                LightRun lights(s);
                const double t_cols = seconds_since(t_start);
                advance_columns(s, centres, anchors, kColumnBudgetMs, windows);
                g_tick_stats.cols += seconds_since(t_start) - t_cols;
                centres.insert(centres.end(), windows.begin(), windows.end());
                if (s.column_clock % kColumnUnloadEvery == 0) unload_idle(s, centres);
            }

            // Streaming, after the tick so a chunk goes out reflecting the
            // world as of this tick and not halfway through it. A chunk goes
            // out once its column is lit.
            std::unordered_set<uint32_t>        still_streaming;
            const std::function<bool(ChunkPos)> lit_chunk = [&s](ChunkPos p) {
                return s.keeper.lit_chunk(p);
            };
            for (const net::PeerId peer : sessions.ready_peers()) {
                net::Session *sess = sessions.find_mutable(peer);
                if (!sess) continue;

                // What it no longer holds, before anything new: on the chunk
                // channel, so a chunk let go and sent again arrives after the
                // message that let it go.
                const std::vector<ChunkPos> gone = sess->interest.take_gone();
                for (size_t at = 0; at < gone.size(); at += net::kMaxChunksGone) {
                    net::ChunksGone msg;
                    msg.chunks.assign(gone.begin() + (ptrdiff_t)at,
                                      gone.begin() +
                                          (ptrdiff_t)std::min(gone.size(), at + net::kMaxChunksGone));
                    transport.send(peer, net::Channel::Chunk, msg.encode());
                    s.chunks_let_go += msg.chunks.size();
                }

                if (!sess->has_moved) continue;   // nowhere to stream to yet

                std::vector<std::vector<uint8_t>> messages;
                const size_t bytes = net::collect_chunk_messages(
                    s.world, sess->interest, messages, lit_chunk, seconds_since(t_start));
                for (const auto &m : messages)
                    transport.send(peer, net::Channel::Chunk, m);

                chunk_bytes_sent += bytes;
                chunks_sent += messages.size();
                if (!messages.empty()) still_streaming.insert((uint32_t)peer);
            }

            // What the mods changed while this tick ran, told to the people who
            // can use it. After the streaming loop and not inside
            // host_set_block: src/script must stay free of core/net, a handler
            // that writes a hundred cells should not send a hundred packets
            // from inside itself, and the decision below needs to know which
            // sessions were sent world this tick.
            //
            // Two rules, both of them about ONE session:
            //
            //   * a cell in a chunk it has not been sent is a cell that will
            //     arrive WITH that chunk -- the chunk is encoded from the world
            //     as it is now -- so it is not owed anything;
            //   * a session that was sent chunks this tick is a session whose
            //     world has not arrived, and the world comes first. What it is
            //     owed waits, it is not dropped.
            {
                for (const net::PeerId peer : sessions.ready_peers()) {
                    net::Session *sess = sessions.find_mutable(peer);
                    if (sess == nullptr) continue;

                    ServerState::Owed &owed = s.owed[(uint32_t)peer];
                    for (uint64_t key : s.mod_changed) {
                        int cx, cy, cz;
                        ServerState::unpacked(key, cx, cy, cz);
                        if (already_has(sessions, peer, cx, cy, cz)) owed.add(key);
                    }

                    // The only place the queue can still grow past the cap: a
                    // session whose world is arriving is told nothing, and
                    // everything else is either sent or turned into a chunk
                    // below.
                    //
                    // Past the cap the queue is emptied -- and the chunks it
                    // named are dropped from this session's interest, so the
                    // streamer sends them again with every one of those cells
                    // in them. It used to be emptied and nothing else, which
                    // left that client wrong about those cells until something
                    // happened to re-send the chunk, and nothing did.
                    //
                    // Every cell in here is in a chunk this session HAS -- see
                    // already_has, which is what put it here -- so there is
                    // always something to forget.
                    if (owed.order.size() > kMaxOwedCells) {
                        std::vector<net::CellPos> stale;
                        stale.reserve(owed.order.size());
                        for (uint64_t key : owed.order) {
                            int cx, cy, cz;
                            ServerState::unpacked(key, cx, cy, cz);
                            stale.push_back(net::CellPos{cx, cy, cz});
                        }

                        std::vector<ChunkPos>     every_chunk;
                        std::vector<net::CellPos> none;
                        net::split_owed_cells(stale, 1, every_chunk, none);

                        for (const ChunkPos p : every_chunk)
                            if (sess->interest.forget(p)) ++s.mod_chunks_resent;

                        // Said once so a long run does not drown in it.
                        if (!s.owed_overflowed) {
                            s.owed_overflowed = true;
                            std::printf("server: a session is owed more than %zu changed cell(s); "
                                        "sending the %zu chunk(s) they are in again\n",
                                        kMaxOwedCells, every_chunk.size());
                        }
                        owed.clear();
                    }

                    if (still_streaming.count((uint32_t)peer) != 0) {
                        s.mod_changes_held += (long)owed.order.size();
                        continue;
                    }

                    // Cell by cell, or the chunk itself.
                    //
                    // A cell change is seventeen bytes on this wire; a chunk of
                    // this world is a couple of hundred. So a mod that rewrites
                    // a 32x32x32 chunk used to send 32768 messages to each of
                    // up to eight sessions, and the honest answer past about a
                    // hundred and fifty cells is to drop the chunk from what
                    // this session has and let the streamer send it again --
                    // encoded from the world as it is now, so it carries every
                    // one of those cells and nothing has to be listed.
                    //
                    // Which is also what makes the overflow above rare rather
                    // than routine: what stays in the queue is scattered cells.
                    std::vector<net::CellPos> cells;
                    cells.reserve(owed.order.size());
                    for (uint64_t key : owed.order) {
                        int cx, cy, cz;
                        ServerState::unpacked(key, cx, cy, cz);
                        cells.push_back(net::CellPos{cx, cy, cz});
                    }

                    std::vector<ChunkPos>     resend;
                    std::vector<net::CellPos> one_by_one;
                    net::split_owed_cells(cells, net::kCellsWorthAChunk, resend, one_by_one);

                    for (const ChunkPos p : resend)
                        if (sess->interest.forget(p)) ++s.mod_chunks_resent;

                    s.mod_changes_told += (long)one_by_one.size();
                    for (const net::CellPos &c : one_by_one)
                        transport.send(peer, net::Channel::Block,
                                       net::ServerSessions::block_change_message(
                                           c.x, c.y, c.z, s.world.get_block(c.x, c.y, c.z),
                                           s.world.get_facing(c.x, c.y, c.z)),
                                       true);
                    owed.clear();
                }
                s.mod_changed.clear();
                s.mod_changed_seen.clear();

                // And the bags a mod reached into. Unconditional, not held back
                // the way a block change is: an inventory is one message, it is
                // sent only when a mod actually moved something, and a player
                // drawn holding what a mod took is being lied to about what
                // they can build with.
                for (uint32_t peer : s.bags_touched)
                    if (sessions.find((net::PeerId)peer))
                        transport.send((net::PeerId)peer, net::Channel::Event,
                                       sessions.inventory_message((net::PeerId)peer), true);
                s.bags_touched.clear();

                // And what the mods said on their own channels. Not held back
                // behind the world the way a block change is: a mod's message
                // is about the mod's own state and not about a cell the client
                // may not have yet, and holding it would be the engine having
                // an opinion about a conversation it does not read.
                // What the server's own stdin typed: admin commands, as the
                // owner. Room 0 serves it, since the store is the process's.
                if (g_room == 0)
                    for (const admin::ConsoleLine &in : admin::take_console_lines()) {
                        std::string              cname, cargs;
                        std::vector<std::string> said;
                        const std::string        cmd = in.line.empty() || in.line[0] == '/' ? in.line : "/" + in.line;
                        if (!net::chat_split(cmd, cname, cargs)) {
                            if (in.reply) in.reply->set_value({});
                            continue;
                        }
                        if (cname == "help")
                            said = admin::help(in.who);
                        else if (!admin::run(in.who, cname, cargs, admin_hooks(), said))
                            said.push_back("no admin command /" + cname + " -- help lists them");
                        if (in.reply) in.reply->set_value(said);
                        else
                            for (const std::string &l : said) std::printf("console: %s\n", l.c_str());
                    }

                if (g_room == 0) admin::federation_tick(admin_hooks());

                // A newer release (--no-update-check to not ask): said once
                // in the log and to the admins online, and to each coming in.
                if (g_room == 0 && !g_update_told && !update::newer().empty()) {
                    g_update_told = true;
                    std::printf("server: %s is out, this is %s (%s)\n", update::newer().c_str(),
                                kEngineVersion, kReleasesPage);
                    for (const admin::Online &o : admin_hooks().online())
                        if (admin::may(o.who, "restart")) say_to(s, o.peer, update_line());
                }

                // Blocks /rollback puts back here: each one only if it still
                // holds what the player left in it.
                if ((size_t)g_room < g_router.restores.size() && !g_router.restores[(size_t)g_room].empty()) {
                    std::vector<admin::Restore> back;
                    back.swap(g_router.restores[(size_t)g_room]);
                    LightRun lights(s);
                    for (const admin::Restore &r : back)
                        if ((int)s.world.get_block(r.x, r.y, r.z) == r.expect)
                            write_told(s, r.x, r.y, r.z, (BlockId)r.block, (uint8_t)r.facing);
                }

                // A restart or a stop counted down to its end (/restart).
                if (g_room == 0) switch (admin::tick(admin_hooks())) {
                    case admin::Stop::None: break;
                    case admin::Stop::Restart:
                        std::printf("server: restarting\n");
                        g_exit_code = 75;
                        g_stop      = 1;
                        break;
                    case admin::Stop::Stop:
                        std::printf("server: stopping\n");
                        g_stop = 1;
                        break;
                    }

                for (const ServerState::ModSay &say : s.mod_says) {
                    const std::vector<uint8_t> bytes =
                        say.chat     ? net::Chat{say.text}.encode()
                        : say.notice ? net::Notice{say.text}.encode()
                                     : net::ModData{say.text}.encode();
                    const net::Channel channel = say.notice || say.chat
                                                     ? net::Channel::Event
                                                     : (net::Channel)say.channel;
                    if (say.player > 0) {
                        if (sessions.find((net::PeerId)say.player))
                            transport.send((net::PeerId)say.player, channel, bytes, true);
                        continue;
                    }
                    for (const net::PeerId peer : sessions.ready_peers()) {
                        // A notice is for the people in this world, not for a
                        // portal looking in from another one.
                        const net::Session *to = sessions.find(peer);
                        if ((say.notice || say.chat) && (!to || to->scope != net::Scope::Enter))
                            continue;
                        transport.send(peer, channel, bytes, true);
                    }
                }
                s.mod_says.clear();

                // The mods' machines on by a tick (core.craft), and what is
                // mounted on them, before the riders are put on their seats.
                cnp::ecs::step_machines(sessions.ecs(), s.world, s.registry, (float)kTickSeconds);

                // The workers riding them, on their places (core.mob_ride): a
                // place is x right, y up, z ahead in what carries it.
                for (auto it = s.mob_rides.begin(); it != s.mob_rides.end();) {
                    double tx = 0, ty = 0, tz = 0, yaw = 0;
                    const ServerState::MobRide &r = it->second;
                    if (!s.lua_ecs.get(r.thing, "x", tx) || !s.lua_ecs.get(r.thing, "y", ty) ||
                        !s.lua_ecs.get(r.thing, "z", tz)) {
                        it = s.mob_rides.erase(it);
                        continue;
                    }
                    s.lua_ecs.get(r.thing, "yaw", yaw);
                    const double sn = std::sin(yaw), cs = std::cos(yaw);
                    if (!cnp::ecs::mob_place(sessions.ecs(), it->first, (float)(tx + cs * r.x + sn * r.z),
                                             (float)(ty + r.y), (float)(tz + sn * r.x - cs * r.z)))
                        it = s.mob_rides.erase(it);
                    else
                        ++it;
                }

                // Seats: the ones mods asked for, then every carried body onto
                // its seat, and off it when what carried it has gone. Before the
                // bodies mods placed: a mod that lets somebody off a seat and
                // puts them down beside it in one tick made the getting off
                // first, and its correction sent after the placement told the
                // client an older placement than the server's -- every claim
                // after was "from before it was put somewhere".
                std::vector<std::pair<net::PeerId, net::SessionAction>> rides =
                    std::move(s.mod_rides);
                s.mod_rides.clear();
                for (auto &r : sessions.step_riders(seconds_since(t_start))) rides.push_back(std::move(r));
                for (const auto &[peer, ra] : rides) {
                    if (ra.reply_channel == (uint8_t)net::Channel::Entity)
                        transport.send(peer, net::Channel::Entity, ra.reply, false);
                    else if (!ra.reply.empty())
                        transport.send(peer, net::Channel::Block, ra.reply, true);
                    for (const auto &m : ra.more) transport.send(peer, net::Channel::Block, m, true);
                    if (!ra.log.empty() && !quiet) std::printf("        %s\n", ra.log.c_str());
                }

                // And the bodies a mod moved, now the block changes have gone
                // out ahead of them. A Teleport, as a portal's: the server moved
                // them, and a second of grace for the frames before they know.
                //
                // Ahead of them for THIS player whatever the streamer is doing,
                // and the move on the same channel after them. A session still
                // being sent chunks is told no block changes, and a player who
                // had just crossed in and got into a kart was put where the
                // parked kart still stood on their side: the chassis inside it,
                // pushed into the road, 12 to 52 corrections one start in four.
                for (const ServerState::ModPlace &p : s.mod_places) {
                    const auto owed = s.owed.find((uint32_t)p.player);
                    if (owed != s.owed.end()) {
                        for (uint64_t key : owed->second.order) {
                            int cx, cy, cz;
                            ServerState::unpacked(key, cx, cy, cz);
                            transport.send(p.player, net::Channel::Block,
                                           net::ServerSessions::block_change_message(
                                               cx, cy, cz, s.world.get_block(cx, cy, cz),
                                               s.world.get_facing(cx, cy, cz)),
                                           true);
                            ++s.mod_changes_told;
                        }
                        owed->second.clear();
                    }
                    const net::SessionAction moved = sessions.teleport(
                        p.player, p.at.x, p.at.y, p.at.z, seconds_since(t_start), "a mod",
                        length(p.look) > 1e-6f ? &p.look : nullptr);
                    if (!moved.reply.empty())
                        transport.send(p.player, net::Channel::Block, moved.reply, true);
                }
                s.mod_places.clear();
                for (const ServerState::ModLaunch &l : s.mod_launches) {
                    if (sessions.launch(l.player, l.vel, seconds_since(t_start), l.seconds))
                        transport.send(l.player, net::Channel::Event,
                                       net::Push{l.vel.x, l.vel.y, l.vel.z, true}.encode(), true);
                }
                s.mod_launches.clear();

                // Nobody on foot stands inside a machine. The client keeps
                // its body out of the hulls it draws (protocol 100), but it
                // draws them a moment late and a machine driven into a body
                // is in it before its client knows: the server shoves the
                // body out the nearest side, harder the faster the machine
                // comes at it, with a launch's grace for the speed. A shove a
                // fifth of a second, since a push adds to the body's speed.
                for (const net::PeerId peer : sessions.ready_peers()) {
                    const double now = seconds_since(t_start);
                    auto         was = s.shoved.find(peer);
                    if (was != s.shoved.end() && now - was->second < 0.2) continue;
                    const net::Session *who = sessions.find(peer);
                    if (who == nullptr || who->scope != net::Scope::Enter || who->riding != 0 ||
                        !who->avatar.is_alive() || !who->authority().valid)
                        continue;
                    const MovementState &at = who->authority();
                    // On a roof: carried at the machine's speed and as high
                    // as it climbs, which the movement check would take for
                    // a body running and flying. The price, said: a body on
                    // a machine is not held to a walk.
                    if (cnp::ecs::on_machine(sessions.ecs(), Vec3{at.x, at.y, at.z}, kPlayerHalfWidth)) {
                        sessions.launch(peer, Vec3{}, now, 0.5);
                        continue;
                    }
                    Vec3 out, machine_vel;
                    if (!cnp::ecs::in_machine(sessions.ecs(), Vec3{at.x, at.y, at.z}, kPlayerHalfWidth,
                                              kPlayerHeight, out, machine_vel))
                        continue;
                    const float k = net::Push::kMax;
                    Vec3 v = out * (3.0f + std::max(0.0f, machine_vel.x * out.x + machine_vel.z * out.z));
                    v.x = std::clamp(v.x, -k, k);
                    v.z = std::clamp(v.z, -k, k);
                    v.y = 1.5f;
                    if (sessions.launch(peer, v, now, 0.5))
                        transport.send(peer, net::Channel::Event, net::Push{v.x, v.y, v.z}.encode(), true);
                    s.shoved[peer] = now;
                }
                for (const auto &[peer, mask] : s.mod_seals)
                    transport.send(peer, net::Channel::Event, net::Seal{mask}.encode(), true);
                s.mod_seals.clear();
                schedule_portals(s, seconds_since(t_start));


                for (const auto &m : s.mod_sounds)
                    for (const net::PeerId peer : sessions.ready_peers())
                        transport.send(peer, net::Channel::Block, m, true);
                s.mod_sounds.clear();

                // A seat that has been given up owes nobody anything.
                for (auto it = s.owed.begin(); it != s.owed.end();)
                    it = sessions.find((net::PeerId)it->first) ? std::next(it) : s.owed.erase(it);
            }

            // The openings, to anybody who has not been told about the
            // version they are at. One test rather than a broadcast on change:
            // a player who joins into a world that already has a pair needs the
            // same message as a player who watched it being placed, and this is
            // the only place that can tell there is no difference.
            //
            // Reliable, because a client that misses this draws no portal at
            // all and there is nothing to correct it -- unlike a position,
            // which is replaced 20 times a second.
            for (const net::PeerId peer : sessions.ready_peers()) {
                net::Session *sess = sessions.find_mutable(peer);
                if (!sess || sess->portals_sent == sessions.portals_version()) continue;

                const net::Portals msg = sessions.portals_message();
                transport.send(peer, net::Channel::Event, msg.encode(), true);
                sess->portals_sent = sessions.portals_version();
            }

            // What each player is carrying, once, as soon as there is
            // somebody to tell. Every later change is sent by whatever caused
            // it; this is the one nothing causes -- an empty bag at the moment
            // of joining -- and without it a player who has not broken anything
            // yet has no bar and cannot open their bag at all.
            for (const net::PeerId peer : sessions.inventories_to_send())
                transport.send(peer, net::Channel::Event, sessions.inventory_message(peer), true);

            // The panel that is up, to whoever has not been told this version
            // of it.
            //
            // A mod chooses who sees it: ui_show takes a player, and the events
            // carry one so a mod has an id to pass. A panel shown to
            // ui::kAllPlayers is the world's -- a notice everybody gets.
            //
            // One message PER PLAYER, not one message sent to several: two
            // people in a world can be looking at different panels, and that is
            // the point of OPEN-8 being closed.
            for (const net::PeerId peer : sessions.panels_to_send())
                transport.send(peer, net::Channel::Event, sessions.panel_message_for(peer), true);

            // And what is in the chest each of them has open. Two people with
            // the same one open are both behind after either moves a stack:
            // there is ONE Inventory on the server, which is what makes a
            // container a place rather than a second bag.
            for (const net::PeerId peer : sessions.containers_to_send())
                transport.send(peer, net::Channel::Event, sessions.container_message(peer, s.now), true);

            // And what each of them is crafting in. One viewer per grid, so
            // there is no revision to compare -- see Session::craft_owed.
            for (const net::PeerId peer : sessions.crafts_to_send())
                transport.send(peer, net::Channel::Event, sessions.craft_message(peer), true);

            // And how much of each of them is left. Reliable, like the bag: a
            // bar that missed the message saying a player is nearly dead is a
            // bar that lies until the next thing happens to them.
            for (const net::PeerId peer : sessions.vitals_to_send())
                transport.send(peer, net::Channel::Event, sessions.vitals_message(peer), true);

            // And how a body moves, to the one a mod gave numbers of its own
            // (TODO.md OPEN-47). Reliable, for the reason the bar is: a body told
            // nothing goes on moving by the old numbers against a server that is
            // already checking the new ones.
            for (const net::PeerId peer : sessions.movement_to_send())
                transport.send(peer, net::Channel::Event, sessions.movement_message(peer), true);
            // And how a mod holds a body, to that body's own player (OwnPose).
            for (const net::PeerId peer : sessions.pose_to_send())
                transport.send(peer, net::Channel::Event, sessions.pose_message(peer), true);

            // And what time it is here. To OBSERVERS as well as to players:
            // somebody looking into this world through a portal is looking at
            // its sky, and a sky stuck at the hour they arrived is the wrong
            // world drawn correctly.
            //
            // Unreliable, like the heartbeat and for the same reason: this says
            // what o'clock it is NOW, so a retransmitted one arrives announcing
            // an hour that has already gone. One lost costs a second of stale
            // sky -- 1/1200 of a day, against the 1/16 of a day one light level
            // is worth -- and the next comes a second later.
            //
            // It has to be unreliable for the silence rule to work at all.
            // Sent reliably it left the server owing every session an answer
            // once a second for ever, and net::quiet_for would then never have
            // let go of anybody; the loopback gate is what said so. Same rule
            // as the heartbeat: a message whose value expires before its own
            // retransmission could arrive must not be sent reliably.
            if (const std::vector<net::PeerId> owed = sessions.time_to_send(); !owed.empty()) {
                const std::vector<uint8_t> msg = sessions.time_message();
                for (const net::PeerId peer : owed)
                    transport.send(peer, net::Channel::Event, msg, false);
            }

            // Still here, and this is what this world currently is.
            //
            // Once a second, per session rather than as one broadcast, because
            // a session that joined half a second ago is not owed one yet and
            // the timer is what says so.
            //
            // NOT reliable, and that is the whole point. A heartbeat says what
            // is true now; a retransmitted one says what was true two seconds
            // ago, and while ENet is retransmitting it every heartbeat behind
            // it waits in order. Measured at 20% loss: gaps of 3.5 to 9.2 s
            // between beats sent one second apart, with the client counting
            // nothing lost -- because a reliable channel never shows a gap in
            // the sequence, so that counter could not move. Unsequenced, a lost
            // beat costs one second and the client can see it went missing.
            {
                const double         beat_now = seconds_since(t_start);
                const net::Heartbeat beat     = sessions.heartbeat_message();
                for (const net::PeerId peer : sessions.ready_peers()) {
                    net::Session *sess = sessions.find_mutable(peer);
                    if (!sess) continue;
                    if (beat_now - sess->last_beat_at < net::kHeartbeatInterval) continue;

                    net::Heartbeat mine = beat;
                    mine.seq            = ++sess->beats_sent;
                    transport.send(peer, net::Channel::Event, mine.encode(), false);
                    sess->last_beat_at = beat_now;
                }
            }

            // Everybody else. Unreliable and sequenced: a dropped update is
            // replaced by the next one 50 ms later, and asking again would
            // deliver the answer after it stopped being true.
            //
            // Sent empty too: a frame is ALL the bodies, so one missing from
            // every part of it is gone at once -- hidden, killed, walked out
            // of range -- and not after five seconds of silence (TODO.md
            // OPEN-20). An empty frame is how the last goes. In parts of
            // kEntitiesPerPart (protocol 109), each under one datagram.
            for (const net::PeerId peer : sessions.ready_peers()) {
                const std::vector<net::EntityState> others =
                    sessions.entities_for(peer, kEntityRadius);

                const size_t parts = std::max<size_t>(1, (others.size() + net::kEntitiesPerPart - 1) /
                                                             net::kEntitiesPerPart);
                for (size_t k = 0; k < parts; ++k) {
                    net::Entities msg;
                    msg.frame = (uint16_t)tick;
                    msg.part  = (uint8_t)k;
                    msg.parts = (uint8_t)parts;
                    const size_t from = k * net::kEntitiesPerPart;
                    const size_t to   = std::min(others.size(), from + net::kEntitiesPerPart);
                    msg.players.assign(others.begin() + (long)from, others.begin() + (long)to);
                    transport.send(peer, net::Channel::Entity, msg.encode(), false);
                }
            }
        }

        if (chunks_sent > 0)
            std::printf("        streamed %ld chunks, %.1f KB, %.0f bytes each on average\n",
                        chunks_sent, (double)chunk_bytes_sent / 1024.0,
                        (double)chunk_bytes_sent / (double)chunks_sent);

        // Should be zero on a healthy link, and worth printing precisely because
        // it is the number nobody would think to look for: a frame that failed
        // to authenticate, was replayed, or arrived too far behind to judge.
        std::printf("        rejected %llu sealed frames\n",
                    (unsigned long long)transport.rejected_frames());

        // The bags of everybody still in the world. A server stopped while
        // people are connected produces no Disconnected event for any of them,
        // so without this the only players whose bags survive are the ones who
        // happened to leave first.
        for (const net::PeerId peer : sessions.ready_peers()) keep_player(peer);

        transport.close();
        keep_pets();            // while the pets are still here to keep
        s.sessions = nullptr;   // the table goes with this scope
        g_router.rooms[(size_t)g_room].sessions = nullptr;
        g_router.rooms[(size_t)g_room].items    = nullptr;
        g_router.rooms[(size_t)g_room].models   = nullptr;
    }

    const double tick_seconds = seconds_since(t_tick);

    if (g_stop) std::printf("server: told to stop, keeping the world\n");

    // What changed since the last save. The one at startup wrote the region, and
    // a column let go of since was kept as it went, so this is everything in
    // which the world differs from the file.
    if (db.is_open()) save_world(true);

    // The hash is over the region the welcome's was taken over, and part of it
    // may have been let go of: read back first, from wherever it was kept.
    for (int cz = 0; cz < region; ++cz)
        for (int cx = 0; cx < region; ++cx) s.keeper.ensure_blocks(ColumnPos{cx, cz}, s.column_clock);

    g_router.rooms[(size_t)g_room].db = nullptr;
    if (db.is_open()) db.close();
    s.keeper.set_db(nullptr);

    const uint64_t hash = world_hash(s.world, s.extent, s.height);

    if (!quiet) {
        std::printf("        %d ticks in %.2f s (%.3f ms/tick), %ld block writes from mods\n",
                    ticks_run, tick_seconds, 1000.0 * tick_seconds / (double)(ticks_run ? ticks_run : 1),
                    s.block_writes);
        if (g_router.rooms.size() > 1)
            std::printf("        held the rooms' lock %.0f ms at the longest\n",
                        1000.0 * g_longest_hold);
        std::printf("        told %ld block change(s) from mods, held %ld back while a world "
                    "was still arriving, resent %ld chunk(s) instead of the cells in them\n",
                    s.mod_changes_told, s.mod_changes_held, s.mod_chunks_resent);
        std::printf("        columns: %llu filled in %.0f ms, %llu lit in %.0f ms, %llu let go, %zu in "
                    "memory (%zu lit); %zu kept in memory in %.1f KB; %llu chunk(s) let go of by "
                    "clients; %llu brought back for a relight to read\n",
                    (unsigned long long)s.keeper.columns().filled_total, s.keeper.fill_ms(),
                    (unsigned long long)s.keeper.columns().lit_total, s.keeper.light_ms(),
                    (unsigned long long)s.keeper.columns().unloaded_total,
                    s.keeper.columns().count(), s.keeper.columns().lit_count(),
                    s.keeper.kept_columns(), (double)s.keeper.kept_bytes() / 1024.0,
                    (unsigned long long)s.chunks_let_go,
                    (unsigned long long)s.keeper.light_reach());

        // What the light cost, which is OPEN-29's own number: passes done for a
        // run of writes instead of one per write, and the writes that could not
        // have changed the light at all.
        if (s.light_batch_flushes > 0 || s.light_writes_skipped > 0)
            std::printf("        light: %d batched pass(es) over %llu cell(s), %llu write(s) "
                        "needed none\n",
                        s.light_batch_flushes, (unsigned long long)s.light_batch_cells,
                        (unsigned long long)s.light_writes_skipped);

        if (!s.modifiers.abms().empty() || !s.modifiers.lbms().empty()) {
            // Cells LOOKED AT beside actions run, because the first is what an
            // ABM costs and the second is what it does -- and a modifier over a
            // block nothing holds costs the first and never the second.
            std::printf("        modifiers: %llu action(s) over %llu cell(s) in %.3f s, "
                        "%d sweep(s) still going\n",
                        (unsigned long long)s.modifier_actions,
                        (unsigned long long)s.modifier_cells, s.modifier_seconds,
                        s.modifiers.behind());
            for (const Modifier &m : s.modifiers.abms())
                std::printf("          %-24s %8llu fired of %10llu tested, %.3f s%s\n",
                            m.name.c_str(), (unsigned long long)m.fired,
                            (unsigned long long)m.tested, m.seconds,
                            m.enabled ? "" : " (disabled)");
            for (const Modifier &m : s.modifiers.lbms())
                std::printf("          %-24s %8llu fired of %10llu tested, %.3f s%s (loading)\n",
                            m.name.c_str(), (unsigned long long)m.fired,
                            (unsigned long long)m.tested, m.seconds,
                            m.enabled ? "" : " (disabled)");
        }
    }
    std::printf("world hash %016llx\n", (unsigned long long)hash);
    return 0;
}

// The server: its rooms, each one serve_room on a thread of its own, sharing
// one socket (see Router).
//
//   cnp_server --listen 25301 --ticks 0 --portals \
//       --room sandbox --mods mods --world-file sandbox.world --portal-to @pista/0 \
//       --room pista --mods games/track/mods --world-file pista.world --portal-to @sandbox/0
//
// What comes before the first --room is every room's; what follows a --room is
// that room's alone. With no --room there is one room, which is the server this
// program always was, with the same arguments.
// --config FILE, replaced by the file's flags where it stands, so what comes
// after it on the command line says otherwise. A line is `key value` or
// `key = value`, meaning --key value; a key alone or set to true is the bare
// flag, false leaves it out; `[room NAME]` starts a room; # starts a comment.
bool expand_config(const std::vector<std::string> &in, std::vector<std::string> &out,
                   std::string &err)
{
    const auto trim = [](std::string t) {
        t.erase(0, t.find_first_not_of(" \t"));
        t.erase(t.find_last_not_of(" \t\r") + 1);
        return t;
    };
    for (size_t i = 0; i < in.size(); ++i) {
        if (in[i] != "--config" || i + 1 >= in.size()) {
            out.push_back(in[i]);
            continue;
        }
        const std::string path = in[++i];
        std::ifstream     f(path);
        if (!f) {
            err = "cannot read the config " + path;
            return false;
        }
        int n = 0;
        for (std::string line; std::getline(f, line);) {
            ++n;
            line = trim(line.substr(0, line.find('#')));
            if (line.empty()) continue;
            if (line.rfind("[room ", 0) == 0 && line.back() == ']') {
                out.push_back("--room");
                out.push_back(trim(line.substr(6, line.size() - 7)));
                continue;
            }
            const size_t eq  = line.find('=');
            const size_t cut = eq != std::string::npos ? eq : line.find_first_of(" \t");
            const std::string key   = trim(line.substr(0, cut));
            const std::string value = cut == std::string::npos ? "" : trim(line.substr(cut + 1));
            if (key.empty() || key.find(' ') != std::string::npos) {
                err = path + ":" + std::to_string(n) + ": not a setting: " + line;
                return false;
            }
            if (value == "false" || value == "no" || value == "off") continue;
            out.push_back("--" + key);
            if (!value.empty() && value != "true" && value != "yes" && value != "on")
                out.push_back(value);
        }
    }
    return true;
}

int main(int argc, char **argv)
{
    // Before anything else, so a crash while a world is being generated says
    // so too.
    install_stack_dumper();
    install_crash_handler();
    install_stop_handler();

    // Line buffered before the first line, which main() may print itself
    // (--owner, rcon): setvbuf after output does nothing reliable, and a log
    // that arrives when the server exits is not a log. See serve_room.
    std::setvbuf(stdout, nullptr, _IOLBF, 0);

    std::vector<std::string> cli;
    {
        std::string err;
        if (!expand_config(std::vector<std::string>(argv + 1, argv + argc), cli, err)) {
            std::printf("server: %s\n", err.c_str());
            return 1;
        }
    }

    // --report FILE [--since 7d]: what a world file remembers, and no server.
    for (size_t i = 0; i + 1 < cli.size(); ++i)
        if (cli[i] == "--report") {
            int64_t since = 0;
            for (size_t j = 0; j + 1 < cli.size(); ++j)
                if (cli[j] == "--since" && !admin::parse_duration(cli[j + 1], since)) {
                    std::printf("server: --since wants 30m, 2h, 7d, 1w\n");
                    return 1;
                }
            return admin::report(cli[i + 1], since,
                                 std::find(cli.begin(), cli.end(), "--json") != cli.end());
        }

    std::vector<std::string>                                      common;
    std::vector<std::pair<std::string, std::vector<std::string>>> rooms;
    for (size_t i = 0; i < cli.size(); ++i) {
        if (cli[i] == "--room" && i + 1 < cli.size()) {
            rooms.push_back({cli[++i], {}});
            continue;
        }
        (rooms.empty() ? common : rooms.back().second).push_back(cli[i]);
    }
    if (rooms.empty()) rooms.push_back({"", {}});
    for (size_t i = 0; i < rooms.size(); ++i)
        for (size_t j = 0; j < i; ++j)
            if (rooms[i].first == rooms[j].first || rooms[i].first.empty()) {
                std::printf("server: every room wants a name of its own, got '%s' twice or "
                            "empty\n", rooms[i].first.c_str());
                return 1;
            }

    const auto last_of = [](const std::vector<std::string> &args, const char *flag) {
        std::string v;
        for (size_t i = 0; i + 1 < args.size(); ++i)
            if (args[i] == flag) v = args[i + 1];
        return v;
    };
    std::string first_world = last_of(rooms[0].second, "--world-file");
    if (first_world.empty()) first_world = last_of(common, "--world-file");

    // Who may do what, once for every room, in the first room's file
    // (src/server/admin.h). --owner is every room's, so it is read here.
    {
        std::string err;
        if (const std::string hook = last_of(common, "--webhook"); !hook.empty() && !admin::set_webhook(hook)) {
            std::printf("server: --webhook wants an http(s) URL\n");
            return 1;
        }
        if (!admin::open(first_world, err)) {
            std::printf("server: %s\n", err.c_str());
            return 1;
        }
        std::string owner = last_of(common, "--owner");
        if (owner.empty()) owner = last_of(rooms[0].second, "--owner");
        if (!owner.empty()) {
            admin::set_owner(owner);
            std::printf("server: '%s' owns this server\n", owner.c_str());
        }
        admin::start_console();

        // RCON: the same commands from a script on this machine.
        const std::string rcon = last_of(common, "--rcon");
        if (!rcon.empty()) {
            if (!admin::start_rcon(std::atoi(rcon.c_str()), last_of(common, "--rcon-password"), err)) {
                std::printf("server: %s\n", err.c_str());
                return 1;
            }
            std::printf("server: rcon on 127.0.0.1:%s\n", rcon.c_str());
        }

        // The server's own life (phase 3 of the administration).
        if (const std::string m = last_of(common, "--motd"); !m.empty()) admin::set_motd(m);
        // A server that listens asks once whether a newer release is out.
        if (!last_of(common, "--listen").empty() &&
            std::find(common.begin(), common.end(), "--no-update-check") == common.end())
            update::start();
        if (const std::string f = last_of(common, "--rules-file"); !f.empty()) {
            std::ifstream in(f);
            std::stringstream text;
            text << in.rdbuf();
            if (!in || text.str().empty()) {
                std::printf("server: --rules-file: cannot read %s\n", f.c_str());
                return 1;
            }
            admin::set_rules(text.str());
        }
        if (const std::string at = last_of(common, "--restart-at"); !at.empty()) {
            if (!admin::restart_at(at)) {
                std::printf("server: --restart-at wants HH:MM\n");
                return 1;
            }
            std::printf("server: restarts every day at %s (under tools/serve.sh)\n", at.c_str());
        }
        std::vector<std::string> files;
        for (const auto &r : rooms) {
            std::string f = last_of(r.second, "--world-file");
            if (f.empty()) f = last_of(common, "--world-file");
            if (!f.empty() && std::find(files.begin(), files.end(), f) == files.end()) files.push_back(f);
        }
        int64_t every = 0;
        if (const std::string e = last_of(common, "--backup-every"); !e.empty() && !admin::parse_duration(e, every)) {
            std::printf("server: --backup-every wants 30m, 6h, 1d\n");
            return 1;
        }
        const std::string keep = last_of(common, "--backup-keep");
        admin::set_backups(files, every, keep.empty() ? 5 : std::atoi(keep.c_str()));
        g_max_per_address = std::atoi(last_of(common, "--max-per-address").c_str());

        // Friend servers (/friend): signed lines on a TCP port of their own,
        // under the key tickets are signed with.
        if (const std::string fed = last_of(common, "--federation"); !fed.empty()) {
            std::string sign = last_of(common, "--ticket-key");
            if (sign.empty()) sign = first_world.empty() ? "server.sign.key" : first_world + ".sign";
            net::SignKeyPair me;
            bool             created = false;
            if (!net::load_or_create_sign_key(sign, me, created, err) ||
                !admin::start_federation(std::atoi(fed.c_str()), me, err)) {
                std::printf("server: %s\n", err.c_str());
                return 1;
            }
            std::printf("server: federation on TCP %s, this server's key %s\n", fed.c_str(),
                        net::to_hex(me.public_key, net::kSignPublicBytes).c_str());
        }
    }

    // More than one room: one identity and one ticket key for all of them,
    // each room trusting its own tickets, so a mouth from one room into
    // another needs no keys on the command line.
    std::vector<std::string> shared;
    if (rooms.size() > 1) {

        std::string key = last_of(common, "--key-file");
        if (key.empty()) key = first_world.empty() ? "server.key" : first_world + ".key";
        std::string sign = last_of(common, "--ticket-key");
        if (sign.empty()) sign = first_world.empty() ? "server.sign.key" : first_world + ".sign";

        net::SignKeyPair issuer;
        bool             created = false;
        std::string      err;
        if (!net::load_or_create_sign_key(sign, issuer, created, err)) {
            std::printf("server: %s\n", err.c_str());
            return 1;
        }
        shared = {"--key-file", key, "--ticket-key", sign, "--trust",
                  net::to_hex(issuer.public_key, net::kSignPublicBytes)};
    }

    for (const auto &r : rooms) g_router.rooms.push_back(RoomShared{r.first});
    // --watchdog SECONDS: a room that has not finished a tick in that long is
    // a server stuck for good; the crash handler prints where, and
    // tools/serve.sh starts it again.
    if (const int dog = std::atoi(last_of(common, "--watchdog").c_str()); dog > 0)
        std::thread([dog] {
            for (;;) {
                std::this_thread::sleep_for(std::chrono::seconds(1));
                const int64_t now = (int64_t)std::chrono::duration_cast<std::chrono::seconds>(
                                        std::chrono::steady_clock::now() - g_started).count();
                for (const RoomShared &r : g_router.rooms)
                    if (r.beat > 0 && now - r.beat > dog) {
                        std::printf("server: watchdog: room '%s' stuck for %lld s\n", r.name.c_str(),
                                    (long long)(now - r.beat));
                        std::fflush(stdout);
                        std::abort();
                    }
            }
        }).detach();
    g_router.queues.resize(rooms.size());
    g_router.far_moved.resize(rooms.size());
    g_router.restores.resize(rooms.size());

    std::vector<int>         rc(rooms.size(), 0);
    std::vector<std::thread> threads;
    for (size_t i = 0; i < rooms.size(); ++i) {
        std::vector<std::string> args{argv[0]};
        args.insert(args.end(), common.begin(), common.end());
        args.insert(args.end(), rooms[i].second.begin(), rooms[i].second.end());
        args.insert(args.end(), shared.begin(), shared.end());

        threads.emplace_back([i, args, &rc, many = rooms.size() > 1] {
            std::vector<char *> av;
            for (const std::string &a : args) av.push_back(const_cast<char *>(a.c_str()));
            av.push_back(nullptr);

            g_room = (int)i;
            std::lock_guard<TurnLock> hold(g_router.big);
            g_held_since = std::chrono::steady_clock::now();
            if (many)
                std::printf("server: room '%s', %zu of %zu\n", g_router.rooms[i].name.c_str(),
                            i + 1, g_router.rooms.size());
            rc[i] = serve_room((int)args.size(), av.data());
            if (rc[i] != 0) g_stop = 1;   // one room down stops the server
        });
    }
    for (std::thread &t : threads) t.join();
    g_router.transport.close();
    admin::close();

    for (const int code : rc)
        if (code != 0) return code;
    return g_exit_code;
}
