#pragma once

#include "core/math/vec.h"
#include "core/registry/node_registry.h"

#include <cstdint>
#include <vector>

// A piece of a track, as stunt racers build one: a smooth road several nodes
// across that snaps to a grid and joins the piece beside it.
//
// A piece is a BLOCK in the corner cell of its footprint (the anchor, whose
// facing byte is one of four quarter turns) and fill blocks in every other
// cell under its road (NodeTurn::Height, the byte how full the cell is). So a
// piece is saved, sent and edited as blocks, and everything that reads cells --
// a foot, the server's checks, a ray, a way through the world -- sees the fill
// cells: boxes UNDER the road, never above it. What a kart drives on, and what
// is drawn, is the road itself, the triangles below, which both ends build
// from the same few numbers (PieceShape) and so get to the last bit.
//
// The grid is 8 nodes a side; a piece's origin is on it, its road 8 wide.
// Unturned, a piece is entered at its z = 0 edge heading +z: a straight runs
// along +z for `size` units, a turn bends right (-x) through a quarter circle
// about its origin and leaves by its x = 0 edge.
//
// `facing` is the anchor cell's byte, an upright block's (NodeTurn::Upright):
// the one a placement writes (node_placement_facing), which turns a front to
// the player -- so a piece is ENTERED from the player's side and runs away
// from them. Facing 2 is unturned.

namespace cnp {

class World;

inline constexpr int   kPieceUnit      = 8;      // nodes a side of a grid square
inline constexpr float kPieceBase      = 0.25f;  // the flat road above the origin cell's floor
inline constexpr int   kPieceMaxUnits  = 4;      // squares a side of the largest footprint: a wide loop's
inline constexpr int   kPieceMaxHeight = 18;     // cells the road may reach above its origin (a tall loop) ...
inline constexpr int   kPieceBelow     = 5;      // ... and below it: a bank's low edge, a wall's foot

bool piece_valid(const PieceShape &p);

// The footprint turned by `facing`, in nodes.
void piece_size(const PieceShape &p, uint8_t facing, int &nx, int &nz);

// The road's height above the origin cell's floor at (x, z) nodes from the
// origin, or false where there is no road.
bool piece_surface(const PieceShape &p, uint8_t facing, float x, float z, float &y);

// The lowest the road is over the column at (cx, cz), in 32nds of a node
// from the origin cell's floor, rounded down so a box never stands above the
// road; negative under a bank's low edge. False where the column has no road.
//
// The column's cells go from the origin's level (or the road's own cell, if
// that is lower) up to the road: full, and the top one as full as `top` says.
bool piece_column(const PieceShape &p, uint8_t facing, int cx, int cz, int &top);

enum : uint8_t { kPieceRoad = 0, kPieceCurb = 1, kPieceSide = 2 };

struct PieceTri {
    Vec3    v[3];   // from the origin cell's corner, counter-clockwise seen from outside
    uint8_t role   = kPieceRoad;
    uint8_t stripe = 0;   // which node along the road it is in, odd or even: a curb's paint
};

// The road (and curbs) a kart drives on, and the sides that are only drawn.
void piece_triangles(const PieceShape &p, uint8_t facing, std::vector<PieceTri> &out);

// How low and how high the piece reaches over its origin cell's floor, sides
// and all: the box an outline is drawn round, with piece_size.
void piece_heights(const PieceShape &p, uint8_t facing, float &lo, float &hi);

// The origin that puts the entry square of a piece over the cell (x, z).
void piece_origin(const PieceShape &p, uint8_t facing, int x, int z, int &ox, int &oz);

// And back: the corner cell of the entry square of a piece whose origin is
// (ox, oz) -- a cell piece_origin takes to that origin again.
void piece_entry(const PieceShape &p, uint8_t facing, int ox, int oz, int &x, int &z);

struct PieceCell {
    int     x = 0, y = 0, z = 0;
    BlockId id     = kAir;
    uint8_t facing = 0;
};

// Every cell a piece at this origin is: the anchor first, then the fill.
void piece_cells(const PieceShape &p, BlockId anchor, uint8_t facing, int ox, int oy, int oz,
                 std::vector<PieceCell> &out);

// Whether a piece can be put there: no cell of it is another piece's. Whatever
// else is in its cells, ground or a block somebody put, it takes the place of.
bool piece_has_room(const World &world, const NodeRegistry &reg, const std::vector<PieceCell> &cells);

// The cells over a piece's road that have to be emptied for a kart to drive it
// -- kPieceHeadroom over the top of each column, other pieces left alone --
// as air, added to `out`.
inline constexpr int kPieceHeadroom = 3;
void piece_headroom(const World &world, const NodeRegistry &reg, const std::vector<PieceCell> &cells,
                    std::vector<PieceCell> &out);

// Makes `id` a track piece of this shape whose other cells are `fill` (turned
// into NodeTurn::Height, dropping as the piece). The one door a mod has into
// this, on the server and the single-process client alike.
bool piece_declare(NodeRegistry &reg, BlockId id, BlockId fill, PieceShape p);

// The anchor of the piece whose cell (x, y, z) is, if one is.
bool piece_find(const World &world, const NodeRegistry &reg, int x, int y, int z, int &ox,
                int &oy, int &oz);

} // namespace cnp
