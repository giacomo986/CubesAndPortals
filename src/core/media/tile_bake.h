#pragma once

#include "core/registry/node_registry.h"

#include <cstdint>
#include <string>
#include <vector>

// The placeholder a block gets when nobody drew one for it.
//
// One function, called from two places on purpose: the baker writes it to a
// file, and the renderer falls back to it for a block whose file is missing. If
// they were two copies, a block drawn from disk and the same block drawn from
// the fallback would differ, and the difference would be invisible until
// somebody deleted a file.
//
// Deterministic from the node's NAME, not from its id: ids are assigned in
// registration order and shift when a mod is added, and a texture that changes
// because somebody installed an unrelated mod is a texture nobody can keep.

namespace cnp {

// What one tile is, everywhere. 32 is what the atlas has always used.
inline constexpr int kTilePixels = 32;

// RGBA8, `size * size * 4` bytes, row 0 at the top.
//
// `border` darkens the outermost ring, which is what makes the edge of a block
// visible at all in a world of one colour. It is also what HIDES whether the
// noise underneath wraps: the seam between a tile and its own repeat falls
// exactly on that ring, so a tile that does not wrap looks fine and tiles
// wrong the moment anything else is drawn there. Turning it off is how the
// wrap becomes measurable -- and the reason it is a parameter and not a test
// hook is that real art comes with its own edges and wants none of this.
std::vector<uint8_t> bake_tile(const NodeDef &def, int size = kTilePixels, bool border = true);

// The same placeholder, with a MARK that says which face it is.
//
// A placeholder is one colour and some mottling, which is the same on all six
// faces -- so a block that TURNS has nothing to show for it: a log lying down
// looks exactly like a log standing up, and the turn is measurable in a gate
// and invisible in the game. The mark is what makes the orientation something
// an eye can check.
//
// `word` is a face group, as `face_texture_name` spells it: "top" draws the
// rings of a cut log, "front" a bullseye. A word this does not know comes back
// as the plain tile rather than as an error -- the caller is a baker walking a
// block table, not a parser.
std::vector<uint8_t> bake_face_tile(const NodeDef &def, const std::string &word,
                                    int size = kTilePixels, bool border = true);

// The file name a node's texture has, derived from the node name: the colon
// that separates owner from block is not a filename character everywhere, and a
// name that works on one machine and not another is worse than an ugly one.
std::string texture_file_name(const std::string &node_name);

} // namespace cnp
