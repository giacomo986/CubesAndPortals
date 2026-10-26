#include "core/storage/column_keeper.h"

#include "core/net/chunk_codec.h"
#include "core/voxel/light.h"

#include <algorithm>
#include <chrono>
#include <cstdio>

namespace cnp {
namespace {

double ms_since(std::chrono::steady_clock::time_point t0)
{
    return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0)
        .count();
}

bool column_before(const ColumnPos &a, const ColumnPos &b)
{
    return (a.z != b.z) ? a.z < b.z : a.x < b.x;
}

} // namespace

ColumnKeeper::ColumnKeeper(World &world, const NodeRegistry &registry, int height)
    : m_world(world), m_registry(registry), m_height(height), m_columns(world)
{
    m_columns.set_hooks(WorldColumns::Hooks{
        [this](ColumnPos c) { fill(c); },
        [this](ColumnPos c) { light(c); },
        [this](const std::vector<ColumnPos> &c) { release(c); },
    });
}

bool ColumnKeeper::takes_writes(int x, int z) const
{
    const ColumnState st = m_columns.state(column_of_cell(x, z));
    return st == ColumnState::Lit || (m_loading && st == ColumnState::Blocks);
}

void ColumnKeeper::defer(const PendingWrite &w)
{
    m_pending[column_of_cell(w.x, w.z)].push_back(w);
}

void ColumnKeeper::wrote(int x, int z, uint64_t now)
{
    const ColumnPos c = column_of_cell(x, z);
    m_columns.touch(c, now);

    // And the blocks the relight this write is about to ask for will read.
    //
    // A relight reaches fifteen blocks and a bit past the cell, and a column
    // that was let go reads as air: sunlight pours in from the side and is
    // written into the border of this column, saved that way, and not put right
    // until something relights it with its neighbour back. That is a lit column
    // whose ring went idle -- a mod or a modifier still writing somewhere
    // nobody is, thirty seconds after the column was made.
    //
    // So the columns that box reaches get their blocks back first. The box
    // spans at most two columns on each axis, so its four corners name them
    // all. They are filled and not lit, which is all light needs to read, and
    // they go idle and are let go again on their own.
    const int r = kMaxLight + 1;
    const ColumnPos reach[4] = {
        column_of_cell(x - r, z - r), column_of_cell(x + r, z - r),
        column_of_cell(x - r, z + r), column_of_cell(x + r, z + r),
    };
    for (const ColumnPos n : reach) {
        if (n == c || m_columns.state(n) != ColumnState::Absent) continue;
        m_columns.ensure_blocks(n, now);
        ++m_light_reach;
    }
}

std::vector<ColumnPos> ColumnKeeper::pending_columns() const
{
    std::vector<ColumnPos> out;
    out.reserve(m_pending.size());
    for (const auto &kv : m_pending) out.push_back(kv.first);
    std::sort(out.begin(), out.end(), column_before);
    return out;
}

size_t ColumnKeeper::pending_writes() const
{
    size_t n = 0;
    for (const auto &kv : m_pending) n += kv.second.size();
    return n;
}

void ColumnKeeper::mark_generated(ColumnPos c, uint64_t now)
{
    m_columns.set_state(c, ColumnState::Blocks, now);
}

size_t ColumnKeeper::lay_stored(ColumnPos c, std::string &err)
{
    if (db() == nullptr) return 0;

    std::vector<std::unique_ptr<Chunk>> stored = m_db->load_column(c.x, c.z, err);
    if (stored.empty()) return 0;

    for (const ChunkPos p : m_columns.chunks_of(c)) m_world.erase_chunk(p);
    for (std::unique_ptr<Chunk> &chunk : stored) {
        const ChunkPos p = chunk->pos();
        net::install_chunk(m_world, std::move(chunk));
        if (Chunk *in = m_world.find_chunk(p)) in->set_saved();
    }
    m_from_store.insert(c);
    return stored.size();
}

void ColumnKeeper::fill(ColumnPos c)
{
    const auto t0 = std::chrono::steady_clock::now();

    // Kept first: what happened in a column is not what the generator says.
    std::vector<std::unique_ptr<Chunk>> stored;
    if (db() != nullptr) {
        std::string err;
        stored = m_db->load_column(c.x, c.z, err);
        if (!err.empty()) std::printf("world: %s\n", err.c_str());
    } else if (const auto it = m_kept.find(c); it != m_kept.end()) {
        for (const std::vector<uint8_t> &blob : it->second) {
            ByteReader  r(blob.data(), blob.size());
            std::string err;
            if (std::unique_ptr<Chunk> chunk = net::decode_chunk(r, err))
                stored.push_back(std::move(chunk));
        }
    }

    if (!stored.empty()) {
        for (std::unique_ptr<Chunk> &chunk : stored) {
            const ChunkPos p = chunk->pos();
            net::install_chunk(m_world, std::move(chunk));
            if (Chunk *in = m_world.find_chunk(p)) in->set_saved();
        }
        m_from_store.insert(c);
    } else {
        if (m_hooks.generate) m_hooks.generate(c);
        m_from_store.erase(c);
    }

    m_fill_ms += ms_since(t0);
}

void ColumnKeeper::light(ColumnPos c)
{
    const auto t0 = std::chrono::steady_clock::now();
    compute_column_lighting(m_world, m_registry, c.x, c.z, m_height);

    const std::vector<ChunkPos> chunks = m_columns.chunks_of(c);

    // Light is worked out again every time a column is lit, so what that wrote
    // over a copy read back is not a change worth keeping. And with no world
    // file, a column just generated is not one either: from here on, a chunk
    // that is unsaved is a chunk something changed -- a player, a mod, a
    // modifier, whichever way it wrote -- and that is what decides whether the
    // column is kept when it is let go.
    if (m_from_store.count(c) != 0 || db() == nullptr)
        for (const ChunkPos p : chunks)
            if (Chunk *in = m_world.find_chunk(p)) in->set_saved();

    if (m_hooks.loaded) m_hooks.loaded(chunks);
    m_light_ms += ms_since(t0);
}

void ColumnKeeper::release(const std::vector<ColumnPos> &columns)
{
    std::vector<ChunkPos> leaving;
    for (const ColumnPos c : columns) {
        const std::vector<ChunkPos> chunks = m_columns.chunks_of(c);
        leaving.insert(leaving.end(), chunks.begin(), chunks.end());
    }
    if (m_hooks.leaving && !leaving.empty()) m_hooks.leaving(leaving);

    std::vector<ChunkPos> to_save;
    for (const ColumnPos c : columns) {
        const bool lit = m_columns.lit(c);
        m_from_store.erase(c);
        if (!lit) continue;

        const std::vector<ChunkPos> chunks = m_columns.chunks_of(c);

        if (db() != nullptr) {
            for (const ChunkPos p : chunks)
                if (const Chunk *in = m_world.find_chunk(p); in && in->unsaved())
                    to_save.push_back(p);
            continue;
        }

        // In memory, only what something changed since the column was lit (see
        // light()): a column nobody touched is what the generator gives back.
        // One that was kept before and has not changed since is kept as it was.
        bool changed = false;
        for (const ChunkPos p : chunks)
            if (const Chunk *in = m_world.find_chunk(p); in && in->unsaved()) changed = true;
        if (!changed) continue;
        const auto it = m_kept.find(c);

        std::vector<std::vector<uint8_t>> blobs;
        for (const ChunkPos p : chunks) {
            std::vector<uint8_t> blob;
            if (net::encode_chunk(*m_world.find_chunk(p), blob, 3)) blobs.push_back(std::move(blob));
        }
        if (it != m_kept.end())
            for (const std::vector<uint8_t> &b : it->second) m_kept_bytes -= b.size();
        for (const std::vector<uint8_t> &b : blobs) m_kept_bytes += b.size();
        m_kept[c] = std::move(blobs);
    }

    if (!to_save.empty()) {
        std::string err;
        if (!m_db->save_chunks(m_world, to_save, err))
            std::printf("world: could not keep the columns it let go of: %s\n", err.c_str());
    }
}

void ColumnKeeper::make_lit(ColumnPos c, uint64_t now)
{
    m_columns.make_lit(c, now);
}

WorldColumns::Advance ColumnKeeper::advance(const std::vector<ColumnPos> &want, double budget_ms,
                                            uint64_t now)
{
    WorldColumns::Advance out;
    if (!want.empty()) out = m_columns.advance(want, budget_ms, now);
    apply_ready_writes();
    return out;
}

void ColumnKeeper::apply_ready_writes()
{
    if (m_pending.empty()) return;

    std::vector<ColumnPos> ready;
    for (const auto &kv : m_pending)
        if (m_columns.lit(kv.first)) ready.push_back(kv.first);
    std::sort(ready.begin(), ready.end(), column_before);

    for (const ColumnPos c : ready) {
        const std::vector<PendingWrite> writes = std::move(m_pending[c]);
        m_pending.erase(c);
        if (!m_hooks.apply) continue;
        for (const PendingWrite &w : writes) m_hooks.apply(w);
    }
}

void ColumnKeeper::unload_idle(const std::vector<ColumnPos> &centres, int keep, uint64_t idle,
                               uint64_t now)
{
    if (now < idle) return;

    std::vector<ColumnPos> columns = m_columns.idle(centres, keep, now - idle);
    columns.erase(std::remove_if(columns.begin(), columns.end(),
                                 [this](ColumnPos c) { return m_pending.count(c) != 0; }),
                  columns.end());
    if (columns.size() > kUnloadPerCall) columns.resize(kUnloadPerCall);
    m_columns.unload(columns);
}

void ColumnKeeper::relight()
{
    for (const ColumnPos c : m_columns.lit_columns())
        compute_column_lighting(m_world, m_registry, c.x, c.z, m_height);
}

std::vector<ChunkPos> ColumnKeeper::take_unsaved_lit()
{
    std::vector<ChunkPos> changed = m_world.take_unsaved();
    changed.erase(std::remove_if(changed.begin(), changed.end(),
                                 [this](ChunkPos p) { return !m_columns.lit_chunk(p); }),
                  changed.end());
    return changed;
}

} // namespace cnp
