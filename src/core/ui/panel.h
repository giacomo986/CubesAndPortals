#pragma once

// A panel a mod declares and the engine draws.
//
// DESIGN.md 8 asks for "UI (microui/Clay guidata da Lua)" in M7, and every
// section of M7_FINDINGS so far has ended with "niente da Lua" -- four times.
// This is the first piece of it, and it is deliberately the smallest one that
// is not a toy: a mod says what is on a panel, the engine puts it on the
// screen, and a click comes back as an event.
//
// It is RETAINED and not immediate mode, and that is not a preference. microui
// and Clay are immediate: the program describes the whole UI every frame. Mods
// run on the server and the panel is drawn on a client, so an immediate-mode
// panel would mean sending the whole description sixty times a second down a
// socket. A declaration that is sent once and a click that comes back is the
// only shape that survives the wire -- and it is the shape formspec already
// has, which is where DESIGN.md 7.5 is going anyway.
//
// It lives in core/ and not in client/ for the same reason Inventory does: the
// single-process client and the server have to agree about what a panel IS, and
// two copies of that is how they stop agreeing. Only the DRAWING is client-side.
//
// Coordinates are in CELLS, not pixels. A panel written in pixels is a panel
// that is a postage stamp at 4K and covers the screen at 800x600; a cell is a
// fraction of the screen height, the same one a hotbar slot uses, so a mod
// author writes a layout once and it is the same shape everywhere.

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace cnp::ui {

// Who is looking at a panel.
//
// The same uint32_t a net::PeerId is, and deliberately so: a server names a
// viewer by the session it is already holding, with no second table to keep in
// step. `kAllPlayers` is net::kNoPeer, which can never be a real session, so a
// panel shown to "no player in particular" is the world's -- a server notice,
// not somebody's chest.
//
// A panel's DECLARATION is shared and its VISIBILITY is not, which is the whole
// of this type. Two players opening the same chest are looking at one
// description and two independent answers to "is it up"; one closing it must
// not close the other's. Before this, a panel had a single `open` flag and they
// did -- TODO OPEN-8.
using PlayerId = uint32_t;
inline constexpr PlayerId kAllPlayers = 0;

// The player at this keyboard, for the two places where there is exactly one:
// the single-process client, whose mods run beside the window, and a networked
// client's mirror of what the server sent it. Any value that is not
// kAllPlayers would do; it is named so the two agree.
inline constexpr PlayerId kLocalPlayer = 1;

// Bounds, all refused rather than clamped. A panel forty cells across does not
// fit on any screen, and quietly making it smaller would leave a mod author
// looking for the bug in their arithmetic.
inline constexpr float  kMaxPanelCells   = 30.0f;
inline constexpr size_t kMaxElements     = 128;
inline constexpr size_t kMaxPanels       = 64;
inline constexpr size_t kMaxPanelText    = 128;

// Panels that are up, across every viewer. A bound because this list grows with
// players and not just with mods: without it, a mod that shows a panel to
// everyone who walks past keeps a row per person forever.
inline constexpr size_t kMaxShown        = 512;

enum class ElemKind : uint8_t {
    Label  = 0,   // text, drawn where it is put
    Button = 1,   // text in a box that answers a click with its id
    Box    = 2,   // a rectangle of colour, for grouping
    Field  = 3,   // a line of text the player may edit; Enter answers with its id and the text
    Item   = 4,   // an item's picture in a square, by name in `text`, that answers a click like a button
    Model  = 5,   // a model turning slowly in a box: `text` is its id, a tint and the ids of pieces on it
};

struct Element {
    ElemKind    kind = ElemKind::Label;
    std::string id;     // buttons and fields: what core:ui_click carries back
    std::string text;   // a field's: what it holds until somebody edits it
    float       x = 0.0f, y = 0.0f, w = 0.0f, h = 0.0f;   // cells
    uint8_t     r = 60, g = 62, b = 70;                   // Box only
};

// A Model element (core.ui_model): what it shows, as its `text`.
inline Element model_element(float x, float y, float w, float h, int model, int tint, const char *pieces)
{
    Element e;
    e.kind = ElemKind::Model;
    e.text = std::to_string(model) + " " + std::to_string(tint);
    if (pieces != nullptr && *pieces != 0) e.text += std::string(" ") + pieces;
    e.x = x; e.y = y; e.w = w; e.h = h;
    return e;
}

// The declaration, and only that. Whether it is on somebody's screen is not
// here: see PanelSet::show, which takes a viewer.
struct Panel {
    std::string          name;
    std::string          owner;   // the mod that declared it
    float                w = 8.0f, h = 6.0f;   // cells
    std::vector<Element> elements;
};

// Where a panel ended up on a screen of a given size, and what is under a point.
//
// Pure arithmetic and in core/ rather than beside the drawing, so it can be
// checked without a window. The drawing reads THIS rather than working the
// layout out again -- a renderer and a hit test with two copies of the same
// arithmetic is how a button stops being where it looks.
struct PanelPlacement {
    float x = 0.0f, y = 0.0f;   // top-left, pixels
    float cell = 0.0f;          // one cell's side, pixels

    void rect_of(const Element &e, float &out_x, float &out_y, float &out_w, float &out_h) const;

    // The index of the BUTTON or FIELD under that point, or -1. Labels and
    // boxes are not hit: a click that landed on a caption is a click on the
    // panel, not on something that does anything, and answering with it would
    // send a mod an event for a thing it never asked to be clickable. A click
    // on a field puts the keyboard in it and sends nothing.
    int button_at(const Panel &p, float px, float py) const;
};

PanelPlacement place_panel(const Panel &p, int screen_w, int screen_h);

// What a client sends as a click's id when the player pressed ESC with a mod's
// panel up: the panel comes off their screen, and its mod hears core:ui_click
// with this id to forget whatever it kept for them. No element may have it.
inline constexpr const char *kPanelEscape = "core:esc";

class PanelSet {
public:
    // Declaring a panel. The same name from the same owner REPLACES it, which
    // is what a reload is; from a different owner it is refused, because a
    // panel's contents belong to whoever declared it.
    bool define(const std::string &name, const std::string &owner, float w, float h,
                std::string &err);

    bool add(const std::string &panel, const std::string &owner, const Element &e,
             std::string &err);

    // Puts a panel on ONE viewer's screen, or takes it off. `kAllPlayers` means
    // everybody in the world -- a server notice rather than somebody's chest.
    //
    // The viewer is not checked against anything: this class does not know what
    // a session is, and a panel shown to a player who has left is a row that
    // drop_player removes. What it does refuse is a mod showing a panel it does
    // not own, which is the same rule add() and define() follow.
    bool show(const std::string &name, const std::string &owner, bool open, PlayerId viewer,
              std::string &err);

    const Panel *find(const std::string &name) const;

    // What to draw for one viewer: the panel shown to them most recently, or to
    // everybody. One at a time PER VIEWER, and that is said out loud -- two
    // panels overlapping is a stacking order, a focus rule and a click that has
    // to pick, and none of those is worth having before the second panel exists.
    //
    // A world panel and a personal one compete on the same clock, so a mod that
    // opens a chest for somebody covers the notice that was up for everybody,
    // and closing the chest brings it back.
    const Panel *topmost_for(PlayerId viewer) const;

    // Whether that viewer has anything up at all, which is not the same
    // question: a panel may be declared, and shown to somebody else.
    bool anything_up() const { return !m_shown.empty(); }

    // A reload, the same two calls NodeRegistry and ItemRegistry take, and for
    // the same reason: what the new version declares again survives, what it
    // stopped declaring goes.
    //
    // Unlike a block there is nothing left behind to keep standing up -- a
    // world is not full of panels -- so a dropped one is erased rather than
    // turned into an orphan. A panel that is still open when its mod reloads
    // stays open, because a mod reloading is not a player closing it.
    void   begin_reload(const std::string &owner);
    size_t end_reload(const std::string &owner);
    void   cancel_reload(const std::string &owner);

    // Everything one mod declared, gone. For a mod being disabled rather than
    // reloaded, where nothing is coming back.
    size_t drop_owner(const std::string &owner);


    // How many times what a viewer can SEE changed.
    //
    // The panel a player is looking at is drawn on a client, so it has to be
    // SENT, and "has it changed since I last sent it" is the only question a
    // server loop needs to ask sixty times a second. Bumped by every call that
    // alters what topmost() returns or what is on it -- including a define that
    // only rewrote a label, because that is exactly the change a mod makes to
    // say something.
    //
    // NOT bumped by a change to a panel nobody can see, and that is the whole
    // subtlety. A mod builds a panel out of one define and a dozen adds; if
    // each of those counted, every player in the world would be sent a message
    // per call describing a panel that is not up. The first run of the gate in
    // net_loopback_test measured exactly that -- four messages where three were
    // owed -- and this is the sentence that was missing.
    //
    // A counter and not a hash: a change that happened to produce the same
    // bytes is still a change somebody made, and a client that has the right
    // bytes will simply be sent them again. Cheap. A hash would have to be
    // recomputed to be trusted, which is the work this avoids.
    uint64_t revision_for(PlayerId viewer) const;

    // Everything one player was shown, forgotten. For a session that left:
    // without it the list grows by a row per panel per player who ever saw one,
    // and kMaxShown eventually refuses a panel to somebody still here.
    size_t drop_player(PlayerId viewer);

    size_t size() const { return m_panels.size(); }
    const std::vector<Panel> &all() const { return m_panels; }

private:
    // One row per (panel, viewer) that is up. By NAME and not by index, because
    // end_reload erases from the middle of m_panels and every index after it
    // would have to be walked back -- the kind of bookkeeping that is right
    // until somebody adds a second erase.
    struct Shown {
        std::string panel;
        PlayerId    viewer = kAllPlayers;
        uint64_t    at     = 0;   // ordering, so "the topmost" has a meaning
    };

    Panel *find_mutable(const std::string &name);

    // Bumps the revision of everybody who can see that panel. What a viewer
    // must be told about is a change to something ON THEIR SCREEN: a redeclared
    // panel that nobody has up is nobody's business, and one that six people
    // have up is six people's.
    void touched(const std::string &panel);
    void bump(PlayerId viewer);
    size_t forget(const std::string &panel, PlayerId viewer);

    std::vector<Panel>   m_panels;
    std::vector<uint8_t> m_stale;   // parallel to m_panels, set by begin_reload
    std::vector<Shown>   m_shown;
    uint64_t             m_clock = 0;

    // Per viewer, plus kAllPlayers' own. revision_for() adds the two, so a
    // world panel changing reaches everybody and a personal one reaches one.
    std::unordered_map<PlayerId, uint64_t> m_revision;
};

} // namespace cnp::ui
