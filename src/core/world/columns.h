#pragma once

#include "core/voxel/chunk.h"
#include "core/worldgen/worldgen.h"

#include <cstdint>
#include <functional>
#include <unordered_map>
#include <vector>

// Which columns of the world are in memory, and how far along each one is
// (TODO.md OPEN-35, OPEN-38).
//
// The world has no edge. A column is put in memory when somebody is near
// enough to want it -- from the world file if it was ever kept, from the
// generator if not -- and let go when nobody has been near it for a while. This
// class decides WHEN; what "put in memory" and "let go" do is the owner's, handed
// in as hooks, because on a server it is a world file and a mod's stages and in a
// test it is a lambda.
//
// Two states, and the difference between them is the one thing here that is not
// obvious:
//
//   - **Blocks.** Its cells are in the world. Its light is not right yet: light
//     reaches fifteen blocks, so a column cannot be lit before the columns
//     beside it have blocks too.
//   - **Lit.** All eight around it had blocks when it was lit, so its light is
//     exact. Only a lit column is sent, saved, swept by a modifier or written
//     by a mod -- a column with blocks and no light is scaffolding for its
//     neighbours, and it is dropped without being kept, because it is exactly
//     what the generator or the file would give back.

namespace cnp {

enum class ColumnState : uint8_t {
    Absent = 0,
    Blocks = 1,
    Lit    = 2,
};

class WorldColumns {
public:
    struct Hooks {
        // Put this column's blocks in the world. The column is empty when this
        // is called: anything a stray write left there has been taken out.
        std::function<void(ColumnPos)> fill;

        // Light it. Every column around it has blocks.
        std::function<void(ColumnPos)> light;

        // These are about to leave the world: keep what has to be kept. Their
        // chunks are still there while this runs.
        std::function<void(const std::vector<ColumnPos> &)> release;
    };

    explicit WorldColumns(World &world) : m_world(world) {}

    void set_hooks(Hooks hooks) { m_hooks = std::move(hooks); }

    ColumnState state(ColumnPos c) const;
    bool        lit(ColumnPos c) const { return state(c) == ColumnState::Lit; }
    bool        lit_chunk(ChunkPos p) const { return lit(column_of(p)); }

    // A column somebody else put in the world, said so. The region a server
    // generates before its mods load is the one case: it is generated whole,
    // stage by stage, and not a column at a time.
    void set_state(ColumnPos c, ColumnState s, uint64_t now);

    // One step towards `c` being lit: a column around it filled, or `c` lit.
    // False when it already is.
    bool step(ColumnPos c, uint64_t now);

    // Every step, now.
    void make_lit(ColumnPos c, uint64_t now);

    // Its blocks and nothing more: filled if it is absent. For a reader that
    // wants the cells and not the light, such as a hash of the world.
    void ensure_blocks(ColumnPos c, uint64_t now);

    struct Advance {
        int  filled = 0;
        int  lit    = 0;
        bool behind = false;   // stopped by the budget with something still owed
    };

    // Steps towards each column of `want`, in its order, until `budget_ms` is
    // spent. At least one step whatever the budget, so a budget smaller than a
    // column still gets there.
    Advance advance(const std::vector<ColumnPos> &want, double budget_ms, uint64_t now);

    // Somebody used this column: it is not idle.
    void touch(ColumnPos c, uint64_t now);

    // Columns further than `radius` from every centre, in columns and on
    // either axis, and not touched since `since`. In a fixed order, so two runs
    // let go of the same columns.
    std::vector<ColumnPos> idle(const std::vector<ColumnPos> &centres, int radius,
                                uint64_t since) const;

    // Releases them, then takes their chunks out of the world.
    void unload(const std::vector<ColumnPos> &columns);

    // The chunks of one column that are in the world now, bottom up.
    std::vector<ChunkPos> chunks_of(ColumnPos c) const;

    size_t count() const { return m_columns.size(); }
    size_t lit_count() const;

    // Every lit column, in a fixed order.
    std::vector<ColumnPos> lit_columns() const;

    uint64_t filled_total   = 0;
    uint64_t lit_total      = 0;
    uint64_t unloaded_total = 0;

private:
    struct Entry {
        ColumnState state   = ColumnState::Absent;
        uint64_t    touched = 0;
    };

    void fill(ColumnPos c, uint64_t now);

    World &m_world;
    Hooks  m_hooks;
    std::unordered_map<ColumnPos, Entry, ColumnPosHash> m_columns;
};

// Every column within `radius` of any of the centres, nearest to its nearest
// centre first -- the column a player stands in before all others -- and by
// position where two are as near, so the order is the same on every run.
std::vector<ColumnPos> columns_near(const std::vector<ColumnPos> &centres, int radius);

} // namespace cnp
