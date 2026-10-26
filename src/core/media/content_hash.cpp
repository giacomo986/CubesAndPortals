#include "core/media/content_hash.h"

#include <monocypher.h>

#include <cstring>

namespace cnp {

bool ContentHash::operator==(const ContentHash &other) const
{
    // Constant time, not because a texture is a secret, but because this is the
    // comparison the cache uses to decide whether a file it just read is the
    // file it asked for -- and the one place where "close enough" would be a
    // way to slip bytes past it. crypto_verify32 returns 0 on equal.
    return crypto_verify32(bytes, other.bytes) == 0;
}

bool ContentHash::empty() const
{
    uint8_t any = 0;
    for (size_t i = 0; i < kContentHashBytes; ++i) any |= bytes[i];
    return any == 0;
}

static const char *kHexDigits = "0123456789abcdef";

std::string ContentHash::hex() const
{
    std::string out;
    out.reserve(kContentHashBytes * 2);
    for (size_t i = 0; i < kContentHashBytes; ++i) {
        out.push_back(kHexDigits[bytes[i] >> 4]);
        out.push_back(kHexDigits[bytes[i] & 0x0F]);
    }
    return out;
}

std::string ContentHash::short_hex() const
{
    return hex().substr(0, 8);
}

ContentHash hash_content(const void *data, size_t size)
{
    ContentHash h;
    crypto_blake2b(h.bytes, kContentHashBytes, (const uint8_t *)data, size);
    return h;
}

ContentHash hash_content(const std::vector<uint8_t> &data)
{
    return hash_content(data.data(), data.size());
}

static int hex_value(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

bool content_hash_from_hex(const std::string &hex, ContentHash &out)
{
    if (hex.size() != kContentHashBytes * 2) return false;
    for (size_t i = 0; i < kContentHashBytes; ++i) {
        const int hi = hex_value(hex[i * 2]);
        const int lo = hex_value(hex[i * 2 + 1]);
        if (hi < 0 || lo < 0) return false;
        out.bytes[i] = (uint8_t)((hi << 4) | lo);
    }
    return true;
}

size_t ContentHashHasher::operator()(const ContentHash &h) const
{
    size_t v = 0;
    for (size_t i = 0; i < sizeof(size_t) && i < kContentHashBytes; ++i)
        v |= (size_t)h.bytes[i] << (8 * i);
    return v;
}

} // namespace cnp
