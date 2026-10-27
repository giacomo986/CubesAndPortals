// Determinism, which is the property everything in M3 is built on.
//
// DESIGN.md 8, M3: "mapgen deterministico con stage registrabili", and the exit
// test's headless half -- a server that runs a thousand ticks and checks a hash
// of the world. This is that, in process: generate, load the real mods, tick,
// hash, then do the whole thing again and require the same number.
//
// A server and a client that disagree about the ground desync before a single
// packet has been sent, and by then the disagreement is somebody else's bug
// report. Cheaper to catch here.

#include "core/registry/core_nodes.h"
#include "core/registry/node_registry.h"
#include "core/voxel/chunk.h"
#include "core/physics/aabb.h"
#include "core/physics/movement.h"
#include "core/worldgen/spawn.h"
#include "core/worldgen/worldgen.h"
#include "script/mod_host.h"

#include <algorithm>
#include <cstdio>
#include <map>
#include <memory>
#include <string>
#include <vector>

using namespace cnp;

namespace {

int g_failures = 0;

void check(bool cond, const char *what)
{
    std::printf(cond ? "  ok    %s\n" : "  FAIL  %s\n", what);
    if (!cond) ++g_failures;
}

struct Fixture {
    World        world;
    NodeRegistry registry;
    ModHost      mods;

    // The mapgen stages a mod registered, and what they are run against. The
    // same four fields the engine keeps, for the same reasons -- see
    // ServerState in src/server/main.cpp. A fixture that answered
    // core.register_mapgen differently from the engine would be a gate on
    // itself.
    struct LuaStage {
        std::string name;
        std::string owner;
        script_vm  *vm  = nullptr;
        int         ref = 0;
    };
    std::vector<LuaStage>          stages;
    BiomeSet                       biomes;
    std::unique_ptr<MapgenContext> ctx;
    bool                           finished   = false;
    bool                           generating = false;

    // The mods as they stood before shutdown, which empties the list. Taken
    // rather than asked for afterwards: whether a mod is still running and what
    // it was blamed for is the answer at the END of a run, and the only place
    // it exists is inside it.
    std::vector<Mod> mods_at_end;
};

void h_set_block(void *ud, int x, int y, int z, int id)
{
    Fixture *f = static_cast<Fixture *>(ud);
    // Kept to the column a stage is generating, as the engine keeps it.
    if (f->generating && f->ctx && !f->ctx->owns(x, z)) return;
    f->world.set_block(x, y, z, (BlockId)id);
}

int h_get_block(void *ud, int x, int y, int z)
{
    return (int)static_cast<Fixture *>(ud)->world.get_block(x, y, z);
}

// Which way a cell is turned, with the same refusal the engine's two hosts
// apply: a byte the block does not allow is not written. Without these the
// fixture had no set_facing at all, so every cell a mod turned came out
// unturned -- and the gate below could not have seen the difference.
void h_set_facing(void *ud, int x, int y, int z, int facing)
{
    Fixture *f = static_cast<Fixture *>(ud);
    if (facing < 0) return;
    if (!node_turn_allows(f->registry.get(f->world.get_block(x, y, z)).turn, (unsigned)facing))
        return;
    f->world.set_facing(x, y, z, (uint8_t)facing);
}

int h_get_facing(void *ud, int x, int y, int z)
{
    return (int)static_cast<Fixture *>(ud)->world.get_facing(x, y, z);
}

void h_set_portal(void *ud, int i, float x, float y, float z) {}
void h_log(void *ud, const char *message) {}

int h_register_node(void *ud, const char *name, int r, int g, int b, int solid, int light,
                    const char *turn, int dig_level, double hardness, const char *material)
{
    Fixture *f = static_cast<Fixture *>(ud);
    NodeDef d;
    d.name = name ? name : "unnamed";
    if (const ModManifest *m = mod_host_current()) d.owner = m->name;
    d.colour[0] = (uint8_t)r; d.colour[1] = (uint8_t)g; d.colour[2] = (uint8_t)b;
    d.solid = solid != 0;
    d.opaque = solid != 0;
    d.light_source = (uint8_t)light;
    d.turn = node_turn_from_name(turn);
    d.dig_level =  (uint8_t)(dig_level < 0 ? 0 : (dig_level > 255 ? 255 : dig_level));
    d.hardness  =  (float)(hardness < 0.0 ? 0.0 : hardness);
    d.material  =  material ? material : "";
    return (int)f->registry.register_node(d);
}

int h_register_mapgen(void *ud, const char *name, int handler_ref, char *err, size_t err_size)
{
    Fixture          *f     = static_cast<Fixture *>(ud);
    const std::string stage = name ? name : "";

    if (stage.empty()) {
        std::snprintf(err, err_size, "a mapgen stage needs a name");
        return 0;
    }
    if (f->finished) {
        std::snprintf(err, err_size,
                      "'%s' asked for a mapgen stage after the world was generated", stage.c_str());
        return 0;
    }
    for (const Fixture::LuaStage &st : f->stages) {
        if (st.name == stage) {
            std::snprintf(err, err_size, "a mapgen stage called '%s' is already registered",
                          stage.c_str());
            return 0;
        }
    }

    Fixture::LuaStage st;
    st.name  = stage;
    st.owner = mod_host_current() ? mod_host_current()->name : "";
    st.vm    = mod_host_current_vm();
    st.ref   = handler_ref;
    f->stages.push_back(st);
    return 1;
}

int h_terrain_height(void *ud, int x, int z)
{
    Fixture *f = static_cast<Fixture *>(ud);
    if (!f->ctx) return 0;
    return mapgen_terrain_height(*f->ctx, x, z);
}

int h_node_liquid(void *ud, int id, int opacity, int fill)
{
    Fixture *f = static_cast<Fixture *>(ud);
    if (id <= 0 || (size_t)id >= f->registry.size()) return 0;
    NodeDef d = f->registry.get((BlockId)id);
    d.solid   = false;
    d.opaque  = false;
    d.liquid  = true;
    d.opacity = (uint8_t)(opacity < 0 ? 0 : (opacity > 254 ? 254 : opacity));
    d.liquid_fill = (uint8_t)(fill < 1 ? 1 : (fill > 8 ? 8 : fill));
    f->registry.register_node(d);
    return 1;
}

const char *h_biome(void *ud, int x, int z)
{
    Fixture *f = static_cast<Fixture *>(ud);
    if (!f->ctx || !f->ctx->biomes) return "";
    return f->ctx->biomes->at(mapgen_biome_at(*f->ctx->biomes, f->ctx->seed, x, z)).name.c_str();
}

int h_node_id(void *ud, const char *name)
{
    return (int)static_cast<Fixture *>(ud)->registry.find(name ? name : "");
}

const char *h_node_name(void *ud, int id)
{
    Fixture *f = static_cast<Fixture *>(ud);
    if (id < 0 || (size_t)id >= f->registry.size()) return "";
    return f->registry.get((BlockId)id).name.c_str();
}

int h_event_define(void *ud, const char *event, const script_field *fields, int count,
                   int cancellable, char *err, size_t err_size)
{
    Fixture *f = static_cast<Fixture *>(ud);
    EventSchema schema;
    schema.name        = event ? event : "";
    schema.owner       = mod_host_current() ? mod_host_current()->name : "core";
    schema.cancellable = cancellable != 0;
    for (int i = 0; i < count; ++i)
        schema.fields.push_back(EventField{fields[i].name, fields[i].type});

    std::string e;
    if (!f->mods.events().define(schema, e)) {
        std::snprintf(err, err_size, "%s", e.c_str());
        return 0;
    }
    return 1;
}

int h_event_on(void *ud, const char *event, int priority, int ref, char *err, size_t err_size)
{
    Fixture *f = static_cast<Fixture *>(ud);
    const std::string owner = mod_host_current() ? mod_host_current()->name : "core";
    std::string e;
    if (!f->mods.events().on(event ? event : "", priority, mod_host_current_vm(), ref, owner, e)) {
        std::snprintf(err, err_size, "%s", e.c_str());
        return 0;
    }
    return 1;
}

int h_event_emit(void *ud, const char *event, script_field *fields, int count,
                 int *out_cancelled, char *err, size_t err_size)
{
    Fixture *f = static_cast<Fixture *>(ud);
    bool        cancelled = false;
    std::string e;
    if (!f->mods.events().emit(event ? event : "", fields, count, &cancelled, e)) {
        std::snprintf(err, err_size, "%s", e.c_str());
        return 0;
    }
    if (out_cancelled) *out_cancelled = cancelled ? 1 : 0;
    return 1;
}

// Generates a world into `f`, optionally runs the mods for `ticks`, and hashes
// it. The fixture is the caller's so that a check can look at the WORLD
// afterwards and not only at the number.
uint64_t build(Fixture &f, uint32_t seed, int extent, int ticks, const char *mods_dir)
{
    register_core_nodes(f.registry);

    Mapgen mapgen;
    mapgen.generate(f.world, f.registry, seed, extent, kWorldHeight);

    // Kept, because a mod's stage runs after this Mapgen has gone and
    // core.terrain_height answers out of it. The biomes are copied for the same
    // reason the engine copies them.
    f.biomes = mapgen.biomes();
    f.ctx.reset(new MapgenContext(
        mapgen_make_context(f.world, f.registry, f.biomes, seed, kWorldHeight)));

    if (mods_dir && ticks > 0) {
        script_host host{};
        host.set_block     = h_set_block;
        host.set_facing    = h_set_facing;
        host.get_facing    = h_get_facing;
        host.get_block     = h_get_block;
        host.set_portal    = h_set_portal;
        host.log           = h_log;
        host.register_node = h_register_node;
        host.register_mapgen = h_register_mapgen;
        host.terrain_height  = h_terrain_height;
        host.biome           = h_biome;
        host.node_liquid     = h_node_liquid;
        host.node_id       = h_node_id;
        host.node_name     = h_node_name;
        host.event_define  = h_event_define;
        host.event_on      = h_event_on;
        host.event_emit    = h_event_emit;
        host.ud            = &f;

        f.mods.set_host(host);
        // As the server does: a VM that goes leaves its stages dead, or the
        // next VM given its address would run them.
        f.mods.set_vm_gone([&f](script_vm *vm) {
            for (Fixture::LuaStage &st : f.stages)
                if (st.vm == vm) st.vm = nullptr;
        });

        const std::vector<EventField> block_fields = {
            {"x", SCRIPT_V_NUMBER}, {"y", SCRIPT_V_NUMBER}, {"z", SCRIPT_V_NUMBER},
            {"id", SCRIPT_V_NUMBER},
        };
        std::string err;
        for (const char *name : {"core:pre_place", "core:placed", "core:pre_break", "core:broken"}) {
            EventSchema schema;
            schema.name   = name;
            schema.owner  = "core";
            schema.fields = block_fields;
            schema.cancellable = true;
            f.mods.events().define(schema, err);
        }
        EventSchema tick;
        tick.name   = "core:tick";
        tick.owner  = "core";
        tick.fields = {{"dt", SCRIPT_V_NUMBER}};
        f.mods.events().define(tick, err);

        f.mods.load_dir(mods_dir);

        // The mods' stages, in the order they asked, after the whole directory
        // has loaded. The engine does this in two places and says why at length
        // in src/server/main.cpp; here it is the same three lines so that what
        // a mod sees is the same.
        //
        // A column at a time, which is how the engine runs them now that the
        // world has no edge; a stage whose mod an earlier column stopped does
        // not run again.
        const int columns = columns_across(extent);
        for (int cz = 0; cz < columns; ++cz) {
            for (int cx = 0; cx < columns; ++cx) {
                mapgen_set_column(*f.ctx, ColumnPos{cx, cz});
                for (const Fixture::LuaStage &st : f.stages) {
                    bool running = false;
                    for (const Mod &m : f.mods.mods())
                        if (st.vm && m.vm == st.vm) running = m.running;
                    if (!running) continue;

                    script_field fields[6] = {
                        {"seed",   SCRIPT_V_NUMBER, (double)seed,         nullptr, 0},
                        {"height", SCRIPT_V_NUMBER, (double)kWorldHeight, nullptr, 0},
                        {"min_x",  SCRIPT_V_NUMBER, (double)f.ctx->min_x, nullptr, 0},
                        {"min_z",  SCRIPT_V_NUMBER, (double)f.ctx->min_z, nullptr, 0},
                        {"max_x",  SCRIPT_V_NUMBER, (double)f.ctx->max_x, nullptr, 0},
                        {"max_z",  SCRIPT_V_NUMBER, (double)f.ctx->max_z, nullptr, 0},
                    };
                    char stage_err[256] = {0};
                    f.generating = true;
                    f.mods.enter_vm(st.vm);
                    const bool ok =
                        script_call_handler(st.vm, st.ref, fields, 6, 0, nullptr, stage_err,
                                            sizeof(stage_err));
                    f.mods.leave_vm(st.vm);
                    f.generating = false;
                    if (!ok)
                        f.mods.disable_vm(st.vm, "mapgen stage '" + st.name + "': " + stage_err);
                }
            }
        }
        f.finished = true;

        // Fixed step. A hash after a thousand ticks means nothing if the ticks
        // were different lengths on the two machines being compared.
        constexpr double kTick = 1.0 / 20.0;
        for (int i = 0; i < ticks; ++i) {
            f.mods.step(kTick);
            script_field dt_field{"dt", SCRIPT_V_NUMBER, kTick, nullptr, 0};
            bool cancelled = false;
            f.mods.events().emit("core:tick", &dt_field, 1, &cancelled, err);
        }
        f.mods_at_end = f.mods.mods();
        f.mods.shutdown();
    }

    return world_hash(f.world, extent, kWorldHeight);
}

uint64_t run(uint32_t seed, int extent, int ticks, const char *mods_dir)
{
    Fixture f;
    return build(f, seed, extent, ticks, mods_dir);
}

} // namespace

int main(int argc, char **argv)
{
    const char *mods_dir    = (argc > 1) ? argv[1] : nullptr;
    const char *mapgen_mods = (argc > 2) ? argv[2] : nullptr;

    std::printf("mapgen\n");

    const uint64_t a = run(1, 64, 0, nullptr);
    const uint64_t b = run(1, 64, 0, nullptr);
    check(a == b, "the same seed generates the same world");

    const uint64_t c = run(2, 64, 0, nullptr);
    check(a != c, "a different seed generates a different world");

    // Pinned. Not because this number means anything, but because a change to
    // the generator that nobody intended shows up here instead of in a report
    // about two players seeing different ground. Change the generator on
    // purpose, update the constant in the same commit, and say so.
    constexpr uint64_t kSeed1Extent64 = 0x3ede900b4ca1d723ull;   // 2026-10-10: seas, rivers, wide biomes
    check(a == kSeed1Extent64, "seed 1 still generates the world it generated yesterday");
    if (a != kSeed1Extent64)
        std::printf("        got %016llx, pinned %016llx\n",
                    (unsigned long long)a, (unsigned long long)kSeed1Extent64);

    // Stage order is part of the contract: a generator with an extra stage is a
    // different generator, and it has to be visible.
    {
        World w1, w2;
        NodeRegistry r1, r2;
        register_core_nodes(r1);
        register_core_nodes(r2);

        Mapgen plain;
        plain.generate(w1, r1, 1, 64, kWorldHeight);

        Mapgen extra;
        extra.add_stage("test:one_block", [](MapgenContext &ctx) {
            // Whatever is there, something else. Setting a fixed id would make
            // this check depend on the terrain not having reached y=40 at that
            // column, which is a property of today's biomes rather than of the
            // thing being checked -- and it did reach it, the first time a
            // sabotage made the mountains taller.
            // Only from the column it is in: every column runs the stage, and
            // four toggles of one cell put it back as it was.
            if (ctx.min_x > 5 || ctx.max_x < 5 || ctx.min_z > 5 || ctx.max_z < 5) return;
            const BlockId there = ctx.world.get_block(5, 40, 5);
            ctx.world.set_block(5, 40, 5, (there == ctx.stone) ? ctx.bedrock : ctx.stone);
        });
        extra.generate(w2, r2, 1, 64, kWorldHeight);

        check(plain.stages().size() == 1, "the core registers one stage");
        check(extra.stages().size() == 2, "and a registered stage is added to it");
        check(world_hash(w1, 64, kWorldHeight) != world_hash(w2, 64, kWorldHeight),
              "one extra block from one extra stage changes the hash");
    }


    // TODO OPEN-35, the half M3 asked for and never built: a stage a MOD
    // registers. The three fixtures in tests/mods_mapgen are one mod per thing
    // that can go wrong -- a stage that works, a stage that asks too late, and
    // a stage that throws.
    if (mapgen_mods) {
        std::printf("\nmapgen stages from a mod\n");

        // The same world, generated without any of them, and the context that
        // says where the ground is. Both are what the mod's answers are checked
        // AGAINST: a gate that asked the mod where it put things would agree
        // with itself whatever the mod did.
        World        plain;
        NodeRegistry plain_reg;
        register_core_nodes(plain_reg);
        Mapgen plain_gen;
        plain_gen.generate(plain, plain_reg, 1, 128, kWorldHeight);
        BiomeSet      plain_biomes = plain_gen.biomes();
        MapgenContext plain_ctx =
            mapgen_make_context(plain, plain_reg, plain_biomes, 1, kWorldHeight);

        constexpr int kSiteX = 10, kSiteZ = 10;
        const int     ground = mapgen_terrain_height(plain_ctx, kSiteX, kSiteZ);

        Fixture        f;
        const uint64_t with = build(f, 1, 128, 2, mapgen_mods);

        const BlockId mark    = f.registry.find("spire:mark");
        const BlockId crown   = f.registry.find("spire:crown");
        const BlockId half    = f.registry.find("boom:half");
        const BlockId refused = f.registry.find("late:refused");
        const BlockId taken   = f.registry.find("late:taken");

        check(f.stages.size() == 4,
              "four stages are registered: one from boom, one from late, two from spire");
        // Weaker than it looks, and said so rather than left to be discovered:
        // these mods also register blocks and write one cell from a tick, so
        // this stays green with the stage pass turned off entirely. What
        // catches that is every check below it.
        check(with != world_hash(plain, 128, kWorldHeight),
              "a world with these mods in it is not the world without them");

        // The contract, and the one thing a list of stages is FOR: the order
        // they were asked for. Both of spire's stages write this cell; the one
        // registered second is the one that is there.
        check(f.world.get_block(7, 40, 7) == crown && crown != kAir,
              "the stage registered second wrote over the one registered first");

        // The generator's own ground, reached from Lua. Four blocks from
        // core.terrain_height's answer upwards, checked against the height this
        // gate derived in C++ from the same context.
        int spire_blocks = 0;
        for (int h = 0; h <= 3; ++h)
            if (f.world.get_block(kSiteX, ground + h, kSiteZ) == mark) ++spire_blocks;
        check(spire_blocks == 4 && mark != kAir,
              "a stage stands four blocks on the ground core.terrain_height gave it");

        // And the seed reaches the stage. The block is placed at the ground
        // plus seed % 5, so a stage handed 0 instead of 1 would put it one
        // lower -- which the check below is the only thing that would notice.
        check(f.world.get_block(kSiteX + 2, ground + 1, kSiteZ + 2) == mark,
              "and the seed it was handed is the seed the world was made from");

        // A stage sees what the ones before it wrote: spire:caps finds the top
        // of what spire:columns built by reading the world, and caps it.
        int top = kWorldHeight - 1;
        while (top > 0 && f.world.get_block(kSiteX, top, kSiteZ) == kAir) --top;
        check(f.world.get_block(kSiteX, top, kSiteZ) == crown,
              "the stage after it put its cap on top of what it found there");

        // Asking too late. `late` registers one stage while it loads and asks
        // for a second from on_step; the second is refused, and the mod says
        // which happened by putting one of two blocks in the world.
        check(f.world.get_block(3, 50, 3) == refused && refused != kAir && refused != taken,
              "a mod that asks for a stage after generation is refused, and told");

        // And a name that is already taken. Same mod, same two blocks, asked
        // while it was still loading -- so this is about the NAME and not about
        // being late.
        check(f.world.get_block(3, 52, 3) == refused,
              "and a stage whose name another stage already has is refused too");

        // What a stage that throws costs. Everything it wrote before it threw
        // is in the world, its mod is stopped, and every stage after it ran --
        // which is what the checks above are, since boom loads first.
        check(f.world.get_block(5, 45, 5) == half && half != kAir,
              "a stage that errored kept the blocks it had already written");
        bool boom_running = true, boom_blamed = false;
        for (const Mod &m : f.mods_at_end) {
            if (m.manifest.name != "boom") continue;
            boom_running = m.running;
            boom_blamed  = m.error.find("mapgen stage") != std::string::npos;
        }
        check(!boom_running && boom_blamed,
              "and cost its own mod, which is told it was the stage");

        // Determinism, which is the rule a stage has to keep and the reason
        // this gate is in this file at all.
        check(run(1, 128, 2, mapgen_mods) == with,
              "two runs of the same mods over the same seed make the same world");
        check(run(2, 128, 2, mapgen_mods) != with,
              "and a different seed makes a different one");
    }

    std::printf("\nbiomi\n");
    {
        // A biome is a climate. These checks are about the two halves of that:
        // that every climate belongs to somebody, and that a border between two
        // of them is a border in the BLOCKS and not a step in the GROUND.
        const BiomeSet set;

        // Every biome owns some climate. A biome placed inside another's
        // territory would be registered, listed, and never appear in a world --
        // and nothing else in here would notice.
        std::vector<int> owned(set.size(), 0);
        for (int ti = 0; ti <= 100; ++ti) {
            for (int hi = 0; hi <= 100; ++hi)
                owned[set.pick((float)ti / 100.0f, (float)hi / 100.0f)]++;
        }
        bool all_owned = true, none_by_water = true;
        int  climates = 0, waters = 0;
        for (size_t i = 0; i < set.size(); ++i) {
            if (set.at(i).climate) {
                ++climates;
                if (owned[i] == 0) all_owned = false;
            } else {
                ++waters;
                if (owned[i] != 0) none_by_water = false;
            }
        }
        check(climates == 4 && waters == 6, "the engine registers four climates and six waters, shores and gorges");
        check(all_owned, "and every climate owns a part of climate space");
        check(none_by_water, "and no water or shore is picked by climate");
        for (size_t i = 0; i < set.size(); ++i)
            if (set.at(i).climate)
                std::printf("        %-14s %4.1f%% of climates\n", set.at(i).name.c_str(),
                            100.0 * owned[i] / (101.0 * 101.0));

        // Two biomes on the same point. Which one wins has to be decided by
        // something both machines can compute -- so, the name. Decided by the
        // order they were registered in, the same seed would grow two different
        // worlds on two builds that loaded their mods in a different order.
        BiomeDef first; first.name = "test:aaa"; first.temperature = 0.02f; first.humidity = 0.02f;
        BiomeDef last;  last.name  = "test:zzz"; last.temperature  = 0.02f; last.humidity  = 0.02f;

        BiomeSet forward;  forward.add(first); forward.add(last);
        BiomeSet backward; backward.add(last); backward.add(first);
        check(forward.at(forward.pick(0.02f, 0.02f)).name == "test:aaa" &&
                  backward.at(backward.pick(0.02f, 0.02f)).name == "test:aaa",
              "two biomes on the same climate are tied by name, not by who was added first");

        // Owning a corner of climate space is not the same as being somewhere a
        // player can walk to. Value noise crowds the middle of its range, so
        // without the contrast stretch in biome.cpp the hot dry corner is
        // reached rarely enough that a desert becomes a curiosity.
        //
        // Six worlds pooled, not one: which biome is rare in a single world is
        // the seed's business -- seed 6 gives four percent desert and seed 2
        // gives twenty-eight -- and a gate on one seed would be measuring that
        // instead of the generator. The plaza is left out because it is forced
        // to plains and it is two fifths of a world this size.
        //
        // Since 2026-10-10 a biome is some two hundred blocks across, so the
        // sample is 2048 blocks round the spawn, one column in sixteen.
        constexpr int kSpan = 2048, kStep = 16;
        std::vector<int> pooled(set.size(), 0);
        int              wild = 0;
        for (uint32_t seed = 1; seed <= 6; ++seed) {
            for (int z = 60 - kSpan / 2; z < 60 + kSpan / 2; z += kStep) {
                for (int x = 60 - kSpan / 2; x < 60 + kSpan / 2; x += kStep) {
                    if (x >= 20 && x <= 100 && z >= 20 && z <= 100) continue;
                    pooled[mapgen_biome_at(set, seed, x, z)]++;
                    ++wild;
                }
            }
        }
        double rarest = 100.0, rarest_water = 100.0;
        for (size_t i = 0; i < set.size(); ++i) {
            const double pc = 100.0 * pooled[i] / (double)wild;
            std::printf("        %-14s %4.1f%% of the ground of six worlds\n",
                        set.at(i).name.c_str(), pc);
            if (set.at(i).climate && pc < rarest) rarest = pc;
            if (!set.at(i).climate && pc < rarest_water) rarest_water = pc;
        }
        check(rarest >= 5.0, "and no climate is rarer than a twentieth of the ground");
        check(rarest_water >= 0.5, "and every water and shore is somewhere in it");
    }

    {
        // The world itself, at the size the client and the server actually
        // generate.
        constexpr int kExtent = 128;
        World        world;
        NodeRegistry reg;
        const CoreNodes ids = register_core_nodes(reg);
        Mapgen mapgen;
        mapgen.generate(world, reg, 1, kExtent, kWorldHeight);

        std::vector<int>     top_y(kExtent * kExtent, 0);
        std::vector<BlockId> top_id(kExtent * kExtent, kAir);
        std::map<BlockId, int> surface;
        for (int z = 0; z < kExtent; ++z) {
            for (int x = 0; x < kExtent; ++x) {
                int y = kWorldHeight - 1;
                while (y > 0 && world.get_block(x, y, z) == kAir) --y;
                top_y[z * kExtent + x]  = y;
                top_id[z * kExtent + x] = world.get_block(x, y, z);
                surface[world.get_block(x, y, z)]++;
            }
        }

        // Every biome reaches the surface. Value noise crowds the middle of its
        // range, and without the contrast stretch in biome.cpp the two extreme
        // climates are rare enough that a whole world can come out with no
        // desert in it.
        //
        // A biome is two hundred blocks across now, so a 128 world is mostly
        // one or two of them: each climate is looked for within 1024 of the
        // spawn and its column generated, and its surface must be its own.
        {
            const BiomeSet &bs = mapgen.biomes();
            const char *names[4] = {"core:plains", "core:desert", "core:tundra", "core:peaks"};
            const BlockId tops[4] = {ids.grass, ids.sand, ids.snow, ids.stone};
            int reached = 0;
            for (int k = 0; k < 4; ++k) {
                const size_t want = bs.find(names[k]);
                bool found = false;
                for (int z = -960; z < 1088 && !found; z += 32) {
                    for (int x = -960; x < 1088 && !found; x += 32) {
                        if (mapgen_biome_at(bs, 1, x + 16, z + 16) != want) continue;
                        World col;
                        mapgen.generate_column(col, reg, 1, column_of_cell(x + 16, z + 16), kWorldHeight);
                        int y = kWorldHeight - 1;
                        while (y > 0 && col.get_block(x + 16, y, z + 16) == kAir) --y;
                        found = col.get_block(x + 16, y, z + 16) == tops[k];
                    }
                }
                if (found) ++reached;
                std::printf("        %-12s %s\n", names[k], found ? "on the surface" : "not found");
            }
            check(reached == 4, "all four climates reach the surface within 1024 of the spawn");
        }

        // The point of blending the height parameters instead of picking them.
        // A biome border is allowed to be visible; it is not allowed to be a
        // cliff that the terrain did not put there.
        int max_step = 0;
        for (int z = 0; z < kExtent; ++z) {
            for (int x = 0; x < kExtent; ++x) {
                const int h = top_y[z * kExtent + x];
                if (x + 1 < kExtent) {
                    const int d = h - top_y[z * kExtent + x + 1];
                    max_step = (d > max_step) ? d : (-d > max_step ? -d : max_step);
                }
                if (z + 1 < kExtent) {
                    const int d = h - top_y[(z + 1) * kExtent + x];
                    max_step = (d > max_step) ? d : (-d > max_step ? -d : max_step);
                }
            }
        }
        check(max_step <= 8, "no two neighbouring columns differ by more than eight blocks");
        std::printf("        biggest step between neighbours: %d blocks\n", max_step);

        // The scene the portal gates stand on does not move because this seed
        // rolled a desert. One check rather than two: the ground of the plaza
        // is grass at exactly y=kGroundTop-1, so a plaza that rose would have
        // filler there and one that sank would have air. Asking about the
        // highest block instead would be asking about the pillars, which stand
        // on the plaza and are supposed to.
        int plaza_wrong = 0;
        for (int z = 20; z <= 100; ++z)
            for (int x = 20; x <= 100; ++x)
                if (world.get_block(x, kGroundTop - 1, z) != ids.grass) ++plaza_wrong;
        check(plaza_wrong == 0,
              "the plaza is still flat grass at the height it was, whatever the climate says");

        // What a column is made of, under the block you can see. Plains,
        // because its surface block and its filler are different ones -- in the
        // desert both are sand and an off-by-one would be invisible.
        const BiomeSet &set    = mapgen.biomes();
        const size_t    plains = set.find("core:plains");
        int  checked_column = -1;
        bool filler_right   = false;
        for (int z = 0; z < kExtent && checked_column < 0; ++z) {
            for (int x = 0; x < kExtent; ++x) {
                if (x >= 20 && x <= 100 && z >= 20 && z <= 100) continue;   // the plaza
                if (mapgen_biome_at(set, 1, x, z) != plains) continue;
                const int y = top_y[z * kExtent + x];
                if (y < 10) continue;

                int dirt = 0;
                while (world.get_block(x, y - 1 - dirt, z) == ids.dirt) ++dirt;
                filler_right = world.get_block(x, y, z) == ids.grass && dirt == 3 &&
                               world.get_block(x, y - 4, z) == ids.stone;
                checked_column = z * kExtent + x;
                break;
            }
        }
        check(checked_column >= 0 && filler_right,
              "a plains column is grass, then exactly three dirt, then stone");

        // The pillars stage builds out of the biome it is standing in. A stone
        // column in the middle of a desert is a column the terrain did not put
        // there.
        int  desert_columns = 0, desert_stone = 0;
        const size_t desert = set.find("core:desert");
        for (int z = -960; z < 1088 && desert_columns <= 100; z += 32) {
            for (int x = -960; x < 1088 && desert_columns <= 100; x += 32) {
                if (mapgen_biome_at(set, 1, x + 16, z + 16) != desert) continue;
                World col;
                const ColumnPos cp = column_of_cell(x + 16, z + 16);
                mapgen.generate_column(col, reg, 1, cp, kWorldHeight);
                for (int cz = cp.z * kChunkSize; cz < (cp.z + 1) * kChunkSize; ++cz) {
                    for (int cx = cp.x * kChunkSize; cx < (cp.x + 1) * kChunkSize; ++cx) {
                        if (mapgen_biome_at(set, 1, cx, cz) != desert) continue;
                        int y = kWorldHeight - 1;
                        while (y > 0 && col.get_block(cx, y, cz) == kAir) --y;
                        ++desert_columns;
                        if (col.get_block(cx, y, cz) == ids.stone) ++desert_stone;
                    }
                }
            }
        }
        check(desert_columns > 100, "the world has a desert in it to check");
        check(desert_stone == 0, "and nothing standing on it is made of stone");
        std::printf("        %d desert columns, %d of them topped with stone\n",
                    desert_columns, desert_stone);
    }

    std::printf("\nthe block table\n");
    {
        // Ids come from registration order, and the world hash is computed over
        // ids. So this order is a contract between every process that has a
        // registry -- the client, the server, and whatever joins them.
        //
        // If this section goes red because you added a core node: the id table
        // changed. Every side has to change together, and until the welcome
        // carries a name-to-id table (which chunk streaming needs anyway), a
        // client built before your change and a server built after it will
        // disagree about what block 3 is. Update the pin below in the same
        // commit and say so.
        NodeRegistry reg;
        const CoreNodes ids = register_core_nodes(reg);

        check(ids.stone == 1 && ids.grass == 2 && ids.bedrock == 3 && ids.frame == 4,
              "the engine's blocks get ids 1 to 4, in that order");
        check(ids.glass == 5 && ids.dirt == 6 && ids.sand == 7 && ids.snow == 8,
              "and the biome blocks got the ids after them, not ids in the middle");
        check(reg.get(ids.bedrock).name == "core:bedrock" && !reg.get(ids.bedrock).breakable,
              "block 3 is the floor of the world, and it does not break");

        // The published list and the function that registers them, checked
        // against each other rather than kept in step by hand.
        const std::vector<std::string> &names = core_node_names();
        bool listed_in_order = names.size() == reg.size() - 1;   // -1 for air
        for (size_t i = 0; i < names.size() && listed_in_order; ++i)
            listed_in_order = reg.get((BlockId)(i + 1)).name == names[i];
        check(listed_in_order, "and core_node_names() lists exactly those, in that order");

        // The hazard itself, run rather than described: the same world, the
        // same seed, registered in a different order, hashes differently.
        NodeRegistry shuffled;
        NodeDef grass;   grass.name   = "core:grass";   shuffled.register_node(grass);
        NodeDef stone;   stone.name   = "core:stone";   shuffled.register_node(stone);
        NodeDef bedrock; bedrock.name = "core:bedrock"; shuffled.register_node(bedrock);
        NodeDef frame;   frame.name   = "core:frame";   shuffled.register_node(frame);

        World normal_world, shuffled_world;
        NodeRegistry normal;
        register_core_nodes(normal);

        Mapgen mapgen;
        mapgen.generate(normal_world, normal, 1, 64, kWorldHeight);
        mapgen.generate(shuffled_world, shuffled, 1, 64, kWorldHeight);

        check(world_hash(normal_world, 64, kWorldHeight) !=
                  world_hash(shuffled_world, 64, kWorldHeight),
              "two sides that register the same blocks in a different order disagree");
        std::printf("        which is why there is one list and not one per binary\n");

        // Adoption: a table from somewhere else, reproduced exactly, with the
        // names this build does not know standing in as placeholders.
        NodeRegistry server_side;
        register_core_nodes(server_side);
        NodeDef mod_block;
        mod_block.name = "somemod:strange";
        server_side.register_node(mod_block);

        NodeRegistry adopted;
        const size_t unknown = adopt_node_table(adopted, node_table(server_side), reg);

        check(adopted.size() == server_side.size(),
              "an adopted table has as many blocks as the one it came from");
        check(adopted.find("core:bedrock") == server_side.find("core:bedrock"),
              "and the same ids, which is the only reason to adopt one");
        check(unknown == 1 && adopted.find("somemod:strange") != kAir,
              "a block this build has never heard of still gets its id");
        check(registry_content_hash(adopted) == registry_content_hash(server_side),
              "so the two registries agree about their content");
        check(!adopted.get(adopted.find("somemod:strange")).owner.empty(),
              "and is marked as one this build does not understand");
    }

    if (mods_dir) {
        std::printf("\nthousand ticks with the mods loaded\n");
        const uint64_t t1 = run(1, 64, 1000, mods_dir);
        const uint64_t t2 = run(1, 64, 1000, mods_dir);
        check(t1 == t2, "two runs of a thousand ticks leave the same world");
        check(t1 != a, "and the mods did change it, so the check has something to compare");
        std::printf("        world hash after 1000 ticks: %016llx\n", (unsigned long long)t1);
    }

    if (mods_dir) {
        std::printf("\nwhat the mods that ship bury in the stone\n");

        // TODO OPEN-35: core.register_mapgen landed with nothing that ships
        // using it, and a mechanism nothing runs is a mechanism nobody has run.
        // mods/ore is the first, and it is checked HERE against the WORLD --
        // the count that mod prints is the mod's own account of itself, and a
        // gate that asked it what it did would agree with it whatever it did.
        Fixture f;
        build(f, 1, 64, 1, mods_dir);

        // Emerald last and not required: it is only under the peaks, and a
        // 64-square world may have none.
        const char *names[] = {"ore:coal_ore", "ore:copper_ore", "ore:iron",
                               "ore:lapis_ore", "ore:gold", "ore:cinnabar_ore",
                               "ore:diamond_ore", "ore:emerald_ore"};
        constexpr int kOres = 8;
        BlockId ids[kOres];
        bool    registered = true;
        for (int i = 0; i < kOres; ++i) registered &= (ids[i] = f.registry.find(names[i])) != kAir;
        check(registered, "the mod that ships a stage registered its eight ores");

        // Counted, and where each cell sits counted with it. Buried is the
        // whole claim: ore lying on the surface is ore nobody has to dig for,
        // and it is also what a stage that ignored core.terrain_height would
        // produce.
        size_t cells[kOres] = {}, exposed = 0, highest_diamond = 0;
        for (int z = 0; z < 64; ++z) {
            for (int x = 0; x < 64; ++x) {
                int top = kWorldHeight - 1;
                while (top > 0 && f.world.get_block(x, top, z) == kAir) --top;

                for (int y = 1; y <= top; ++y) {
                    const BlockId id = f.world.get_block(x, y, z);
                    for (int i = 0; i < kOres; ++i) {
                        if (id != ids[i]) continue;
                        ++cells[i];
                        if (i == 6 && (size_t)y > highest_diamond) highest_diamond = (size_t)y;
                        if (y == top) ++exposed;
                    }
                }
            }
        }

        bool all = true;
        for (int i = 0; i < kOres - 1; ++i) all &= cells[i] > 0;
        check(all, "a stage a mod registered put every ore in the stone");
        check(exposed == 0, "and none of it was left lying on the surface");
        check(highest_diamond <= 10, "and diamond only near the floor of the world");
        for (int i = 0; i < kOres; ++i) std::printf("        %zu cell(s) of %s\n", cells[i], names[i]);
    }

    if (mods_dir) {
        std::printf("\nand which way the mods that ship lay their blocks down\n");

        // TODO OPEN-27: twenty-four orientations landed with nothing that ships
        // using more than four of them, which is OPEN-24's sentence again.
        // mods/timber is the answer, and it is checked against the WORLD -- the
        // cells and their bytes -- rather than against what the mod says it
        // wrote.
        Fixture f;
        build(f, 1, 64, 1, mods_dir);

        const BlockId log   = f.registry.find("timber:log");
        const BlockId rungs = f.registry.find("timber:rungs");
        check(log != kAir && rungs != kAir, "the mod that ships turns registered its two blocks");
        check(f.registry.get(log).turn == NodeTurn::Facedir &&
                  f.registry.get(rungs).turn == NodeTurn::Wallmounted,
              "one of them turns on any axis and the other mounts on a face");

        // Every orientation the mod wrote, and every one of the six faces,
        // gathered off the cells. Sets and not counts: "five logs are turned"
        // is true of five logs turned the same way, which is the case a byte
        // masked into range would produce.
        std::vector<uint8_t> log_turns, rungs_faces;
        int                  logs = 0, panels = 0, off_axis = 0;
        for (int z = 0; z < 64; ++z) {
            for (int y = 0; y < (int)kWorldHeight; ++y) {
                for (int x = 0; x < 64; ++x) {
                    const BlockId id = f.world.get_block(x, y, z);
                    const uint8_t fc = f.world.get_facing(x, y, z);
                    if (id == log) {
                        ++logs;
                        if (fc >= 4) ++off_axis;
                        if (std::find(log_turns.begin(), log_turns.end(), fc) == log_turns.end())
                            log_turns.push_back(fc);
                    } else if (id == rungs) {
                        ++panels;
                        if (std::find(rungs_faces.begin(), rungs_faces.end(), fc) ==
                            rungs_faces.end())
                            rungs_faces.push_back(fc);
                    }
                }
            }
        }

        check(logs > 0 && panels > 0, "both of them are in the world");
        check(log_turns.size() >= 3 && off_axis > 0,
              "the logs lie on more than one axis, which four quarter turns could not say");
        check(rungs_faces.size() == 6, "and the rungs are mounted on all six faces");
        std::printf("        %d log(s) in %zu orientation(s), %d of them off the vertical; "
                    "%d panel(s) on %zu face(s)\n",
                    logs, log_turns.size(), off_axis, panels, rungs_faces.size());

        // And the one thing a cell may not be: a byte its block does not allow.
        // Asked of the world rather than of the API, because what the world
        // holds is what the mesher will read.
        int wild = 0;
        for (int z = 0; z < 64; ++z) {
            for (int y = 0; y < (int)kWorldHeight; ++y) {
                for (int x = 0; x < 64; ++x) {
                    const BlockId id = f.world.get_block(x, y, z);
                    if (!node_turn_allows(f.registry.get(id).turn,
                                          f.world.get_facing(x, y, z))) ++wild;
                }
            }
        }
        check(wild == 0, "and no cell in the world carries a byte its block does not allow");
    }

    if (mods_dir) {
        std::printf("\nand what they cut out of it\n");

        // Wider than the 64 the checks above use, and the reason is the mod's
        // own rule: one cave per square of 32 columns, so a 64 world holds four
        // of them and four tunnels are four samples. At 128 it is sixteen, and
        // that is the width at which the rule for a tunnel that WANDERED -- the
        // roof held under the ground it is under now, asked again at every step
        // -- starts to matter: with it gone the world differs, and at 64 it did
        // not.
        constexpr int kCaveExtent = 128;

        // The other half of OPEN-35, and the one CLOSED-23 was the condition
        // for: a cave under the open sky is a second piece of air in the same
        // chunk, and a walk that added the two together drew the whole world
        // from inside the cave.
        //
        // Measured against TWO worlds and not against what the mod says it did:
        // the same seed with the core stages alone and with the mods, cell by
        // cell. What turned from stone into air is the cave, whoever cut it.
        Fixture plain_world;
        build(plain_world, 1, kCaveExtent, 0, nullptr);
        Fixture with_mods;
        build(with_mods, 1, kCaveExtent, 1, mods_dir);

        const BlockId plain_stone = plain_world.registry.find("core:stone");
        check(plain_stone != kAir, "there is stone in a world with no mods to cut it");

        size_t cut = 0, floor_cut = 0, opened = 0, foreign = 0, paired = 0;
        int    shallowest = kWorldHeight;

        for (int z = 0; z < kCaveExtent; ++z) {
            for (int x = 0; x < kCaveExtent; ++x) {
                int top = kWorldHeight - 1;
                while (top > 0 && with_mods.world.get_block(x, top, z) == kAir) --top;

                for (int y = 0; y < kWorldHeight; ++y) {
                    if (plain_world.world.get_block(x, y, z) != plain_stone) continue;
                    if (with_mods.world.get_block(x, y, z) != kAir) continue;

                    ++cut;
                    if (y == 0) ++floor_cut;

                    // Air that is ABOVE the highest solid cell of its column is
                    // air with nothing over it: the cave came out into the
                    // open. No threshold and no guess at where the ground was
                    // -- the column says where its own surface is.
                    if (y > top) ++opened;
                    if (top - y < shallowest) shallowest = top - y;

                    const bool air_above =
                        y + 1 < kWorldHeight && with_mods.world.get_block(x, y + 1, z) == kAir;
                    const bool air_below =
                        y > 0 && with_mods.world.get_block(x, y - 1, z) == kAir;
                    if (air_above || air_below) ++paired;
                }

                // And nothing that was NOT stone was cut. The rule that keeps
                // the stage off the surface, the biome's filler and the bedrock
                // is "only a cell that is core:stone", and this is the only
                // thing that would notice it going.
                for (int y = 0; y < kWorldHeight; ++y) {
                    const BlockId was = plain_world.world.get_block(x, y, z);
                    if (was == kAir || was == plain_stone) continue;
                    if (with_mods.world.get_block(x, y, z) == kAir) ++foreign;
                }
            }
        }

        std::printf("        %zu cell(s) of stone became air, the highest %d block(s) under its "
                    "own surface\n",
                    cut, shallowest);

        check(cut > 0, "a stage that ships cut tunnels through the stone");
        check(floor_cut == 0, "and none of them through the floor of the world");
        check(opened == 0, "and none of them out into the open, where a cave is a hole in a field");
        check(foreign == 0, "and nothing that was not stone was cut away");

        // And the passage is two cells tall, which is an exact claim and not a
        // share: the stamp cuts dy 0 and dy 1 together, so EVERY cell it cuts
        // has another cut cell directly above it or directly below it. A
        // passage one cell high has that only where the tunnel stepped up or
        // down -- measured, 1755 of 2952 against 4937 of 4937.
        std::printf("        %zu of the %zu have another cut cell above or below them\n",
                    paired, cut);
        check(cut > 0 && paired == cut,
              "and every one of them is one of a pair, which is a passage two cells tall and "
              "not a crawl one cell high");
    }

    std::printf("\nsomewhere to stand in a world with no openings in it\n");
    {
        // A world started without portals had no spawn at all, and a server
        // with no spawn cannot put a dead body anywhere: kill_body makes it
        // whole and returns, leaving it where it died. A player who walks off
        // the edge of a finite world then falls for ever -- measured at
        // y = -130822 after twenty minutes of the thirty minute run.
        //
        // Across seeds, because whether the middle of the plaza is free depends
        // on where stage_scatter dropped its columns, and one seed that happens
        // to be clear is a spawn nobody has tested.
        bool   all_free = true, all_on_ground = true, all_inside = true;
        double worst_off = 0.0;

        for (uint32_t seed = 1; seed <= 24; ++seed) {
            Fixture f;
            register_core_nodes(f.registry);
            Mapgen mapgen;
            mapgen.generate(f.world, f.registry, seed, 128, kWorldHeight);

            const Vec3 sp = spawn_in_plaza(f.world, f.registry, 128);

            // Not inside a block: a spawn in a column is a player who has to
            // dig their way out of their own respawn.
            if (inside_solid(f.world, f.registry, sp.x, sp.y, sp.z)) all_free = false;

            // And something under them, or the respawn is the fall again.
            const int bx = (int)std::floor(sp.x), bz = (int)std::floor(sp.z);
            const int by = (int)std::floor(sp.y) - 1;
            if (!f.registry.is_solid(f.world.get_block(bx, by, bz))) all_on_ground = false;

            // And in the world it belongs to. The whole failure was a body
            // outside the extent with nothing under it.
            if (sp.x < 0.0f || sp.x > 128.0f || sp.z < 0.0f || sp.z > 128.0f) all_inside = false;

            const double off = std::sqrt((sp.x - 64.5) * (sp.x - 64.5) +
                                         (sp.z - 64.5) * (sp.z - 64.5));
            if (off > worst_off) worst_off = off;
        }

        check(all_free, "the middle of the plaza is somewhere a body can stand, on every seed");
        check(all_on_ground, "with something solid under it, so coming back is not falling again");
        check(all_inside, "and inside the world, which is the whole of what went wrong");
        std::printf("        24 seeds, furthest the point had to step aside: %.1f nodes\n",
                    worst_off);

        // On every one of those seeds the plaza's middle happened to be clear,
        // so nothing above asked the part that steps aside -- and that part is
        // shared with the spawn in front of an opening, which was written
        // because a scatter column stood exactly there on seed 1. So: put a
        // column in the way on purpose.
        {
            Fixture f;
            register_core_nodes(f.registry);
            Mapgen mapgen;
            mapgen.generate(f.world, f.registry, 1, 128, kWorldHeight);

            const Vec3 clear = spawn_in_plaza(f.world, f.registry, 128);
            const BlockId stone = f.registry.find("core:stone");
            for (int y = kGroundTop; y < kGroundTop + 3; ++y)
                f.world.set_block((int)std::floor(clear.x), y, (int)std::floor(clear.z), stone);

            const Vec3 moved = spawn_in_plaza(f.world, f.registry, 128);
            check(!inside_solid(f.world, f.registry, moved.x, moved.y, moved.z),
                  "with something standing in it, the spawn steps aside instead of into the block");
            check(moved.z == clear.z && moved.x != clear.x,
                  "sideways, so it keeps whatever the point was derived from");
            std::printf("        a column in the way moves it %.1f nodes\n",
                        (double)(moved.x - clear.x));
        }
    }

    std::printf("\n%s\n", g_failures == 0 ? "all worldgen checks passed"
                                          : "WORLDGEN CHECKS FAILED");
    return g_failures == 0 ? 0 : 1;
}
