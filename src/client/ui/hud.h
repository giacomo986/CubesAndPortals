#pragma once

// The bar along the bottom of the screen, and the first thing in this engine
// drawn for a person rather than for a gate.
//
// DESIGN.md 8 puts "UI (microui/Clay guidata da Lua), HUD" in M7, and
// plat_debug_text has said "debug overlay only, a real UI arrives in M7" since
// M0. This is where that starts -- but it starts with the SURFACE (gfx_2d_*)
// and one thing drawn on it, not with a widget toolkit. microui and Clay both
// emit a list of rectangles, images and text runs; a back end for that list is
// what has to exist first, and a hotbar is the smallest honest customer for it.
//
// Deliberately stateless: everything it draws comes from the inventory and the
// registries it is handed. A HUD that kept its own copy of what the player is
// carrying would be a second source of truth for something the server owns.

#include "client/gfx/gfx.h"
#include "core/item/inventory.h"
#include "core/item/item_registry.h"
#include "core/registry/node_registry.h"
#include "core/ui/panel.h"

#include <string>
#include <vector>

namespace cnp::ui {

// What the hotbar needs to draw a block's face. The atlas and its layout come
// from the WorldRenderer of the world being stood in, so an icon is the same
// picture as the block: see WorldRenderer::atlas().
struct IconSource {
    gfx_texture atlas{0};
    int         tiles = 4;   // tiles per row, the layout kWorldFS reads
};

// Where the bar ended up, in pixels. Returned rather than recomputed by a
// caller that wants to know: the gate reads pixels out of these rectangles, and
// a gate that recomputed the layout would pass against its own arithmetic
// instead of against what was drawn.
struct HotbarLayout {
    float x = 0.0f, y = 0.0f;      // top-left of the whole bar
    float slot = 0.0f;             // one slot's side, including its border
    float pad  = 0.0f;             // gap between slots
    int   slots = 0;

    // The rectangle of slot `i`, or a zero rectangle for an index that is not
    // on the bar.
    void slot_rect(int i, float &out_x, float &out_y, float &out_w, float &out_h) const;
};

// Draws the hotbar centred along the bottom. Call between gfx_2d_begin and
// gfx_2d_end; returns where it put it.
//
// It takes SLOTS and not an Inventory, and that is not a detail: a networked
// player's bag is an InventoryState the server sent, and a single-process
// player's is an Inventory. Making this take one of the two would mean the
// other had to be converted into it every frame -- which is a second copy of
// something the server owns, kept in a type whose whole point is to be the
// authority. A pointer and a count is what both already are.
//
// `screen_w`/`screen_h` are the pass's dimensions and not the window's, because
// the gate draws this into a render target.
HotbarLayout draw_hotbar(const ItemStack *slots, int count, int selected,
                         const ItemRegistry &items, const NodeRegistry &nodes,
                         const IconSource &icons, int screen_w, int screen_h);

// The single-process case, which has a real Inventory to hand.
HotbarLayout draw_hotbar(const Inventory &bag, const ItemRegistry &items,
                         const NodeRegistry &nodes, const IconSource &icons,
                         int screen_w, int screen_h);

// One line the world said to this player, shown for a few seconds.
//
// It exists because a refusal that reaches a client and is never drawn is the
// same as no refusal at all. The server has been sending a sentence with every
// refused edit since M7's block-on-the-wire work -- ClientSession::edit_refusal
// has held it all along -- and nothing displayed it, so a mod protecting an
// area produced a player clicking at a wall with no idea why nothing happened.
// The single-process loop was silent for the same reason on its own side.
//
// Stateless drawing over a tiny piece of state, the same split as everything
// else here: `alpha` is arithmetic a gate can hand a clock to, and the drawing
// takes a number rather than working out the time itself.
inline constexpr double kNoticeHold = 3.0;   // seconds at full strength
inline constexpr double kNoticeFade = 1.0;   // and how long it takes to go

struct Notice {
    std::string text;

    // Far enough in the past that a notice is not showing before anything is
    // said. Zero would be "said at time zero", which on a clock that starts at
    // zero is a line on screen for the first four seconds of every session.
    double said_at = -1000.0;

    void say(const std::string &what, double now);

    // 1 while it is up, falling to 0 over the last second, and 0 when there is
    // nothing to show. A notice that is said again while one is up replaces it
    // and starts over: the newer sentence is the one that answers what the
    // player just did.
    float alpha(double now) const;
};

// Centred above the bar. `alpha` is what Notice::alpha returned; at 0 this
// draws nothing at all rather than a transparent rectangle.
void draw_notice(const std::string &text, float alpha, int screen_w, int screen_h);

// The chat, the classic block game's: T opens it, / opens it with the slash typed.
//
// Shut, a line shows for kChatHold seconds after it arrives and fades over
// kChatFade; open, the last kChatShown come back with the line being typed
// under them. `sent` is what Up and Down walk through, newest last.
inline constexpr double kChatHold  = 10.0;
inline constexpr double kChatFade  = 1.0;
inline constexpr int    kChatShown = 10;
inline constexpr size_t kChatKeep  = 100;

struct ChatLine {
    std::string text;
    double      at = 0.0;
};

struct Chat {
    std::vector<ChatLine>    lines;   // oldest first, kChatKeep at most
    bool                     open = false;
    std::string              line;    // being typed
    std::vector<std::string> sent;
    int                      back = -1;   // index into `sent` while walking it, else -1

    void add(const std::string &text, double now);
};

// Bottom left, over the world and under a panel.
void draw_chat(const Chat &c, double now, int screen_w, int screen_h);

// How far through a dig is, 0..1, under the middle of the screen.
//
// Under the crosshair and not over the block, because the block is what the
// player is already looking at and a bar drawn on it covers the thing it is
// about. At 0 this draws nothing at all: a dig that has not started is not an
// empty bar sitting on screen for ever, and every block in this game takes no
// time unless a mod says otherwise -- so for most worlds this never appears.
//
// It exists because holding a button for two seconds with nothing happening is
// indistinguishable from a game that has stopped working.
void draw_dig_progress(float t, int screen_w, int screen_h);

// A small plus at the middle of the screen: where a click lands.
void draw_crosshair(int screen_w, int screen_h);

// The whole bag, as a panel over the world.
//
// Deliberately NOT microui. DESIGN.md 2.7 names microui or Clay for "no
// declarative UI", and the mitigation still stands -- but what a bag needs is a
// rectangle, a grid of squares and hit testing, and none of those is a widget.
// What microui would really buy is FORMS: text fields, sliders, checkboxes,
// scrolling lists. That is the formspec-compat work of DESIGN.md 7.5, and
// vendoring a toolkit before the first form is vendoring it for the wrong
// reason.
//
// `hand` is the hotbar slot the hand is on; `dragged` is the slot a drag is
// currently lifting from, or -1. They are two different marks because they are
// two different things, and a player who cannot tell them apart cannot tell
// what is about to happen.
//
// The dragged slot is drawn EMPTY. The stack is under the cursor, and a bag
// that showed it in both places at once would be telling a player they have two
// of something they have one of. It is only a picture -- nothing has left the
// bag, and if the drop turns out to be nowhere the square fills back in.
struct PanelLayout {
    float x = 0.0f, y = 0.0f;
    float slot = 0.0f, pad = 0.0f;
    int   cols = 0, rows = 0, slots = 0;

    // From this square on, the bag's worn squares (Inventory::kWorn) and then
    // its arms (Inventory::kArms), drawn left of the grid on a figure: head,
    // chest, legs and feet down its middle, the back beside it, and the arms
    // over its shoulders -- only as many as the back opens (`arms`). -1 for a
    // panel with none. `figure` is the figure's left edge.
    int   worn_from = -1;
    int   arms      = 0;
    float figure    = 0.0f;

    // Past zero, the first row (the hotbar) is drawn LAST, this far below the
    // rest, so the bag reads as the bar it sits over. 0 for a chest.
    float hotbar_gap = 0.0f;

    // One square drawn on its own, outside the grid, at loose_x/loose_y: a
    // chest's bin. -1 for none.
    int   loose = -1;
    float loose_x = 0.0f, loose_y = 0.0f;

    // A chest's search box, left of its top row; zero wide for none.
    float field_x = 0.0f, field_y = 0.0f, field_w = 0.0f, field_h = 0.0f;
    bool  in_field(float px, float py) const
    {
        return field_w > 0.0f && px >= field_x && px < field_x + field_w && py >= field_y &&
               py < field_y + field_h;
    }

    void slot_rect(int i, float &out_x, float &out_y, float &out_w, float &out_h) const;

    // Which slot is under that point, or -1. The panel's own arithmetic and not
    // the caller's: a caller that recomputed the grid would be hit-testing its
    // own idea of where the squares are.
    int slot_at(float px, float py) const;
};

PanelLayout draw_bag(const ItemStack *slots, int count, int hand, int dragged,
                     const ItemRegistry &items, const NodeRegistry &nodes,
                     const IconSource &icons, int screen_w, int screen_h);

// What the cursor is over, as the name to show for it -- empty when it is over
// no square of this panel or over an empty one.
//
// Here and not in the loops because both of them ask it of three or four panels
// in a row, and because it is the panel's own arithmetic: slot_at is what says
// which square a point is in, and a caller working it out again would be
// hit-testing its own idea of where the squares are.
std::string name_under(const PanelLayout &at, const ItemStack *slots, int count, float mx,
                       float my, const ItemRegistry &items);

// The name of what is under the cursor, drawn beside it.
//
// A square with a picture in it says what a thing LOOKS like and nothing says
// what it is: a pick and a shovel are two grey squares to somebody who has not
// been told, and a stack of somebody else's mod's blocks is a colour. Drawn at
// the cursor and clamped to the screen, so a square in the far corner does not
// put its name off the edge.
void draw_item_tooltip(const std::string &text, float mx, float my, int screen_w, int screen_h);

// A container's grid, ABOVE the bag, drawn after it.
//
// The bag does not move to make room, and that is deliberate: the bottom row of
// the bag panel is the hotbar, and a row that shifts whenever a chest opens is
// the row a player's hand had learned. The chest goes above instead, which is
// also where every game of this kind puts it.
//
// Nine across, for the same reason the bag is nine across: two grids whose
// columns do not line up are two grids a hand has to learn separately.
//
// `bag` is where draw_bag put itself, so this reads the placement rather than
// working it out again -- the rule PanelPlacement exists for.
//
// `roles`, `count` of them or null, is what each square is for (SlotRole): a
// square that is not Store gets a bar of its role's colour along its bottom,
// and the colours are named over the grid. `progress`, 0..1, is how far what
// is going on in it is (core.container_progress): a bar under the squares; -1
// draws none. A last square that is a bin (kRoleTrash) is drawn outside the
// grid, right of its bottom row. `scroll_size` above 0 draws a scrollbar right
// of the grid, its thumb at `scroll` (core.container_scroll). `search`, not
// null, is a search box left of the top row holding that text, `typing` when
// it has the keyboard (core.container_search).
PanelLayout draw_chest(const ItemStack *slots, int count, int dragged,
                       const ItemRegistry &items, const NodeRegistry &nodes,
                       const IconSource &icons, const PanelLayout &bag,
                       const uint8_t *roles = nullptr, float progress = -1.0f,
                       float scroll = 0.0f, float scroll_size = 0.0f,
                       const char *search = nullptr, bool typing = false);

// A craft grid and the square it makes something in, ABOVE the bag.
//
// The same place a chest goes, because the two are never up at once: a player
// is either reaching into a box or arranging things to make one, and putting
// them side by side would be two grids to aim at for two things that are one
// screen apart in every game of this kind.
//
// `w` by `h` -- two by two is the grid in the corner of everybody's bag, three
// by three is what standing at a table makes it. NOT nine columns like the bag
// and the chest: a two-by-two stretched across nine columns is four squares
// with five holes, and the point of matching the bag's columns was that a hand
// learns one row. A small grid has nothing to line up with.
//
// The result sits to the right with a gap. Returned separately rather than as
// slot w*h of the same layout, because it is not a square of the grid: a hit
// test that could return it as "slot 4 of a 2x2" would be a hit test whose
// answer means two things.
struct CraftLayout {
    PanelLayout grid;
    PanelLayout result;   // one slot, numbered 0
};

CraftLayout draw_craft(const ItemStack *slots, int w, int h, const ItemStack &result,
                       int dragged, const ItemRegistry &items, const NodeRegistry &nodes,
                       const IconSource &icons, const PanelLayout &bag);

// The stack the cursor is dragging, drawn under the cursor. It is a PICTURE and
// not a place: the stack is still in the slot it came from, and the server has
// not been told anything yet. Nothing anywhere holds "the stack in the hand" --
// a stack halfway out of a bag is state the client owns and the server does
// not, and the first dropped packet leaves a player holding something nobody
// else knows about.
// A mod's core.hud.item draws the same square with the bag's own border.
void draw_drag_ghost(const ItemStack &stack, float cx, float cy, float side,
                     const ItemRegistry &items, const NodeRegistry &nodes,
                     const IconSource &icons, bool picked = true);

// What one press-and-release over the bag means.
//
// A pure function, and in the header, because both client loops have to agree
// about it and because the gate has to run the same code the loops run. Written
// inline in each loop it would be two rules that drift, and a gate that tested
// a third copy.
//
// Dragging is the ONLY way a stack moves. There was a click-then-click gesture
// here too and it is gone on purpose: two ways to do one thing means two states
// a player has to hold in their head -- "is something chosen right now?" -- and
// that state is invisible the moment they look away from the panel. A drag
// carries its own state in the mouse button, where a hand already knows it is.
// Where a press or a release landed. A slot AND which grid it was in, because
// with a container open there are two grids on screen and a slot number alone
// no longer says which square a player touched.
// Which of the grids on screen a slot number belongs to.
//
// An enum and not a pair of flags. It started as one bool ("is it the chest"),
// and a craft grid made it two -- at which point three of the four
// combinations are impossible and every call site has to know which. A name
// per grid says the same thing and cannot hold nonsense.
enum class Grid : uint8_t {
    Bag    = 0,
    Chest  = 1,
    Craft  = 2,

    // The one square that is not a square. Nothing can be put IN it, and taking
    // from it is not a move -- it is "make that, and pay for it out of the
    // grid". It has exactly one slot, numbered 0.
    Result = 3,
};

struct BagPoint {
    int  slot = -1;               // -1: not on any grid
    Grid grid = Grid::Bag;
};

struct BagAction {
    enum Kind {
        Nothing,   // the gesture ended somewhere that moves nothing
        Move,      // `from` -> `to`, in the grids the two flags name
        Drop,      // `from` leaves the bag entirely and lands in the world

        // `from` goes to the obvious other grid, wherever it fits. `to` and
        // `to_grid` mean nothing here and are not read: the player named one
        // square, and where it lands is a rule -- see net::quick_target.
        Quick,
    };

    Kind kind = Nothing;
    int  from = -1;
    int  to   = -1;

    // Which grid each end is in. Both Bag is a drag inside one bag, which is
    // what every move was before containers existed.
    Grid from_grid = Grid::Bag;
    Grid to_grid   = Grid::Bag;

    // How many of the stack the move is for. Zero is "all of it", the same
    // spelling MoveItem uses on the wire and move_between uses in core -- one
    // word for one idea, in the three layers a move passes through.
    uint16_t count = 0;
};

// `press` and `release` are where the two ends of the gesture landed.
// `press_has_item` is whether the square the press landed on holds anything.
//
// A drag that ENDS off the panel throws the stack into the world. That used to
// be Nothing "because there is no ground to drop a stack on", and there is one
// now -- see core/ecs/drop.h. Dragging a stack out of the window is the gesture
// every game of this kind already uses for it, and it costs no new state: it is
// the same press and the same release, read at a point the panel does not own.
//
// Everything else moves nothing: a press that began off the panel is not the
// bag's however it ends (otherwise a click aimed at the world behind lands in
// the bag), a press and release on the same square is a click and clicks no
// longer move anything, and a drag that started on an empty square is carrying
// nothing to either destination.
BagAction bag_click(BagPoint press, BagPoint release, bool press_has_item);

// The right button pressed part way through a drag: one item of the carried
// stack into the square under the cursor, and the drag goes on.
//
// This is what this gesture model has instead of the half-stack in the cursor
// that other games use. There is no stack in the cursor here, on purpose -- a
// stack held between two squares is state the client owns and the server does
// not -- so "put one down" has to name the square it goes into, and the only
// square it can mean is the one the mouse is over.
//
// `press` is where the drag started and `at` is where the cursor is NOW; there
// is no release, because the left button is still down and the drag is not
// over. Everything it refuses, bag_click refuses too and for the same reasons,
// with one addition: never out of the result square. Taking an answer is
// all-or-nothing -- see craft_take -- and there is no one-ninth of a pickaxe.
BagAction bag_tap(BagPoint press, BagPoint at, bool press_has_item);

// A shift-click: the stack goes to the other grid without being carried there.
//
// A CLICK and not a drag -- down and up on the same square. Shift held through
// a real drag is still a drag, because the modifier does not change where the
// player pointed, and a shift-drag that teleported a stack somewhere else would
// be the one gesture in this panel whose result is not where the mouse ended.
//
// This is the one gesture that gives a click a meaning again. An unmodified
// click moves nothing on purpose (see bag_click), and that stays true: what
// this reads is shift AND click, which is a gesture nobody was making before.
//
// The answer square is allowed here, and means "make that until the grid or
// the bag runs out" -- the only place in the panel where one gesture makes more
// than one of something.
BagAction bag_shift_click(BagPoint press, BagPoint release, bool press_has_item);

// Where the health bar ended up, in pixels. Returned for the same reason
// HotbarLayout is: whoever draws something beside it hit-tests against what was
// actually drawn rather than against a second copy of the arithmetic.
struct HealthLayout {
    float x = 0.0f, y = 0.0f;
    float w = 0.0f, h = 0.0f;
    int   pips = 0;     // how many segments the bar is cut into

    // Where one segment ended up. The same service PanelLayout::slot_rect
    // provides and for the same reason: whoever asks about a segment asks the
    // layout that drew it rather than doing the arithmetic a second time.
    void pip_rect(int i, float &out_x, float &out_y, float &out_w, float &out_h) const;
};

// The health bar, above the hotbar and centred on it.
//
// Segments and not a smooth strip: a player has to be able to read "how much"
// at a glance while something is chasing them, and a bar that slides is a bar
// they have to measure. One segment per two points, which is the ten hearts
// every player of this kind of game already counts in.
//
// `max` comes in rather than being a constant here, because it comes over the
// wire for exactly that reason -- see net::Vitals.
//
// Drawn only when there is something to say: `max` of zero is "nobody has told
// this client yet", which is not the same as "empty" and must not look like it.
HealthLayout draw_health(int health, int max, int screen_w, int screen_h);

// The armour worn, as a second bar of ten over the health bar; nothing at 0.
void draw_armor(int armor, const HealthLayout &health);

// How many of a stack a right drag carries: half of it, rounded UP.
//
// One function, because three places need the answer -- the move that is sent,
// the ghost drawn under the cursor, and the count left in the square behind it
// -- and three copies of "(n + 1) / 2" is three chances for the picture and the
// move to disagree about which half moved.
//
// Rounded up because one of three carried and two left is not half of anything
// a player counted, and because rounding down makes a stack of one carry
// nothing at all.
uint16_t half_of(uint16_t count);

// A drag with the RIGHT button: half the stack goes, the other half stays.
//
// The nearest thing this model has to picking a stack up by halves.
// `press_count` is how many were in the square the drag started on, and how
// many of them travel is half_of's answer.
//
// The count is worked out from what the client is SHOWING, which is one round
// trip old at worst; the server clamps a count larger than the square holds, and
// answers with the whole bag either way. It refuses everything bag_click
// refuses, and one more thing: never out of the answer square, which is paid for
// all at once or not at all.
BagAction bag_half(BagPoint press, BagPoint release, uint16_t press_count);

// Whether a right-button press on that square starts a half-drag at all.
//
// Everywhere but the answer square. A craft is paid for all at once, so there
// is no half of one to pick up -- and a drag that started there could only end
// in nothing, with a ghost under the cursor saying otherwise the whole way.
bool half_drag_starts(Grid g);

// What the cursor should be doing, now that a mod's panel is up or is not.
//
// A pure function in the header for the same reason bag_click is one: both
// loops have to agree, and this is exactly where they did not. The panel took
// the cursor in each loop and neither gave it back, so a panel a mod closed
// left the player looking at the world with a free mouse and no way to turn.
// Written inline twice it was two rules; written here it is one, and a gate can
// run the one the loops run.
//
// `took` is the caller's memory of whether the PANEL is what freed the cursor.
// It matters: re-grabbing whenever no panel is up would snatch the mouse off an
// open bag, and grabbing only when the panel closes needs to know it was the
// panel that let go.
struct CursorWish {
    bool change = false;   // call plat_cursor_grab at all
    bool grab   = false;   // ... with this
    bool took   = false;   // and remember this for next frame
};

CursorWish panel_cursor(bool panel_up, bool bag_open, bool grabbed, bool took);

// Which slot the hand lands on after the wheel turned by `notches`.
//
// It WRAPS, both ways: nine slots in a row have no ends in the way a list does,
// and a bar that stops at the ninth makes a player turn the wheel eight times
// to get back to the first. Scrolling away from you goes left, which is what
// every game of this kind does and the opposite of what a scrollbar does.
//
// A pure function for the same reason bag_click is one: two loops call it, and
// a gate has to run the code the loops run.
int hotbar_scroll(int selected, float notches, int count);

// A panel a mod declared, drawn where PanelPlacement puts it.
//
// The drawing reads the placement rather than working the layout out again:
// a renderer and a hit test with two copies of the same arithmetic is how a
// button stops being where it looks. See core/ui/panel.h, which owns both.
//
// `hover` is the element the cursor is over, or -1, so a button says it is
// about to be pressed. A panel whose buttons do not react is one a player
// clicks twice to find out whether the first click landed.
//
// `typing` is the field that has the keyboard, or -1, and `typed` what it
// holds now: drawn in place of the text the mod gave it, with a caret.
//
// An Item element is drawn as a slot of the bag holding one of it, when the
// caller hands over what to draw it with; without, as a button with its name.
void draw_mod_panel(const Panel &p, const PanelPlacement &at, int hover, int screen_w,
                    int screen_h, int typing = -1, const char *typed = nullptr,
                    const ItemRegistry *items = nullptr, const NodeRegistry *nodes = nullptr,
                    const IconSource *icons = nullptr);

// An item's name as a player reads it: "rocks:red_sand" is "Red sand".
std::string item_display_name(const std::string &name);

} // namespace cnp::ui
