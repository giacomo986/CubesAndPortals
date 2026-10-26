#pragma once

#include "core/net/bytes.h"
#include "core/net/interest.h"
#include "core/voxel/chunk.h"

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

// One chunk, on the wire.
//
// A 32-cubed chunk is 32768 blocks: 65536 bytes of ids plus 32768 of light, and
// a player walking into a fresh area wants hundreds of them. So this is the
// message whose size decides whether the game is playable on a domestic
// connection, and the only part of the protocol where the encoding is worth
// arguing about.
//
// Three things it does, in order of how much they save:
//
//   - **A uniform chunk is a header.** Most of a voxel world is air or solid
//     stone, and the storage already knows that (`Chunk::is_uniform`). Sending
//     one block id instead of 98304 bytes is the whole optimisation, and it is
//     not compression, it is not sending what was never there.
//   - **A palette.** A chunk with nine distinct blocks does not need sixteen
//     bits per block. The ids become indices into a small table, packed to the
//     width the table actually needs.
//   - **zstd** over the result, at the level DESIGN.md 3.2 names.
//
// Whether the palette helps *on top of* zstd is a measurement, not an opinion:
// bit packing destroys the byte alignment a compressor exploits, and it is
// entirely possible for raw + zstd to beat palette + zstd. The codec test
// prints both, and the choice below follows those numbers.
//
// On decoding: the sizes are known before a byte is read, because a chunk is
// always 32 cubed. Nothing here allocates from a length in the message, and the
// decompressor is given a buffer of exactly the size a chunk must be and
// required to fill it exactly.

namespace cnp {
namespace net {

// The bytes a chunk turns into, and what they cost.
struct ChunkEncodeStats {
    size_t blocks_raw    = 0;   // what the ids would have been, uncompressed
    size_t before_zstd   = 0;   // after uniform/palette, before compression
    size_t after_zstd    = 0;   // what goes on the wire, payload only
    bool   uniform       = false;
    bool   palette_used  = false;
    size_t palette_size  = 0;
};

// Appends one encoded chunk to `out`. Returns false only if zstd itself fails,
// which on a buffer this size means the library is broken.
// `allow_palette` exists so the codec test can encode the same chunk both ways
// and print the two sizes. The choice of default is made from those numbers,
// and a switch that lets them be measured is cheaper than a claim that they
// were.
bool encode_chunk(const Chunk &chunk, std::vector<uint8_t> &out, int level = 3,
                  ChunkEncodeStats *stats = nullptr, bool allow_palette = true);

// Reads one chunk. Returns null on anything malformed -- a truncated message, a
// palette index outside the palette, a compressed frame that does not expand to
// exactly one chunk. `err` says which, for the log.
std::unique_ptr<Chunk> decode_chunk(ByteReader &r, std::string &err);

// Encodes the chunks one player should be sent now: asks the interest what it
// wants, encodes until a budget runs out, and marks them sent. Each entry is a
// complete message, ready for channel 1, with its type byte already on it.
//
// Here rather than in the server's main loop because the loopback test needs
// the same behaviour, and a second copy of "which chunks, in what order, up to
// what budget" is a second thing to get wrong.
size_t collect_chunk_messages(const World &world, PlayerInterest &interest,
                              std::vector<std::vector<uint8_t>> &out,
                              const std::function<bool(ChunkPos)> &sendable = {},
                              double now = 0.0);

// Writes the chunk into a world, replacing whatever was there. Separate from
// decoding so a client can decide -- a chunk that arrives for a position it no
// longer cares about is dropped without touching the world.
void install_chunk(World &world, std::unique_ptr<Chunk> chunk);

} // namespace net
} // namespace cnp
