#pragma once

#include "core/net/crypto.h"
#include "core/media/media_cache.h"
#include <cstring>

#include "core/net/protocol.h"
#include "core/world/daylight.h"
#include "core/net/transport.h"
#include "core/item/item_registry.h"
#include "core/registry/node_registry.h"
#include "core/voxel/cell.h"
#include "core/voxel/chunk.h"

#include <cstdint>
#include <memory>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

// The client half of a session: the mirror of ServerSessions, and the piece
// that was missing for longer than it should have been.
//
// M3 built the whole network against a test probe. That probe grew a complete
// client -- connect, handshake, adopt the block table, install chunks, claim a
// position, take a correction -- inside tests/net_loopback_test.cpp, where the
// game itself could not reach it. Two clients would then have existed: the one
// under test and the one people play, and the soak would have been measuring
// the wrong one.
//
// So this file is that client, moved somewhere both can link. The probe uses
// it, the graphical client uses it, and the thirty-minute soak therefore
// exercises the code that ships rather than a sibling of it.
//
// It lives in cnp_net rather than cnp_core because it owns a socket. Every
// decision it makes -- whether a welcome is acceptable, what the ids mean --
// is a call into cnp_core, and can still be tested with no socket at all.

namespace cnp {
namespace net {

enum class ClientState : uint8_t {
    Idle = 0,     // nothing attempted yet
    Connecting,   // the socket is opening; no answer either way
    Handshaking,  // connected, hello sent, waiting
    Playing,      // welcomed, and the server's block table adopted
    Refused,      // the server said no -- refusal() says why
    TimedOut,     // the socket went nowhere, or the server never answered
    Failed,       // this client rejected what it was sent -- error() says what
    Closed,       // the connection ended after a successful join
};

// Whether the join has settled -- in, out, or broken. What a caller waits on
// before deciding what to do; it says nothing about whether the session is
// still live, which is what playing() is for.
inline bool client_join_settled(ClientState s)
{
    return s == ClientState::Playing || s == ClientState::Refused ||
           s == ClientState::TimedOut || s == ClientState::Failed ||
           s == ClientState::Closed;
}

const char *client_state_text(ClientState s);

// Another player, as far as this client knows. `last_seen` is what lets a
// player who left stop being drawn: entity updates are unreliable, so silence
// has to mean something on its own rather than waiting for a message that says
// "gone" and may never arrive.
// Somebody else's body, or a mob's, or a stack lying on the ground. The name
// predates the second and the third of those and is now narrower than what it
// holds; renaming it touches every caller and buys nothing today, so it is said
// here instead of pretended away.
struct RemotePlayer {
    uint64_t   id  = 0;
    float      x   = 0.0f, y = 0.0f, z = 0.0f;
    float      yaw = 0.0f, pitch = 0.0f;
    double     last_seen = 0.0;
    EntityKind kind = EntityKind::Player;

    // Which stack this is, when it is one; on a person, what is in their hand
    // (protocol 48). kNoItem for anything else.
    uint16_t item = 0;
    uint16_t item_tint = 0;
    uint32_t livery = 0;   // a person's colours and extras (protocol 93, kLiveryColours)

    // And what it looks like: a position in the welcome's model table plus one,
    // 0 for a body nothing was said about. A client that has not got that model
    // draws what it drew before, which is a box -- an unknown model is a poorer
    // picture and never an invisible player.
    uint16_t model = 0;

    // Swinging at something, a dig or a hit (protocol 49), and crouched (51).
    bool working   = false;
    bool crouching = false;
    std::string clip;   // a worker creature's, named by its mod (protocol 81); a person's (92)

    // What they wear, head to feet, by item id (protocol 58).
    uint16_t worn[5] = {0, 0, 0, 0, 0};
    uint16_t arm[2]  = {0, 0};   // what the pack's arms hold (protocol 96)
    uint16_t worn_tint[5] = {0, 0, 0, 0, 0};
    uint16_t dress[10] = {}, dress_tint[10] = {};   // a mod's models on them (102)
    bool burning   = false;   // a creature in the sun
    bool falling   = false;   // a Drop that is a block falling: `item` is the block (73)
    bool lying     = false;   // a Player on their back, yaw the way the head points (75)
    bool flying    = false;   // a Player in the air on purpose (97)
    bool held      = false;   // a creature this player carries, drawn by its mod (91)

    // In a kart, how it sits (protocol 66; EntityState::chassis).
    bool  chassis    = false;
    float body_pitch = 0.0f, body_roll = 0.0f;
    float base_dx = 0.0f, base_dy = 0.0f, base_dz = 0.0f;
    // A machine's hull, to stand on (protocol 100; EntityState::hull_w).
    float hull_w = 0.0f, hull_h = 0.0f, hull_l = 0.0f, hull_ride = 0.0f;
    bool  boosting = false, sliding = false;   // a kart's nitro and drift (103)
};

struct ClientConfig {
    std::string host = "127.0.0.1";
    uint16_t    port = 25301;

    double handshake_timeout = 10.0;   // a floor; see ClientSession::handshake_window
    double move_interval     = 0.05;   // 20 Hz, the server's tick rate
    double forget_players    = 5.0;    // silence this long and a player stops existing

    // The server's long-term public key, if the player has one to insist on.
    // With it, a server presenting any other key is refused before a word is
    // exchanged. Without it the session is still encrypted, but an attacker who
    // can intercept could be the one on the other end -- and the client says so
    // rather than letting it pass unmentioned.
    bool    pin_server = false;
    uint8_t server_key[kKeyBytes]{};

    // Who the server must be, by its signing key: what a portal's PortalAway
    // said was over there. A welcome naming another key, or none, fails the
    // join -- a crossing lands only on the server the link was made to.
    bool    expect_signer = false;
    uint8_t signer[kSignPublicBytes]{};

    // Off only for measuring what the encryption costs, and for the in-process
    // loopback test. A caller that says nothing gets encryption.
    bool encrypt = true;

    // Who the player is, as opposed to what they are called. DESIGN.md 4.4.
    // Without one the join is anonymous, which a server may refuse; with one,
    // the session signs a value only this session has, so the proof is worth
    // nothing to anybody who records it.
    SignKeyPair identity;
    bool        has_identity = false;

    // What this session is for. A passive session behind a portal asks to
    // Observe: chunks and the people in the world, no avatar of its own,
    // nothing it can change. Everything a person plays is Enter.
    Scope scope = Scope::Enter;

    // Which of the server's worlds (protocol 56); empty for its first.
    std::string room;

    // A ticket for this world, signed by the world the portal is in. Presented
    // in the hello for a session that is opening a window; a world that admits
    // anybody ignores it, and one that does not is the reason it exists.
    bool         has_ticket = false;
    PortalTicket ticket;

    // Trying again, for a session that is supposed to exist.
    //
    // DESIGN.md M6's exit test names this: a passive session pointed at a dead
    // server must not spam the log. There are two ways to satisfy that and only
    // one of them is right -- giving up after one attempt is quiet and also
    // means the portal never comes back when the server does. What is wanted is
    // to keep trying, at a rate that goes down.
    //
    // Zero means one attempt and nothing after it, which is what every caller
    // did before this existed. The wait starts at `retry_first`, doubles, and
    // stops growing at `retry_max` -- so an hour of a dead neighbour costs
    // about sixty handshake attempts rather than about a hundred thousand.
    //
    // A refusal that refusal_is_permanent() calls permanent ends it: that is a
    // decision on the other end, and asking again cannot change it.
    double retry_first = 0.0;
    double retry_max   = 60.0;

    // How long a session may hear nothing from a server that HAS sent a
    // heartbeat before it treats the silence as an outage.
    //
    // Only after the first one, which is the whole compatibility rule: a
    // server that never sends any is not a dead server, it is a server that
    // does not do this. Without that rule, adding heartbeats would declare
    // every session against anything that has not adopted them dead five
    // seconds after it joined.
    double heartbeat_silence = kHeartbeatSilence;

    // Where media this session is offered end up, and where they are looked
    // for first. Shared with every other session this client has -- that is the
    // point of it (DESIGN.md 4.5), and the reason it is a pointer the caller
    // owns rather than something a session makes for itself.
    //
    // Null means no cache: the session still asks for nothing and draws with
    // whatever the client can produce locally, which is what every client did
    // before this existed.
    MediaCache *media_cache = nullptr;
};

// The hello this build sends: its version, its block table, and what it can do.
// Exposed rather than built inside connect() so a test can mutate it -- a
// pretended block table, or a client that cannot adopt one -- and still go
// through exactly the code path the real client goes through.
Hello make_client_hello(const std::string &player_name);
// The livery every hello this process makes says the player picked (Hello::livery);
// the client sets it from its options before it joins anything.
inline uint32_t g_client_livery = 0;

class ClientSession {
public:
    ClientSession() = default;
    ~ClientSession();

    ClientSession(const ClientSession &)            = delete;
    ClientSession &operator=(const ClientSession &) = delete;

    // Opens the socket and arranges for the hello to go out when the connection
    // comes up. Returns false only if the socket could not be opened at all;
    // everything after that is reported through state().
    bool connect(const ClientConfig &cfg, const Hello &hello, double now, std::string &err);

    // Convenience: the ordinary case, with the ordinary hello.
    bool connect(const ClientConfig &cfg, const std::string &player_name, double now,
                 std::string &err)
    {
        return connect(cfg, make_client_hello(player_name), now, err);
    }

    void close();

    // Where arriving chunks are installed, and where the server's block table
    // gets adopted. By default the session's own, which is what the probe wants;
    // the game points these at the world and registry its renderer already
    // holds, because the alternative is copying a world every frame.
    //
    // Must be called before connect(): the adoption happens the instant the
    // welcome lands, and a registry swapped out after that is a registry the
    // arriving ids do not index.
    void use(World &world, NodeRegistry &registry);

    // And where the world's movement rules go when the welcome says them
    // (TODO.md OPEN-44). A caller that moves a body by them passes its own, so
    // that the numbers the body moves by are the ones the server checks; the
    // two-argument form keeps them here, for a caller with no body to move.
    void use(World &world, NodeRegistry &registry, MovementRules &movement);

    // Drains whatever has arrived, spending at most `timeout_ms` waiting for the
    // first event. Also enforces the handshake timeout, so a server that accepts
    // the socket and then says nothing does not hang the caller forever.
    void pump(double now, int timeout_ms);

    // Claims a position. Rate limited to cfg.move_interval; returns whether one
    // actually went out, so a caller can count what it sent rather than what it
    // asked to send. Unreliable on purpose: a position claim that arrives late
    // is worse than one that never arrives, because the next one is already
    // truer. `flying` is whether the body flew here: the server does not charge
    // a flight down as a fall.
    //
    // With it goes what a body that drives did since a few claims back
    // (record_drive), the velocity, and which placement and which numbers the
    // body is on, for the server to drive the same kart (TODO.md OPEN-47).
    bool send_move(float x, float y, float z, float look_x, float look_y, float look_z,
                   double now, bool flying = false, Vec3 velocity = {});

    // What this frame's chassis did (core/physics/vehicle.h), to go with the
    // next claims. Recorded against the numbers the body moves by now.
    void record_drive(const std::vector<DriveEvent> &events);

    // True when this client had an identity to offer and could not prove it --
    // which today means the session is unencrypted. The join, if it happened,
    // was anonymous, and a caller that cares is expected to say so out loud.
    bool identity_unproven() const { return m_identity_unproven; }

    // What the world on the other end last said it is: how full, and whether
    // it can still be looked into or walked into. All zeroes until the first
    // heartbeat arrives, which `has_heartbeat()` is how a caller tells.
    const Heartbeat &heartbeat() const { return m_beat; }
    bool  has_heartbeat() const { return m_beats > 0; }
    long  heartbeats() const { return m_beats; }

    // Whether the world on the other end will take somebody who walks in.
    //
    // The half of the heartbeat that is not about liveness. DESIGN.md 4.4 has a
    // mouth showing a world the player may want to walk into, and whether that
    // is possible right now is not something a client can work out from chunks
    // -- so without this the first news of a full world is a refused crossing,
    // a round trip after the player has already walked into the opening.
    //
    // **No heartbeat is a yes.** A world that sends none is not a full world,
    // it is a world that does not do this -- the same compatibility rule that
    // keeps the silence timer from declaring every older server dead. Backwards,
    // it would make every mouth onto anything built before heartbeats
    // impassable, and nothing about that would look like a bug.
    bool world_has_room() const { return m_beats == 0 || m_beat.enterable(); }

    // And the other door: whether it would let a portal look in at all. Not
    // acted on yet -- a window that is already open stays open -- and reported
    // so that a mouth onto a world that has closed to portals is a thing a
    // person can see rather than infer from a refused reconnection.
    bool world_takes_windows() const { return m_beats == 0 || m_beat.watchable(); }

    // Heartbeats that were sent and did not arrive, from the gaps in the
    // sequence. The message is reliable, so this should be zero; it is here
    // because "should be zero" is a thing to measure rather than assume.
    long  heartbeats_lost() const { return m_beats_lost; }

    // The longest this session ever waited between two heartbeats.
    //
    // The number the silence window has to be chosen against, and it was not
    // measured before it was chosen: five seconds came from "five intervals" on
    // a link where an interval costs a tenth of a millisecond. While the beat
    // was reliable this measured the tail of ENet's retransmission backoff --
    // 9.2 s at 20% loss, against a 10 s window, with heartbeats_lost() stuck at
    // zero because a reliable channel cannot show a gap in the sequence.
    double heartbeat_gap_max() const { return m_beat_gap_max; }

    // What ENet measures the round trip to this server as, in seconds, and the
    // silence window that follows from it. Both reported rather than assumed:
    // a window derived from a number nobody prints is a window nobody can
    // check.
    double round_trip() const;
    double heartbeat_window() const;
    double silence_window_used() const { return m_silence_window; }

    // How long to wait for a join to finish, and how long the one that finished
    // took. Ten seconds was another loopback number: at 400 ms and 20% loss one
    // join in six never completed inside it, and the run ended before it had a
    // world.
    double handshake_window() const;
    double joined_in() const { return m_joined_in; }

    // How long since the last one, or -1 if none has ever arrived.
    double since_heartbeat(double now) const
    {
        return m_beats > 0 ? now - m_last_beat : -1.0;
    }

    // How many times this session has opened a socket, the first one included.
    // What a gate counts: with the backoff working this grows like the log of
    // the time spent, and without it, like the time spent.
    int    attempts() const { return m_attempts; }

    // The wait before the next attempt, and when it is due. `retry_delay` is
    // what a caller logs against: it doubles until it reaches the ceiling and
    // then stops changing, which is exactly the point at which there is nothing
    // new to say.
    double retry_delay() const { return m_retry_delay; }
    bool   retrying() const { return m_retry_at > 0.0; }
    double retry_due() const { return m_retry_at; }

    // Whether this session has stopped for good: a refusal that trying again
    // cannot change, or a caller that asked for no retries. A caller shows this
    // to the player instead of a spinner.
    bool   given_up() const;

    ClientState        state() const { return m_state; }
    bool               playing() const { return m_state == ClientState::Playing; }
    const std::string &error() const { return m_error; }
    const Refuse      &refusal() const { return m_refusal; }
    const Welcome     &welcome() const { return m_welcome; }

    // The name this server goes by, as opposed to the address it was dialled
    // at. Empty for a world with no ticket key, and then a caller has nothing
    // to match on but the address -- see Welcome::signing_key.
    bool same_server_as(const uint8_t key[kSignPublicBytes]) const
    {
        return m_welcome.has_signing_key &&
               std::memcmp(m_welcome.signing_key, key, kSignPublicBytes) == 0;
    }
    bool server_is_named() const { return m_welcome.has_signing_key; }

    // Where this session was dialled, and which room of that server it is in.
    const std::string &host() const { return m_cfg.host; }
    uint16_t           port() const { return m_cfg.port; }
    // The room's own name once it has said it; what was asked for until then.
    const std::string &room() const
    {
        return m_welcome.room.empty() ? m_cfg.room : m_welcome.room;
    }

    // The server's block table, adopted. Valid from the moment state() first
    // becomes Playing: ids in the chunks that follow are indices into this.
    const NodeRegistry &registry() const { return *m_registry; }
    size_t              unknown_blocks() const { return m_unknown; }

    World       &world() { return *m_world; }
    const World &world() const { return *m_world; }

    // Asks the server to break a block, or to build on one of its faces.
    //
    // The block the ray HIT and the face of it, never the cell that should
    // change: the server derives that itself. Reliable and immediate, like a
    // portal placement and for the same reason -- an edit that is dropped is a
    // click that did nothing with nothing on screen to say so.
    bool send_edit(int x, int y, int z, int nx, int ny, int nz, bool breaking, int slot,
                   int turn = 0, bool place_only = false);

    // Asks the server to put one stack in another slot. `count` of 0 is the
    // whole thing. What comes back is the whole bag, always -- this client
    // never works out what its own inventory now holds.
    //
    // The two sides are Side values and not flags. They started as "is it the
    // chest", which was one bool while there were two grids; a craft grid made
    // three of them, and three bools have five states that cannot happen.
    bool send_move_item(int from, int to, int count = 0, Side from_side = Side::Bag,
                        Side to_side = Side::Bag);

    // Takes what the craft grid makes, paying for it out of the grid. Its own
    // call rather than a move from a magic slot number at every call site: the
    // result is not a square, and a caller writing kCraftResultSlot by hand is
    // a caller that can write the wrong number.
    bool send_take_craft();

    // A shift-click: that stack, to wherever it obviously goes. No destination
    // travels with it because the player did not name one -- see
    // net::quick_target, which is the server's half of this.
    bool send_quick_move(int slot, Side side = Side::Bag);

    // And a shift-click on the answer, which is "make that until something runs
    // out". Its own call for the reason send_take_craft is its own call: a
    // caller writing kCraftResultSlot by hand can write the wrong number.
    bool send_quick_craft();

    // How much of this player is left, as the server last said. A mirror and
    // never a source: nothing on this side takes health off anybody, and a
    // client that drew its own would be drawing a guess.
    //
    // has_vitals() is false until the first message, which is not "they are
    // dead" -- it is "nobody has said yet", and a bar drawn from it would show
    // an empty one to everybody who just joined.
    const Vitals &vitals() const { return m_vitals; }
    bool          has_vitals() const { return m_has_vitals; }

    // A seat (core.ride): whether this body is carried on another, and where
    // the seat is, as the server last said.
    bool riding(Ride &out) const
    {
        out = m_ride;
        return m_riding;
    }

    // The sounds a mod played (core.play_sound) since the last call.
    std::vector<Sound> take_sounds() { return std::exchange(m_sounds, {}); }

    // Laid down or got up (core.player_lie) since the last call: false when
    // the server has said nothing new.
    bool take_lie(Lie &out)
    {
        if (!m_has_lie) return false;
        out       = m_lie;
        m_has_lie = false;
        return true;
    }

    // The mouths this world has shut to us (core.portal_seal), bit m.
    uint32_t sealed() const { return m_sealed; }

    // The knocks the server sent since the last call, added up, and forgotten:
    // false when there were none.
    bool take_push(Push &out)
    {
        if (!m_has_push) return false;
        out        = m_push;
        m_push     = Push{};
        m_has_push = false;
        return true;
    }

    // What time it is in the world this client is in, as the server last said.
    //
    // A mirror, and there is no clock behind it: the time is not advanced
    // between messages. That is not a shortcut -- core/world/daylight.h says
    // what one telling-interval of staleness is worth on the screen, and it is
    // a tenth of the step between two of the sixteen light levels a block can
    // have. A clock here to smooth that over would be a second clock to
    // disagree with the server's.
    //
    // has_time() is false until the first message. Noon is the value it starts
    // at because a world that opens in the dark is a world nobody can see, but
    // "nobody has said yet" and "it is noon" are different things and the
    // caller can tell them apart.
    double time_of_day() const { return m_time; }
    bool   has_time() const { return m_has_time; }

    // How far the server's wind has carried the clouds, in nodes, run on at
    // kWindSpeed since it was last told: the one thing here that does move
    // between tellings, because a cloud that jumps once a second is seen.
    double wind() const;

    // What this player's craft grid holds and what it makes, as the server last
    // described it. A mirror and never a source, like carried().
    const CraftState &crafting() const { return m_craft; }
    bool              has_craft() const { return m_has_craft; }

    // Out of the bag and onto the ground. `count` of 0 is the whole stack --
    // which is what the client has to say, because the number in that slot is
    // one round trip old and the server's is the only one that counts.
    bool send_drop_item(int slot, int count = 0);

    // "I struck that", with the hotbar slot that is the hand. The entity id
    // from the list this client is already being sent: what the item in that
    // slot takes off is the server's table (ItemDef::damage) and how often it
    // may swing is its rule (ecs::kHitCooldown). See net::Hit.
    bool send_hit(uint64_t target, int slot, bool use = false);

    // "The hand is this square now", when it changes: what the server shows
    // everybody else in it (net::Hold).
    bool send_hold(int slot, bool working, bool crouching);

    // A line the player typed in chat, said or a /command (net::Chat).
    bool send_chat(const std::string &text);

    // The livery picked mid-game, for everybody else to see (net::Dress).
    bool send_livery(uint32_t livery);

    // The chat lines the server sent since the last call, oldest first.
    std::vector<std::string> take_chat() { return std::exchange(m_chat, {}); }

    // A button on the panel that is up, or Enter in one of its fields. The
    // panel's name and the element's id, both as the mod wrote them -- never a
    // coordinate and never an index -- and a field's text. See net::UiClick.
    bool send_ui_click(const std::string &panel, const std::string &id,
                       const std::string &text = "");

    // A cell the server says has changed, and that this client has not drawn
    // yet. The block is already IN the world by the time this is taken -- this
    // session installs it the way it installs a chunk -- so what the caller
    // still owes it is light and a mesh.
    //
    // Drained one at a time rather than handed over as a batch: a burst of
    // changes is several cells and each one needs its own relight, so a caller
    // that took them all at once would only have to loop anyway.
    bool next_block_change(BlockChange &out);

    // What a mod last wanted this player to read (core.notice), once. Only the
    // newest: a notice said while one is up replaces it on the screen anyway.
    bool take_notice(std::string &out)
    {
        if (m_notice.empty()) return false;
        out = std::move(m_notice);
        m_notice.clear();
        return true;
    }

    // The last refusal the server sent for an edit, if any. Empty when the last
    // edit went through -- the sentence is the server's and it names what the
    // player can do about it.
    //
    // Cleared by news about the CELL it was about and by nothing else. A
    // refusal used to be wiped by any BlockChange at all, which on a world with
    // a mod in it meant a sentence that lived a fraction of a second: measured
    // by tests/tools.sh, where twelve refusals in a row read back as an empty
    // string because mods/beacons had changed a cell somewhere else in between.
    // Whether somebody has a container open at that cell, as far as this
    // client has been told.
    //
    // A set of cells and not a flag on a block: a chest with a lid looks open
    // to the whole room, so it is the CELL that is open and not the session
    // that opened it. It holds only what has been SAID -- a client that joins
    // while a chest stands open is told nothing until it changes, which is
    // written down in TODO OPEN-32 rather than papered over.
    bool cell_open(int x, int y, int z) const
    {
        return m_open_cells.find(Cell{x, y, z}) != m_open_cells.end();
    }
    const std::unordered_set<Cell, CellHash> &open_cells() const { return m_open_cells; }

    const std::string &edit_refusal() const { return m_edit_refusal; }
    long               edits_refused() const { return m_edits_refused; }

    // What this session is carrying, as the server last described it. A mirror
    // and never a source: DESIGN.md 3.4 puts inventories server-side, and there
    // is no message in this protocol that lets a client put something in a bag.
    const InventoryState &carried() const { return m_carried; }
    bool                  has_inventory() const { return m_has_inventory; }

    // The panel the server says is up, or nothing. A mirror and never a source,
    // exactly like carried(): this client draws what the last PanelShow said and
    // has no way to open, close or edit a panel of its own.
    const PanelShow *panel() const { return m_panel.open ? &m_panel : nullptr; }

    // What is in the container this session has open, as the server last
    // described it. A mirror and never a source, exactly like carried(): a
    // chest belongs to a cell on the server and this client cannot put anything
    // in one, only ask.
    const ContainerState *container() const { return m_container.open ? &m_container : nullptr; }

    // Done with it. Sent when the player shuts the panel, because the chest is
    // the server's and hiding the picture of one is not closing it.
    bool send_close_container();

    // How many PanelShow messages have arrived. A caller that has to REBUILD
    // something out of panel() -- and this client's own PanelSet is exactly
    // that -- needs to know when to, and comparing panels field by field every
    // frame is the work this replaces. Includes the ones that closed a panel:
    // a close is a change like any other.
    uint64_t panel_revision() const { return m_panel_rev; }

    // This world's item table, adopted the way its block table is. Ids in
    // carried() are indices into it.
    // A mod's own channel, by the mod's name. 0 for a mod this server does not
    // run: the numbers come from the WELCOME, so they are the server's and not
    // a guess made from the mods this process happens to have.
    uint8_t channel_for(const std::string &mod) const
    {
        for (const ChannelAssignment &c : m_welcome.channels)
            if (c.name == mod) return c.id;
        return 0;
    }

    // Says something on that mod's channel. False for a mod with no channel
    // here -- which is not an error: a client without the mod simply never
    // sends on it.
    bool send_mod(const std::string &mod, const std::string &text);

    // What arrived on those channels since the last time this was cleared, each
    // with the name of the mod whose channel it came in on.
    struct ModMessage {
        std::string mod;
        std::string text;
    };
    const std::vector<ModMessage> &mod_messages() const { return m_mod_messages; }
    void                           clear_mod_messages() { m_mod_messages.clear(); }

    const std::vector<ItemDesc> &item_table() const { return m_welcome.items; }
    const ItemRegistry          &items() const { return m_own_items; }
    // Mutable for one thing: the atlas writes each item's icon tile into the
    // table the hotbar names things from, which is this one.
    ItemRegistry                &items() { return m_own_items; }

    // A correction the server sent and this client has not applied yet. Taking
    // it clears it: two corrections in one frame mean the older one is already
    // wrong, and applying both would move the player twice.
    bool take_correction(Correct &out);
    long corrections() const { return m_corrections; }

    // A crossing the server decided and this client has not drawn yet. Same
    // rule as a correction -- taking it clears it -- and for a stronger reason:
    // two crossings in one frame would move the player through the second one
    // from where the first one put them, which is a place they were never
    // shown standing.
    bool take_teleport(Teleport &out);

    long teleports() const { return m_teleports; }

    // How many times the server has given this body numbers of its own
    // (TODO.md OPEN-47). What they are is wherever use() put the movement.
    long body_movements() const { return m_body_movements; }
    // How much faster than its tool this body digs (BodyMovement::dig).
    float dig_factor() const { return m_dig; }

    // Asks the server to put an opening on the face of a block. Reliable, and
    // it goes out immediately rather than being rate limited: a placement is
    // something a person did once, not a stream.
    //
    // Nothing changes on this client when it is sent. Where the opening ends
    // up, and whether it is allowed at all, is the server's answer -- which
    // arrives as a Portals message like everybody else's does. A client that
    // drew its own opening on the click would be showing a portal that may not
    // exist.
    bool send_place_portal(int which, int x, int y, int z, int nx, int ny, int nz);

    // Asks this session to become a player, coming out of this world's opening,
    // having walked into `from` at that pose in the world being left. The
    // answer, if there is one, arrives as a Teleport: this world decides where
    // its own opening puts somebody.
    //
    // And the other direction: a player who has left becomes a window onto the
    // world they left. No pose, because they are not anywhere any more.
    // `portal_id` is which of this world's openings to come out of, copied
    // from the PortalAway::portal of the mouth that was walked into. The
    // world being left decides it, not the player and not this client.
    bool send_enter(uint32_t portal_id, const PortalDesc &from, float x, float y, float z,
                    float look_x, float look_y, float look_z, float vx, float vy, float vz,
                    const PortalTicket *ticket = nullptr);
    bool send_observe();

    // The other answer to a scope change: it was refused, and here is why.
    // Taken and cleared, like a correction, because it is an event about one
    // request and not a state anything should keep reading.
    //
    // A caller that waits for a Teleport and nothing else still works -- it
    // just waits out its own timeout instead of being told. That was the whole
    // behaviour before this existed.
    bool take_scope_denial(ScopeDenied &out);
    long scope_denials() const { return m_scope_denials; }

    // "Your session continues over there." Taken and cleared like a
    // correction, because it is an event about one moment.
    //
    // A caller with a live window onto that world does not need it -- the
    // seamless crossing is available and this is the fallback. Taking it and
    // dropping it is the right thing in that case, and counting them is how a
    // gate tells the two paths apart.
    bool take_redirect(Redirect &out);
    long redirects() const { return m_redirects; }

    // Asks the world this session is in to vouch for the player at the world
    // its opening leads to. `scopes` is kTicketObserve, kTicketEnter, or both;
    // `opening` is which of this world's mouths, since each may lead elsewhere.
    //
    // The answer arrives as a Ticket, granted or not, and is taken like a
    // correction: it is an event about one request. A client that never asks
    // gets in wherever the door is open, which is every world until somebody
    // closes one.
    bool send_ticket_request(uint8_t scopes, uint8_t opening = 0);
    bool take_ticket(Ticket &out);
    long tickets_received() const { return m_tickets_received; }

    // What this session is for, as far as this client asked. Not what the
    // server granted: a request that was refused leaves this saying Enter while
    // the server still has an observer, which is why the Teleport is what a
    // caller should wait for.
    Scope scope() const { return m_cfg.scope; }

    // Where the server says the openings are. Unlike a correction this is not
    // taken and cleared: it is state, not an event, and a client that joined
    // late needs the last one it was sent rather than the ones it missed.
    const Portals &portals() const { return m_portals; }
    bool           has_portals() const { return m_portals_version > 0; }
    uint32_t       portals_version() const { return m_portals_version; }

    const std::vector<RemotePlayer> &players() const { return m_players; }

    // Whether the server proved which server it is. False means encrypted but
    // unauthenticated -- fine on a LAN, not fine for a portal into somebody
    // else's world -- and the caller is expected to say which it got.
    bool authenticated() const
    {
        return m_transport.encrypted() && m_transport.authenticated(m_transport.server_peer());
    }

    // The long-term key the server presented, in hex. What a player reads once
    // and then passes back as a pin.
    std::string server_key_hex() const
    {
        return m_transport.peer_key_hex(m_transport.server_peer());
    }

    // ------------------------------------------------------------- media
    //
    // What this world said its blocks look like, what of that was already on
    // disk, and what had to come over the wire. The second and third numbers
    // are the whole claim of DESIGN.md 4.5: a second world offering the same
    // texture must move `media_had` and leave `media_received` alone.

    // Every name the world offered, with the hash it resolves to. State and not
    // an event: a caller rebuilding an atlas needs the whole table, not the
    // part that arrived since it last looked.
    const std::vector<MediaEntry> &media_offer() const { return m_media_offer; }

    // The bytes for a file this world offered, from the cache. This is what
    // goes behind the renderer's MediaSource -- the callback exists so that
    // this can be what fills it, instead of a folder next to the binary.
    //
    // Returns false for a name this world did not offer, and for one it offered
    // that has not arrived yet. Both are the ordinary case for a few frames
    // after the join.
    bool media_file(const std::string &name, std::vector<uint8_t> &out);

    // Nothing left to wait for: every offered file has either arrived or been
    // asked for and answered. What a caller waits for before rebuilding its
    // atlas once instead of on every file.
    //
    // Not "everything is here". A file the server has and this client could
    // not store -- a full disk, a read-only cache -- is a file that will never
    // be here, and a renderer waiting for it would wait forever. The number
    // that says whether it IS all here is media_missing(), reported next to it.
    bool media_complete() const { return m_media_in_flight == 0 && !media_unasked(); }

    // Bumped when the offer lands and again for every file that arrives.
    //
    // What a caller rebuilds its atlas against. media_complete() alone is not
    // enough and the reason is a defect a real link found and loopback never
    // could: before the offer arrives the offer is EMPTY, so nothing is
    // outstanding and nothing is unasked -- complete() says yes, a one-shot
    // rebuild fires against no media at all, and the files that land 60 ms
    // later never reach the atlas. On loopback the offer beat the renderer
    // every time and the bug did not exist.
    unsigned media_version() const { return m_media_version; }

    int    media_offered() const { return (int)m_media_offer.size(); }
    int    media_had() const { return m_media_had; }
    int    media_received() const { return m_media_received; }
    int    media_missing() const { return m_media_missing; }
    int    media_rejected() const { return m_media_rejected; }

    // Files this world offered and then said it does not hold. A correct server
    // never does it -- the offer is built from what it has -- so this is the
    // count that says one is not correct, or that its content set changed under
    // it while a client was reading the old one.
    int    media_denied() const { return m_media_refused_by_server; }

    // Files that were asked for and never answered AT ALL, not even to say no.
    // With MediaMissing that means the link stopped, not the world: it is the
    // backstop firing, and the session's own silence window is about to have
    // something to say.
    int    media_dropped() const { return m_media_dropped; }
    size_t media_bytes() const { return m_media_bytes; }

    int                chunks_received() const { return m_chunks; }

    // Of those, the ones for a chunk this world already held. Zero is the claim
    // a seamless crossing makes: the world behind the opening was streamed
    // once, while it was being looked into, and walking in sent for nothing.
    int                chunks_repeated() const { return m_chunks_repeated; }
    int                chunks_dropped() const { return m_chunks_dropped; }
    const std::string &chunk_error() const { return m_chunk_error; }
    int                entity_messages() const { return m_entity_messages; }
    uint64_t           bytes_received() const { return m_transport.bytes_received(); }
    uint64_t           bytes_sent() const { return m_transport.bytes_sent(); }

    Transport &transport() { return m_transport; }

private:
    void handle(const std::vector<uint8_t> &data, double now, uint8_t channel);

    // Opens another socket when one is due. Called from pump(), because that is
    // the only place a caller already promises to be.
    void consider_retry(double now);
    void fail(std::string why);

    Transport   m_transport;
    ClientConfig m_cfg;
    Hello       m_hello;
    ClientState m_state = ClientState::Idle;
    double      m_started  = 0.0;
    double      m_joined_in = -1.0;

    Welcome     m_welcome;
    Refuse      m_refusal;
    std::string m_error;

    NodeRegistry  m_own_registry;   // the server's table, adopted
    NodeRegistry *m_registry = &m_own_registry;
    size_t        m_unknown  = 0;

    // And how a body moves in that world, written where use() said when the
    // welcome arrives.
    MovementRules  m_own_movement;
    MovementRules *m_movement = &m_own_movement;

    // And its item table, adopted the same way. Owned here rather than left to
    // the caller because there is nothing to decide: unlike a block, an item a
    // client has never heard of needs no placeholder invented for it -- it
    // names a block in a table this session has already adopted.
    ItemRegistry  m_own_items;
    std::vector<ModMessage> m_mod_messages;

    World       m_own_world;
    World      *m_world = &m_own_world;
    int         m_chunks          = 0;
    int         m_chunks_repeated = 0;
    int         m_chunks_dropped  = 0;
    std::string m_chunk_error;

    Correct m_correction;
    bool    m_has_correction = false;
    long    m_corrections    = 0;

    Teleport m_teleport;
    bool     m_has_teleport = false;

    // The server put the body somewhere (a correction or a teleport taken).
    void placed(uint32_t epoch);
    bool stale_placement(uint32_t epoch);

    std::vector<BlockChange> m_block_changes;
    std::string              m_notice;
    std::vector<std::string> m_chat;
    std::unordered_set<Cell, CellHash> m_open_cells;
    std::string              m_edit_refusal;
    long                     m_edits_refused = 0;
    int                      m_edit_x = 0, m_edit_y = 0, m_edit_z = 0;   // the cell last asked about
    bool                     m_edit_asked = false;
    InventoryState           m_carried;
    bool                     m_has_inventory = false;

    Vitals                   m_vitals;
    bool                     m_has_vitals = false;
    Push                     m_push;
    uint32_t                 m_sealed = 0;
    Ride                     m_ride;
    bool                     m_riding = false;
    Lie                      m_lie;
    bool                     m_has_lie = false;
    std::vector<Sound>       m_sounds;
    bool                     m_has_push = false;

    double                   m_time     = kNoon;
    bool                     m_has_time = false;
    double                   m_wind     = 0.0;
    double                   m_wind_at  = 0.0;   // steady seconds when it was told

    CraftState               m_craft;
    bool                     m_has_craft = false;
    PanelShow                m_panel;
    ContainerState           m_container;
    uint64_t                 m_panel_rev = 0;
    long     m_teleports    = 0;
    long     m_body_movements = 0;

    Portals  m_portals;
    uint32_t m_portals_version = 0;

    Ticket m_ticket;
    bool   m_has_ticket_answer = false;
    long   m_tickets_received  = 0;

    Heartbeat m_beat;
    long      m_beats     = 0;
    double    m_last_beat = 0.0;
    uint32_t  m_beat_seq  = 0;
    long      m_beats_lost = 0;
    double    m_beat_gap_max = 0.0;
    double    m_silence_window = 0.0;
    mutable double m_last_rtt  = 0.0;

    Redirect m_redirect;
    bool     m_has_redirect = false;
    long     m_redirects    = 0;

    ScopeDenied m_denial;
    bool        m_has_denial    = false;
    long        m_scope_denials = 0;

    // What is known about each offered file, in the offer's own order.
    //
    // A pair of flags and not a lookup into the cache, which is what this was
    // first: asking the cache "do you have it yet" to decide what to request
    // again means a cache that cannot store anything is asked for the same file
    // forever. Sabotaged exactly that way, the client fetched 179862 files and
    // 368 MB in twenty-five seconds instead of 62 files and 254 KB.
    struct MediaWant {
        bool have  = false;   // in the cache, now
        bool asked = false;   // requested from this world, answered or not
    };

    bool media_unasked() const
    {
        for (const MediaWant &w : m_media_state)
            if (!w.have && !w.asked) return true;
        return false;
    }

    std::vector<MediaEntry> m_media_offer;
    std::vector<MediaWant>  m_media_state;
    int                     m_media_had      = 0;
    int                     m_media_received = 0;
    int                     m_media_missing  = 0;
    int                     m_media_rejected = 0;
    int                     m_media_refused_by_server = 0;
    double                  m_media_renewed_at = 0.0;
    double                  m_hold_set_at      = 0.0;
    int                     m_media_in_flight = 0;
    unsigned                m_media_version   = 0;
    double                  m_media_asked_at  = 0.0;
    double                  m_media_looked_at = 0.0;
    int                     m_media_dropped   = 0;
    size_t                  m_media_bytes    = 0;

    // Asks for the next batch of what is still missing, up to
    // kMediaPerRequest. Called when the offer lands and again whenever a batch
    // finishes, because a world with more blocks than one request holds needs
    // more than one request.
    void request_missing_media(double now);

    // What this session was before the scope change it is waiting on, so a
    // refusal can put it back. m_cfg.scope is what this client ASKED for, and
    // an asked-for scope that was refused is not what the session is.
    Scope       m_scope_before_request = Scope::Enter;

    std::vector<RemotePlayer> m_players;
    int                       m_entity_messages = 0;
    // The frame of bodies being put together (protocol 109): which, the parts
    // come so far, and who was in them.
    uint16_t                  m_ent_frame = 0;
    uint32_t                  m_ent_mask  = 0;
    std::vector<uint64_t>     m_ent_seen;

    bool     m_identity_unproven = false;

    uint32_t m_move_seq  = 0;
    double   m_last_move = -1.0;

    // Which of the server's placements of this body was applied last, and
    // which of its numbers it moves by (Move::epoch, Move::gen).
    uint32_t m_epoch = 0;
    uint32_t m_epoch_heard = 0;   // the newest placement's, taken or not
    uint8_t  m_gen   = 0;
    float    m_dig   = 1.0f;

    // What a body that drives did, numbered: m_drive_first is the number of
    // the first one still kept, and m_drive_sent where each of the last few
    // claims began, which is how far back the next one reaches.
    std::vector<DriveRecord> m_drive;
    uint32_t                 m_drive_first = 0;
    uint32_t                 m_drive_sent[3]{};

    // Reconnection. `m_retry_at` of 0 means nothing is scheduled -- either the
    // session is alive, or it has stopped for good.
    int    m_attempts    = 0;
    double m_retry_delay = 0.0;
    double m_retry_at    = 0.0;
    bool   m_gave_up     = false;

    // Closed sends the state to Closed, and so does the server going away
    // after a successful join. They are the same state and opposite
    // situations: one is the caller saying stop, the other is exactly the
    // outage a retry exists for. Without this the mouth onto a server that
    // dies never comes back, which is the failure the backoff was built to
    // avoid, arrived at from the other side.
    bool   m_closed_by_caller = false;
};

// Every connection this process holds, as many as there are.
//
// The client used to have one, and the scene it draws used to have two worlds
// -- a number that came from "the portal pair leads somewhere else" and got
// written into an array bound. A portal in the world behind a portal is a third
// server, and a hub with four doorways is five; neither is exotic and neither
// should need a code change.
//
// So: sessions live in a list, they are named by an id, and the id survives
// other sessions closing. A caller that holds an id -- an opening that shows
// world 3 -- must not find it pointing at world 4 because world 2 went away.
// That is why a closed slot is kept rather than erased.
//
// What this owns that a bare vector does not: the pump. Waiting `timeout_ms` on
// each of five sockets in turn is five times the wait, and a client that does
// that at 50 ms a session has spent a quarter of a second before it draws
// anything. The first poll may block; the rest drain what is already there.
class ClientSessionSet {
public:
    static constexpr int kNoSession = -1;

    // Opens a connection and returns its id. The id is an index into a list
    // that only ever grows, so it stays valid until this set is destroyed.
    // Returns kNoSession and fills `err` if the socket could not be opened at
    // all; everything after that is reported through the session's state().
    int open(const ClientConfig &cfg, const Hello &hello, double now, std::string &err);
    int open(const ClientConfig &cfg, const std::string &player_name, double now,
             std::string &err)
    {
        return open(cfg, make_client_hello(player_name), now, err);
    }

    // The session with that id, or null for an id that was never handed out.
    // A closed session is still there and still answers state(): "it dropped"
    // is something a caller has to be able to find out.
    ClientSession       *get(int id);
    const ClientSession *get(int id) const;

    // Drains all of them. Only the first poll may block -- see the note above.
    void pump(double now, int timeout_ms);

    void close(int id);
    void close_all();

    // Ids that have been handed out, including closed ones. Iterate with get().
    int    count() const { return (int)m_sessions.size(); }
    size_t playing() const;

    // Across every session, for a status line that is about the process rather
    // than about one server.
    uint64_t bytes_received() const;
    uint64_t bytes_sent() const;

private:
    // unique_ptr and not a vector of sessions: a ClientSession holds a socket
    // and hands out pointers into its own world, so it has to keep its address
    // when the list grows.
    std::vector<std::unique_ptr<ClientSession>> m_sessions;
};

} // namespace net
} // namespace cnp
