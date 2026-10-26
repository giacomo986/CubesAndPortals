#pragma once

#include "core/media/face_tiles.h"
#include "core/registry/node_registry.h"
#include "core/track/piece.h"

#include <algorithm>
#include <cstdint>

// Where a cell's block actually IS, as boxes.
//
// `NodeDef::parts` arrived with the chest and reached the mesher only: a block
// could declare boxes inside its cell, the renderer drew them, and the
// collision resolver, the server's movement validator and the raycast went on
// treating the whole cell as full. That is a shape which is a picture -- a slab
// stood on half a node above itself, with a wall of air beside it and an aim
// that selected it by pointing at the sky.
//
// This is the one place that answers "where are the boxes", so that what is
// drawn, what stops a body and what a ray finds are the same three boxes. The
// rotation is `orientation_matrix`, which is the mesher's own (media/
// face_tiles.h): a box is corners in the cell, and turning corners by a table
// of face permutations does not turn them at all.
//
// A block with NO boxes is the whole cell, and it is one box here rather than a
// special case at every call site. Every block this engine ships is that block.

namespace cnp {

// A box in world coordinates. Not physics/aabb.h's AABB: this header is under
// the registry, which the voxel core and the physics both sit on top of, and
// the two of them would otherwise have to agree through a type neither owns.
struct NodeBox {
    float min[3] = {0.0f, 0.0f, 0.0f};
    float max[3] = {1.0f, 1.0f, 1.0f};
};

// How many boxes a block is. One for a cube, which is nearly every block.
inline int node_shape_count(const NodeDef &def)
{
    return def.parts.empty() ? 1 : (int)def.parts.size();
}

// Whether this block is anything other than the whole cell. The hot paths ask
// it first so that a world of cubes pays one comparison and no arithmetic.
inline bool node_has_shape(const NodeDef &def)
{
    return !def.parts.empty() || def.turn == NodeTurn::Height || def.piece.plan != 0;
}

// Box `i` of the cell at (bx, by, bz), turned the way the cell is turned.
//
// The rotation is about the MIDDLE of the cell, which is where the renderer
// turns it: about a corner it would leave the cell. Rotations of a cube are
// signed permutations of the axes, so the corners of a box map onto the corners
// of a box and the result here is exact rather than a bound.
inline NodeBox node_shape_box(const NodeDef &def, uint8_t facing, int i, int bx, int by, int bz)
{
    NodeBox out;

    const float base[3] = {(float)bx, (float)by, (float)bz};

    // The cells of a track piece: a box from the floor up, as full as the byte
    // says, or as the road over the anchor's own column is (core/track/piece.h)
    // -- and none at all where that road is under the anchor's cell, or there
    // is no road: a box of no height, which nothing is inside.
    if (def.turn == NodeTurn::Height || def.piece.plan != 0) {
        int top = (int)facing;
        if (def.piece.plan != 0 && !piece_column(def.piece, facing, 0, 0, top)) top = 0;
        top = std::clamp(top, 0, 32);
        for (int a = 0; a < 3; ++a) {
            out.min[a] = base[a];
            out.max[a] = base[a] + 1.0f;
        }
        out.max[1] = base[1] + (float)top / 32.0f;
        return out;
    }

    if (def.parts.empty() || i < 0 || i >= (int)def.parts.size()) {
        for (int a = 0; a < 3; ++a) {
            out.min[a] = base[a];
            out.max[a] = base[a] + 1.0f;
        }
        return out;
    }

    const NodeDef::Box &box = def.parts[(size_t)i];

    const int o = node_cell_orientation(def.turn, facing);
    int       rot[3][3];
    orientation_matrix(o, rot);

    float lo[3] = {1e30f, 1e30f, 1e30f};
    float hi[3] = {-1e30f, -1e30f, -1e30f};

    for (int corner = 0; corner < 8; ++corner) {
        const float p[3] = {
            ((corner & 1) ? box.max[0] : box.min[0]) - 0.5f,
            ((corner & 2) ? box.max[1] : box.min[1]) - 0.5f,
            ((corner & 4) ? box.max[2] : box.min[2]) - 0.5f,
        };
        for (int a = 0; a < 3; ++a) {
            const float v = 0.5f + (float)rot[a][0] * p[0] + (float)rot[a][1] * p[1] +
                            (float)rot[a][2] * p[2];
            if (v < lo[a]) lo[a] = v;
            if (v > hi[a]) hi[a] = v;
        }
    }

    for (int a = 0; a < 3; ++a) {
        out.min[a] = base[a] + lo[a];
        out.max[a] = base[a] + hi[a];
    }
    return out;
}

} // namespace cnp
