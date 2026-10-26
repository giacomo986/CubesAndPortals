#pragma once

#include "core/registry/node_registry.h"
#include "core/voxel/chunk.h"

#include <string>
#include <vector>

namespace cnp {

// Sunlight and block light, flood filled, four bits each (DESIGN.md 2.6).
//
// The model is the classic block game's and the original engine's: sunlight falls straight down at full
// strength through anything transparent and loses nothing doing it, then both
// channels spread sideways losing one level per block. Keeping the vertical
// fall free is what makes an open field uniformly bright instead of shading off
// with distance from the sky.
//
// This is the whole-region pass, used after generation. Incremental relight
// after a single edit is relight_around().
void compute_lighting(World &world, const NodeRegistry &reg,
                      int min_x, int min_y, int min_z,
                      int max_x, int max_y, int max_z);

// The light of one column, exactly, from the blocks around it.
//
// The pass above writes every cell of its box, and the cells near the box's
// sides get only the light that started inside it -- a seam, wherever the box
// ends. This one runs the pass over the column grown by the light's reach on
// both horizontal sides and writes back only the column, so every cell it
// writes has seen everything within fifteen blocks of it. Exact when those
// blocks exist, which is why a column is lit only once all eight around it have
// been generated (core/world/columns.h).
void compute_column_lighting(World &world, const NodeRegistry &reg, int cx, int cz,
                             int height);

// Whether a cell changing from one block to another can change the light at
// all.
//
// The pass reads exactly two things about a block -- whether it is opaque, and
// what it emits -- so two blocks that agree about both are two blocks the light
// cannot tell apart. Stone becoming stone underground, or one wall becoming
// another, is a relight that has nothing to find: TODO.md OPEN-29 measured that
// at 1.9 ms a cell, paid the same whether anything could have changed or not.
//
// Here and not in the caller, because "what the light depends on" is this
// file's business: a caller's copy of that list would be a second opinion that
// stops agreeing the day the pass reads a third thing.
bool light_unchanged(const NodeRegistry &reg, BlockId from, BlockId to);

// A run of writes whose light is done ONCE, at the end.
//
// A mod says what it wants a cell to be, a thousand cells at a time; relighting
// around each of them is a lighting pass per block, and that is what made a mod
// that builds anything at load unusable -- 640 cells cost 1.36 s against a
// 0.12 s baseline, and a whole chunk cost 69 s (OPEN-29).
//
// Boxes and not a list of cells, because the pass takes a box: cells near each
// other cost one pass between them. Boxes and not ONE box, because a mod that
// writes two cells at opposite ends of the world would otherwise relight
// everything in between. At most kLightBatchBoxes of them, so the work is
// bounded whatever a mod does; past that the cheapest pair is merged, which
// over-lights and never under-lights.
inline constexpr int kLightBatchBoxes = 16;

class LightBatch {
public:
    // A cell whose light may have changed. Ask light_unchanged first: this
    // cannot tell, and a batch full of writes that changed nothing is a pass
    // over a region that did not need one.
    void add(int x, int y, int z);

    bool   empty() const { return m_boxes.empty(); }
    size_t boxes() const { return m_boxes.size(); }

    // Each box as "x0,y0,z0..x1,y1,z1", for a log line about a flush that took
    // longer than it should have.
    std::string describe() const;
    void   clear() { m_boxes.clear(); }

    // Relights every box, expanded by the light radius, and empties the batch.
    // Returns how many cells the passes covered, which is the number worth
    // printing: it is what the batching actually bought.
    size_t flush(World &world, const NodeRegistry &reg);

private:
    struct Box {
        int min[3];
        int max[3];

        long long volume() const
        {
            return (long long)(max[0] - min[0] + 1) * (long long)(max[1] - min[1] + 1) *
                   (long long)(max[2] - min[2] + 1);
        }
    };

    static Box       joined(const Box &a, const Box &b);
    static long long growth(const Box &a, const Box &b);

    // What a box costs at flush time: its volume grown by the light radius on
    // every side, which is the only measure worth deciding on -- a box's own
    // volume says a line of cells is cheap, and a pass over that line is not.
    static long long expanded_volume(const Box &b);

    std::vector<Box> m_boxes;
};

// Recomputes lighting in a box around one edited block. The radius has to
// exceed the maximum light level or a removed block leaves a dark column behind
// it: light travels 15 blocks, so anything closer than that re-lights partially
// and leaves a seam.
void relight_around(World &world, const NodeRegistry &reg, int x, int y, int z,
                    int radius = kMaxLight + 1);


} // namespace cnp
