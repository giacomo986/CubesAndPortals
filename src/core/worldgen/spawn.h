#pragma once

// Where a player stands when a world puts them somewhere.
//
// It was in the client, next to the loop that used it. The server needs the
// same answer now -- a body that dies has to come back somewhere, and the place
// it comes back to must be the place it started -- and a second copy of this
// arithmetic would be two spawns in one world, differing by whichever seed
// found the difference first.

#include "core/math/vec.h"
#include "core/portal/portal_traverse.h"
#include "core/registry/node_registry.h"

namespace cnp {

// `stand_off` nodes in front of an opening, on the ground, and out of whatever
// happens to be standing there. See the comment on the definition: the search
// steps SIDEWAYS, which is what keeps the point derived from the portal rather
// than nudged until one seed looked right.
Vec3 spawn_in_front(const World &world, const NodeRegistry &reg, const Portal &p,
                    float stand_off);

// And where a player stands in a world with no opening in it: the middle of the
// plaza, on the ground, out of whatever is standing there.
//
// It exists because a world without portals had no spawn AT ALL, and a server
// with no spawn cannot put a dead body anywhere -- ServerSessions::kill_body
// makes it whole and returns, leaving it exactly where it died. A player who
// walks off the edge of a finite world then falls for ever: killed and healed
// on every claim, corrected every few seconds for falling too fast, and never
// landing.
//
// Found by the thirty minute run, at y = -130822 after twenty minutes of it.
// The rule that should have caught it was there and gated (net_loopback:
// "falling out of the bottom of the world puts the body back") -- and that gate
// sets a spawn itself, so it never once asked what a server without one does.
Vec3 spawn_in_plaza(const World &world, const NodeRegistry &reg, int extent);

} // namespace cnp
