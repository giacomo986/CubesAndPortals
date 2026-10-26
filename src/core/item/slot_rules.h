#pragma once

// What the squares of a container are for (core.container_roles and
// core.container_accepts). Its own header because a block declares them
// (NodeDef::slot_rules) and a bag obeys them (Inventory), and the registry of
// blocks is under the one of items, which is under the bag.

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace cnp {

// What a square of a container is for (core.container_roles), declared per
// block by the mod that makes it: a furnace's ore, its coal, what it made.
// A block that says nothing is all Store, which is every chest.
//
// It is what machinery goes by -- a belt, a hopper, an extractor -- and not a
// lock on what a player may do, except for one rule: nobody puts anything into
// a Product square. Only the block makes what is in it (Inventory::add with
// kRolesProduct).
enum SlotRole : uint8_t {
    kRoleStore   = 0,   // magazzino: in and out, by anybody
    kRoleRaw     = 1,   // materia prima: what the block works on
    kRoleFuel    = 2,   // materiale di consumo: what it burns doing it
    kRoleProduct = 3,   // prodotto finito: what came out, to be taken away
    kRoleSource  = 4,   // a catalogue's square: taking from it takes a copy, it keeps its own
    kRoleTrash   = 5,   // what is put in it is gone
};
inline constexpr int kRoleCount = 6;
inline constexpr unsigned kRolesIn      = (1u << kRoleStore) | (1u << kRoleRaw) | (1u << kRoleFuel);
inline constexpr unsigned kRolesOut     = (1u << kRoleStore) | (1u << kRoleProduct);
inline constexpr unsigned kRolesProduct = 1u << kRoleProduct;

// What the squares of a block's container are: each one's role, and for a
// role, the items it takes (by name; none listed: anything). A furnace's ore
// row takes what smelts and its coal row what burns (core.container_accepts).
struct SlotRules {
    std::vector<uint8_t>                    roles;
    std::array<std::vector<std::string>, kRoleCount> accepts;
    bool operator==(const SlotRules &) const = default;
};

// A role by the name a mod gives it -- "store", "raw", "fuel", "product" --
// or -1. And the squares a mod's core.container_add may fill: its role's, or,
// for none (null or ""), kRolesIn; 0 for a name that is none of them.
int      slot_role(const char *name);
unsigned role_mask(const char *name);
// The squares a mod's container_count and container_take look in: its role's,
// or for none every square; 0 for a name that is not a role.
inline unsigned role_mask_any(const char *name) { return name == nullptr || *name == '\0' ? 0x3Fu : role_mask(name); }

} // namespace cnp
