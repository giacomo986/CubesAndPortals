#pragma once

#include "core/ecs/components.h"
#include "core/ecs/drop.h"
#include "core/ecs/health.h"
#include "core/ecs/mob.h"
#include "core/item/container.h"
#include "core/item/inventory.h"
#include "core/item/item_registry.h"
#include "core/item/craft_grid.h"
#include "core/item/recipe.h"
#include "core/net/interest.h"
#include "core/physics/fall.h"
#include "core/world/daylight.h"
#include "core/physics/movement.h"
#include "core/media/media_set.h"
#include "core/net/protocol.h"
#include "core/portal/portal_traverse.h"
#include "core/ui/panel.h"

#include <array>
#include <cstdint>
#include <functional>
#include <map>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

// What a server does with a peer between "a socket connected" and "a player is
// in the world", with no socket in it.
//
// The state machine is the part that gets attacked, so it is the part that
// should be testable without two processes and a port. A peer that connects and
// says nothing, a peer that sends a second hello after it is already in, a peer
// that sends chunk traffic before it has been admitted -- all of those are
// three lines of test here and a scheduling problem over a real network.
//
// The transport calls in (`on_connect`, `on_message`, `on_disconnect`,
// `expired`) and gets back what to send and whether to close. It never gets
// back a decision it has to make itself.

namespace cnp {
namespace net {

using PeerId = uint32_t;
inline constexpr PeerId kNoPeer = 0;

// A peer that has connected and not finished the handshake holds a slot and
// costs memory. Ten seconds is long for a handshake and short for a slot.
inline constexpr double kHandshakeTimeout = 10.0;

// How long a Ready session may say nothing before the server decides it is
// gone. A playing client sends a position claim twenty times a second, so this
// is two hundred and forty missed ones -- not a quiet client, an absent one.
//
// It exists because the peer timeout ENet applies by default was closing
// sessions this code still considered alive: measured at 400 ms and 25% loss,
// a peer dropped 1.2 seconds after the last message arrived, because one
// reliable command's retransmission backoff had run past ENet's limit. Holding
// that timeout open needs somebody else to free the seat, and that is this.
inline constexpr double kSessionSilence = 12.0;

// How many handshakes' worth of patience a session gets before it has said
// anything since its welcome. See join_patience.
//
// Derived from the client's own constant rather than chosen beside it, because
// two numbers chosen in two files is exactly what OPEN-12 was: the client gives
// a join kHandshakeRtts round trips and the server gave every session a flat
// twelve seconds, so past a 300 ms link the server always gave up first and the
// client's window could never do anything.
//
// The half is the margin. The server has to OUTLAST the client, not tie with
// it: the client is the side that can retry, and it cannot retry a server that
// has already forgotten it. A handshake measures about one round trip -- 377 ms
// on a 400 ms link, measured -- so this is the same window with half of one
// again on top.
inline constexpr double kJoinSilenceHandshakes = kHandshakeRtts * 1.5;

enum class SessionState : uint8_t {
    Pending = 0,   // connected, has not been admitted
    Ready   = 1,   // handshake done
};

// The server's own copy of a body that drives (TODO.md OPEN-47): the chassis
// its client runs (core/physics/vehicle.h), put down where the client put it
// and stepped with the keys its claims carry. Bit for bit the client's own, so
// a claim that is not where this kart is was not driven there with those keys.
struct DriveShadow {
    Player body;   // body.vehicle is the chassis; none until the client puts one down

    // The server put the body somewhere, and the client's next put is there:
    // the place, the velocity and the heading it was sent.
    bool  pending = false;
    Vec3  pending_pos, pending_vel;
    float pending_yaw       = 0.0f;
    bool  pending_yaw_known = false;   // a correction on foot leaves the client its own

    // The number of the next record (Move::first_drive), unknown after a
    // placement until the first claim that follows it says.
    uint32_t next       = 0;
    bool     next_known = false;

    // Seconds of steps in hand, filled at the rate time passes: a client
    // sending more steps than time allows is running its clock fast.
    double bank    = 0.0;
    double bank_at = -1.0;

    // Nodes a put nobody asked for may still move the body -- the client
    // holding a body over ground it has not been sent, which puts the chassis
    // down afresh every frame -- filled at a node a second.
    float  put_bank    = 0.0f;
    double put_bank_at = -1.0;

    long resyncs = 0;
    long driven  = 0;   // claims the copy agreed with, for the log
};

struct Session {
    PeerId       id    = kNoPeer;
    SessionState state = SessionState::Pending;
    double       since = 0.0;   // when it entered its current state
    std::string  name;          // validated before it is stored
    uint16_t     protocol = 0;

    // What this session is for. An observer is a portal looking in: it gets
    // chunks and sees the people in the world, and it has no body, no avatar
    // and nothing it can change.
    Scope        scope = Scope::Enter;
    std::vector<std::string> features;

    // What this player has been sent and what they should be sent next. Lives
    // with the session because it dies with the session: a player who leaves
    // takes their queue with them.
    PlayerInterest interest;

    // Who this player is, when they said. DESIGN.md 4.4: the key is the
    // identity and the name is a label -- two players may pick the same name,
    // and neither can pick the other's key.
    uint8_t identity[kSignPublicBytes]{};
    bool    has_identity = false;

    // The value this session shares with that peer and with nobody else, from
    // the transport. What their proof was signed over; empty when the session
    // is unencrypted, and then no proof is possible at all.
    uint8_t binding[kKeyBytes]{};
    bool    has_binding = false;

    // The newest position claim accepted. A sequenced channel can still deliver
    // an old packet after a new one, and moving the interest centre backwards
    // would resend chunks the player already has.
    uint32_t move_seq = 0;
    bool     has_moved = false;

    // The body this session drives.
    //
    // An entity and not a pair of fields, and that is the whole point of
    // adopting an ECS before the milestone that needs one: what a player looks
    // like to everybody else has to be the same thing a mob looks like, or the
    // streaming path is not a path but a special case about connections. See
    // core/ecs/components.h.
    //
    // Created with the session and destroyed with it, so the accessors below
    // never have to ask whether it exists.
    flecs::entity avatar;

    // Where the server says this player is. The claim in a Move becomes this
    // only if it passes; otherwise the claim is thrown away and this is what
    // gets sent back. A client that never cheats never notices it exists.
    //
    // A reference into the ECS, and it must not be kept: creating or destroying
    // an entity, or adding a component, can move what it points at. Every use
    // site reads or writes through a fresh call, which is why these are
    // functions and not a cached pointer.
    // A seat (core.ride, TODO.md OPEN-47): the body this one is carried on, 0
    // for none, and where on it in its frame (x right, y up, z front). While it
    // is carried its claims are not looked at: step_riders puts it there.
    PeerId riding = 0;
    Vec3   seat;
    // Or a mod's machine, by entity (core.craft.seat), riding then kRideThing.
    uint64_t riding_thing = 0;
    static constexpr PeerId kRideThing = 0xFFFFFFFFu;

    // Which way the body faces, radians, 0 down -Z: its chassis's in a kart,
    // its look's on foot. What a seat on it turns with.
    float heading = 0.0f;

    MovementState       &authority() { return avatar.get_mut<ecs::Position>(); }
    const MovementState &authority() const { return avatar.get<ecs::Position>(); }
    ecs::Look           &look() { return avatar.get_mut<ecs::Look>(); }
    const ecs::Look     &look() const { return avatar.get<ecs::Look>(); }

    // What this player is carrying. On the body and not on the session, which
    // is the argument core/ecs/components.h makes about Body: a chest has an
    // inventory and no session, and an observer has a session and no inventory
    // (DESIGN.md 4.4, "scope OBSERVE = niente inventario"). Written as a
    // component both are one sentence.
    // Where this session is digging, and since when.
    //
    // A block with a hardness takes TIME, and the server is what holds the
    // stopwatch: a client that decided when a block broke would break granite
    // in a frame. Nothing about this is on the wire -- a dig is two Edits, the
    // one that starts it and the one that finishes it -- so a client that lies
    // about its own progress gains nothing, because the clock being read is
    // this one.
    //
    // The BLOCK is remembered as well as the cell: a dig was for what was
    // there, and a cell that became something else while it ran is a different
    // dig.
    bool     digging   = false;
    int32_t  dig_x = 0, dig_y = 0, dig_z = 0;
    BlockId  dig_block = kAir;
    double   dig_since = 0.0;

    Inventory       &bag() { return avatar.get_mut<Inventory>(); }
    const Inventory &bag() const { return avatar.get<Inventory>(); }

    // How much of this body is left. A component like the bag and for the same
    // reason: it belongs to the body, and a player who crosses a portal keeps
    // the session and gets a new one.
    ecs::Health       &vitals() { return avatar.get_mut<ecs::Health>(); }
    const ecs::Health &vitals() const { return avatar.get<ecs::Health>(); }

    // The fall this connection is in the middle of, and the clock its body
    // heals on.
    //
    // On the session and not on the body, because both are about the stream of
    // CLAIMS this connection makes: a fall is seen in the positions a client
    // sends, and no positions arrive for a body nobody is driving. When mobs
    // take falls they will bring their own watcher, driven by whatever actually
    // moves them.
    FallWatch  fall;
    ecs::Regen regen;

    // Whether its last accepted claim was flown (kMoveFlying on a world where
    // it flies): told to everybody else (protocol 97), who otherwise see a
    // body skimming the ground as one walking.
    bool flying = false;

    // Whether this client's bar is behind. Like inventory_sent and craft_owed:
    // one viewer, so there is no revision to compare -- there is only "it
    // changed and they have not been told".
    bool vitals_owed = true;

    // This session has the player's bag: read in when they became a body here,
    // and theirs until another room of this server takes it (take_carried in
    // the server). Only a session that holds it keeps it on disk.
    bool holds_bag = false;

    // A line a mod keeps about this body (core.player_carry), which goes with
    // it when the body itself goes into another room of this server -- a kart
    // driven through a mouth, and where it is on its lap. Written only while
    // the body is here (holds_bag); in memory, never on disk.
    std::string carried;

    // And whether the body itself went too: it drove into another room of
    // this server, which took it (take_carried). What is left here is a
    // window, and a mod asks (core.body_taken) so it does not leave a kart
    // behind for one that is being driven over there.
    bool body_taken = false;

    // How THIS body moves (TODO.md OPEN-47): the world's numbers from the
    // welcome, until a mod gives it its own with set_body_movement. The
    // validator checks this body against these, and `movement_owed` is the
    // message saying so, not yet sent.
    //
    // `movement_before` and `movement_since` are the second after a change, when
    // the body is checked against whichever of the old and the new numbers are
    // faster: its client goes on moving by the old ones until the message lands,
    // and slowing a body down must not correct the steps it had already taken.
    MovementRules movement;
    MovementRules movement_before;
    double        movement_since = -1.0;
    bool          movement_owed  = false;

    // Which numbers `movement` are (BodyMovement::gen, 0 from the welcome),
    // and the last few before them with when each stopped being the body's: a
    // claim's records say which they were driven by, and a set that stopped
    // longer ago than a change's window is not one a client still drives by.
    uint8_t movement_gen = 0;
    struct PastMovement {
        uint8_t       gen   = 0;
        MovementRules rules;
        double        until = -1.0;
    };
    PastMovement movement_past[4];

    // Which of the server's placements of this body is the last one sent
    // (Move::epoch): a claim made before the client took it is dropped.
    uint32_t epoch        = 0;
    long     stale_epochs = 0;

    // The server's copy of this body's kart, when it drives.
    DriveShadow drive;

    // And whether they have been told what time it is. True to begin with, so a
    // session that has just joined gets the sky before it gets its first frame
    // rather than a fifth of a second of noon.
    bool time_owed = true;

    // Whether this body died and has not been put back yet. The Teleport that
    // does it goes out from the tick and not from the reply to whatever killed
    // them: a death can happen inside a Move, inside a mod's handler or inside
    // a tick, and three call sites each building a Teleport is three chances to
    // build a different one.
    bool death_owed = false;

    // Where a mod said this body comes back after dying (core.player_spawn):
    // a bed. Instead of the world's spawn while set.
    bool  has_bed = false;
    bool  no_regen = false;   // core.player_regen(player, false)

    // core.player_factors: this body's walk and sprint, dig speed, blows taken
    // and regeneration against everybody else's. Walk is folded into
    // `movement`; dig travels in BodyMovement, its old value kept for the
    // second after a change (movement_since) as the speeds are.
    float walk_factor  = 1.0f;
    float dig_factor   = 1.0f;
    float dig_before   = 1.0f;
    float hurt_factor  = 1.0f;
    float regen_factor = 1.0f;
    // Each source's four (core.player_factors' `source`); the four above are
    // their product.
    std::map<std::string, std::array<float, 4>> factor_sources;
    // Owned ground this player is one of the owners of (core.owned_member):
    // what they take there is not marked.
    std::vector<uint32_t> owner_of;
    float bed[3]  = {0.0f, 0.0f, 0.0f};

    // How many claims have been refused. Not used to punish anybody -- it is
    // for the log, because one refusal is a lag spike and four hundred is a
    // player running a modified client.
    long corrections = 0;

    // Claims that arrived describing a session this one is no longer. Around a
    // crossing on a slow link there is one round trip of them, and they are
    // dropped rather than checked -- see the Move handler in session.cpp.
    long stale_claims = 0;

    // Where each opening stands with THIS player. Per session because a
    // trigger is a thing that happens to somebody: two players walking into the
    // same portal are two crossings, and one shared state would let the first
    // one's arrival cancel the second one's approach.
    PortalTriggerState portal_state[kMaxOpenings];

    // Mouths shut to this player (core.portal_seal), bit m: drawn red, not
    // walked into, and no Redirect for them. Forgotten when they leave.
    uint32_t sealed = 0;

    // How many crossings this player has been given. For the log and for a gate
    // to count, the same way `corrections` is.
    long crossings = 0;

    // Edits this session asked for and did not get. Same use as `corrections`:
    // one is a player clicking at the sky, four hundred is a modified client.
    long edits_refused = 0;

    // Which version of the openings this player has been told about. The
    // openings are world state and the message is per player, so somebody who
    // joins after a portal was placed needs the same message somebody watching
    // got when it was placed -- and this is the only thing that distinguishes
    // the two cases.
    uint32_t portals_sent = 0;

    // Whether this session has ever been told what it is carrying.
    //
    // Sent once when it becomes Ready and then on every change. Without the
    // first one a player who has not broken anything yet has no bar and no bag
    // -- the client draws the last thing the server said, and until an edit or
    // a slot move the server had said nothing. An empty bag is a fact and it
    // has to be sent like any other.
    bool inventory_sent = false;

    // The grid this player crafts in.
    //
    // On the SESSION and not in the bag, and that is the whole reason it is a
    // field at all: a bag is saved, and a saved grid is things a player put
    // down for a moment coming back on a screen they are no longer looking at,
    // or worse, sitting on disk out of sight. It also has no business being a
    // container -- nobody else may reach into it.
    //
    // Nine slots always, of which w * h are used. Two by two is the grid in the
    // corner of everybody's bag; three by three is what standing at a table
    // makes it. The SIZE is the state and there is no second flag: a client
    // told 3 knows a table is open, and two fields could disagree.
    //
    // Changing the size EMPTIES it, and that is not tidiness. The same nine
    // slots are read as a 2x2 or a 3x3, so slot 3 is (1,1) in one and (0,1) in
    // the other -- a grid that survived a resize would rearrange itself.
    CraftGrid   craft;

    // Whether this session is owed a picture of it.
    //
    // A flag and not a revision, unlike the container above, and the difference
    // is the number of viewers: a chest has several and each needs to know
    // whether IT is behind, while a craft grid has exactly one person who can
    // change it. So "it changed" and "you are behind" are the same fact, and a
    // number beside the flag would be a second way to say it.
    //
    // True to begin with: a session that has never been told anything is owed
    // the first picture, which is the same reason inventory_sent starts false.
    // Without it a client would not know how big its own grid is until it put
    // something in one, which it cannot do without seeing it.
    bool craft_owed = true;

    // Which revision of this world's panels this session has been told about.
    //
    // The same shape as portals_sent above and for the same reason: a panel is
    // world state and the message is per player, so somebody who joins while a
    // panel is up needs the message somebody already in the world got when it
    // went up. Zero means never told, which is right on both counts -- a world
    // whose panels never changed is at revision zero and owes nobody anything.
    uint64_t panel_rev = 0;

    // The container this session has open, if any. A cell, held by the SERVER:
    // a client that could name a chest in its messages could name one across
    // the map, so what crosses the wire is net::Side::Container -- "the one I
    // have open" -- and this is what that resolves to.
    bool        container_open = false;
    ContainerAt container;

    // The last contents this session was told about. Two people can have the
    // same chest open, so a move by one is a message to both, and this is what
    // says who is behind.
    uint64_t container_rev = 0;

    // And whether they are owed one regardless of that number. Opening and
    // closing both have to be SENT, and neither one changes the contents --
    // "the chest you had is closed" and "you have never been told about a
    // chest" are the same revision and are not the same message.
    bool container_owed = false;

    // When this session was last told the world is still there, and which
    // number that was. Per session because the message is per session: a
    // player who joined a moment ago has not missed any.
    double   last_beat_at = 0.0;

    // When anything last arrived from this peer. The server's half of the
    // liveness question: the client has a heartbeat to watch and until now the
    // server had only ENet's peer timeout, which is not a rule anybody here
    // chose. See ServerSessions::silent.
    double   last_heard_at = 0.0;
    uint32_t beats_sent   = 0;

    // When this end last still had an answer of its own in flight to that
    // peer: reliable data sent and not acknowledged, or queued behind a window
    // that is full.
    //
    // This is the other half of OPEN-12, and the half join_patience does not
    // reach. That one gives a session longer while it has not spoken YET; this
    // one says a session cannot be counted silent across a stretch in which the
    // server itself was still talking. A client waiting on a world that is
    // still being retransmitted to it has nothing it can answer with, and that
    // silence is the server's own doing.
    //
    // Seen once, on `bad_link`: a player asked the far server to take them out
    // of its opening, that server GRANTED it -- "came out of this world's
    // opening 1 at 61 20 61" -- and twelve seconds later dropped them for saying
    // nothing, while the answer saying so was still being retransmitted to
    // them. The drop is what ended the retransmission: a server that gives up
    // on a peer destroys the answer it is still trying to deliver. And the
    // client's silence was deliberate -- it stops claiming a viewpoint on a
    // session the moment it asks to cross on it, or the claim would describe a
    // portal centre to a server that has already read it as a position.
    //
    // Written by whoever owns the socket, once a tick, and never read on its
    // own: see quiet_for.
    double   last_owed_at = 0.0;

    // Whether this session has said anything since it was welcomed.
    //
    // Not the same question as last_heard_at, and the difference is the whole
    // of OPEN-12: a client that has just been admitted has nothing to say until
    // the welcome reaches it, and the welcome is the biggest message this
    // protocol sends -- a block table and a media offer, fragmented. Counting
    // that wait as silence drops the player for not answering something they
    // never received.
    //
    // A field rather than `last_heard_at > since`, which is true today only
    // because of the order two assignments happen in a hundred lines apart.
    bool     spoke_since_welcome = false;

    // How long the handshake took, in seconds: connect to hello, which is
    // ENet's own exchange plus the key exchange.
    //
    // It is here because it is the only measurement of this client's LINK the
    // server has, and it was taken on this very connection rather than assumed.
    // See join_patience.
    double   joined_after = 0.0;

    // When this session last landed a blow. The cooldown ecs::kHitCooldown is
    // measured from here, on the server, because a client that sends a hit
    // every frame is exactly the client the rule exists for.
    double   last_hit_at = 0.0;

    // And how many it asked for and did not get -- same purpose as the two
    // below: one is a player swinging at air, and four hundred is a client
    // sending them faster than a person can click.
    long     hits_refused = 0;

    // When this player last said something in chat, for kChatGap.
    double chat_at = -1000.0;

    // How many placements this player asked for and did not get. Same purpose
    // as `corrections`: one is a player clicking at the sky, and four hundred
    // is something sending them faster than a person can click.
    long placements_refused = 0;

    // Whether this session got in on somebody else's word, and whose. Kept for
    // the log, and for the same reason the identity is kept: "admitted" and
    // "admitted because a server we trust vouched for them" are two different
    // lines to read after something goes wrong.
    bool    admitted_by_ticket = false;
    uint8_t ticket_issuer[kSignPublicBytes]{};

    // How many tickets this session has been issued. One per portal is normal;
    // a number that climbs is a client asking faster than a portal needs one.
    long tickets_issued = 0;

    // What this session has been sent of the world's media, and what it asked
    // for that the world does not have. The second number is the interesting
    // one: a client asking for hashes nobody offered it is either a stale cache
    // or somebody probing what this server holds, and either way the server
    // should be able to say how often rather than guess.
    long   media_sent      = 0;
    size_t media_bytes     = 0;
    long   media_unknown   = 0;
};

// What the transport should do about the message it just handed in.
// An edit the session layer has agreed to, for whoever owns the world to apply.
//
// The split is deliberate. core/net decides WHETHER: is this session a body,
// is the block within reach, is there something there to break, is the cell
// free, is the item in the hand. What actually happens -- the mod handlers, the
// write, the relight, the row in the database -- belongs to the caller, because
// core/net owns none of those and growing a scripting dependency to reach them
// would be the wrong end of the telescope.
//
// Nothing is spent or gained here. The caller calls edit_done() once the edit
// has really happened, so a placement a mod cancelled costs nothing -- see
// M7_FINDINGS 1.3, which is the same ordering the single-process client uses.
struct EditPlan {
    int32_t x = 0, y = 0, z = 0;   // the cell that changes -- derived, never sent
    BlockId want     = kAir;       // what goes in it; kAir when breaking

    // Which way the cell is turned once the block is in it, 0..3 quarter turns
    // about the vertical. Decided HERE, from the placer's position, because the
    // position is the authority's own state -- there is no look direction on
    // the wire, and inventing one for this would be a field a modified client
    // fills in.
    uint8_t facing   = 0;

    BlockId had      = kAir;       // what is in it now
    bool    breaking = false;
    int     slot     = 0;          // the slot that pays, when placing
    ItemId  drop     = 0;          // what the bag gains, when breaking
    int     wears    = -1;         // the slot whose tool dug it, -1 a hand or a block with no hardness
    bool    own      = false;      // a block a player placed on owned ground: not stolen

    // The block the right click landed ON, as opposed to the cell it would
    // build into. Every right click within reach at something solid is a use
    // first and a placement second, and a mod that CANCELS the use has claimed
    // the click -- which is how a chest opens with a block in hand.
    //
    // Not the same fields as x/y/z, and that is the point: with something in
    // hand a plan carries both, the block that was used and the cell that would
    // be built into, and one set overwriting the other is a placement landing
    // on the chest itself.
    bool    use      = false;
    int32_t use_x = 0, use_y = 0, use_z = 0;
    BlockId use_had = kAir;

    // And what was IN THE HAND when it landed, as an item.
    //
    // The click is the only moment the engine knows this. Inventory keeps a
    // selected slot, but on the server that is only written when a placement
    // succeeds -- so between two placements it is the slot of the last one and
    // not the hand. A mod asking "what is this player holding" at any other
    // time would be told something that was true a while ago, which is worse
    // than not being told: docs/archive/M8_FINDINGS.md 1.3 found the same shape in
    // `use_had`, which carries the block clicked and had been read as the hand.
    ItemId  use_holding = kNoItem;
};

// What a player is told when a mod cancels their edit.
//
// One sentence and not two. The networked loop produces it in the server binary
// and the single-process loop produces it in the client, and a player who is
// given different words in the two is a player learning two games. It lives
// here because this is the header both of them already include for EditPlan.
const char *refused_by_mod(bool breaking);

// Whether a hand holding a tool of `tool_level` gets a block that asks for
// `needs` out at all, and what to tell somebody whose tool does not.
//
// Total, so no caller has two branches that can drift: a block asking for 0 is
// dug by everything, including a bare hand, which is every block registered
// before levels existed. Here for the reason refused_by_mod is here -- the
// networked server refuses with this sentence and so does the single-process
// client, and two sentences for one rule is two games.
//
// The sentence names both numbers. "You need a better tool" sends a player to
// try every tool they own; "that needs a level 2 tool and this one is level 0"
// tells them what they are looking for.
inline bool tool_digs(uint8_t tool_level, uint8_t needs) { return tool_level >= needs; }
std::string refused_for_tool(uint8_t needs, uint8_t tool_level, const std::string &material);

// And WHICH KIND of work the tool is for. A tool's level and its speed are its
// own against the material it was made for, and a bare hand's against anything
// else: that is the whole of what makes an axe not a pickaxe.
//
// Two rules and not one, because they answer different questions -- one is a
// refusal and the other is a duration -- and both of them are asked by the
// networked server and by the single-process client. Total, like every rule
// beside them: a tool that names no material is a general one, and a block that
// says nothing about what it is made of is dug by anything for whatever that
// thing is worth. Both of those are what everything registered before materials
// existed is, so declaring them changed nothing that shipped.
uint8_t tool_level_for(const ItemDef &tool, const std::string &material);
float   tool_speed_for(const ItemDef &tool, const std::string &material, uint8_t ench = 0);

// Which square digs this material: the hand, unless a tool in an arm the back
// opens (Inventory::arm, mods/armpack) digs it better -- a higher level, or
// the same level faster. A player's whole bag; any other is the hand.
int best_dig_slot(const ItemRegistry &items, const std::vector<ItemStack> &slots, int hand,
                  const std::string &material);

// What a hit takes off: the hand's damage (a fist's, ecs::kPunch, for one that
// does none), plus that of every weapon in an arm the back opens -- the arms
// strike with it (mods/armpack). Sharpness a point a level on each.
uint16_t hit_damage(const ItemRegistry &items, const std::vector<ItemStack> &slots, int hand);

// How long a block of this hardness takes in a hand that digs this fast.
//
// One function, for the reason above: the server times a dig with it and the
// single-process client times its own with it, and two formulas for one rule
// would be a block that takes two seconds on one side and three on the other.
// A speed at or below zero reads as one, so a tool cannot ask to dig in no
// time at all.
inline double dig_seconds(float hardness, float speed)
{
    if (hardness <= 0.0f) return 0.0;
    return (double)hardness / (double)(speed > 0.0f ? speed : 1.0f);
}

// Where a stack goes when a player shift-clicks it.
//
// A SIDE and not a slot, because the player pointed at one square and not at
// two: out of a chest goes to the bag, out of the craft grid goes to the bag,
// out of the bag goes to the chest -- and out of the bag with no chest open
// goes nowhere, because a bag is not somewhere else from itself.
//
// Returns false when there is nowhere obvious, and leaves `to` alone. Here for
// the reason refused_by_mod is here: the networked server answers this and so
// does the single-process client, and two answers is two games.
bool quick_target(Side from, bool container_open, Side &to);

// Where a body that died comes back.
//
// Held rather than worked out here, because "the spawn" is a fact about a
// WORLD -- see cnp::spawn_in_front -- and this file has no world. It is handed
// in, and there is no way to not hand it in: a point and no flag, because a
// table with nowhere to put a body is a table whose dead fall for ever.
//
// It used to carry `known`, and the false case was reachable: a server started
// without portals never called set_spawn, so a body below the world was killed,
// healed, left where it was and killed again on the next claim. That is
// TODO.md CLOSED-16, and the flag went with it. What kept the seat warm
// afterwards was that the flag still existed -- the binary could no longer
// produce the state and the type still could.
struct SpawnPoint {
    float x = 0.0f, y = 0.0f, z = 0.0f;
};

struct SessionAction {
    std::vector<uint8_t> reply;         // empty means send nothing
    uint8_t              reply_channel = (uint8_t)Channel::Handshake;
    bool                 close = false; // and then disconnect the peer
    std::string          log;           // one line, already phrased for a human

    // Messages that follow the reply, on the event channel, in order.
    //
    // Here because one request can legitimately have several answers: a media
    // request names up to kMediaPerRequest files and each one is its own
    // message. Putting the loop in the caller would put it in two callers --
    // the real server and the loopback test -- and the second copy is where
    // the difference between what is tested and what runs would live.
    std::vector<std::vector<uint8_t>> more;

    // An edit this session is allowed to make. See EditPlan: the decision is
    // made here and the doing is the caller's.
    bool     has_edit = false;
    EditPlan edit;

    // A block this session right-clicked, within reach. Same split as has_edit
    // -- this layer decides the gesture is legitimate and whoever owns the mod
    // host emits the event.
    //
    // It exists because a container needs a way to be OPENED, and the only
    // gestures this protocol had were break and place.
    //
    // It comes BESIDE has_edit rather than instead of it: the use is emitted
    // first and is cancellable, and a mod that cancels has claimed the click,
    // so the placement in `edit` does not happen. That is what lets a chest
    // open with a block in hand -- the alternative, "a use only when the hand
    // is empty", makes a player put down what they are carrying to open a box.
    bool     has_use = false;
    EditPlan use;

    // A blow this session landed (core:hit): on what, how many points it took
    // and whether that was the last of them.
    bool     has_hit    = false;
    uint64_t hit_target = 0;
    uint16_t hit_taken  = 0;
    bool     hit_killed = false;

    // A right click on a worker creature (core:used_mob): which, by the low
    // bits of its id, and what was in the hand. Its mod decides what it means.
    bool     has_used_mob = false;
    uint32_t used_mob     = 0;
    ItemId   used_held    = kNoItem;

    // Why the placement was refused, when it was. Held here rather than sent as
    // `reply` because whether the player should ever SEE it depends on the use:
    // a mod that claimed the click was not refusing anything, and a player told
    // "there is already something there" after successfully opening a chest is
    // being answered about a thing they did not ask for.
    std::string edit_refusal;
    int32_t     refused_x = 0, refused_y = 0, refused_z = 0;

    // This session has just become Ready: the hello was accepted and the
    // welcome is in `reply`.
    //
    // Here rather than left to be worked out from the state, because "it was
    // Pending last time I looked and it is Ready now" is a thing only a caller
    // that polls every session every tick can notice, and the one thing that
    // has to happen exactly once at that moment -- reading this player's bag
    // off the disk -- has to happen BEFORE the first InventoryState goes out.
    // A caller that noticed a tick late would send an empty bag and then the
    // real one.
    bool has_joined = false;

    // A window that became a body here through an opening, and a body that
    // stepped back out to watching: when what the player carries arrives and
    // leaves, for a server whose worlds share it (its rooms).
    bool has_entered     = false;
    bool has_stepped_out = false;

    // A button this session pressed on a panel that is actually up. Same split
    // as has_edit: this layer decides the click is legitimate, and emitting the
    // event belongs to whoever owns the mod host.
    bool    has_ui_click = false;
    UiClick ui_click;

    // A line this player typed, trimmed and checked: a /command or something
    // said. Whoever owns the mod host and the room's players answers it.
    bool        has_chat = false;
    std::string chat;
};

// The shortest time between two lines from one player. A line sooner is
// dropped, so a client cannot fill everybody's screen and the log.
inline constexpr double kChatGap = 0.25;

// How long this session may say nothing before the server decides it is gone.
//
// Two answers, because there are two situations and one number cannot be right
// for both: a player who has been playing, and a client that has been welcomed
// and has not been told so yet. See the definition.
double join_patience(const Session &s);

// How long this session has actually been quiet at `now`.
//
// Not `now - last_heard_at`, which counts a wait the server itself caused
// against the client that was waiting for it. The clock starts at the LATER of
// the last thing heard from that peer and the last moment this end still owed
// it an answer.
//
// One function rather than the same subtraction written once where the drop is
// decided and again where it is logged: the log line used to print the
// constant kSessionSilence beside a session whose patience was something else
// entirely, which is a log that can lie about the rule it reports.
double quiet_for(const Session &s, double now);

class ServerSessions {
public:
    // The spawn is not optional and not a setter that may go uncalled. Every
    // other thing this table can be told -- a world, items, recipes, panels --
    // is declared optional above with what its absence costs; the spawn is the
    // one that cost a player twenty-two minutes of falling, so it comes in
    // through the door with the config.
    ServerSessions(ServerConfig config, SpawnPoint spawn)
        : m_config(std::move(config)), m_spawn(spawn)
    {
        for (int i = 0; i < kMaxOpenings; ++i) m_link[i] = i ^ 1;
    }

    // The world a claimed position is checked against. Optional: without it
    // there is no movement authority at all, and that is stated rather than
    // silently degraded -- a server that forgets to call this accepts any
    // position anybody sends.
    void set_world(const World *world, const NodeRegistry *registry);

    // The item table this world hands out and validates against. Optional in
    // the same declared way the world is: without it nothing can be placed,
    // because there is nothing an item could be, and the refusal says so rather
    // than the server quietly accepting anything.
    void set_items(const ItemRegistry *items) { m_items = items; }
    const ItemRegistry *items() const { return m_items; }

    // What a player's body wears: an id in this world's model table, 0 for the
    // box a body was drawn as before there was anything else to draw.
    //
    // One model for every player and not one each, which is the honest shape of
    // what exists today: a world says what a person looks like, and there is
    // nothing yet that would let one player look different from another. When
    // there is -- a skin, a mod dressing one body -- it is a `Wears` on that
    // avatar, which is where this writes anyway.
    void     set_player_model(uint16_t model);

    // And what ONE body looks like (core.body_model), which is how a world
    // where a player walks up to a kart and gets in draws the difference. 0
    // puts them back in the box. False when there is no such session.
    bool     set_body_model(PeerId peer, uint16_t model);
    uint16_t player_model() const { return m_player_model; }

    // The bodies. Handed out so a caller can put something in it that is not a
    // player -- which is the thing that makes this an entity system rather than
    // a session table with a library attached.
    flecs::world       &ecs() { return m_ecs; }
    const flecs::world &ecs() const { return m_ecs; }

    // Every creature through this world's own openings, after its step: one
    // that went into an opening comes out of the one it leads to, turned and
    // going the way the crossing turns it -- a cube thrown into a portal flies
    // out of the other. The same trigger a player's claim is judged by
    // (trigger_along), one record per creature and opening. Returns how many
    // crossed.
    int cross_mobs(double now);

    // A launch (core.player_launch): the body is sent that speed and the
    // server lets it move as a crossing's grace allows, `seconds` long.
    bool launch(PeerId peer, Vec3 vel, double now, double seconds);

    // Who is driving the body with this id on the wire, or nothing.
    //
    // The id an entity travels under is the ENTITY's, not the peer's -- one id
    // space, so a mob needs no second one and nothing has to know which kind it
    // is looking at. This is how a caller that does care gets back to a
    // session, and a null answer is an answer: something in this world that
    // nobody is connected to.
    const Session *driver_of(uint64_t entity_id) const;

    // Whether this session may make that edit, and what it would be. Fills
    // `why` when the answer is no, in a sentence meant for the player.
    // `now` is the server's clock, and it is a parameter rather than something
    // read off the last message because a DIG is timed with it: the first swing
    // at a block that takes time starts one, and the swing that breaks it is
    // the one made after the seconds have passed. Not const for that reason --
    // where a session is digging is state the session keeps.
    bool plan_edit(PeerId peer, const Edit &req, EditPlan &out, std::string &why, double now);

    // The edit happened. Spends what a placement cost or adds what a break
    // dropped -- and nothing else: the world was written by whoever called it.
    void edit_done(PeerId peer, const EditPlan &plan, double now);

    // One cell, for everybody who can see it. `why` is empty except on the copy
    // that goes back to whoever asked and was told no.
    //
    // The facing has no default on purpose. Every caller here reads the block
    // out of the world a line above; one that could leave the turn out would
    // read the block and forget the turn, and the cell would arrive holding the
    // right block facing whichever way it happened to be facing before.
    static std::vector<uint8_t> block_change_message(int32_t x, int32_t y, int32_t z,
                                                     BlockId id, uint8_t facing,
                                                     const std::string &why = "");

    // What that session is carrying, whole. Empty when there is no such session.
    std::vector<uint8_t> inventory_message(PeerId peer) const;

    // Who has never been told what they are carrying, and marking them told.
    //
    // Here rather than as a flag the server loop reads, so that "a player is
    // owed their bag exactly once, when they become Ready" is a sentence a gate
    // can ask about without a socket. Every later change is sent by whatever
    // caused it; this is the one nothing causes -- an empty bag at the moment of
    // joining -- and without it a player who has broken nothing yet has no bar
    // and cannot open their bag at all.
    std::vector<PeerId> inventories_to_send();

    // The panel that is up, as a message. `nullptr` means nothing is up, which
    // is a message too -- see PanelShow: a client cannot tell a panel that
    // closed from a tick in which nothing arrived.
    static std::vector<uint8_t> panel_message(const ui::Panel *p);

    // What THAT player should have on screen. Per peer and not per world: two
    // people in a world can be looking at different panels, which is the whole
    // of ui::PlayerId.
    std::vector<uint8_t> panel_message_for(PeerId peer) const;

    // Who is behind their own panel revision, and marking them told. A session
    // is named to the panels by its PeerId -- one number, no second table to
    // keep in step, and ui::kAllPlayers is kNoPeer so the two can never
    // collide.
    std::vector<PeerId> panels_to_send();

    // What this world's panels are, so a click can be checked against the panel
    // that is actually up rather than against the one the client says it has.
    // Optional and silent when absent: a server that never calls this has no
    // panels, and every click is refused.
    //
    // NOT const: a session that leaves has to be forgotten by the panels too,
    // or the list of what is on screen grows by a row per player who ever saw
    // anything. on_disconnect does it.
    // Asked before a name is let in: whether this identity may play under this
    // name here, with nullptr for a hello that carried no identity. A server
    // that keeps a world file sets it, and there a name belongs to whoever
    // first used it; without one the only rule is the one below -- two players
    // cannot wear one name at the same time.
    using NameCheck = std::function<bool(const std::string &name, const uint8_t *identity)>;
    void set_name_check(NameCheck check) { m_name_check = std::move(check); }

    // And after it, whether this server lets this player in at all: a ban or a
    // whitelist (src/server/admin.h). False with the reason and the sentence.
    using AdmitCheck = std::function<bool(const std::string &name, const uint8_t *identity,
                                          PeerId peer, RefuseReason &reason, std::string &why)>;
    void set_admit_check(AdmitCheck check) { m_admit_check = std::move(check); }

    // Told of each player that comes through a portal on a ticket: the server
    // that signed it, and who. For the administration's record of who sends
    // players here (src/server/admin.h).
    using CrossingSeen = std::function<void(const uint8_t *issuer, PeerId who)>;
    void set_crossing_seen(CrossingSeen seen) { m_crossing_seen = std::move(seen); }

    void set_panels(ui::PanelSet *panels) { m_panels = panels; }

    // The containers this world holds. Optional and silent when absent: a
    // server that never calls this has no chests, and a move naming one is
    // refused for naming a chest that is not there.
    void set_containers(ContainerSet *containers) { m_containers = containers; }
    ContainerSet *containers() { return m_containers; }

    // Ground somebody owns (core.owned_area): what a player takes out of a
    // container there, breaks there or spills there carries the owner's
    // number in its tag's low bits -- a village's goods, marked stolen. The
    // same corners again change the number; 0 takes the area away. In memory.
    struct OwnedArea {
        int      x1, z1, x2, z2;
        uint32_t tag;
    };
    void     set_owned(int x1, int z1, int x2, int z2, uint32_t tag);
    // 0: nobody's -- or a cell whose block a player put there themselves
    // (own_cell, the server's node meta): their own chest is not stolen from.
    uint32_t owned_tag(int x, int y, int z) const;
    // Single columns owned (core.owned_cell), before any area: a player's
    // territory, or a lot of theirs inside a village. 0 takes it away.
    void     set_owned_cell(int x, int z, uint32_t tag);
    // Whose the column is, own cells or not (core.owned_at).
    uint32_t owned_at(int x, int z) const;
    // owned_tag, but 0 where this session is one of the owners.
    uint32_t owned_tag_for(const Session &s, int x, int y, int z) const;
    void     set_own_cell(std::function<bool(int x, int y, int z)> f) { m_own_cell = std::move(f); }

    // Puts a container on that session's screen, or takes it off. The server's
    // side of a mod calling core.container_show: this layer holds WHICH cell,
    // so that Side::Container on the wire means something a client cannot lie
    // about.
    bool open_container(PeerId peer, const ContainerAt &at);
    void close_container(PeerId peer);

    // The container at that cell, gone, and off every screen that had it open.
    //
    // One call and not two, which is the whole reason it exists. Erasing
    // through the set alone leaves `container_open` true on everybody who was
    // looking into it: the close does go out, because the revision drops to
    // zero, but the session still says it has that CELL open -- so a new chest
    // built on the same spot puts the session behind again and the container
    // opens by itself on the screen of somebody who never touched it.
    //
    // Whoever breaks the block still empties it first: this class has no world
    // to drop anything into. See core.container_spill.
    void erase_container(const ContainerAt &at);

    // Every cell somebody has a container open at, once each and in no
    // particular order.
    //
    // Asked rather than tracked, because "who is looking into what" is already
    // on the sessions and a second table beside it is a second truth: a session
    // that leaves, is redirected, walks out of reach or has its container
    // erased would have to remember to update it, and the one path that forgot
    // would leave a chest open for ever with nobody inside.
    std::vector<ContainerAt> open_cells() const;

    // What changed since the last time this was asked: a message per cell that
    // has just been opened or just stopped being open.
    //
    // The DIFF is here and not in the server's loop because it is a rule and
    // not plumbing -- and because a gate can then ask it without a socket. The
    // loop's job is to put the answers on the wire.
    //
    // Asked once a tick rather than sent from each of the places a container
    // opens or closes: the click, the close message, walking out of reach, the
    // container being erased under somebody, a session dropped, a session
    // redirected. The one path that forgot would leave a chest standing open
    // with nobody in it.
    std::vector<CellOpen> open_cell_changes();

    // Closes the container of anybody who has walked out of reach of it, in
    // whatever state it is in: what is inside stays inside, and anybody still
    // standing at it goes on looking at it.
    //
    // Reach and not a distance of its own -- see container_in_reach. Returns
    // who was closed, because a caller that wants to log or count it should not
    // have to ask the same question again.
    std::vector<PeerId> close_far_containers();

    // A three-by-three at that cell, on that player's screen. The server's side
    // of a mod calling core.craft_show.
    //
    // Anything already in the grid goes back to the bag first, and what will
    // not fit falls on the ground: the same nine slots are read as a 2x2 or a
    // 3x3, so a grid that survived the resize would rearrange itself.
    bool open_craft(PeerId peer, const ContainerAt &at, double now);

    // Back to the two-by-two in the player's own bag, emptying it the same way.
    void close_craft(PeerId peer, double now);

    // And for everybody who has walked out of reach of the table they were
    // standing at. Reach and not a distance of its own -- the same rule and the
    // same function as close_far_containers.
    std::vector<PeerId> close_far_crafts(double now);

    // What that session's grid holds and what it makes, as a message.
    std::vector<uint8_t> craft_message(PeerId peer) const;

    // Who is behind their own grid. Per session and not per world: a craft grid
    // belongs to one player and nobody else can change it.
    std::vector<PeerId> crafts_to_send();

    // What that grid currently makes, or an empty stack. Public because the
    // gate asks it directly, and because it is the one question a caller might
    // want without a message.
    ItemStack craft_result(PeerId peer) const;

    // The recipes of this world. Optional and silent when absent: a server that
    // never calls this has no recipes, and every grid makes nothing.
    void       set_recipes(const RecipeSet *recipes) { m_recipes = recipes; }

    // MOVES where the dead come back. The table was built with one -- see the
    // constructor -- so this is a world changing its mind, not a world being
    // given its first answer.
    //
    // Nothing in the server binary calls it today: the point is derived once,
    // before the table exists, and never moves. It is kept because a bed, a
    // place to restart or a mod saying "come back here" is the same call, and it is
    // gated the same way the constructor is.
    void set_spawn(float x, float y, float z);

    // And what it ended up being, read back rather than remembered by the
    // caller.
    //
    // It exists because the line the server logs about it used to be a
    // computation printed beside a table that had not been told: sabotaging the
    // spawn left the log saying where the dead come back while nothing could
    // put anybody there, and the gate on that line stayed green. Same shape as
    // the drop line in M7_FINDINGS 31.7 -- a log that reports a rule it is not
    // reading.
    const SpawnPoint &spawn() const { return m_spawn; }

    // Takes health off a body, and kills it if that empties it. Returns what is
    // left, which is 0 for a peer with no body and for an observer -- both of
    // which are "nothing happened" and not "it died".
    // `bare` of the amount goes through armour (a creature's pierce).
    uint16_t hurt(PeerId peer, uint16_t amount, double now, uint16_t bare = 0);

    // And puts it back, never above the maximum. Returns what is left.
    uint16_t heal(PeerId peer, uint16_t amount);

    // A full body's points (core.player_max_health): what it has now is cut to
    // it. False for a peer with no body.
    bool set_max_health(PeerId peer, uint16_t max);

    // What a body has now, for a mod that wants to ask before it acts.
    uint16_t health_of(PeerId peer) const;

    // The clock every body heals on. Called once per tick with the seconds
    // since the last one -- ecs::regen_step decides the rest.
    void step_health(double dt);

    // Whose bar changed since this was last asked, and the message that says
    // so. The same shape as crafts_to_send: one viewer, so "it changed" is the
    // whole of it.
    std::vector<PeerId>  vitals_to_send();
    std::vector<uint8_t> vitals_message(PeerId peer) const;

    // One body's movement (TODO.md OPEN-47). False for a player who is not here
    // or has no body, and for numbers this engine cannot run.
    bool set_body_movement(PeerId peer, const MovementRules &rules, double now);

    // A body arriving through a mouth still what it was in the room it left
    // (a kart, take_carried): these rules under the numbering the client
    // already drives by, and not a new one. Its client took the kart along
    // too and claims by the generation it last had here; a new one made those
    // claims a body on foot's, refused, and the kart put back at a stop.
    bool adopt_body_movement(PeerId peer, const MovementRules &rules);

    // Whose movement changed since this was last asked, and the message that
    // tells them -- the vitals' shape.
    std::vector<PeerId>  movement_to_send();
    std::vector<uint8_t> movement_message(PeerId peer) const;

    // What a claim from this body is checked against: the world's limits, with
    // the speeds worked out from this body's numbers -- the wider of the old and
    // the new for the second after a change.
    MovementLimits limits_of(const Session &s, double now) const;

    // The world's clock.
    //
    // Here rather than in the server binary for the same reason the spawn is:
    // the sessions are what turn it into messages, and a second copy of "what
    // time is it" is the two-clock bug this whole thing exists to prevent.
    void   set_time(double t);
    double time_of_day() const { return m_time; }

    // How long a day lasts, in seconds. Never sent: the server decides how fast
    // its sun moves, and a client told the number would be free to disagree
    // between two updates -- which is the drift the updates exist to remove.
    void   set_day_length(double seconds);
    double day_length() const { return m_day_length; }

    // Moves it on, and marks everybody owed once the sky has actually moved --
    // see kTellEvery. Sending it every tick would be twenty messages a second
    // to say what nobody could see change; sending it once and never again
    // would leave every client drawing the hour it joined at.
    void step_time(double dt);

    // Who has not been told the time. Unlike the bar, this goes to OBSERVERS
    // too: somebody looking into this world through a portal is looking at its
    // sky, and a sky stuck at noon is the wrong world drawn correctly.
    std::vector<PeerId>  time_to_send();
    std::vector<uint8_t> time_message() const;

    // Who died since this was last asked, and where they go back to.
    //
    // A Teleport, because that is already the message for "the server moved
    // you", and a death is exactly that.
    struct Death {
        PeerId   peer = 0;
        Teleport back;
    };
    std::vector<Death> deaths_to_serve();

    // Which bodies entities_for sent each peer last, to keep them a little
    // longer at the edge of what fits.
    mutable std::unordered_map<PeerId, std::unordered_set<uint64_t>> m_last_sent;
    const RecipeSet *recipes() const { return m_recipes; }

    // What that session's open container holds, as a message. Closed when it
    // has none, which is a message too -- see net::ContainerState. `now` is
    // the clock a mod's core.container_progress was started on.
    std::vector<uint8_t> container_message(PeerId peer, double now = 0.0) const;

    // Who is behind the contents of the chest they have open, and marking them
    // told. Two people with the same chest open are both behind after either
    // one moves a stack, which is the reason there is one Inventory on the
    // server and not a copy per viewer.
    std::vector<PeerId> containers_to_send();
    bool has_authority() const { return m_world != nullptr && m_registry != nullptr; }

    // The pair of openings this world has, and whether it has any.
    //
    // DESIGN.md 3.4: the teleport is decided by the server and drawn by the
    // client. That makes the openings server state, checked against the
    // server's own world -- the same `Portal` the client's physics uses, since
    // core/portal has never needed a renderer.
    //
    // Optional, and silent when absent: a server that never calls this has no
    // portals, and a client walking where one would be walks through air.
    void set_portals(const Portal &a, const Portal &b);
    bool has_portals() const;

    // An opening a mod opens or closes while the world runs (core.portal_open,
    // core.portal_close): a frame of blocks somebody built. The opening already
    // standing at that centre in that plane keeps its index and takes the new
    // shape; anything else takes the first free index. Closing is by centre,
    // so a mod closes what it opened and not whatever holds that index now.
    // Both return the index, or -1: all kMaxOpenings in use, or nothing at that
    // centre. Closing leaves the link: the partner leads to an empty index,
    // which draws nothing, until the mod links it again.
    int open_portal(const Portal &p);

    // One of the openings that may be mouths (0 .. kMaxMouths-1) put or moved
    // while the world runs: a frame a player lit (core.portal_place), or a
    // third mouth's opening at startup. Its destination stays what it was.
    void move_opening(int index, const Portal &p)
    {
        if (index < 0 || index >= kMaxMouths) return;
        m_portal[index]     = p;
        m_portal_set[index] = true;
        openings_changed();
    }
    int close_portal(Vec3 center);

    // The same by index, and how many openings are neither in use nor a mouth
    // onto another room: what the server's virtual portals (core.portal_open,
    // src/server/main.cpp) have to hand out.
    void close_opening(int index);
    int  free_openings() const;

    // Links two openings to each other (core.portal_link), and whatever each
    // was linked to before to nothing. `b` of -1 unlinks `a`. False for an
    // index out of range; an index with no opening may be linked, so a mod can
    // link before it opens.
    bool link_portals(int a, int b);

    // The media this world serves. Optional and silent when absent: a server
    // that never calls this offers nothing, and every client that joins it
    // falls back to whatever it can produce locally -- which is what every
    // client did before this existed.
    //
    // A pointer, not a copy: one content set is read by every session, it does
    // not change while they run, and copying a few hundred kilobytes per join
    // would be a cost paid for nothing. The caller keeps it alive.
    void set_media(const MediaSet *media) { m_media = media; }
    const MediaSet *media() const { return m_media; }

    // What this world tells a joining session its blocks look like. Built here
    // rather than inline at the join so the server can measure it at startup
    // and print what a join is about to cost, instead of the number being
    // discovered on somebody's connection.
    MediaOffer media_offer_message() const;

    // The key this server signs tickets with, and the key of the server its
    // mouth points at.
    //
    // Two separate calls because they are two separate facts: the first is who
    // this server IS as an issuer, and the second is who it is issuing FOR.
    // Without the first, this world hands out no tickets. Without the second it
    // cannot, either -- a ticket with no audience is one that works at every
    // server that trusts this one, which is a different and much larger
    // promise than the one being made.
    void set_ticket_key(const SignKeyPair &key);
    bool can_issue(int opening) const
    {
        return m_has_ticket_key && opening >= 0 && opening < kMaxMouths && m_away[opening].has_key;
    }
    void set_destination_key(int opening, const uint8_t peer[kSignPublicBytes]);

    // A ticket for the world one of this world's mouths leads to -- the one
    // `req.opening` names -- for the player on this session. Refused, with a
    // reason, when this world does not issue them or when the asker has not
    // said who they are -- a ticket names a subject, and an anonymous session
    // has none to name.
    bool issue_ticket(PeerId peer, const TicketRequest &req, double now, PortalTicket &out,
                      RefuseReason &reason, std::string &why);

    // Whether a ticket lets THIS session do THAT, here, now. Signature, issuer,
    // audience, subject, scope, opening and expiry -- all of them, in one place,
    // because a check that lives in two places is a check with two answers.
    bool ticket_admits(const PortalTicket &t, const Session &s, uint8_t scope_bit,
                       uint32_t portal_id, std::string &why) const;

    // Where one opening leads, when it leads to another server.
    //
    // With the first one set, this world has MOUTHS rather than a pair: walking
    // into an opening is walking out of somebody else's, and the other opening
    // here is not what is on the other side. The second may lead to a third
    // world -- a sandbox with a door to an arena and one to a track -- and
    // when it leads nowhere it is unused. What the far end looks like is not
    // stored -- see PortalAway for why a copy of it would be a copy that goes
    // stale.
    // `portal` is WHICH opening over there this mouth comes out of -- the far
    // world has two, and the side that configured the link is the side that
    // knows which one it meant. It travels to the client in the Portals
    // message and back out again in the ChangeScope the client sends there.
    // `room` is which of the far server's worlds (protocol 56); with no host
    // and no port, a room of this same server.
    void set_destination(int opening, const std::string &host, uint16_t port,
                         uint16_t portal = 0, const std::string &room = {});
    bool leads_away() const { return m_away[0].leads_away(); }
    // Where one of the two openings leads (set_destination).
    const PortalAway &away(int opening) const { return m_away[opening]; }

    // One opening, put where a player asked for it.
    //
    // The same entry point as set_portals for everything downstream: a portal a
    // player placed and a portal the server was started with are the same
    // portal, and a crossing through one has to be granted the same grace as a
    // crossing through the other. That is the whole point of the openings being
    // server state -- the server cannot grant a translation it does not know
    // about, and a portal it does not know about is a player teleporting
    // themselves.
    //
    // Returns false and says why when the placement is refused.
    bool place_portal(PeerId peer, const PlacePortal &req, std::string &why);

    // A session that was watching becomes a player, or a player steps back to
    // watching -- without reconnecting, which is the whole point: the interest
    // set stays, so nothing is streamed twice and there is no loading screen
    // between two worlds.
    //
    // Becoming a player carries where they are coming from; see ChangeScope for
    // what is checked about it. On success `out` holds the Teleport that tells
    // the client where this world put them.
    //
    // On failure BOTH out parameters are filled: `why` for the log a person
    // reads here, and `reason` for the ScopeDenied that goes back over the
    // wire. Two of them because they are for two different readers -- the
    // sentence is for whoever runs this server, and the code is what the client
    // can act on without parsing English.
    bool change_scope(PeerId peer, const ChangeScope &req, double now, Teleport &out,
                      RefuseReason &reason, std::string &why);

    // What this world is, right now, for anybody looking at it through a
    // portal. Built here because every number in it is one this class already
    // owns, and a second count of "how full is this" is a second thing to get
    // wrong.
    Heartbeat heartbeat_message();

    // What the openings are now, and a number that changes when they do. The
    // server loop sends the message to anybody whose `portals_sent` is behind.
    uint32_t             portals_version() const { return m_portals_version; }
    Portals              portals_message() const;
    const Portal        *opening(int index) const;

    MovementLimits       &limits() { return m_limits; }
    const MovementLimits &limits() const { return m_limits; }

    // Everybody else within `radius` blocks of this player, as the entity
    // message will carry them. Here rather than in the server loop because the
    // loopback test needs the same answer and two copies of "who can see whom"
    // is two things to get wrong.
    std::vector<EntityState> entities_for(PeerId peer, float radius) const;

    // Moves a player because the SERVER decided to, and grants the grace that
    // makes the next second of their movement legal.
    //
    // DESIGN.md 3.4 puts the teleport on the server's side of the wire -- "la
    // teletrasportazione è decisa dal server, il client la disegna" -- and this
    // is that decision arriving at the one place that would otherwise refuse
    // it. A portal crossing moves a player twenty blocks in a tick and hands
    // them speed the cap forbids; both are the speed check doing its job on a
    // move the server itself made.
    //
    // The grace is granted here and asked for nowhere. There is no message a
    // client can send that produces one, which is the whole difference between
    // this and a token a cheat would simply include.
    //
    // Returns the message to send. Without a look it is a Correct: a
    // correction the player did not earn, which keeps their aim. With one it is
    // a Teleport, which says "I moved you" and turns them -- placing a player
    // who has just joined at the spawn is that, and sent as a Correct it read on
    // the client as a claim refused from somebody standing still. An unknown or
    // not-yet-ready peer gets an empty action and nothing moves.
    SessionAction teleport(PeerId peer, float x, float y, float z, double now,
                           const std::string &why, const Vec3 *look = nullptr);

    // A seat (core.ride): `peer` is carried on `host` at `seat` in its frame,
    // or, with host 0, gets off where it is. Returns the message for `peer`,
    // empty when refused -- no such body, one on itself, a seat too far out.
    SessionAction ride(PeerId peer, PeerId host, Vec3 seat, double now);
    // The same on a mod's thing (core.craft.seat): its Position and Look, and
    // its Roll when it has one, carry the seat.
    SessionAction ride_thing(PeerId peer, uint64_t thing, Vec3 seat, double now);

    // Lying down (core.player_lie): `peer`'s body drawn on its back, its middle
    // at `at` and its head along `head`, or up again when `on` is false. The
    // body is put there too, as a teleport is. Returns the messages for `peer`,
    // empty when refused.
    SessionAction lie(PeerId peer, bool on, Vec3 at, float head, double now);

    // Every carried body onto its seat, once a tick. A body whose host has
    // gone is let off, and those messages come back to be sent.
    std::vector<std::pair<PeerId, SessionAction>> step_riders(double now);

    void          on_connect(PeerId peer, double now);

    // The channel binding for a peer, handed down by whoever owns the
    // transport. Called between the connection coming up and the hello
    // arriving; a peer with no binding can still join anonymously, but cannot
    // prove an identity. Passing null says there is none.
    void          set_binding(PeerId peer, const uint8_t *binding);

    SessionAction on_message(PeerId peer, uint8_t channel, const void *data, size_t size,
                             double now);
    void          on_disconnect(PeerId peer);

    // Peers that have been Pending longer than kHandshakeTimeout. The caller
    // disconnects them; nothing here touches a socket.
    std::vector<PeerId> expired(double now) const;

    // Ready sessions nothing has been heard from for as long as join_patience
    // allows them. The caller disconnects them; this table does not own the
    // transport.
    std::vector<PeerId> silent(double now) const;

    // "I still owe this peer an answer, as of `now`." Told once a tick by
    // whoever owns the socket, for every peer with reliable data of ours still
    // outstanding -- Transport::owed_bytes is how that is measured.
    //
    // A push and not a pull because this table has no transport to ask, and
    // giving it one would put a socket inside the state machine that is
    // deliberately testable without one.
    void note_owed(PeerId peer, double now);

    const Session *find(PeerId peer) const;

    // The mutable one exists for streaming: the sender marks chunks as sent on
    // the session's own interest, and there is nowhere else for that state to
    // live -- it has to die when the player does.
    Session *find_mutable(PeerId peer);

    // Admitted players, in ascending peer order so a server streaming to
    // several of them does it the same way twice.
    std::vector<PeerId> ready_peers() const;
    size_t         ready_count() const;

    // The two of them apart. A world full of players must still be watchable
    // through a portal, and a world full of watchers must still be playable, so
    // the two capacities are counted against different numbers.
    size_t         player_count() const;
    size_t         observer_count() const;
    size_t         size() const { return m_sessions.size(); }

    const ServerConfig &config() const { return m_config; }
    ServerConfig       &config() { return m_config; }

private:
    SessionAction refuse_and_close(RefuseReason reason, const std::string &detail,
                                   const std::string &log) const;

    bool check_identity(Session &s, const Hello &hello, std::string &why);

    ServerConfig                          m_config;
    // What open_cell_changes() last answered about, so the next call can say
    // what moved.
    std::unordered_set<Cell, CellHash>    m_open_cells;

    NameCheck                             m_name_check;
    AdmitCheck                            m_admit_check;
    CrossingSeen                          m_crossing_seen;
    std::unordered_map<PeerId, Session>   m_sessions;

    // Every body in this world, players and everything else. Declared before
    // the session table on purpose: the entities outlive the loop that destroys
    // sessions, and a world torn down first would leave the handles in those
    // sessions pointing at nothing.
    flecs::world                          m_ecs;

    const ItemRegistry *m_items    = nullptr;

    // See set_player_model. 0 until a world says otherwise, which is every
    // world that has not got a model.
    uint16_t            m_player_model = 0;
    ui::PanelSet       *m_panels   = nullptr;
    ContainerSet       *m_containers = nullptr;
    std::vector<OwnedArea> m_owned;
    std::unordered_map<uint64_t, uint32_t> m_owned_cells;
    std::function<bool(int, int, int)> m_own_cell;
    const RecipeSet    *m_recipes    = nullptr;

    // The last time this layer was told. Kept because on_disconnect has no
    // clock of its own and has to arm and date the stacks it drops: a caller
    // that had to pass one would be a caller deciding what "now" means for a
    // peer that has already gone.
    double              m_now      = 0.0;

    // Everything in a session's craft grid, back where a player can get at it:
    // the bag first, the ground for what will not fit. Private because it is
    // half of open_craft and half of close_craft and never a thing to do on its
    // own -- a grid emptied without its size being settled is a grid whose next
    // resize rearranges nothing, which reads as working.
    void empty_craft(Session &s, double now, int w, int h);

    // The two halves of being hurt, with the session already in hand. Private
    // because everything outside names a peer, and looking a session up twice
    // in one blow is how the second half acts on somebody else.
    // A blow this session landed, or did not.
    //
    // `taken` and `killed` are read off the health BEFORE the blow rather than
    // after it: a player who dies is put straight back on their feet whole
    // (kill_body), so a body asked afterwards whether it is alive says yes and
    // the one thing worth logging would be lost.
    struct HitOutcome {
        bool     landed = false;
        uint16_t taken  = 0;
        bool     killed = false;
    };

    // Everything a strike has to be true of, in one place: the striker is a
    // player and the server knows where they are, the cooldown has passed, the
    // target exists, has a body, can be hurt, is not the striker, is not a
    // stack lying on the ground, and is within reach.
    HitOutcome strike(Session &s, uint64_t target, int slot, double now, std::string &why);
    bool       in_reach(Session &s, flecs::entity e, const ecs::Position &p, std::string &why);
    bool       use_body(Session &s, uint64_t target, int slot, double now, std::string &why);

    // True when that blow killed it (and so it is whole again, to come back).
    bool hurt_body(Session &s, uint16_t amount, double now, bool armoured = true);
    void kill_body(Session &s, double now);

    SpawnPoint m_spawn;

    double m_time           = kNoon;   // a world that opens in the dark is a world nobody can see
    double m_day_length     = kDayLength;
    double m_since_told     = 0.0;   // seconds since the last telling, see tell_interval
    double m_wind           = 0.0;   // nodes the clouds have drifted, see kWindSpeed
    const World        *m_world    = nullptr;
    const NodeRegistry *m_registry = nullptr;
    // The other half of the same question, for a pair that leads somewhere
    // else: this server cannot walk the player through, so it says where to.
    //
    // Server-decided for the same reason every teleport here is: it owns the
    // opening and it is the one that sees somebody step into it. Whether the
    // client needs it is the client's business -- one with a live window onto
    // that world crosses without a reconnect and ignores this.
    SessionAction redirect_if_triggered(Session &s, const Move &move, const MovementState &was,
                                        double now);

    // One crossing, if this claim fired a trigger. Private because it is not a
    // thing a caller decides: it is what happens when somebody walks into an
    // opening the server put there.
    SessionAction cross_if_triggered(Session &s, const Move &move, const MovementState &was,
                                     double now);

    // The server put this body somewhere: the next placement (Move::epoch),
    // and its kart, when it drives, is put down afresh there by both ends.
    uint32_t placed(Session &s, Vec3 at, Vec3 vel, float yaw, bool yaw_known);

    // The numbers a claim's record says it was driven by, while its client may
    // still be driving by them; the body's own otherwise.
    const MovementRules &rules_of_gen(const Session &s, uint8_t gen, double now) const;

    // What the server's copy of a kart makes of a claim (TODO.md OPEN-47):
    // not a kart, no copy to judge it by yet, where the copy is, not where it
    // is, or a copy that lost the thread and has to be put down again.
    enum class Drive { Walks, Unknown, Accept, Refuse, Resync };
    Drive drive_claim(Session &s, const Move &move, double now, std::string &why);
    bool  put_fits(Session &s, const DriveEvent &e, double now, std::string &why);

    // The opening a face of a node implies: which way it points, how big it is,
    // and whether there is room for it. Static because it is a question about
    // the world and a node, not about a session.
    bool opening_from_face(const PlacePortal &req, Portal &out, std::string &why) const;

    MovementLimits      m_limits;

    // An opening that has been placed and one that has not are different
    // states, and `m_has_portals` used to be the only way to tell -- which made
    // "one end placed, the other not yet" impossible to represent. A player
    // placing a pair is in that state for as long as it takes them to walk to
    // the other wall.
    Portal   m_portal[kMaxOpenings];
    bool     m_portal_set[kMaxOpenings] = {};
    int      m_link[kMaxOpenings];   // symmetric; i ^ 1 until link_portals says otherwise
    uint32_t m_portals_version = 0;

    // Where each opening leads, in the wire's own shape: the same four facts
    // go out in the Portals message and nothing here needs more than they say.
    PortalAway  m_away[kMaxMouths];

    SignKeyPair m_ticket_key;
    bool        m_has_ticket_key = false;

    // Everything an opening changing has to do to the players already in the
    // world: their approach to the opening that was there is about an opening
    // that is not there any more.
    void openings_changed();

    // The far end of every linked opening within reach of `feet`, as that
    // session's interest windows (PlayerInterest::set_windows): what is behind
    // an opening four hundred blocks from its other end is streamed while the
    // player is near enough to look into it. None for mouths, whose far end is
    // another server's.
    void aim_windows(Session &s, Vec3 feet) const;
    // Whether openings i and j are a pair inside this world: both there, linked,
    // and neither a mouth onto another world. A world with mouths can hold
    // pairs of its own besides them (a mod's core.portal_open).
    // cross_mobs' memory: where each creature was last tick, and its record
    // at each opening.
    struct MobCrossing {
        Vec3               last;
        bool               seen = false;
        double             at   = 0.0;
        PortalTriggerState state[kMaxOpenings];
    };
    std::unordered_map<uint64_t, MobCrossing> m_mob_crossing;

    bool local_pair(int i, int j) const
    {
        const auto mouth = [&](int k) { return k < kMaxMouths && m_away[k].leads_away(); };
        return j >= 0 && m_portal_set[i] && m_portal_set[j] && !mouth(i) && !mouth(j);
    }

    const MediaSet *m_media = nullptr;

    // The nodes that hold floor portals up, for the movement check. Writes at
    // most kMaxSolidExemptions entries and returns how many.
    size_t floor_exemptions(SolidExemption *out) const;
};

} // namespace net
} // namespace cnp
