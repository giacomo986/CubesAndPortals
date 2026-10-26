#include "core/media/png.h"

#include <cstdio>
#include <cstring>

namespace cnp {
namespace {

uint32_t crc32_of(const uint8_t *data, size_t n, uint32_t crc = 0xFFFFFFFFu)
{
    // Built on first use rather than written out: a 256-entry table in the
    // source is 256 magic numbers nobody checks.
    static uint32_t table[256];
    static bool     built = false;
    if (!built) {
        for (uint32_t i = 0; i < 256; ++i) {
            uint32_t c = i;
            for (int k = 0; k < 8; ++k) c = (c & 1) ? (0xEDB88320u ^ (c >> 1)) : (c >> 1);
            table[i] = c;
        }
        built = true;
    }
    for (size_t i = 0; i < n; ++i) crc = table[(crc ^ data[i]) & 0xFFu] ^ (crc >> 8);
    return crc;
}

void be32(std::vector<uint8_t> &out, uint32_t v)
{
    out.push_back((uint8_t)(v >> 24));
    out.push_back((uint8_t)(v >> 16));
    out.push_back((uint8_t)(v >> 8));
    out.push_back((uint8_t)v);
}

void chunk(std::vector<uint8_t> &out, const char type[4], const std::vector<uint8_t> &body)
{
    be32(out, (uint32_t)body.size());

    const size_t start = out.size();
    out.insert(out.end(), type, type + 4);
    out.insert(out.end(), body.begin(), body.end());

    // The CRC covers the type AND the body, and not the length -- which is the
    // detail every hand-written PNG encoder gets wrong once.
    const uint32_t crc = crc32_of(&out[start], out.size() - start) ^ 0xFFFFFFFFu;
    be32(out, crc);
}

// zlib around STORED deflate blocks. Each block carries at most 65535 bytes and
// the last one is flagged, which is the whole of the format when nothing is
// being compressed.
std::vector<uint8_t> zlib_stored(const std::vector<uint8_t> &raw)
{
    std::vector<uint8_t> z;
    z.push_back(0x78);   // CM = deflate, CINFO = 32K window
    z.push_back(0x01);   // FCHECK making the two bytes a multiple of 31, no dictionary

    size_t at = 0;
    do {
        const size_t n    = raw.size() - at > 65535 ? 65535 : raw.size() - at;
        const bool   last = at + n >= raw.size();

        z.push_back(last ? 1 : 0);
        z.push_back((uint8_t)(n & 0xFF));
        z.push_back((uint8_t)(n >> 8));
        z.push_back((uint8_t)(~n & 0xFF));
        z.push_back((uint8_t)((~n >> 8) & 0xFF));
        z.insert(z.end(), raw.begin() + (long)at, raw.begin() + (long)(at + n));
        at += n;
    } while (at < raw.size());

    // Adler-32 of the uncompressed data, big endian, at the end.
    uint32_t a = 1, b = 0;
    for (const uint8_t byte : raw) {
        a = (a + byte) % 65521;
        b = (b + a) % 65521;
    }
    const uint32_t adler = (b << 16) | a;
    z.push_back((uint8_t)(adler >> 24));
    z.push_back((uint8_t)(adler >> 16));
    z.push_back((uint8_t)(adler >> 8));
    z.push_back((uint8_t)adler);
    return z;
}

} // namespace

std::vector<uint8_t> png_encode_rgba(const uint8_t *pixels, int width, int height)
{
    if (pixels == nullptr || width <= 0 || height <= 0) return {};
    if ((long long)width * height > 64LL * 1024 * 1024) return {};   // not a tile, not a mistake

    // Every row prefixed with filter type 0. Filters exist to make the deflate
    // that follows compress better, and nothing here compresses.
    std::vector<uint8_t> raw;
    raw.reserve((size_t)height * ((size_t)width * 4 + 1));
    for (int y = 0; y < height; ++y) {
        raw.push_back(0);
        const uint8_t *row = pixels + (size_t)y * (size_t)width * 4;
        raw.insert(raw.end(), row, row + (size_t)width * 4);
    }

    std::vector<uint8_t> out = {0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A};

    std::vector<uint8_t> ihdr;
    be32(ihdr, (uint32_t)width);
    be32(ihdr, (uint32_t)height);
    ihdr.push_back(8);   // bit depth
    ihdr.push_back(6);   // colour type: truecolour with alpha
    ihdr.push_back(0);   // deflate
    ihdr.push_back(0);   // adaptive filtering
    ihdr.push_back(0);   // no interlace
    chunk(out, "IHDR", ihdr);
    chunk(out, "IDAT", zlib_stored(raw));
    chunk(out, "IEND", {});
    return out;
}

bool png_write_rgba(const std::string &path, const uint8_t *pixels, int width, int height,
                    std::string &err)
{
    const std::vector<uint8_t> png = png_encode_rgba(pixels, width, height);
    if (png.empty()) {
        err = "nothing to write: " + std::to_string(width) + "x" + std::to_string(height);
        return false;
    }

    std::FILE *f = std::fopen(path.c_str(), "wb");
    if (f == nullptr) {
        err = "could not open " + path + " for writing";
        return false;
    }
    const size_t wrote = std::fwrite(png.data(), 1, png.size(), f);
    std::fclose(f);

    if (wrote != png.size()) {
        err = "wrote " + std::to_string(wrote) + " of " + std::to_string(png.size()) +
              " bytes to " + path;
        return false;
    }
    return true;
}

} // namespace cnp
