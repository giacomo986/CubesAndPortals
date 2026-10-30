#include "core/net/chunk_codec.h"

#include "core/net/protocol.h"

#include <zstd.h>

#include <cstring>
#include <unordered_map>

namespace cnp {
namespace net {
namespace {

// How the payload was built. On the wire so the decoder does not guess.
enum class Encoding : uint8_t {
    Uniform = 0,   // one block id, one light value, no payload
    Palette = 1,   // a table of ids and packed indices
    Raw     = 2,   // ids as they sit in memory
};

// Set on the encoding byte when the payload carries a facing plane as well.
//
// A FLAG and not a fourth Encoding, so a message without it is byte for byte
// the message this codec produced before facings existed: every chunk already
// saved in a world file has no facings, decodes with none, and is right --
// which is what makes this an additive change to the disk format and not a
// schema bump. The wire still moves 22 -> 23, because an OLD decoder handed a
// flagged byte refuses it as an unknown encoding, and refusing is what the
// version range is for.
constexpr uint8_t kHasFacings = 0x80;

constexpr size_t kBlockBytes  = (size_t)kChunkVolume * sizeof(BlockId);
constexpr size_t kLightBytes  = (size_t)kChunkVolume;
constexpr size_t kFacingBytes = (size_t)kChunkVolume;

// A palette bigger than this is not worth the table: at 256 entries the indices
// are already a byte each, which is half of a raw id, and the table costs 512
// bytes. Past that, raw compresses better than a wide-index palette.
constexpr size_t kMaxPalette = 256;

int bits_for(size_t palette_size)
{
    int bits = 1;
    while (((size_t)1 << bits) < palette_size) ++bits;
    return bits;
}

// Indices packed at `bits` each, little end first, spilling across bytes.
void pack_indices(const std::vector<uint16_t> &indices, int bits, std::vector<uint8_t> &out)
{
    uint32_t acc      = 0;
    int      acc_bits = 0;

    for (const uint16_t v : indices) {
        acc |= (uint32_t)v << acc_bits;
        acc_bits += bits;
        while (acc_bits >= 8) {
            out.push_back((uint8_t)(acc & 0xFF));
            acc >>= 8;
            acc_bits -= 8;
        }
    }
    if (acc_bits > 0) out.push_back((uint8_t)(acc & 0xFF));
}

bool unpack_indices(const uint8_t *data, size_t size, int bits, size_t count,
                    std::vector<uint16_t> &out)
{
    // The exact number of bytes those indices must occupy. This is an early
    // out with a clear failure, not the safety boundary: the loop below bounds
    // its own reads, and deleting this line leaves the decoder correct, which
    // is what trying to sabotage it showed. It stays because failing here says
    // "the body is the wrong size" instead of "the indices do not unpack".
    const size_t needed = (count * (size_t)bits + 7) / 8;
    if (size != needed) return false;

    out.resize(count);
    uint32_t acc      = 0;
    int      acc_bits = 0;
    size_t   at       = 0;
    const uint32_t mask = (bits >= 32) ? 0xFFFFFFFFu : ((1u << bits) - 1u);

    for (size_t i = 0; i < count; ++i) {
        while (acc_bits < bits) {
            if (at >= size) return false;
            acc |= (uint32_t)data[at++] << acc_bits;
            acc_bits += 8;
        }
        out[i] = (uint16_t)(acc & mask);
        acc >>= bits;
        acc_bits -= bits;
    }
    return true;
}

bool compress_into(const std::vector<uint8_t> &plain, int level, std::vector<uint8_t> &out)
{
    const size_t bound = ZSTD_compressBound(plain.size());
    std::vector<uint8_t> buf(bound);

    const size_t n = ZSTD_compress(buf.data(), buf.size(), plain.data(), plain.size(), level);
    if (ZSTD_isError(n)) return false;

    out.assign(buf.begin(), buf.begin() + (long)n);
    return true;
}

// Decompresses into a buffer of exactly `expected` bytes and requires that it
// filled it. The declared frame size is not consulted, because a peer writes
// that field: a hostile chunk claiming 4 GB would otherwise be a 4 GB
// allocation before anything has been validated.
bool decompress_exact(const uint8_t *src, size_t src_size, size_t expected,
                      std::vector<uint8_t> &out)
{
    out.assign(expected, 0);
    const size_t n = ZSTD_decompress(out.data(), out.size(), src, src_size);
    if (ZSTD_isError(n) || n != expected) {
        out.clear();
        return false;
    }
    return true;
}

} // namespace

bool encode_chunk(const Chunk &chunk, std::vector<uint8_t> &out, int level,
                  ChunkEncodeStats *stats, bool allow_palette)
{
    ChunkEncodeStats st;
    st.blocks_raw = kBlockBytes;

    ByteWriter header;
    const ChunkPos p = chunk.pos();
    header.i32(p.x);
    header.i32(p.y);
    header.i32(p.z);

    // Both halves have to be uniform. A chunk of open air has uniform blocks
    // and a full light array as soon as the sun has been computed into it, and
    // sending only is_uniform()'s two values loses every one of those 32768
    // light values -- which is a sky that arrives black. Found by the storage
    // gate comparing light after a save and load; the network path had the same
    // bug and no check that would have shown it.
    // All three planes, not two. A chunk of one block and one light value whose
    // cells are turned differently is not one value, and sending it as a header
    // loses all 32768 facings -- the same shape as the light bug above, on the
    // plane that was added after it. A chunk nothing has turned still goes as a
    // header, which is every chunk of every world this engine makes today.
    const bool facing_free = chunk.is_facing_uniform() && chunk.uniform_facing() == 0;

    if (chunk.is_uniform() && chunk.is_light_uniform() && facing_free) {
        // Nothing to compress: a chunk that is one block and one light value is
        // two values, and running them through zstd would make them bigger.
        header.u8((uint8_t)Encoding::Uniform);
        header.u16(chunk.uniform_block());
        header.u8(chunk.uniform_light());
        header.u32(0);   // payload length, for a decoder that skips messages

        out.insert(out.end(), header.data().begin(), header.data().end());

        st.uniform     = true;
        st.before_zstd = 0;
        st.after_zstd  = 0;
        if (stats) *stats = st;
        return true;
    }

    // Blocks and light, straight out of the chunk.
    std::vector<BlockId> blocks((size_t)kChunkVolume);
    std::vector<uint8_t> light((size_t)kChunkVolume);
    std::vector<uint8_t> facing((size_t)kChunkVolume);
    for (int z = 0; z < kChunkSize; ++z)
        for (int y = 0; y < kChunkSize; ++y)
            for (int x = 0; x < kChunkSize; ++x) {
                const int i = Chunk::index(x, y, z);
                blocks[(size_t)i] = chunk.get(x, y, z);
                light[(size_t)i]  = chunk.light(x, y, z);
                facing[(size_t)i] = chunk.facing(x, y, z);
            }

    // The palette, if there is a small one.
    std::unordered_map<BlockId, uint16_t> seen;
    std::vector<BlockId>                  palette;
    std::vector<uint16_t>                 indices((size_t)kChunkVolume);
    bool small_enough = allow_palette;

    for (size_t i = 0; i < blocks.size() && small_enough; ++i) {
        const BlockId id = blocks[i];
        auto          it = seen.find(id);
        if (it == seen.end()) {
            if (palette.size() >= kMaxPalette) {
                small_enough = false;
                break;
            }
            const uint16_t index = (uint16_t)palette.size();
            seen.emplace(id, index);
            palette.push_back(id);
            indices[i] = index;
        } else {
            indices[i] = it->second;
        }
    }

    std::vector<uint8_t> plain;
    Encoding             encoding;

    if (small_enough && palette.size() >= 1) {
        encoding = Encoding::Palette;
        const int bits = bits_for(palette.size());

        ByteWriter w;
        w.u16((uint16_t)palette.size());
        for (const BlockId id : palette) w.u16(id);
        w.u8((uint8_t)bits);
        plain = w.data();

        pack_indices(indices, bits, plain);

        st.palette_used = true;
        st.palette_size = palette.size();
    } else {
        encoding = Encoding::Raw;
        plain.reserve(kBlockBytes);
        for (const BlockId id : blocks) {
            plain.push_back((uint8_t)(id & 0xFF));
            plain.push_back((uint8_t)(id >> 8));
        }
    }

    // Light rides along in the same compressed frame. It is one byte per block
    // and mostly runs of the same value, so it costs almost nothing here and
    // would cost a second frame's overhead on its own.
    plain.insert(plain.end(), light.begin(), light.end());

    // And the facings, behind the light and only when there are any. A plane of
    // zeroes would compress to almost nothing and still cost every world that
    // has never turned a block a longer payload and a decoder that has to know
    // which shape it is looking at; the flag says which.
    if (!facing_free) plain.insert(plain.end(), facing.begin(), facing.end());

    st.before_zstd = plain.size();

    std::vector<uint8_t> compressed;
    if (!compress_into(plain, level, compressed)) return false;
    st.after_zstd = compressed.size();

    header.u8((uint8_t)((uint8_t)encoding | (facing_free ? 0 : kHasFacings)));
    header.u16(0);   // unused where the encoding is not Uniform
    header.u8(0);
    header.u32((uint32_t)compressed.size());

    out.insert(out.end(), header.data().begin(), header.data().end());
    out.insert(out.end(), compressed.begin(), compressed.end());

    if (stats) *stats = st;
    return true;
}

std::unique_ptr<Chunk> decode_chunk(ByteReader &r, std::string &err)
{
    ChunkPos pos;
    pos.x = r.i32();
    pos.y = r.i32();
    pos.z = r.i32();

    const uint8_t  encoding_byte = r.u8();
    const bool     has_facings   = (encoding_byte & kHasFacings) != 0;
    const uint8_t  encoding      = (uint8_t)(encoding_byte & (uint8_t)~kHasFacings);
    const uint16_t uniform_block = r.u16();
    const uint8_t  uniform_light = r.u8();
    const uint32_t payload_size  = r.u32();

    if (!r.ok()) {
        err = "truncated chunk header";
        return nullptr;
    }

    if (encoding == (uint8_t)Encoding::Uniform) {
        if (payload_size != 0) {
            err = "a uniform chunk with a payload";
            return nullptr;
        }
        if (has_facings) {
            // There is nowhere to put them: the uniform form is a header. An
            // encoder that flagged one is an encoder this one cannot read, and
            // saying so beats returning a chunk with the facings silently gone.
            err = "a uniform chunk that claims to carry facings";
            return nullptr;
        }
        auto chunk = std::make_unique<Chunk>(pos);
        chunk->set_uniform_block((BlockId)uniform_block);
        chunk->set_uniform_light(uniform_light);
        return chunk;
    }

    if (encoding != (uint8_t)Encoding::Palette && encoding != (uint8_t)Encoding::Raw) {
        err = "unknown chunk encoding " + std::to_string((int)encoding);
        return nullptr;
    }

    // A ceiling on the compressed payload, so a length field cannot ask for a
    // buffer larger than any honest chunk could need. Uncompressed is the worst
    // case a real encoder produces; the small slack covers zstd's own framing
    // when its input does not compress at all.
    const size_t max_payload = kBlockBytes + kLightBytes + kFacingBytes + 1024;
    if (payload_size > max_payload) {
        err = "chunk payload claims " + std::to_string(payload_size) + " bytes";
        return nullptr;
    }

    std::vector<uint8_t> compressed(payload_size);
    if (payload_size > 0 && !r.bytes(compressed.data(), payload_size)) {
        err = "truncated chunk payload";
        return nullptr;
    }

    std::vector<uint8_t> plain;

    if (encoding == (uint8_t)Encoding::Raw) {
        const size_t want = kBlockBytes + kLightBytes + (has_facings ? kFacingBytes : 0);
        if (!decompress_exact(compressed.data(), compressed.size(), want, plain)) {
            err = "chunk payload does not expand to one chunk";
            return nullptr;
        }

        auto chunk = std::make_unique<Chunk>(pos);
        for (int z = 0; z < kChunkSize; ++z)
            for (int y = 0; y < kChunkSize; ++y)
                for (int x = 0; x < kChunkSize; ++x) {
                    const size_t i  = (size_t)Chunk::index(x, y, z);
                    const BlockId id = (BlockId)((uint16_t)plain[i * 2] |
                                                 ((uint16_t)plain[i * 2 + 1] << 8));
                    chunk->set(x, y, z, id);
                    chunk->set_light(x, y, z, plain[kBlockBytes + i]);
                    if (has_facings)
                        chunk->set_facing(x, y, z, plain[kBlockBytes + kLightBytes + i]);
                }
        return chunk;
    }

    // Palette. The uncompressed size depends on the palette, which is inside
    // the compressed frame, so the bound is the largest a palette encoding can
    // legitimately be: a 256-entry table, a byte of width, one byte per block,
    // and the light.
    const size_t palette_max_plain =
        2 + kMaxPalette * 2 + 1 + kChunkVolume + kLightBytes + kFacingBytes;
    std::vector<uint8_t> buf(palette_max_plain);
    const size_t n = ZSTD_decompress(buf.data(), buf.size(), compressed.data(), compressed.size());
    if (ZSTD_isError(n)) {
        err = "chunk payload is not a valid compressed frame";
        return nullptr;
    }
    plain.assign(buf.begin(), buf.begin() + (long)n);

    ByteReader pr(plain.data(), plain.size());
    size_t     palette_size = 0;
    if (!pr.count(palette_size, kMaxPalette) || palette_size == 0) {
        err = "chunk palette is empty or too large";
        return nullptr;
    }

    std::vector<BlockId> palette(palette_size);
    for (size_t i = 0; i < palette_size; ++i) palette[i] = (BlockId)pr.u16();

    const int bits = (int)pr.u8();
    if (!pr.ok() || bits < 1 || bits > 16 || bits != bits_for(palette_size)) {
        err = "chunk palette width does not match its size";
        return nullptr;
    }

    // Likewise an early out: if this were removed, a short body would still be
    // refused by unpack_indices and a long one by the pr.done() below. Both
    // sabotages were tried and neither turned the gate red, which is worth
    // saying out loud -- a check nobody can break is not protecting anything by
    // itself, and pretending otherwise is how a defence gets removed later by
    // someone who tested that it was load bearing and found it was not.
    const size_t index_bytes = ((size_t)kChunkVolume * (size_t)bits + 7) / 8;
    if (pr.remaining() != index_bytes + kLightBytes + (has_facings ? kFacingBytes : 0)) {
        err = "chunk body is not the size its palette implies";
        return nullptr;
    }

    std::vector<uint8_t> index_bytes_buf(index_bytes);
    if (!pr.bytes(index_bytes_buf.data(), index_bytes)) {
        err = "truncated chunk indices";
        return nullptr;
    }

    std::vector<uint16_t> indices;
    if (!unpack_indices(index_bytes_buf.data(), index_bytes_buf.size(), bits,
                        (size_t)kChunkVolume, indices)) {
        err = "chunk indices do not unpack";
        return nullptr;
    }

    std::vector<uint8_t> light(kLightBytes);
    if (!pr.bytes(light.data(), kLightBytes)) {
        err = "truncated chunk light";
        return nullptr;
    }

    std::vector<uint8_t> facing(has_facings ? kFacingBytes : 0);
    if (has_facings && !pr.bytes(facing.data(), kFacingBytes)) {
        err = "truncated chunk facings";
        return nullptr;
    }
    if (!pr.done()) {
        err = "chunk body has bytes after the planes it declared";
        return nullptr;
    }

    auto chunk = std::make_unique<Chunk>(pos);
    for (int z = 0; z < kChunkSize; ++z)
        for (int y = 0; y < kChunkSize; ++y)
            for (int x = 0; x < kChunkSize; ++x) {
                const size_t   i     = (size_t)Chunk::index(x, y, z);
                const uint16_t index = indices[i];
                // The index comes from the wire and is used to subscript the
                // palette. Checked here rather than assumed from the bit width:
                // a 3-bit index can be 7 with a palette of five.
                if (index >= palette.size()) {
                    err = "a palette index outside the palette";
                    return nullptr;
                }
                chunk->set(x, y, z, palette[index]);
                chunk->set_light(x, y, z, light[i]);
                if (has_facings) chunk->set_facing(x, y, z, facing[i]);
            }

    return chunk;
}

size_t collect_chunk_messages(const World &world, PlayerInterest &interest,
                              std::vector<std::vector<uint8_t>> &out,
                              const std::function<bool(ChunkPos)> &sendable, double now)
{
    size_t bytes = 0;

    for (const ChunkPos p : interest.next(world, sendable)) {
        const Chunk *chunk = world.find_chunk(p);
        if (!chunk) continue;   // it went away between the query and here

        std::vector<uint8_t> message;
        message.push_back((uint8_t)MsgType::Chunk);
        if (!encode_chunk(*chunk, message, 3)) continue;

        // The byte budget is checked against what the encoder produced, not
        // against an estimate. A worst-case chunk is 90 KB and a typical one is
        // 300 bytes; budgeting on the average is how a tick full of noisy
        // chunks becomes a stall.
        if (bytes + message.size() > interest.config().bytes_per_tick && !out.empty()) break;

        bytes += message.size();
        interest.mark_sent(p, now);
        out.push_back(std::move(message));
    }

    return bytes;
}

void install_chunk(World &world, std::unique_ptr<Chunk> chunk)
{
    if (!chunk) return;

    const ChunkPos p = chunk->pos();
    Chunk         &dst = world.ensure_chunk(p);

    for (int z = 0; z < kChunkSize; ++z)
        for (int y = 0; y < kChunkSize; ++y)
            for (int x = 0; x < kChunkSize; ++x) {
                dst.set(x, y, z, chunk->get(x, y, z));
                dst.set_light(x, y, z, chunk->light(x, y, z));
                dst.set_facing(x, y, z, chunk->facing(x, y, z));
            }

    if (chunk->is_uniform()) {
        dst.set_uniform_block(chunk->uniform_block());
        dst.set_uniform_light(chunk->uniform_light());
        dst.set_uniform_facing(chunk->uniform_facing());
    }
    // And every chunk round it: one meshed while this was missing drew its
    // border against air -- a lake's side as a wall standing in the water,
    // along a chunk's edge, for as long as nothing else touched that chunk.
    // The edges and corners too, for ambient occlusion and a liquid's corners.
    for (int dz = -1; dz <= 1; ++dz)
        for (int dy = -1; dy <= 1; ++dy)
            for (int dx = -1; dx <= 1; ++dx)
                if (dx != 0 || dy != 0 || dz != 0) world.mark_mesh_dirty(ChunkPos{p.x + dx, p.y + dy, p.z + dz});
}

} // namespace net
} // namespace cnp
