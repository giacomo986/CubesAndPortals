#include "core/item/recipe.h"

#include <algorithm>

namespace cnp {

namespace {

bool is_hole(const std::string &name)
{
    return name.empty() || name == kCraftHole;
}

// A grid, or a pattern, as its bounding box of non-empty cells.
//
// This is the whole of "the same shape anywhere in the grid": a pattern stored
// as its own box and a grid reduced to its own box are the same thing or they
// are not, and there is nothing to slide. The alternative -- trying the pattern
// at every offset -- is nine comparisons where one will do, and nine places to
// get an index wrong.
//
// Returns false when there is nothing in it at all.
bool bounding_box(const std::vector<std::string> &cells, int w, int h, int &x0, int &y0, int &x1,
                  int &y1)
{
    x0 = w; y0 = h; x1 = -1; y1 = -1;

    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x) {
            if (is_hole(cells[(size_t)(y * w + x)])) continue;
            if (x < x0) x0 = x;
            if (y < y0) y0 = y;
            if (x > x1) x1 = x;
            if (y > y1) y1 = y;
        }

    return x1 >= x0 && y1 >= y0;
}

// The same, over an Inventory read as a grid. A second function and not a
// template, because the two hold different things -- names on one side, ids on
// the other -- and a template over that would be a template over "how do I ask
// whether this is empty", which is the only interesting line in either.
bool bounding_box(const Inventory &grid, int w, int h, int &x0, int &y0, int &x1, int &y1)
{
    x0 = w; y0 = h; x1 = -1; y1 = -1;

    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x) {
            if (grid.at(y * w + x).empty()) continue;
            if (x < x0) x0 = x;
            if (y < y0) y0 = y;
            if (x > x1) x1 = x;
            if (y > y1) y1 = y;
        }

    return x1 >= x0 && y1 >= y0;
}

bool same_recipe(const Recipe &a, const Recipe &b)
{
    if (a.shaped != b.shaped) return false;
    if (a.shaped && (a.w != b.w || a.h != b.h)) return false;
    if (a.pattern.size() != b.pattern.size()) return false;

    if (a.shaped) return a.pattern == b.pattern;

    // Shapeless: the same ingredients in any order is the same recipe, so the
    // comparison sorts. Written out rather than sorting at registration,
    // because a stored order that is not the mod's is one more thing that has
    // to be explained when somebody prints the table.
    std::vector<std::string> x = a.pattern, y = b.pattern;
    std::sort(x.begin(), x.end());
    std::sort(y.begin(), y.end());
    return x == y;
}

} // namespace

std::vector<std::string> craft_words(const std::string &row)
{
    std::vector<std::string> out;
    size_t                   at = 0;

    while (at < row.size()) {
        while (at < row.size() && row[at] == ' ') ++at;
        if (at >= row.size()) break;

        const size_t start = at;
        while (at < row.size() && row[at] != ' ') ++at;
        out.push_back(row.substr(start, at - start));
    }
    return out;
}

bool RecipeSet::add_shaped(const std::string &owner, const std::string &result, uint16_t count,
                           const std::string &row0, const std::string &row1,
                           const std::string &row2, std::string &err)
{
    if (result.empty() || is_hole(result)) {
        err = "a recipe that makes nothing";
        return false;
    }
    if (count == 0) {
        err = "a recipe that makes none of it";
        return false;
    }

    std::vector<std::vector<std::string>> rows;
    for (const std::string *r : {&row0, &row1, &row2}) {
        std::vector<std::string> words = craft_words(*r);
        if (!words.empty()) rows.push_back(std::move(words));
    }

    if (rows.empty()) {
        err = "a recipe made of nothing";
        return false;
    }
    if ((int)rows.size() > kCraftMax) {
        err = "a recipe " + std::to_string(rows.size()) + " rows tall";
        return false;
    }

    size_t widest = 0;
    for (const auto &r : rows) widest = r.size() > widest ? r.size() : widest;
    if ((int)widest > kCraftMax) {
        err = "a recipe " + std::to_string(widest) + " cells wide";
        return false;
    }

    // Padded to a rectangle, so a mod need not write trailing holes: a row that
    // ends early ends in empty cells, which is what it looks like it means.
    const int    w = (int)widest;
    const int    h = (int)rows.size();
    std::vector<std::string> cells((size_t)(w * h), kCraftHole);
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < (int)rows[(size_t)y].size(); ++x)
            cells[(size_t)(y * w + x)] = rows[(size_t)y][(size_t)x];

    int x0 = 0, y0 = 0, x1 = 0, y1 = 0;
    if (!bounding_box(cells, w, h, x0, y0, x1, y1)) {
        err = "a recipe of nothing but holes";
        return false;
    }

    Recipe made;
    made.owner  = owner;
    made.result = result;
    made.count  = count;
    made.shaped = true;
    made.w      = x1 - x0 + 1;
    made.h      = y1 - y0 + 1;
    made.pattern.reserve((size_t)(made.w * made.h));
    for (int y = y0; y <= y1; ++y)
        for (int x = x0; x <= x1; ++x) {
            const std::string &cell = cells[(size_t)(y * w + x)];
            made.pattern.push_back(is_hole(cell) ? std::string(kCraftHole) : cell);
        }

    return take(std::move(made), "the same shape is already made by ", err);
}

bool RecipeSet::add_shapeless(const std::string &owner, const std::string &result, uint16_t count,
                              const std::string &items, std::string &err)
{
    if (result.empty() || is_hole(result)) {
        err = "a recipe that makes nothing";
        return false;
    }
    if (count == 0) {
        err = "a recipe that makes none of it";
        return false;
    }

    std::vector<std::string> words = craft_words(items);
    for (const std::string &word : words) {
        if (!is_hole(word)) continue;
        err = "a shapeless recipe with a hole in it, which is a shape";
        return false;
    }

    if (words.empty()) {
        err = "a recipe made of nothing";
        return false;
    }
    if ((int)words.size() > kCraftMax * kCraftMax) {
        err = "a recipe of " + std::to_string(words.size()) + " things, and a grid holds " +
              std::to_string(kCraftMax * kCraftMax);
        return false;
    }

    Recipe made;
    made.owner   = owner;
    made.result  = result;
    made.count   = count;
    made.shaped  = false;
    made.pattern = std::move(words);

    return take(std::move(made), "the same things already make something, for ", err);
}

bool RecipeSet::take(Recipe &&made, const char *clash, std::string &err)
{
    for (size_t i = 0; i < m_recipes.size(); ++i) {
        if (!same_recipe(m_recipes[i], made)) continue;

        // A mod putting back a recipe it is reloading is not a mod colliding
        // with itself. `begin_reload` marks its rows stale and this is where
        // one comes back -- the same shape from the same owner, replaced whole
        // so that a mod which changed what its shape MAKES gets the new answer.
        //
        // Whole rather than un-marked in place, and the reason is the one
        // ItemRegistry paid for in M7_FINDINGS 16.2: a row patched field by
        // field keeps whatever the old one happened to hold.
        if (m_stale[i] && m_recipes[i].owner == made.owner) {
            m_recipes[i] = std::move(made);
            m_stale[i]   = 0;
            return true;
        }

        // Named both ways: which shape, whose, and what each makes -- a clash
        // that only says whose leaves a modder bisecting recipes.
        err = clash + (m_recipes[i].owner.empty() ? std::string("the engine") : m_recipes[i].owner) +
              " (" + m_recipes[i].result + "; this one makes " + made.result + ")";
        return false;
    }

    m_recipes.push_back(std::move(made));
    m_stale.push_back(0);
    return true;
}

const Recipe *RecipeSet::match(const ItemRegistry &items, const Inventory &grid, int w,
                               int h) const
{
    if (w < 1 || h < 1 || grid.size() < w * h) return nullptr;

    int gx0 = 0, gy0 = 0, gx1 = 0, gy1 = 0;
    if (!bounding_box(grid, w, h, gx0, gy0, gx1, gy1)) return nullptr;   // an empty grid

    const int gw = gx1 - gx0 + 1;
    const int gh = gy1 - gy0 + 1;

    // What is in it, for the shapeless comparison. Sorted once here rather than
    // once per recipe.
    std::vector<ItemId> loose;
    for (int i = 0; i < w * h; ++i)
        if (!grid.at(i).empty()) loose.push_back(grid.at(i).item);
    std::sort(loose.begin(), loose.end());

    for (const Recipe &r : m_recipes) {
        if (r.shaped) {
            if (r.w != gw || r.h != gh) continue;

            bool same = true;
            for (int y = 0; y < gh && same; ++y)
                for (int x = 0; x < gw && same; ++x) {
                    const std::string &want = r.pattern[(size_t)(y * gw + x)];
                    const ItemStack   &has  = grid.at((gy0 + y) * w + (gx0 + x));

                    if (is_hole(want)) {
                        same = has.empty();
                    } else {
                        const ItemId id = items.find(want);
                        same = id != kNoItem && !has.empty() && has.item == id;
                    }
                }
            if (same) return &r;
            continue;
        }

        if (r.pattern.size() != loose.size()) continue;

        std::vector<ItemId> want;
        want.reserve(r.pattern.size());
        bool known = true;
        for (const std::string &name : r.pattern) {
            const ItemId id = items.find(name);
            if (id == kNoItem) { known = false; break; }
            want.push_back(id);
        }
        if (!known) continue;

        std::sort(want.begin(), want.end());
        if (want == loose) return &r;
    }

    return nullptr;
}

ItemStack RecipeSet::result_of(const ItemRegistry &items, const Recipe *r) const
{
    if (r == nullptr) return ItemStack{};

    const ItemId id = items.find(r->result);
    if (id == kNoItem) return ItemStack{};

    ItemStack out;
    out.item  = id;
    out.count = r->count;
    return out;
}

bool RecipeSet::add_dyeable(const std::string &item, std::string &err)
{
    if (item.empty()) {
        err = "a thing to dye with no name";
        return false;
    }
    m_dyeable.insert(item);
    return true;
}

bool RecipeSet::add_dye(const std::string &item, int dye, std::string &err)
{
    if (item.empty() || dye < 1 || dye > 16) {
        err = "a dye is an item and one of the sixteen, 1 to 16";
        return false;
    }
    m_dyes[item] = dye;
    return true;
}

ItemStack RecipeSet::dyed(const ItemRegistry &items, const Inventory &grid, int w, int h) const
{
    if (m_dyeable.empty()) return ItemStack{};
    const ItemStack *thing = nullptr;
    int dyes[3], n = 0;
    for (int i = 0; i < w * h; ++i) {
        const ItemStack &s = grid.at(i);
        if (s.empty()) continue;
        const std::string &name = items.get(s.item).name;
        if (m_dyeable.count(name) != 0) {
            if (thing != nullptr) return ItemStack{};   // one thing at a time
            thing = &s;
            continue;
        }
        const auto d = m_dyes.find(name);
        if (d == m_dyes.end() || n == 3) return ItemStack{};
        dyes[n++] = d->second;
    }
    if (thing == nullptr || n == 0) return ItemStack{};

    ItemStack out = *thing;
    out.count     = 1;
    for (int k = 0; k < n; ++k) out.tag = tag_with_colour(out.tag, k, dyes[k]);
    // The colours it already has: nothing to make, and no dye spent on it.
    if (out.tag == thing->tag) return ItemStack{};
    return out;
}

ItemStack RecipeSet::craft(const ItemRegistry &items, const Inventory &grid, int w, int h) const
{
    ItemStack made = dyed(items, grid, w, h);
    if (!made.empty()) return made;
    return result_of(items, match(items, grid, w, h));
}

bool RecipeSet::consume(const ItemRegistry &items, Inventory &grid, int w, int h) const
{
    if (craft(items, grid, w, h).empty()) return false;

    for (int i = 0; i < w * h; ++i) {
        if (grid.at(i).empty()) continue;
        grid.take(i, 1);
    }
    return true;
}

std::vector<const Recipe *> RecipeSet::unresolved(const ItemRegistry &items) const
{
    std::vector<const Recipe *> out;

    for (const Recipe &r : m_recipes) {
        bool ok = items.find(r.result) != kNoItem;
        for (const std::string &name : r.pattern) {
            if (is_hole(name)) continue;
            if (items.find(name) == kNoItem) ok = false;
        }
        if (!ok) out.push_back(&r);
    }
    return out;
}

void RecipeSet::begin_reload(const std::string &owner)
{
    for (size_t i = 0; i < m_recipes.size(); ++i)
        if (m_recipes[i].owner == owner) m_stale[i] = 1;
}

void RecipeSet::cancel_reload(const std::string &owner)
{
    for (size_t i = 0; i < m_recipes.size(); ++i)
        if (m_recipes[i].owner == owner) m_stale[i] = 0;
}

size_t RecipeSet::end_reload(const std::string &owner)
{
    size_t gone = 0;

    // Backwards, so removing one does not move the next one out from under the
    // index. Removed and not orphaned: nothing in a save file points at a
    // recipe, so there is no id to keep.
    for (size_t i = m_recipes.size(); i-- > 0;) {
        if (!m_stale[i] || m_recipes[i].owner != owner) continue;
        m_recipes.erase(m_recipes.begin() + (long)i);
        m_stale.erase(m_stale.begin() + (long)i);
        ++gone;
    }

    for (size_t i = 0; i < m_recipes.size(); ++i)
        if (m_recipes[i].owner == owner) m_stale[i] = 0;

    return gone;
}

} // namespace cnp
