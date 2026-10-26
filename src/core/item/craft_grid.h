#pragma once

// The squares a player arranges things in, and what happens when they take the
// answer.
//
// A struct and three free functions rather than logic inside the session layer,
// for the reason this codebase has now paid for four times: the networked
// server and the single-process client both do this, and a rule written once in
// each is two rules that drift. ui::panel_cursor, ui::bag_click and
// container_in_reach are here for the same reason.
//
// What it is NOT: a container. Nobody else may reach into it, it belongs to a
// session and not to a cell, and it is not saved -- a grid that survived a
// restart would be things a player put down for a moment, sitting on disk out
// of sight.

#include <vector>

#include "core/item/container.h"
#include "core/item/inventory.h"
#include "core/item/item_registry.h"
#include "core/item/recipe.h"

namespace cnp {

struct CraftGrid {
    // Nine slots always, of which w * h are used. Two by two is the grid in the
    // corner of everybody's bag; three by three is what standing at a table
    // makes it.
    //
    // The SIZE is the state, and there is no second "a table is open" flag on
    // the wire: a client told three knows, and two fields could disagree. The
    // cell below is here for the reach check and for nothing else.
    Inventory   slots{kCraftMax * kCraftMax};
    int         w = 2, h = 2;

    bool        table = false;   // a three-by-three, opened at a cell
    ContainerAt at;

    int used() const { return w * h; }
};

// Everything in the grid back into `bag`, and whatever will not fit handed
// back.
//
// Returned rather than dropped, because this has no world to drop into and no
// idea what a drop is -- the same split ContainerSet::erase makes, and the same
// reason. A caller that ignores the return value loses stacks, which is why it
// is the return value and not an out-parameter nobody has to pass.
std::vector<ItemStack> craft_empty(const ItemRegistry &items, CraftGrid &grid, Inventory &bag);

// The grid becomes `w` by `h`, emptied first.
//
// Emptied is not tidiness. The same nine slots are read as a 2x2 or a 3x3, so
// slot 3 is (1,1) in one and (0,1) in the other: a grid that survived a resize
// would rearrange itself under the player's hands.
std::vector<ItemStack> craft_resize(const ItemRegistry &items, CraftGrid &grid, Inventory &bag,
                                    int w, int h);

// Takes what the grid makes and pays for it out of the grid.
//
// All of it or none of it. A result that only partly fits leaves everything
// alone: a player charged for a craft they did not get is worse than one told
// to make room, and there is no half of a pickaxe.
//
// Returns whether anything was made.
bool craft_take(const ItemRegistry &items, const RecipeSet &recipes, CraftGrid &grid,
                Inventory &bag);

// Takes it again and again, until the grid runs out or the bag does -- what a
// shift-click on the answer means.
//
// A loop over craft_take and not a second rule: each round is all-or-nothing on
// its own, so a bag that fills half way through leaves the grid holding exactly
// what was not paid for. It ends because every round takes at least one item
// out of a grid that holds a finite number of them.
//
// Returns how many were made.
size_t craft_take_all(const ItemRegistry &items, const RecipeSet &recipes, CraftGrid &grid,
                      Inventory &bag);

// Whether the grid must shut because the table it was opened at is out of
// reach.
//
// Asked of the GRID and not of a cell, because the answer for a bag's own 2x2
// is "never": it stands at no table, and every caller that had to remember
// that is another place the rule can be forgotten. It was forgotten twice
// already -- the single-process loop closed a chest walked away from and left
// the table open behind it.
bool craft_out_of_reach(const CraftGrid &grid, float eye_x, float eye_y, float eye_z, float reach);

} // namespace cnp
