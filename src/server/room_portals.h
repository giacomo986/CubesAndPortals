#pragma once

#include "core/portal/portal_traverse.h"

#include <functional>
#include <string>
#include <vector>

// Portals between the rooms of one server, made while it runs (core.room_portal_*):
// each has a name of its own in its room and leads to one other, in another
// room. Unlike a --portal-to mouth it holds none of its room's mouths until
// somebody is near: the server gives the mouths it has to spare to the linked
// portals nearest a player, at either end, and takes them back from the rest
// (schedule_mouths, src/server/main.cpp). So a world holds as many as are
// built, and what is bounded is how many are in use near players at once.
//
// A link is two wishes that agree: each end names the other. One made from
// one end (link) sets both when the far portal is there; when it is not yet --
// a room whose mod has not said its portals again after a restart -- it waits,
// and is made when that portal comes and names nothing else. Nothing is kept
// here across a restart: the mod that set a portal says it and its link again.
//
// One table for the process, under its own lock: every room's mods and every
// room's tick use it.

namespace cnp {
namespace roomp {

// The rooms of this server, once, before any of them runs.
void set_rooms(const std::vector<std::string> &rooms);

// --- what the mods call (room: the caller's room) ---------------------------

// Sets or reshapes a portal. False, with why, for a bad name.
bool set(const std::string &room, const std::string &name, const Portal &shape, std::string &why);
// Gone, and whatever it led to leads nowhere.
bool close(const std::string &room, const std::string &name);
// Links it to a portal of another room, or to nothing (far_room empty). ""
// when done or waiting for the far one, or why not.
std::string link(const std::string &room, const std::string &name, const std::string &far_room,
                 const std::string &far_name);
// "none", "waiting ROOM\tNAME" (the far one is not there, or leads elsewhere),
// "linked ROOM\tNAME\tlive" with a mouth at both ends now, or "...\tidle".
std::string state(const std::string &room, const std::string &name);
// The other rooms and their portals, a line each:
//   R\tROOM
//   P\tROOM\tNAME\tfree|busy|yours\tWHY  (WHY empty when `name` could link to it)
std::string list(const std::string &room, const std::string &name);

// --- the rooms' ticks -------------------------------------------------------

// A linked portal of a room, as its scheduler sees it.
struct Item {
    std::string name;
    Portal      shape;
    std::string far_room, far_name;
    int         far_index = -1;    // the far end's mouth now, or -1
    int         index     = -1;    // this end's mouth now, or -1
    float       key       = 1e30f; // how far the nearest player is, at either end
};
// Every linked portal of that room; `near` says how far the nearest player of
// this room is from a point, and is kept for the far room's next call.
std::vector<Item> items(const std::string &room, const std::function<float(Vec3)> &near);
// That portal has that mouth now (-1: none).
void took(const std::string &room, const std::string &name, int index);

// For /portals: a line a portal.
std::vector<std::string> lines();

} // namespace roomp
} // namespace cnp
