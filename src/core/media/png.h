#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

// PNG, written by the half of the engine that must build without a GPU.
//
// It is here and not in the client because the SERVER is the thing that owns a
// content set: DESIGN.md 4.5 has media addressed by hash and shipped from the
// server, so whatever produces a placeholder for a block whose mod shipped no
// art has to be reachable from a headless build. The client decodes with what
// its renderer already links; nothing here decodes anything.
//
// Deliberately the smallest encoder that produces a real PNG: one IDAT holding
// STORED deflate blocks -- no Huffman coding, no LZ77 window, no compression at
// all. That is a valid zlib stream and every decoder reads it. A 32x32 RGBA
// tile comes out at about 4.2 KB where a compressed one would be a few hundred
// bytes, which is the trade: a hundred lines of code that can be read in one
// sitting against a dependency this project does not have.
//
// What that means downstream, said here rather than discovered by measuring the
// wire: generated placeholder media are BIG for what they show. Real art comes
// out of a real encoder and is smaller. Nothing above this file cares -- media
// travel as opaque bytes with a hash on them.

namespace cnp {

// RGBA8, `width * height * 4` bytes, row 0 at the top. Returns an empty vector
// for a size that is not a picture.
std::vector<uint8_t> png_encode_rgba(const uint8_t *pixels, int width, int height);

// Writes it, creating nothing but the file: the directory has to be there.
// Returns false and fills `err` rather than reporting through errno at a caller
// that has already lost the context.
bool png_write_rgba(const std::string &path, const uint8_t *pixels, int width, int height,
                    std::string &err);

} // namespace cnp
