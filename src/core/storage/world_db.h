#pragma once

#include "core/item/container.h"
#include "core/net/crypto.h"
#include "core/item/item_registry.h"
#include "core/registry/node_registry.h"
#include "core/storage/mod_store.h"
#include "core/voxel/chunk.h"
#include "core/world/node_meta.h"

#include <cstdint>
#include <string>
#include <vector>

// The world on disk (DESIGN.md 3.3, M3's "SQLite").
//
// One file, one table of chunks keyed by position, one of players keyed by
// name, and a handful of rows of metadata. Two-to-eight players per world was
// decided in DESIGN.md, and that decision is what makes this the whole storage
// design: no region files, no sharding, no compaction pass. SQLite handles a
// few thousand blobs without being asked nicely.
//
// **A stored chunk is the same bytes a streamed chunk is.** The wire format and
// the storage format are the same encoder, which is worth being explicit about
// because the alternative is two formats, two decoders, and two sets of
// hostile-input handling of which one is always the weaker. It also means a
// chunk read from disk can be forwarded to a client without being decoded.
//
// A file keeps the block table it was written with, as names in id order. A
// server whose table is different -- a mod added a block, and every id after it
// moved -- has the stored chunks rewritten by NAME into its own table when it
// opens the file. A block the server no longer has at all -- its mod is gone --
// becomes a magenta placeholder under its own name (register_unknown), and is
// itself again once the mod is back. What is refused rather than worked
// around: a world whose seed is not the server's.

// Forward declared at global scope, which is where sqlite3 lives. Declaring it
// inside the namespace makes a different, incompatible type with the same name,
// and the error that produces points at the call rather than at the cause.
struct sqlite3;

namespace cnp {

// Where a player was when the server last kept them. Keyed by identity for the
// reason the bag is (see save_player_bag). No look direction: a correction
// carries a position and nothing else, so a stored yaw would be a number
// nothing could hand back.
struct StoredPlayer {
    uint8_t     identity[net::kSignPublicBytes]{};
    std::string name;            // the last label that key played under, for a human
    float       x = 0.0f, y = 0.0f, z = 0.0f;
    int64_t     last_seen = 0;   // unix seconds, for a server listing its players
    // Where they were looking, a unit vector; all 0 for a row that kept none.
    float       look_x = 0.0f, look_y = 0.0f, look_z = 0.0f;
};

struct WorldDbStats {
    int64_t chunks        = 0;
    int64_t players       = 0;
    int64_t containers    = 0;
    int64_t bytes_on_disk = 0;
};

// What this build writes into a world file, and what it will open.
//
// Exposed because a test has to be able to write a file that says something
// ELSE. A version that never moves is a check that cannot fail: it compares
// this build's number with this build's number and passes for any file any
// build ever wrote, which is what happened between M2 and M8 while the schema
// gained two tables.
inline constexpr int kWorldSchemaVersion = 3;

class WorldDb {
public:
    WorldDb() = default;
    ~WorldDb();

    WorldDb(const WorldDb &)            = delete;
    WorldDb &operator=(const WorldDb &) = delete;

    // Opens or creates `path`. On a new file the seed and `blocks`' table are
    // written; on an existing one the seed is checked, and a table that moved
    // is carried over by name (see remap_blocks) -- which may add placeholders
    // to `blocks`, so it is opened after the mods have registered theirs. A
    // mismatch that cannot be carried is a failure with a message rather than
    // a silent reinterpretation.
    bool open(const std::string &path, uint32_t seed, NodeRegistry &blocks, std::string &err);
    void close();
    bool is_open() const { return m_db != nullptr; }

    uint32_t seed() const { return m_seed; }
    uint64_t content_hash() const { return m_content_hash; }

    // How many stored chunks the last open rewrote into a block table that had
    // moved: 0 when the file's table was this server's.
    int64_t chunks_remapped() const { return m_remapped; }

    // The blocks the last open found in stored cells that nothing registered,
    // and added to the registry as placeholders.
    const std::vector<std::string> &placeholders() const { return m_placeholders; }

    // Chunks. save_chunks takes a batch because one transaction per chunk is
    // roughly fifty times slower, and a save of a whole world is a batch.
    bool save_chunks(const World &world, const std::vector<ChunkPos> &positions, std::string &err);
    bool save_all_chunks(const World &world, std::string &err);

    // Returns null when there is no such chunk, which is not an error: a world
    // is generated on demand and most of it has never been stored.
    std::unique_ptr<Chunk> load_chunk(ChunkPos p, std::string &err) const;

    // Every chunk stored at one column, whatever its y (TODO.md OPEN-35): how a
    // world that is loaded a column at a time asks whether a column was ever
    // kept. Empty when it was not, which is not an error. A row that will not
    // read is skipped and named in `err`, the way load_all_chunks does it.
    std::vector<std::unique_ptr<Chunk>> load_column(int cx, int cz, std::string &err) const;

    // Loads everything into `world`. Returns how many chunks were read.
    int64_t load_all_chunks(World &world, std::string &err) const;

    // What is in the chests. All of them at once, in one transaction that
    // clears the table first: a container broken since the last save has to
    // leave the file, and a save that only writes what exists would load it
    // back next time holding what it held.
    //
    // Loading takes a MUTABLE registry, which is the one surprising thing in
    // this header and belongs in it rather than in a comment further down: a
    // stack is stored by name, and a name this build has never heard of is
    // minted as an orphan so that the stack keeps its identity. See
    // core/item/item_codec.h.
    bool    save_containers(const ItemRegistry &items, const ContainerSet &set, std::string &err);
    int64_t load_containers(ItemRegistry &items, ContainerSet &set, std::string &err) const;

    // What a player is carrying, keyed by the public key that proves who they
    // are. NOT by the name: DESIGN.md 4.4 says the key is the identity and the
    // name is a label, so a bag keyed by name is a bag anybody takes by typing
    // yours. The name goes in the row for a human reading the file and nothing
    // looks anything up by it.
    //
    // A session with no identity has no key and therefore no saved bag. That is
    // a limit and not an oversight: there is nothing to key it by that another
    // connection could not also claim.
    //
    // Loading returns false when there is no such row, which is not an error --
    // a player who has never been here has an empty bag and that is right. It
    // also returns false when the row will not read, and then `err` says so;
    // either way `bag` is left as it was, because a half-filled bag out of a
    // rotten row is worse than an empty one.
    bool save_player_bag(const uint8_t identity[net::kSignPublicBytes], const std::string &name,
                         const ItemRegistry &items, const Inventory &bag, int64_t when,
                         std::string &err);
    // When this player's bag was last kept here, in unix seconds; -1 for never.
    int64_t bag_seen(const uint8_t identity[net::kSignPublicBytes]) const;

    bool load_player_bag(const uint8_t identity[net::kSignPublicBytes], ItemRegistry &items,
                         Inventory &bag, std::string &err) const;

    // What the cells remember (core/world/node_meta.h). Written the way the
    // containers are -- everything, in one transaction that clears the table
    // first -- because a field a mod erased has to leave the file.
    bool    save_node_meta(const NodeMetaSet &set, std::string &err);
    int64_t load_node_meta(NodeMetaSet &set, std::string &err) const;

    // What mods keep across a restart (core.storage_set): the keys that
    // changed since the last save (ModStore::take_touched).
    bool    save_mod_store(const std::vector<ModStore::Row> &changed, std::string &err);
    int64_t load_mod_store(ModStore &store, std::string &err) const;

    // Who a name belongs to on this world: the identity that first joined
    // under it. False when nobody has it yet, which is when claim_name gives
    // it to one -- first come, and it stays theirs for as long as this file
    // does. It is what stops somebody coming back tomorrow under a name that
    // the players here know as another person's.
    bool name_owner(const std::string &name, uint8_t out[net::kSignPublicBytes]) const;
    bool claim_name(const std::string &name, const uint8_t identity[net::kSignPublicBytes],
                    int64_t when, std::string &err);

    // For the server's owner, by hand, on a world file no server holds. A
    // freed name is claimed afresh by the next identity to join under it, and
    // that identity takes over the old one's bag and place (claim_name).
    struct NameRow {
        std::string name;
        uint8_t     identity[net::kSignPublicBytes]{};
        int64_t     first_seen = 0;
    };
    static bool list_names(const std::string &path, std::vector<NameRow> &out, std::string &err);
    static bool free_name(const std::string &path, const std::string &name, std::string &err);

    // Loading returns false when that key has no row, like load_player_bag.
    bool save_player(const StoredPlayer &player, std::string &err);
    bool load_player(const uint8_t identity[net::kSignPublicBytes], StoredPlayer &out,
                     std::string &err) const;

    bool set_meta(const std::string &key, const std::string &value, std::string &err);
    bool get_meta(const std::string &key, std::string &out) const;

    WorldDbStats stats() const;

    // One transaction round several saves: chunks, containers, cell memory and
    // the mods' store go to disk together, so a crash leaves all of them as
    // they were or all as they are (CLOSED-10). Each save is a savepoint inside
    // it, and one that fails is undone alone, as it was on its own.
    bool begin(std::string &err) { return exec("BEGIN", err); }
    // A commit that fails wrote nothing, so the sets it held are owed again.
    bool commit(std::string &err)
    {
        if (exec("COMMIT", err)) return true;
        std::string ignored;
        exec("ROLLBACK", ignored);
        m_containers_saved = m_meta_saved = false;
        return false;
    }

private:
    // What save_containers and save_node_meta last wrote, as a sum of entry
    // hashes, so an unchanged set is not written again.
    bool     m_containers_saved = false, m_meta_saved = false;
    uint64_t m_containers_hash  = 0, m_meta_hash = 0;

    bool exec(const char *sql, std::string &err);
    bool has_table(const char *name) const;
    bool has_column(const char *table, const char *column) const;

    // Brings a file forward from the version it says it is. One step per
    // version, in order, in one transaction.
    bool migrate(int from, std::string &err);

    // Rewrites every stored chunk from the block table `from` into `to`'s, by
    // name, and records that as the file's table -- in one transaction, so a
    // file is never half one table and half the other. A name a stored cell
    // holds and `to` lacks is registered in it as a placeholder first.
    // Refused, and nothing written, only for an id past the end of `from`,
    // which is a file that has rotted.
    bool remap_blocks(const std::vector<std::string> &from, NodeRegistry &to, std::string &err);
    bool write_block_table(const std::vector<std::string> &names, std::string &err);

    ::sqlite3 *m_db = nullptr;

    uint32_t m_seed         = 0;
    uint64_t m_content_hash = 0;
    int64_t  m_remapped     = 0;
    std::vector<std::string> m_placeholders;
    std::string m_path;
};

} // namespace cnp
