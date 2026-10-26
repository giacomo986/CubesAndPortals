#include "core/item/item_codec.h"

#include "core/item/container.h"
#include "core/net/bytes.h"

namespace cnp {

static_assert(kMaxStoredSlots == kMaxContainerSlots,
              "the stored bound and the container bound are the same bound; raising one alone "
              "either refuses a chest that exists or accepts a file no chest can hold");

std::vector<uint8_t> encode_inventory(const ItemRegistry &items, const Inventory &bag)
{
    ByteWriter w;
    w.u8(kInventoryFormat);
    w.u16((uint16_t)bag.size());
    w.u16((uint16_t)bag.selected());

    size_t filled = 0;
    for (int i = 0; i < bag.size(); ++i)
        if (!bag.at(i).empty()) ++filled;
    w.u16((uint16_t)filled);

    for (int i = 0; i < bag.size(); ++i) {
        const ItemStack &s = bag.at(i);
        if (s.empty()) continue;

        w.u16((uint16_t)i);
        w.str(items.get(s.item).name);
        w.u16(s.count);
        w.u16(s.used);
        w.u8(s.ench);
        w.u32(s.tag);
    }

    return w.data();
}

int stored_slots(const uint8_t *data, size_t size)
{
    ByteReader r(data, size);
    const uint8_t format = r.u8();
    if (format < 1 || format > kInventoryFormat) return 0;

    const int slots = (int)r.u16();
    if (!r.ok() || slots < 1 || slots > kMaxStoredSlots) return 0;
    return slots;
}

int stored_slots(const std::vector<uint8_t> &bytes)
{
    return stored_slots(bytes.data(), bytes.size());
}

bool decode_inventory(ItemRegistry &items, const uint8_t *data, size_t size, Inventory &bag,
                      std::string &err)
{
    ByteReader r(data, size);

    const uint8_t format = r.u8();
    if (!r.ok()) {
        err = "an inventory of no bytes at all";
        return false;
    }
    if (format < 1 || format > kInventoryFormat) {
        err = "an inventory written in format " + std::to_string((int)format) + ", and this build "
              "reads " + std::to_string((int)kInventoryFormat);
        return false;
    }

    const int slots    = (int)r.u16();
    const int selected = (int)r.u16();
    if (!r.ok() || slots < 1 || slots > kMaxStoredSlots) {
        err = "an inventory claiming " + std::to_string(slots) + " slots";
        return false;
    }

    size_t filled = 0;
    if (!r.count(filled, (size_t)slots)) {
        err = "an inventory claiming more full slots than it has slots";
        return false;
    }

    // Read the lot before writing any of it. A stack whose name is fine
    // followed by one that is not would otherwise leave the bag half filled
    // from a file that was refused, and a refusal that changes something is
    // worse than either answer on its own.
    struct Stored {
        int         slot;
        std::string name;
        uint16_t    count;
        uint16_t    used;
        uint8_t     ench;
        uint32_t    tag;
    };
    std::vector<Stored> stacks;
    stacks.reserve(filled);

    for (size_t n = 0; n < filled; ++n) {
        Stored s;
        s.slot = (int)r.u16();
        if (!r.str(s.name, kMaxStoredItemName)) {
            err = "a stack with no name, or a name longer than a name can be";
            return false;
        }
        s.count = r.u16();
        s.used  = format >= 2 ? r.u16() : 0;
        s.ench  = format >= 3 ? r.u8() : 0;
        s.tag   = format >= 5 ? r.u32() : 0;
        if (format == 3) s.ench = enchant_from_v3(s.ench);
        if (!r.ok()) {
            err = "an inventory that stops in the middle of a stack";
            return false;
        }

        if (s.slot < 0 || s.slot >= slots) {
            err = "a stack in slot " + std::to_string(s.slot) + " of " + std::to_string(slots);
            return false;
        }
        if (s.slot >= bag.size()) {
            err = "a stack in slot " + std::to_string(s.slot) + ", and this inventory has " +
                  std::to_string(bag.size());
            return false;
        }
        if (s.count == 0 || s.name.empty()) {
            err = "a stack of nothing, which is what an empty slot is and is not written";
            return false;
        }

        // Two stacks naming one slot is a file that says two things about the
        // same place. Refused rather than resolved: whichever one is written
        // second wins, and a rule for which one that should be is a rule about
        // corruption.
        for (const Stored &had : stacks) {
            if (had.slot != s.slot) continue;
            err = "two stacks in slot " + std::to_string(s.slot);
            return false;
        }

        stacks.push_back(s);
    }

    if (!r.done()) {
        err = "an inventory with bytes left over after the last stack";
        return false;
    }

    for (const Stored &s : stacks) {
        ItemStack &into = bag.slot(s.slot);
        into.item       = items.adopt_stored(s.name);
        into.count      = s.count;
        into.used       = s.used;
        into.ench       = s.ench;
        into.tag        = s.tag;
    }

    bag.select(selected);
    return true;
}

bool decode_inventory(ItemRegistry &items, const std::vector<uint8_t> &bytes, Inventory &bag,
                      std::string &err)
{
    return decode_inventory(items, bytes.data(), bytes.size(), bag, err);
}

} // namespace cnp
