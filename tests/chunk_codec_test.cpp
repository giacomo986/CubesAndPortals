// Chunks on the wire: does the same world come out, and what does it cost.
//
// Two halves, and the second is the one with numbers in it. First: a chunk
// encoded and decoded is the chunk that went in, for all three encodings, and
// nothing malformed gets past the decoder. Then: a real generated world, chunk
// by chunk, with the sizes printed -- because "compressed chunk streaming" is
// a claim about bytes, and a test that only proves correctness would leave the
// interesting half of that claim unmeasured.
//
// The palette-versus-raw choice is decided here rather than asserted: bit
// packing destroys the byte alignment a compressor exploits, so it is entirely
// possible for raw plus zstd to beat palette plus zstd. Both are printed.

#include "core/net/chunk_codec.h"
#include "core/net/session.h"
#include "core/physics/movement.h"
#include "core/registry/core_nodes.h"
#include "core/voxel/chunk.h"
#include "core/voxel/light.h"
#include "core/worldgen/worldgen.h"

#include <zstd.h>

#include <chrono>
#include <cstdlib>
#include <unordered_set>
#include <cstdio>
#include <string>
#include <vector>

using namespace cnp;
using namespace cnp::net;

namespace {

int g_failures = 0;

void check(bool cond, const char *what)
{
    std::printf(cond ? "  ok    %s\n" : "  FAIL  %s\n", what);
    if (!cond) ++g_failures;
}

struct Rng {
    uint64_t s;
    explicit Rng(uint64_t seed) : s(seed ? seed : 1) {}
    uint32_t next()
    {
        s ^= s << 13;
        s ^= s >> 7;
        s ^= s << 17;
        return (uint32_t)(s >> 32);
    }
};

bool same_chunk(const Chunk &a, const Chunk &b)
{
    for (int z = 0; z < kChunkSize; ++z)
        for (int y = 0; y < kChunkSize; ++y)
            for (int x = 0; x < kChunkSize; ++x) {
                if (a.get(x, y, z) != b.get(x, y, z)) return false;
                if (a.light(x, y, z) != b.light(x, y, z)) return false;
                if (a.facing(x, y, z) != b.facing(x, y, z)) return false;
            }
    return true;
}

// Encodes, decodes, and checks the round trip. Returns the encoded size.
size_t round_trip(const Chunk &c, const char *what, bool allow_palette = true)
{
    std::vector<uint8_t> buf;
    ChunkEncodeStats     st;
    if (!encode_chunk(c, buf, 3, &st, allow_palette)) {
        std::printf("  FAIL  %s: encoding failed\n", what);
        ++g_failures;
        return 0;
    }

    ByteReader  r(buf);
    std::string err;
    auto        back = decode_chunk(r, err);
    if (!back) {
        std::printf("  FAIL  %s: %s\n", what, err.c_str());
        ++g_failures;
        return buf.size();
    }

    check(back->pos() == c.pos() && same_chunk(c, *back), what);
    return buf.size();
}

double seconds_since(std::chrono::steady_clock::time_point t0)
{
    return std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
}

} // namespace

int main()
{
    std::printf("round trips\n");
    {
        // Uniform: the case that is most of a world.
        Chunk air(ChunkPos{1, 2, 3});
        air.set_uniform_block(kAir);
        air.set_uniform_light(pack_light(15, 0));
        const size_t air_bytes = round_trip(air, "a uniform chunk survives the wire");
        std::printf("        a uniform chunk is %zu bytes\n", air_bytes);
        check(air_bytes < 32, "and it costs a header, not a chunk");

        // A handful of blocks: the palette path.
        Chunk mixed(ChunkPos{-4, 0, 7});
        for (int z = 0; z < kChunkSize; ++z)
            for (int y = 0; y < kChunkSize; ++y)
                for (int x = 0; x < kChunkSize; ++x) {
                    const BlockId id = (BlockId)((y < 8) ? 3 : (y < 16 ? 1 : (x % 3 ? 0 : 2)));
                    mixed.set(x, y, z, id);
                    mixed.set_light(x, y, z, pack_light((uint8_t)(y > 16 ? 15 : 0), 0));
                }
        round_trip(mixed, "a chunk with a few blocks survives the wire");

        // More distinct ids than a palette holds: the raw path.
        Chunk many(ChunkPos{0, 0, 0});
        Rng   rng(7);
        for (int z = 0; z < kChunkSize; ++z)
            for (int y = 0; y < kChunkSize; ++y)
                for (int x = 0; x < kChunkSize; ++x)
                    many.set(x, y, z, (BlockId)(rng.next() % 400));
        round_trip(many, "a chunk with 400 different blocks survives the wire");

        // Negative coordinates, because a chunk position is signed and the
        // whole world is not in the positive octant.
        Chunk negative(ChunkPos{-9, -1, -300});
        negative.set(0, 0, 0, 5);
        round_trip(negative, "and so does one at a negative position");
    }

    std::printf("\nwhich way a cell is turned\n");
    {
        // TODO.md OPEN-27. Turning used to be extra block ids -- four per
        // rotatable block, one per quarter turn about the vertical -- which is
        // cheap while there are four and is twenty-four rows a block for the
        // orientations a piston or a log lying down needs. This is the byte per
        // cell that replaces them, and the part that cannot be changed quietly
        // later is the wire and the disk.
        //
        // Nothing READS it yet. What is checked here is that it survives.
        Chunk turned(ChunkPos{2, 0, 5});
        for (int z = 0; z < kChunkSize; ++z)
            for (int y = 0; y < kChunkSize; ++y)
                for (int x = 0; x < kChunkSize; ++x) {
                    turned.set(x, y, z, (BlockId)((x + y + z) % 5));
                    turned.set_facing(x, y, z, (uint8_t)((x * 7 + y * 3 + z) % 24));
                }
        round_trip(turned, "a chunk that remembers which way its cells are turned survives "
                           "the wire");

        // The trap the LIGHT fell into, on the other plane. A chunk of one
        // block and one light value takes the uniform path -- a header and no
        // payload -- and a chunk whose cells are turned differently is not one
        // value, whatever its blocks and its light are. Sending it as a header
        // loses all 32768 facings, which is the shape of "a sky that arrives
        // black" in M2.
        Chunk one_block(ChunkPos{9, 9, 9});
        one_block.set_uniform_block(4);
        one_block.set_uniform_light(pack_light(15, 0));
        one_block.set_facing(3, 4, 5, 17);
        check(one_block.is_uniform() && one_block.is_light_uniform(),
              "a chunk can be one block and one light value and still be turned");

        std::vector<uint8_t> buf;
        ChunkEncodeStats     st;
        check(encode_chunk(one_block, buf, 3, &st), "it encodes");
        check(!st.uniform,
              "and it does NOT go as a header: one block and one light is not one FACING");
        round_trip(one_block, "so the cell that is turned comes back turned");

        // And the world every chunk of this game is: no facings at all. It must
        // cost nothing -- not a plane of zeroes, not a bigger header.
        Chunk flat(ChunkPos{1, 2, 3});
        flat.set_uniform_block(kAir);
        flat.set_uniform_light(pack_light(15, 0));
        std::vector<uint8_t> flat_buf;
        ChunkEncodeStats     flat_st;
        check(encode_chunk(flat, flat_buf, 3, &flat_st) && flat_st.uniform,
              "a chunk nothing has turned is still a header and no payload");
        check(flat_buf.size() < 32, "and still costs under 32 bytes");
        // Against sizeof and against a chunk that IS turned, not against
        // another untouched chunk: two chunks with no plane stay equal to each
        // other however much either of them over-counts. Measured -- a
        // memory_bytes() that added the plane unconditionally left that green.
        Chunk with_plane(ChunkPos{0, 0, 0});
        with_plane.set_facing(0, 0, 0, 1);
        check(flat.memory_bytes() == sizeof(Chunk),
              "and allocates no plane: the byte per cell is paid by worlds that use it");
        check(with_plane.memory_bytes() - flat.memory_bytes() == (size_t)kChunkVolume,
              "while a chunk one cell of which is turned pays exactly one byte a cell");
        std::printf("        a turned chunk is %zu bytes, an untouched one %zu\n", buf.size(),
                    flat_buf.size());
    }

    std::printf("\nwhat a real world costs\n");
    {
        World        world;
        NodeRegistry registry;
        register_core_nodes(registry);

        constexpr int kExtent = 128;
        Mapgen        mapgen;
        mapgen.generate(world, registry, 1, kExtent, kWorldHeight);
        compute_lighting(world, registry, 0, 0, 0, kExtent - 1, kWorldHeight - 1, kExtent - 1);

        size_t chunks = 0, uniform = 0, palette = 0;
        size_t raw_total = 0, wire_total = 0, wire_raw_encoding = 0;
        size_t largest = 0;

        const auto t0 = std::chrono::steady_clock::now();
        for (const auto &kv : world.chunks()) {
            const Chunk &c = *kv.second;
            ++chunks;

            std::vector<uint8_t> buf;
            ChunkEncodeStats     st;
            encode_chunk(c, buf, 3, &st);

            raw_total += st.blocks_raw + (size_t)kChunkVolume;   // ids + light
            wire_total += buf.size();
            if (buf.size() > largest) largest = buf.size();
            if (st.uniform) ++uniform;
            if (st.palette_used) ++palette;

            // The same chunk again with the palette switched off, so the
            // comparison is between two measurements and not between a
            // measurement and an expectation.
            std::vector<uint8_t> raw_buf;
            encode_chunk(c, raw_buf, 3, nullptr, false);
            wire_raw_encoding += raw_buf.size();
        }
        const double encode_seconds = seconds_since(t0);

        std::printf("        %zu chunks: %zu uniform, %zu palette, %zu raw\n", chunks, uniform,
                    palette, chunks - uniform - palette);
        std::printf("        in memory %.2f MB -> on the wire %.2f MB (%.1f%%)\n",
                    (double)raw_total / (1024.0 * 1024.0),
                    (double)wire_total / (1024.0 * 1024.0),
                    100.0 * (double)wire_total / (double)raw_total);
        std::printf("        same chunks without the palette: %.2f MB (%.1f%%)\n",
                    (double)wire_raw_encoding / (1024.0 * 1024.0),
                    100.0 * (double)wire_raw_encoding / (double)raw_total);
        std::printf("        largest single chunk %zu bytes, encode %.1f us/chunk\n", largest,
                    1e6 * encode_seconds / (double)(chunks ? chunks : 1));

        check(wire_total < raw_total / 4,
              "a world costs under a quarter of its size on the wire");
        check(largest < 65536, "and no single chunk message is larger than 64 KB");

        // The number above is flattering and should not be quoted on its own:
        // this world has a flat plaza in the middle of it and four block types.
        // The bound that matters for a streaming budget is what an
        // incompressible chunk costs, so it is measured rather than hoped for.
        {
            Chunk worst(ChunkPos{0, 0, 0});
            Rng   noise(99);
            for (int z = 0; z < kChunkSize; ++z)
                for (int y = 0; y < kChunkSize; ++y)
                    for (int x = 0; x < kChunkSize; ++x) {
                        worst.set(x, y, z, (BlockId)(noise.next() % 4000));
                        worst.set_light(x, y, z, (uint8_t)(noise.next() & 0xFF));
                    }

            std::vector<uint8_t> buf;
            encode_chunk(worst, buf, 3);
            std::printf("        worst case (random blocks, random light): %zu bytes\n",
                        buf.size());

            // Whatever it is, the decoder's own ceiling has to be above it, or
            // the encoder can produce a message its own decoder refuses.
            check(buf.size() < (size_t)kChunkVolume * 3 + 2048,
                  "and the worst case still fits inside the decoder's ceiling");

            ByteReader  r(buf);
            std::string err;
            auto        back = decode_chunk(r, err);
            check(back != nullptr && same_chunk(worst, *back),
                  "and it still round trips");
        }

        // Decoding back into a second world, and the worlds compared. This is
        // the property streaming exists to provide, so it is checked on a real
        // world rather than on one chunk.
        World        received;
        const auto   t1 = std::chrono::steady_clock::now();
        size_t       decoded = 0;
        bool         all_ok  = true;
        for (const auto &kv : world.chunks()) {
            std::vector<uint8_t> buf;
            encode_chunk(*kv.second, buf, 3);

            ByteReader  r(buf);
            std::string err;
            auto        back = decode_chunk(r, err);
            if (!back) {
                all_ok = false;
                std::printf("        decode failed: %s\n", err.c_str());
                break;
            }
            install_chunk(received, std::move(back));
            ++decoded;
        }
        const double decode_seconds = seconds_since(t1);

        check(all_ok && decoded == chunks, "every chunk decodes");
        check(world_hash(received, kExtent, kWorldHeight) ==
                  world_hash(world, kExtent, kWorldHeight),
              "and the world that arrives hashes the same as the one that was sent");
        std::printf("        decode %.1f us/chunk\n",
                    1e6 * decode_seconds / (double)(decoded ? decoded : 1));
    }

    std::printf("\nchunks from somebody who is not us\n");
    {
        Chunk c(ChunkPos{2, 0, 2});
        for (int z = 0; z < kChunkSize; ++z)
            for (int y = 0; y < 12; ++y)
                for (int x = 0; x < kChunkSize; ++x) c.set(x, y, z, (BlockId)(1 + (x + z) % 5));

        std::vector<uint8_t> good;
        encode_chunk(c, good, 3);

        // Every prefix.
        int accepted_short = 0;
        for (size_t n = 0; n < good.size(); ++n) {
            ByteReader  r(good.data(), n);
            std::string err;
            if (decode_chunk(r, err)) ++accepted_short;
        }
        check(accepted_short == 0, "no prefix of a chunk decodes as a chunk");

        // A payload length that is a lie.
        {
            std::vector<uint8_t> lying = good;
            // The length is the last four bytes of the 21-byte header.
            lying[17] = 0xFF; lying[18] = 0xFF; lying[19] = 0xFF; lying[20] = 0x7F;
            ByteReader  r(lying);
            std::string err;
            check(!decode_chunk(r, err),
                  "a payload claiming two gigabytes is refused, not allocated");
            std::printf("        refused with: %s\n", err.c_str());
        }

        // A compressed frame that expands to something that is not a chunk.
        {
            std::vector<uint8_t> bomb_plain(4 * 1024 * 1024, 0);
            std::vector<uint8_t> bomb(ZSTD_compressBound(bomb_plain.size()));
            const size_t n = ZSTD_compress(bomb.data(), bomb.size(), bomb_plain.data(),
                                           bomb_plain.size(), 3);
            bomb.resize(n);

            ByteWriter w;
            w.i32(0); w.i32(0); w.i32(0);
            w.u8(2);            // Raw
            w.u16(0); w.u8(0);
            w.u32((uint32_t)bomb.size());
            w.bytes(bomb.data(), bomb.size());

            ByteReader  r(w.data());
            std::string err;
            check(!decode_chunk(r, err),
                  "four megabytes hiding in a small frame does not become four megabytes");
            std::printf("        refused with: %s\n", err.c_str());
        }

        // Everything above corrupts the *compressed* payload, so zstd refuses
        // it and the checks inside the decoder are never reached. Sabotaging
        // three of them -- the palette index bound, the palette width, and the
        // requirement that a raw frame expand to exactly one chunk -- left this
        // whole section green, which is what a fuzz test that stops at the
        // compression layer is worth.
        //
        // So these three are built by hand: valid frames, valid zstd, hostile
        // contents.
        {
            // A palette of five, an index of seven. Three bits holds both.
            ByteWriter plain;
            plain.u16(5);
            for (int i = 0; i < 5; ++i) plain.u16((uint16_t)(i + 1));
            plain.u8(3);   // bits, correct for a palette of five

            std::vector<uint8_t> packed;
            {
                uint32_t acc = 0;
                int      acc_bits = 0;
                for (int i = 0; i < kChunkVolume; ++i) {
                    const uint32_t index = (i == 12345) ? 7u : 0u;   // one lie
                    acc |= index << acc_bits;
                    acc_bits += 3;
                    while (acc_bits >= 8) {
                        packed.push_back((uint8_t)(acc & 0xFF));
                        acc >>= 8;
                        acc_bits -= 8;
                    }
                }
                if (acc_bits > 0) packed.push_back((uint8_t)(acc & 0xFF));
            }
            plain.bytes(packed.data(), packed.size());
            std::vector<uint8_t> light(kChunkVolume, 0);
            plain.bytes(light.data(), light.size());

            std::vector<uint8_t> frame(ZSTD_compressBound(plain.size()));
            const size_t n = ZSTD_compress(frame.data(), frame.size(), plain.data().data(),
                                           plain.size(), 3);
            frame.resize(n);

            ByteWriter msg;
            msg.i32(0); msg.i32(0); msg.i32(0);
            msg.u8(1);            // Palette
            msg.u16(0); msg.u8(0);
            msg.u32((uint32_t)frame.size());
            msg.bytes(frame.data(), frame.size());

            ByteReader  r(msg.data());
            std::string err;
            check(!decode_chunk(r, err),
                  "an index of seven into a palette of five is refused, not read");
            std::printf("        refused with: %s\n", err.c_str());
        }

        {
            // A palette of five that claims to be packed at four bits. The
            // body length would then be wrong, and a decoder that trusted the
            // declared width would walk off the end of it.
            ByteWriter plain;
            plain.u16(5);
            for (int i = 0; i < 5; ++i) plain.u16((uint16_t)(i + 1));
            plain.u8(4);   // a lie: five entries need three bits

            std::vector<uint8_t> body((size_t)kChunkVolume / 2, 0);
            plain.bytes(body.data(), body.size());
            std::vector<uint8_t> light(kChunkVolume, 0);
            plain.bytes(light.data(), light.size());

            std::vector<uint8_t> frame(ZSTD_compressBound(plain.size()));
            const size_t n = ZSTD_compress(frame.data(), frame.size(), plain.data().data(),
                                           plain.size(), 3);
            frame.resize(n);

            ByteWriter msg;
            msg.i32(0); msg.i32(0); msg.i32(0);
            msg.u8(1);
            msg.u16(0); msg.u8(0);
            msg.u32((uint32_t)frame.size());
            msg.bytes(frame.data(), frame.size());

            ByteReader  r(msg.data());
            std::string err;
            check(!decode_chunk(r, err),
                  "a palette width that does not match the palette is refused");
            std::printf("        refused with: %s\n", err.c_str());
        }

        {
            // A raw frame that decompresses to one byte short of a chunk. Valid
            // zstd, valid length field, not a chunk.
            std::vector<uint8_t> plain((size_t)kChunkVolume * 3 - 1, 0);
            std::vector<uint8_t> frame(ZSTD_compressBound(plain.size()));
            const size_t n = ZSTD_compress(frame.data(), frame.size(), plain.data(), plain.size(),
                                           3);
            frame.resize(n);

            ByteWriter msg;
            msg.i32(0); msg.i32(0); msg.i32(0);
            msg.u8(2);            // Raw
            msg.u16(0); msg.u8(0);
            msg.u32((uint32_t)frame.size());
            msg.bytes(frame.data(), frame.size());

            ByteReader  r(msg.data());
            std::string err;
            check(!decode_chunk(r, err),
                  "a raw frame one byte short of a chunk is refused");
            std::printf("        refused with: %s\n", err.c_str());
        }

        {
            // A palette whose declared width is right and whose body is short.
            // Refused -- though not by the length check it looks like it should
            // be: with that check deleted the indices simply fail to unpack.
            // Both were sabotaged, neither turned this red on its own, and that
            // is recorded in the codec rather than left as an assumption.
            ByteWriter plain;
            plain.u16(5);
            for (int i = 0; i < 5; ++i) plain.u16((uint16_t)(i + 1));
            plain.u8(3);

            std::vector<uint8_t> body(100, 0);   // nowhere near 32768 * 3 bits
            plain.bytes(body.data(), body.size());
            std::vector<uint8_t> light(kChunkVolume, 0);
            plain.bytes(light.data(), light.size());

            std::vector<uint8_t> frame(ZSTD_compressBound(plain.size()));
            const size_t n = ZSTD_compress(frame.data(), frame.size(), plain.data().data(),
                                           plain.size(), 3);
            frame.resize(n);

            ByteWriter msg;
            msg.i32(0); msg.i32(0); msg.i32(0);
            msg.u8(1);
            msg.u16(0); msg.u8(0);
            msg.u32((uint32_t)frame.size());
            msg.bytes(frame.data(), frame.size());

            ByteReader  r(msg.data());
            std::string err;
            check(!decode_chunk(r, err), "a palette body far too short is refused");
            std::printf("        refused with: %s\n", err.c_str());
        }

        // Corruption, everywhere, one byte at a time.
        Rng rng(0xC0DEC);
        int decoded = 0;
        for (int iter = 0; iter < 20000; ++iter) {
            std::vector<uint8_t> buf = good;
            const int            k   = 1 + (int)(rng.next() % 6);
            for (int j = 0; j < k; ++j)
                buf[rng.next() % buf.size()] = (uint8_t)(rng.next() & 0xFF);

            ByteReader  r(buf);
            std::string err;
            if (decode_chunk(r, err)) ++decoded;
        }
        std::printf("        20000 corrupted chunks, %d still decoded\n", decoded);
        check(true, "and none of them crashed the decoder");
    }

    std::printf("\ninterest: who gets what, and in what order\n");
    {
        World        world;
        NodeRegistry registry;
        register_core_nodes(registry);
        Mapgen       mapgen;
        mapgen.generate(world, registry, 1, 128, kWorldHeight);

        InterestConfig config;
        config.radius          = 2;
        config.vertical_radius = 1;
        config.chunks_per_tick = 4;
        PlayerInterest interest(config);

        // Standing exactly on a chunk boundary, looking towards +x. The
        // position is chosen, not incidental: x=64 and z=64 are the corner
        // where four chunks meet, which is where a player is every time they
        // cross one, and where an ordering that measures distance alone puts
        // the ground under their feet fourth.
        interest.set_position(64.0f, 22.0f, 64.0f, 1.0f, 0.0f, 0.0f);

        const std::vector<ChunkPos> first = interest.next(world);
        check(first.size() == 4, "a tick sends at most the budget");
        check(first[0] == interest.centre(),
              "and the chunk the player is standing in comes first");

        // Everything it offers is real, generated, and inside the radius.
        bool all_near = true, all_exist = true;
        for (const ChunkPos p : first) {
            if (world.find_chunk(p) == nullptr) all_exist = false;
            if (std::abs(p.x - interest.centre().x) > config.radius ||
                std::abs(p.z - interest.centre().z) > config.radius)
                all_near = false;
        }
        check(all_exist, "every chunk offered is one the server has generated");
        check(all_near, "and inside the radius");

        // Looking the other way changes the order but not the contents: a
        // player who spins should not wait for the world behind them.
        //
        // Measured from inside a chunk rather than from the corner above, and
        // that is a finding rather than a convenience: the facing bias is a
        // multiplier on distance, so at distance zero it does nothing, and on a
        // boundary every neighbour is at distance zero. Standing on a corner,
        // which way you look genuinely does not matter -- all four are equally
        // adjacent. It matters everywhere else.
        PlayerInterest ahead(config), behind(config);
        ahead.set_position(80.0f, 22.0f, 80.0f, 1.0f, 0.0f, 0.0f);
        behind.set_position(80.0f, 22.0f, 80.0f, -1.0f, 0.0f, 0.0f);

        const std::vector<ChunkPos> looking_ahead  = ahead.next(world);
        const std::vector<ChunkPos> looking_behind = behind.next(world);

        bool order_differs = false;
        for (size_t i = 0; i < looking_ahead.size() && i < looking_behind.size(); ++i)
            if (!(looking_ahead[i] == looking_behind[i])) order_differs = true;
        check(order_differs, "looking the other way changes what arrives first");
        check(looking_ahead[0] == looking_behind[0],
              "except the chunk underfoot, which is first either way");

        // And the direction is the one asked for, not merely a different one.
        check(looking_ahead[1].x >= looking_behind[1].x,
              "the second chunk is the one the player is looking at");

        const std::vector<ChunkPos> corner = interest.next(world);
        check(corner == first, "on a corner the order is the tie-break, and it is stable");

        // Determinism: same state, same answer, twice.
        PlayerInterest twin(config);
        twin.set_position(64.0f, 22.0f, 64.0f, 1.0f, 0.0f, 0.0f);
        check(twin.next(world) == first, "the same player in the same place gets the same order");

        // Drain it, and check nothing is sent twice.
        std::unordered_set<long long> seen;
        size_t                        total = 0;
        bool                          duplicate = false;
        for (int tick = 0; tick < 200; ++tick) {
            const std::vector<ChunkPos> batch = interest.next(world);
            if (batch.empty()) break;
            for (const ChunkPos p : batch) {
                const long long key = ((long long)p.x << 40) ^ ((long long)p.y << 20) ^ p.z;
                if (!seen.insert(key).second) duplicate = true;
                interest.mark_sent(p);
                ++total;
            }
        }
        check(!duplicate, "no chunk is sent to the same player twice");
        check(interest.next(world).empty(),
              "and a player who has everything nearby is sent nothing");
        std::printf("        %zu chunks around one player at radius %d\n", total, config.radius);

        // Walk away far enough and the old ones are forgotten, so coming back
        // sends them again -- but a step across a boundary does not.
        const size_t before = interest.sent_count();
        interest.set_position(64.0f + 32.0f, 22.0f, 64.0f, 1.0f, 0.0f, 0.0f);
        check(interest.forget_distant() == 0,
              "one chunk of walking forgets nothing, which is what the margin is for");
        check(interest.sent_count() == before, "so nothing is queued up to be resent");

        interest.set_position(64.0f + 32.0f * 9.0f, 22.0f, 64.0f, 1.0f, 0.0f, 0.0f);
        const size_t dropped = interest.forget_distant();
        check(dropped > 0, "walking a long way forgets what is behind");
        std::printf("        walking nine chunks away forgot %zu of them\n", dropped);

        // And ONE chunk forgotten on purpose, which is the other reason a
        // chunk stops being current: a mod rewrote it under a player standing
        // in it. forget_distant() cannot be asked for this -- it decides by
        // distance, and this chunk is as near as a chunk gets.
        {
            // A session of its own, drained, so "this chunk was sent" is a fact
            // and not a leftover of the walk above.
            PlayerInterest standing(config);
            standing.set_position(64.0f, 22.0f, 64.0f, 1.0f, 0.0f, 0.0f);
            for (int tick = 0; tick < 200; ++tick) {
                const std::vector<ChunkPos> batch = standing.next(world);
                if (batch.empty()) break;
                for (const ChunkPos p : batch) standing.mark_sent(p);
            }

            const ChunkPos here = standing.centre();
            check(standing.was_sent(here), "the chunk the player is standing in was sent");
            check(standing.next(world).empty(), "and nothing is queued before it is forgotten");

            check(standing.forget(here), "forgetting a chunk this session has says so");
            check(!standing.was_sent(here), "and the session no longer has it");

            const std::vector<ChunkPos> again = standing.next(world);
            bool comes_back = false;
            for (const ChunkPos p : again) comes_back = comes_back || p == here;
            check(comes_back, "so the streamer sends it again");

            check(!standing.forget(here),
                  "forgetting it twice says no the second time: a caller counting resends "
                  "must not be able to count the same one twice");
        }

        // What a session owed a pile of changed cells should be sent.
        //
        // TODO OPEN-19: every cell a mod changes goes out as its own nine-byte
        // BlockChange, one per session, so a mod that rewrites a 32x32x32 chunk
        // sends 32768 of them to each of up to eight sessions. Past about a
        // hundred and fifty cells in one chunk the chunk itself is the cheaper
        // message.
        {
            std::vector<CellPos>  cells;
            std::vector<ChunkPos> resend;
            std::vector<CellPos>  one_by_one;

            // One chunk rewritten wholesale, and two lonely cells in another.
            // Both in the same list, because the decision is per CHUNK and a
            // version that decided per session would send the two cells as a
            // chunk as well.
            // In a chunk with TERRAIN in it and not in the sky: a chunk of
            // nothing but air encodes to almost nothing, and a comparison made
            // against that would find the chunk cheaper whatever the codec did.
            for (int i = 0; i < 400; ++i)
                cells.push_back(CellPos{64 + i % 32, 16 + i / 32, 64});
            cells.push_back(CellPos{100, 20, 100});
            cells.push_back(CellPos{101, 20, 100});

            split_owed_cells(cells, kCellsWorthAChunk, resend, one_by_one);

            check(resend.size() == 1, "a chunk with four hundred changed cells is resent whole");
            check(resend.empty() || resend[0] == World::chunk_of(64, 16, 64),
                  "and it is the chunk those cells are in");
            check(one_by_one.size() == 2,
                  "while two lonely cells in another chunk still go one at a time");
            check(one_by_one.size() == 2 && one_by_one[0].x == 100 && one_by_one[1].x == 101,
                  "in the order they happened, because the last write is what the cell holds");

            // The bytes, which is the whole argument. A BlockChange is ten
            // bytes plus its reason string; a chunk of this world is what the
            // encoder actually produces.
            std::vector<uint8_t> encoded;
            const Chunk         *whole = world.find_chunk(World::chunk_of(64, 16, 64));
            const bool           got   = whole != nullptr && encode_chunk(*whole, encoded);
            const size_t         one   = ServerSessions::block_change_message(
                                              64, 16, 64, 1, 0, std::string()).size();
            const size_t         as_cells = 402 * one;
            const size_t         as_chunk = (got ? encoded.size() : 0) + 2 * one;
            check(got && as_chunk < as_cells,
                  "and the chunk really is the smaller message");
            std::printf("        400 cells in one chunk: %zu bytes as cells, %zu as the chunk\n",
                        as_cells, as_chunk);

            // The threshold is a threshold and not a shape: the same cells
            // under a higher bar stay cells.
            std::vector<ChunkPos> none;
            std::vector<CellPos>  all;
            split_owed_cells(cells, 4096, none, all);
            check(none.empty() && all.size() == cells.size(),
                  "under a bar nothing reaches, every cell goes one at a time");
        }

        // And the byte budget, checked against real encoded sizes.
        PlayerInterest budgeted(config);
        budgeted.config().bytes_per_tick = 1;   // absurdly small on purpose
        budgeted.set_position(64.0f, 22.0f, 64.0f, 1.0f, 0.0f, 0.0f);
        std::vector<std::vector<uint8_t>> messages;
        collect_chunk_messages(world, budgeted, messages);
        check(messages.size() == 1,
              "a byte budget smaller than one chunk still sends one, and only one");
    }

    std::printf("\nwhat the server will believe about where you are\n");
    {
        World        world;
        NodeRegistry registry;
        register_core_nodes(registry);
        Mapgen       mapgen;
        mapgen.generate(world, registry, 1, 128, kWorldHeight);

        const float ground = (float)kGroundTop;   // the plaza is flat at this height

        MovementState last;
        last.x = 64.0f; last.y = ground; last.z = 64.0f;
        last.when = 0.0; last.valid = true;

        // A tick of walking, which is what a player does every tick.
        {
            const MovementVerdict v =
                check_move(last, 64.0f + kWalkSpeed * 0.05f, ground, 64.0f, 0.05, world, registry);
            check(v.accepted, "walking for one tick is accepted");
        }

        // Sprinting, which is faster than walking and still legal.
        {
            const MovementVerdict v = check_move(last, 64.0f, ground, 64.0f + kSprintSpeed * 0.05f,
                                                 0.05, world, registry);
            check(v.accepted, "and so is sprinting");
        }

        // Flying at a sprint, for longer than a budget lasts. The client
        // doubles the flying speed while sprint is held (core/physics/player.cpp),
        // so this is a claim stream an unmodified client produces by holding
        // two keys. Eight seconds, back and forth across the plaza in the air,
        // one claim a tick, starting with the full budget a player who has
        // been standing still has in hand.
        {
            const float speed = kFlySpeed * kFlySprintFactor;

            MovementState at = last;
            at.x = 24.0f;
            at.y = ground + 20.0f;
            at.budget_h = MovementLimits{}.max_horizontal * MovementLimits{}.tolerance;
            at.budget_v = MovementLimits{}.max_vertical * MovementLimits{}.tolerance;

            int    refused = 0;
            double first   = -1.0;
            for (int tick = 1; tick <= 160; ++tick) {
                const double now = 0.05 * tick;
                const float  dir = ((tick - 1) / 60) % 2 == 0 ? 1.0f : -1.0f;
                const float  x   = at.x + dir * speed * 0.05f;

                const MovementVerdict v = check_move(at, x, at.y, at.z, now, world, registry);
                if (!v.accepted) {
                    ++refused;
                    if (first < 0.0) first = now;
                }
                at.x = v.x; at.y = v.y; at.z = v.z;
                at.when = now;
                at.budget_h = v.budget_h;
                at.budget_v = v.budget_v;
            }
            std::printf("        flying at a sprint, %.1f m/s for 8 s: %d of 160 claims refused, "
                        "the first after %.2f s\n",
                        (double)speed, refused, first);
            check(refused == 0, "and so is flying at a sprint, for as long as the keys are held");
        }

        // A world's own speeds (TODO.md OPEN-44). The limits are worked out from
        // what the world declared, by the rule the defaults follow -- so the
        // engine's own numbers must give the defaults exactly, and a declared
        // number must move the limit and not merely be carried beside it.
        {
            const MovementLimits plain = limits_for(MovementRules{});
            check(plain.max_horizontal == MovementLimits{}.max_horizontal &&
                      plain.max_vertical == MovementLimits{}.max_vertical,
                  "the engine's own numbers give exactly the validator's defaults");

            // Four seconds back and forth at `speed`, one claim a tick, from a
            // full budget. How many were refused.
            const auto refusals = [&](const MovementLimits &limits, float speed) {
                MovementState at = last;
                at.x = 24.0f;
                at.y = ground + 20.0f;
                at.budget_h = limits.max_horizontal * limits.tolerance * (float)limits.max_burst;
                at.budget_v = limits.max_vertical * limits.tolerance * (float)limits.max_burst;
                int refused = 0;
                for (int tick = 1; tick <= 80; ++tick) {
                    const double now = 0.05 * tick;
                    const float  dir = ((tick - 1) / 20) % 2 == 0 ? 1.0f : -1.0f;
                    const MovementVerdict v = check_move(at, at.x + dir * speed * 0.05f, at.y,
                                                         at.z, now, world, registry, limits);
                    if (!v.accepted) ++refused;
                    at.x = v.x; at.y = v.y; at.z = v.z;
                    at.when = now;
                    at.budget_h = v.budget_h;
                    at.budget_v = v.budget_v;
                }
                return refused;
            };

            MovementRules race;
            race.sprint = 60.0f;
            const MovementLimits fast = limits_for(race);

            const int declared   = refusals(fast, 60.0f);
            const int undeclared = refusals(MovementLimits{}, 60.0f);
            const int twice      = refusals(fast, 120.0f);
            std::printf("        a sprint of 60 m/s for 4 s: %d of 80 refused where the world "
                        "declared it, %d where it did not; 120 m/s: %d\n",
                        declared, undeclared, twice);
            check(declared == 0, "a world that declared a sprint of 60 m/s accepts one, for as "
                                 "long as it is held");
            check(undeclared > 0, "and a world that declared nothing refuses the same claims");
            check(twice > 0, "and the declaring world still refuses twice its own sprint");
        }

        // Four hundred blocks in a tick.
        {
            const MovementVerdict v = check_move(last, 464.0f, ground, 64.0f, 0.05, world,
                                                 registry);
            check(!v.accepted, "four hundred blocks in one tick is not");
            check(v.x == last.x && v.z == last.z,
                  "and the answer is where they were, not where they said");
            std::printf("        refused: %s\n", v.reason.c_str());
        }

        // Falling. Faster than walking, and the vertical limit is its own.
        //
        // In the air, twenty blocks up: the first version of this dropped two
        // blocks from the plaza surface, which is inside the ground, and was
        // refused for a reason that had nothing to do with the speed cap. The
        // check was measuring the wrong rule and passing would have meant
        // nothing.
        {
            MovementState aloft = last;
            aloft.y = ground + 20.0f;
            const MovementVerdict v = check_move(aloft, 64.0f, ground + 20.0f - 40.0f * 0.05f,
                                                 64.0f, 0.05, world, registry);
            check(v.accepted, "falling is not a speed cap violation");
        }

        // But falling faster than terminal velocity is.
        {
            MovementState aloft = last;
            aloft.y = ground + 40.0f;
            const MovementVerdict v = check_move(aloft, 64.0f, ground + 40.0f - 200.0f * 0.05f,
                                                 64.0f, 0.05, world, registry);
            check(!v.accepted, "and falling faster than anything falls is");
        }

        // ------------------------------------------------ what a claim crossed
        //
        // TODO.md OPEN-46: the validator judged where a claim ENDED. These are
        // claims whose ends are all legal, and whose paths are not all
        // possible. A wall a node thick and four tall at x = 40, z 26..34, on
        // the plaza, far from everything the checks around this one use.
        {
            const BlockId stone = registry.find("core:stone");
            const BlockId air   = registry.find("core:air");
            const auto    wall  = [&](BlockId id) {
                for (int z = 26; z <= 34; ++z)
                    for (int dy = 0; dy < 4; ++dy) world.set_block(40, (int)ground + dy, z, id);
            };
            // A player who has been standing still, with the budget that gives.
            const auto standing = [&](float fx, float fy, float fz) {
                MovementState at = last;
                at.x = fx; at.y = fy; at.z = fz;
                at.budget_h = MovementLimits{}.max_horizontal * MovementLimits{}.tolerance;
                at.budget_v = MovementLimits{}.max_vertical * MovementLimits{}.tolerance;
                return at;
            };

            // The test owns its ground. The plaza is flat but not empty: the
            // scatter stands columns of dirt on it, one in about eighty, and the
            // first version of this put a wall beside one at (40, 20..21, 35) --
            // so "round the end of the wall" was a body walking into a pillar,
            // and was refused for being honest about it. Cleared, so the world
            // here is exactly what the comments say it is.
            for (int z = 24; z <= 48; ++z)
                for (int x = 34; x <= 48; ++x)
                    for (int dy = 0; dy <= 6; ++dy) world.set_block(x, (int)ground + dy, z, air);

            wall(stone);

            // Eight nodes in one claim, straight across it. Both ends are on
            // the plaza and neither is inside anything.
            const MovementVerdict through =
                check_move(standing(36.5f, ground, 30.5f), 44.5f, ground, 30.5f, 0.2, world,
                           registry);
            std::printf("        eight nodes straight across a wall: %s (%s)\n",
                        through.accepted ? "accepted" : "refused", through.reason.c_str());
            check(!through.accepted,
                  "a claim on the far side of a wall no way round could reach is refused");

            MovementState graced_there = standing(36.5f, ground, 30.5f);
            graced_there.grace_until   = 1.0;
            check(check_move(graced_there, 44.5f, ground, 30.5f, 0.2, world, registry).accepted,
                  "and accepted when the server moved the body there itself");

            // A body the server has just placed has been nowhere
            // (MovementState::placed): its first claim is judged by where it
            // ends, as every claim was before the path, and only the next is
            // walked. Out of its grace window, so it is the mark that decides.
            MovementState placed_there = standing(36.5f, ground, 30.5f);
            placed_there.placed        = true;
            check(check_move(placed_there, 44.5f, ground, 30.5f, 0.2, world, registry).accepted,
                  "a first claim after the server placed the body is judged by where it ends");

            // Round the end of it in one claim: along x past the wall's end at
            // z = 35, then back along z. The straight line between the two
            // positions goes through the wall; the body did not.
            const MovementVerdict corner =
                check_move(standing(38.5f, ground, 35.8f), 41.8f, ground, 32.5f, 0.2, world,
                           registry);
            check(corner.accepted,
                  "round the end of a wall in one claim is accepted, though the straight line "
                  "between the two positions goes through it");

            // Over a wall one node high: up first, then across.
            for (int z = 40; z <= 44; ++z) world.set_block(40, (int)ground, z, stone);
            const MovementVerdict over =
                check_move(standing(36.5f, ground, 42.0f), 44.5f, ground + 1.2f, 42.0f, 0.2,
                           world, registry);
            check(over.accepted, "and so is rising over a wall one node high and on across");
            for (int z = 40; z <= 44; ++z) world.set_block(40, (int)ground, z, air);

            // Off a block one node high, a little way sideways and all the way
            // down in one claim -- what four_clients' walker sends stepping off
            // a dirt column, and what every walk that drops before it moves
            // over refused: its box is still over the block when it tries.
            world.set_block(44, (int)ground, 38, stone);
            const MovementVerdict off =
                check_move(standing(44.5f, ground + 1.0f, 38.5f), 45.3f, ground, 38.5f, 0.2,
                           world, registry);
            std::printf("        off a block one high, 0.8 across and 1 down in one claim: %s%s%s\n",
                        off.accepted ? "accepted" : "refused (", off.reason.c_str(),
                        off.accepted ? "" : ")");
            check(off.accepted, "and stepping down off a block, across before down, is accepted");
            world.set_block(44, (int)ground, 38, air);

            // The control: the first claim again, with nothing in the way. A
            // refusal here would be the check refusing distance, not walls.
            wall(air);
            const MovementVerdict open =
                check_move(standing(36.5f, ground, 30.5f), 44.5f, ground, 30.5f, 0.2, world,
                           registry);
            check(open.accepted, "and the same eight nodes with the wall taken away are accepted");

            // The limit, stated: that first claim is also what a body at
            // 160 m/s sends if it swerves round the far end of the wall within
            // one claim -- out along z, across, and back. A path that turns back
            // on an axis between two claims is not one the three walks find, so
            // it is refused as though it had gone through. Nothing at a player's
            // speed can turn back within a twentieth of a second.
        }

        // ------------------------------------------------ the portal's grace
        //
        // A crossing breaks this validator twice: it moves the player twenty
        // blocks in a tick, and what they carry out of it is a fall's worth of
        // speed on the horizontal axis, which the cap forbids. DESIGN.md 3.4
        // answers both with a window the SERVER opens when it moves somebody
        // itself. Every case here is the same claim with the window open and
        // shut, because a grace that is never checked shut is a cap removed.
        {
            const float far_x = 64.0f + 20.0f;   // a crossing, in one tick

            const MovementVerdict without =
                check_move(last, far_x, ground, 64.0f, 0.05, world, registry);
            check(!without.accepted, "a twenty block crossing is refused without a grace");

            MovementState graced = last;
            graced.grace_until   = 1.0;   // the server moved them; the window is open
            const MovementVerdict with =
                check_move(graced, far_x, ground, 64.0f, 0.05, world, registry);
            check(with.accepted, "and accepted with one");

            // The speed carried out of it: terminal velocity turned sideways,
            // 60 blocks a second against a cap of 22.4.
            const float carried = 64.0f + kTerminalFallSpeed * 0.05f;
            const MovementVerdict fast_without =
                check_move(last, carried, ground, 64.0f, 0.05, world, registry);
            check(!fast_without.accepted,
                  "a fall's worth of speed on the horizontal axis is refused without one");
            const MovementVerdict fast_with =
                check_move(graced, carried, ground, 64.0f, 0.05, world, registry);
            check(fast_with.accepted, "and accepted with one");

            // The window shuts by the clock, not by being spent. The same
            // tick's worth of movement, one tick after the window closed --
            // and it has to be a TICK, because the allowance is per second and
            // waiting long enough makes any distance legal on its own.
            MovementState after = graced;
            after.when          = 1.45;
            const MovementVerdict expired =
                check_move(after, carried, ground, 64.0f, 1.5, world, registry);
            check(!expired.accepted, "and the window shuts on its own, by the clock");

            // What a grace is not: a licence. Inside a block is refused with
            // the window wide open, because a teleport that has to break the
            // world's own rule is not one this server performs.
            MovementState in_wall = graced;
            const MovementVerdict buried =
                check_move(in_wall, 64.0f, ground - 3.0f, 64.0f, 0.05, world, registry);
            check(!buried.accepted, "a graced claim inside a block is still refused");
        }

        // ------------------------------- the node that holds a floor portal up
        //
        // The one place where a claim inside a solid node is not a cheat. The
        // client raises the bottom of the collision box so the player sinks
        // into the node a floor portal sits in; without the exemption the
        // server refuses that claim, and the player is corrected once per claim
        // for a portal they placed themselves -- 97 of them in five seconds,
        // measured in play before this existed.
        {
            // A node of the ground, and a claim a tenth of a node into it: what
            // sinking through a floor portal looks like from the server's side.
            const float sunk_y = ground - 0.1f;

            const MovementVerdict refused =
                check_move(last, 64.5f, sunk_y, 64.5f, 0.05, world, registry);
            check(!refused.accepted, "sinking into the ground is refused with no portal there");

            SolidExemption hole;
            hole.min_x = 64.0f; hole.max_x = 65.0f;
            hole.min_z = 64.0f; hole.max_z = 65.0f;
            hole.min_y = ground - 1.0f; hole.max_y = ground;

            const MovementVerdict allowed =
                check_move(last, 64.5f, sunk_y, 64.5f, 0.05, world, registry, MovementLimits{},
                           &hole, 1);
            check(allowed.accepted, "and accepted when that node is the one holding a portal up");

            // The exemption is one node, not a direction. A player who has sunk
            // past it is underground rather than passing through, and the check
            // says so.
            const MovementVerdict deeper =
                check_move(last, 64.5f, ground - 1.5f, 64.5f, 0.05, world, registry,
                           MovementLimits{}, &hole, 1);
            check(!deeper.accepted, "a node deeper than the opening is still inside the world");

            // And it is the node, not the neighbourhood. Standing on the corner
            // where four nodes meet puts half the box over the node beside the
            // hole, which is solid ground the player is standing on -- and
            // exempting it would open a tunnel one node wide beside every floor
            // portal. Measured in play as the case that made a spawn unusable.
            const MovementVerdict beside =
                check_move(last, 64.0f, sunk_y, 64.0f, 0.05, world, registry, MovementLimits{},
                           &hole, 1);
            check(!beside.accepted,
                  "half a box over the node beside the hole is still inside that node");
        }

        // Going quiet and coming back. Without a clamp on the interval, a
        // client that says nothing for a minute has earned a minute of travel.
        {
            const MovementVerdict v = check_move(last, 64.0f + 300.0f, ground, 64.0f, 60.0, world,
                                                 registry);
            check(!v.accepted, "a minute of silence does not buy a teleport");
            std::printf("        refused: %s\n", v.reason.c_str());
        }

        // Inside the ground. A speed check alone would let a player walk
        // through a wall as long as they did it slowly.
        {
            const MovementVerdict v = check_move(last, 64.0f, ground - 3.0f, 64.0f, 5.0, world,
                                                 registry);
            check(!v.accepted, "and neither does walking slowly into solid stone");
            std::printf("        refused: %s\n", v.reason.c_str());
        }

        // A player who has never been placed has to start somewhere.
        {
            MovementState fresh;
            const MovementVerdict v = check_move(fresh, 64.0f, ground, 64.0f, 0.0, world, registry);
            check(v.accepted, "a player's first claim is taken as given");
        }

        // The box is the client's box: standing with your head just under a
        // ceiling is legal, and a server that rounded it in would refuse every
        // player in a two-high tunnel.
        {
            World  tunnel;
            tunnel.set_block(0, 0, 0, 1);   // floor
            tunnel.set_block(0, 3, 0, 1);   // ceiling, two blocks of headroom
            NodeRegistry r2;
            register_core_nodes(r2);
            check(!inside_solid(tunnel, r2, 0.5f, 1.0f, 0.5f),
                  "standing in a two-high tunnel is not standing inside a block");
            check(inside_solid(tunnel, r2, 0.5f, 3.0f, 0.5f), "standing in the ceiling is");
        }
    }

    std::printf("\n%s\n", g_failures == 0 ? "all chunk codec checks passed"
                                          : "CHUNK CODEC CHECKS FAILED");
    return g_failures == 0 ? 0 : 1;
}
