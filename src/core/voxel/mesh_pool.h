#pragma once

// Meshing on somebody else's thread.
//
// What moves here is exactly one step of three, and the split came from a
// measurement rather than from a guess. On a 512-node world a chunk costs
// 3.45 ms, and the profiler's rows say where: snapshot 0.32, build 3.12,
// upload 0.00. `snapshot` reads the World and has to run where the World is;
// `upload` is a GL call and can never leave the thread holding the context.
// `build` reads a ChunkSnapshot and a NodeRegistry and touches nothing else,
// which is the whole reason it can go -- and it is 90% of the cost.
//
// The registry is passed by reference and NOT copied, and it is not locked
// either. A lock around node lookup would be a lock taken a million times per
// chunk. The rule instead is that nothing may be in flight while the registry
// changes, and drain() is how a caller reaches that state before a mod reload
// rewrites it.
//
// Ownership of buffers is a swap in both directions -- submit() takes the
// caller's snapshot and leaves a spare in its place, try_take() takes a result
// and leaves the caller's old one behind to be reused. That is not a
// convenience: a 118 KB snapshot and two vertex vectors allocated per chunk per
// frame churn the allocator hard enough to show as a slow climb in resident
// memory over a long session, because glibc does not hand the arena back.

#include <cstdint>
#include <vector>

#include "core/registry/node_registry.h"
#include "core/voxel/chunk.h"
#include "core/voxel/mesher.h"

namespace cnp {

class MeshPool {
public:
    // Who a job belongs to. A pointer, because the caller that has one already
    // -- a renderer's `this` -- needs no register of handles to be invented
    // beside it.
    //
    // Not a nicety, and the gate for it exists because the alternative was
    // nearly shipped: with one queue for everybody, a session's renderer takes
    // whatever finished first and uploads it at that chunk's position in ITS
    // world. Two worlds have the same ChunkPos values -- (0,0,0) is a chunk in
    // both -- so the mesh of one world's ground lands in the other's, and the
    // staleness guard, keyed by position alone, throws away one world's fresh
    // mesh because the other world submitted later.
    using Owner = const void *;

    struct Result {
        ChunkPos pos{};

        // Which submission this is the answer to. Monotonic across the pool,
        // so comparing two of them for the same chunk says which is newer.
        uint64_t seq = 0;

        std::vector<Vertex> opaque;
        std::vector<Vertex> alpha;

        // The cells this chunk holds that are not drawn as cubes. Built on the
        // same walk the mesh is, for the same reason the visibility is: it
        // comes off the snapshot this thread is already holding.
        std::vector<PartCell> parts;

        // And the cells of liquid, with the shape of their surfaces.
        std::vector<LiquidCell> liquids;

        // Which of the chunk's faces you can see through to which. Built here
        // rather than by the caller because it comes off the same snapshot the
        // mesh does and costs one flood fill on a thread that is already busy;
        // asking the caller for it would put it back on the frame thread.
        ChunkVisibility visibility;
    };

    MeshPool();
    ~MeshPool();
    MeshPool(const MeshPool &)            = delete;
    MeshPool &operator=(const MeshPool &) = delete;

    // `workers` of 0 means one fewer than the machine has, and never fewer than
    // one. Starting a pool that is already running is a no-op.
    void start(int workers = 0);

    // Waits for what is being meshed, drops the results, joins the threads.
    void stop();

    int workers() const;

    // Hands a chunk over. The caller's snapshot is SWAPPED with a spare, so
    // whatever it passed in comes back as a buffer it can fill again.
    //
    // Returns the submission's number, or 0 for a pool that is not running --
    // nothing, rather than a number that looks like an answer. A stopped pool
    // does not quietly mesh on the caller's thread: that is a stall nobody
    // asked for and nobody could see in a profile.
    uint64_t submit(Owner owner, ChunkPos pos, ChunkSnapshot &snapshot,
                    const NodeRegistry &reg);

    // A finished mesh, if there is one. Never blocks. The caller's `out` is
    // swapped in as the recycled buffer.
    //
    // Never hands back a result older than one already handed back for the same
    // chunk: a chunk edited while it was being meshed is submitted twice, two
    // workers race, and the older answer arriving second would put the block
    // the player just broke back on the screen. Dropped here rather than in the
    // caller, because there will be more than one caller.
    bool try_take(Owner owner, Result &out);

    // Blocks until nothing is being meshed. Results stay queued for try_take.
    void drain();

    // How many submissions have not come back yet -- being meshed or waiting to
    // be. Zero after drain(), and the thing a caller checks before touching
    // anything a worker reads. With no owner it counts everybody's, which is
    // the number the registry rule is about.
    int in_flight() const;
    int in_flight(Owner owner) const;

    // Everything belonging to one owner, forgotten. For a renderer going away
    // while others keep meshing.
    //
    // WAITS first, rather than telling the caller to. A worker may be writing
    // into a job this is about to drop, and "call drain() before this" is a
    // rule that holds until the day somebody does not -- at which point the
    // pool is left believing work is outstanding that nobody will ever finish,
    // and the next drain() waits for ever. Doing it here costs a wait nobody
    // notices at shutdown and removes the misuse entirely.
    void forget(Owner owner);

private:
    struct Impl;
    Impl *m_impl = nullptr;
};

// The one pool this process has.
//
// Shared and not one per renderer: a session has a WorldRenderer each, and a
// pool each would be two lots of the machine's threads doing one machine's
// worth of work -- they would spend their time taking each other's cores away.
// Nothing about a job belongs to a renderer anyway: it carries the registry it
// is to be read with.
MeshPool &shared_mesh_pool();

} // namespace cnp
