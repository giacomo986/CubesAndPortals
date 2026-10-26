#include "core/item/craft_grid.h"

namespace cnp {

std::vector<ItemStack> craft_empty(const ItemRegistry &items, CraftGrid &grid, Inventory &bag)
{
    std::vector<ItemStack> spilled;

    for (int i = 0; i < grid.slots.size(); ++i) {
        const ItemStack out = grid.slots.take_from(i);
        if (out.empty()) continue;

        const uint16_t left = bag.add(items, out.item, out.count, -1, out.used, out.ench, out.tag);
        if (left == 0) continue;

        ItemStack rest = out;
        rest.count     = left;
        spilled.push_back(rest);
    }

    return spilled;
}

std::vector<ItemStack> craft_resize(const ItemRegistry &items, CraftGrid &grid, Inventory &bag,
                                    int w, int h)
{
    std::vector<ItemStack> spilled = craft_empty(items, grid, bag);

    grid.w = w < 1 ? 1 : (w > kCraftMax ? kCraftMax : w);
    grid.h = h < 1 ? 1 : (h > kCraftMax ? kCraftMax : h);
    return spilled;
}

bool craft_take(const ItemRegistry &items, const RecipeSet &recipes, CraftGrid &grid,
                Inventory &bag)
{
    const ItemStack made = recipes.craft(items, grid.slots, grid.w, grid.h);
    if (made.empty()) return false;   // no recipe, or one naming an item nothing registered

    // Tried on a copy, so a result that only partly fits changes nothing at
    // all. Doing it in place and undoing it afterwards is the same thing
    // written twice, and the second half is the one people forget.
    Inventory would = bag;
    if (would.add(items, made.item, made.count, -1, made.used, made.ench, made.tag) != 0) return false;

    recipes.consume(items, grid.slots, grid.w, grid.h);
    bag = would;
    return true;
}

size_t craft_take_all(const ItemRegistry &items, const RecipeSet &recipes, CraftGrid &grid,
                      Inventory &bag)
{
    size_t made = 0;
    while (craft_take(items, recipes, grid, bag)) ++made;
    return made;
}

bool craft_out_of_reach(const CraftGrid &grid, float eye_x, float eye_y, float eye_z, float reach)
{
    // A grid that is not a table is at no cell at all, and `at` holds whatever
    // the last table left there. Asking the distance to that would shut the
    // bag's own 2x2 the moment its owner walked away from a table they had
    // closed an hour ago.
    if (!grid.table) return false;

    return !container_in_reach(grid.at, eye_x, eye_y, eye_z, reach);
}

} // namespace cnp
