#pragma once

// An inventory as bytes, for a file rather than for a socket.
//
// DESIGN.md 8 puts containers in M7 and `TODO.md` OPEN-10 records what was
// missing: `ContainerSet` was memory, so a world reloaded from disk had no
// chests and nothing that was in them. A world that saves its blocks and throws
// away what is in them is worse than one that saves nothing, because the ground
// coming back says it is the same world.
//
// **A stack is stored by NAME, and that is the whole design.** An ItemId is
// dense and handed out in registration order, and nothing on disk guards that
// order: `registry_content_hash` covers the block table only, and even if it
// covered items, refusing to open a world because somebody added a tool is a
// far harsher answer than a bag needs. Names cost a few bytes per stack and
// survive a mod list that changed.
//
// What happens to a name the registry has never heard of is the second half,
// and the answer already existed: it is minted as an ORPHAN, on a fresh id,
// unobtainable and placing nothing -- exactly what `ItemRegistry::end_reload`
// does to an item whose mod stopped registering it. The disk has the same
// problem a hot reload has, one week apart instead of one second, so it gets
// the same answer rather than a second one. A mod that comes back revives the
// name through `adopt_blocks` on the id the stacks are already holding.
//
// That is why decoding takes a MUTABLE registry. It is not a const read: a bag
// off disk can add rows to the item table, and a signature that hid this would
// be hiding the only surprising thing here.

#include <cstdint>
#include <string>
#include <vector>

#include "core/item/inventory.h"
#include "core/item/item_registry.h"

namespace cnp {

// The format this build writes. A byte, because the alternative to a version is
// a future reader guessing, and guessing about a file is how somebody's chest
// comes back holding something else. A reader that meets a version it does not
// know refuses and says so.
// 2 adds, after each count, how much of the stack's durability is used; a 1 is
// still read, every stack in it new.
inline constexpr uint8_t kInventoryFormat = 5;   // 3 adds a stack's enchantment, 4 several, 5 its tag

// The longest item name that will be written or read back. Item names are
// namespaced identifiers -- "core:stone", "chest:box" -- and one claiming a
// kilobyte is a row that has rotted.
inline constexpr size_t kMaxStoredItemName = 128;

// The most slots any stored inventory may claim. The same bound containers are
// held to, because a container is the biggest inventory there is -- tied to
// kMaxContainerSlots by a static_assert in the .cpp rather than by an include,
// so that raising one and not the other does not compile.
inline constexpr int kMaxStoredSlots = 128;

// Only the slots that hold something are written, so an empty 128-slot chest is
// a handful of bytes rather than a kilobyte of "nothing" repeated.
std::vector<uint8_t> encode_inventory(const ItemRegistry &items, const Inventory &bag);

// How many slots those bytes describe, or 0 if they are not an inventory at
// all. For the caller that has to CREATE the thing before it can be filled: a
// container's size is decided when it is made and never changed afterwards, so
// the size has to be known one step before the contents.
int stored_slots(const uint8_t *data, size_t size);
int stored_slots(const std::vector<uint8_t> &bytes);

// Fills `bag` from the bytes. Everything it finds goes in, or nothing does.
//
// A slot index at or past `bag.size()` is a REFUSAL and not a skip. A smaller
// bag than the one that was saved is a bag that would silently lose the stacks
// past its end, and losing them quietly is the failure this whole file exists
// to prevent -- the caller wanting to shrink an inventory can say so by reading
// it into a big one and moving what it wants.
//
// `items` may gain rows: see the orphan paragraph at the top.
bool decode_inventory(ItemRegistry &items, const uint8_t *data, size_t size, Inventory &bag,
                      std::string &err);
bool decode_inventory(ItemRegistry &items, const std::vector<uint8_t> &bytes, Inventory &bag,
                      std::string &err);

} // namespace cnp
