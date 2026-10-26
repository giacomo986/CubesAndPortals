#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

// What a piece of media IS, as opposed to what somebody called it.
//
// DESIGN.md 4.5: media are content-addressed and the cache is shared between
// every session, so two servers that send the same texture cost one download.
// That only works if the name of a file is computed from the file. A cache
// keyed by "core_stone.png" holds one entry for two worlds that drew different
// stone; a cache keyed by the hash holds two, and holds one when they drew the
// same stone -- which is the case the design is actually about.
//
// **BLAKE2b-256, not BLAKE3.** DESIGN.md names BLAKE3 and this is not it. The
// reason is that monocypher is already linked -- it is what seals the session
// and signs the tickets -- and it ships BLAKE2b; BLAKE3 would be a new
// dependency bought for a property nothing here needs. Both are 256-bit and
// collision-resistant, and the wire format carries 32 bytes either way, so
// swapping later costs a protocol version and no design. Written down because
// a silent deviation from the design document is worse than a stated one.
//
// The hash is over the file's bytes exactly as they travel. Not over the pixels
// it decodes to: two encoders that produce the same picture from different
// bytes are two different files, and a cache that pretended otherwise would
// have to decode everything it stores to find out.

namespace cnp {

inline constexpr size_t kContentHashBytes = 32;

struct ContentHash {
    uint8_t bytes[kContentHashBytes]{};

    bool operator==(const ContentHash &other) const;
    bool operator!=(const ContentHash &other) const { return !(*this == other); }

    // All zeroes: no content, as opposed to the hash of nothing, which is a
    // perfectly good 32 bytes and is not this.
    bool empty() const;

    std::string hex() const;

    // The first eight hex digits, for a log line a person reads. Never for
    // deciding anything: eight digits collide, and a cache that looked things
    // up by them would serve the wrong file about once in four billion.
    std::string short_hex() const;
};

ContentHash hash_content(const void *data, size_t size);
ContentHash hash_content(const std::vector<uint8_t> &data);

bool content_hash_from_hex(const std::string &hex, ContentHash &out);

// For unordered_map. Takes the first eight bytes, which are already uniformly
// distributed -- hashing a hash again would only cost time.
struct ContentHashHasher {
    size_t operator()(const ContentHash &h) const;
};

} // namespace cnp
