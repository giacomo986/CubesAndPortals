#pragma once

// What an entity in this engine is made of.
//
// DESIGN.md 6.1 picks Flecs and says why: archetype storage, native relations
// for the multi-world case, and a Lua binding when M8 gets there. This file is
// the part that comes first -- the components the C++ side owns, before any of
// it is reachable from a mod.
//
// The reason it is here now and not in M8, where the milestone list puts it: M7
// asks for "mob base con ECS", which is eight weeks of inventory, crafting, UI
// and mapgen with an ECS assumed underneath it. Building the foundation during
// that is building it while standing on it.
//
// What is deliberately NOT here: a component for anything a session is. Keys,
// scope, tickets and the handshake state machine belong to a connection and not
// to a body -- a player who walks through a portal keeps their session and gets
// a new body over there, and a mob has a body and no session at all.

#include <cstdint>

#include "flecs.h"

#include "core/physics/movement.h"

namespace cnp::ecs {

// Where a thing is looking. Kept apart from MovementState, which is where a
// thing IS and what it is allowed to do next: the server has authority over the
// second and merely repeats the first.
struct Look {
    float yaw   = 0.0f;
    float pitch = 0.0f;
};

// This entity has a body other players can see.
//
// A tag rather than a flag on a session, and that is the whole point of the
// exercise: what gets streamed to a player is "the entities near them with a
// Body", not "the other sessions, minus the observers, minus the ones that have
// not said where they are". The first sentence is a query and a mob satisfies
// it; the second is a list of exceptions about connections.
struct Body {};

// This body is something a MOD made visible (TODO.md OPEN-20), not a person, a
// creature or a stack. What decides how the client draws it, and what makes it
// the only kind of body a mod may hide again: core.ecs.visible never touches a
// body the engine gave something.
struct Thing {};

// How big a mod's visible thing is to a shot or a burst (core.ecs.box): half
// its width round its middle and its height up from its feet. Without one it
// is half a node round, 0.6 high, at its feet.
struct ThingBox {
    float half   = 0.3f;
    float height = 0.6f;
};

// The connection driving this body, when there is one. A mob has no such
// component, which is how the streaming path tells the two apart without
// knowing anything about sessions.
struct DrivenBy {
    uint32_t peer = 0;
};

// On a body whose player has the left button down on something -- a dig, a
// hit -- as their client last said (net::Hold). What everybody else draws
// swinging.
struct Working {};

// And crouched, Ctrl held on foot: drawn low, and its reach measured from the
// lowered eye (movement.h kCrouchEye).
struct Crouching {};

// And lying on their back (core.player_lie, protocol 75): which way the head
// points, as a heading, which everybody else draws them turned to.
struct Lying {
    float head = 0.0f;
};

// What this body looks like: an index into the world's model table, 1 based,
// and 0 -- or no component at all -- for the box every body was drawn as before
// there was anywhere to say otherwise.
//
// A number and not a name, because this is what goes on the wire sixty-four
// bodies at a time; the table that turns it back into a name is sent once, in
// the welcome.
struct Wears {
    uint16_t model = 0;
};

// The MovementState from physics/ doubles as the position component. Not a new
// struct that happens to hold the same three floats: the server's authority
// over where something is and the record of where it is are the same fact, and
// two copies of it is how they come to disagree.
using Position = MovementState;

} // namespace cnp::ecs
