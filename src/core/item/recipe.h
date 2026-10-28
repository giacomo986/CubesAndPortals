#pragma once

// What turns some things into another thing.
//
// DESIGN.md 8 lists "craft" in M7. This is the table and the matching, and
// nothing else: no grid on a screen, no message on a wire, no slot a player can
// click. Those come next and they come after, for the reason core/ui/panel.h
// was written before panels went on the wire -- a rule that can be decided with
// no window and no socket should be, because that is the half a gate can hold
// still and examine.
//
// In core/ because the single-process client is its own server and DESIGN.md
// 3.4 puts inventories on the server: two implementations of a recipe are two
// answers to "does this make a pickaxe".
//
// **Recipes are held by NAME and not by id**, and that is the one decision the
// rest of this file follows from. A mod registers its recipes while it loads,
// and the item form of a block does not exist yet at that moment -- the engine
// mints one per block after every mod has had its say, so an id taken during
// loading is kNoItem for everything a mod is most likely to build with. Names
// are resolved at match time instead. It costs a lookup per cell per match,
// which is a few thousand hashes when a player moves a stack, and buys a table
// that cannot be built wrong by being built early.
//
// A recipe naming something nothing has ever registered is not an error and not
// a silent nothing: it never matches, and `unresolved` lists it so a server can
// say so once at startup rather than leaving a mod author to wonder.

#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "core/item/inventory.h"
#include "core/item/item_registry.h"

namespace cnp {

// The biggest grid a recipe may ask for. Three by three, which is the shape
// every player of this kind of game already has in their head, and the shape
// the pattern parser's three rows come from.
inline constexpr int kCraftMax = 3;

// A cell that must be EMPTY, in a pattern. Written as a name so the pattern is
// a row of names all the way across: "-" and not "" because an empty word in
// the middle of a row is invisible in a source file.
inline constexpr const char *kCraftHole = "-";

struct Recipe {
    std::string owner;         // which mod registered it, empty for the engine
    std::string result;        // by name, like everything else here
    uint16_t    count = 1;

    // Shaped: `pattern` is w*h names in row order, kCraftHole for a cell that
    // must be empty, and it is TRIMMED -- stored as its own bounding box, so a
    // one-cell recipe is 1x1 whatever the rows around it looked like.
    //
    // Shapeless: `pattern` is the ingredients as a list, one entry per cell the
    // player has to fill, order meaningless. w and h are 0.
    bool                     shaped = true;
    int                      w = 0, h = 0;
    std::vector<std::string> pattern;
};

class RecipeSet {
public:
    // A shaped recipe, from up to three rows of item names separated by spaces.
    //
    // Strings and not ids, and rows and not a flat list, because this is the
    // form a mod writes and the alternative is every caller splitting the same
    // string the same way -- there are two binaries with a mod host in them and
    // the second copy is where they would differ.
    //
    // The rows are trimmed to their bounding box before being stored, which is
    // what makes "the same shape anywhere in the grid" one comparison rather
    // than nine. Rows of different lengths are padded, so a mod need not write
    // trailing holes.
    //
    // Returns false and says why: an empty pattern, a pattern wider or taller
    // than kCraftMax, a result of nothing, a count of nothing.
    bool add_shaped(const std::string &owner, const std::string &result, uint16_t count,
                    const std::string &row0, const std::string &row1, const std::string &row2,
                    std::string &err);

    // A shapeless recipe, from one list of item names separated by spaces. The
    // player fills that many cells with those things, anywhere.
    bool add_shapeless(const std::string &owner, const std::string &result, uint16_t count,
                       const std::string &items, std::string &err);

    // What this grid makes, or nullptr.
    //
    // Registration order decides when two recipes both match, and that is
    // stated rather than resolved: a table where two recipes match one grid is
    // a table whose answer depends on order, and every rule for picking between
    // them ("shaped first", "the more specific one") is a rule somebody has to
    // learn. An EXACT duplicate is refused at registration instead, which is
    // the case that is always a mistake and is the only one that can be
    // detected without trying every grid there is.
    const Recipe *match(const ItemRegistry &items, const Inventory &grid, int w, int h) const;

    // What that recipe makes, as a stack. Empty for no recipe or a result this
    // registry has never heard of.
    ItemStack result_of(const ItemRegistry &items, const Recipe *r) const;

    // A thing that takes colours at a table (core.recipe_dye), and a dye, as
    // which of the sixteen (kDyeColours, 1..16; core.item_dye). One thing that
    // takes them and one to three dyes, anywhere in the grid, make that same
    // thing -- the same tag, so the same one -- with its first colours those
    // dyes in reading order and the rest as they were (ItemStack::tag).
    bool add_dyeable(const std::string &item, std::string &err);
    bool add_dye(const std::string &item, int dye, std::string &err);
    // What is left of an item spent in a recipe (core.item_returns): a
    // bucket of milk leaves the bucket in its square.
    void add_returns(const std::string &item, const std::string &back) { m_returns[item] = back; }

    // What this grid makes: a thing dyed, or else what a recipe makes. The one
    // question a table asks; match and result_of are its second half.
    ItemStack craft(const ItemRegistry &items, const Inventory &grid, int w, int h) const;

    // One of everything in the grid, gone. The same for every recipe there is,
    // shaped or not, which is why it does not take one: a recipe says what has
    // to be there, and what it costs is one of each of them.
    //
    // Returns false and changes nothing when the grid does not make anything,
    // so a caller cannot spend a grid on a recipe that was not there.
    bool consume(const ItemRegistry &items, Inventory &grid, int w, int h) const;

    // Recipes naming something no registry entry has. For a server to print
    // once, so a mod's typo is a line in a log rather than a recipe that
    // quietly never works.
    std::vector<const Recipe *> unresolved(const ItemRegistry &items) const;

    size_t size() const { return m_recipes.size(); }
    const std::vector<Recipe> &all() const { return m_recipes; }

    // Reloading a mod. Simpler than the item table's, and the difference is
    // worth saying: an item id is held by every chest in the world, so a
    // dropped one keeps its number and becomes an orphan. A recipe is held by
    // NOBODY -- nothing in a save file points at one -- so a dropped recipe is
    // simply removed, and the mod's next load puts back whatever it still has.
    void   begin_reload(const std::string &owner);
    size_t end_reload(const std::string &owner);
    void   cancel_reload(const std::string &owner);

private:
    // The one place a recipe enters the table, and the one place a clash is
    // decided -- including the clash that is not one: a mod putting back a
    // recipe it is reloading.
    bool take(Recipe &&made, const char *clash, std::string &err);

    std::vector<Recipe>  m_recipes;
    std::vector<uint8_t> m_stale;   // parallel to m_recipes
    // ponytail: not undone by a reload; a mod that stops calling recipe_dye
    // keeps its things dyeable until the process restarts.
    std::unordered_set<std::string>      m_dyeable;   // by name, like a recipe
    std::unordered_map<std::string, int> m_dyes;      // name -> 1..16
    std::unordered_map<std::string, std::string> m_returns;   // add_returns

    ItemStack dyed(const ItemRegistry &items, const Inventory &grid, int w, int h) const;
};

// The words in a row, split on spaces. Exposed because both the parser and its
// gate want the same splitting, and a gate that splits its own way is a gate
// agreeing with itself.
std::vector<std::string> craft_words(const std::string &row);

} // namespace cnp
