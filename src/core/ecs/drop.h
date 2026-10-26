#pragma once

// A stack that is not in anybody's bag.
//
// DESIGN.md 8 lists "drop" among M7's pieces, and until now the engine had
// nowhere to put a stack that was not carried: breaking a block with a full bag
// threw the drop away (M7_FINDINGS 1.3) and letting go of a drag outside the
// panel did nothing "because there is no ground to drop a stack on" (6). Both
// were the same missing thing said twice.
//
// It is an ENTITY and not a list of its own, and that is the point of the ECS
// having been built first: what gets streamed to a player is "the entities near
// them with a Body", so a stack on the ground reaches every client through code
// that was not changed to let it -- the same claim core/ecs/mob.h was written
// to make, made a second time by something that is not a creature.

#include <vector>

#include "flecs.h"

#include <functional>

#include "core/item/inventory.h"
#include "core/item/item_registry.h"
#include "core/physics/aabb.h"
#include "core/registry/node_registry.h"
#include "core/voxel/chunk.h"

namespace cnp::ecs {

// The side of the cube a dropped stack is drawn and collected as. Small enough
// to read as an object rather than a block, big enough to see from across a
// room.
inline constexpr float kDropSize = 0.3f;

// How long a stack lies untouchable before anyone may pick it up.
//
// It exists for the gesture that throws a stack out of the bag: without it the
// stack lands inside the thrower and is back in the bag on the same tick, and
// the player sees nothing happen. One second is long enough to walk out of, and
// it applies to EVERYBODY rather than only the thrower -- "who dropped this"
// is a fact the drop would have to carry, and a second of nobody taking it is
// not worth a field that can be wrong.
inline constexpr double kDropArming = 1.0;

// How long a stack lies on the ground before it is gone.
//
// Five minutes. A world where nothing ever expires fills with the contents of
// every chest anybody has ever broken, and every one of those is an entity that
// is streamed to everybody who walks near it -- so the cost of keeping them is
// paid by players who have nothing to do with them.
//
// Long enough to walk back for: the thing a player actually does with a spilled
// chest is empty their own bag somewhere and come back, and five minutes is
// more than that takes. Short enough that a session does not accumulate a field
// of debris nobody will ever pick up.
//
// Counted from when the stack landed and not from when it was last looked at: a
// timer that a passing player refreshes is a timer that never fires in a world
// with people in it.
inline constexpr double kDropLifetime = 300.0;

// How far in front of a player a thrown stack appears, in metres.
//
// Far enough that it is not inside them and they can see it leave; near enough
// that it cannot clear a wall they are standing against -- a stack that ends up
// on the other side of a block is a stack a player cannot get back.
inline constexpr float kThrowAhead = 0.9f;

// A stack on the ground.
//
// The velocity lives here and not in Position: Position is MovementState, which
// is the server's record of where something IS and what it may do next, and a
// falling crate is neither of those questions. See core/ecs/components.h.
struct Dropped {
    ItemStack stack;
    double    arm_at    = 0.0;  // nobody may pick it up before this
    double    expire_at = 0.0;  // and after this there is nothing to pick up
    float     vy      = 0.0f;   // metres per second, downward is negative
    bool      resting = false;  // it found a floor and stopped asking
};

// One stack, at a point, falling from there. `now` is server seconds; the arming
// window is counted from it.
flecs::entity spawn_drop(flecs::world &w, const ItemStack &what, float x, float y, float z,
                         double now);

// Gravity, and a floor.
//
// An explicit call rather than a Flecs system, unlike core:wander. Wander is
// closed-form -- it reads the world clock and needs no step -- while this
// INTEGRATES, so a gate is worth much more when it can hand it exact steps than
// symmetry with mob.h is worth. It is called from the server's tick, in one
// place, with the tick's own dt.
void step_drops(flecs::world &w, const World &world, const NodeRegistry &nodes, float dt);

// Everything within reach of somebody who can carry it goes into their bag.
//
// Returns the avatars whose bag changed, because this cannot send anything: an
// inventory that changed has to reach its owner, and knowing which peer that is
// belongs to the session table and not here. A caller that forgets to send is a
// player whose bag fills up silently, so the list is the return value rather
// than something to ask for afterwards.
//
// A stack that only partly fits leaves the rest on the ground: half a pickup is
// better than none, and a drop that refuses to be touched until a slot frees up
// is a drop a player walks over forever.
std::vector<flecs::entity_t> collect_drops(flecs::world &w, const ItemRegistry &items, double now);

// For a mod moving things along (mods/conveyor's sorter and extractor), a
// stack at a time and as it is -- worn, enchanted, tagged.
//
// The item of the first stack lying in cell (x, y, z), or kNoItem.
ItemId drop_in_cell(flecs::world &w, int x, int y, int z);
// That same stack, put at a point and falling from there. Its item, or kNoItem
// when the cell had none.
ItemId send_drop(flecs::world &w, int x, int y, int z, float tx, float ty, float tz);
// Up to `count` of the first stack in a bag that may come out (a Store or a
// Product square, SlotRole), on the ground at a point. How many.
int drop_from(flecs::world &w, Inventory &bag, int count, float tx, float ty, float tz, double now);

// Stacks whose time is up, gone. Returns how many.
//
// Its own call rather than a line inside step_drops, and the difference is that
// step_drops needs a world and this does not: a drop expires the same way in a
// world nobody is standing in. It also means a gate can hand it a clock without
// having to build terrain for the physics to fall onto.
size_t expire_drops(flecs::world &w, double now);

// The box a drop occupies, for whoever has to draw it or reach it.
AABB drop_box(float x, float y, float z);

// A block falling (core.fall): sand, gravel, an anvil. The cell it left is
// already air; it is a whole block in flight, its middle at the entity's
// position, and it lands in the first cell whose floor stops it.
struct Falling {
    BlockId block  = 0;
    uint8_t facing = 0;
    float   vy     = 0.0f;
    float   from_y = 0.0f;   // where it started, for how far it fell
};

// One block falling from the cell (x, y, z), which the caller has emptied.
flecs::entity spawn_falling(flecs::world &w, BlockId block, uint8_t facing, int x, int y, int z);

// Gravity on every falling block, and a landing for each that met a floor:
// `land(x, y, z, block, facing, fell)` with the cell it came to rest in and how
// many nodes it fell. The entity is gone after; what the landing does -- put
// the block there, or its item when the cell is taken -- is the caller's.
void step_falling(flecs::world &w, const World &world, const NodeRegistry &nodes, float dt,
                  const std::function<void(int, int, int, BlockId, uint8_t, int)> &land);

} // namespace cnp::ecs
