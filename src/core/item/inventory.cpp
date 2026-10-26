#include "core/item/inventory.h"

#include <cstring>

namespace cnp {

Inventory::Inventory(int slots)
{
    if (slots < 1) slots = 1;
    m_slots.resize((size_t)slots);
}

const ItemStack &Inventory::at(int slot) const
{
    // Out of range answers with an empty stack rather than asserting. A slot
    // index arrives from a client, and a client that sends a bad one is a
    // message to ignore, not a server to stop.
    static const ItemStack kEmpty;
    if (slot < 0 || slot >= (int)m_slots.size()) return kEmpty;
    return m_slots[(size_t)slot];
}

void Inventory::select(int slot)
{
    if (slot < 0) slot = 0;
    if (slot >= kHotbar) slot = kHotbar - 1;
    if (slot >= (int)m_slots.size()) slot = (int)m_slots.size() - 1;
    m_selected = slot;
}

void Inventory::normalise(ItemStack &s) const
{
    if (s.count == 0 || s.item == kNoItem) {
        s.item  = kNoItem;
        s.count = 0;
        s.used  = 0;
        s.ench  = 0;
        s.tag   = 0;
    }
}

uint16_t Inventory::add(const ItemRegistry &items, ItemId item, uint16_t count, int prefer,
                        uint16_t used, uint8_t ench, uint32_t tag, unsigned roles)
{
    if (item == kNoItem || count == 0) return 0;
    if (!takes(items, item)) return count;

    const uint16_t cap = items.max_stack(item);

    // Partial stacks of the same thing first. Without this pass a player with
    // eight slots of one stone each fills the bag with eight stones, which is
    // the behaviour every one of these games gets right and every first draft
    // of one gets wrong.
    for (int i = 0; i < size() && count > 0; ++i) {
        ItemStack &s = m_slots[(size_t)i];
        // A tag merges with the same tag: stolen wheat with stolen wheat.
        if (used != 0 || ench != 0) break;
        if (worn(i) || s.item != item || s.count >= cap || s.used != 0 || s.ench != 0 || s.tag != tag ||
            !((roles >> role(i)) & 1u) || !accepts(items, i, item))
            continue;

        const uint16_t room = (uint16_t)(cap - s.count);
        const uint16_t put  = count < room ? count : room;
        s.count             = (uint16_t)(s.count + put);
        count               = (uint16_t)(count - put);
    }

    // Then the empty slots, `prefer` before the others.
    for (int k = -1; k < size() && count > 0; ++k) {
        const int slot = k < 0 ? prefer : k;
        if (slot < 0 || slot >= size() || (k >= 0 && slot == prefer) || worn(slot) ||
            !((roles >> role(slot)) & 1u) || !accepts(items, slot, item))
            continue;

        ItemStack &s = m_slots[(size_t)slot];
        if (!s.empty()) continue;

        const uint16_t put = count < cap ? count : cap;
        s.item             = item;
        s.count            = put;
        s.used             = used;
        s.ench             = ench;
        s.tag              = tag;
        count              = (uint16_t)(count - put);
    }

    // Whatever is left over is returned, not dropped on the floor here: where
    // it goes is the caller's decision and this has no idea where the body is.
    return count;
}

int Inventory::take_by_item(ItemId item, int count)
{
    if (m_slots.empty()) return 0;

    int taken = 0;
    for (int k = -1; k < size() && taken < count; ++k) {
        const int slot = k < 0 ? m_selected : k;
        if (k >= 0 && slot == m_selected) continue;   // already had its turn
        if (worn(slot)) continue;                      // not off somebody's back

        const ItemStack &in = m_slots[(size_t)slot];
        if (in.empty() || in.item != item) continue;

        const int want = count - taken;
        const int here = (int)in.count < want ? (int)in.count : want;
        if (take(slot, (uint16_t)here)) taken += here;
    }
    return taken;
}

uint16_t Inventory::give_to_hand(const ItemRegistry &items, ItemId item, uint16_t count)
{
    return add(items, item, count, m_selected);
}

bool Inventory::take(int slot, uint16_t count)
{
    if (count == 0) return true;
    if (slot < 0 || slot >= (int)m_slots.size()) return false;

    ItemStack &s = m_slots[(size_t)slot];
    if (s.empty() || s.count < count) return false;

    s.count = (uint16_t)(s.count - count);
    normalise(s);
    return true;
}

ItemStack Inventory::take_from(int slot, uint16_t count)
{
    if (slot < 0 || slot >= (int)m_slots.size()) return ItemStack{};

    ItemStack &src = m_slots[(size_t)slot];
    if (src.empty()) return ItemStack{};

    if (count == 0 || count > src.count) count = src.count;

    ItemStack out;
    out.item  = src.item;
    out.count = count;
    out.used  = src.used;
    out.ench  = src.ench;
    out.tag   = src.tag;

    src.count = (uint16_t)(src.count - count);
    normalise(src);
    return out;
}

bool Inventory::spend_held(uint16_t count)
{
    return take(m_selected, count);
}

bool Inventory::move_to(const ItemRegistry &items, int from, int to, uint16_t count)
{
    if (from < 0 || from >= (int)m_slots.size()) return false;
    if (to < 0 || to >= (int)m_slots.size()) return false;
    if (from == to) return false;

    ItemStack &src = m_slots[(size_t)from];
    if (src.empty()) return false;

    if (count == 0 || count > src.count) count = src.count;

    ItemStack &dst = m_slots[(size_t)to];
    if (!fits(items, to, src.item) || !fits(items, from, dst.item)) return false;

    // Different things: a swap, and only for the whole stack. Moving half a
    // stack onto something else has nowhere to put what it displaced, and the
    // answer "silently merge them" would be an inventory inventing items.
    if (!dst.empty() && !dst.same_as(src)) {
        if (count != src.count) return false;
        const ItemStack tmp = dst;
        dst = src;
        src = tmp;
        return true;
    }

    const uint16_t cap  = items.max_stack(src.item);
    const uint16_t room = dst.empty() ? cap : (uint16_t)(dst.count < cap ? cap - dst.count : 0);
    if (room == 0) return false;

    const uint16_t put = count < room ? count : room;

    dst.item  = src.item;
    dst.used  = src.used;
    dst.ench  = src.ench;
    dst.tag   = src.tag;
    dst.count = (uint16_t)(dst.count + put);
    src.count = (uint16_t)(src.count - put);
    normalise(src);
    return true;
}

ItemStack &Inventory::slot(int i)
{
    static ItemStack nowhere;
    if (i < 0 || i >= (int)m_slots.size()) {
        // A slot that is not there is answered with one that is not anybody's.
        // Written back to, it changes nothing -- which is what a caller with a
        // stale index deserves, and better than an index nobody checked.
        nowhere = ItemStack{};
        return nowhere;
    }
    return m_slots[(size_t)i];
}

bool move_between(const ItemRegistry &items, Inventory &from_bag, int from, Inventory &to_bag,
                  int to, uint16_t count)
{
    if (&from_bag == &to_bag) {
        // A catalogue is not rearranged, and a bin holds nothing to move.
        if (from_bag.role(from) >= kRoleSource || from_bag.role(to) >= kRoleSource) return false;
        return from_bag.move_to(items, from, to, count);
    }

    if (from < 0 || from >= from_bag.size()) return false;
    if (to < 0 || to >= to_bag.size()) return false;

    ItemStack &src = from_bag.slot(from);
    if (src.empty()) return false;

    if (count == 0 || count > src.count) count = src.count;

    // Into a catalogue or a bin: what goes in is gone. Out of a catalogue: a
    // copy, and the square keeps what it shows.
    const uint8_t into = to_bag.role(to);
    if (into == kRoleTrash || into == kRoleSource) {
        if (from_bag.role(from) == kRoleSource || from_bag.pinned(items, from)) return false;
        src.count = (uint16_t)(src.count - count);
        if (src.count == 0) src = ItemStack{};
        return true;
    }
    const bool copy = from_bag.role(from) == kRoleSource;

    ItemStack &dst = to_bag.slot(to);
    if (!to_bag.fits(items, to, src.item) || !from_bag.fits(items, from, dst.item)) return false;

    // Different things: a swap, and only for the whole stack. Exactly the rule
    // move_to states, for exactly the reason it states it -- half a stack
    // moving onto something else has nowhere to put what it displaced.
    if (!dst.empty() && !dst.same_as(src)) {
        if (copy || count != src.count) return false;
        const ItemStack tmp = dst;
        dst = src;
        src = tmp;
        return true;
    }

    const uint16_t cap  = items.max_stack(src.item);
    const uint16_t room = dst.empty() ? cap : (uint16_t)(dst.count < cap ? cap - dst.count : 0);
    if (room == 0) return false;

    const uint16_t put = count < room ? count : room;

    dst.item  = src.item;
    dst.used  = src.used;
    dst.ench  = src.ench;
    dst.tag   = src.tag;
    dst.count = (uint16_t)(dst.count + put);
    if (copy) return true;
    src.count = (uint16_t)(src.count - put);
    if (src.count == 0) src = ItemStack{};
    return true;
}

bool quick_move(const ItemRegistry &items, Inventory &from_bag, int from, Inventory &to_bag)
{
    // One object twice would look for room in the square the stack is standing
    // in and find it, every time.
    if (&from_bag == &to_bag) return false;
    if (from < 0 || from >= from_bag.size()) return false;
    if (from_bag.pinned(items, from)) return false;

    const ItemStack out = from_bag.take_from(from, 0);
    if (out.empty()) return false;

    const uint16_t left = to_bag.add(items, out.item, out.count, -1, out.used, out.ench, out.tag);

    // A catalogue's square keeps what it shows: what fitted was a copy.
    if (from_bag.role(from) == kRoleSource) {
        from_bag.slot(from) = out;
        return left < out.count;
    }

    // Back where it came from. The square is empty -- this is what came out of
    // it -- so nothing is displaced, and a full destination is a stack that did
    // not move rather than a stack that is gone.
    if (left > 0) from_bag.slot(from) = ItemStack{out.item, left, out.used, out.ench, out.tag};

    return left < out.count;
}

bool Inventory::fits(const ItemRegistry &items, int slot, ItemId item) const
{
    if (!takes(items, item)) return false;
    if (item != kNoItem && role(slot) == kRoleProduct) return false;   // only the block fills it
    if (item != kNoItem && !accepts(items, slot, item)) return false;   // not what that row takes
    if (arm(slot))
        return item == kNoItem || (items.get(item).wear == 0 && slot - m_worn_from - kWorn < arms_open(items));
    if (slot == m_worn_from + kWornBack - 1 && m_worn_from >= 0) {
        // What goes on the back must have arms for whatever they hold.
        int need = 0;
        for (int a = 0; a < kArms; ++a)
            if (!at(m_worn_from + kWorn + a).empty()) need = a + 1;
        if (need > 0 && (item == kNoItem || items.get(item).arms < need)) return false;
    }
    if (!worn(slot) || item == kNoItem) return true;
    return items.get(item).wear == slot - m_worn_from + 1;
}

int Inventory::arms_open(const ItemRegistry &items) const
{
    if (m_worn_from < 0) return 0;
    const ItemStack &back = at(m_worn_from + kWornBack - 1);
    return back.empty() ? 0 : items.get(back.item).arms;
}

bool Inventory::pinned(const ItemRegistry &items, int slot) const
{
    return m_worn_from >= 0 && slot == m_worn_from + kWornBack - 1 && !fits(items, slot, kNoItem);
}

int Inventory::armor(const ItemRegistry &items) const
{
    int n = 0;
    for (int i = 0; i < size(); ++i)
        if (worn(i) && !arm(i) && !m_slots[(size_t)i].empty())
            n += items.get(m_slots[(size_t)i].item).armor +
                 enchant_level_for(m_slots[(size_t)i].ench, Enchant::Protection);   // a point a level
    return n;
}

bool Inventory::wear_out(const ItemRegistry &items, uint16_t amount)
{
    bool changed = false;
    for (int i = 0; i < size(); ++i)
        if (worn(i) && !arm(i) && wear_slot(items, i, amount)) changed = true;
    return changed;
}

bool Inventory::wear_slot(const ItemRegistry &items, int slot, uint16_t amount)
{
    if (slot < 0 || slot >= size()) return false;
    ItemStack &s = m_slots[(size_t)slot];
    if (s.empty()) return false;
    const uint16_t last = items.get(s.item).durability;
    if (last == 0) return false;
    // Unbreaking: a use costs wear one time in level + 1. Drawn from a count of
    // every use this bag has seen, which moves whether the use cost anything or
    // not -- drawn from how worn the thing is, a use that cost nothing drew the
    // same number again, for ever, and it never wore at all.
    const uint8_t keep = enchant_level_for(s.ench, Enchant::Unbreaking);
    if (keep > 0) {
        const uint32_t draw = ++m_wear_draws * 2654435761u;
        if ((draw >> 16) % (keep + 1u) != 0) return true;   // used, and nothing spent
    }
    const uint32_t used = (uint32_t)s.used + amount;
    if (used >= last) s = ItemStack{};
    else s.used = (uint16_t)used;
    return true;
}

bool Inventory::swap_worn(const ItemRegistry &items, int slot)
{
    if (m_worn_from < 0 || slot < 0 || slot >= size()) return false;
    const ItemStack s = m_slots[(size_t)slot];
    if (s.empty()) return false;

    if (worn(slot)) {
        if (pinned(items, slot)) return false;
        const uint16_t left = add(items, s.item, s.count, -1, s.used, s.ench, s.tag);
        if (left == s.count) return false;
        m_slots[(size_t)slot] = left > 0 ? ItemStack{s.item, left, s.used, s.ench, s.tag} : ItemStack{};
        return true;
    }
    const uint8_t w = items.get(s.item).wear;
    if (w == 0) return false;
    return move_to(items, slot, m_worn_from + w - 1);
}

uint32_t Inventory::count_of(ItemId item) const
{
    if (item == kNoItem) return 0;

    uint32_t n = 0;
    for (const auto &s : m_slots) {
        if (s.item == item) n += s.count;
    }
    return n;
}

bool Inventory::accepts(const ItemRegistry &items, int slot, ItemId item) const
{
    const std::vector<std::string> &only = m_rules.accepts[role(slot) % kRoleCount];
    if (only.empty() || item == kNoItem) return true;
    const std::string &name = items.get(item).name;
    for (const std::string &n : only)
        if (n == name) return true;
    return false;
}

int slot_role(const char *name)
{
    static const char *const kNames[] = {"store", "raw", "fuel", "product", "source", "trash"};
    for (int i = 0; i < kRoleCount; ++i)
        if (name != nullptr && std::strcmp(name, kNames[i]) == 0) return i;
    return -1;
}

unsigned role_mask(const char *name)
{
    if (name == nullptr || *name == '\0') return kRolesIn;
    const int r = slot_role(name);
    return r < 0 ? 0u : 1u << r;
}

} // namespace cnp
