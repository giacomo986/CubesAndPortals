#pragma once

// What a body is carrying.
//
// A component and not a field on a session, and that is the same argument that
// made Body a tag (core/ecs/components.h): a chest has an inventory and no
// session, an observer has a session and no inventory -- DESIGN.md 4.4 says so
// in as many words, "scope OBSERVE = niente avatar, niente inventario". Written
// as "the entity's Inventory" both cases are one sentence; written as a field
// on Session the chest needs a second mechanism.
//
// It lives in core/ and not in the server because DESIGN.md 3.4 is explicit
// that inventories are server-side only. The single-process client is its own
// server, so the same code runs there and the rule is not two implementations
// that have to agree.

#include <algorithm>
#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include "core/item/item_registry.h"
#include "core/item/slot_rules.h"

namespace cnp {

// One slot's worth. An empty slot is kNoItem with a count of 0, never a count
// of 0 with an item still named in it -- see Inventory::normalise, which is the
// one place that invariant is enforced.
// A stack's enchantments, one byte: two bits of level, 0..3, for each of four
// kinds -- Efficiency in the lowest two, then Sharpness, Unbreaking, Protection.
// A thing may have all four at once.
enum class Enchant : uint8_t { Efficiency = 1, Sharpness = 2, Unbreaking = 3, Protection = 4 };
inline uint8_t enchant_level_for(uint8_t ench, Enchant kind)
{
    return (uint8_t)((ench >> (2 * ((int)kind - 1))) & 3);
}
inline uint8_t enchant_with(uint8_t ench, Enchant kind, int level)
{
    const int shift = 2 * ((int)kind - 1);
    return (uint8_t)((ench & ~(3 << shift)) | ((level & 3) << shift));
}
// The highest of them, for how bright a glint is.
inline uint8_t enchant_level(uint8_t ench)
{
    uint8_t top = 0;
    for (int k = 1; k <= 4; ++k) top = std::max(top, enchant_level_for(ench, (Enchant)k));
    return top;
}
// What bag format 3 and protocol 61 held -- one kind, kind * 4 + level, kind 0
// both of the first two -- as this byte.
inline uint8_t enchant_from_v3(uint8_t old)
{
    const int kind = old >> 2, level = old & 3;
    if (level == 0) return 0;
    if (kind == 0) return enchant_with(enchant_with(0, Enchant::Efficiency, level), Enchant::Sharpness, level);
    if (kind > 4) return 0;
    return enchant_with(0, (Enchant)kind, level);
}

// A stack's TAG: which one thing it is, and its colours. The low 17 bits are
// a mod's to number with (mods/backpack: which backpack, so its contents follow
// it); the three fields of 5 bits above them are its three colours, each 0 for
// none or 1..16 for one of the sixteen dyes (kDyeColours), set at a table by
// dyes (RecipeSet::add_dyeable). 0 on anything new; a stack with one merges
// only with a stack of the same one.
inline constexpr uint32_t kTagIdBits = 17;
inline constexpr uint32_t kTagIdMask = (1u << kTagIdBits) - 1;
inline int tag_colour(uint32_t tag, int i)   // i 0..2; 0 none, 1..16 a dye
{
    return (int)((tag >> (kTagIdBits + 5 * i)) & 31);
}
inline uint32_t tag_with_colour(uint32_t tag, int i, int dye)
{
    const uint32_t shift = kTagIdBits + 5u * (uint32_t)i;
    return (tag & ~(31u << shift)) | ((uint32_t)(dye & 31) << shift);
}
// The classic block game's sixteen, in the order mods/colors registers its dyes:
// white, orange, magenta, light blue, yellow, lime, pink, gray, light gray,
// cyan, purple, blue, brown, green, red, black.
inline constexpr uint8_t kDyeColours[16][3] = {
    {234, 236, 236}, {240, 118, 20}, {190, 68, 180}, {58, 176, 218},
    {248, 198, 40},  {112, 186, 26}, {238, 142, 172}, {62, 68, 72},
    {142, 142, 136}, {22, 138, 146}, {122, 42, 172},  {52, 56, 158},
    {114, 72, 40},   {84, 110, 28},  {160, 38, 34},   {20, 20, 24},
};

struct ItemStack {
    ItemId   item  = kNoItem;
    uint16_t count = 0;
    uint16_t used  = 0;   // how much of ItemDef::durability is spent; 0 on anything new
    uint8_t  ench  = 0;   // its enchantments, two bits a kind (core.enchant_held); 0 on anything new
    uint32_t tag   = 0;   // which one it is, and its colours (tag_colour); 0 on anything new

    bool empty() const { return item == kNoItem || count == 0; }
    // Whether two stacks are the same kind of thing: what may share a slot.
    bool same_as(const ItemStack &o) const
    {
        return item == o.item && used == o.used && ench == o.ench && tag == o.tag;
    }
};

class Inventory {
public:
    // Nine, because that is the row along the bottom of the screen every player
    // of this kind of game already knows, and the rest of the bag is a second
    // page of the same thing rather than a different structure.
    static constexpr int kHotbar = 9;
    static constexpr int kDefaultSlots = 36;   // the hotbar plus three rows

    // And a player's bag has five squares more after those: what is WORN, head
    // to feet and then the back (kWornHead .. kWornBack). They hold one thing each, and only
    // something whose ItemDef::wear names that square -- add() never fills
    // them, and a move into one of something else is refused.
    static constexpr int kWorn = 5;

    // Then two ARM squares, left and right: open while the back wears a piece
    // with arms (ItemDef::arms, mods/armpack), each holding one thing that is
    // not worn -- a tool more. Past the worn ones, so add() and take_by_item
    // leave them alone as they leave the worn ones.
    static constexpr int kArms = kMaxArms;

    // A player's bag: the 36, the five worn squares and the two arms. A bag
    // saved with fewer loads into it as it was, the rest empty.
    Inventory() : m_slots((size_t)(kDefaultSlots + kWorn + kArms)), m_worn_from(kDefaultSlots) {}

    // Any other inventory -- a chest, a grid -- with nothing worn in it.
    explicit Inventory(int slots);

    int  size() const { return (int)m_slots.size(); }

    // More slots, empty, after the ones there are -- or fewer, and what was
    // in the slots cut off is GONE: a caller shrinking a bag empties those
    // slots first (core.container_spill with a first slot).
    void resize(int slots)
    {
        if (slots >= 1) m_slots.resize((size_t)slots);
    }
    const ItemStack &at(int slot) const;
    const std::vector<ItemStack> &slots() const { return m_slots; }

    // Which hotbar slot the hand is on. Clamped rather than rejected: a client
    // that asks for slot 40 gets slot 8, because the alternative is a refusal
    // path for something that is not an attack, only a stale message.
    int  selected() const { return m_selected; }
    void select(int slot);

    // Puts `count` of `item` in, filling partial stacks of the same item before
    // taking an empty slot. Returns what did NOT fit, so a caller can decide
    // whether the remainder falls on the floor or the action is refused -- the
    // one thing it must not do is quietly vanish.
    // An empty `prefer` slot is filled before the other empty ones.
    // A `used` other than 0 is a worn piece or a used tool, and an `ench` other
    // than 0 an enchanted one: it never merges, it takes an empty slot as it
    // is. A `tag` merges only with stacks of the same tag.
    // Only into squares whose role is in `roles`: by default everything but a
    // Product square, which only the block itself fills.
    uint16_t add(const ItemRegistry &items, ItemId item, uint16_t count, int prefer = -1,
                 uint16_t used = 0, uint8_t ench = 0, uint32_t tag = 0, unsigned roles = kRolesIn);

    // What each square is for (SlotRole), past the end Store, and what each
    // role takes. Set by the ContainerSet from the block at the container's
    // cell, every time it is looked up -- they are the block's, and are never
    // saved with the bag.
    void             set_rules(const SlotRules &r) { m_rules = r; }
    const SlotRules &rules() const { return m_rules; }
    uint8_t          role(int slot) const
    {
        return slot >= 0 && (size_t)slot < m_rules.roles.size() ? m_rules.roles[(size_t)slot]
                                                                : (uint8_t)kRoleStore;
    }
    // Whether that square's role takes that item (SlotRules::accepts).
    bool accepts(const ItemRegistry &items, int slot, ItemId item) const;

    // What a mod's core.take_item and core.give_item do: the same as take() and
    // add(), but THE HAND FIRST. Taking starts at the held slot and giving
    // fills an empty hand before any other empty slot (after partial stacks). A mod swapping one item
    // for another -- a bucket for a full one -- then swaps the one being held;
    // scanning from slot 0 emptied whichever full bucket came first in the bag
    // and put the new one wherever there was room (seen in play, 2026-09-19).
    // take_by_item returns how many came out; give_to_hand what did NOT fit.
    int      take_by_item(ItemId item, int count);
    uint16_t give_to_hand(const ItemRegistry &items, ItemId item, uint16_t count);

    // Takes `count` off one slot. Returns false and changes nothing if the slot
    // does not hold that many: a partial spend is how an inventory ends up
    // paying for something the player did not get.
    bool take(int slot, uint16_t count);

    // Takes a stack, or part of one, OUT of the bag and hands it back.
    //
    // `count` of 0 means all of it, and a count larger than the slot holds
    // takes what is there rather than refusing: unlike take(), whose caller is
    // paying for something and must not half-pay, this one's caller is a player
    // saying "get rid of that", and a request for more than they have is a
    // client whose picture of the bag is one round trip old.
    //
    // Returns what came out, empty if nothing did -- so a caller that has to
    // put it somewhere cannot forget how much it got.
    ItemStack take_from(int slot, uint16_t count = 0);

    // What the hand is holding, and spending one of it. Placing a block is the
    // only caller today and it is worth its own name, because "take(selected(),
    // 1)" at every call site is one place away from taking from the wrong slot.
    const ItemStack &held() const { return at(m_selected); }
    bool             spend_held(uint16_t count = 1);

    // Moves a stack, or part of one, from one slot to another. `count` of 0
    // means the whole thing, which is what a click on a slot means.
    //
    // What happens at the far end depends on what is already there, and all
    // three cases are the ones a player expects: an empty slot takes it, a slot
    // holding the SAME item takes as much as it has room for and leaves the
    // rest behind, and a slot holding something else SWAPS -- which is only
    // possible when the whole stack is moving, because half a stack has nowhere
    // to swap into.
    //
    // Returns false and changes nothing when it cannot be done. A partial move
    // that half-happens is how an inventory loses items.
    bool move_to(const ItemRegistry &items, int from, int to, uint16_t count = 0);

    // How many of an item are in here, across every slot. Used by gates and by
    // anything that asks "can this be afforded" before committing to it.
    uint32_t count_of(ItemId item) const;

    // The one slot, for the callers that move stacks in and out from outside.
    // Non-const because move_between writes through it; everything else should
    // be using at().
    ItemStack &slot(int i);

    // Whether a square is one of the worn ones, and whether `item` may go in
    // it: anything may go in an ordinary square, only the right piece in a worn
    // one, and nothing at all is always allowed.
    bool worn(int slot) const { return m_worn_from >= 0 && slot >= m_worn_from; }
    bool fits(const ItemRegistry &items, int slot, ItemId item) const;

    // Whether a square is one of the arms (past the worn ones), how many the
    // piece on the back opens, and whether the back is held on by them: a
    // piece whose arms hold something does not come off -- not into the bag,
    // a chest or the ground -- until they are emptied, so nothing is left in
    // a square nobody can see.
    bool arm(int slot) const { return m_worn_from >= 0 && slot >= m_worn_from + kWorn; }
    int  arms_open(const ItemRegistry &items) const;
    bool pinned(const ItemRegistry &items, int slot) const;

    // A container someone carries (kCarriedBelow, a backpack's) takes nothing
    // worn on a back: a backpack put in itself would be gone for good.
    void carried(bool on) { m_carried = on; }
    bool takes(const ItemRegistry &items, ItemId item) const
    {
        return !m_carried || item == kNoItem || items.get(item).wear != kWornBack;
    }

    // The armour points of everything worn, summed.
    int armor(const ItemRegistry &items) const;

    // A blow landed: every worn piece that wears out spends `amount` of its
    // durability, and one that runs out is gone. Returns whether anything
    // changed, so a caller knows the bag has to be told again.
    bool wear_out(const ItemRegistry &items, uint16_t amount);

    // One use of the stack in that slot: `amount` of its durability spent, and
    // gone when it runs out. False for an empty slot or one that never wears.
    bool wear_slot(const ItemRegistry &items, int slot, uint16_t amount);

    // A shift-click with nothing open: a piece in the bag goes on, if its
    // square is free; a worn one comes off into the bag, if there is room.
    bool swap_worn(const ItemRegistry &items, int slot);

private:
    void normalise(ItemStack &s) const;

    std::vector<ItemStack> m_slots;
    SlotRules              m_rules;
    int                    m_selected  = 0;
    uint32_t               m_wear_draws = 0;   // Unbreaking's draws, see wear_slot
    int                    m_worn_from = -1;
    bool                   m_carried   = false;
};

// Moves a stack from one inventory into another -- a bag into a chest, or back.
//
// The same three cases move_to has and the same all-or-nothing rule, because
// they are the same question asked across two objects rather than one. A
// separate function rather than a method, because neither inventory is more the
// subject than the other: `bag.move_to(chest, ...)` reads as though the bag
// owns the outcome, and it does not.
//
// The two may be the SAME inventory, which is what a drag inside one bag is:
// that case delegates to move_to rather than aliasing two references to one
// object, which is how a swap turns into a copy.
bool move_between(const ItemRegistry &items, Inventory &from_bag, int from, Inventory &to_bag,
                  int to, uint16_t count = 0);

// Moves a whole stack out of one slot and into another inventory, WHEREVER it
// fits -- what a shift-click means.
//
// There is no destination slot, and that is the whole difference from
// move_between: the player did not point at one. So this fills partial stacks
// first and takes an empty square after, which is Inventory::add's rule and not
// a second one written here.
//
// What will not fit goes BACK in the square it came from, never on the floor
// and never nowhere. A bag with room for two of a stack of five moves two and
// leaves three where they were, which is the answer a player can see; the
// alternative is a quiet three-item hole.
//
// Returns whether anything moved. The two inventories must be different
// objects: "wherever it fits" inside one bag would find the square the stack is
// already in.
bool quick_move(const ItemRegistry &items, Inventory &from_bag, int from, Inventory &to_bag);

} // namespace cnp
