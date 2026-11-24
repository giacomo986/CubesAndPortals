#pragma once

#include <string>

namespace cnp {

class WorldDb;
class NodeRegistry;

// The stored world from above: one PNG per 256 x 256 nodes and an index.html,
// into `dir`. Returns the process exit code.
int write_map_tiles(const WorldDb &db, const NodeRegistry &reg, const std::string &dir);

} // namespace cnp
