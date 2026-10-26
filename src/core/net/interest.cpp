#include "core/net/interest.h"

#include <algorithm>
#include <cmath>
#include <unordered_map>

namespace cnp {
namespace net {

void PlayerInterest::set_position(float x, float y, float z, float look_x, float look_y,
                                  float look_z)
{
    m_x = x;
    m_y = y;
    m_z = z;

    const float len = std::sqrt(look_x * look_x + look_y * look_y + look_z * look_z);
    if (len > 1e-6f) {
        m_look_x = look_x / len;
        m_look_y = look_y / len;
        m_look_z = look_z / len;
    }
}

ChunkPos PlayerInterest::centre() const
{
    return World::chunk_of((int)std::floor(m_x), (int)std::floor(m_y), (int)std::floor(m_z));
}

std::vector<ChunkPos> PlayerInterest::window_centres() const
{
    std::vector<ChunkPos> out;
    for (const Window &w : m_windows)
        out.push_back(
            World::chunk_of((int)std::floor(w.x), (int)std::floor(w.y), (int)std::floor(w.z)));
    return out;
}

namespace {

// From a point to the nearest point of a chunk, in blocks: zero inside it.
float to_chunk(ChunkPos p, float x, float y, float z)
{
    const float lo_x = (float)(p.x * kChunkSize);
    const float lo_y = (float)(p.y * kChunkSize);
    const float lo_z = (float)(p.z * kChunkSize);
    const float hi   = (float)kChunkSize;

    const float ox = std::min(std::max(x, lo_x), lo_x + hi) - x;
    const float oy = std::min(std::max(y, lo_y), lo_y + hi) - y;
    const float oz = std::min(std::max(z, lo_z), lo_z + hi) - z;
    return std::sqrt(ox * ox + oy * oy + oz * oz);
}

} // namespace

std::vector<ChunkPos> PlayerInterest::next(const World                         &world,
                                           const std::function<bool(ChunkPos)> &sendable) const
{
    const ChunkPos c = centre();

    struct Candidate {
        ChunkPos pos;
        float    priority;   // lower is sooner
    };
    std::vector<Candidate> candidates;

    for (int dz = -m_config.radius; dz <= m_config.radius; ++dz) {
        for (int dy = -m_config.vertical_radius; dy <= m_config.vertical_radius; ++dy) {
            for (int dx = -m_config.radius; dx <= m_config.radius; ++dx) {
                const ChunkPos p{c.x + dx, c.y + dy, c.z + dz};
                if (m_sent.count(p) != 0) continue;
                if (world.find_chunk(p) == nullptr) continue;   // nothing generated there
                if (sendable && !sendable(p)) continue;         // not lit yet

                // Distance to the nearest point of the chunk, not to its
                // middle. Measuring to the middle put the chunk the player is
                // standing in behind three of its neighbours whenever they
                // stood near a corner, because from there four centres are
                // equidistant -- and the chunk under your feet arriving fourth
                // is the difference between spawning on the ground and spawning
                // in a hole. From the nearest point it is zero, always.
                const float lo_x = (float)(p.x * kChunkSize);
                const float lo_y = (float)(p.y * kChunkSize);
                const float lo_z = (float)(p.z * kChunkSize);
                const float hi   = (float)kChunkSize;

                const float nx = std::min(std::max(m_x, lo_x), lo_x + hi);
                const float ny = std::min(std::max(m_y, lo_y), lo_y + hi);
                const float nz = std::min(std::max(m_z, lo_z), lo_z + hi);

                const float ox = nx - m_x, oy = ny - m_y, oz = nz - m_z;
                const float distance = std::sqrt(ox * ox + oy * oy + oz * oz);

                // The facing bias uses the direction to the middle, because the
                // nearest point of the chunk you are inside has no direction at
                // all.
                const float cx = lo_x + hi * 0.5f - m_x;
                const float cy = lo_y + hi * 0.5f - m_y;
                const float cz = lo_z + hi * 0.5f - m_z;
                const float clen = std::sqrt(cx * cx + cy * cy + cz * cz);

                // In front comes first. The bias is a fraction of the distance
                // rather than a fixed number of blocks, so it keeps meaning the
                // same thing at every radius: a chunk behind you is worth about
                // one and a half of the same distance in front.
                float facing = 0.0f;
                if (clen > 1e-3f)
                    facing = (cx * m_look_x + cy * m_look_y + cz * m_look_z) / clen;

                // The chunk the player is standing in goes first, always. It is
                // not enough for it to be nearest: a player standing exactly on
                // a chunk boundary -- which is where they are every time they
                // cross one -- is at distance zero from as many as four chunks,
                // and the tie-break then decides by coordinate, which can put
                // the ground under their feet fourth. Found by a test that put
                // a player on x=64 with 32-block chunks.
                const float priority = (p == c) ? -1.0f : distance * (1.25f - 0.25f * facing);
                candidates.push_back(Candidate{p, priority});
            }
        }
    }

    // And round every window, each chunk once: one the player's own cube
    // already has keeps the place it has there.
    std::unordered_set<ChunkPos, ChunkPosHash> listed;
    for (const Candidate &k : candidates) listed.insert(k.pos);
    for (const Window &w : m_windows) {
        const ChunkPos wc = World::chunk_of((int)std::floor(w.x), (int)std::floor(w.y),
                                            (int)std::floor(w.z));
        const int r = m_config.window_radius;
        for (int dz = -r; dz <= r; ++dz) {
            for (int dy = -m_config.vertical_radius; dy <= m_config.vertical_radius; ++dy) {
                for (int dx = -r; dx <= r; ++dx) {
                    const ChunkPos p{wc.x + dx, wc.y + dy, wc.z + dz};
                    if (m_sent.count(p) != 0 || !listed.insert(p).second) continue;
                    if (world.find_chunk(p) == nullptr) continue;
                    if (sendable && !sendable(p)) continue;
                    candidates.push_back(Candidate{p, w.reach + to_chunk(p, w.x, w.y, w.z)});
                }
            }
        }
    }

    // Sorted by priority, and by coordinates where priorities tie. The
    // tie-break is not cosmetic: without it the order depends on how the loop
    // happened to visit them, and two servers streaming the same world to the
    // same player would send it in different orders -- which is the kind of
    // thing that makes a bug reproduce on one machine and not the other.
    std::sort(candidates.begin(), candidates.end(), [](const Candidate &a, const Candidate &b) {
        if (a.priority != b.priority) return a.priority < b.priority;
        if (a.pos.x != b.pos.x) return a.pos.x < b.pos.x;
        if (a.pos.y != b.pos.y) return a.pos.y < b.pos.y;
        return a.pos.z < b.pos.z;
    });

    std::vector<ChunkPos> out;
    const size_t          budget = (size_t)std::max(0, m_config.chunks_per_tick);
    for (size_t i = 0; i < candidates.size() && out.size() < budget; ++i)
        out.push_back(candidates[i].pos);
    return out;
}

size_t PlayerInterest::forget_distant()
{
    const ChunkPos c = centre();

    const int keep   = m_config.radius + m_config.forget_margin;
    const int keep_y = m_config.vertical_radius + m_config.forget_margin;

    const std::vector<ChunkPos> windows = window_centres();
    const int                   keep_w  = m_config.window_radius + m_config.forget_margin;
    auto near_a_window = [&](ChunkPos p) {
        for (const ChunkPos &w : windows)
            if (std::abs(p.x - w.x) <= keep_w && std::abs(p.z - w.z) <= keep_w &&
                std::abs(p.y - w.y) <= keep_y)
                return true;
        return false;
    };

    size_t dropped = 0;
    for (auto it = m_sent.begin(); it != m_sent.end();) {
        const ChunkPos &p  = it->first;
        const int       dx = std::abs(p.x - c.x);
        const int       dy = std::abs(p.y - c.y);
        const int       dz = std::abs(p.z - c.z);

        if ((dx > keep || dz > keep || dy > keep_y) && !near_a_window(p)) {
            m_gone.push_back(p);
            it = m_sent.erase(it);
            ++dropped;
        } else {
            ++it;
        }
    }
    return dropped;
}

bool PlayerInterest::forget(ChunkPos p)
{
    return m_sent.erase(p) > 0;
}

bool PlayerInterest::drop(ChunkPos p)
{
    if (m_sent.erase(p) == 0) return false;
    m_gone.push_back(p);
    return true;
}

std::vector<ChunkPos> PlayerInterest::take_gone()
{
    std::vector<ChunkPos> out;
    out.swap(m_gone);
    return out;
}

void split_owed_cells(const std::vector<CellPos> &cells, size_t worth_a_chunk,
                      std::vector<ChunkPos> &resend, std::vector<CellPos> &one_by_one)
{
    resend.clear();
    one_by_one.clear();

    // Counted first, then the cells walked again in the order they happened.
    // Two passes and not one, because what to do with a chunk is not known
    // until every cell has been seen: a version that decided as it went would
    // send the first hundred and fifty cells of a chunk and then resend the
    // whole chunk behind them, which is the worst of both.
    std::unordered_map<ChunkPos, size_t, ChunkPosHash> per_chunk;
    for (const CellPos &c : cells) ++per_chunk[World::chunk_of(c.x, c.y, c.z)];

    std::unordered_set<ChunkPos, ChunkPosHash> named;
    for (const CellPos &c : cells) {
        const ChunkPos p = World::chunk_of(c.x, c.y, c.z);
        if (per_chunk[p] >= worth_a_chunk) {
            // Named once, at its first cell, so the order out is the order the
            // changes happened in and a byte count is reproducible.
            if (named.insert(p).second) resend.push_back(p);
            continue;
        }
        one_by_one.push_back(c);
    }
}

} // namespace net
} // namespace cnp
