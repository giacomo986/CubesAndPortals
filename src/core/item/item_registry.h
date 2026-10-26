#pragma once

// What a player can hold, as data.
//
// DESIGN.md 8 puts inventory and crafting in M7, and M2's exit test has a box
// still open for the same reason: "un mod che aggiunge 20 blocchi e un item" --
// the twenty blocks work, the item had nowhere to be registered. This is that
// somewhere.
//
// It is a second registry rather than a field on NodeDef because the two sets
// only mostly overlap. Every block a player can hold has an item form, and the
// engine derives those rather than making a mod write them twice; but a pick,
// a stick and a bucket are items with no block, and a block form that only
// exists to be placed is not a thing anybody carries. Two tables, one of which
// adopts from the other.

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

#include "core/registry/node_registry.h"
#include "core/voxel/chunk.h"

namespace cnp {

// Dense, assigned in registration order, same bargain as BlockId: the hot path
// indexes a flat array instead of hashing a name.
using ItemId = uint16_t;

// Nothing, and it is a real id rather than a sentinel outside the range: an
// empty slot holds it, so every slot in an inventory is a valid ItemStack and
// there is no "is this slot real" question anywhere.
inline constexpr ItemId kNoItem = 0;

struct ItemDef {
    std::string name;    // "core:stone", namespaced like a block
    std::string owner;   // which mod registered it, empty for the engine's own

    // How many fit in one slot. Sixty-four is the number every player of this
    // kind of game already has in their head; a mod that wants a tool sets 1.
    uint16_t max_stack = 64;

    // The block this puts down, or kAir for an item that places nothing. This
    // is what makes an item form of a block a block form of an item and not a
    // second copy of it: the item does not describe the block, it names it.
    BlockId places = kAir;

    uint16_t tex = 0;   // icon layer; the block form borrows the block's own

    // How hard a block this digs. Zero is what a bare hand does, and it is what
    // every item that is really a block form is: a stack of stone in the hand
    // digs no better than the hand. See NodeDef::dig_level for why this is a
    // level and not a kind.
    uint8_t dig_level = 0;

    // How many times faster than a bare hand this digs. One is a bare hand,
    // which is what every item is unless it says otherwise. Anything at or
    // below zero is read as one: a tool that digs infinitely fast is a tool
    // that breaks the rule this exists to keep.
    float   dig_speed = 1.0f;

    // What this tool is FOR, by the same names a block uses for what it is made
    // of: a pick digs "stone", a shovel digs "soil". Its level and its speed
    // are its own against that material and a bare hand's against anything
    // else, which is what makes an axe not a pickaxe.
    //
    // Empty is a general tool -- worth its level and its speed against
    // everything -- and that is what every item registered before materials
    // existed is, so declaring them changed no tool that existed.
    std::string digs;

    // How many points a hit takes off a body while this is in the hand. Zero
    // is a fist's (ecs::kPunch), which is what every item that says nothing
    // is. The server's table decides: a hit names the slot and never a number.
    uint16_t damage = 0;

    // Where on a body this is worn, as a worn square of the player's bag: 0 is
    // nowhere (held, like everything else), then kWornHead .. kWornBack. And
    // how many points of armour it gives there, out of the 20 a full set of
    // the best one adds up to -- see ecs::armoured for what a point is worth.
    uint8_t wear  = 0;
    uint8_t armor = 0;

    // How much use this takes before it is gone -- for armour, a point a
    // blow. Zero never wears out, which is what everything that says nothing is.
    uint16_t durability = 0;

    // How many arm squares of the bag wearing this on the back opens
    // (Inventory::kArms): mods/armpack's two claws, each one tool more.
    uint8_t arms = 0;
};

inline constexpr uint8_t kWornHead = 1, kWornChest = 2, kWornLegs = 3, kWornFeet = 4,
                          kWornBack = 5;

// The most arm squares a back piece opens (ItemDef::arms, Inventory::kArms).
inline constexpr uint8_t kMaxArms = 2;

// A mod's word for it, as register_item takes it. Anything else is not worn.
inline uint8_t wear_slot(const char *name)
{
    const std::string n = name ? name : "";
    return n == "head" ? kWornHead : n == "chest" ? kWornChest : n == "legs" ? kWornLegs
         : n == "feet" ? kWornFeet : n == "back" ? kWornBack : 0;
}

class ItemRegistry {
public:
    ItemRegistry();   // registers "" as kNoItem, so slot 0 of the table is empty

    // Returns the id, or the existing id if the name is taken -- in which case
    // the definition is replaced. Same rule as NodeRegistry, for the same
    // reason: a reload must not renumber what is already in a chest.
    ItemId register_item(const ItemDef &def);

    // Every block gets an item form it does not already have. Called after the
    // mods have run and after each reload, so a mod that registers twenty
    // blocks gets twenty items without writing a line about items.
    //
    // Air is skipped: there is no such thing as holding nothing in particular.
    //
    // A name already in the table is left alone unless its item is an orphan
    // and the block is not: that is a block a reload dropped and a later one
    // brought back, and it becomes obtainable again on the same id rather than
    // staying dead behind a name nothing can re-mint.
    //
    // Returns how many became obtainable that were not before -- minted and
    // revived together, because a gate counting "what can a player get now"
    // does not care which of the two it was.
    size_t adopt_blocks(const NodeRegistry &nodes);

    const ItemDef &get(ItemId id) const
    {
        return (id < m_defs.size()) ? m_defs[id] : m_defs[0];
    }

    ItemId find(const std::string &name) const
    {
        auto it = m_by_name.find(name);
        return (it == m_by_name.end()) ? kNoItem : it->second;
    }

    // Which atlas tile this item's picture ended up in, written by whoever
    // built the atlas. Zero is "no picture of its own", which is what an item
    // nobody drew keeps: the bag then falls back to the block it places.
    //
    // A setter and not a field the planner fills in, for the reason
    // NodeRegistry::set_face_tiles is one: the numbering belongs to the atlas
    // and changes when the atlas is rebuilt, while the rest of a definition is
    // what a mod said and never changes under it.
    void set_icon(ItemId id, uint16_t tile)
    {
        if (id < m_defs.size()) m_defs[id].tex = tile;
    }

    uint16_t max_stack(ItemId id) const
    {
        const uint16_t n = get(id).max_stack;
        return n == 0 ? 1 : n;
    }

    // The id for a name that came off disk.
    //
    // A name the table already has is that id, whatever state it is in -- an
    // orphan included, because a stack that sat in a chest through a mod's
    // absence is still that stack and gets its id back.
    //
    // A name the table does NOT have is minted as an ORPHAN: unobtainable,
    // placing nothing, holding its id. That is the same answer end_reload gives
    // to an item whose mod stopped registering it, and it is the same question
    // one week apart instead of one second -- a world file written when a mod
    // was installed, read back when it is not. The alternative is dropping the
    // stack, which is a save file quietly deciding what somebody owns.
    //
    // A mod that comes back revives the name through adopt_blocks or
    // end_reload, on the id the stored stacks are already holding.
    //
    // The empty name is kNoItem, which is what an empty slot holds.
    ItemId adopt_stored(const std::string &name);

    // A name a saved bag or chest may still hold for an item that has another
    // now. Read only by adopt_stored, and only for a name nothing registers.
    void alias(const std::string &old_name, const std::string &name) { m_alias[old_name] = name; }

    // The item form of a block, for the drop a break produces. kNoItem if that
    // block has none, which is how a mod says "this cannot be picked up".
    ItemId of_block(BlockId id) const;

    // Reloading a mod, the same two calls as NodeRegistry. An item its mod
    // stopped registering is not turned magenta -- there is no world full of it
    // to keep standing up -- it simply becomes unobtainable and says so.
    // end_reload needs the blocks because most items are not registered at all:
    // they are block forms adopt_blocks minted, and a mod that re-registers its
    // blocks never touches the item table. Deciding orphanhood from the item
    // table alone made one reload orphan every block form a mod had -- see
    // M7_FINDINGS 5.
    void   begin_reload(const std::string &owner);
    size_t end_reload(const std::string &owner, const NodeRegistry &nodes);
    void   cancel_reload(const std::string &owner);

    size_t size() const { return m_defs.size(); }
    const std::vector<ItemDef> &all() const { return m_defs; }

    // Whether this item was dropped by a reload. An orphan can still sit in an
    // inventory -- that is the point of not renumbering -- but nothing new can
    // be made of it and it places nothing.
    bool is_orphan(ItemId id) const { return id < m_orphan.size() && m_orphan[id] != 0; }

private:
    std::vector<ItemDef> m_defs;
    std::unordered_map<std::string, ItemId> m_by_name;
    std::unordered_map<std::string, std::string> m_alias;
    std::vector<BlockId> m_block_of;   // parallel to m_defs
    std::unordered_map<BlockId, BlockId> m_drops_as;   // NodeDef::drops_as, by block
    std::unordered_map<BlockId, std::string> m_drops_item;   // NodeDef::drops_item
    std::vector<uint8_t> m_stale;
    std::vector<uint8_t> m_orphan;
};

} // namespace cnp
