#pragma once

#include <string>

#include "core/registry/node_registry.h"
#include "core/voxel/chunk.h"

// Searching the world for a block by name.
//
// Written once and called from both mains, because the server and the client
// both run mods and a rule written twice is two rules -- this repo keeps
// finding that as the cause rather than as a style note (docs/archive/M8_FINDINGS.md).
//
// The original engine's equivalent is what an LBM is given. What it is FOR is the cells a
// mod did not put down: a mod that placed its own blocks could as easily have
// remembered where they went, and it is the ones a player placed, or a mapgen,
// that nobody wrote down.

namespace cnp {

// Fills `out` with x, y, z triples and returns how many NUMBERS were written,
// so a buffer of 3n holds n positions. The box is inclusive and may be given in
// any order; anything outside the world reads as air and simply does not match.
//
// Two refusals, and both are the caller's fault rather than the world's:
//
//   - a name no registry entry has. Returns 0 and says so, because a typo that
//     answers "there are none of those" is a mod that does nothing and a bug
//     report that says the search is broken.
//   - a box of more than kFindNodesMaxCells cells. The search runs inside the
//     tick and the interrupt budget that stops a runaway mod counts safepoints
//     in the VM, not time the engine spends on the mod's behalf.
//
// `why` is filled on a refusal and left alone otherwise.
inline constexpr long long kFindNodesMaxCells = 1LL << 20;   // a 101-node cube

int find_nodes(const World &world, const NodeRegistry &registry, int x1, int y1, int z1, int x2,
               int y2, int z2, const char *name, int *out, int max, std::string &why);

} // namespace cnp
