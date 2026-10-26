#pragma once

#include "core/voxel/chunk.h"

#include <cstdint>
#include <functional>
#include <unordered_map>
#include <unordered_set>
#include <vector>

// Which chunks a player is entitled to, and in what order they should be sent.
//
// DESIGN.md 3.3: "interest management: per giocatore, raggio in chunk +
// priorità per direzione dello sguardo. Un portale aggiunge un secondo centro
// di interesse." The second centres are windows: the far end of each opening
// near the player, smaller than the player's own cube and sent after the chunks
// around the player that are as near (see set_windows).
//
// No socket, no world state beyond "does this chunk exist". A test can walk a
// player across a world and check what it would have been sent, which is the
// only way any of this gets checked at all -- over a real network the same
// question needs two processes, a player moving, and a way to watch the queue.
//
// Two rules that are easy to get wrong and expensive to get wrong:
//
//   - **Sent once.** A chunk already sent is not sent again until it leaves
//     the radius, or a player standing still costs bandwidth forever.
//   - **Forgotten on leaving, with hysteresis.** A player walking along the
//     boundary would otherwise resend the same chunk every step. The forget
//     radius is deliberately larger than the send radius.

namespace cnp {
namespace net {

struct InterestConfig {
    // In chunks. Four is 128 blocks of horizontal view at 32-block chunks,
    // which is the draw distance the client already uses.
    int radius = 4;

    // Worlds are wider than they are tall, and a player 128 blocks above the
    // ground wants what is below them, not another 128 blocks of sky.
    int vertical_radius = 2;

    // The forget radius, as chunks beyond `radius`. Hysteresis: without it a
    // player pacing across a chunk boundary resends a chunk per step.
    int forget_margin = 2;

    // Round the far end of an opening near the player, in chunks: 64 blocks of
    // what can be seen through it. Its height is vertical_radius's.
    int window_radius = 2;

    // How near an opening has to be, in blocks, for its far end to be owed.
    // The player's own radius: an opening further off is not drawn with
    // anything worth streaming behind it.
    float window_reach = 128.0f;

    // How many chunks may be sent to one player in one tick. A budget rather
    // than a queue drain: a player who teleports should not take the tick with
    // them. 4 chunks of typical size is a few kilobytes; 4 worst-case chunks
    // is 360 KB, which is why the sender also has a byte budget.
    int chunks_per_tick = 4;

    // And the byte budget, checked against what the encoder actually produced
    // rather than against an estimate.
    size_t bytes_per_tick = 128 * 1024;
};

// A cell somebody is owed, in block coordinates.
struct CellPos {
    int32_t x = 0, y = 0, z = 0;
};

// How many changed cells in ONE chunk stop being worth sending one at a time.
//
// A cell change is seventeen bytes on this wire; a chunk of a world like the
// one the gate builds is a couple of hundred, because a chunk is compressed and
// gets RELATIVELY cheaper the more of it changed. Measured in chunk_codec:
// four hundred cells cost 6834 bytes as cells and 233 as the chunk. A hundred
// and fifty is where the two cross for a chunk of typical size (README: 3.00 MB
// of world becomes 0.01 MB), and it is the direction of the error that decides
// the exact number matters little -- below the bar the cells also spare the
// client a mesh rebuild, above it the chunk only gets cheaper.
inline constexpr size_t kCellsWorthAChunk = 150;

// Splits what one session is owed into the chunks worth resending whole and the
// cells still worth sending one at a time.
//
// Order is kept for the cells: a session owed changes to the same cell twice
// gets them in the order they happened, and the last one is what it ends up
// with. The chunks come out in the order their first cell appeared, for the
// same reason -- a stable order is what makes a byte count reproducible.
void split_owed_cells(const std::vector<CellPos> &cells, size_t worth_a_chunk,
                      std::vector<ChunkPos> &resend, std::vector<CellPos> &one_by_one);

// One player's view of what they have. Owned by whatever owns the connection.
class PlayerInterest {
public:
    explicit PlayerInterest(InterestConfig config = {}) : m_config(config) {}

    // Where the player is, in blocks, and where they are looking. The look
    // direction only orders the queue; it never decides what is in it, because
    // a player who spins around should not have to wait for the world behind
    // them to arrive.
    void set_position(float x, float y, float z, float look_x, float look_y, float look_z);

    // The far ends of the openings near the player, and how far the player is
    // from the near end of each. A window's chunks are sent in order of that
    // distance plus their own from the far end -- a chunk beyond an opening
    // ten blocks away is ten blocks further than it would be standing in it --
    // and kept while the window lasts. Replaced whole on every call.
    struct Window {
        float x = 0.0f, y = 0.0f, z = 0.0f;   // the far end
        float reach = 0.0f;                    // blocks from the player to the near end
    };
    void set_windows(std::vector<Window> windows) { m_windows = std::move(windows); }
    const std::vector<Window> &windows() const { return m_windows; }

    // Which way the owner of this interest last said it was facing, as the
    // vector it sent (ecs::Look keeps it as yaw and pitch).
    void look(float &x, float &y, float &z) const { x = m_look_x; y = m_look_y; z = m_look_z; }

    // The next chunks to send, most important first, at most the configured
    // budget. Only chunks that exist in `world` -- a client is never told about
    // a chunk the server has not generated -- and, when `sendable` is given,
    // only those it allows: a server sends a chunk once its column is lit.
    std::vector<ChunkPos> next(const World &world,
                               const std::function<bool(ChunkPos)> &sendable = {}) const;

    void mark_sent(ChunkPos p, double when = 0.0) { m_sent[p] = when; }
    bool was_sent(ChunkPos p) const { return m_sent.count(p) != 0; }

    // When it was, in the server's seconds; negative when it was not. A chunk
    // sent a moment ago may still be on its way, and a client that drives over
    // where it will be has not got it yet (TODO.md OPEN-47).
    double sent_at(ChunkPos p) const
    {
        const auto it = m_sent.find(p);
        return it == m_sent.end() ? -1.0 : it->second;
    }

    // Drops chunks that have fallen outside the forget radius. Called after
    // moving; returns how many were dropped, which is worth logging when it is
    // large because it means somebody teleported.
    //
    // And remembers them for take_gone: the client is TOLD, so that what it
    // holds is what this set says it holds (TODO.md OPEN-38). The other order
    // -- a client dropping by a radius of its own -- disagrees with this
    // hysteresis the moment the two centres differ by a claim, and a chunk the
    // client dropped that the server still believes it sent is a hole the
    // player walks into.
    size_t forget_distant();

    // Drops a chunk that has left the WORLD, and remembers it for take_gone
    // the same way. Returns whether this session had it.
    bool drop(ChunkPos p);

    // What this session was told it no longer holds since the last call, in
    // the order it happened.
    std::vector<ChunkPos> take_gone();

    // Drops ONE chunk, so the streamer sends it again. Returns whether this
    // session had it at all.
    //
    // forget_distant() decides by distance, which is the wrong question when a
    // mod has just rewritten a chunk under a player standing in it: the cells
    // are near, and near is exactly why they matter. This is the other reason a
    // chunk stops being current -- see split_owed_cells and TODO OPEN-19.
    bool forget(ChunkPos p);

    size_t sent_count() const { return m_sent.size(); }

    ChunkPos centre() const;

    // The chunk of each window's far end.
    std::vector<ChunkPos> window_centres() const;

    const InterestConfig &config() const { return m_config; }
    InterestConfig       &config() { return m_config; }

private:
    InterestConfig m_config;

    float m_x = 0.0f, m_y = 0.0f, m_z = 0.0f;
    float m_look_x = 0.0f, m_look_y = 0.0f, m_look_z = -1.0f;

    std::vector<Window> m_windows;

    std::unordered_map<ChunkPos, double, ChunkPosHash> m_sent;
    std::vector<ChunkPos>                      m_gone;
};

} // namespace net
} // namespace cnp
