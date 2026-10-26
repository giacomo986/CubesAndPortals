// The world on disk.
//
// The property is short to state and the whole point of the milestone's storage
// half: a world saved and loaded is the world that was saved. Everything else
// here is about what happens when it is not -- a file from a different seed, a
// file from a build with different mods, a row that has rotted.
//
// A stored chunk is the same bytes a streamed chunk is, so the codec's own
// hostile-input gate covers the parsing. What this file adds is the database
// around it and the refusals that belong to a file rather than to a socket.

#include "core/ecs/health.h"
#include "core/item/item_codec.h"
#include "core/net/crypto.h"
#include "core/net/chunk_codec.h"
#include "core/registry/core_nodes.h"
#include "core/storage/world_db.h"
#include "core/world/node_meta.h"
#include "core/voxel/light.h"
#include "core/worldgen/worldgen.h"

#include <sqlite3.h>

#include <chrono>
#include <unistd.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
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

double seconds_since(std::chrono::steady_clock::time_point t0)
{
    return std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
}

std::string temp_path(const char *name)
{
    const char *dir = std::getenv("TMPDIR");
    return std::string(dir ? dir : "/tmp") + "/cnp_" + name + "_" +
           std::to_string((long)::getpid()) + ".db";
}

void remove_db(const std::string &path)
{
    std::remove(path.c_str());
    std::remove((path + "-wal").c_str());
    std::remove((path + "-shm").c_str());
}

} // namespace

int main()
{
    constexpr int      kExtent = 64;
    constexpr uint32_t kSeed   = 1;

    NodeRegistry registry;
    register_core_nodes(registry);

    World world;
    Mapgen mapgen;
    mapgen.generate(world, registry, kSeed, kExtent, kWorldHeight);
    compute_lighting(world, registry, 0, 0, 0, kExtent - 1, kWorldHeight - 1, kExtent - 1);

    // A handful of cells turned, before anything is saved. TODO.md OPEN-27: the
    // facing is a byte per cell, it rides in the same blob as the blocks and
    // the light, and a world reloaded without it is a world whose chests all
    // face the same way again.
    struct Turned { int x, y, z; uint8_t f; };
    static const Turned kTurned[] = {
        {10, kGroundTop, 10, 1}, {11, kGroundTop, 10, 7}, {12, kGroundTop, 10, 23},
        {64, kGroundTop, 64, 3}, {65, kGroundTop, 64, 11},
    };
    for (const Turned &t : kTurned) world.set_facing(t.x, t.y, t.z, t.f);

    const uint64_t original_hash = world_hash(world, kExtent, kWorldHeight);
    const std::string path = temp_path("world");
    remove_db(path);

    std::printf("saving and loading\n");
    {
        WorldDb     db;
        std::string err;
        check(db.open(path, kSeed, registry, err), err.empty() ? "a new world file opens"
                                                              : err.c_str());

        const auto t0 = std::chrono::steady_clock::now();
        check(db.save_all_chunks(world, err), err.empty() ? "and every chunk saves" : err.c_str());
        const double save_seconds = seconds_since(t0);

        const WorldDbStats st = db.stats();
        check(st.chunks == (int64_t)world.chunk_count(),
              "the file holds as many chunks as the world");
        std::printf("        %lld chunks, %.1f KB on disk, saved in %.0f ms (%.2f KB/chunk)\n",
                    (long long)st.chunks, (double)st.bytes_on_disk / 1024.0, save_seconds * 1000.0,
                    (double)st.bytes_on_disk / 1024.0 / (double)(st.chunks ? st.chunks : 1));

        db.close();
    }

    {
        WorldDb     db;
        std::string err;
        check(db.open(path, kSeed, registry, err), "the file opens again");

        World loaded;
        const auto    t0 = std::chrono::steady_clock::now();
        const int64_t n  = db.load_all_chunks(loaded, err);
        const double  load_seconds = seconds_since(t0);

        check(n == (int64_t)world.chunk_count(), "and every chunk comes back");
        check(world_hash(loaded, kExtent, kWorldHeight) == original_hash,
              "the world that comes back is the world that went in");
        std::printf("        loaded in %.0f ms\n", load_seconds * 1000.0);

        // Light too. It rides in the same blob, and a world reloaded without it
        // is a world that is black until something recomputes it.
        bool light_matches = true;
        for (int y = 0; y < kWorldHeight && light_matches; y += 7)
            for (int z = 0; z < kExtent && light_matches; z += 5)
                for (int x = 0; x < kExtent; x += 5)
                    if (loaded.get_light(x, y, z) != world.get_light(x, y, z)) {
                        light_matches = false;
                        break;
                    }
        check(light_matches, "including the light, which would otherwise have to be recomputed");

        // And the facings, which ride in the same blob for the same reason.
        bool facing_matches = true;
        for (const Turned &t : kTurned)
            facing_matches = facing_matches && loaded.get_facing(t.x, t.y, t.z) == t.f;
        check(facing_matches,
              "and which way each cell is turned, which nothing could recompute");

        // And the cells nobody turned are not turned, or the check above would
        // pass over a loader that set every facing to something.
        check(loaded.get_facing(20, kGroundTop, 20) == 0 &&
                  loaded.get_facing(13, kGroundTop, 10) == 0,
              "while a cell nobody turned comes back untouched");

        db.close();
    }

    std::printf("\none block changed\n");
    {
        // The ordinary case in a running server: a player breaks something, one
        // chunk is written, the rest is not touched.
        WorldDb     db;
        std::string err;
        db.open(path, kSeed, registry, err);

        const int bx = 40, by = kGroundTop - 1, bz = 40;
        const BlockId before = world.get_block(bx, by, bz);
        world.set_block(bx, by, bz, kAir);

        const ChunkPos p = World::chunk_of(bx, by, bz);
        check(db.save_chunks(world, {p}, err), "one chunk saves on its own");

        World loaded;
        db.load_all_chunks(loaded, err);
        check(loaded.get_block(bx, by, bz) == kAir, "and the change is in the file");
        check(loaded.get_block(bx + 8, by, bz) == world.get_block(bx + 8, by, bz),
              "with the rest of that chunk still intact");

        world.set_block(bx, by, bz, before);
        db.save_chunks(world, {p}, err);
        db.close();
    }

    std::printf("\nplayers\n");
    {
        WorldDb     db;
        std::string err;
        db.open(path, kSeed, registry, err);

        net::SignKeyPair key, other;
        check(net::SignKeyPair::generate(key) && net::SignKeyPair::generate(other),
              "two keys to keep players under");

        StoredPlayer giacomo;
        std::memcpy(giacomo.identity, key.public_key, net::kSignPublicBytes);
        giacomo.name = "giacomo";
        giacomo.x = 12.5f; giacomo.y = 21.0f; giacomo.z = -33.25f;
        giacomo.last_seen = 1755000000;

        check(db.save_player(giacomo, err), "a player saves");

        StoredPlayer back;
        check(db.load_player(key.public_key, back, err), "and loads");
        check(back.x == giacomo.x && back.z == giacomo.z && back.name == "giacomo",
              "where they left off, negative coordinates and all");

        // Same name, another key: the name is a label and finds nothing.
        check(!db.load_player(other.public_key, back, err),
              "a key that has never played is not there, whatever name it uses");

        // Saving twice updates rather than duplicating: the key is the key.
        giacomo.x = 99.0f;
        db.save_player(giacomo, err);
        db.load_player(key.public_key, back, err);
        check(back.x == 99.0f && db.stats().players == 1,
              "and playing again moves them rather than making a second one");

        db.close();
    }

    std::printf("\na player's bag, keyed by who they are\n");
    {
        // DESIGN.md 4.4: the key is the identity and the name is a label, so a
        // bag keyed by name is a bag anybody takes by typing yours. Everything
        // in this section is that one sentence.
        ItemRegistry items;
        items.adopt_blocks(registry);
        const ItemId stone = items.find("core:stone");
        const ItemId glass = items.find("core:glass");

        WorldDb     db;
        std::string err;
        db.open(path, kSeed, registry, err);

        net::SignKeyPair mine, theirs;
        check(net::SignKeyPair::generate(mine) && net::SignKeyPair::generate(theirs),
              "two players with two keys");

        Inventory bag(Inventory::kDefaultSlots);
        bag.slot(0) = ItemStack{stone, 30};
        bag.slot(9) = ItemStack{glass, 4};
        bag.select(3);

        check(db.save_player_bag(mine.public_key, "giacomo", items, bag, 1755000000, err),
              "a bag saves under a key");

        Inventory back(Inventory::kDefaultSlots);
        check(db.load_player_bag(mine.public_key, items, back, err), "and loads under it");
        check(back.at(0).item == stone && back.at(0).count == 30 && back.at(9).count == 4,
              "holding what it held");
        check(back.selected() == 3, "with the same slot in hand");

        // The one that matters. Same name, another key.
        Inventory stranger(Inventory::kDefaultSlots);
        check(!db.load_player_bag(theirs.public_key, items, stranger, err),
              "and somebody else's key gets nothing, whatever name they play under");
        check(stranger.at(0).empty(), "an empty bag, not a copy of the first one");

        // A key is 32 bytes and one bit of it is the whole difference. Flipping
        // one is not a plausible accident; it is the cheapest way to ask
        // whether the lookup is really on the whole key.
        uint8_t nearly[net::kSignPublicBytes];
        std::memcpy(nearly, mine.public_key, sizeof(nearly));
        nearly[31] ^= 0x01;
        Inventory near_miss(Inventory::kDefaultSlots);
        check(!db.load_player_bag(nearly, items, near_miss, err),
              "a key that differs by one bit is a different player");

        // Playing again moves the bag rather than making a second one.
        bag.slot(0) = ItemStack{stone, 1};
        db.save_player_bag(mine.public_key, "giacomo2", items, bag, 1755000001, err);
        Inventory again(Inventory::kDefaultSlots);
        db.load_player_bag(mine.public_key, items, again, err);
        check(again.at(0).count == 1, "and playing again writes over the last bag");

        // What is worn: only the right piece goes in a worn square, add() never
        // fills one, it counts as armour, and it is saved like the rest.
        ItemDef helm_def;
        helm_def.name      = "test:helmet";
        helm_def.max_stack = 1;
        helm_def.wear      = kWornHead;
        helm_def.armor     = 3;
        const ItemId helm  = items.register_item(helm_def);

        Inventory worn;
        worn.slot(0) = ItemStack{stone, 5};
        worn.slot(1) = ItemStack{helm, 1};
        const int head = Inventory::kDefaultSlots;
        check(!worn.move_to(items, 0, head) && !worn.move_to(items, 1, head + 1),
              "a worn square refuses stone, and a helmet on the feet");
        check(worn.move_to(items, 1, head) && worn.armor(items) == 3, "a helmet on the head counts");
        check(worn.add(items, stone, 64 * 36) > 0 && worn.at(head + 1).empty(),
              "a full bag does not spill into the worn squares");
        check(!worn.move_to(items, 2, head), "and nothing swaps a helmet for stone");
        check(ecs::armoured(10, 20) == 2 && ecs::armoured(1, 20) == 1 && ecs::armoured(4, 0) == 4,
              "20 points take 80% off, a hit still costs one");

        db.save_player_bag(mine.public_key, "giacomo", items, worn, 1755000002, err);
        Inventory worn_back;
        db.load_player_bag(mine.public_key, items, worn_back, err);
        check(worn_back.at(head).item == helm, "and the helmet is still on after a save");

        // Wear: a blow spends durability, a stack keeps it through a save, and
        // what runs out is gone. Shift-click takes it off and puts it back on.
        helm_def.durability = 10;
        items.register_item(helm_def);
        check(worn_back.wear_out(items, 4) && worn_back.at(head).used == 4, "a blow wears the helmet");
        db.save_player_bag(mine.public_key, "giacomo", items, worn_back, 1755000003, err);
        Inventory used_back;
        db.load_player_bag(mine.public_key, items, used_back, err);
        check(used_back.at(head).used == 4, "and the wear is still there after a save");
        check(!used_back.swap_worn(items, head), "a full bag has no room to take it off into");
        used_back.slot(5) = ItemStack{};
        check(used_back.swap_worn(items, head) && used_back.at(head).empty() &&
                  used_back.count_of(helm) == 1,
              "a shift-click takes it off, wear and all");
        int off = -1;
        for (int i = 0; i < head; ++i) if (used_back.at(i).item == helm) off = i;
        check(off >= 0 && used_back.at(off).used == 4 && used_back.swap_worn(items, off) &&
                  used_back.at(head).item == helm,
              "and another puts it back on");
        check(used_back.wear_out(items, 6) && used_back.at(head).empty(), "until it breaks");

        // A block that breaks into an item that is no block: diamond ore.
        {
            NodeRegistry nodes;
            register_core_nodes(nodes);
            NodeDef ore;
            ore.name       = "test:diamond_ore";
            ore.drops_item = "test:diamond";
            const BlockId ore_id = nodes.register_node(ore);
            ItemRegistry gems;
            ItemDef gem;
            gem.name = "test:diamond";
            const ItemId gem_id = gems.register_item(gem);
            gems.adopt_blocks(nodes);
            check(gems.of_block(ore_id) == gem_id && gems.find("test:diamond_ore") == kNoItem,
                  "diamond ore breaks into a diamond, and has no item of its own");
        }

        int64_t rows = 0;
        {
            ::sqlite3 *raw = nullptr;
            if (sqlite3_open(path.c_str(), &raw) == SQLITE_OK) {
                sqlite3_stmt *stmt = nullptr;
                sqlite3_prepare_v2(raw, "SELECT COUNT(*) FROM player_items", -1, &stmt, nullptr);
                if (sqlite3_step(stmt) == SQLITE_ROW) rows = sqlite3_column_int64(stmt, 0);
                sqlite3_finalize(stmt);
                sqlite3_close(raw);
            }
        }
        check(rows == 1, "rather than leaving two rows for one player");

        db.close();
    }

    std::printf("\nwhat the cells remember\n");
    {
        // M8: three of the four classic portal-puzzle blocks are a cell holding a value, and
        // the whole point of them is that the value is still there when the
        // world is opened again. The clock's exit test is a number that came
        // back the same number.
        NodeMetaSet meta;
        std::string why;
        meta.set(Cell{1, 2, 3}, "count", "9", why);
        meta.set(Cell{1, 2, 3}, "who", "giacomo", why);
        meta.set(Cell{4, 5, 6}, "enabled", "1", why);

        WorldDb     db;
        std::string err;
        db.open(path, kSeed, registry, err);
        check(db.save_node_meta(meta, err), err.empty() ? "what the cells remember saves"
                                                        : err.c_str());
        db.close();

        WorldDb     again;
        NodeMetaSet back;
        again.open(path, kSeed, registry, err);
        const int64_t n = again.load_node_meta(back, err);
        check(n == 3, "and all three fields come back");
        check(back.cells() == 2, "across the two cells that had them");
        check(back.get(Cell{1, 2, 3}, "count") == "9", "with the value that was written");
        check(back.get(Cell{1, 2, 3}, "who") == "giacomo", "and the second field of that cell");
        check(back.get(Cell{4, 5, 6}, "enabled") == "1", "and the other cell's own");
        check(back.get(Cell{9, 9, 9}, "count").empty(), "and nothing anybody else's");

        // A field a mod erased has to LEAVE the file. A save that only wrote
        // what is there would load the erased one back next time -- a pedestal
        // that comes back from the dead holding what it held.
        meta.set(Cell{1, 2, 3}, "who", "", why);
        meta.clear(Cell{4, 5, 6});
        check(again.save_node_meta(meta, err), "and it saves again after two were taken away");
        NodeMetaSet after;
        check(again.load_node_meta(after, err) == 1, "with one field left in the file");
        check(after.get(Cell{1, 2, 3}, "who").empty(), "the erased one gone");
        check(after.get(Cell{4, 5, 6}, "enabled").empty(), "and the cleared cell with it");
        check(after.get(Cell{1, 2, 3}, "count") == "9", "and the one that stayed still there");
        again.close();
    }

    std::printf("\ncontainers\n");
    {
        // What is in the chests. TODO.md OPEN-10: this was memory, so a world
        // that came back from disk came back with no chests in it and nothing
        // in them -- a world that keeps its blocks and loses what is in them is
        // worse than one that keeps nothing, because the ground coming back
        // says it is the same world.
        ItemRegistry items;
        items.adopt_blocks(registry);
        const ItemId stone = items.find("core:stone");
        const ItemId glass = items.find("core:glass");

        WorldDb     db;
        std::string err;
        db.open(path, kSeed, registry, err);

        ContainerSet set;
        Inventory   *a = set.open(ContainerAt{4, 21, 9}, 27, err);
        Inventory   *b = set.open(ContainerAt{-40, 3, -7}, 9, err);
        Inventory   *c = set.open(ContainerAt{0, 0, 0}, 128, err);
        check(a && b && c, "three chests, one of them at negative coordinates");

        a->slot(0)  = ItemStack{stone, 64};
        a->slot(26) = ItemStack{glass, 2};
        b->slot(4)  = ItemStack{glass, 1};
        // c is left empty on purpose: an empty chest is still a chest, and a
        // save that only writes the ones with something in them loses the box.

        check(db.save_containers(items, set, err), "they save");
        check(db.stats().containers == 3, "all three of them, empty one included");

        ContainerSet   back;
        std::string    load_err;
        const int64_t  n = db.load_containers(items, back, load_err);
        check(n == 3 && load_err.empty(), "and all three load");

        const Inventory *ba = back.find(ContainerAt{4, 21, 9});
        const Inventory *bb = back.find(ContainerAt{-40, 3, -7});
        const Inventory *bc = back.find(ContainerAt{0, 0, 0});
        check(ba && bb && bc, "at the cells they were at");
        check(ba && ba->size() == 27 && bb && bb->size() == 9 && bc && bc->size() == 128,
              "each the size it was made, because a container never resizes");
        check(ba && ba->at(0).item == stone && ba->at(0).count == 64 &&
                  ba->at(26).item == glass && ba->at(26).count == 2,
              "holding what they held, in the slots they held it in");
        check(bb && bb->at(4).count == 1, "and the little one too");

        // A chest that was broken has to LEAVE the file. A save that only
        // writes what exists would leave the old row behind, and the next load
        // would put the chest back holding what it held.
        set.erase(ContainerAt{4, 21, 9});
        check(db.save_containers(items, set, err), "the world saves again after one is broken");
        check(db.stats().containers == 2, "and the file has two rows, not three");

        ContainerSet after;
        db.load_containers(items, after, load_err);
        check(after.find(ContainerAt{4, 21, 9}) == nullptr,
              "the broken one does not come back from the dead holding what it held");

        db.close();
    }

    std::printf("\na container row that has rotted\n");
    {
        ItemRegistry items;
        items.adopt_blocks(registry);

        // Same shape as the rotted chunk below: through SQLite, so the database
        // is perfectly valid and only the payload is rubbish.
        {
            WorldDb     db;
            std::string err;
            db.open(path, kSeed, registry, err);
            ContainerSet set;
            set.open(ContainerAt{1, 2, 3}, 27, err);
            set.open(ContainerAt{4, 5, 6}, 27, err);
            db.save_containers(items, set, err);
            db.close();
        }

        ::sqlite3 *raw = nullptr;
        if (sqlite3_open(path.c_str(), &raw) == SQLITE_OK) {
            sqlite3_exec(raw,
                         "UPDATE containers SET data = CAST('not an inventory' AS BLOB) "
                         "WHERE x = 1 AND y = 2 AND z = 3",
                         nullptr, nullptr, nullptr);
            sqlite3_close(raw);
        }

        WorldDb     db;
        std::string err;
        check(db.open(path, kSeed, registry, err), "the world still opens");

        ContainerSet  set;
        std::string   load_err;
        const int64_t n = db.load_containers(items, set, load_err);

        check(n == 1, "the good chest loads");
        check(!load_err.empty(), "and the bad one is reported, not swallowed");
        check(set.find(ContainerAt{4, 5, 6}) != nullptr, "the other chest is untouched");
        std::printf("        reported: %s\n", load_err.c_str());
        db.close();
    }

    std::printf("\na container row that starts well and stops\n");
    {
        // The other half of a rotted row, and a different code path: the header
        // reads, so the size is known and the chest gets made, and then the
        // stacks run out halfway. A row that is not an inventory at all never
        // reaches the decoder, so a decoder that swallowed its own errors would
        // be covered by nothing above.
        ItemRegistry items;
        items.adopt_blocks(registry);
        const ItemId stone = items.find("core:stone");

        std::vector<uint8_t> good;
        {
            WorldDb     db;
            std::string err;
            db.open(path, kSeed, registry, err);

            ContainerSet set;
            Inventory   *chest = set.open(ContainerAt{7, 7, 7}, 27, err);
            chest->slot(0) = ItemStack{stone, 9};
            good           = encode_inventory(items, *chest);
            db.save_containers(items, set, err);
            db.close();
        }

        // Half of it. The first bytes still say "an inventory of 27 slots".
        const std::vector<uint8_t> half(good.begin(), good.begin() + (long)good.size() / 2);
        check(stored_slots(half) == 27, "the truncated row still says how big the chest is");

        ::sqlite3 *raw = nullptr;
        if (sqlite3_open(path.c_str(), &raw) == SQLITE_OK) {
            sqlite3_stmt *stmt = nullptr;
            sqlite3_prepare_v2(raw, "UPDATE containers SET data = ? WHERE x = 7 AND y = 7 AND z = 7",
                               -1, &stmt, nullptr);
            sqlite3_bind_blob(stmt, 1, half.data(), (int)half.size(), SQLITE_TRANSIENT);
            sqlite3_step(stmt);
            sqlite3_finalize(stmt);
            sqlite3_close(raw);
        }

        WorldDb     db;
        std::string err;
        db.open(path, kSeed, registry, err);

        ContainerSet set;
        std::string  load_err;
        db.load_containers(items, set, load_err);

        check(!load_err.empty(), "the half a row is reported, not swallowed");
        const Inventory *chest = set.find(ContainerAt{7, 7, 7});
        check(chest != nullptr,
              "and the chest is still there: a row that will not read says the contents are "
              "unreadable, not that the block was never placed");
        check(chest && chest->at(0).empty(),
              "empty, rather than holding whatever the first half of the row happened to say");
        std::printf("        reported: %s\n", load_err.c_str());
        db.close();
    }

    std::printf("\nfiles that are not this world\n");
    {
        // A different seed. The stored chunks and the ones generated on demand
        // would come from two different worlds, and the seam is somewhere a
        // player walks into.
        WorldDb     db;
        std::string err;
        check(!db.open(path, kSeed + 1, registry, err),
              "a file made with another seed is refused");
        std::printf("        refused: %s\n", err.c_str());

        check(db.open(path, kSeed, registry, err), "while the right seed still opens it");
        World before;
        db.load_all_chunks(before, err);
        const uint64_t before_hash = world_hash(before, kExtent, kWorldHeight);
        db.close();

        // A block table that moved: a block registered ahead of the core ones
        // shifts every id after it, which is what a mod adding a block does.
        // The ids in the chunks now mean something else, so the file is carried
        // into the new table by name, and every cell holds what it held.
        NodeRegistry moved;
        NodeDef      extra;
        extra.name = "test:added";
        moved.register_node(extra);
        register_core_nodes(moved);

        check(db.open(path, kSeed, moved, err) && db.chunks_remapped() > 0,
              "a table with a block added opens, and the chunks are rewritten into it");
        std::printf("        %lld chunk(s) rewritten\n", (long long)db.chunks_remapped());
        World after;
        db.load_all_chunks(after, err);
        db.close();

        int64_t wrong = 0;
        for (int z = 0; z < kExtent; ++z)
            for (int y = 0; y < kWorldHeight; ++y)
                for (int x = 0; x < kExtent; ++x)
                    wrong += registry.get(before.get_block(x, y, z)).name !=
                             moved.get(after.get_block(x, y, z)).name;
        check(wrong == 0, "and every cell holds the block it held, by name");

        check(db.open(path, kSeed, moved, err) && db.chunks_remapped() == 0,
              "and the file now says the new table: it opens again as it is");
        db.close();

        // A table without a block the world holds -- its mod is gone. It opens,
        // the block is a placeholder under its own name that nobody can hold,
        // and every cell still holds what it held, by name.
        NodeRegistry lacking;
        for (const std::string &name : core_node_names()) {
            if (name == "core:stone") continue;
            NodeDef d;
            d.name = name;
            lacking.register_node(d);
        }
        const bool opened = db.open(path, kSeed, lacking, err);
        const BlockId stone = lacking.find("core:stone");
        check(opened && db.placeholders().size() == 1 && stone != kAir && lacking.is_orphan(stone),
              "a table that lost a block the world holds opens it, the block a placeholder");
        World lost;
        db.load_all_chunks(lost, err);
        db.close();
        wrong = 0;
        for (int z = 0; z < kExtent; ++z)
            for (int y = 0; y < kWorldHeight; ++y)
                for (int x = 0; x < kExtent; ++x)
                    wrong += registry.get(before.get_block(x, y, z)).name !=
                             lacking.get(lost.get_block(x, y, z)).name;
        check(wrong == 0, "and every cell still holds the block it held, by name");

        // And back. With the block's mod back it is itself again, and the block
        // the moved table added was never placed, so a table without it loses
        // nothing: the world is the one it was.
        check(db.open(path, kSeed, registry, err) && db.chunks_remapped() > 0 &&
                  db.placeholders().empty(),
              "the original table opens it again, with no placeholder left");
        World back;
        db.load_all_chunks(back, err);
        check(world_hash(back, kExtent, kWorldHeight) == before_hash,
              "and the world is exactly the one saved before the table moved");
        db.close();
    }

    std::printf("\na file that has rotted\n");
    {
        // A chunk blob replaced with something that is not a chunk. A file is
        // not more trustworthy than a socket: it can be corrupted by the disk,
        // written by another version, or handed over by somebody else.
        //
        // Corrupted through SQLite rather than by poking at the bytes of the
        // file, so the database stays perfectly valid and only the payload is
        // wrong -- which is the case the code is supposed to survive. The first
        // version of this section asserted the decoder refuses junk, which the
        // codec test already proves and says nothing about the database.
        const std::string broken = temp_path("broken");
        remove_db(broken);

        int64_t total = 0;
        {
            WorldDb     db;
            std::string err;
            db.open(broken, kSeed, registry, err);
            db.save_all_chunks(world, err);
            total = db.stats().chunks;
            db.close();
        }

        {
            ::sqlite3 *raw = nullptr;
            check(sqlite3_open_v2(broken.c_str(), &raw, SQLITE_OPEN_READWRITE, nullptr) ==
                      SQLITE_OK,
                  "the file opens as an ordinary database");

            // Sixty-four bytes that are not a chunk -- except that they are:
            // 0xAB followed by zeros decodes as a valid uniform chunk at
            // position (171, 0, 0), because the position comes from the blob.
            // That is exactly the case worth having: the loader has to notice
            // the blob does not belong to the row it was found in, or one
            // rotten row takes a healthy chunk somewhere else with it.
            const unsigned char junk[64] = {0xAB};
            sqlite3_stmt       *stmt     = nullptr;
            // Not by rowid: the chunks table is WITHOUT ROWID, so there is
            // none, and an UPDATE keyed on it matches nothing and reports
            // success. The check below caught that -- it counted the rows that
            // loaded and there were still all of them.
            sqlite3_prepare_v2(raw,
                               "UPDATE chunks SET data = ? WHERE (x, y, z) IN "
                               "(SELECT x, y, z FROM chunks ORDER BY x, y, z LIMIT 1)",
                               -1, &stmt, nullptr);
            sqlite3_bind_blob(stmt, 1, junk, (int)sizeof(junk), SQLITE_TRANSIENT);
            const bool wrote   = sqlite3_step(stmt) == SQLITE_DONE;
            const int  changed = sqlite3_changes(raw);
            sqlite3_finalize(stmt);
            sqlite3_close(raw);
            // Both, because a statement that matched no rows also returns DONE.
            check(wrote && changed == 1, "and one chunk's payload is replaced with rubbish");
        }

        {
            WorldDb     db;
            std::string err;
            check(db.open(broken, kSeed, registry, err),
                  "the world still opens, because one bad row is not a bad world");

            World         loaded;
            std::string   load_err;
            const int64_t n = db.load_all_chunks(loaded, load_err);

            check(n == total - 1, "every chunk but the broken one is loaded");
            check(!load_err.empty(), "and the one that is broken is reported, not swallowed");
            check(loaded.find_chunk(ChunkPos{171, 0, 0}) == nullptr,
                  "and the rubbish did not install itself somewhere else in the world");
            std::printf("        reported: %s\n", load_err.c_str());
            db.close();
        }

        remove_db(broken);
    }

    remove_db(path);

    std::printf("\n%s\n", g_failures == 0 ? "all storage checks passed" : "STORAGE CHECKS FAILED");
    return g_failures == 0 ? 0 : 1;
}
