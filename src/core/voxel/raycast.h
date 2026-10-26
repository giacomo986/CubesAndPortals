#pragma once

#include "core/math/vec.h"
#include "core/registry/node_registry.h"
#include "core/voxel/chunk.h"

namespace cnp {

struct RaycastHit {
    bool  hit = false;
    int   x = 0, y = 0, z = 0;      // the solid block that was hit
    int   nx = 0, ny = 0, nz = 0;   // face normal: add it to get the empty cell in front
    float distance = 0.0f;
};

// Amanatides and Woo grid traversal: step from cell to cell along the ray,
// always crossing the nearest boundary, so every voxel the ray passes through
// is visited exactly once and none are missed.
//
// Sampling points along the ray at a fixed interval is the tempting shortcut
// and it is wrong in two directions at once: too coarse and it tunnels through
// thin walls at glancing angles, too fine and it costs more than the exact
// method it approximates.
//
// `liquids` stops the ray at water as well as at solids: what a bucket is
// pointed at. Off for everything else, since a dig or a build aimed at a lake
// is aimed at its bed.
RaycastHit raycast(const World &world, const NodeRegistry &reg, Vec3 origin, Vec3 direction,
                   float max_distance, bool liquids = false);

} // namespace cnp
