#pragma once

#include "core/registry/node_registry.h"
#include "core/storage/world_db.h"
#include "core/world/columns.h"
#include "core/worldgen/worldgen.h"

#include <cstdint>
#include <functional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

// A world with no edge, kept (TODO.md OPEN-35, OPEN-38).
//
// core/world/columns.h decides WHEN a column comes into memory, is lit and is
// let go. This is what those three things DO, written once for the two programs
// that hold a world of their own: the server, and the client playing alone.
//
//   - **Coming in.** From the world file when there is one and it has the
//     column; from the copy kept in memory when there is no file; from the
//     generator otherwise, which is the owner's hook because the stages are
//     the owner's (a server's mods, a client's scene).
//   - **Lit.** The light of the column, exactly, and then the owner is told
//     the chunks have loaded -- which is the event a loading modifier waits for.
//   - **Let go.** Whoever holds its chunks is told. A lit column is kept: every
//     change in the world file; without one, only a column something CHANGED,
//     because a column nobody touched is exactly what the generator gives back
//     and keeping it would be a server remembering every place anybody ever
//     walked past.
//
// And the one rule about writes: a write goes into a lit column, or -- while the
// mods are loading -- into the region generated before them. Anything else waits
// for its column to be lit and is handed back to the owner then.

namespace cnp {

struct PendingWrite {
    int     x = 0, y = 0, z = 0;
    BlockId id          = kAir;
    uint8_t facing      = 0;
    bool    facing_only = false;
};

class ColumnKeeper {
public:
    struct Hooks {
        // Generate this column into an empty place in the world: every stage.
        std::function<void(ColumnPos)> generate;

        // A column was lit: these are its chunks, just loaded.
        std::function<void(const std::vector<ChunkPos> &)> loaded;

        // These chunks are about to leave the world.
        std::function<void(const std::vector<ChunkPos> &)> leaving;

        // A write that waited for its column, now that the column is lit.
        std::function<void(const PendingWrite &)> apply;
    };

    ColumnKeeper(World &world, const NodeRegistry &registry, int height);

    ColumnKeeper(const ColumnKeeper &)            = delete;
    ColumnKeeper &operator=(const ColumnKeeper &) = delete;

    void set_hooks(Hooks hooks) { m_hooks = std::move(hooks); }

    // Where columns are kept. Null keeps them in memory.
    void     set_db(WorldDb *db) { m_db = db; }
    WorldDb *db() const { return (m_db != nullptr && m_db->is_open()) ? m_db : nullptr; }

    WorldColumns       &columns() { return m_columns; }
    const WorldColumns &columns() const { return m_columns; }
    bool                lit_chunk(ChunkPos p) const { return m_columns.lit_chunk(p); }

    // ------------------------------------------------------------------ writes

    // While true, a column that has blocks and no light yet takes writes: the
    // region generated before the mods load, which they build their scenes in.
    void set_loading(bool loading) { m_loading = loading; }

    bool takes_writes(int x, int z) const;

    // A write that has to wait for its column.
    void defer(const PendingWrite &w);

    // A write went in: that column is in use, and the columns its relight will
    // read have their blocks back. Whether it CHANGED is read off its chunks
    // when it is let go, so a write that did not come through here -- a
    // player's edit, say -- is not a change lost.
    void wrote(int x, int z, uint64_t now);

    // Columns with writes waiting, in a fixed order.
    std::vector<ColumnPos> pending_columns() const;

    // ----------------------------------------------------------------- startup

    // A column put in the world whole by the owner.
    void mark_generated(ColumnPos c, uint64_t now);

    // The world file's copy of a column laid over what is there, when it has
    // one. Returns how many chunks it read; a row that will not read is named
    // in `err` and skipped.
    size_t lay_stored(ColumnPos c, std::string &err);

    // -------------------------------------------------------------------- work

    void make_lit(ColumnPos c, uint64_t now);
    void ensure_blocks(ColumnPos c, uint64_t now) { m_columns.ensure_blocks(c, now); }

    // Towards each column of `want` in order, until the budget is spent, and
    // then the writes whose columns are lit now.
    WorldColumns::Advance advance(const std::vector<ColumnPos> &want, double budget_ms,
                                  uint64_t now);

    // The writes whose columns are lit, handed to the owner.
    void apply_ready_writes();

    // Lets go of columns further than `keep` from all the centres and not
    // touched for `idle`, except one a write is waiting for -- at most
    // kUnloadPerCall of them a call. A player who has just crossed a hundred
    // columns leaves a hundred behind at once, and keeping each of them is an
    // encode or a row written: all in one frame, that was a 279 ms stall.
    static constexpr size_t kUnloadPerCall = 12;
    void unload_idle(const std::vector<ColumnPos> &centres, int keep, uint64_t idle, uint64_t now);

    // Every lit column lit again: what emits light, or stops it, has changed.
    void relight();

    // ------------------------------------------------------------------ saving

    // Chunks changed since the last call, of lit columns only. A column that
    // holds blocks for its neighbours' light is what the generator or the file
    // gives back, and its light is not right yet.
    std::vector<ChunkPos> take_unsaved_lit();

    // ------------------------------------------------------------------- stats

    double fill_ms() const { return m_fill_ms; }
    double light_ms() const { return m_light_ms; }
    size_t kept_columns() const { return m_kept.size(); }

    // Columns brought back because a write's relight would have read them as
    // air. Nothing is wrong with a number here -- it is a mod writing where
    // nobody is -- but a number that grows every tick is a column border being
    // relit over and over.
    uint64_t light_reach() const { return m_light_reach; }
    size_t kept_bytes() const { return m_kept_bytes; }
    size_t pending_writes() const;

private:
    void fill(ColumnPos c);
    void light(ColumnPos c);
    void release(const std::vector<ColumnPos> &columns);

    World              &m_world;
    const NodeRegistry &m_registry;
    int                 m_height;
    WorldColumns        m_columns;
    Hooks               m_hooks;
    WorldDb            *m_db      = nullptr;
    bool                m_loading = true;

    std::unordered_map<ColumnPos, std::vector<std::vector<uint8_t>>, ColumnPosHash> m_kept;
    size_t                                                                          m_kept_bytes = 0;

    std::unordered_set<ColumnPos, ColumnPosHash> m_from_store;   // read back, not generated
    std::unordered_map<ColumnPos, std::vector<PendingWrite>, ColumnPosHash> m_pending;

    uint64_t m_light_reach = 0;

    double m_fill_ms  = 0.0;
    double m_light_ms = 0.0;
};

} // namespace cnp
