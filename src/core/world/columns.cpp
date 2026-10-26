#include "core/world/columns.h"

#include <algorithm>
#include <chrono>
#include <cstdlib>

namespace cnp {

ColumnState WorldColumns::state(ColumnPos c) const
{
    const auto it = m_columns.find(c);
    return (it == m_columns.end()) ? ColumnState::Absent : it->second.state;
}

void WorldColumns::set_state(ColumnPos c, ColumnState s, uint64_t now)
{
    if (s == ColumnState::Absent) {
        m_columns.erase(c);
        return;
    }
    Entry &e  = m_columns[c];
    e.state   = s;
    e.touched = std::max(e.touched, now);
}

void WorldColumns::touch(ColumnPos c, uint64_t now)
{
    const auto it = m_columns.find(c);
    if (it != m_columns.end()) it->second.touched = std::max(it->second.touched, now);
}

std::vector<ChunkPos> WorldColumns::chunks_of(ColumnPos c) const
{
    std::vector<ChunkPos> out;
    for (int y = m_world.lowest_layer(); y <= m_world.highest_layer(); ++y) {
        const ChunkPos p{c.x, y, c.z};
        if (m_world.find_chunk(p) != nullptr) out.push_back(p);
    }
    return out;
}

void WorldColumns::fill(ColumnPos c, uint64_t now)
{
    // Whatever is already there did not come from the generator or the file.
    // A relight whose box reached past the lit columns writes light into cells
    // nobody generated, and writing light makes the chunk; left in place, those
    // chunks would be sky sitting inside the terrain about to be laid over it.
    for (const ChunkPos p : chunks_of(c)) m_world.erase_chunk(p);

    if (m_hooks.fill) m_hooks.fill(c);

    Entry &e  = m_columns[c];
    e.state   = ColumnState::Blocks;
    e.touched = std::max(e.touched, now);
    ++filled_total;
}

bool WorldColumns::step(ColumnPos c, uint64_t now)
{
    if (lit(c)) return false;

    for (int dz = -1; dz <= 1; ++dz) {
        for (int dx = -1; dx <= 1; ++dx) {
            const ColumnPos n{c.x + dx, c.z + dz};
            if (state(n) != ColumnState::Absent) continue;
            fill(n, now);
            return true;
        }
    }

    if (m_hooks.light) m_hooks.light(c);
    Entry &e  = m_columns[c];
    e.state   = ColumnState::Lit;
    e.touched = std::max(e.touched, now);
    ++lit_total;
    return true;
}

void WorldColumns::make_lit(ColumnPos c, uint64_t now)
{
    while (step(c, now)) {
    }
}

void WorldColumns::ensure_blocks(ColumnPos c, uint64_t now)
{
    if (state(c) == ColumnState::Absent) fill(c, now);
}

WorldColumns::Advance WorldColumns::advance(const std::vector<ColumnPos> &want, double budget_ms,
                                            uint64_t now)
{
    Advance    out;
    const auto t0    = std::chrono::steady_clock::now();
    int        steps = 0;

    for (const ColumnPos c : want) {
        while (!lit(c)) {
            const double spent =
                std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0)
                    .count();
            if (steps > 0 && spent >= budget_ms) {
                out.behind = true;
                return out;
            }

            const ColumnState before = state(c);
            step(c, now);
            ++steps;
            if (lit(c) && before != ColumnState::Lit) ++out.lit;
            else ++out.filled;
        }
    }
    return out;
}

std::vector<ColumnPos> WorldColumns::idle(const std::vector<ColumnPos> &centres, int radius,
                                          uint64_t since) const
{
    std::vector<ColumnPos> out;
    for (const auto &kv : m_columns) {
        if (kv.second.touched >= since) continue;

        bool near = false;
        for (const ColumnPos &c : centres) {
            if (std::abs(kv.first.x - c.x) <= radius && std::abs(kv.first.z - c.z) <= radius) {
                near = true;
                break;
            }
        }
        if (!near) out.push_back(kv.first);
    }

    std::sort(out.begin(), out.end(), [](const ColumnPos &a, const ColumnPos &b) {
        return (a.z != b.z) ? a.z < b.z : a.x < b.x;
    });
    return out;
}

void WorldColumns::unload(const std::vector<ColumnPos> &columns)
{
    if (columns.empty()) return;
    if (m_hooks.release) m_hooks.release(columns);

    for (const ColumnPos c : columns) {
        for (const ChunkPos p : chunks_of(c)) m_world.erase_chunk(p);
        if (m_columns.erase(c) > 0) ++unloaded_total;
    }
}

std::vector<ColumnPos> WorldColumns::lit_columns() const
{
    std::vector<ColumnPos> out;
    for (const auto &kv : m_columns)
        if (kv.second.state == ColumnState::Lit) out.push_back(kv.first);
    std::sort(out.begin(), out.end(), [](const ColumnPos &a, const ColumnPos &b) {
        return (a.z != b.z) ? a.z < b.z : a.x < b.x;
    });
    return out;
}

size_t WorldColumns::lit_count() const
{
    size_t n = 0;
    for (const auto &kv : m_columns)
        if (kv.second.state == ColumnState::Lit) ++n;
    return n;
}

std::vector<ColumnPos> columns_near(const std::vector<ColumnPos> &centres, int radius)
{
    struct Candidate {
        ColumnPos pos;
        int       distance;   // squared, in columns, to the nearest centre
    };

    std::unordered_map<ColumnPos, int, ColumnPosHash> nearest;
    for (const ColumnPos &c : centres) {
        for (int dz = -radius; dz <= radius; ++dz) {
            for (int dx = -radius; dx <= radius; ++dx) {
                const ColumnPos p{c.x + dx, c.z + dz};
                const int       d = dx * dx + dz * dz;
                const auto      it = nearest.find(p);
                if (it == nearest.end() || d < it->second) nearest[p] = d;
            }
        }
    }

    std::vector<Candidate> all;
    all.reserve(nearest.size());
    for (const auto &kv : nearest) all.push_back(Candidate{kv.first, kv.second});

    std::sort(all.begin(), all.end(), [](const Candidate &a, const Candidate &b) {
        if (a.distance != b.distance) return a.distance < b.distance;
        if (a.pos.z != b.pos.z) return a.pos.z < b.pos.z;
        return a.pos.x < b.pos.x;
    });

    std::vector<ColumnPos> out;
    out.reserve(all.size());
    for (const Candidate &c : all) out.push_back(c.pos);
    return out;
}

} // namespace cnp
