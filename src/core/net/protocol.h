#pragma once

#include "core/media/content_hash.h"
#include "core/media/media_limits.h"
#include "core/net/bytes.h"
#include "core/net/crypto.h"
#include "core/physics/movement.h"
#include "core/physics/vehicle.h"
#include "core/registry/node_registry.h"
#include "core/voxel/chunk.h"

#include <array>
#include <cstdint>
#include <string>
#include <vector>

// The protocol: message framing, the versioned handshake, and feature
// negotiation (DESIGN.md 3.2).
//
// Nothing in this file knows what a socket is. That is deliberate -- the
// transport is ENet, and ENet needs two processes and a port to say anything at
// all, which makes every property below expensive to test and therefore untested
// in practice. Encoding, version rules, negotiation and refusal are decided
// here, on buffers, where a test can hand the parser eight thousand corrupted
// messages in a second.
//
// Two rules the design names and this file implements:
//
//   - **A versioned header plus a negotiated feature table**, so a new client
//     can talk to an old server. Not "so it does not crash": so it works, with
//     the parts both sides know.
//   - **Channels are fixed for the engine and assigned for mods** (0 handshake,
//     1 chunks, 2 entities, 3 events, 4 media, 5 blocks, 6+ mods). A mod's
//     channel number is not a constant anyone hardcodes; it comes back in the
//     welcome (DESIGN.md 6.5), and as of protocol 21 a mod actually uses one.

namespace cnp {
namespace net {

// 'CNP1' little endian. A packet that does not start with this is not ours --
// cheap to check, and it means a client pointed at the wrong port is told so
// instead of being disconnected by a parse error twenty fields later.
inline constexpr uint32_t kMagic = 0x31504E43u;

// Bumped when the wire format changes in a way an older parser gets wrong.
// kMinProtocol is how far back we still speak: a range, not a number, because
// "versioned" means nothing if the only accepted version is today's.
inline constexpr uint16_t kProtocol    = 109; // 109 the bodies in parts of a frame (Entities)
//
// And 24 is also as far back as this build speaks, which is the honest answer
// rather than a range this code cannot keep. 22 grew every body on the Entity
// channel by two bytes; 23 gives a chunk's payload a third plane, flagged on
// the encoding byte; 24 puts the same byte on a single cell's change, which is
// what a placement is. Nothing here encodes a message differently for an older
// peer: a 22 client handed a flagged encoding byte refuses it as an unknown
// encoding -- which is the good case -- and a 21 client would read every body
// shifted by two and see a world of people standing in the wrong places.
// Speaking an older version again means the codec branching on the agreed
// version, which is a thing to build when there is an older client worth
// keeping -- not a constant to leave behind and hope.
//
// 23 and 24 are a day apart and both of them are OPEN-27, which is a fair
// question to ask of a version number. They are two versions and not one
// because 23 shipped as a commit that a build could be sitting on: reusing it
// for a second, different wire would be one number meaning two formats, which
// is the failure a version exists to prevent.
// 26 widens the facing byte a single cell's change carries from four values to
// twenty-four (TODO.md OPEN-27). Same field, same width, more of its values
// legal -- so a 25 peer handed a byte of 7 refuses the message as malformed,
// which is exactly the case a version number exists to turn into a refused
// handshake instead of a refused cell.
//
// 27 puts a block's SHAPE in the welcome beside its name (TODO.md OPEN-43). The
// block table was names only, so a client rebuilt each definition out of its own
// build and a block it had never heard of became a magenta cube -- which is a
// fair picture of "I do not know this" and a wrong answer about where a body may
// go. A mod's slab was a cube to everybody who did not have that mod installed:
// the server let them stand on top of it and their own collision never let them
// try. A 26 client handed this welcome reads the table it wanted and then a
// field it does not know is there, so the version is what turns that into a
// refused handshake.
//
// 28 puts how a body moves in the welcome (TODO.md OPEN-44): walk, sprint, fly,
// jump and gravity, which a world's mods may now declare. Five floats after the
// shapes, sent to every client. A 27 client handed this welcome finds twenty
// bytes past the end of what it knows and refuses the whole message -- and one
// that did not would move at the engine's speeds in a world checking it
// against others, which is a client corrected every tick for obeying its keys.
//
// 29 adds a message and changes nothing that was there (TODO.md OPEN-47):
// BodyMovement, the same five numbers for one body, whenever a mod gives it
// some. A 28 client handed it has no such message type and refuses it as
// unknown -- and a client that ignored it would go on moving by the world's
// numbers while the server checks it against its own, which is the correction a
// version exists to turn into a refused handshake instead.
//
// 30 adds a kind of body to the entity message (TODO.md OPEN-20): Thing, what a
// mod made visible with core.ecs.visible. A 29 client handed one refuses the
// whole entity message -- its decoder refuses a kind it does not draw -- which
// is every body in it and not only the new one, so the version turns that into
// a refused handshake instead.
//
// 31 widens every body's id from 32 bits to 64 (TODO.md OPEN-48), in Entities
// and in Hit. An id is an index and a generation, and the generation is the top
// half: the first time anything died and its index was reused, the id on the
// wire named the dead body and not the living one, and a swing at a stack that
// stood where a creature had been was refused as "nothing with that id". A 30
// peer reads every body four bytes short and is refused at the handshake.
//
// 32 puts what a player's own body wears in the welcome (core.view). Every
// other body carries its model in the entity message; the one the client moves
// itself is never sent back to it, and a camera behind the player has to draw
// it. Two bytes after the movement numbers: a 31 peer finds them past the end
// of what it knows and refuses the welcome, which the version turns into a
// refused handshake.
//
// 33 says whether bodies drive (core.drive): four floats after the movement
// numbers, in the welcome and in BodyMovement -- acceleration, braking, turn and
// drag. A 32 client handed either finds sixteen bytes it does not know and
// refuses it; and one that read around them would walk in a world whose server
// checks it as a body that drives, which is a client corrected for every first step.
//
// 34 gives each opening its own destination: Portals carries two of them where
// it carried one, and a TicketRequest names the opening. A 33 peer reads the
// second destination as bytes past the end and refuses the message -- a mouth
// it never draws -- so the version turns that into a refused handshake.
//
// 35 says how wide a body is (core.body_width): a float after the driving
// numbers, in the welcome and in BodyMovement. A 34 peer finds four bytes past
// what it knows and refuses both, and one that read around them would move a
// kart as wide as a person through a world whose server checks it as a kart.
//
// 36 says how blocks and tools dig: a tool's level, speed and material after
// its name in the item table, and a table of each block's hardness, level and
// material after it. A 35 peer reads the first as the next item and refuses the
// welcome.
//
// 37 lets go of chunks: ChunksGone on the chunk channel names the ones the
// server has forgotten sending, and the world behind them is unloaded
// (TODO.md OPEN-38). A 36 client would never drop a chunk and never be sent it
// again while the server believed it had it -- a world that goes stale where
// the player has been away.
//
// 38 says what each block does to a body and to light: solid, opaque, liquid
// and its opacity, two bytes a block after the dig table. A 37 client reads them
// as the model table and refuses the welcome -- the good case; one that read
// around them would walk on a lake the server lets bodies sink into.
//
// 39 puts a byte of flags after a position claim's scope, and the one flag is
// flying. The server sees positions only, so a flight down to the ground was a
// fall from where the descent began: thirty nodes flown down cost 27 and the bar
// is 20, and a player exploring by air died every time they came down. A 38
// server reads the byte as one past the end and refuses every claim.
//
// 40 puts a third byte on each block's body: how full a cell of it is, in
// eighths, so water running off a source is drawn at its own height with a
// surface sloping to its neighbours instead of as seven identical cubes. A 39
// client reads the byte as the next block's flags and refuses the welcome.
//
// 41 lets a mod tell one player something (core.notice, TODO.md OPEN-11):
// Notice, a sentence on the event channel, drawn where a refused edit's reason
// is. A 40 client has no such message and drops the connection over it.
//
// 42 lets a world have more than a pair (the portal mod's frames): Portals says
// which of up to kMaxOpenings openings exist in two bytes, and each carries the
// index it leads to. A 41 client reads the second mask byte as the first
// opening's centre and refuses the message.
//
// 43 puts a line of text a player may edit on a panel (ui::ElemKind::Field),
// and what they typed in UiClick after the id. A 42 client refuses the panel
// over a kind it has no drawing for; a 42 server reads the text as bytes past
// the end and refuses the click.
//
// 44 says how long a body that drives is (core.body_width's second number), a
// float after the width in the welcome and in BodyMovement: the chassis a kart
// is simulated as (core/physics/vehicle.h). A 43 peer finds four bytes past
// what it knows and refuses both.
//
// 45 says how high above where it last stood a body may be (core.body_movement's
// rise), a float after the length in the welcome and in BodyMovement. A 44 peer
// finds four bytes past what it knows and refuses both, and one that read
// around them would bounce a kart off a ceiling its server does not check.
//
// 46 lets the server drive a kart with the client's keys (TODO.md OPEN-47): a
// claim carries what its chassis did since a few claims back, its velocity, the
// numbers it moved by and the server's last placement of it; a correction and a
// teleport carry which placement they are, and a correction of a body that
// drives its velocity and heading; BodyMovement says which numbers it is. A 45
// peer reads each of them as bytes past the end and refuses it.
//
// 47 puts the hotbar slot a hit was struck with after its target, as an Edit
// carries the slot it dug with, so a sword hits harder than a fist (ItemDef::
// damage, the server's table). A 46 server reads the byte as one past the end
// and refuses every hit.
//
// 48 lets everybody see what a player holds: Hold tells the server which
// square of the hotbar is the hand whenever it changes, and a Player's Entity
// `item` is what is in it (a Drop's is still its stack). A 47 client refuses
// a player that carries an item; a 47 server has no Hold and drops the
// connection over it.
//
// 49 lets everybody see somebody else dig or strike: Hold carries whether the
// left button is down on something, and each Entity a byte saying so. A 48
// peer finds a byte past what it knows in both and refuses them.
//
// 50 says how high a body's eye is (core.eye_height), a float after the rise
// in the welcome and in BodyMovement: the camera, and what reach is measured
// from on both ends. A 49 peer finds four bytes past what it knows and refuses
// both.
//
// 51 lets a body crouch (Ctrl on foot): Hold's working byte and each Entity's
// become flags, 1 working and 2 crouching, and the server measures a crouched
// body's reach from its lowered eye. A 50 peer refuses a 2 in either byte.
//
// 54 knocks a body back: a message type (Push) a 53 client does not know, and a
// client drops a server that sends it one.
//
// 55 says a creature burns: 4 in each Entity's flags, which a 54 peer refuses.
//
// 56 lets one server hold several worlds, its ROOMS: a Hello names the room it
// is for, and a mouth or a redirect names the room it leads to. A 55 client's
// Hello is too short and is refused as malformed; a 55 client refuses the
// longer Portals and Redirect.
//
// 58 wears armour: every item in the welcome carries where it is worn, its
// armour and its durability; every slot of a bag or a chest how much of that
// durability is used; and a Player's Entity may carry 8 in its flags and the
// four things it wears after them. A 57 peer reads all three short.
//
// 60 grows Hit by a byte: whether it is a use (a right click, taming a pet)
// rather than a blow. A 59 client's hits are a byte short and refused.
//
// 63 adds Ride (a seat: this body is carried on that one), which a 62 client
// does not know and disconnects on.
//
// 64 adds Chat, both ways: a line a player typed, and a line for the chat on
// their screen. A 63 peer does not know it and disconnects on it.
//
// 65 gives every shape a track piece (PieceShape, 8 bytes and its fill id) and
// a drive record's missing chunks four bytes instead of one. A 64 peer reads
// both short.
//
// 66 lets a Player's entity say it sits in a kart (flag 16) and how: the
// chassis's pitch and roll and where the bottom of its model is from the feet,
// five floats. A 65 client refuses the flag.
//
// 67 gives a track piece one more byte, whether it is a wall ride
// (PieceShape::wall). A 66 peer reads every shape after it a byte out.
//
// 68 one more, how many squares wide it is (PieceShape::width).
//
// 69 gives an Edit a byte, the quarter turns a track piece is put down with on
// top of the one it would face the player with (Edit::turn). A 68 server
// refuses the Edit as too long.
//
// 70 gives a drive record's kind byte a fourth bit, shift held (the nitro,
// DriveKeys::boost), and lets a body that drives go kNitroTop times its top
// speed. A 69 server refuses the bit.
//
// 71 lets a mouth come out of any of the far world's 16 openings.
//
// 72 sends a world's mouths as a count and that many, up to kMaxMouths,
// instead of always two: a 71 client reads the count as a host name.
//
// 73 lets a Drop be a block falling (bit 32 of its flags): a 72 client refuses
// the bit.
//
// 74 lets a panel hold an item's picture (ui::ElemKind::Item): a 73 client
// refuses the kind.
//
// 75 lets a player lie down (core.player_lie): Lie tells their own client, and
// bit 64 of an Entity's flags tells everybody else, whose yaw is then the way
// from the head to the feet. A 74 client refuses both.
//
// 76 gives a player a fifth worn square, the back (a flight pack): a dressed
// Entity carries five item ids and their five tints (and, bit 128, the tint
// of what a player holds), and a slot of a bag, a
// chest or a craft grid its tag (ItemStack::tag), which a 75 client would read
// as the next field.
//
// 77 lets a mod play a sound at a place (core.play_sound): Sound, which a 76
// client does not know.
//
// 78 says which blocks are conveyor belts (core.node_conveyor): bit 16 of a
// block's body flags, and then its speed, height and rise, which a 77 client
// refuses as a flag it does not know.
//
// 79 says what each square of an open container is for (core.container_roles):
// a byte a slot after the slots of a Container, which a 78 client reads as the
// end of the message and refuses.
//
// 80 says how far what is going on in an open container is (core.container_
// progress): two floats after the roles, which a 79 client refuses.
//
// 81 lets a creature hold something (`item` on a Mob, which an 80 client
// refuses) and play a clip its mod named (core.mob_act): flag 2 on a Mob, then
// the name as a byte of length and its bytes.
//
// 82 says how fast this body digs against everybody else (core.player_factors):
// a float after the generation in BodyMovement, which an 81 client refuses.
//
// 83 gives a Correct the server's nitro gauge (Correct::nitro), a float after
// the yaw, which an 82 client reads as the reason's length and refuses.
//
// 84 gives a Push a byte saying it SETS the body's speed rather than adds to
// it (core.player_launch, an launch plate), which an 83 client refuses;
// and every block's description in the welcome the light it gives, a byte
// after its fill, so a client relighting round a change counts a mod's lamps.
//
// 85 lets a world have eight mouths (kMaxMouths), one per themed world round
// the sandbox's plaza; an 84 client refuses a Portals with more than four.
//
// 86 lets the welcome's model table hold 1024 (kMaxModelTable), which an 85
// client refuses past 256: the sandbox was at 225 with mods/cards' 41.
//
// 87 says whether bodies do parkour (core.parkour), a byte after the eye in
// the welcome and in BodyMovement, which an 86 client refuses.
//
// 88 lets a mod's visible thing play a clip its mod named (core.ecs.clip):
// flag 2 on a Thing, as on a Mob since 81, which an 87 client refuses.
//
// 90 lets a world have sixteen mouths and 32 openings (a gate and a way out
// per themed world round the plaza), Portals' mask in four bytes, which an 89
// client reads as two; and Seal, a message an 89 client does not know.
// 92 puts flag 4 on a Player, which a 91 client refuses (only a creature burns):
// a clip the mod named follows (core.player_clip), as a creature's flag 2 does.
// 93 puts the player's livery at the end of the Hello, which a 92 server reads
// as trailing bytes, and flag 32 on a Player, which a 92 client refuses (only a
// drop falls): the livery follows, four bytes; and Dress, a message a 92
// server does not know.
// 94 widens the livery by four bits, a species and its style, which a 93
// server refuses as past kLiveryMax.
// 95 puts the arm squares an item opens (ItemDef::arms) after its durability in
// the welcome, which a 94 client reads as the next item's name.
// 96 puts what the two arms hold after a dressed Player's worn colours, which a
// 95 client reads as the next field.
// 97 puts a byte after every Player's flags, bit 0 flying, which a 96 client
// reads as the next field.
// 98 lets a Thing carry flag 16 and its roll, which a 97 client refuses (only a
// person drives); 100 puts a machine's hull after that roll, which a 99 client
// reads as the next field. 101 sets bit 1 of the movement rules' parkour byte
// (no ledges), which a 100 client reads as parkour. 102 sets bit 1 of a
// Player's second flags byte when a mod dresses them (core.player_wear): a
// count and, per piece, its slot, model and tint, which a 101 client refuses.
// 103 sets bits 2 and 3 of that byte for a Player in a kart: its nitro burning
// and its tyres sliding (DriveLook), which a 102 client refuses.
// 104 lets a panel hold a model turning in a box (ui::ElemKind::Model,
// core.ui_model), which a 103 client refuses.
// 105 gives a drive record's kind byte a fifth bit, ctrl held (the drift,
// DriveKeys::drift), which a 104 server refuses.
// 106 lets a container's square be a "source" or a "trash" one (roles 4 and 5,
// kRoleSource and kRoleTrash), which a 105 client refuses, and ends a
// container's state with where its scrollbar is (core.container_scroll) and
// its search box (core.container_search).
// 107 lets a world have twenty mouths (kMaxMouths): the sandbox's sixteen were
// all taken when the assault (games/assault) came; a 106 client refuses a
// Portals with more than sixteen, and a Seal past the sixteenth bit.
// 108 lets a world's sky be a colour that keeps its sun, under clouds of that
// colour turning in whirls here and there (core.world_storm): a flag and three
// bytes after the sky's in the Welcome, which a 107 client reads as the item
// table's count.
inline constexpr uint16_t kMinProtocol = 109;

// What a player picked to look like (protocol 93): the three colours of their
// model's tint_1..3 materials -- primary, secondary, accent -- five bits each,
// 0 for the model's own or 1..16 a dye, in the order an ItemStack's tag holds
// them above kTagIdBits (tag_colour); then which extra they wear on the head
// and which at the neck, three bits each, 0 for none. The client names the
// extras (kLiveryHeads, kLiveryNecks); the server only passes the number on.
// Then (94) two bits for who they are -- 0 the world's player model, else a
// species, the model "<player model>_<species>" (kLiverySpecies) -- and two
// for that species' style, 0 for none (kLiveryStyles).
inline constexpr uint32_t kLiveryColours = 0x7FFFu;
inline constexpr int      kLiveryHeadShift = 15;
inline constexpr int      kLiveryNeckShift = 18;
inline constexpr int      kLiverySpeciesShift = 21;
inline constexpr int      kLiveryStyleShift   = 23;
inline constexpr uint32_t kLiveryMax = 1u << 25;   // anything at or above it is refused

// 57 lets a world's sky be one colour with no sun (core.world_sky): a flag and
// three bytes after the room in the Welcome, which a 56 client refuses.
//
// The longest room name.
inline constexpr size_t kMaxRoomName = 64;

// How many openings a world may have. The first two are the pair every scene
// had; the first kMaxMouths may be mouths onto other worlds (--portal-to); the
// rest are local, each leading to the one it is linked with
// (core.portal_link). A number for the wire and for the arrays behind it, not
// a promise about frame rate: the client draws the nearest few in sight.
inline constexpr int kMaxOpenings = 32;

// How many of them may lead to other worlds. Each is a window the client keeps
// open and draws into a target of its own, which is what bounds it.
inline constexpr int kMaxMouths = 20;

enum class Channel : uint8_t {
    Handshake = 0,   // reliable
    Chunk     = 1,   // reliable, ordered, compressed
    Entity    = 2,   // unreliable, sequenced
    Event     = 3,   // reliable
    // A world's media, and nothing else. Its own channel because ENet orders
    // reliable messages WITHIN a channel: a 4 KB texture that lost a fragment
    // holds up everything queued behind it, and on the event channel that
    // included the heartbeat.
    //
    // Measured rather than reasoned about. Same link -- 400 ms round trip, 10%
    // loss -- twice: with the media already cached, forty seconds and a clean
    // run; with 127 KB of them to fetch, the session died after 5.1 seconds
    // saying "the server stopped answering for 5 seconds". The world was
    // answering. Its answers were behind a texture.
    Media     = 4,   // reliable
    // Every change to a block, from a player's edit and from a mod's write, and
    // nothing else. The same argument the media channel above is made of, made
    // a second time by the same measurement.
    //
    // Mods write continuously -- mods/beacons moves a beacon every tick -- and
    // the moment those writes started reaching players (TODO CLOSED-17) the
    // event channel carried a reliable message every time a mod named a cell.
    // On a 400 ms link losing a fifth of them, each loss holds up everything
    // queued behind it, and behind them was the heartbeat: bad_link went from
    // green to four reds in five runs, logging thirteen lost heartbeats and a
    // 4.09 s gap.
    //
    // This channel alone did NOT fix that, and the first version of this
    // comment said it had -- written before the runs that would have checked.
    // Measured, in order, five to eight runs each:
    //
    //   every write, on Channel::Event                    4 red of 5
    //   + this channel                                    3 red of 5
    //   + only actual changes sent                        red
    //   + skipping sessions without the chunk             3 red of 8
    //   + what a session is owed waits while it streams   0 red of 8
    //
    // The channel is a third of an answer: it stops a lost block change from
    // holding the heartbeat behind it, and it does nothing about the rate. See
    // the drain in src/server/main.cpp for the other two thirds.
    //
    // BOTH kinds on ONE channel, and that is not an accident: two changes to
    // the same cell have to arrive in the order they happened, and ENet orders
    // within a channel and not across them. A mod's write on one channel and a
    // player's edit on another is a cell that ends up holding the older of the
    // two.
    Block     = 5,   // reliable, ordered
    ModBase   = 6,   // 6..kMaxChannels-1 handed out at handshake
};

inline constexpr uint8_t kModChannelBase = 6;
inline constexpr uint8_t kMaxChannels    = 32;   // our cap, well under ENet's 255

enum class MsgType : uint8_t {
    Invalid = 0,
    Hello   = 1,   // client -> server, first packet on channel 0
    Welcome = 2,   // server -> client, accepted
    Refuse  = 3,   // server -> client, and then the connection closes
    Chunk   = 4,   // server -> client, channel 1, one encoded chunk
    Move    = 5,   // client -> server, channel 2, where the player says it is
    Correct = 6,   // server -> client, channel 3, where it actually is
    Entities = 7,  // server -> client, channel 2, everybody else
    Teleport = 8,  // server -> client, channel 3, the server moved you
    PlacePortal = 9,  // client -> server, channel 3, put an opening on that wall
    Portals    = 10,  // server -> client, channel 3, where the openings are now
    ChangeScope = 11, // client -> server, channel 3, this session becomes a player, or stops
    ScopeDenied = 12, // server -> client, channel 3, and the session stays what it was
    TicketRequest = 13, // client -> server, channel 3, "sign me something for the far side"
    Ticket        = 14, // server -> client, channel 3, the signed thing, or why not
    Heartbeat     = 15, // server -> client, channel 3, still here, and this is what I am
    Redirect      = 16, // server -> client, channel 3, your session continues over there
    MediaOffer    = 17, // server -> client, channel 3, what this world's blocks look like
    MediaNeed     = 18, // client -> server, channel 3, the ones it does not already have
    MediaData     = 19, // server -> client, channel 3, one file, bytes and all
    MediaMissing  = 20, // server -> client, channel 3, the asked-for ones it does not hold
    Edit          = 21, // client -> server, channel 3, break that block or build on that face
    BlockChange   = 22, // server -> client, channel 3, this cell is that block now
    Inventory     = 23, // server -> client, channel 3, what you are carrying
    MoveItem      = 24, // client -> server, channel 3, put that stack in that slot
    DropItem      = 25, // client -> server, channel 3, take that stack out of the bag
    PanelShow     = 26, // server -> client, channel 3, this is the panel that is up
    UiClick       = 27, // client -> server, channel 3, that button on that panel was pressed
    Container     = 28, // server -> client, channel 3, what is in the chest you have open
    CloseContainer = 29, // client -> server, channel 3, I am done with it

    // The grid this player crafts in, and what it currently makes.
    //
    // There is no "open" flag on it, and that is the design rather than an
    // omission: the SIZE is the state. Every player always has a grid -- two by
    // two, the one in the corner of their own bag -- and standing at a crafting
    // table makes it three by three. So `w` is what the client draws, and a
    // client that has been told 3 knows a table is open without a second field
    // that could disagree with the first.
    Craft = 30,          // server -> client, channel 3

    // "Put that stack where it obviously goes", which is a shift-click.
    //
    // Its own message and not a MoveItem, because a MoveItem names two squares
    // and this one names none: the player pointed at a stack and not at a
    // destination, and the destination is a RULE -- out of a chest goes to the
    // bag, out of the bag goes to the chest that is open. A MoveItem with a
    // made-up `to` would be a field that means nothing, which is a field that
    // can lie.
    QuickMove = 31,      // client -> server, channel 3

    // How much of this player is left.
    //
    // Whole and not a delta, the same argument InventoryState makes: it changes
    // when something hurts or heals a body, which is an event and not a stream,
    // and a delta scheme is a second source of truth that can drift from the
    // first.
    //
    // It carries no reason. What hurt somebody is a sentence for a log or a
    // notice, not a field every client has to have an opinion about -- and a
    // reason that only some damage sets is a field that is wrong by default.
    Vitals = 32,         // server -> client, channel 3
    Time   = 33,         // server -> client, channel 3

    // "I struck that."
    //
    // It names the target and the hotbar slot it was struck with, and nothing
    // else: no damage, no point of impact. What that slot's item takes off is
    // the server's table (ItemDef::damage, ecs::kPunch for a fist) and a client
    // that could say it would be a client that could say twenty. The point of impact
    // would only be drawn, and a client can work out where it hit something it
    // was already aiming at.
    //
    // It does not say WHERE the striker was either, for the reason Edit does
    // not: the server has a position it accepted for that session, and reach is
    // measured against that. A claim in this message would be a second position
    // that could disagree with the first.
    Hit = 34,            // client -> server, channel 3

    // What one mod says to its own copy on the other side.
    //
    // Both ways, and on a channel of its own -- the one the handshake gave that
    // mod. The message carries no mod name: the CHANNEL is the name, which is
    // the whole point of handing them out. A name in the payload would be a
    // second answer to "whose is this" and the two could disagree.
    //
    // The engine never looks inside. A mod's bytes are a mod's business, and a
    // string is what the API hands it because a mod that wants structure has
    // Lua and can put it in the string.
    ModData = 35,        // both ways, channel 6 and up

    // "Somebody has that cell open."
    //
    // A chest with a lid has to look open to everybody in the room and not only
    // to whoever clicked it, so this is state of the CELL and not of the
    // session -- the same argument the facing byte makes.
    //
    // It is NOT stored with the cell, and that is the difference from the
    // facing byte: a chest is open while somebody is looking inside it, which
    // is true of a moment and not of a world. It is not in the chunk, not in
    // the world file, and a client that joins while a chest is open is told
    // nothing until it changes -- said in TODO OPEN-32 rather than hidden.
    //
    // One bit of payload in a message of its own rather than a field on
    // BlockChange, because nothing about the BLOCK changed: a client that
    // treated this as a block change would relight the cell and rebuild the
    // chunk mesh every time a lid moved.
    CellOpen = 36,       // server -> client, channel 3

    // How THIS body moves from now on, when a mod has given it numbers of its
    // own (TODO.md OPEN-47). Reliable: a body told nothing goes on moving by the
    // old numbers against a server checking the new ones.
    BodyMovement = 37,   // server -> client, channel 3

    // Chunks this session no longer holds (TODO.md OPEN-38): the server has
    // forgotten it sent them, so the client lets them go too. On the chunk
    // channel, so it is ordered against the chunks themselves -- a chunk let go
    // and sent again later arrives after the message that let it go.
    ChunksGone = 38,     // server -> client, channel 1

    // A sentence a mod wants one player to read (core.notice, TODO.md
    // OPEN-11): why the block they placed came out as something else.
    Notice = 39,         // server -> client, channel 3

    // "The hand is this square of the hotbar now, and it is (not) working":
    // sent when either changes, so the server can show everybody else what is
    // in it (Entities' `item`) and whether it swings (`working`).
    Hold = 40,           // client -> server, channel 3

    // "Something struck you this hard": a speed added to the body, which the
    // client's own physics then carries and spends like any other.
    Push = 41,           // server -> client, channel 3

    // "You are carried on that body now, and here is the seat" (core.ride,
    // TODO.md OPEN-47). The server puts this body on the other every tick and
    // ignores its claims; the client stops walking it and follows the seat.
    // Reliable when a seat is taken or left, and every tick after on the entity
    // channel, where the next one replaces a lost one. Host 0 lets go.
    Ride = 42,           // server -> client, channel 1 or 2

    // A line of chat (T in game). Client to server: what the player typed,
    // said or a /command. Server to client: one line for the chat on screen,
    // already written as it is to be read ("<name> hello"); the client adds
    // nothing, so a mod's answer and a player's line are the same message.
    Chat = 43,           // both ways, channel 3

    // "You are lying down, there, head that way" (core.player_lie), or "you
    // are up". The client draws its body lying and lowers its eye; walking
    // off gets it up, and the server says so when a mod does.
    Lie = 44,            // server -> client, channel 2

    // A sound by name at a place (core.play_sound). The client turns the name
    // into a file of its own, as it does a model's, and is silent without one.
    Sound = 45,          // server -> client, channel 2

    // "These mouths are shut to you" (core.portal_seal): bit m, mouth m. The
    // client draws a sealed mouth as a red membrane and does not walk into it;
    // the server sends no Redirect for it. Per player, so a way out opens for
    // whoever passed the trial behind it and stays shut for the rest.
    Seal = 46,           // server -> client, channel 3

    // "I look like this now" (protocol 93): the livery picked in a game's
    // pause menu, after the one the hello carried.
    Dress = 47,          // client -> server, channel 3

    // "Who is there?" before any hello: the list of servers asks, the server
    // answers with a Status and lets go. A server from before them refuses the
    // question as a malformed hello, which the list reads as "up, nothing more".
    StatusQuery = 48,    // client -> server, channel 0, instead of a Hello
    Status      = 49,    // server -> client, channel 0
};

// Players per world, decided in DESIGN.md, and the reason the entity message
// can be a flat list rather than a delta scheme: seven other players at 25
// bytes each is under 200 bytes a tick.
inline constexpr size_t kMaxEntities = 96;

// The bodies are sent in parts of at most this many (protocol 109): each part
// under one UDP datagram, so a lost packet costs the bodies in it for a tick
// and not every body -- 64 in one message were three fragments, and a world
// full of soldiers on a link that loses one in a hundred froze them all,
// players with them, now and then.
inline constexpr size_t kEntitiesPerPart = 24;
inline constexpr size_t kMaxEntityParts  = (kMaxEntities + kEntitiesPerPart - 1) / kEntitiesPerPart;

enum class RefuseReason : uint8_t {
    None            = 0,
    BadMagic        = 1,
    VersionTooOld   = 2,
    VersionTooNew   = 3,
    BadName         = 4,
    Malformed       = 5,
    ServerFull      = 6,
    MissingFeature  = 7,   // the server requires something the client cannot do
    ContentMismatch = 8,   // same protocol, different block table
    BadIdentity     = 9,   // the identity is malformed, unproven, or missing where required
    NotWatchable    = 10,  // this world does not let a portal look into it
    NoSuchOpening   = 11,  // there is no opening here with that number to come out of
    ArrivalBlocked  = 12,  // there is, and what is in front of it is solid
    BadTicket       = 13,  // no ticket, an expired one, or one this world will not take
    NoSuchRoom      = 14,  // this server holds no world by that name
    Banned          = 15,  // this player or address is banned here; the detail says until when
    NotAllowed      = 16,  // the server lets in only the players on its list
    Kicked          = 17,  // sent to a player already in, by an admin, before the cut
    Restarting      = 18,  // the server is going down to come straight back: try again
    Stopping        = 19,  // the server is going down
    Maintenance     = 20,  // only staff come in for now; the detail says why

    // Always the last real value. What a decoder compares against, so that
    // adding a reason does not silently turn it into "no reason" on every
    // client that was built before it.
    Last = Maintenance,
};

const char *refuse_reason_text(RefuseReason r);

// Whether trying the same thing again could ever produce a different answer.
//
// A refusal is not an outage. "This world does not allow observers" and "your
// block table is not mine" are decisions, and a client that reconnects every
// minute to be told the same thing is a client that has confused a policy with
// a network problem -- it costs the server a handshake a minute forever and
// puts the same line in both logs.
//
// The one that is NOT permanent is a full server, which is the only refusal
// here that is about a moment rather than about the two ends.
bool refusal_is_permanent(RefuseReason r);

// Feature names are strings, not bits in a mask. A bit has to be allocated by
// somebody who owns the protocol; a name can be minted by a mod, which is what
// DESIGN.md 6.5 needs. The cost is a few bytes once per connection.
inline constexpr size_t kMaxFeatures    = 64;
inline constexpr size_t kMaxFeatureName = 64;
inline constexpr size_t kMaxNameLength  = 32;
inline constexpr size_t kMaxVersionString = 32;

// The block table in the welcome. 4096 nodes of 96 characters is the ceiling on
// what one welcome can make a client hold: about 390 KB, stated here rather
// than discovered. A server with more blocks than that cannot be joined, which
// is a limit worth having written down before somebody's mod pack finds it.
inline constexpr size_t kMaxNodeTable    = 4096;
inline constexpr size_t kMaxItemTable    = 4096;

// And the model table, which is a much shorter list: these are characters and
// creatures somebody modelled, not blocks a generator can mint by the thousand
// -- or cards: mods/cards draws every card of a deck as a model of its own.
// 1024 of them at 96 characters is 96 KB of welcome (protocol 86; 256 before).
inline constexpr size_t kMaxModelTable   = 1024;

// Slots in the inventory a server describes. Larger than the 36 anybody has
// today so a mod can widen a bag without a protocol version, and small enough
// that the message stays a message: 256 slots is 1 KB on the wire.
inline constexpr size_t kMaxInventorySlots = 256;

// What a panel may carry on the wire. These are the ui:: numbers, and
// protocol.cpp static_asserts that they still are: a wire cap smaller than the
// model's would truncate a panel silently, which is a mod author looking for
// the missing button in their own arithmetic.
inline constexpr size_t kMaxWirePanelElements = 128;
inline constexpr size_t kMaxWirePanelText     = 128;

// What one ModData carries. A mod's message is a string and the engine never
// reads it, so the only thing this number decides is how much a peer can make
// the other side hold before anybody has looked at it.
inline constexpr size_t kMaxWireModText       = 512;
inline constexpr size_t kMaxNodeName     = 96;

// How many blocks in one welcome may carry a shape, and how many boxes one of
// them may be made of.
//
// Both are far above what content does -- a game of the original engine's nodeboxes are a few
// hundred blocks of a handful of boxes each -- and both are here because this is
// a list a stranger's server sends: 1024 x 16 boxes is at most 1024 * 16 * 25
// bytes, about 400 KB, the same ceiling the name table already has.
inline constexpr size_t kMaxNodeShapes   = 1024;
inline constexpr size_t kMaxNodeBoxes    = 16;

// And what a box's PART may be called. A name is what makes a box a piece that
// moves -- a lid -- and the engine never reads it, so this only decides how much
// a peer can make the other side hold.
inline constexpr size_t kMaxNodePartName = 32;

// A client that declares this can be handed the server's block table and will
// adopt it, ids and all. Without it, a server whose content differs has nothing
// to do but refuse: the client would read every chunk through its own ids and
// draw a world out of the wrong blocks.
inline constexpr const char *kFeatureContentAdoption = "core:content-adoption";

// What a session is FOR. DESIGN.md 4.4.
//
// A player walking around is Enter. The other one is what a portal opens
// towards somebody else's world: a session that exists to be looked through,
// with no body in it and nothing it can touch. Keeping them apart is not
// politeness -- an observer with an avatar is a player standing in a world they
// never entered, and an observer that can dig is a way to edit a world without
// being in it.
enum class Scope : uint8_t {
    Enter   = 0,   // an ordinary player
    Observe = 1,   // chunks and nothing else
};

const char *scope_text(Scope s);


// --------------------------------------------------------------- tickets
//
// DESIGN.md 4.4's other door. `allow_observers` is the first one -- a world that
// says anybody may look in -- and it needs no infrastructure, which is why it
// came first. This is for the worlds that do not admit anybody: server A signs
// a statement about one player, and B admits that player because it trusts A.
//
// What the signature covers, and why each field is in it:
//
//   issuer    who signed. The key B looks up in its own trusted list.
//   subject   WHICH PLAYER. Without it the ticket is a bearer token: anybody
//             who copied it off the wire could spend it. With it, spending it
//             also takes the secret half of that key, which is proven
//             separately against the session's channel binding.
//   audience  WHICH SERVER. Without it a ticket A issues for B is also good at
//             C, and every server A trusts becomes a server that trusts A's
//             players by accident. A learns B's key from configuration; B
//             compares against its own, so a NAT or a second host name cannot
//             break the check the way a host:port audience would.
//   portal    which opening. The crossing names one, and the ticket has to
//             agree, or one ticket is good for every hole in the world.
//   scopes    watching and entering are different permissions, and a world may
//             well grant one and not the other.
//   issued /
//   expires   a window. Not a revocation list: this is a portal, not a bank.
//
// What it is NOT: a session. A ticket says "this player may come in", and
// everything after that is the ordinary handshake. And it says nothing about
// the world on the far side -- B decides where somebody lands, as it always
// did.
inline constexpr uint8_t kTicketObserve = 1u << 0;
inline constexpr uint8_t kTicketEnter   = 1u << 1;
inline constexpr uint8_t kTicketScopes  = kTicketObserve | kTicketEnter;

// How long a ticket is good for, and how far apart two servers' clocks may be
// before one of them starts refusing the other's tickets.
//
// Both are declared here rather than tuned somewhere: an expiry is a shared
// clock, and two machines that disagree by more than the skew allowance cannot
// portal to each other. That is a real limit of doing it this way and it is
// better written down than discovered.
//
// The allowance applies to BOTH ends of the ticket's window. It did not always:
// a ticket dated in the future got kTicketSkew of rope and an expired one got
// none, so a far server whose clock ran fast reported "the ticket has expired"
// about a ticket signed a second earlier. Two clocks, one rule.
//
// What that buys, exactly, with d = (checking clock - issuing clock): a ticket
// presented as soon as it is signed is honoured for -60s <= d < 360s. The upper
// figure is generous only because it is mostly the LIFETIME doing the work, and
// it shrinks as the ticket ages -- so kTicketSkew is the number to reason with,
// not the 360.
inline constexpr uint64_t kTicketLifetime = 300;   // seconds
inline constexpr uint64_t kTicketSkew     = 60;    // each end of the window

struct PortalTicket {
    uint8_t  issuer[kSignPublicBytes]{};
    uint8_t  subject[kSignPublicBytes]{};
    uint8_t  audience[kSignPublicBytes]{};
    uint32_t portal_id = 0;
    uint8_t  scopes    = 0;
    uint64_t issued    = 0;   // seconds since the epoch
    uint64_t expires   = 0;
    uint8_t  signature[kSignatureBytes]{};

    std::vector<uint8_t> encode() const;
    static bool decode(ByteReader &r, PortalTicket &out);
};

// Exactly the bytes the signature covers, with a label in front so a ticket can
// never be read as some other thing this protocol signs. The signature itself
// is not in here, which is the whole point of it being a separate function.
std::vector<uint8_t> ticket_challenge(const PortalTicket &t);

// Whether the signature on this ticket is the issuer's. Says nothing about
// whether the ticket should be honoured -- trust, audience, expiry and scope
// are policy, and policy lives with the server that has one.
bool ticket_signed_by_issuer(const PortalTicket &t);

// A client asking the world it is standing in for a ticket to the world its
// portal leads to. It names what it wants to do over there; the server decides
// what it is willing to sign for.
struct TicketRequest {
    uint8_t scopes = kTicketObserve;

    // Which of this world's openings the ticket is for. Each one may lead to a
    // different server, and a ticket names its audience.
    uint8_t opening = 0;

    std::vector<uint8_t> encode() const;
    static bool decode(ByteReader &r, TicketRequest &out);
};

// And the answer, granted or not. One message for both, because a client that
// asked has to stop waiting either way -- the same reason ScopeDenied exists.
struct Ticket {
    bool         granted = false;
    PortalTicket ticket;
    RefuseReason reason = RefuseReason::None;   // when it is not granted
    std::string  detail;

    std::vector<uint8_t> encode() const;
    static bool decode(ByteReader &r, Ticket &out);
};

struct Hello {
    uint32_t magic        = kMagic;
    uint16_t protocol     = kProtocol;      // newest the sender speaks
    uint16_t min_protocol = kMinProtocol;   // oldest it still speaks

    // What this build calls itself, and what its block table actually is. The
    // first is for the log line a person reads; the second is what the
    // handshake decides on. See core/version.h for why they are two fields and
    // not one.
    std::string engine_version;
    uint64_t    content_hash = 0;

    std::string              player_name;
    std::vector<std::string> features;      // what the client can do

    // Who this player IS, as opposed to what they are called. DESIGN.md 4.4:
    // the identity is a public key and the name is only a label -- two people
    // may pick the same name, and neither of them can pick the other's key.
    //
    // Both empty for an anonymous join, which a server may or may not allow.
    // When present, `identity` is 32 bytes and `proof` is a 64-byte signature
    // over the session's channel binding and the name. What makes that worth
    // sending is what it is NOT: a signature over the name alone would be a
    // password that works forever, on every server, for anybody who copied it.
    std::vector<uint8_t> identity;   // 0 or kSignPublicBytes
    std::vector<uint8_t> proof;      // 0 or kSignatureBytes

    // What this session is for. A client that says nothing is a player, which
    // is what every client before this field was.
    Scope scope = Scope::Enter;

    // The ticket, when one is being presented. Read for a session that is
    // opening a window -- a world may allow anybody to look in, or only the
    // players some other server vouches for.
    //
    // Always CHECKED when present, whether or not this world requires one, for
    // the same reason an identity is: a server that ignores a ticket it was
    // handed is a server whose logs say something it did not verify.
    bool         has_ticket = false;
    PortalTicket ticket;

    // Which of the server's worlds this session is for (protocol 56). Empty is
    // the first, which is every server that holds one.
    std::string room;

    // What this player looks like (protocol 93, kLiveryColours): 0 is the
    // model as it was made.
    uint32_t livery = 0;

    std::vector<uint8_t> encode() const;
    static bool decode(ByteReader &r, Hello &out);
};

// What a player signs, and what the server checks it against: the binding, a
// label, and the name being claimed.
//
// The name is in there so that a proof cannot be lifted out of one hello and
// put in another with a different name on the same session -- which is a thing
// a server could otherwise do to its own players.
std::vector<uint8_t> identity_challenge(const uint8_t binding[32], const std::string &name);

// ----------------------------------------------------------- the heartbeat
//
// A server saying it is still there, and what it currently is.
//
// The "still there" half is not something ENet already answers. ENet pings the
// peer and drops it when the socket stops working -- but a server whose main
// loop has wedged, or whose world thread is stuck, keeps answering those pings
// perfectly while streaming nothing. To the client that is indistinguishable
// from a quiet world, which is exactly the state a portal onto somebody else's
// server must not sit in forever.
//
// The "what I am" half is what makes it worth its bytes rather than a ping. A
// mouth shows a world the player may want to walk into, and whether that is
// possible right now -- a free seat, a world still willing to be looked into --
// is not something the client can work out from chunks. Without it the first
// news of a full world arrives as a refused crossing, with the player already
// standing in the opening.
inline constexpr double kHeartbeatInterval = 1.0;   // how often a server sends one

// How long a client waits before deciding the world it is watching has stopped.
// Five of them, so a lost packet and a slow tick are not an outage: the message
// is reliable, but a server under load can be late.
inline constexpr double kHeartbeatSilence = 5.0;

// And the same window as a multiple of the round trip, for a link where five
// seconds is not five intervals.
//
// A heartbeat is unsequenced, so what has to fit inside the window is a run of
// consecutive losses, not a retransmission chain. At 400 ms this is a ten
// second window -- ten beats -- and losing ten in a row needs a link that has
// stopped rather than one that is dropping packets. On loopback the constant
// above wins and nothing changes.
inline constexpr double kHeartbeatSilenceRtts = 25.0;

// The join, as a multiple of the round trip. A join is ENet's connect, a key
// exchange, and a hello and welcome -- four reliable exchanges before a world
// arrives, each of which can need retransmitting on a link that drops packets.
// At 400 ms this is a sixteen second window; on loopback the floor in
// ClientConfig wins and nothing changes.
inline constexpr double kHandshakeRtts = 40.0;

inline constexpr uint8_t kWorldWatchable = 1u << 0;   // a portal may look in
inline constexpr uint8_t kWorldEnterable = 1u << 1;   // and somebody may walk in right now

struct Heartbeat {
    uint32_t seq = 0;   // grows by one; a gap means messages were lost, not that time passed

    uint16_t players       = 0;
    uint16_t max_players   = 0;
    uint16_t observers     = 0;
    uint16_t max_observers = 0;

    uint8_t flags = 0;   // kWorldWatchable | kWorldEnterable

    bool watchable() const { return (flags & kWorldWatchable) != 0; }
    bool enterable() const { return (flags & kWorldEnterable) != 0; }

    std::vector<uint8_t> encode() const;
    static bool decode(ByteReader &r, Heartbeat &out);
};

struct ChannelAssignment {
    std::string name;   // "yaportal:link"
    uint8_t     id = 0;
};

// One item, as the wire carries it.
//
// Deliberately not `cnp::ItemDef`, on the same argument as PortalDesc: that is a
// registry struct and the day somebody adds a field to it the wire format would
// change underneath every client already shipped. Two types and one conversion.
//
// `places` is a BlockId in THIS server's table, which the client has adopted, so
// a client can draw a stack of stone as stone without a second media path. Zero
// means the item puts nothing down, which is what a tool is.
struct ItemDesc {
    std::string name;
    uint16_t    places    = 0;
    uint16_t    max_stack = 64;

    // How it digs (ItemDef), since protocol 36: what a client times the bar
    // under the crosshair with. Without them a pick in the hand filled it at a
    // bare hand's pace, and the server, which knew better, waited for it.
    uint8_t     dig_level = 0;
    float       dig_speed = 1.0f;
    std::string digs;

    // Where it is worn and what it gives there, and how long it lasts
    // (ItemDef, protocol 58): what the armour bar and a slot's wear bar need.
    uint8_t     wear       = 0;
    uint8_t     armor      = 0;
    uint16_t    durability = 0;
    uint8_t     arms       = 0;   // arm squares it opens on the back (protocol 95)
};

// How a block digs (NodeDef), by id beside the block table, since protocol 36.
// The client's own numbers are the engine's blocks at best, and a block from a
// mod this build never heard of came out at once on its screen and not on the
// server's.
struct NodeDigDesc {
    float       hardness  = 0.0f;
    uint8_t     dig_level = 0;
    std::string material;
};

// What a block does to a body and to light, as it travels (protocol 38). Two
// bytes a block: whether it stops a body, whether it stops light, whether a body
// swims in it, and how much shows through it.
struct NodeBodyDesc {
    bool    solid   = true;
    bool    opaque  = true;
    uint8_t opacity = 255;
    bool    liquid  = false;
    uint8_t fill    = 8;   // eighths of a cell, for a liquid (protocol 40)
    uint8_t light   = 0;   // the light it gives, 0..15 (protocol 84)
    bool    climbable = false;   // a ladder (protocol 53)
    float   conveyor        = 0.0f;   // a belt, m/s, and its top (protocol 78)
    float   conveyor_height = 1.0f;
    float   conveyor_rise   = 0.0f;
};

// A block that is not the cell it lives in, as it travels.
//
// Sent only for the blocks that have something to say -- a shape, or a turn --
// which in a world of thousands of blocks is a handful. The rest are cubes that
// do not turn, and saying so for each of them would be a table of zeroes.
//
// The coordinates are the floats the server holds and not a fixed point: the
// collision skin is a ten-thousandth of a node, so a quantisation of a
// hundredth would put the two ends of a wire a hundred times further apart than
// the tolerance they judge a position by.
struct NodeShapeDesc {
    struct Box {
        float       min[3] = {0.0f, 0.0f, 0.0f};
        float       max[3] = {1.0f, 1.0f, 1.0f};
        std::string part;   // empty for a box that is simply part of the shape
    };

    uint16_t         node  = 0;   // its id in this welcome's block table
    uint8_t          turn  = 0;   // NodeTurn: what the cell's facing byte means
    uint16_t         model = 0;   // its id in this welcome's model table, 0 for none
    std::vector<Box> boxes;
    PieceShape       piece;       // plan 0 for a block that is not a track piece
};

struct Welcome {
    uint32_t magic    = kMagic;
    uint16_t protocol = kProtocol;   // the version both sides will use

    std::string engine_version;      // the server's, for the client's log
    uint64_t    content_hash = 0;    // and its block table, for the client's check

    uint32_t seed     = 0;           // so the client generates the same world
    // A hash of the world *as generated*, before a mod or a player touched it.
    // Not of the live world: mods write blocks during load, players write them
    // afterwards, and a client cannot reproduce either from a seed. Advertising
    // the live world instead is a check that fails on a correct client, which
    // is worse than no check -- and it is exactly what this field held until a
    // two-process run reported a desync that did not exist.
    //
    // Known limit, stated rather than discovered later: the hash is over block
    // *ids*, and ids are assigned by registration order inside one process. Two
    // sides that generate identical worlds but registered their nodes in a
    // different order hash differently. Today both sides register the same core
    // nodes in the same order and it holds; it is not something the protocol
    // enforces. The fix is a name-to-id table in the welcome, and it belongs
    // with chunk streaming, which needs that table anyway to read a chunk.
    uint64_t mapgen_hash = 0;
    std::vector<std::string>       features;   // the agreed set, server's answer
    std::vector<ChannelAssignment> channels;

    // Every node name in id order -- the ids are the positions. Sent only when
    // both sides agreed on content adoption; empty otherwise, and then the
    // handshake has already refused a client whose table differs.
    std::vector<std::string> nodes;

    // And what those of them that are not plain cubes look like, by id. Under
    // the same agreement as the names: a client that does not adopt the table
    // has already been refused.
    //
    // The SERVER's shape wins, including over a name this client knows. Two
    // copies of one mod that disagree about a box are a client standing where
    // the server says it cannot, and the world you are in is the one that
    // decides what is in it.
    std::vector<NodeShapeDesc> shapes;

    // The key this world signs tickets with, when it has one.
    //
    // Sent so a client can tell WHICH server it is talking to, independently of
    // the address it dialled. That sounds like a thing nobody needs until two
    // servers are on two machines behind NAT: measured in containers, server B
    // told a client "the world through this opening is at 172.31.8.1:25891"
    // while that same client already had a session to that very server, opened
    // as 127.0.0.1:25891. It opened a second one -- to a machine it was already
    // talking to -- and that one was refused for want of a ticket.
    //
    // An address is a route and not a name. On loopback the two are the same
    // string and the difference cannot be seen at all.
    uint8_t signing_key[kSignPublicBytes]{};
    bool    has_signing_key = false;

    // Which room of the server this is (protocol 56), by its own name: a
    // Hello that named none is in the first, and the client learns which, so a
    // mouth back into it is recognised as the world it already has.
    std::string room;

    // A sky of one colour and no sun (core.world_sky, protocol 57).
    bool    has_sky = false;
    uint8_t sky[3]{};

    // A sky tinted this colour by day, sun and all, its clouds the same colour
    // and turning in whirls (core.world_storm, protocol 108).
    bool    has_storm = false;
    uint8_t storm[3]{};

    // This world's item table, in id order, the way `nodes` is its block table.
    //
    // A second table and not a flag on `nodes` for the same reason the two
    // registries are separate: a pick has no block, and a client that only knew
    // block names could not name what is in the third slot of its own bag.
    //
    // Sent under the same agreement as the block table -- content adoption -- so
    // a client that refuses to adopt gets neither and has already been refused.
    std::vector<ItemDesc> items;

    // And how every block in `nodes` digs, by the same ids.
    std::vector<NodeDigDesc> node_digs;
    std::vector<NodeBodyDesc> node_bodies;   // by id, like node_digs (protocol 38)

    // And this world's model table, in id order, the way `nodes` is its block
    // table -- a body's `model` is a position in here plus one, so that 0 can
    // mean "nothing said" without costing the first model its id.
    //
    // The names only. What the model is MADE of -- a .gltf, its buffer, its
    // textures -- travels the way every other file does, through the media
    // path, and a client that has not got it yet draws the box it draws today.
    std::vector<std::string> models;

    // How a body moves in this world (TODO.md OPEN-44), sent to every client and
    // not only to one that adopts the tables: a client that moved by its own
    // numbers in a world with others is a client the server corrects every
    // tick, whatever else it agreed to. The client moves by these, over
    // anything its own mods said, for the reason shapes win: the server is the
    // side that checks.
    //
    // Refused on the way in when it is not something this engine can run -- a
    // NaN, a negative speed, a speed over kMaxDeclaredSpeed, gravity upwards --
    // because it is a message a stranger can send and a client moves a body by
    // it before anything asks.
    MovementRules movement;

    // What a player's body wears here: an id into `models` plus one, 0 for the
    // box. Other bodies carry theirs in the entity message; this one is the
    // body the client itself moves, which the server never sends back to it,
    // and a camera behind the player has to draw it (core.view).
    uint16_t player_model = 0;

    std::vector<uint8_t> encode() const;
    static bool decode(ByteReader &r, Welcome &out);
};

// One thing a driving body's chassis did (core/physics/vehicle.h), with which
// of that body's numbers it was done by (BodyMovement::gen).
struct DriveRecord {
    DriveEvent event;
    uint8_t    gen = 0;
};

// Where a player says they are. Says, not is: this is a claim by a client, and
// until M3's movement authority lands the server takes it at face value for one
// purpose only -- deciding which chunks to send. A lie costs the liar chunks
// they cannot see; it does not move anything in the world.
//
// Unreliable and sequenced on the wire (channel 2), because the newest position
// makes every older one worthless, and `seq` is what lets the server drop one
// that arrives late rather than teleporting the interest centre backwards.
struct Move {
    uint32_t seq = 0;
    float    x = 0.0f, y = 0.0f, z = 0.0f;
    float    look_x = 0.0f, look_y = 0.0f, look_z = -1.0f;

    // What the sender believed this session was when it made the claim.
    //
    // The same message means two things. From a body it is "I am here", and the
    // server checks it against the world. From a window it is "keep chunks
    // around here", and there is nothing to check because there is nothing
    // there. A crossing turns one into the other, and on a link with a round
    // trip in it a claim sent as a window arrives after the server has made it a
    // body -- so the centre of a portal, which is what a window claims, gets
    // checked as a position and lands inside a solid block. Measured at 400 ms
    // and 20% loss: fifteen corrections and a player falling to y = -1607.
    //
    // The sequence number cannot catch it: the stale claim is the NEWEST one.
    // So the claim carries what it is, and the server applies it only if the
    // session still is that.
    uint8_t  scope = (uint8_t)Scope::Enter;

    // What the body was doing when it got here: kMoveFlying, or nothing. Only
    // the fall watch reads it. A body that says it is flying starts its fall
    // again where it is instead of being charged the descent -- and only on a
    // world where that body may fly, so saying it where flying is off buys
    // nothing a body could not already get by flying down slowly.
    uint8_t  flags = 0;

    // Which of the server's placements of this body the claim comes after
    // (Correct::epoch, Teleport::epoch). A claim made before the client had
    // been put somewhere describes where it no longer is, and the server drops
    // it instead of refusing it again: that second refusal, and the one after,
    // is how one correction used to become a run of them.
    uint32_t epoch = 0;

    // Which of this body's numbers it moves by (BodyMovement::gen), and how
    // fast it is going. The velocity is what a server that lost the thread of
    // a chassis puts it down at again.
    uint8_t  gen = 0;
    float    vx = 0.0f, vy = 0.0f, vz = 0.0f;

    // What a body that drives did since a few claims back (TODO.md OPEN-47):
    // its chassis put down, and every fixed step's keys, numbered from
    // `first_drive`, for the server to do again. A few claims back and not
    // since the last one, because this channel drops packets and one lost
    // claim would otherwise be a gap the server cannot drive through.
    uint32_t first_drive = 0;
    std::vector<DriveRecord> drive;

    std::vector<uint8_t> encode() const;
    static bool decode(ByteReader &r, Move &out);
};

inline constexpr uint8_t kMoveFlying = 1;

// The most records one claim carries: a second at 60 steps is more than three
// claims' worth, and a claim of them is still one packet.
inline constexpr size_t kMaxDriveRecords = 64;

// The server disagreeing about where a player is. Reliable, on the event
// channel, because a correction that is dropped leaves the two sides disagreeing
// permanently -- the client would go on predicting from a position the server
// has already rejected.
struct Correct {
    uint32_t seq = 0;    // the claim being answered, so a client can match it
    uint32_t epoch = 0;  // which placement of this body this is (Move::epoch)
    float    x = 0.0f, y = 0.0f, z = 0.0f;

    // For a body that drives (kCorrectDrive): the velocity and heading its
    // chassis is put down at afresh, as the server's own copy of it is. Without
    // the flag the body stops where it is put and keeps its aim.
    uint8_t  flags = 0;
    float    vx = 0.0f, vy = 0.0f, vz = 0.0f;
    float    yaw = 0.0f;
    float    nitro = -1.0f;   // the server's gauge for a body that drives, 0..1; -1 keeps the client's

    std::string reason;  // for the log, and eventually for the player

    std::vector<uint8_t> encode() const;
    static bool decode(ByteReader &r, Correct &out);
};

inline constexpr uint8_t kCorrectDrive = 1;

// The server moved this player, and everything that move changed, in one
// message.
//
// Not a Correct with extra fields. A correction says "you were wrong"; this says
// "I moved you", and the two want different things on the client: a correction
// keeps the player's aim, and a portal crossing replaces it. PORTAL_SYSTEM 4.1
// is the reason they arrive together rather than as three messages -- sending
// position, view and velocity apart leaves a frame where the player is in the
// new place with the old orientation, which is a full turn of the world drawn
// once and then corrected.
//
// The roll is here for the same reason it exists on the client: crossing
// between openings that are not parallel tips the horizon, and the client
// animates it back to level. Sent rather than recomputed because the client
// does not know the destination opening.
struct Teleport {
    uint32_t seq = 0;    // the claim this answers, so a client can match it
    uint32_t epoch = 0;  // which placement of this body this is (Move::epoch)

    float x = 0.0f, y = 0.0f, z = 0.0f;                // feet
    float look_x = 0.0f, look_y = 0.0f, look_z = -1.0f; // unit, transformed
    float vx = 0.0f, vy = 0.0f, vz = 0.0f;             // absolute, not a delta
    float roll = 0.0f;                                 // radians, animated out

    std::string reason;  // for the log, and eventually for the player

    std::vector<uint8_t> encode() const;
    static bool decode(ByteReader &r, Teleport &out);
};

// A player asking for an opening to be put on the wall they are looking at.
//
// The client sends what its raycast hit -- a solid node and which of its six
// faces -- and nothing else. It does not send the opening: where the hole ends
// up, how big it is and which way it faces are all derived on the server from
// that node and that face, because a client that sent a finished opening could
// send one hanging in the air a hundred blocks away, and the server would have
// to rebuild it from the node anyway to check that it did not.
//
// Reliable, on the event channel. A dropped placement is a portal the player
// asked for and did not get, with nothing on screen to say why, and asking
// again is what a player does when the answer is "nothing happened".
struct PlacePortal {
    uint8_t which = 0;              // 0 = the first opening, 1 = the second

    int32_t x = 0, y = 0, z = 0;    // the solid node the ray hit
    int8_t  nx = 0, ny = 0, nz = 0; // the face of it, as a unit normal

    std::vector<uint8_t> encode() const;
    static bool decode(ByteReader &r, PlacePortal &out);
};

// One opening, as the wire carries it.
//
// Deliberately not `cnp::Portal`. That is a gameplay struct with methods, and
// the day somebody adds a field to it the wire format would change underneath
// every client that had already been shipped. Two types and one conversion is
// the cost of being able to change either one on purpose.
struct PortalDesc {
    float   cx = 0.0f, cy = 0.0f, cz = 0.0f;   // the centre of the opening
    uint8_t axis        = 0;                   // PortalAxis
    int8_t  normal_sign = 1;                   // which side is the front
    uint8_t rot         = 0;                   // quarter turns of the in-plane basis
    float   half_w = 0.5f, half_h = 1.0f;
    uint8_t kind = 0;                          // PortalKind
    uint8_t colour = 0;                        // closed: 0 green (fixed), 1 blue, 2 orange
};

// Where the openings are, as the server holds them. Sent to everybody whenever
// they change, and once to a player who joins a world that already has some.
//
// A bitmask rather than a count, because the openings are not
// interchangeable. A player who has placed one end and is walking off to place
// the other has exactly one opening, and WHICH one it is decides which wall it
// is drawn on -- a lone second opening sent as a list of one would arrive as
// the first and be drawn in the wrong place.
// Whether one opening description is one anything may read. See the definition.
bool portal_desc_sane(const PortalDesc &d);

// Where one opening LEADS, when it does not lead back into this world.
//
// Per opening since protocol 34. Until then a world had one destination and
// the second opening of a mouth was unused, so a sandbox could lead to an
// arena or to a track and never to both.
//
// The geometry at the far end is NOT here, and that is deliberate. This server
// does not know where the other server's opening is, and a copy of it here
// would be a copy that goes stale the moment somebody over there moves it. The
// client asks that world directly: a passive session gets its Portals message
// like anybody else.
struct PortalAway {
    std::string host;
    uint16_t    port = 0;

    // WHICH opening over there this mouth comes out of: any of its sixteen
    // since protocol 71 (one a mod opened is a way out and not a way in). The
    // far server has several and this side has to name one, or every mouth in existence arrives out of
    // the far world's first opening -- which is not a rule, it is what happens
    // when nobody sends a number.
    //
    // Here rather than in the crossing message because it is a property of the
    // link, decided by whoever configured this end, and not a thing the player
    // walking through gets to choose. The client copies it into the ChangeScope
    // it sends; the far server checks it against the openings it actually has.
    uint16_t portal = 0;

    // WHO is over there, as opposed to how to get there. The far server's
    // signing key -- what this server was configured with as the audience of
    // the tickets it issues, so it already had it.
    //
    // The client matches this against the key each of its sessions announced in
    // its Welcome. Without it the only name a client has for a server is the
    // address it was handed, and two routes to one machine read as two
    // machines. See Welcome::signing_key.
    uint8_t key[kSignPublicBytes]{};
    bool    has_key = false;

    // Which of the far server's worlds (protocol 56). With no host and no port
    // it is a room of the server this link came from: the client reaches it at
    // the address it already reached that server by.
    std::string room;

    bool leads_away() const { return (!host.empty() && port != 0) || !room.empty(); }
};

struct Portals {
    uint32_t   version = 0;   // bumped on every change, so a client can skip a stale one
    uint32_t   present = 0;   // bit i: opening i exists
    PortalDesc opening[kMaxOpenings];

    // Which opening each one leads to, or -1 for none: an opening whose partner
    // is not there is drawn as nothing and crossed by nobody. Symmetric: the
    // server links two at a time (core.portal_link). The first two lead to each
    // other until something says otherwise, which is every scene this engine
    // had before a world could hold more than a pair.
    int8_t     link[kMaxOpenings];

    // Where each of the first openings leads. All empty means the pair is
    // local -- the two openings lead to each other, which is every scene this
    // engine had until M5. Mouth k may lead away only when every one before it
    // does: a world is a pair or a set of mouths, never half of each, because a
    // local crossing needs both ends here.
    PortalAway away[kMaxMouths];

    Portals()
    {
        for (int i = 0; i < kMaxOpenings; ++i) link[i] = (int8_t)(i ^ 1);
    }

    bool has(int i) const { return i >= 0 && i < kMaxOpenings && (present & (1u << i)) != 0; }
    bool leads_away() const { return away[0].leads_away(); }
    int  mouths() const
    {
        int n = 0;
        while (n < kMaxMouths && away[n].leads_away()) ++n;
        return n;
    }

    // The opening i leads to here, or -1: none, or not there.
    int partner(int i) const
    {
        if (!has(i)) return -1;
        const int j = link[i];
        return has(j) ? j : -1;
    }

    // Whether there is something a player can walk through and something to
    // show inside it. A mouth needs only its own opening on this side; what is
    // behind it comes from the other server.
    bool complete() const
    {
        if (leads_away()) {
            for (int k = 0; k < mouths(); ++k)
                if (!has(k)) return false;
            return true;
        }
        for (int i = 0; i < kMaxOpenings; ++i)
            if (partner(i) >= 0) return true;
        return false;
    }

    std::vector<uint8_t> encode() const;
    static bool decode(ByteReader &r, Portals &out);
};

// A session changing what it is for, without reconnecting.
//
// This is what a portal to another server costs on the wire, and the reason it
// is a scope change rather than a second join: the passive session behind the
// opening has ALREADY been streamed that world. A fresh connection would arrive
// with an empty interest set and be sent every chunk again -- which is a loading
// screen, and DESIGN.md's test for M6 is a crossing without one.
//
// Becoming a player carries where the player is coming from: the opening they
// walked into, in the world they are leaving, and their pose in that world.
// This server does not know that opening -- it is somebody else's -- so it is
// sent rather than looked up, and then it is CHECKED: the pose has to be inside
// the bounds of the opening it claims to be in, and the opening itself has to
// be a plausible size. That bounds the lie. Whatever a modified client sends,
// the arrival comes out of portal_traverse against THIS server's own opening,
// so it lands within one opening's width of where the hole is -- not anywhere
// in the world.
//
// Giving it up entirely -- letting the client name its arrival position -- would
// be no worse than a fresh join, which also takes the first claim as given. It
// would also be no better, and this costs twenty bytes.
struct ChangeScope {
    uint8_t scope = 0;      // the Scope this session wants to be

    // Read only when becoming a player. Ignored, and not required, for a
    // session that is stepping back to watching.

    // Which of this world's openings to come out of. Copied from the
    // PortalAway::portal of the mouth that was walked into, so the world
    // being left decides it and the player does not.
    //
    // Not a licence to arrive anywhere: it selects one of the openings this
    // server has put there, and an opening it has not is refused. The widest a
    // wrong number gets anybody is the other end of a portal the server itself
    // installed.
    uint32_t portal_id = 0;

    // And the ticket for coming in, when this world wants one. Separate from
    // the one in the Hello because they are different permissions: a world may
    // be happy to be looked into by anybody and picky about who walks out of
    // its openings.
    bool         has_ticket = false;
    PortalTicket ticket;

    PortalDesc from;                                    // the opening left behind
    float      x = 0.0f, y = 0.0f, z = 0.0f;            // feet, in that world
    float      look_x = 0.0f, look_y = 0.0f, look_z = -1.0f;
    float      vx = 0.0f, vy = 0.0f, vz = 0.0f;

    std::vector<uint8_t> encode() const;
    static bool decode(ByteReader &r, ChangeScope &out);
};

// "Your session continues over there."
//
// DESIGN.md M6's redirect fallback, and the reason it is a fallback rather than
// the way it is done: the seamless crossing needs a session that is ALREADY
// looking into the far world, and there are perfectly ordinary reasons for that
// session not to exist -- it was refused, it is between attempts after an
// outage, or this client does not open passive sessions at all. Without a
// fallback, a mouth in any of those states is a hole a player walks into and
// nothing happens.
//
// Decided by the server, like every other teleport here (DESIGN.md 3.4): it
// owns the opening and it is the one that sees the player step into it. What
// the client does with it depends on what the client has -- a live window onto
// that world means the seamless path is available and this is ignored.
//
// It carries a ticket when the issuing world can make one, because a redirect
// to a world that requires one is otherwise a redirect to a refusal.
struct Redirect {
    std::string host;
    uint16_t    port      = 0;
    uint32_t    portal_id = 0;   // which opening over there, as the link names it
    std::string room;            // and which world; no host: this same server (56)

    bool         has_ticket = false;
    PortalTicket ticket;

    std::string reason;   // for the player to read, and for the log

    std::vector<uint8_t> encode() const;
    static bool decode(ByteReader &r, Redirect &out);
};

// ------------------------------------------------------------------ media
//
// DESIGN.md 4.5, in three messages: the server says what its blocks look like,
// the client says which of those it does not already have, and the server sends
// only those.
//
// The order matters and it is the opposite of what the design sentence reads
// like. "Il client manda la lista di hash che ha gia" taken literally means a
// client announcing its entire cache -- every texture from every world it has
// ever looked into -- to a server that has no use for all but a handful. So the
// offer comes first and the client answers within it. The delta is the same
// delta; what changes is that its size is bounded by the world being joined
// rather than by the player's history.
//
// Media are offered by FILE NAME, not by block id. Ids are per session
// (DESIGN.md 4.5 again) and a texture outlives the session that carried it: the
// name is what the next world's block will ask for when it turns out to be the
// same stone.

struct MediaEntry {
    std::string name;   // "core_stone.png"
    ContentHash hash;
    uint32_t    size = 0;   // what the bytes will be, so a client can budget
};

struct MediaOffer {
    std::vector<MediaEntry> items;

    std::vector<uint8_t> encode() const;
    static bool decode(ByteReader &r, MediaOffer &out);
};

struct MediaNeed {
    std::vector<ContentHash> hashes;

    std::vector<uint8_t> encode() const;
    static bool decode(ByteReader &r, MediaNeed &out);
};

// The other answer to a MediaNeed: these, this world does not hold.
//
// It exists because its absence was a hang with a stopwatch bolted on. A server
// used to say nothing at all about a hash it did not have, so a client could
// not tell "not coming" from "slow" -- and the only tool it had was a timeout,
// which on a link that drops packets fires on files that were merely slow. When
// it fired the client asked for the whole batch again while the first copy was
// still queued: measured at 400 ms and 25% loss, 55 files fetched for 31
// distinct.
//
// With an answer for every hash, the timeout stops being how the client learns
// anything and goes back to being what it should always have been: a backstop
// for a link that has stopped.
struct MediaMissing {
    std::vector<ContentHash> hashes;

    std::vector<uint8_t> encode() const;
    static bool decode(ByteReader &r, MediaMissing &out);
};

struct MediaData {
    ContentHash          hash;
    std::vector<uint8_t> bytes;

    std::vector<uint8_t> encode() const;
    static bool decode(ByteReader &r, MediaData &out);
};

// How many files one MediaNeed may ask for, and therefore how many MediaData a
// single request can cost the server. A world offers at most kMaxMediaItems, so
// a client that needs all of them asks several times; a client that asks for
// more than the world has ever offered is refused the excess rather than
// served it.
inline constexpr size_t kMediaPerRequest = 64;

// How long a client waits with NOTHING arriving from a batch it asked for,
// before deciding those files are not coming and moving on to the rest.
//
// Not how long the batch may take. That was the first version and it was the
// same mistake as every other constant in this chapter: a world's content set
// travels reliably over whatever link there is, and how long it takes is the
// link's business. Fifteen seconds for a whole batch fails at 20% loss -- the
// client gave up in the middle and asked again, and both copies arrived: 36
// files fetched for 31 distinct. Fifteen seconds of complete silence, with the
// clock restarted by every file that lands, means a stall and nothing else.
inline constexpr double kMediaSilence = 15.0;

// The answer to a ChangeScope that was refused.
//
// It exists because its absence was a defect with a stopwatch in it: a refused
// crossing used to produce nothing at all, and the client discovered it by
// waiting two seconds and giving up. Two seconds of a player standing in an
// opening that is not taking them, with no reason on screen, is the same
// failure as a connection that hangs instead of being refused -- and the fix
// is the same one, which is to say so.
//
// Only sent for a refusal. A granted change replies with what it produced: a
// Teleport for somebody arriving, and nothing for somebody stepping back to
// watching, because there is nothing about that to draw.
struct ScopeDenied {
    uint8_t      scope  = 0;                    // what was asked for
    RefuseReason reason = RefuseReason::None;   // and why not
    std::string  detail;                        // in words, for the player to read

    std::vector<uint8_t> encode() const;
    static bool decode(ByteReader &r, ScopeDenied &out);
};

// Everybody else, as far as this player is concerned. Unreliable and sequenced:
// a dropped update is replaced by the next one 50 ms later, and asking for it
// again would deliver it after the answer.
// What a body in the entity list is.
//
// A field and not something read off the others. It used to be read off `item`:
// a stack on the ground was "the one with an item id", and a mob was "a player
// that happens to be smaller" -- which the client had no way to see at all, so
// it drew mobs as people and let players swing at each other by mistake.
enum class EntityKind : uint8_t {
    Player = 0,   // somebody's body, with a session behind it
    Mob    = 1,   // a creature, with none
    Drop   = 2,   // a stack lying on the ground
    Thing  = 3,   // something a mod made visible (TODO.md OPEN-20)
};

struct EntityState {
    uint64_t   id   = 0;
    float      x = 0.0f, y = 0.0f, z = 0.0f;
    float      yaw = 0.0f, pitch = 0.0f;
    EntityKind kind = EntityKind::Player;

    // Which stack, when this is a Drop, so the client can draw the thing rather
    // than a person-shaped box. On a Player, what is in their hand (protocol
    // 48), kNoItem for an empty one. kNoItem on anything else.
    //
    // `kind` says WHAT it is and this says WHICH -- two questions and not one
    // asked twice. A decoder that finds an item on something that is not a drop
    // refuses the message rather than picking whichever field it likes better.
    //
    // Two bytes on every body rather than a message of its own, because a drop
    // is streamed by exactly the query that streams a player -- see
    // core/ecs/drop.h -- and giving it its own message would be writing the
    // interest and distance rules a second time to say the same thing.
    uint16_t item = 0;

    // What this body LOOKS like: an index into the welcome's model table, 1
    // based, and 0 for "nothing said" -- which is the box every body is drawn
    // as today and what a build with no models sends for everything.
    //
    // An id and not a name, for the reason `item` is one: sixty-four bodies
    // twenty times a second cannot each carry a string, and the table that
    // turns the id into a name is sent once, in the welcome, beside the block
    // table and the item table.
    //
    // A Drop may not carry one. A stack on the ground is drawn as the thing it
    // is -- that is what `item` is for -- and a drop wearing a model would be
    // two answers to one question, which is the rule `kind` and `item` already
    // live by.
    uint16_t model = 0;

    // A person swinging at something -- digging, striking -- as their client
    // last said (protocol 49), and crouched (51). Only a Player may be either.
    bool working   = false;
    bool crouching = false;

    // A worker creature's clip, named by its mod (protocol 81, core.mob_act):
    // played instead of the walk while it is not empty. A Mob's or a Thing's,
    // and since 92 a Player's (core.player_clip), at most kMaxClipName bytes.
    std::string clip;

    // A creature in the sun, losing a point a second (protocol 55). Only a Mob.
    bool burning = false;

    // A block falling (protocol 73), on a Drop only: `item` is then the BLOCK's
    // id and it is drawn as the whole block, not as a stack.
    bool falling = false;

    // A Player lying on their back (protocol 75), the middle of their body at
    // x, y + kLieLift, z and `yaw` the way their head points.
    bool lying = false;

    // A Player flying (protocol 97), in a byte of a Player's own after the
    // flags: drawn hovering and leaning, not walking.
    bool flying = false;

    // A Player in a kart (protocol 103, bits 2 and 3 of that byte): its
    // nitro burning, flames out of its exhausts, and its tyres sliding, which
    // leave marks on the road (DriveLook).
    bool boosting = false;
    bool sliding  = false;

    // A creature this client's player carries (protocol 91, core.mob_unseen),
    // on a Mob only, flag 64 as a Player's lying: their own client draws it in
    // their hands, and sent rather than left out so the client does not take
    // it for dead and play its death where it was last seen.
    bool held = false;

    // What a Player wears, head to feet and then the back, by item id
    // (protocol 58, the back 76): drawn as the model of the same name as the
    // item, on their skeleton.
    uint16_t worn[5] = {0, 0, 0, 0, 0};
    // And each one's three colours, tag >> kTagIdBits (protocol 76).
    uint16_t worn_tint[5] = {0, 0, 0, 0, 0};
    // And what the back's arms hold, left and right (Inventory::kArms,
    // protocol 96): drawn in the claws of the pack's model.
    uint16_t arm[2] = {0, 0};
    // And what a Player holds, its colours the same way (protocol 76, flag 128).
    uint16_t item_tint = 0;
    // And the livery they picked (protocol 93, flag 32 on a Player).
    uint32_t livery = 0;
    // And what a mod dresses them in (protocol 102, core.player_wear): a model
    // id and its tint per slot, 0 for none.
    uint16_t dress[10]      = {};
    uint16_t dress_tint[10] = {};

    // A Thing a mod rolled (protocol 98, core.craft): `chassis` and `body_roll`
    // alone, right side down; its pitch is `pitch`.
    // A Player in a kart (protocol 66): how the chassis sits, nose up and
    // right side down, and where the bottom of its model is from x, y, z --
    // which are its feet, the highest point a wheel touches, and not a place
    // to stand a tipped kart on (Player::body_base).
    bool  chassis    = false;
    float body_pitch = 0.0f, body_roll = 0.0f;
    float base_dx = 0.0f, base_dy = 0.0f, base_dz = 0.0f;

    // And a rolled Thing's hull, when it is a machine (protocol 100): half its
    // width, height and length, and how far the hull's middle stands over x,
    // y, z, in sixteenths of a node on the wire. All 0 on what rides one. The
    // client stands bodies on it and keeps them out of it (World::movers).
    float hull_w = 0.0f, hull_h = 0.0f, hull_l = 0.0f, hull_ride = 0.0f;
};

inline constexpr size_t kMaxClipName = 32;

struct Entities {
    // Part `part` of `parts` of the bodies of frame `frame` (protocol 109): a
    // client takes a body out once every part of a frame came and it was in
    // none of them.
    uint16_t frame = 0;
    uint8_t  part  = 0;
    uint8_t  parts = 1;
    std::vector<EntityState> players;

    std::vector<uint8_t> encode() const;
    static bool decode(ByteReader &r, Entities &out);
};

struct Refuse {
    uint32_t     magic  = kMagic;
    RefuseReason reason = RefuseReason::None;
    std::string  detail;

    std::vector<uint8_t> encode() const;
    static bool decode(ByteReader &r, Refuse &out);
};

// What a player did with their hands.
//
// It carries the block the ray HIT and the face of it, never the cell that
// should change -- the same shape as PlacePortal and for the same reason: with
// a target cell on the wire there is a field in which a modified client can
// hang a block in mid-air, and the server would have to reconstruct the ray to
// find out. From a node and a face the server derives the cell itself, so there
// is nothing to reconstruct and nothing to disagree about.
//
// `slot` is which hotbar slot the hand was on, because the click and the slot
// have to be the same decision: a separate "select" message can arrive after
// the click that meant it, and then a player pays for a block they did not put
// down. Clamped rather than refused -- see Inventory::select.
struct Edit {
    int32_t x = 0, y = 0, z = 0;    // the node the ray hit
    int8_t  nx = 0, ny = 0, nz = 0; // its face, as a unit normal
    uint8_t breaking = 0;           // 1 = break that node, 0 = build against the face
    uint8_t slot     = 0;
    // Quarter turns (0..3) added to a TRACK PIECE's placement facing: the
    // player turns the piece in hand before putting it down. Nothing else is
    // turned by it, so a chest still faces whoever put it there.
    uint8_t turn     = 0;
    // Shift held: put the block in hand down even against a block that would
    // answer the click (a chest, a table) -- no core:used when the placement
    // goes through. Travels as bit 2 of the turn byte, which a server that
    // predates it masks off, so no protocol bump.
    bool    place_only = false;

    std::vector<uint8_t> encode() const;
    static bool decode(ByteReader &r, Edit &out);
};

// What a cell is now.
//
// Sent to everybody who can see it, and sent to the player who asked WHETHER OR
// NOT anything changed -- a refused edit is answered with the block that is
// still there, so there is no request without a reply and no client waiting on
// a timeout to find out it was told no. M6_FINDINGS 25, the same rule.
//
// `why` is filled only on the copy that goes back to whoever asked, and only
// when the answer is no. Bystanders get the cell and nothing else; the player
// who was refused gets the sentence, because a refusal nobody can read sends
// them to look in the wrong place -- M6_FINDINGS 31.
struct BlockChange {
    int32_t     x = 0, y = 0, z = 0;
    uint16_t    id = 0;

    // Which way the cell is turned, 0..3 quarter turns about the vertical.
    //
    // Here and not derivable from the id, since OPEN-27: a block with a front
    // is ONE block whose cells carry a byte, so a placement that did not send
    // this would put the block down facing whichever way the cell happened to
    // be turned already -- and stay wrong until the whole chunk was sent again,
    // which for the chunk a player is standing in may be never.
    uint8_t     facing = 0;

    std::string why;

    // Not on the wire: what the cell held before the receiving session wrote
    // `id` over it. Whoever relights after the change needs it -- lava turning
    // into lava of another level changes no light, and a Inferno seen through
    // an opening sends a hundred of those a second.
    uint16_t    had = 0;

    std::vector<uint8_t> encode() const;
    static bool decode(ByteReader &r, BlockChange &out);
};

// A cell somebody has open, or has just stopped having open.
//
// The cell and a flag, and nothing about what is at it: the block is whatever
// the client already has there, and a message that carried the block as well
// would be a second answer to "what is in this cell" that could disagree with
// the chunk.
struct CellOpen {
    int32_t x = 0, y = 0, z = 0;
    uint8_t open = 0;

    std::vector<uint8_t> encode() const;
    static bool decode(ByteReader &r, CellOpen &out);
};

// How many chunks one ChunksGone names, at most. A player carried across the
// world lets go of every chunk around them at once -- 9 by 9 by 5 is 405 -- so
// the sender splits, and a receiver refuses a count past this rather than
// reserving whatever a peer says.
inline constexpr size_t kMaxChunksGone = 1024;

struct ChunksGone {
    std::vector<ChunkPos> chunks;

    std::vector<uint8_t> encode() const;
    static bool decode(ByteReader &r, ChunksGone &out);
};

// What a session is carrying, in full.
//
// Whole and not a delta, because the whole thing is 36 slots of four bytes and a
// delta scheme is a second source of truth that can drift from the first. It is
// sent when it changes, which is a player action and not a stream.
//
// DESIGN.md 3.4: inventories are server-side. This message is the client being
// TOLD, never the client claiming -- there is no message in this protocol that
// puts an item in a bag.
struct InventoryState {
    struct Slot {
        uint16_t item  = 0;
        uint16_t count = 0;
        uint16_t used  = 0;   // of the item's durability (protocol 58)
        uint8_t  ench  = 0;   // its enchantments, two bits a kind (protocol 62)
        uint32_t tag   = 0;   // which one it is, and its colours (protocol 76)
    };

    uint8_t           selected = 0;
    std::vector<Slot> slots;

    std::vector<uint8_t> encode() const;
    static bool decode(ByteReader &r, InventoryState &out);
};

// A stack moved from one slot to another, in one's own bag.
//
// The only message in this protocol that changes an inventory, and it still is
// not the client CHANGING one: it is a request, the server does the arithmetic,
// and what comes back is the whole InventoryState. DESIGN.md 3.4 -- inventories
// are server-side -- means the client may say where it wants something, never
// what it now has.
//
// There is no "held in the cursor" state, and that is a design choice rather
// than a simplification: a stack half-way out of a bag is state the client
// would own and the server would not, and the first dropped packet is a player
// holding something the server has never heard of. A click selects a slot and
// the next click sends this.
// Out of the bag and onto the ground.
//
// It carries a slot and a count and nothing else -- no position and no
// direction. Where the stack lands is the server's arithmetic from where the
// player already is: a point on the wire is a field in which a modified client
// throws a stack across the map, and the server would have to check it against
// a reach it already knows. The same argument as Edit naming a block and a face
// instead of a cell.
//
// `count` of 0 means the whole stack, which is the only way to say "all of it"
// without the client having to know how many there are -- and it does not,
// because the bag it draws is one round trip old.
struct DropItem {
    uint16_t slot  = 0;
    uint16_t count = 0;

    std::vector<uint8_t> encode() const;
    static bool decode(ByteReader &r, DropItem &out);
};

// Which inventory a slot number is a slot of.
//
// A handle and not a place. `Container` means "the one I have open", which the
// server already knows, so a modified client cannot name a chest across the map
// and cannot name one it never opened -- the same argument as Edit carrying a
// block and a face instead of a cell, and DropItem carrying no position.
enum class Side : uint8_t {
    Bag       = 0,
    Container = 1,

    // The grid this player is crafting in. Like Container, it is "the one I
    // have open" and not a place: the server holds which cell and what size,
    // so there is no field here for a modified client to lie in.
    Craft     = 2,
};

// The one square of a craft grid that is not a square.
//
// Nothing can be put IN it, and taking from it is not a move -- it is "make
// that, and pay for it out of the grid". It rides on MoveItem rather than on a
// message of its own because it is the same gesture a player makes for every
// other move, and a second message would be a second thing the client's drag
// code has to decide between while the mouse is down.
//
// Out of the range of any INVENTORY on purpose, and not merely out of the range
// of a grid: an index that could also be a real slot is an index a bug can
// silently mean, and 64 is a perfectly good slot in a bag of 256.
inline constexpr uint16_t kCraftResultSlot = (uint16_t)kMaxInventorySlots;

// What a player's craft grid holds, and what it makes.
//
// The result travels with the grid rather than being worked out again on the
// client: the recipe table is the server's (DESIGN.md 3.4 puts inventories
// there), and a client computing its own answer would be a second table that
// has to agree -- the first time it did not, a player would see a result they
// could not take.
struct CraftState {
    uint8_t w = 2, h = 2;

    // Where the table is, when one is open. All zero and w == 2 for the grid
    // every player carries in their own bag.
    int32_t x = 0, y = 0, z = 0;

    std::vector<InventoryState::Slot> slots;   // w * h of them, in row order
    InventoryState::Slot              result;  // empty when the grid makes nothing

    std::vector<uint8_t> encode() const;
    static bool decode(ByteReader &r, CraftState &out);
};

struct MoveItem {
    uint8_t  from_side = (uint8_t)Side::Bag;
    uint8_t  to_side   = (uint8_t)Side::Bag;
    uint16_t from  = 0;
    uint16_t to    = 0;
    uint16_t count = 0;   // 0 means the whole stack

    std::vector<uint8_t> encode() const;
    static bool decode(ByteReader &r, MoveItem &out);
};

// How much of a body is left, and how much it could hold.
//
// `max` travels with it rather than being a constant both ends agree on: a bar
// drawn against a maximum the client made up is a bar that is wrong the day a
// mod changes one.
struct Vitals {
    uint16_t health = 0;
    uint16_t max    = 0;

    std::vector<uint8_t> encode() const;
    static bool decode(ByteReader &r, Vitals &out);
};

// How one body moves, once a mod has given it numbers of its own (TODO.md
// OPEN-47). The welcome says how bodies move in the world; this says how THIS
// one does from now on, and may arrive at any time.
//
// Refused on the way in on the same terms as the welcome's movement: it is the
// same five numbers, the client moves a body by them, and a stranger can send
// them.
// A seat (core.ride): which body this one is carried on, by entity id as
// Entities names it, and where the seat is in the world now -- worked out by
// the server, which knows which way the host faces. Host 0: carried on
// nothing, walking again from where it is.
// Lying down (core.player_lie): where the middle of the body is -- x and z,
// and y the surface it lies on -- and the way its head points, as a heading
// (Player::yaw's). Off: standing up again wherever the body is.
struct Sound {
    std::string name;   // "mymod:bell"; sound_name_ok
    float x = 0.0f, y = 0.0f, z = 0.0f;

    std::vector<uint8_t> encode() const;
    static bool decode(ByteReader &r, Sound &out);
};

// A sound name: "mod:thing", lower case, digits and '_' only, at most 64. It
// becomes a file name on the client, so nothing in it can climb a directory.
bool sound_name_ok(const std::string &name);

struct Lie {
    bool  on = false;
    float x = 0.0f, y = 0.0f, z = 0.0f;
    float head = 0.0f;

    std::vector<uint8_t> encode() const;
    static bool decode(ByteReader &r, Lie &out);
};

// How far above what it lies on the middle of a lying body is: half the
// robot's depth, so its back is on the mattress.
inline constexpr float kLieLift = 0.22f;

struct Ride {
    uint64_t host = 0;
    float    x = 0.0f, y = 0.0f, z = 0.0f;
    // And the seat in the host's frame (x right, y up, z front; protocol 99):
    // a client puts the body on the host as it DRAWS it, so the two move as
    // one instead of the seat arriving a tick behind the machine.
    float    seat_x = 0.0f, seat_y = 0.0f, seat_z = 0.0f;

    static constexpr float kMax = 8.0f;   // a seat's offset from the host's feet, on any axis

    std::vector<uint8_t> encode() const;
    static bool decode(ByteReader &r, Ride &out);
};

struct BodyMovement {
    MovementRules rules;

    // Which numbers these are for this body, counting from the welcome's 0: a
    // claim says which it moved by, and the server replays it by those.
    uint8_t gen = 0;

    // How much faster than its tool this body digs (core.player_factors):
    // both ends time a dig by it.
    float dig = 1.0f;

    std::vector<uint8_t> encode() const;
    static bool decode(ByteReader &r, BodyMovement &out);
};

// What time it is in the world, as a fraction of a day.
//
// Sixteen bits and not a float: the range is known, [0, 1), so a float would
// spend most of its bits on exponents of numbers that cannot occur. One code is
// a fiftieth of a second of a twenty-minute day, which is finer than anything
// an eye follows across a sky.
//
// The LENGTH of the day does not travel. It is the server's, and a client that
// was told it would be free to disagree about how fast the sun moves between
// two updates -- which is exactly the drift the updates exist to remove.
// A blow's knock: m/s added to the body that took it (TODO.md, creatures).
// A speed and not a place, so the client's physics moves the body and the
// server checks the result like any other movement; a body that flies faster
// than this on its own is within every budget it has.
struct Push {
    float x = 0.0f, y = 0.0f, z = 0.0f;
    bool  set = false;   // the body's speed becomes this (a launch), not this more

    static constexpr float kMax = 16.0f;   // on any axis; more is refused

    std::vector<uint8_t> encode() const;
    static bool decode(ByteReader &r, Push &out);
};

struct Seal {
    uint32_t mask = 0;

    std::vector<uint8_t> encode() const;
    static bool decode(ByteReader &r, Seal &out);
};

struct Time {
    uint16_t tod  = 0;   // clock_to_wire of a time of day
    uint32_t wind = 0;   // how far the wind has carried the clouds, in hundredths of a node

    std::vector<uint8_t> encode() const;
    static bool decode(ByteReader &r, Time &out);
};

// One stack, to wherever it goes.
//
// It carries the square it came from and nothing else. Where it lands is the
// server's to decide -- see net::quick_target -- and "wherever it fits" is not
// a slot number the client could send even if it wanted to.
//
// `slot` of kCraftResultSlot with `side` Craft is the answer square, and means
// "make that until the grid or the bag runs out". The same sentinel MoveItem
// uses for taking one, for the same reason: the result is not a square any bag
// has, so no real square can be mistaken for it.
struct QuickMove {
    uint8_t  side = (uint8_t)Side::Bag;
    uint16_t slot = 0;

    std::vector<uint8_t> encode() const;
    static bool decode(ByteReader &r, QuickMove &out);
};

// What is in the container this session has open, and where it is.
//
// Whole and not a delta, the same argument InventoryState makes. Sent when it
// changes, which is somebody's hand and not a stream -- but "somebody" is now
// plural: two players with the same chest open both get this when either one
// moves a stack, which is the reason a container is one Inventory on the server
// and not a copy per viewer.
//
// `open` false means the chest is closed and the rest is empty. A message and
// not an absence, for the reason PanelShow says it: a client cannot tell "it
// closed" from "nothing arrived this tick".
// I am done with the chest.
//
// It carries nothing at all -- not even which one. A session has one container
// open and the server holds which; a cell in this message would be a cell to
// check against the one already known, which is a check that can only ever
// disagree with itself.
//
// It exists because there was no way to shut a chest. A mod could close one and
// nothing a player did could: pressing E hid the panel on this side while the
// server went on holding the chest open, and the next message re-opened it.
// The thing that was struck, by entity id -- the same ids Entities carries, so
// a client aims with the list it is already being sent.
struct Hit {
    uint64_t target = 0;
    uint8_t  slot   = 0;   // which square of the hotbar is the hand
    bool     use    = false;   // a right click: used on it, not struck (ecs::use_pet)

    std::vector<uint8_t> encode() const;
    static bool decode(ByteReader &r, Hit &out);
};

struct CloseContainer {
    std::vector<uint8_t> encode() const;
    static bool decode(ByteReader &r, CloseContainer &out);
};

struct ContainerState {
    bool     open = false;
    int32_t  x = 0, y = 0, z = 0;
    std::vector<InventoryState::Slot> slots;
    std::vector<uint8_t> roles;   // one a slot, SlotRole (protocol 79)
    float progress = -1.0f;       // how far what is going on in it is, 0..1; -1 nothing (80)
    float rate     = 0.0f;        // and how fast it goes on, per second
    float scroll      = 0.0f;     // a catalogue's rows on show: where they start, 0..1 (106)
    float scroll_size = 0.0f;     // and how much of the list they are; 0 no scrollbar
    bool        search = false;   // a search box beside it, holding `query` (106)
    std::string query;

    std::vector<uint8_t> encode() const;
    static bool decode(ByteReader &r, ContainerState &out);
};

// The panel a player should have on screen, whole.
//
// The reason this message exists is the reason the panel model is RETAINED and
// not immediate mode: mods run on the server and panels are drawn on a client,
// so the description has to cross a socket. Immediate mode would cross it sixty
// times a second. This crosses it when the panel CHANGES, which is a mod
// action, not a stream -- the same argument InventoryState makes.
//
// Whole and not a delta, for the same reason InventoryState is: a panel is a
// handful of elements and a delta scheme is a second source of truth that can
// drift from the first.
//
// `open` false means NOTHING is up, and the rest of the message is then empty.
// It is a message and not an absence because a client cannot tell "the panel
// closed" from "no message arrived this tick" -- and a panel that stays on a
// screen after the mod closed it is a player looking at a button that no longer
// does anything.
//
// The element type is spelled out here rather than reusing ui::Element, the
// same way InventoryState::Slot does not reuse ItemStack. Two reasons, both
// load-bearing: what is on the wire has to be bounded and validated field by
// field, and a wire format that is a mirror of an in-memory struct is one
// refactor away from silently changing what old clients receive.
struct PanelShow {
    struct Elem {
        uint8_t     kind = 0;   // ui::ElemKind
        std::string id;
        std::string text;
        float       x = 0.0f, y = 0.0f, w = 0.0f, h = 0.0f;
        uint8_t     r = 0, g = 0, b = 0;
    };

    bool        open = false;
    std::string name;
    std::string owner;
    float       w = 0.0f, h = 0.0f;
    std::vector<Elem> elements;

    std::vector<uint8_t> encode() const;
    static bool decode(ByteReader &r, PanelShow &out);
};

// A button on a panel was pressed.
//
// It names the panel and the button by the ids the mod itself chose, and
// carries no coordinates: where the button was on this player's screen is the
// client's arithmetic from a screen size the server does not know, and a point
// on the wire would be a field in which a modified client presses a button that
// is not there. The server checks the pair against the panel it actually has
// up, which is the whole of the validation and the reason the ids travel
// instead of an index -- an index into a panel the client has and the server
// has replaced is a click on the wrong button, silently.
// A mod's own message, on a mod's own channel. See MsgType::ModData.
struct ModData {
    std::string text;

    std::vector<uint8_t> encode() const;
    static bool decode(ByteReader &r, ModData &out);
};

// Capped like ModData: it is a line on a screen, and a server that could make a
// client hold anything it likes has found a way to spend its memory.
struct Hold {
    uint8_t slot    = 0;       // a square of the hotbar, 0 .. Inventory::kHotbar - 1
    bool    working   = false;   // the left button down on something: a dig, a hit
    bool    crouching = false;   // Ctrl held on foot (protocol 51)

    std::vector<uint8_t> encode() const;
    static bool decode(ByteReader &r, Hold &out);
};

struct StatusQuery {
    std::vector<uint8_t> encode() const;
};

struct Status {
    std::string version;        // kEngineVersion
    uint16_t    players = 0;    // playing, in every room
    uint16_t    rooms   = 1;
    std::string motd;           // the message of the day, "" for none
    std::string maintenance;    // "" when open to everybody

    std::vector<uint8_t> encode() const;
    static bool decode(ByteReader &r, Status &out);
};

struct Notice {
    std::string text;

    std::vector<uint8_t> encode() const;
    static bool decode(ByteReader &r, Notice &out);
};

// The longest line a player may type. A server line may be longer -- it carries
// the name in front -- and is capped by kMaxWireModText like a notice.
inline constexpr size_t kMaxChatLine = 256;

struct Dress {
    uint32_t livery = 0;   // Hello::livery, under kLiveryMax

    std::vector<uint8_t> encode() const;
    static bool decode(ByteReader &r, Dress &out);
};

struct Chat {
    std::string text;

    std::vector<uint8_t> encode() const;
    static bool decode(ByteReader &r, Chat &out);
};

struct UiClick {
    std::string panel;
    std::string id;
    std::string text;   // a field's contents when Enter was pressed in it; empty for a button

    std::vector<uint8_t> encode() const;
    static bool decode(ByteReader &r, UiClick &out);
};

// Reads the type byte without consuming the rest, so a dispatcher can switch on
// it. Returns Invalid for an empty buffer or an unknown type -- an unknown type
// is not a crash and not an assert, it is a message from something newer.
MsgType peek_type(const void *data, size_t size);

// What the server will accept. Held by the caller so a test can build an old
// server and a new one in the same process.
struct ServerConfig {
    uint16_t protocol     = kProtocol;
    uint16_t min_protocol = kMinProtocol;

    std::string engine_version;
    uint64_t    content_hash = 0;   // registry_content_hash of the server's registry

    std::vector<std::string> features;           // what the server can do
    std::vector<std::string> required_features;  // and what it will not do without
    std::vector<std::string> mod_channels;       // names, in registration order

    // What this server's registry holds, in id order. Sent to a client that
    // declared it can adopt a table; also what content_hash is computed over.
    std::vector<std::string> nodes;

    // And the shape of those that have one. NOT part of content_hash: a shape
    // travels now, so two sides that disagree about one are no longer two sides
    // reading different worlds -- the client takes the server's. What the hash
    // still guards is the ids in the stored chunks.
    std::vector<NodeShapeDesc> shapes;

    // And its item table, in id order, under the same agreement. Not part of
    // content_hash: two servers with the same blocks and different tools are
    // not a content mismatch, and refusing that join would be a rule nobody
    // asked for.
    std::vector<ItemDesc> items;

    // And how each of `nodes` digs, by the same ids.
    std::vector<NodeDigDesc> node_digs;
    std::vector<NodeBodyDesc> node_bodies;   // by id, like node_digs (protocol 38)

    // And what the bodies in this world may look like, in id order. Not part of
    // content_hash either, and for a stronger reason than the item table: a
    // client that has never seen one of these models draws the box it drew
    // before, so a difference here is a poorer picture and not a wrong world.
    std::vector<std::string> models;

    // How a body moves in this world (TODO.md OPEN-44): what the welcome tells
    // every client, and what the server that owns this config works its
    // validator's limits out from. The engine's own numbers when no mod said.
    MovementRules movement;

    uint32_t seed       = 0;
    uint64_t mapgen_hash = 0;

    int max_players = 8;   // DESIGN.md: 2-8 per world, decided
    int players     = 0;

    // Whether a player has to say who they are, and prove it. Off by default,
    // which is a LAN server where the names on the screen are the people in the
    // room. On, nobody joins without a key they hold the secret half of -- and
    // since a proof needs a channel binding, on also means encrypted.
    //
    // Independent of the check itself: an identity that IS presented is always
    // verified, on or off. This decides whether an anonymous join is allowed,
    // not whether a lie is.
    bool require_identity = false;

    // Whether this world lets a portal look into it.
    //
    // DESIGN.md 4.4 offers two ways for a passive session to be admitted: a
    // ticket from a server this one trusts, or this -- a per-world setting that
    // says anybody may watch. The second one exists first because it is the one
    // that needs no infrastructure, and a world that refuses both is a world no
    // portal can show.
    bool allow_observers = true;

    // How many of them at once. An observer costs chunk streaming and nothing
    // else, but "nothing else" times a thousand is still a server. Separate
    // from max_players because they are not competing for the same thing: a
    // world full of players must still be watchable, and a world full of
    // watchers must still be playable.
    int max_observers = 8;
    int observers     = 0;

    // Who this server is when it signs a ticket, and whether it can. The secret
    // half lives in ServerSessions -- a config is a thing that gets copied into
    // logs and tests, and a secret key is not.
    uint8_t sign_public[kSignPublicBytes]{};
    bool    has_sign_key = false;

    // Whose tickets this world honours. Empty means nobody's, which is not the
    // same as "anybody may come in" -- that is allow_observers, and the two are
    // separate on purpose: a world can be open to everyone, open to the players
    // of two servers it knows, or open to neither.
    std::vector<std::array<uint8_t, kSignPublicBytes>> trusted_issuers;

    // Whether a portal needs one. Off by default, which is DESIGN.md 4.4's
    // first door: this world admits whoever asks, subject to allow_observers
    // and a free seat. On, a session that arrives through somebody else's
    // portal needs a ticket a trusted server signed for the player holding it.
    //
    // It says nothing about an ordinary join. A player typing this server's
    // address is not coming through a portal, and require_identity is the knob
    // for that.
    bool require_ticket = false;

    // This world's name among the server's rooms, told in the Welcome.
    std::string room;

    // And its sky, when it has one of its own (core.world_sky), or its storm
    // (core.world_storm).
    bool    has_sky = false;
    uint8_t sky[3]{};
    bool    has_storm = false;
    uint8_t storm[3]{};
};

struct HandshakeResult {
    bool         accepted = false;
    RefuseReason reason   = RefuseReason::None;
    std::string  detail;
    Welcome      welcome;   // filled when accepted
};

// The negotiation, in one function, with no I/O in it. Version: the highest
// both sides speak, refused when the ranges do not overlap -- and refused with
// which side is too old, because "connection failed" is not a thing a player
// can act on. Features: the intersection, in the server's order so both sides
// end up with the same table in the same sequence.
// `vouched` says the caller has already checked a ticket and it holds. It is
// what makes DESIGN.md 4.4's "oppure" real: a world may refuse anonymous
// watchers and still admit the players of a server it trusts. Passing it here
// rather than letting this function verify the ticket keeps the signature
// checking with the session that has the channel binding, and keeps this
// function what it has always been -- a decision about two structs.
HandshakeResult negotiate(const Hello &hello, const ServerConfig &config, bool vouched = false);

// The client's half of the same decision, and it is not a formality: a welcome
// is the first thing a server says, it arrives before anything has been
// verified, and the client is about to store the version, the feature table and
// the channel numbers it will use for the rest of the session. So the client
// checks that the answer is an answer to the question it asked -- a version
// inside the range it offered, no feature it did not offer, no channel number
// handed out twice.
bool accept_welcome(const Welcome &welcome, const Hello &sent, std::string &err);

// Names end up in logs, in chat, and in a database. Validated once, here,
// before any of that: printable ASCII, no control characters, bounded.
bool valid_player_name(const std::string &name);

} // namespace net
} // namespace cnp
