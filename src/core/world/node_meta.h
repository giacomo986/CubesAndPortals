#pragma once

#include "core/voxel/cell.h"

#include <string>
#include <unordered_map>
#include <vector>

// What a cell remembers.
//
// DESIGN.md 8 gives M8 "trait sui nodi", and the four classic portal-puzzle blocks are what
// that means in practice: a clock holds a count, a vent holds whether it is
// enabled, a pedestal holds what is on display. Three of the four cannot be
// written at all without a cell that keeps a value -- see docs/archive/M8_FINDINGS.md
// 1.2, where the mods themselves say so.
//
// **Strings, and only strings.** Not because numbers are hard, but because a
// number is often the wrong thing to store: a pedestal holding item id 41 is
// holding whatever id 41 means the next time the world is opened, and ids are
// assigned in registration order. A name survives a mod being added. A mod that
// wants a number writes `tostring` and reads `tonumber`, which is one call it
// can see rather than a conversion the engine does behind it.
//
// Bounded, for the reason ContainerSet is bounded: a mod opens these, and a mod
// that writes one per block it places would otherwise fill memory. Refused
// rather than evicted -- throwing away the oldest cell's state is throwing away
// what it remembered.

namespace cnp {

inline constexpr size_t kMaxMetaCells      = 65536;
inline constexpr size_t kMaxMetaKeysPerCell = 16;
inline constexpr size_t kMaxMetaKeyBytes   = 64;
inline constexpr size_t kMaxMetaValueBytes = 256;

class NodeMetaSet {
public:
    // Writes one field. An empty value ERASES the field, which is the same
    // thing "there is nothing here" means when it is read back -- so a mod has
    // one way to say it rather than two that a reader would have to tell apart.
    //
    // Returns false only when the write is refused: too long, too many fields
    // on this cell, or a world already holding as many cells as it may. `why`
    // says which.
    bool set(const Cell &at, const std::string &key, const std::string &value, std::string &why);

    // Empty when there is no such field, which is not an error: most cells
    // remember nothing and that is the right answer for them.
    std::string get(const Cell &at, const std::string &key) const;

    // Everything a cell remembers, gone. What breaking a block does -- see
    // write_block in the server: a cell that has become air has no state, and a
    // new block placed there must not inherit the last one's.
    bool clear(const Cell &at);

    size_t cells() const { return m_cells.size(); }
    size_t fields() const;

    // For saving. A vector rather than exposing the map, because the storage
    // layer should not be able to write into this while iterating it.
    struct Field {
        Cell        at;
        std::string key;
        std::string value;
    };
    std::vector<Field> all() const;

    // For loading. Bypasses nothing: the same limits apply, so a file holding
    // more than a world may hold loads what fits and says how much it dropped.
    size_t load(const std::vector<Field> &fields);

    void clear_all() { m_cells.clear(); }

private:
    using Fields = std::unordered_map<std::string, std::string>;
    std::unordered_map<Cell, Fields, CellHash> m_cells;
};

} // namespace cnp
