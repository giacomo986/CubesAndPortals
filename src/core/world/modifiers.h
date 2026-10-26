#pragma once

#include "core/registry/node_registry.h"
#include "core/voxel/chunk.h"

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

// Things that happen TO cells, on their own, without anybody touching them.
//
// DESIGN.md M11 lists an "scheduler ABM/LBM" among what the engine owes the
// The original engine compatibility layer, and it is the mechanism most of somebody else's
// content is made of: grass spreading onto dirt, a furnace cooking, water
// flowing, a sapling growing. Two kinds, and they are not the same thing:
//
//   ABM   an Active Block Modifier. Every `interval` seconds, each loaded cell
//         holding one of its blocks has a one-in-`chance` chance of its action
//         running. This is the one that costs something.
//   LBM   a Loading Block Modifier. Runs once per matching cell of a chunk WHEN
//         THE CHUNK IS LOADED, and never again. It is what a mod uses to fix up
//         a world written by an older version of itself.
//
// Three things decide whether this is usable at all, and all three are here
// rather than in whoever calls it:
//
// **It must not read every cell.** A world of 512 is 512 chunks and 16.7
// million cells; an ABM over one block asked every second would read all of
// them to find out that almost none of them hold it. Chunk::ids() is a bit
// per block id modulo 1024, so a chunk that cannot hold an ABM's blocks is
// skipped with one AND -- and a chunk that might is read cell by cell, because
// a bit is a maybe.
//
// **It must not be able to stall the tick.** A step is given a budget in
// milliseconds and stops when it is spent, keeping its place; the sweep
// continues on the next tick and the interval is only counted from a sweep that
// FINISHED. So a mod that asks for more than the budget allows runs slower than
// it asked rather than making the server late, and `Modifiers::behind()` says
// so rather than leaving it to be guessed.
//
// **What it does to a world must be the same on two runs of the same seed.**
// The chance is drawn from a hash of the seed, the sweep number and the cell's
// coordinates -- not from a generator whose state depends on how the sweeps
// happened to be cut up by the budget. The chunks are walked in sorted order
// for the same reason (Chunk::chunk_positions_sorted).
//
// This knows nothing about Lua: an action is a callback the caller supplies,
// exactly as a mapgen stage is. What a mod registers is turned into one of
// these by whoever owns the VM.

namespace cnp {

// One in this many cells of a sweep, at most, before the clock is read again.
// A read of steady_clock is tens of nanoseconds and a cell test is a few, so
// asking the time per cell would be most of the cost of the sweep.
inline constexpr int kModifierClockEvery = 256;

struct Modifier {
    std::string              name;        // "mymod:grow", and unique
    std::string              owner;       // the mod that registered it
    std::vector<std::string> node_names;  // what it runs on, by name
    std::vector<BlockId>     nodes;       // resolved when registration closes
    Chunk::IdMask            mask;        // the id bits those blocks occupy

    double interval = 1.0;   // seconds, ABM only
    int    chance   = 1;     // one in this many, per interval, ABM only

    // The caller's handle on whatever runs the action. Neither is looked at
    // here; they are handed back with every call so the caller knows whose
    // action it is and in which VM.
    void *vm      = nullptr;
    int   handler = -1;

    // What it has done, for the line a server prints and the gate reads.
    uint64_t tested  = 0;   // cells looked at
    uint64_t fired   = 0;   // actions run
    double   seconds = 0.0;

    bool enabled = true;
};

// What a step did, so the caller can print it rather than reason about it.
struct ModifierStepStats {
    int      abms_swept    = 0;   // sweeps that finished this step
    int      cells_tested  = 0;
    int      actions_run   = 0;
    int      chunks_looked = 0;   // chunks whose bits said maybe
    int      chunks_skipped = 0;  // chunks the bits ruled out
    double   seconds       = 0.0;
    bool     out_of_budget = false;
};

class Modifiers {
public:
    // `fire` runs one action. Returning false means the action failed and its
    // modifier is disabled -- the same bargain a mod's tick handler makes.
    using Fire = std::function<bool(const Modifier &, int x, int y, int z, BlockId id)>;

    // Registration, with the refusals that matter. Refused and NOT accepted
    // quietly, because an ABM that never runs is a mod doing nothing and never
    // being told why: an empty name, a name another modifier has, no blocks to
    // run on, an interval that is not positive, a chance below one, and
    // anything at all after registration has closed.
    bool register_abm(Modifier m, std::string &error);
    bool register_lbm(Modifier m, std::string &error);

    // Resolves every modifier's block names to ids. Called once, after the
    // whole mod directory has loaded, for the same reason the mapgen stages are
    // run then: what a mod names may be registered by a mod that loads later.
    // A name nothing registered disables that modifier and says which name.
    void close_registration(const NodeRegistry &reg, std::vector<std::string> &errors);
    bool registration_closed() const { return m_closed; }

    // A chunk has arrived -- generated, read from a file, or streamed in. Its
    // LBMs run on the next step rather than here: loading a chunk happens in
    // the middle of other work, and running somebody's Lua inside that is how a
    // load becomes a stall.
    void chunk_loaded(ChunkPos p);

    // One tick. `now` is the world's own clock in seconds, `budget_ms` what
    // this may spend. Returns what it did.
    ModifierStepStats step(World &world, double now, double budget_ms, const Fire &fire);

    // Sweeps that wanted to run and have not finished, which is the honest
    // answer to "is this keeping up".
    int behind() const;

    const std::vector<Modifier> &abms() const { return m_abms; }
    const std::vector<Modifier> &lbms() const { return m_lbms; }

    // For the caller that has to put a number in a log line.
    size_t chunks_waiting_for_lbm() const { return m_fresh.size(); }

    void disable(const std::string &name, const std::string &why);

    // The seed the chance is drawn against. A world's own seed, so two runs of
    // one world fire the same cells.
    void set_seed(uint32_t seed) { m_seed = seed; }

    // Which chunks an active modifier may visit. Unset, every chunk in the
    // world. A server sets it to the lit columns (core/world/columns.h): a
    // column that only holds blocks for its neighbours' light is let go without
    // being kept, and moss that spread into one would be moss nobody sees and
    // the world forgets.
    void set_active(std::function<bool(ChunkPos)> active) { m_active = std::move(active); }

    // Where the players are, as chunks, asked when a sweep starts: the sweep
    // visits the chunks nearest to them first, so a sweep the budget cuts short
    // has already done what somebody can see (OPEN-42). Which cells fire does
    // not depend on the order -- that is the hash's -- only when.
    void set_near(std::function<std::vector<ChunkPos>()> near) { m_near = std::move(near); }

private:
    struct Sweep {
        size_t   at_chunk = 0;      // index into the sorted positions
        int      at_cell  = 0;      // index into that chunk
        bool     running  = false;
        uint64_t number   = 0;      // which sweep this is, for the hash
        double   due_at   = 0.0;
        std::vector<ChunkPos> order;
    };

    bool add(Modifier m, bool is_abm, std::string &error);

    std::vector<Modifier> m_abms;
    std::vector<Modifier> m_lbms;
    std::vector<Sweep>    m_sweeps;   // one per ABM, same order
    std::vector<ChunkPos> m_fresh;    // chunks whose LBMs have not run
    size_t m_fresh_lbm = 0;           // the LBM the front chunk resumes at
    bool                  m_closed = false;
    uint32_t              m_seed   = 0;

    std::function<bool(ChunkPos)> m_active;
    std::function<std::vector<ChunkPos>()> m_near;
};

// Whether an ABM fires on this cell, this sweep. A hash and not a generator:
// the same world and the same sweep pick the same cells however the budget cut
// the sweep up, and two runs of one seed do the same thing.
bool modifier_fires(uint32_t seed, uint64_t sweep, int x, int y, int z, int chance);

} // namespace cnp
