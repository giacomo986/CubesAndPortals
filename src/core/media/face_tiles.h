#pragma once

#include <array>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

// Which picture each face of each block wears, and which atlas tile that
// becomes.
//
// Three shapes of block, and the engine is told them by WHICH FILES EXIST
// rather than by a declaration:
//
//   one picture     core_stone.png                       all six faces
//   top/bottom/side core_grass_top.png, _bottom, _side   three
//   directional     core_furnace_front.png, _back, ...   up to six
//
// A face asks for its own file first, then its group's, then the block's plain
// one -- so a pack that supplies nothing new behaves exactly as before, and a
// pack that adds core_grass_top.png splits the block without anybody declaring
// that grass has three faces. The declaration would be a second place for the
// truth to live, and the two would disagree the first time a pack shipped a
// file the node table had not heard about.
//
// The tile assignment is here, away from the atlas, because it is the part with
// arithmetic in it: one tile per DISTINCT picture, a ceiling the vertex format
// imposes, and what happens at the ceiling. All three are checkable without a
// GPU, and none of them were before.

namespace cnp {

// The face order the mesher indexes: +X, -X, +Y, -Y, +Z, -Z.
enum FaceIndex {
    kFaceRight = 0,   // +X
    kFaceLeft,        // -X
    kFaceTop,         // +Y
    kFaceBottom,      // -Y
    kFaceFront,       // +Z
    kFaceBack,        // -Z
};

// A tile index is ELEVEN BITS in the vertex (`Vertex::tex` and the top three of
// `face_ao`, see pack_face_ao), so an atlas can hold 2048 pictures and no more.
// Written here rather than in the renderer because it is the vertex format's
// limit and not the texture's, and the place that has to respect it is the
// assignment below.
inline constexpr int kMaxAtlasTiles = 2048;

// Which faces a word after the block name covers, as a six-bit mask, or 0 for a
// word that is not one of them. "side" is the four upright faces; "all" is the
// six, and is what the block's own plain name already means.
uint8_t faces_named(const std::string &word);

// The files face `f` of `node_name` asks for, most specific first and without
// repeats. Always ends with the block's own plain file, so every chain has a
// last resort that a pack with no per-face art already satisfies.
std::vector<std::string> face_texture_names(const std::string &node_name, int face);

// The file a block wears on a named face or group: ("core:grass", "top") makes
// core_grass_top.png, and "all" makes the plain core_grass.png. This is what a
// pack's manifest key `core:grass:top` resolves to, so the mangling lives in one
// place and a pack and the atlas cannot disagree about it.
std::string face_texture_name(const std::string &node_name, const std::string &word);

// Every file any face of any block could ask for, in a stable order. What a
// server has to offer if a pack of its is to reach a client whole.
std::vector<std::string> all_texture_names(const std::string &node_name);

// The four upright faces in the order a quarter turn about +Y moves them:
// +X -> -Z -> -X -> +Z -> +X. Derived rather than chosen: rotating (1,0,0) by
// ninety degrees about +Y gives (0,0,-1), and the ring follows from there.
inline constexpr int kTurnRing[4] = {kFaceRight, kFaceBack, kFaceLeft, kFaceFront};

// Where face `f` of a block ends up after `turn` quarter turns. The top and the
// bottom do not move; the four upright ones go round the ring.
//
// Inline because the MESHER asks it now, once per exposed face of every turned
// cell, and the alternative to inlining it is a second copy of the ring beside
// the hot path -- which is the block table's own hazard applied to face
// numbering: two tables that agree until somebody edits one.
inline int turned_face(int face, int turn)
{
    if (face == kFaceTop || face == kFaceBottom) return face;

    for (int i = 0; i < 4; ++i)
        if (kTurnRing[i] == face) return kTurnRing[(unsigned)(i + turn) & 3u];

    return face;
}

// ------------------------------------------------------------------ turning
//
// Twenty-four orientations, because four are a furnace and a chest and not a
// piston, a stair or a log lying down (TODO.md OPEN-27). An orientation is
//
//     axis * 4 + turn
//
// where `axis` says where the block's own +Y ends up -- 0 +Y, 1 +Z, 2 -Z, 3 +X,
// 4 -X, 5 -Y -- and `turn` is quarter turns about that new up, counted the way
// the four upright turns above already count.
//
// **0..3 are exactly the four turns that existed**, since axis 0 is no tilt at
// all. That is not a coincidence to be grateful for, it is the reason the
// numbering is this way round: every facing byte in every world file and every
// chunk on the wire keeps the meaning it had.
//
// The axis order is the original engine's facedir order, so that M11's compatibility layer
// can hand a param2 straight through instead of carrying a table of its own.

inline constexpr int kOrientations = 24;

// The unit normal of each face, in the face order above.
inline constexpr int kFaceNormal[6][3] = {
    {1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1},
};

namespace detail {

// A rotation, as the three rows of an integer 3x3 matrix. Integer because these
// are quarter turns of a cube: every entry is -1, 0 or 1, and floating point
// would buy rounding where there is nothing to round.
struct Rot3 {
    int m[3][3]{};
};

constexpr Rot3 rot_identity()
{
    return Rot3{{{1, 0, 0}, {0, 1, 0}, {0, 0, 1}}};
}

// One quarter turn about +Y, and it is DERIVED from kTurnRing rather than
// chosen: the ring says +X goes to -Z, and this is the matrix that does that.
constexpr Rot3 rot_y1()
{
    return Rot3{{{0, 0, 1}, {0, 1, 0}, {-1, 0, 0}}};
}

constexpr Rot3 rot_mul(const Rot3 &a, const Rot3 &b)
{
    Rot3 out{};
    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 3; ++j) {
            int sum = 0;
            for (int k = 0; k < 3; ++k) sum += a.m[i][k] * b.m[k][j];
            out.m[i][j] = sum;
        }
    }
    return out;
}

// The tilt that puts the block's +Y on each axis. Written as matrices and not
// as a table of face permutations, because a permutation table is six numbers
// somebody has to get right and a rotation is three rows that can be checked
// against what it does to (0,1,0).
constexpr Rot3 rot_axis(int axis)
{
    switch (axis) {
    case 0: return rot_identity();                                  // +Y, no tilt
    case 1: return Rot3{{{1, 0, 0}, {0, 0, -1}, {0, 1, 0}}};         // +Y -> +Z
    case 2: return Rot3{{{1, 0, 0}, {0, 0, 1}, {0, -1, 0}}};         // +Y -> -Z
    case 3: return Rot3{{{0, 1, 0}, {-1, 0, 0}, {0, 0, 1}}};         // +Y -> +X
    case 4: return Rot3{{{0, -1, 0}, {1, 0, 0}, {0, 0, 1}}};         // +Y -> -X
    default: return Rot3{{{1, 0, 0}, {0, -1, 0}, {0, 0, -1}}};       // +Y -> -Y
    }
}

// The turn happens in the block's own frame and the tilt after it, which is the
// same thing as turning about the NEW up: B * Y1 = (B * Y1 * B^-1) * B, and
// that conjugate is the rotation about B(+Y). Said here because "about which
// axis" is the question somebody will ask of this line.
constexpr Rot3 rot_for(int orientation)
{
    Rot3 turn = rot_identity();
    for (int i = 0; i < (orientation & 3); ++i) turn = rot_mul(rot_y1(), turn);
    return rot_mul(rot_axis(orientation >> 2), turn);
}

constexpr int face_of_normal(int x, int y, int z)
{
    for (int f = 0; f < 6; ++f) {
        if (kFaceNormal[f][0] == x && kFaceNormal[f][1] == y && kFaceNormal[f][2] == z) return f;
    }
    return -1;
}

// Where each face goes, for each orientation. 144 bytes, built at compile time.
constexpr std::array<std::array<uint8_t, 6>, kOrientations> build_oriented()
{
    std::array<std::array<uint8_t, 6>, kOrientations> table{};
    for (int o = 0; o < kOrientations; ++o) {
        const Rot3 r = rot_for(o);
        for (int f = 0; f < 6; ++f) {
            const int x = r.m[0][0] * kFaceNormal[f][0] + r.m[0][1] * kFaceNormal[f][1] +
                          r.m[0][2] * kFaceNormal[f][2];
            const int y = r.m[1][0] * kFaceNormal[f][0] + r.m[1][1] * kFaceNormal[f][1] +
                          r.m[1][2] * kFaceNormal[f][2];
            const int z = r.m[2][0] * kFaceNormal[f][0] + r.m[2][1] * kFaceNormal[f][1] +
                          r.m[2][2] * kFaceNormal[f][2];
            table[(size_t)o][(size_t)f] = (uint8_t)face_of_normal(x, y, z);
        }
    }
    return table;
}

// And the inverse, which is what the MESHER asks: given a face of the cell,
// which face of the block is showing there. Built by turning the forward table
// round rather than by inverting matrices, so the two cannot disagree.
constexpr std::array<std::array<uint8_t, 6>, kOrientations> build_face_source()
{
    const auto forward = build_oriented();
    std::array<std::array<uint8_t, 6>, kOrientations> table{};
    for (int o = 0; o < kOrientations; ++o) {
        for (int f = 0; f < 6; ++f) table[(size_t)o][(size_t)forward[(size_t)o][(size_t)f]] =
                                        (uint8_t)f;
    }
    return table;
}

inline constexpr auto kOriented    = build_oriented();
inline constexpr auto kFaceSource  = build_face_source();

// Which orientation points the block's FRONT -- +Z before it is turned -- at
// each face. The first one that does, so the answer is canonical; there are
// four for every direction, differing by a spin about the front itself, and
// nothing in this engine can tell them apart yet.
constexpr std::array<uint8_t, 6> build_front_to()
{
    std::array<uint8_t, 6> table{};
    for (int dir = 0; dir < 6; ++dir) {
        table[(size_t)dir] = 0;
        for (int o = 0; o < kOrientations; ++o) {
            if (kOriented[(size_t)o][kFaceFront] == (uint8_t)dir) {
                table[(size_t)dir] = (uint8_t)o;
                break;
            }
        }
    }
    return table;
}

inline constexpr auto kFrontTo = build_front_to();

} // namespace detail

// The rotation itself, as integers, for whoever has to turn GEOMETRY rather
// than pick a picture: a node's boxes are corners in the cell, and a box turned
// by a table of face permutations cannot be turned at all.
inline void orientation_matrix(int orientation, int m[3][3])
{
    const detail::Rot3 r =
        detail::rot_for((orientation >= 0 && orientation < kOrientations) ? orientation : 0);
    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 3; ++j) m[i][j] = r.m[i][j];
    }
}

// Which face has this unit normal, or -1. The mesher does not need it -- it
// knows which face it is looking at -- but a PLACEMENT does: the message
// carries the face somebody clicked as a normal.
inline int face_of_normal(int x, int y, int z)
{
    return detail::face_of_normal(x, y, z);
}

// Where face `f` of a block ends up under `orientation`.
inline int oriented_face_24(int face, int orientation)
{
    if (face < 0 || face > 5 || orientation < 0 || orientation >= kOrientations) return face;
    return detail::kOriented[(size_t)orientation][(size_t)face];
}

// Which face of the block shows on face `f` of a cell under `orientation`. This
// is the one the mesher asks, once per exposed face of every turned cell.
inline int face_source(int face, int orientation)
{
    if (face < 0 || face > 5 || orientation < 0 || orientation >= kOrientations) return face;
    return detail::kFaceSource[(size_t)orientation][(size_t)face];
}

// The orientation that points a block's front at `face`. What a wallmounted
// cell's byte means: the byte IS a face index -- where the front points -- and
// this turns it into one of the twenty-four for the mesher.
inline int orientation_facing(int face)
{
    if (face < 0 || face > 5) return 0;
    return detail::kFrontTo[(size_t)face];
}

// Which quarter turn puts a block's front -- +Z before it is turned -- back
// along the direction somebody is LOOKING, so the front ends up facing them.
// The horizontal axis with the larger component wins; a tie goes to z, which is
// the unturned direction and so the one that changes least.
int turn_towards(float look_x, float look_z);

// Where one tile's picture comes from.
struct TileSource {
    std::string file;       // empty means "bake this node's placeholder"
    int         node = 0;   // which node, for a baked one
};

// A block, as the planner needs to see it.
//
// A name and nothing else. It carried a base and a quarter turn while a block
// with a front was four blocks, and the planner permuted the base's face table
// into the other three. Turning is resolved at MESH time now, off the cell's
// own byte, so every block in this list asks for its own pictures.
struct NodeShape {
    std::string name;
};

struct TilePlan {
    std::vector<TileSource>             tiles;
    std::vector<std::array<uint16_t, 6>> face_tile;   // per node, in node id order

    // Per ITEM, in item id order. An item is one picture and not six: it is
    // never a cube, it is a square in a bag. Zero means "no picture of its
    // own", which is what an item whose file nobody supplied gets -- the bag
    // then falls back to the block it places, and to a plain square for a tool
    // that places nothing.
    std::vector<uint16_t> item_tile;

    int over_budget = 0;   // pictures the atlas had no room for
};

// What a source can supply for one file, and WHAT PICTURE it is.
//
// Empty means the source cannot supply that file, or supplied something the
// atlas cannot use. Anything else is an identity for the PICTURE: two names
// that come back with the same identity are one picture and share one tile.
//
// A bool would have been enough to answer "is there art here", and that is what
// this used to be -- which is why a pack whose manifest sent six blocks to one
// flat.png spent six tiles on six copies of it. The caller is the only one that
// can say what a file decodes to, so it is the caller that says what it IS; the
// planner only compares. What the identity is made of is the caller's business
// -- the client hashes the decoded pixels, a test can use the name itself.
using TileIdentity = std::function<std::string(const std::string &file)>;

// One tile per distinct picture.
//
// `picture_of(file)` answers what that file holds -- a caller that decodes
// inside it and remembers the result is what makes this one pass over the media
// rather than two.
//
// Nodes are walked in id order and faces in face order, so the assignment is
// deterministic: two runs of the same world produce the same atlas, and a diff
// between two atlases means something. Node 0 keeps tile 0, which is the loud
// red air is drawn in.
//
// At the ceiling a face falls back to a tile its own node already has, and to
// tile 0 if it has none. Counted, never silent: an atlas that quietly wrapped
// would draw stone wearing somebody else's picture.
TilePlan plan_tiles(const std::vector<NodeShape> &nodes, const TileIdentity &picture_of,
                    int max_tiles = kMaxAtlasTiles);

// The same, for a world where nothing turns. Every name is its own base.
TilePlan plan_tiles(const std::vector<std::string> &node_names, const TileIdentity &picture_of,
                    int max_tiles = kMaxAtlasTiles);

// And with the ITEMS as well, which share the same atlas and the same budget.
//
// One file each, asked for by the item's own name -- tools_pick.png for
// tools:pick -- so a pack dresses an item exactly the way it dresses a block,
// by a line in its manifest and without the engine learning a second kind of
// name. Items are walked AFTER the blocks so that adding an item cannot move a
// block's tile: a world whose items nobody drew is numbered exactly as it was.
//
// An item whose picture is one a block already wears shares that block's tile,
// for the reason every other sharing here exists: the budget is 256.
TilePlan plan_tiles(const std::vector<NodeShape> &nodes, const std::vector<std::string> &items,
                    const TileIdentity &picture_of, int max_tiles = kMaxAtlasTiles);

} // namespace cnp
