#include "core/world/modifiers.h"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdlib>

namespace cnp {
namespace {

double seconds_since(const std::chrono::steady_clock::time_point &t0)
{
    return std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
}

} // namespace

bool modifier_fires(uint32_t seed, uint64_t sweep, int x, int y, int z, int chance)
{
    if (chance <= 1) return true;

    // A 64-bit mix of the five numbers. Written out rather than taken from a
    // library for the reason the ore stage writes its own: this has to give the
    // same answer on every machine and every build, and a std:: generator's
    // sequence is not that.
    uint64_t h = 0x9E3779B97F4A7C15ull;
    const uint64_t parts[5] = {(uint64_t)seed, sweep, (uint64_t)(int64_t)x, (uint64_t)(int64_t)y,
                               (uint64_t)(int64_t)z};
    for (const uint64_t p : parts) {
        h ^= p + 0x9E3779B97F4A7C15ull + (h << 6) + (h >> 2);
        h *= 0xBF58476D1CE4E5B9ull;
        h ^= h >> 31;
    }

    return (h % (uint64_t)chance) == 0;
}

bool Modifiers::add(Modifier m, bool is_abm, std::string &error)
{
    error.clear();

    if (m_closed) {
        error = "registration is closed: a modifier is asked for while the mods load, and "
                "this was asked for after the world started";
        return false;
    }
    if (m.name.empty()) {
        error = "a modifier with no name";
        return false;
    }
    for (const Modifier &other : m_abms) {
        if (other.name == m.name) {
            error = "'" + m.name + "' is already registered";
            return false;
        }
    }
    for (const Modifier &other : m_lbms) {
        if (other.name == m.name) {
            error = "'" + m.name + "' is already registered";
            return false;
        }
    }
    if (m.node_names.empty()) {
        error = "'" + m.name + "' names no blocks to run on";
        return false;
    }
    if (is_abm) {
        if (!(m.interval > 0.0)) {
            error = "'" + m.name + "' asks for an interval of " + std::to_string(m.interval) +
                    " seconds";
            return false;
        }
        if (m.chance < 1) {
            error = "'" + m.name + "' asks for a chance of one in " + std::to_string(m.chance);
            return false;
        }
    }

    if (is_abm) {
        m_abms.push_back(std::move(m));
        Sweep s;
        s.due_at = 0.0;   // the first sweep is due at once
        m_sweeps.push_back(std::move(s));
    } else {
        m_lbms.push_back(std::move(m));
    }
    return true;
}

bool Modifiers::register_abm(Modifier m, std::string &error)
{
    return add(std::move(m), true, error);
}

bool Modifiers::register_lbm(Modifier m, std::string &error)
{
    return add(std::move(m), false, error);
}

void Modifiers::close_registration(const NodeRegistry &reg, std::vector<std::string> &errors)
{
    m_closed = true;

    const auto resolve = [&](Modifier &m) {
        m.nodes.clear();
        m.mask = {};
        for (const std::string &name : m.node_names) {
            const BlockId id = reg.find(name);
            if (id == kAir && name != "core:air") {
                // Disabled and NAMED. A modifier whose block does not exist
                // would otherwise run for ever over cells that can never match,
                // which costs a sweep and looks like a mod that does nothing.
                m.enabled = false;
                errors.push_back("'" + m.name + "' runs on '" + name +
                                 "', which no mod registered");
                continue;
            }
            m.nodes.push_back(id);
            m.mask.add(id);
        }
        if (m.nodes.empty()) m.enabled = false;
    };

    for (Modifier &m : m_abms) resolve(m);
    for (Modifier &m : m_lbms) resolve(m);
}

void Modifiers::chunk_loaded(ChunkPos p)
{
    // A sweep under way visits it too, at its end, rather than leaving it for
    // the next sweep -- an interval later -- as it did (OPEN-42).
    for (Sweep &s : m_sweeps)
        if (s.running && std::find(s.order.begin(), s.order.end(), p) == s.order.end())
            s.order.push_back(p);

    if (m_lbms.empty()) return;
    m_fresh.push_back(p);
}

void Modifiers::disable(const std::string &name, const std::string &why)
{
    for (std::vector<Modifier> *list : {&m_abms, &m_lbms}) {
        for (Modifier &m : *list) {
            if (m.name != name) continue;
            m.enabled = false;
            (void)why;
        }
    }
}

int Modifiers::behind() const
{
    int n = 0;
    for (const Sweep &s : m_sweeps) {
        if (s.running) ++n;
    }
    return n;
}

ModifierStepStats Modifiers::step(World &world, double now, double budget_ms, const Fire &fire)
{
    ModifierStepStats out;
    if (!m_closed) return out;
    if (m_abms.empty() && m_fresh.empty()) return out;

    const auto   t0      = std::chrono::steady_clock::now();
    const double budget  = budget_ms / 1000.0;
    int          counter = 0;

    const auto spent = [&]() {
        if (++counter < kModifierClockEvery) return false;
        counter = 0;
        if (seconds_since(t0) < budget) return false;
        out.out_of_budget = true;
        return true;
    };

    // The loading modifiers first, and in the order the chunks arrived. A cell
    // an LBM is meant to fix must not be run over by an ABM before the LBM has
    // seen it, which is the one ordering between the two kinds that matters.
    //
    // One LBM over one chunk at least, every step, and never one it already
    // finished. The budget used to be asked AFTER each LBM and the chunk left
    // at the front of the queue if it was spent -- to run every LBM over it
    // again next step, even when the last of them had just finished it. So a
    // chunk that cost more than the budget on its own (a slow machine, a Debug
    // build, another process on the core) was the front of the queue for ever:
    // the same chunk scanned every tick, no other chunk's loading modifiers
    // run, and no active modifier run at all, because the step was always out
    // of budget before it reached them. Found as a world that came back
    // different across a restart, but only with the rest of the suite running
    // beside it: moss the first run grew was scraped off on load and never grew
    // back. Now the step asks before starting an LBM, not after, and a chunk
    // resumes at the LBM it had reached.
    bool ran_one = false;
    while (!m_fresh.empty()) {
        if (out.out_of_budget) break;

        const ChunkPos p     = m_fresh.front();
        const Chunk   *chunk = world.find_chunk(p);
        if (chunk == nullptr) {
            // Unloaded again before its turn came. Nothing to do and nothing
            // to say: a chunk that is gone is not a chunk with cells in it.
            m_fresh.erase(m_fresh.begin());
            m_fresh_lbm = 0;
            continue;
        }

        bool finished = true;
        for (; m_fresh_lbm < m_lbms.size(); ++m_fresh_lbm) {
            if (ran_one && seconds_since(t0) >= budget) {
                out.out_of_budget = true;
                finished          = false;
                break;
            }
            ran_one = true;

            Modifier &m = m_lbms[m_fresh_lbm];
            if (!m.enabled) continue;
            if (!chunk->ids().meets(m.mask)) {
                ++out.chunks_skipped;
                continue;
            }
            ++out.chunks_looked;

            const auto m0 = std::chrono::steady_clock::now();
            for (int i = 0; i < kChunkVolume; ++i) {
                const int lx = i % kChunkSize;
                const int ly = (i / kChunkSize) % kChunkSize;
                const int lz = i / (kChunkSize * kChunkSize);

                const BlockId id = chunk->get(lx, ly, lz);
                ++m.tested;
                ++out.cells_tested;
                if (std::find(m.nodes.begin(), m.nodes.end(), id) == m.nodes.end()) continue;

                const int wx = p.x * kChunkSize + lx;
                const int wy = p.y * kChunkSize + ly;
                const int wz = p.z * kChunkSize + lz;

                ++m.fired;
                ++out.actions_run;
                if (!fire(m, wx, wy, wz, id)) {
                    m.enabled = false;
                    break;
                }
            }
            m.seconds += seconds_since(m0);
        }

        if (!finished) break;
        m_fresh.erase(m_fresh.begin());
        m_fresh_lbm = 0;
    }

    // And a step that spent its budget on loading modifiers has nothing left
    // for the active ones, the same as before -- said as a flag and not a clock
    // read, so a step that finished the queue exactly at the budget still does.
    if (!out.out_of_budget && seconds_since(t0) >= budget) out.out_of_budget = true;

    // And the active ones. Each has a sweep of its own with its own place in
    // the world, so a slow one does not hold the others back beyond the budget
    // they share.
    for (size_t i = 0; i < m_abms.size() && !out.out_of_budget; ++i) {
        Modifier &m = m_abms[i];
        Sweep    &s = m_sweeps[i];

        if (!m.enabled) continue;
        if (!s.running) {
            if (now < s.due_at) continue;

            // A sweep walks the chunks that were loaded when it started, in a
            // fixed order -- nearest a player first, then by position -- and
            // those that arrive while it runs after them (chunk_loaded). One
            // that leaves is skipped when its turn comes.
            s.order = world.chunk_positions_sorted();
            if (m_near) {
                const std::vector<ChunkPos> near = m_near();
                if (!near.empty()) {
                    auto dist = [&near](ChunkPos p) {
                        int best = INT32_MAX;
                        for (const ChunkPos &n : near)
                            best = std::min(best, std::max({std::abs(p.x - n.x), std::abs(p.y - n.y),
                                                             std::abs(p.z - n.z)}));
                        return best;
                    };
                    std::vector<std::pair<int, ChunkPos>> keyed;
                    keyed.reserve(s.order.size());
                    for (const ChunkPos &p : s.order) keyed.emplace_back(dist(p), p);
                    std::stable_sort(keyed.begin(), keyed.end(),
                                     [](const auto &a, const auto &b) { return a.first < b.first; });
                    for (size_t k = 0; k < keyed.size(); ++k) s.order[k] = keyed[k].second;
                }
            }
            s.at_chunk = 0;
            s.at_cell  = 0;
            s.running  = true;
            ++s.number;
        }

        const auto m0 = std::chrono::steady_clock::now();

        while (s.at_chunk < s.order.size()) {
            const ChunkPos p     = s.order[s.at_chunk];
            const Chunk   *chunk = world.find_chunk(p);

            if (chunk == nullptr || (m_active && !m_active(p)) ||
                !chunk->ids().meets(m.mask)) {
                if (chunk != nullptr) ++out.chunks_skipped;
                ++s.at_chunk;
                s.at_cell = 0;
                continue;
            }
            if (s.at_cell == 0) ++out.chunks_looked;

            bool interrupted = false;
            for (; s.at_cell < kChunkVolume; ++s.at_cell) {
                const int lx = s.at_cell % kChunkSize;
                const int ly = (s.at_cell / kChunkSize) % kChunkSize;
                const int lz = s.at_cell / (kChunkSize * kChunkSize);

                const BlockId id = chunk->get(lx, ly, lz);
                ++m.tested;
                ++out.cells_tested;

                if (std::find(m.nodes.begin(), m.nodes.end(), id) != m.nodes.end()) {
                    const int wx = p.x * kChunkSize + lx;
                    const int wy = p.y * kChunkSize + ly;
                    const int wz = p.z * kChunkSize + lz;

                    if (modifier_fires(m_seed, s.number, wx, wy, wz, m.chance)) {
                        ++m.fired;
                        ++out.actions_run;
                        if (!fire(m, wx, wy, wz, id)) {
                            m.enabled = false;
                            interrupted = true;
                            break;
                        }
                    }
                }

                if (spent()) {
                    interrupted = true;
                    ++s.at_cell;   // resume after this cell, not on it again
                    break;
                }
            }

            if (interrupted) break;

            ++s.at_chunk;
            s.at_cell = 0;
        }

        m.seconds += seconds_since(m0);

        if (s.at_chunk >= s.order.size()) {
            // The sweep finished, so the interval starts counting from now.
            // From NOW and not from when it was due: an interval measured from
            // the due time would try to catch up, and a sweep that cannot keep
            // up would then run continuously.
            s.running = false;
            s.order.clear();
            s.due_at = now + m.interval;
            ++out.abms_swept;
        }
    }

    out.seconds = seconds_since(t0);
    return out;
}

} // namespace cnp
