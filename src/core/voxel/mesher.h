#pragma once

#include "core/registry/node_registry.h"
#include "core/voxel/chunk.h"

#include <cstdint>
#include <vector>

namespace cnp {

// Twelve bytes: DESIGN.md 2.6's eight, and the light grown from one packed byte
// to two full ones (sky, lamp) with three left over to keep the stride a
// multiple of four.
//
// Everything is an unsigned byte, and every field is read back in the shader by
// converting the float the vertex fetch produced and pulling bits out of it.
// That is deliberate: it means the format needs no glVertexAttribIPointer,
// which rlgl does not expose, so the compact vertex costs no GL shim at all.
// Only texture arrays still need one.
//
// Positions are chunk-local and integral, 0..32 inclusive, because greedy
// meshing emits quads on block boundaries. Sub-block geometry (nodeboxes, the
// compat layer's drawtypes) will need a finer fixed point; the field is already
// a byte, so that is a scale change rather than a format change.
//
// The light is BRIGHTNESS and not a level: light_curve() of the smoothed level,
// 0..255. Per corner, the mean of the four cells meeting there, so a shadow's
// edge is a ramp and not a staircase. The lamp byte is also turned by which way
// the face looks at the light (facing_light in mesher.cpp): a wall facing a
// torch is lit, its back is not.
struct Vertex {
    uint8_t x, y, z;      // chunk-local position, 0..32
    uint8_t face_ao;      // face index in bits 0..2, ambient occlusion in 3..4, tile bits 8..10 in 5..7
    uint8_t u, v;         // texture coordinate in tiles, 0..32, wraps per block
    uint8_t sky;          // how much of the sky this corner sees, as brightness 0..255
    uint8_t tex;          // texture layer's low eight bits, from the node definition
    uint8_t lamp;         // block light reaching this corner, as brightness 0..255
    uint8_t classic;      // the face's light as it was: the packed byte of the cell it looks into
    uint8_t flow    = 0;  // a liquid's (PartVertex has its own); unused on a cube
    uint8_t foliage = 0;  // 1 on a block that takes the season's colour (NodeDef::foliage)
};

static_assert(sizeof(Vertex) == 12, "vertex layout is a contract with world_render.cpp");

// How bright a light level is, 0..1, for a level 0..15 (fractions allowed).
// Steep at the bottom, so the last few levels of a torch's reach fall off into
// the dark instead of lighting a whole cave grey: level 15 is 1, 7 is 0.23, 2
// is 0.04 and 0 is nothing. The classic block game's own curve.
inline float light_curve(float level)
{
    const float r = level / 15.0f;
    return r / (4.0f - 3.0f * r);
}

// What the darkest place still shows. Not zero: a player who sees nothing at
// all cannot tell a cave from a broken screen. The world shader says the same.
inline constexpr float kLightFloor = 0.02f;

// The world shader's rule for something drawn by another shader, from a cell's
// packed light: the brighter of the sky (scaled by the day) and the lamps, over
// the floor.
inline float light_shade(uint8_t packed, float daylight)
{
    const float sky  = light_curve((float)(packed >> 4)) * daylight;
    const float lamp = light_curve((float)(packed & 15));
    return kLightFloor + (1.0f - kLightFloor) * (sky > lamp ? sky : lamp);
}

// The tile's three high bits ride in the byte's top three, so a tile index is
// eleven bits (kMaxAtlasTiles) in a vertex that stays eight bytes.
inline uint8_t pack_face_ao(int face, int ao, int tile = 0)
{
    return (uint8_t)((face & 0x07) | ((ao & 0x03) << 3) | (((tile >> 8) & 0x07) << 5));
}

// A chunk plus a one-block skirt of its neighbours.
//
// Meshing reads every neighbour of every block, and going back to the world for
// each of those is a hash lookup per read. Copying once into a padded cube
// turns that into an array index -- and, more importantly, makes the mesher
// take no reference to the world at all, which is what lets it run on a worker
// thread while the main thread keeps editing (DESIGN.md 2.6).
struct ChunkSnapshot {
    // Two and not one: which way a cell's light runs is read from ITS
    // neighbours, and the cells whose light falls on a face are already one
    // out from the chunk.
    static constexpr int kPad = 2;
    static constexpr int kDim = kChunkSize + 2 * kPad;   // 36

    ChunkPos pos;
    std::vector<BlockId> blocks;   // kDim^3
    std::vector<uint8_t> light;

    // Which way each cell is turned, or EMPTY for a snapshot where nothing is.
    //
    // Empty and not a plane of zeroes, which is the same rule Chunk itself
    // keeps one layer down: almost every chunk in a world has nothing turned in
    // it, and a snapshot is already 118 KB before this. A caller reads it
    // through facing_at(), which answers 0 for the empty case, so the mesher
    // has one path and not two.
    std::vector<uint8_t> facing;

    static int index(int x, int y, int z)
    {
        return (x + kPad) + kDim * ((y + kPad) + kDim * (z + kPad));
    }

    // Coordinates run -kPad .. kChunkSize + kPad - 1.
    BlockId block(int x, int y, int z) const { return blocks[(size_t)index(x, y, z)]; }
    uint8_t light_at(int x, int y, int z) const { return light[(size_t)index(x, y, z)]; }
    uint8_t facing_at(int x, int y, int z) const
    {
        return facing.empty() ? (uint8_t)0 : facing[(size_t)index(x, y, z)];
    }
};

ChunkSnapshot snapshot_chunk(const World &world, ChunkPos pos);

// Same, into a snapshot the caller keeps. Meshing runs several times a frame
// forever, and allocating a 118 KB snapshot and a fresh vertex vector each time
// churns the allocator hard enough to show up as a slow climb in resident
// memory over a long session -- glibc does not hand the arena back.
void snapshot_chunk_into(const World &world, ChunkPos pos, ChunkSnapshot &out);

// Which of a chunk's six faces you can get to from which, through the chunk.
//
// This is what occlusion culling walks on. The renderer starts at the chunk the
// camera is in and only enters a neighbour if it could have crossed the chunk it
// came from -- in by one face, out by another. A chunk of solid rock joins none
// of its faces, so the walk stops there and the world behind the rock is never
// drawn.
//
// Six faces make fifteen unordered pairs, so the whole table is fifteen bits and
// travels with a mesh at no cost worth counting. Faces are numbered the way the
// mesher numbers them: 0 -x, 1 +x, 2 -y, 3 +y, 4 -z, 5 +z.
//
// Being wrong in the two directions costs differently, and that asymmetry is
// worth knowing before reading the code. Claiming two faces are joined when they
// are not costs draw calls. Claiming they are NOT joined when they are is a hole
// in the world -- a room the player can see into that is never drawn -- so
// where this is unsure it says joined.
// How many ways through one chunk the fine walk can carry, one per bit of the
// mask it passes from chunk to chunk. A chunk with more than this many of them
// keeps the coarse table above and drops the rest: over-draw, which is the
// direction this file is allowed to be wrong in.
inline constexpr int kMaxRegions  = 64;
inline constexpr int kFaceWords   = (kChunkSize * kChunkSize + 63) / 64;
inline constexpr int kCoarseDim   = kChunkSize / 4;
inline constexpr int kCoarseWords = (kCoarseDim * kCoarseDim * kCoarseDim + 63) / 64;

// Where a region meets one of the chunk's faces, cell by cell.
//
// This is what lets two chunks agree about a crossing: the walk goes from a
// region of one into a region of the other only where they share a cell of the
// plane between them. A face is indexed by the two axes that are not its own,
// in the order they come -- so the +x face of a chunk and the -x face of the
// chunk beyond it index the same (y, z) pair, which is the whole point.
struct FaceCells {
    uint8_t  face   = 0;
    uint8_t  region = 0;
    uint64_t cells[kFaceWords] = {};
};

// One connected piece of a chunk's transparent cells that reaches a face.
struct Region {
    // Which of the six it reaches. The walk leaves a region by these and by no
    // others, and that sentence is the whole of OPEN-23.
    uint8_t faces = 0;

    // Where the region is, to a quarter of the chunk's width: 8x8x8 blocks of
    // 4x4x4 cells. This is how the walk finds the region the camera stands in,
    // and it is coarse on purpose -- the exact answer is a region id per cell,
    // which is 32 KB a chunk kept for ever to answer one question a frame.
    // Two regions sharing a block of 64 cells are both returned and the walk
    // leaves by the faces of both, which over-draws and never hides.
    uint64_t coarse[kCoarseWords] = {};
};

struct ChunkVisibility {
    uint16_t bits = 0;

    // The pieces the chunk's air comes in, and which faces each of them
    // reaches. Empty for a chunk whose air is all one piece to the coarse table
    // -- there is nothing finer to say -- and empty for a chunk with more ways
    // through it than the mask can carry.
    //
    // Only a region that reaches a FACE is here. A sealed room is not a way
    // through the chunk and nothing crossing the chunk can enter one, so it
    // costs nothing and is not counted against the cap.
    std::vector<Region>    regions;

    // Grouped by face, with `face_begin[f] .. face_begin[f + 1]` the entries of
    // face f, so a crossing scans the one face it is crossing rather than the
    // whole table.
    std::vector<FaceCells> face_cells;
    uint16_t               face_begin[7] = {};

    // The bit for an unordered pair. Fifteen slots, a < b.
    static int slot(int a, int b)
    {
        if (a > b) { const int t = a; a = b; b = t; }
        return a * 5 - a * (a - 1) / 2 + (b - a - 1);
    }

    bool joined(int a, int b) const { return a == b || (bits & (1u << slot(a, b))) != 0; }
    void join(int a, int b) { if (a != b) bits = (uint16_t)(bits | (1u << slot(a, b))); }

    bool all_joined() const { return bits == 0x7FFF; }
    bool none_joined() const { return bits == 0; }

    // Whether there is anything finer than the fifteen bits to ask.
    bool fine() const { return !regions.empty(); }

    // The regions that reach a face, as a mask. What a chunk entered with no
    // idea of WHERE it was entered has to assume.
    uint64_t regions_touching(int face) const
    {
        uint64_t m = 0;
        for (size_t i = 0; i < regions.size(); ++i)
            if (regions[i].faces & (1u << face)) m |= 1ull << i;
        return m;
    }

    // The regions that may hold a chunk-local cell. More than one is possible
    // and is an over-draw, never a hole; none means the cell is in rock, or in
    // a room sealed away from every face.
    uint64_t regions_at(int x, int y, int z) const
    {
        const int cx = x / 4, cy = y / 4, cz = z / 4;
        if (cx < 0 || cy < 0 || cz < 0 || cx >= kCoarseDim || cy >= kCoarseDim || cz >= kCoarseDim)
            return 0;
        const int bit = (cx * kCoarseDim + cy) * kCoarseDim + cz;
        uint64_t  m   = 0;
        for (size_t i = 0; i < regions.size(); ++i)
            if (regions[i].coarse[bit >> 6] & (1ull << (bit & 63))) m |= 1ull << i;
        return m;
    }
};

// Flood fills the chunk's transparent cells and joins the faces each connected
// region touches.
//
// OPAQUE and not solid: sight and movement are different questions, and a wall
// of glass stops one and not the other. A chunk that closed its bores at a
// window would stop drawing the room on the far side of it.
//
// The snapshot's skirt is NOT walked. It holds a block of the neighbours, and
// letting their air join two of this chunk's faces would be describing somebody
// else's chunk.
//
// The regions are kept as well as summed, which is OPEN-23: the fifteen bits
// are the union of what every region can do, and a chunk holding a cave under
// the open sky answers that union -- everything joins everything -- however
// sealed the cave is. A walk carrying the region it is in crosses the cave and
// not the sky.
ChunkVisibility compute_visibility(const ChunkSnapshot &snapshot, const NodeRegistry &reg);

// Greedy meshing: coplanar faces that agree on block, light and ambient
// occlusion merge into one quad. A flat floor becomes a handful of triangles
// instead of two per block, which is most of why a voxel renderer is fast.
//
// Two streams out, not one. Translucent blocks have to be drawn after
// everything opaque and in back-to-front order, which is a different pass with
// different state, so they are separated here rather than sorted out at draw
// time: the mesher already knows which block every face belongs to, and the
// renderer would have to ask again per face.
std::vector<Vertex> mesh_snapshot(const ChunkSnapshot &snapshot, const NodeRegistry &reg);

// A cell that is NOT drawn as a cube.
//
// Everything the boxes need and nothing about the boxes themselves: what the
// block is says what its shape is, and the shape belongs to the block table
// rather than to every cell that wears it. The light is the cell's own, taken
// here because the mesher is holding the snapshot and whoever draws is not.
struct PartCell {
    uint8_t x = 0, y = 0, z = 0;   // chunk-local, 0..31
    BlockId block  = kAir;
    uint8_t facing = 0;
    uint8_t light  = 0;
};

// A cell of liquid, and the shape of its surface.
//
// Not a cube and not in either stream. A cube's vertices are bytes, one unit per
// node, and a cell of water running off a source is a cell with less in it and a
// top that slopes towards its emptier neighbours -- neither of which a byte can
// say. So the mesher hands over what the surface IS and the renderer builds it
// in floats, the way it builds a box.
//
// The heights are shared: each corner's is worked out from the four cells that
// meet at that corner, so two neighbours put the same height on the edge they
// share and the surface has no cracks along it.
struct LiquidCell {
    uint8_t x = 0, y = 0, z = 0;   // chunk-local, 0..31
    BlockId block = kAir;

    // The light on each face, by face index: the light of the cell that face
    // looks into, which is how a cube is lit, and not the cell's own -- a
    // channel's side in the shade of a wall is darker than its top in the sun.
    uint8_t light[6] = {};

    // And the top's light at each corner, indexed like `h`: the four cells over
    // the surface that meet at that corner, averaged, the opaque ones left out.
    // A lake is a wide flat thing of one texture, and lit a cell at a time the
    // edge of a shadow across it is a staircase.
    uint8_t top_light[4] = {};

    // Corner heights in sixteenths of a node, 0..16, indexed x + 2 z: [0] is the
    // corner at the cell's own (x, z), [1] at x + 1, [2] at z + 1, [3] at both.
    uint8_t h[4] = {16, 16, 16, 16};

    // Which faces to draw, bit f for face f in the order +X -X +Y -Y +Z -Z. A
    // face against more liquid or against a block that hides it is not drawn,
    // which is what keeps a lake from being a stack of translucent walls. A
    // cell inside the water has none and is handed over all the same.
    uint8_t faces = 0;
};

// Clears them but keeps their capacity, so a steady state stops allocating.
// `alpha` may be null, which is what a caller that has no alpha pass passes --
// its faces are then dropped rather than drawn in the wrong pass. `parts` may
// be null too, and then a block that is drawn as boxes is simply not drawn:
// leaving a hole is the honest answer for a caller with no pass to draw it in,
// where meshing it as a cube would be this engine quietly disagreeing with
// itself about the shape of a chest.
//
// `liquids` may be null as well, and then liquids are not drawn at all -- the
// same honest hole. They are never meshed as cubes any more.
void mesh_snapshot_into(const ChunkSnapshot &snapshot, const NodeRegistry &reg,
                        std::vector<Vertex> &out, std::vector<Vertex> *alpha = nullptr,
                        std::vector<PartCell> *parts = nullptr,
                        std::vector<LiquidCell> *liquids = nullptr);

} // namespace cnp
