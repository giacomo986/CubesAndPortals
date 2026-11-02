#pragma once

#include "core/net/protocol.h"
#include "core/portal/portal_traverse.h"

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

// Portals between servers built in the world (mods/portal's server frame):
// each one has a name, the friend servers it is offered to, and at most one
// portal of one of those friends it is linked with.
//
// Friends (admin.cpp, /friend ... portals) send each other, over the
// federation, the portals they offer to that friend (offer_for / offer_in):
// name, room, which mouth it is, its size, and what it is linked to. A link
// is asked for by one side (link: "ask") and made when the other side's next
// offer says it took it ("ok"). Once made it is DURABLE: kept in the world
// file (dump/load) with both servers' signing keys, it survives either server
// being off or silent, and comes undone only when one side lets go -- unlinks,
// closes the frame, stops offering it, resizes it: a "portals_unlink" line
// tells the other, and an offer still saying "ok" about a portal that is not
// linked back is answered with another -- or when either server's signing key
// is not the one it was linked under. Only a linked portal takes one of
// its room's mouths (sync): an opening that leads to another server, crossed
// on a ticket this server signs for the friend's key, which the client checks
// against the far server it reaches.
//
// One table for the process, under its own lock: the rooms' mods and the
// federation's tick both use it.

namespace cnp {
namespace farp {

// A friend that shares portals, as the federation knows it.
struct Friend {
    std::string name, host, key_hex;   // key: its signing key, 64 hex
};

// --- what the mods call (room: the caller's room) ---------------------------

// Opens or reshapes a server portal; `offered` the friends it is offered to,
// comma separated. False for a bad name or a lying shape: a mouth stands up.
// `ignore`: the checks this portal does without, comma separated --
// "offline" (the friend need not be heard from; a link asked waits for it
// for ever), "protocol" (no protocol in common), "size" (holes of two sizes).
bool set(const std::string &room, const std::string &name, const Portal &shape,
         const std::string &offered, const std::string &ignore, std::string &why);
// Closes it: its link goes too.
bool close(const std::string &room, const std::string &name);
// Links it to a portal a friend offers, or unlinks it (friend empty). "" when
// asked for, or why not: every check that can be made from here.
std::string link(const std::string &room, const std::string &name, const std::string &fr,
                 const std::string &far_room, const std::string &far_name);
// "none", "asking FRIEND\tROOM\tNAME", "linked FRIEND\tROOM\tNAME", and after
// a link that came undone "none\tWHY".
std::string state(const std::string &room, const std::string &name);
// The friends and what each offers this server, a line each:
//   S\tFRIEND\tok|WHY\tENGINE
//   P\tFRIEND\tROOM\tNAME\tfree|busy|yours\tWHY (empty when `name` could link to it)
std::string list(const std::string &room, const std::string &name);

// --- the federation ---------------------------------------------------------

// This server's UDP port, said in every offer with its protocol range.
void set_self(int udp_port);
// This server's signing key (hex), and the table as the world file kept it:
// links made under another key of this server's are dropped. Merged into
// what the mods already set.
void load(const std::string &my_key_hex, const std::string &kept);
// The table, for the world file.
std::string dump();
// The friends that share portals now. Links to one no longer here come undone.
void set_friends(const std::vector<Friend> &friends);
// The "portals" line for that friend.
std::string offer_for(const std::string &friend_name);
// That friend's "portals" line (payload without its first line's kind).
// `key_hex`: the key that signed it, which is that friend's now.
void offer_in(const std::string &friend_name, const std::string &key_hex, const std::string &body);
// That friend's "portals_unlink" line: one of its portals let go of ours.
void unlink_in(const std::string &friend_name, const std::string &key_hex, const std::string &body);
// The "portals_unlink" lines to send, as (friend, payload).
std::vector<std::pair<std::string, std::string>> take_unlinks();
// Whether something changed since the last call: offers go out at once.
bool take_dirty();
// Whether the table changed since the last call: written to the world file.
bool take_save();

// --- the rooms --------------------------------------------------------------

// What one of a room's mouths is to be, worked out by sync.
struct Mouth {
    int         index = -1;
    bool        open  = false;   // the opening is there; else only reserved
    Portal      shape;
    std::string host, far_room;
    uint16_t    port = 0, far_index = 0;
    uint8_t     key[net::kSignPublicBytes]{};
};

// Called by each room once a tick. `leads_away(k)`: whether mouth k leads to
// another world now (the room's own --portal-to ones included). Gives each
// linked portal a mouth, takes it back from one unlinked -- the last mouth is
// moved into the hole, as a world's mouths are always the first ones -- and
// returns every mouth this table holds in that room, and in `freed` the ones
// to clear. `trusted`: the keys of friends linked from this room, whose
// tickets it takes.
std::vector<Mouth> sync(const std::string &room, const std::function<bool(int)> &leads_away,
                        std::vector<int> &freed, std::vector<std::string> &trusted);

// For /portals: a line a server portal.
std::vector<std::string> lines();

} // namespace farp
} // namespace cnp
