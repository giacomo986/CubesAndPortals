#pragma once

// An inventory that belongs to a PLACE.
//
// DESIGN.md 8 lists "contenitori" in M7, and the sentence that makes them
// different from everything in core/item so far is short: a bag belongs to a
// session and a chest belongs to a cell. A session leaves and its bag goes with
// it; a chest stays where it was put, and two people can have their hands in it
// at the same time.
//
// That last part is the whole design. An inventory keyed by position is not a
// clever structure -- it is a map -- but it is the only shape in which "both of
// them are looking at it" is a fact rather than a race: there is ONE Inventory,
// the server owns it, and both sessions send requests against the same object.
// A copy per viewer would be two answers to "how many are in slot 3".
//
// In core/ and not in the server for the same reason Inventory is: the
// single-process client is its own server, and two implementations of a chest
// are two chests that disagree.

#include <cstdint>
#include <functional>
#include <unordered_map>
#include <vector>

#include "core/item/inventory.h"
#include "core/voxel/cell.h"

namespace cnp {

// Where a container is. A cell, and cells are what the world is made of, so
// there is nothing to invent: a chest is at the block that is the chest.
//
// An alias since node metadata arrived, because that is keyed by a cell too and
// two hashes over three integers are two hashes that agree until one of them is
// tuned. The name stays so that ninety call sites did not have to move.
using ContainerAt     = Cell;
using ContainerAtHash = CellHash;

// How many containers one world may hold, and how big one may be.
//
// Bounded because a mod opens these, and a mod that opens one per block it
// places would otherwise fill memory with empty chests nobody will ever look
// in. Refused rather than evicted: throwing away the oldest chest is throwing
// away what was in it.
inline constexpr size_t kMaxContainers    = 4096;
inline constexpr int    kMaxContainerSlots = 128;

// A container at a cell this far down or further is not in the world at all:
// it is CARRIED -- a backpack's (mods/backpack), one per player at a cell of
// its own down here -- and never out of reach, wherever its player walks.
// Saved and loaded like any other.
inline constexpr int    kCarriedBelow = -(1 << 30);

// Whether somebody at `eye` is close enough to the container at `at` to still
// have their hands in it.
//
// A free function and not a method, because it is about a point and a cell and
// neither of them owns the question -- and because it has to be the SAME rule
// in two places. The networked server closes a chest a player walked away from;
// the single-process client has to close the same one for the same reason, and
// the last three defects in this area were all one rule written twice. See
// ui::panel_cursor, which is here for the same reason.
//
// Measured from the eye to the middle of the cell, which is how the reach for
// breaking and placing is measured (core/net/session.cpp): the number handed in
// is `place_reach`, so what this says is "a chest you can no longer reach is a
// chest you are no longer in" rather than a second distance nobody can explain.
bool container_in_reach(const ContainerAt &at, float eye_x, float eye_y, float eye_z,
                        float reach);

class ContainerSet {
public:
    // Where a container's square roles come from (SlotRole): the block at its
    // cell, which only the owner of both the world and the registry can say.
    // Applied to the bag on every open() and find(). None: every square Store.
    using RolesOf = std::function<const SlotRules *(const ContainerAt &)>;
    ContainerSet() = default;
    explicit ContainerSet(RolesOf roles_of) : m_roles_of(std::move(roles_of)) {}

    // The container at that cell, making it if there is not one. Returns
    // nullptr only when it is refused: a slot count outside what a container
    // may be, or a world already holding as many as it may.
    //
    // An existing container is never made SMALLER: asking for 9 slots where
    // there are 27 returns the 27, because shrinking a chest somebody has
    // already filled loses stacks. Asking for MORE grows it, keeping every
    // stack in its slot -- a chest that has become half of a double chest is
    // the same chest with a second row of room (mods/chest).
    Inventory *open(const ContainerAt &at, int slots, std::string &err);

    // The container at that cell, or nullptr. Never makes one -- this is the
    // question a click asks, and a click that makes a chest is a click that
    // makes a chest anywhere.
    Inventory       *find(const ContainerAt &at);
    const Inventory *find(const ContainerAt &at) const;

    // The container at that cell, gone, and what was in it with it. For a block
    // that stopped being a chest. Returns whether there was one.
    //
    // What was inside is NOT dropped here: this class has no world to drop it
    // into and no idea what a drop is. Whoever breaks the block empties it
    // first -- see the gate, which is where that order is written down.
    bool erase(const ContainerAt &at);

    // How many times what is IN that container changed. Per cell and not one
    // counter for the world: two chests in use at once would otherwise send
    // each viewer the other one's changes.
    //
    // Zero for a cell with no container, which is also what a session that has
    // never been told anything holds -- so "never told" and "nothing there"
    // agree without a second flag. A live container starts at one.
    uint64_t revision(const ContainerAt &at) const;

    // What was in it changed. Called by whoever did the moving, because the
    // moving is done through Inventory references this class hands out: a
    // container that could only be written through a method here would need a
    // method for every way an inventory can change.
    void touched(const ContainerAt &at);

    size_t size() const { return m_map.size(); }

    // A process going on in it (core.container_progress): started at `since`
    // on the owner's clock and done `secs` later; 0 secs is none. Whoever looks
    // in sees a bar fill. Changing it is a change to what is in it (touched).
    void set_progress(const ContainerAt &at, double since, float secs);
    // How far it is at `now`, 0..1, and how fast it goes on (per second).
    // False for none, or no container.
    bool progress(const ContainerAt &at, double now, float &done, float &rate) const;

    // How far a catalogue is turned (core.container_scroll): where the rows on
    // show start and how much of the list they are, both 0..1; `size` 0 is no
    // scrollbar. Changing it is a change to what is in it (touched).
    void set_scroll(const ContainerAt &at, float pos, float size);
    bool scroll(const ContainerAt &at, float &pos, float &size) const;

    // A search box beside it (core.container_search), holding `text`. What is
    // typed in it comes back as core:ui_click from panel "core:chest", id
    // "search". Changing it is a change to what is in it (touched).
    void set_search(const ContainerAt &at, const std::string &text);
    bool search(const ContainerAt &at, std::string &text) const;

    // Every cell that has a container, in no particular order. For whoever has
    // to write them all down at once -- there is no other reason to walk a map
    // keyed by position, and a caller that wants one container has find().
    //
    // Cells and not references: a reference into the map is a reference the
    // next open() can move, and a save that walks a world while a mod is
    // running would be walking it through pointers that stopped being valid.
    std::vector<ContainerAt> cells() const;

private:
    struct Held {
        Inventory bag;
        uint64_t  rev = 1;   // one, so that zero can mean "never told"
        double    prog_since = 0.0;
        float     prog_secs  = 0.0f;
        float     scroll_pos = 0.0f, scroll_size = 0.0f;
        bool        searchable = false;
        std::string query;

        explicit Held(int slots) : bag(slots) {}
    };

    Inventory *with_roles(const ContainerAt &at, Inventory *bag);

    std::unordered_map<ContainerAt, Held, ContainerAtHash> m_map;
    RolesOf m_roles_of;
};

} // namespace cnp
