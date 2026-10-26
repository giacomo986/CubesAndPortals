#include "core/voxel/mesh_pool.h"

#include <algorithm>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <vector>

namespace cnp {

namespace {

// One chunk on its way to a worker and back.
struct Job {
    MeshPool::Owner owner = nullptr;
    ChunkPos        pos{};
    uint64_t        seq = 0;
    ChunkSnapshot   snapshot;

    // Filled in by the worker. Kept in the job rather than in a second object
    // so that one allocation's worth of buffers travels the whole round trip
    // and comes back to be reused.
    std::vector<Vertex>   opaque;
    std::vector<Vertex>   alpha;
    std::vector<PartCell> parts;
    std::vector<LiquidCell> liquids;
    ChunkVisibility       visibility;

    // Read by a worker, written by nobody while it is in flight.
    const NodeRegistry *reg = nullptr;
};

// What the pool knows about one caller.
//
// Per owner and not per pool, both of them. A shared `done` queue hands one
// world's mesh to another world's renderer; a shared staleness table throws one
// world's fresh mesh away because the other world submitted a chunk with the
// same coordinates later. Two worlds both have a chunk at (0,0,0).
struct OwnerState {
    std::deque<Job>                                      done;
    std::unordered_map<ChunkPos, uint64_t, ChunkPosHash> handed_back;
    int                                                  busy = 0;
};

} // namespace

struct MeshPool::Impl {
    mutable std::mutex      mu;
    std::condition_variable work_ready;    // a worker waits on this
    std::condition_variable went_quiet;    // drain waits on this

    std::vector<std::thread> threads;
    bool                     stopping = false;

    std::deque<Job>  queue;    // waiting for a worker, everybody's, in order
    std::vector<Job> spare;    // buffers to be filled again

    std::unordered_map<Owner, OwnerState> owners;

    // Everybody's in-flight count. Kept beside the per-owner ones rather than
    // summed on demand, because drain() waits on it and a predicate that walks
    // a map under a condition variable is a predicate run on every wake-up.
    int busy = 0;

    uint64_t next_seq = 1;     // 0 is "no submission", which is what a stopped
                               // pool answers with

    Job take_spare()
    {
        if (spare.empty()) return Job{};
        Job j = std::move(spare.back());
        spare.pop_back();
        return j;
    }

    void recycle(Job &&j)
    {
        j.opaque.clear();
        j.alpha.clear();
        j.reg   = nullptr;
        j.owner = nullptr;
        spare.push_back(std::move(j));
    }

    void worker()
    {
        for (;;) {
            Job job;
            {
                std::unique_lock<std::mutex> lock(mu);
                work_ready.wait(lock, [this] { return stopping || !queue.empty(); });
                if (stopping && queue.empty()) return;
                job = std::move(queue.front());
                queue.pop_front();
            }

            // Outside the lock, which is the entire point of this file.
            mesh_snapshot_into(job.snapshot, *job.reg, job.opaque, &job.alpha, &job.parts,
                               &job.liquids);
            job.visibility = compute_visibility(job.snapshot, *job.reg);

            {
                std::lock_guard<std::mutex> lock(mu);
                const Owner                 owner = job.owner;
                owners[owner].done.push_back(std::move(job));
                --owners[owner].busy;
                --busy;
                if (busy == 0) went_quiet.notify_all();
            }
        }
    }
};

MeshPool::MeshPool() : m_impl(new Impl) {}

MeshPool::~MeshPool()
{
    stop();
    delete m_impl;
}

void MeshPool::start(int workers)
{
    Impl &d = *m_impl;
    {
        std::lock_guard<std::mutex> lock(d.mu);
        if (!d.threads.empty()) return;
        d.stopping = false;
    }

    if (workers <= 0) {
        // One fewer than the machine has: the frame thread is one of them and
        // it is not idle while these run -- it is snapshotting the next chunks
        // and drawing. hardware_concurrency answers 0 when it does not know,
        // and one worker is the honest floor.
        const unsigned n = std::thread::hardware_concurrency();
        workers          = (n > 1) ? (int)(n - 1) : 1;
    }

    for (int i = 0; i < workers; ++i) d.threads.emplace_back([&d] { d.worker(); });
}

void MeshPool::stop()
{
    Impl &d = *m_impl;

    // Everything in flight is waited for rather than abandoned: a worker is
    // reading a registry and a snapshot this object owns, and freeing them
    // under it is a crash in whatever ran next.
    drain();

    {
        std::lock_guard<std::mutex> lock(d.mu);
        if (d.threads.empty()) return;
        d.stopping = true;
    }
    d.work_ready.notify_all();

    for (std::thread &t : d.threads)
        if (t.joinable()) t.join();

    std::lock_guard<std::mutex> lock(d.mu);
    d.threads.clear();
    d.queue.clear();
    d.spare.clear();
    d.owners.clear();
    d.busy = 0;
}

int MeshPool::workers() const
{
    std::lock_guard<std::mutex> lock(m_impl->mu);
    return (int)m_impl->threads.size();
}

uint64_t MeshPool::submit(Owner owner, ChunkPos pos, ChunkSnapshot &snapshot,
                          const NodeRegistry &reg)
{
    Impl &d = *m_impl;

    std::unique_lock<std::mutex> lock(d.mu);
    if (d.threads.empty()) return 0;

    Job job   = d.take_spare();
    job.owner = owner;
    job.pos   = pos;
    job.seq   = d.next_seq++;
    job.reg   = &reg;
    std::swap(job.snapshot, snapshot);

    const uint64_t seq = job.seq;
    d.queue.push_back(std::move(job));
    ++d.owners[owner].busy;
    ++d.busy;

    lock.unlock();
    d.work_ready.notify_one();
    return seq;
}

bool MeshPool::try_take(Owner owner, Result &out)
{
    Impl &d = *m_impl;

    std::lock_guard<std::mutex> lock(d.mu);
    const auto                  found = d.owners.find(owner);
    if (found == d.owners.end()) return false;
    OwnerState &st = found->second;

    while (!st.done.empty()) {
        Job job = std::move(st.done.front());
        st.done.pop_front();

        // Stale: a newer mesh of this chunk has already gone out. Recycled
        // rather than returned, so the caller never has to ask whether what it
        // was handed is worth uploading.
        const auto it = st.handed_back.find(job.pos);
        if (it != st.handed_back.end() && job.seq < it->second) {
            d.recycle(std::move(job));
            continue;
        }
        st.handed_back[job.pos] = job.seq;

        // The caller's old buffers go back on the spare pile and the job's come
        // out. Swaps rather than copies of a vertex vector that can hold tens
        // of thousands of entries.
        Job back = d.take_spare();
        std::swap(back.opaque, out.opaque);
        std::swap(back.alpha, out.alpha);
        std::swap(back.parts, out.parts);
        std::swap(back.liquids, out.liquids);
        d.recycle(std::move(back));

        out.pos        = job.pos;
        out.seq        = job.seq;
        out.visibility = job.visibility;
        std::swap(out.opaque, job.opaque);
        std::swap(out.alpha, job.alpha);
        std::swap(out.parts, job.parts);
        std::swap(out.liquids, job.liquids);

        d.recycle(std::move(job));
        return true;
    }
    return false;
}

void MeshPool::drain()
{
    Impl                        &d = *m_impl;
    std::unique_lock<std::mutex> lock(d.mu);

    // `threads.empty()` is not a nicety. Waiting for work that nobody can do is
    // a deadlock, and it was found by a sabotage rather than by reasoning: with
    // submit() made to accept a job on a stopped pool, the gate did not go red,
    // it HUNG -- the destructor's stop() waited for a busy count that no worker
    // would ever bring down, and the run was killed by its timeout with nothing
    // to say. A gate must fail, not hang, and so must the thing it is gating.
    d.went_quiet.wait(lock, [&d] { return d.busy == 0 || d.threads.empty(); });
}

int MeshPool::in_flight() const
{
    std::lock_guard<std::mutex> lock(m_impl->mu);
    return m_impl->busy;
}

int MeshPool::in_flight(Owner owner) const
{
    std::lock_guard<std::mutex> lock(m_impl->mu);
    const auto                  it = m_impl->owners.find(owner);
    return (it == m_impl->owners.end()) ? 0 : it->second.busy;
}

void MeshPool::forget(Owner owner)
{
    // Before the lock, and before the erase: a worker may be part way through a
    // job belonging to this owner, and dropping the owner's state under it
    // leaves the pool owing work nobody will finish -- which the next drain()
    // waits for for ever.
    drain();

    Impl                       &d = *m_impl;
    std::lock_guard<std::mutex> lock(d.mu);
    d.owners.erase(owner);
}

MeshPool &shared_mesh_pool()
{
    static MeshPool pool;
    return pool;
}

} // namespace cnp
