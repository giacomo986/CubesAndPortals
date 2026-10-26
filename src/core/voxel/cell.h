#pragma once

#include <cstdint>
#include <cstddef>

namespace cnp {

// A cell of the world, as a key.
//
// Two things belong to a cell rather than to a player: what is inside it
// (core/item/container.h) and what it remembers (core/world/node_meta.h). Both
// need the same key and the same hash, and this is that key -- one definition,
// because two hashes over three integers are two hashes that agree until one
// of them is tuned.
//
// `ContainerAt` was here first and is now an alias of this, so nothing at any
// of its ninety call sites had to change to stop being a second copy.
struct Cell {
    int32_t x = 0, y = 0, z = 0;

    bool operator==(const Cell &o) const { return x == o.x && y == o.y && z == o.z; }
};

struct CellHash {
    size_t operator()(const Cell &c) const
    {
        // Three 32-bit fields into one 64-bit hash. The multipliers are odd and
        // large so that two cells a block apart do not land in the same bucket,
        // which is the arrangement a wall of chests actually has.
        uint64_t h = (uint64_t)(uint32_t)c.x * 0x9E3779B97F4A7C15ull;
        h ^= (uint64_t)(uint32_t)c.y * 0xC2B2AE3D27D4EB4Full;
        h ^= (uint64_t)(uint32_t)c.z * 0x165667B19E3779F9ull;
        return (size_t)(h ^ (h >> 31));
    }
};

} // namespace cnp
