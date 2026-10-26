#pragma once

// A way through the blocks for a body that walks: A* over the cells it could
// stand in.
//
// The blocks are read by their SHAPE (NodeDef::parts), each once a search:
//
//   full   a cube, a stair, a slab at the top of its cell: a wall;
//   low    a slab or anything whose boxes cover the floor of the cell up to at
//          most 0.55: a body stands IN that cell, on top of it;
//   thin   boxes covering under half the floor -- a ladder's panel, an open
//          door or trapdoor at the side of its cell: a body fits beside them;
//   post   a thin block standing across the middle of its cell higher than a
//          step -- a fence post and its arms, a pane, iron bars: a wall, and
//          nothing stood on from above, since no body fits beside it and a
//          way along the top of a fence is not one;
//   door   NodeDef::door: shut is a wall, unless the body opens doors;
//   open   nothing to stand on or bump into.
//
// A cell is STANDABLE for a body when the body fits with its feet on the
// floor of it -- the top of a low block in it, or of a full block under it --
// with `height` of room above that, and something holds it there: that floor,
// water round it for a body that swims, or a climbable block (a ladder) for a
// body that climbs. From a standable cell a body moves to one of the eight
// around it:
//
//   level      its feet rise at most a slab (0.55), which is a step and not
//              a jump: a slab, a stair taken a half at a time;
//   up         its feet rise up to `step_up` cells and a little more (a jump
//              reaches 1.28), with room over its head to jump;
//   down       its feet fall up to `drop`, through open cells;
//
// and a diagonal only when both of the cells it cuts past hold the body over
// the whole move, so a path never goes through the corner of a wall. On a
// ladder it also goes straight up and down.
//
// Nothing in here knows what a creature is. Whoever needs a way -- a creature
// chasing a player, a mod's pet following its owner, a villager going home --
// says how big the body is and how it moves, and gets cells back.
//
// The search is bounded (`max_nodes`): a way that does not exist is the
// expensive answer, and a caller that asks every second must not be able to
// spend the tick on it. When the budget runs out, or there is no way, the
// answer is the way to the cell that came closest -- somewhere to go that is
// nearer than here, which is what a chaser wants.

#include <cstdint>
#include <vector>

namespace cnp {

class World;
class NodeRegistry;

struct PathCell {
    int x = 0, y = 0, z = 0;   // the cell the feet are in
};

struct PathBody {
    int  height    = 2;      // cells of room it needs, feet up
    int  step_up   = 1;      // how many cells it climbs in one move
    int  drop      = 3;      // how far it will fall
    bool swim      = true;   // water holds it up; if false, water is a wall
    int  max_nodes = 2000;   // cells looked at before it gives up
    bool climbs    = true;   // goes up and down ladders (NodeDef::climbable)
    bool opens_doors = false;   // a shut door is a way through, for a little more

    // How much shortness it gives up for a quicker answer: the estimate of the
    // way left is multiplied by this. 1 is the shortest way; 1.5 looks at far
    // fewer cells and finds one at most half again as long, and usually within
    // a few percent -- what a creature running at somebody wants.
    float hurry = 1.0f;

    // Cells it does not step into, the goal excepted: where a body tried and
    // could not get in, whatever the cells say (a creature stuck, core/ecs).
    const std::vector<PathCell> *avoid = nullptr;
};

// Whether that cell is one the body avoids.
inline bool path_avoids(const PathBody &body, int x, int y, int z)
{
    if (body.avoid == nullptr) return false;
    for (const PathCell &c : *body.avoid)
        if (c.x == x && c.y == y && c.z == z) return true;
    return false;
}

struct PathResult {
    bool                  reached  = false;   // the goal itself; otherwise the closest
    int                   expanded = 0;       // cells looked at, for whoever measures
    std::vector<PathCell> cells;              // from the first step to the end; empty: stay
};

// Whether a body could stand with its feet in that cell.
bool path_standable(const World &world, const NodeRegistry &reg, int x, int y, int z,
                    const PathBody &body);

// Where a body at (x, y, z) -- feet, any height, perhaps in the air -- would
// come to stand: that cell or the first standable one under it within `drop`
// + 2. False when there is none.
bool path_ground(const World &world, const NodeRegistry &reg, int &x, int &y, int &z,
                 const PathBody &body);

// The way from one standable cell to another. Both are snapped to the ground
// first (path_ground); a start with no ground under it gives an empty answer.
PathResult find_path(const World &world, const NodeRegistry &reg, PathCell from, PathCell to,
                     const PathBody &body);

// The same way with the corners it does not need taken out: a cell is kept
// only where walking straight on from the last kept one (from `from` for the
// first) would leave the level or cross a cell the body cannot stand in. What
// a body steering at points walks; `cells` from find_path, which says every
// cell, is what a body stepping cell by cell walks.
std::vector<PathCell> path_smooth(const World &world, const NodeRegistry &reg, PathCell from,
                                  const std::vector<PathCell> &cells, const PathBody &body);

} // namespace cnp
