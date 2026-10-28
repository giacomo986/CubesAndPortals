#include "core/net/session.h"

#include "core/media/face_tiles.h"

#include "core/ecs/drop.h"
#include "core/ecs/machine.h"
#include "core/physics/aabb.h"
#include "core/track/piece.h"
#include "core/voxel/raycast.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <cstdio>
#include <ctime>

namespace cnp {
namespace net {

namespace {

// Which way a player faces, as the angles every other body's Look holds and
// the snapshot sends: yaw 0 down -z (Player::forward), pitch up. It was the
// look vector's x and y in those two fields, and a client drew x as an angle:
// somebody else turned the right way only facing near -z (from LAN play).
void set_look(ecs::Look &look, float x, float y, float z)
{
    look.yaw   = std::atan2(x, -z);
    look.pitch = std::asin(std::clamp(y, -1.0f, 1.0f));
}

// How high a session's eye is over its feet: the world's, lowered while it
// crouches (Hold). What reach and every ray it casts are measured from.
float eye_of(const Session &s)
{
    const bool low = s.avatar.is_alive() && s.avatar.has<ecs::Crouching>();
    return s.movement.eye * (low ? kCrouchEye : 1.0f);
}

// The speed a crossing hands over: the way the map sends it, as fast as the
// energy it came in with allows at the height it comes out at, and out of a
// floor at least fast enough to rise half a node clear of it.
//
// The energy: a dive from one floor opening into another lost a metre and a
// half of height a bounce -- 26.0, 24.5, 22.9, 21.5, 20.4 (reported from play,
// 2026-10-05) -- because the exit is put lower than where the trigger fired
// (the eye at the exit depth, the feet under the floor) and a speed taken from
// the last step is the step's average. `in_y` is the height that speed was had
// at. The lift: a walk into a wall comes out of a floor at walking speed,
// which lifted the feet half a node and dropped the body back into the hole.
//
// `in_pos` is where that speed was had, and its height is taken as the map
// sends it to `to`: what is evened out is the step between where the map puts
// the body and where the exit does, never the height between the two openings.
// Measured from the raw height, a walk into an opening four nodes lower than
// its other end came out at no speed at all (v^2 - 8g is below zero at a
// walk), and in test chamber 04 every crossing up lost its run --
// reported from play, 2026-10-10. Into another world the raw height was that
// world's, which was meaningless besides.
Vec3 exit_velocity(const Portal &from, const Portal &to, Vec3 in_vel, Vec3 out_vel, Vec3 in_pos,
                   float out_y, float gravity)
{
    const float in_y = portal_transform_pos(from, to, in_pos).y;
    const float g = std::fabs(gravity);
    if (g > 0.0f && length_sq(out_vel) > 1e-6f) {
        const float keep = length_sq(in_vel) + 2.0f * g * (in_y - out_y);
        out_vel          = out_vel * (std::sqrt(std::max(0.0f, keep)) / length(out_vel));
    }
    if (to.horizontal() && to.normal().y > 0.5f && g > 0.0f) {
        const float rise = to.center.y + 0.5f * kNodeSize + 0.5f - out_y;
        if (rise > 0.0f) out_vel.y = std::max(out_vel.y, std::sqrt(2.0f * g * rise));
    }
    return out_vel;
}

// A stack about to leave a container on somebody's ground takes their number,
// unless it has one of its own already (a backpack, a stack marked before).
void mark_owned(Inventory &from, int slot, uint32_t tag)
{
    if (tag == 0 || slot < 0 || slot >= from.size()) return;
    ItemStack &st = from.slot(slot);
    if (!st.empty() && (st.tag & kTagIdMask) == 0) st.tag |= tag & kTagIdMask;
}

} // namespace


void ServerSessions::on_connect(PeerId peer, double now)
{
    Session s;
    s.id    = peer;
    s.state = SessionState::Pending;
    s.since = now;

    // The body, made with the session and not when the first claim arrives.
    // Two reasons: the accessors on Session then never have to ask whether it
    // exists, and a body that appears halfway through a handshake is a body
    // some code path can be looking at while another creates it.
    //
    // No Body tag yet. That is what says "draw this for other players", and a
    // session that has not said where it is has nowhere to be drawn.
    // The bag comes with the body, before the handshake decides what this
    // session is for. An observer's stays empty and nothing will ever put
    // anything in it -- the edit path refuses on scope, not on whether a bag
    // happens to exist -- which is one branch fewer everywhere else.
    s.avatar = m_ecs.entity()
                   .set<ecs::Position>({})
                   .set<ecs::Look>({})
                   .set<ecs::DrivenBy>({peer})
                   .set<Inventory>(Inventory{})
                   .set<ecs::Health>({});

    // What this body looks like, from what the world said before anybody
    // joined. Put on at creation rather than when the handshake finishes: the
    // streaming query reads it off the entity, and an avatar dressed later is
    // an avatar that goes out bare for however many ticks "later" is.
    if (m_player_model != 0) s.avatar.set<ecs::Wears>({m_player_model});

    m_sessions[peer] = s;
}

void ServerSessions::set_player_model(uint16_t model)
{
    m_player_model = model;

    // And everybody already here. A world that declares its model while people
    // are standing in it would otherwise dress the next arrival and nobody
    // else, which is a rule that depends on when a mod happened to load.
    for (auto &kv : m_sessions) {
        if (!kv.second.avatar.is_alive()) continue;
        if (model == 0) kv.second.avatar.remove<ecs::Wears>();
        else            kv.second.avatar.set<ecs::Wears>({model});
    }
}

bool ServerSessions::set_body_model(PeerId peer, uint16_t model)
{
    Session *s = find_mutable(peer);
    if (s == nullptr || !s->avatar.is_alive()) return false;

    if (model == 0) s->avatar.remove<ecs::Wears>();
    else            s->avatar.set<ecs::Wears>({model});
    return true;
}

void ServerSessions::set_binding(PeerId peer, const uint8_t *binding)
{
    auto it = m_sessions.find(peer);
    if (it == m_sessions.end()) return;

    if (binding == nullptr) {
        it->second.has_binding = false;
        return;
    }

    std::memcpy(it->second.binding, binding, kKeyBytes);
    it->second.has_binding = true;
}

// Whether this hello's identity is one the sender can prove they hold.
//
// Three answers, and the middle one is the one worth naming. A hello with no
// identity is anonymous, and whether that is allowed is the server's policy. A
// hello WITH an identity is always checked, policy or not: claiming a key you
// cannot prove is a lie whether or not this server cares who you are, and
// accepting it would let anybody wear anybody's public key.
//
// The proof is over the channel binding, so it is worth nothing anywhere except
// this session with this server. That is the whole reason it is not a signature
// over the player's name: that one would be a password, replayable forever, by
// every server that ever saw it.
bool ServerSessions::check_identity(Session &s, const Hello &hello, std::string &why)
{
    s.has_identity = false;

    if (hello.identity.empty()) {
        if (m_config.require_identity) {
            why = "this server requires a player identity and none was sent";
            return false;
        }
        return true;
    }

    // No binding means no encryption, and without encryption there is nothing
    // unique to sign: any proof would be replayable by whoever heard it. Said
    // out loud rather than accepted with a shrug -- an identity that cannot be
    // bound is not a weaker identity, it is a decoration.
    if (!s.has_binding) {
        why = "an identity cannot be proven on an unencrypted session";
        return false;
    }

    const std::vector<uint8_t> challenge = identity_challenge(s.binding, hello.player_name);
    if (!verify_message(hello.identity.data(), challenge.data(), challenge.size(),
                        hello.proof.data())) {
        why = "the signature does not match that identity";
        return false;
    }

    std::memcpy(s.identity, hello.identity.data(), kSignPublicBytes);
    s.has_identity = true;
    return true;
}

void ServerSessions::on_disconnect(PeerId peer)
{
    m_last_sent.erase(peer);
    const auto it = m_sessions.find(peer);
    if (it == m_sessions.end()) return;

    // What was in their craft grid goes back in their bag, BEFORE the body is
    // destroyed: the bag is a component of it, and where the leftovers fall is
    // read off its position. A grid is not saved and a bag is, so a stack left
    // in one when a player quits is a stack that never comes back -- and
    // somebody who quits mid-craft has not agreed to lose anything.
    empty_craft(it->second, m_now, 2, 2);

    // The body goes with the session. A leftover entity is a player standing
    // where somebody used to be, which is the oldest bug in this genre.
    if (it->second.avatar.is_alive()) it->second.avatar.destruct();

    // And so does whatever was on their screen. The same argument one line up:
    // a panel left open for a player who has gone is a row that nothing will
    // ever close, and the next session to hold that peer id would inherit it.
    if (m_panels) m_panels->drop_player((ui::PlayerId)peer);

    m_sessions.erase(it);
}

SessionAction ServerSessions::refuse_and_close(RefuseReason reason, const std::string &detail,
                                               const std::string &log) const
{
    Refuse r;
    r.reason = reason;
    r.detail = detail;

    SessionAction a;
    a.reply = r.encode();
    a.close = true;
    a.log   = log;
    return a;
}

SessionAction ServerSessions::on_message(PeerId peer, uint8_t channel, const void *data,
                                         size_t size, double now)
{
    m_now = now;

    auto it = m_sessions.find(peer);
    if (it == m_sessions.end()) {
        // A message from a peer the session table does not know. Not an assert:
        // the transport and this table can be one event apart, and the answer
        // is the same either way.
        SessionAction a;
        a.close = true;
        a.log   = "message from an unknown peer";
        return a;
    }

    Session &s = it->second;
    s.last_heard_at = now;   // anything at all counts: this is about liveness
    if (s.state == SessionState::Ready) s.spoke_since_welcome = true;

    if (s.state == SessionState::Pending) {
        // Before admission, channel 0 is the only channel that exists. A peer
        // that opens with chunk traffic is not a client that got the order
        // wrong, and treating it as one means parsing attacker-chosen bytes in
        // a parser that assumes an admitted player.
        if (channel != (uint8_t)Channel::Handshake)
            return refuse_and_close(RefuseReason::Malformed, "handshake first",
                                    "peer sent channel " + std::to_string((int)channel) +
                                        " before the handshake");

        if (peek_type(data, size) != MsgType::Hello)
            return refuse_and_close(RefuseReason::Malformed, "expected a hello",
                                    "peer opened with something that is not a hello");

        ByteReader r(data, size);
        Hello      hello;
        if (!Hello::decode(r, hello))
            return refuse_and_close(RefuseReason::Malformed, "unparseable hello",
                                    "peer sent a hello that does not parse");

        // The count of players the negotiation sees is the count of admitted
        // ones, not of open sockets: a pending peer holds a slot for at most
        // the handshake timeout, and letting it hold a *player* slot would make
        // filling the server a matter of opening eight connections.
        m_config.players   = (int)player_count();
        m_config.observers = (int)observer_count();

        // Who they are, before what they can do. Checked here rather than
        // inside negotiate() because it is the one part of a handshake that
        // needs something negotiate() has no business holding: a secret shared
        // with this peer and no other.
        std::string identity_err;
        if (!check_identity(s, hello, identity_err))
            return refuse_and_close(RefuseReason::BadIdentity, identity_err,
                                    "refused '" + hello.player_name + "': " + identity_err);

        // One name at a time. The identity is what the server tells players
        // apart by, and the name is only a label -- but it is the label every
        // other player reads, so two different keys wearing one name is
        // somebody being taken for somebody else. The same key under the same
        // name is that player joining again while their old session has not
        // gone yet, which is theirs to do.
        for (const auto &[other_peer, other] : m_sessions) {
            if (other_peer == peer || other.state == SessionState::Pending ||
                other.name != hello.player_name)
                continue;
            if (s.has_identity && other.has_identity &&
                std::memcmp(s.identity, other.identity, kSignPublicBytes) == 0)
                continue;
            return refuse_and_close(RefuseReason::BadName,
                                    "somebody else is playing as '" + hello.player_name +
                                        "' here",
                                    "refused '" + hello.player_name +
                                        "': that name is another player's right now");
        }

        // And who the name belongs to on this world, when somebody keeps one:
        // a name is the first identity's that used it here, so coming back
        // tomorrow as somebody else's name is refused too, not only wearing it
        // while they are online.
        if (m_name_check &&
            !m_name_check(hello.player_name, s.has_identity ? s.identity : nullptr))
            return refuse_and_close(RefuseReason::BadName,
                                    "'" + hello.player_name + "' is another player's name here",
                                    "refused '" + hello.player_name +
                                        "': that name belongs to another identity on this world");

        if (m_admit_check) {
            RefuseReason reason = RefuseReason::Banned;
            std::string  why;
            if (!m_admit_check(hello.player_name, s.has_identity ? s.identity : nullptr, peer,
                               reason, why))
                return refuse_and_close(reason, why,
                                        "refused '" + hello.player_name + "': " +
                                            refuse_reason_text(reason) + " (" + why + ")");
        }

        // The ticket, if one was presented -- and it is checked whether or not
        // this world requires one, for the same reason the identity is: a
        // server that ignores a credential it was handed is a server whose log
        // line is a guess.
        //
        // Only a window is admitted this way. Somebody typing this server's
        // address is not coming through a portal, and DESIGN.md 4.4's ticket is
        // about the sessions that are.
        if (hello.scope == Scope::Observe) {
            std::string ticket_err;
            if (hello.has_ticket) {
                if (!ticket_admits(hello.ticket, s, kTicketObserve, hello.ticket.portal_id,
                                   ticket_err))
                    return refuse_and_close(RefuseReason::BadTicket, ticket_err,
                                            "refused '" + hello.player_name +
                                                "' with a ticket: " + ticket_err);
                s.admitted_by_ticket = true;
                std::memcpy(s.ticket_issuer, hello.ticket.issuer, kSignPublicBytes);
            } else if (m_config.require_ticket) {
                return refuse_and_close(RefuseReason::BadTicket,
                                        "this world only lets in portals it has been vouched for",
                                        "refused '" + hello.player_name +
                                            "': no ticket, and this world requires one");
            }
        }

        const HandshakeResult res = negotiate(hello, m_config, s.admitted_by_ticket);
        if (!res.accepted)
            return refuse_and_close(res.reason, res.detail,
                                    "refused '" + hello.player_name + "': " +
                                        refuse_reason_text(res.reason) + " (" + res.detail + ")");

        s.joined_after        = (s.since > 0.0 && now > s.since) ? now - s.since : 0.0;
        s.spoke_since_welcome = false;
        s.state    = SessionState::Ready;
        s.since    = now;
        s.scope    = hello.scope;
        if (hello.livery != 0) s.avatar.set<ecs::Livery>({hello.livery});   // 93

        // How this body moves: the world's numbers, which the welcome is about
        // to tell it, until a mod gives it its own (TODO.md OPEN-47).
        s.movement        = m_config.movement;
        s.movement_before = m_config.movement;
        s.movement_since  = -1.0;
        s.name     = hello.player_name;   // validated inside negotiate()
        s.protocol = res.welcome.protocol;
        s.features = res.welcome.features;

        SessionAction a;
        Welcome welcome      = res.welcome;
        welcome.player_model = m_player_model;
        a.reply     = welcome.encode();
        a.has_joined = true;

        // What this world's blocks look like, sent with the welcome rather than
        // after it. The client has just adopted this world's block table, so
        // the names in the offer are names it can already resolve to ids; a
        // round trip in between would be a window in which the world is drawn
        // with placeholders it is about to be given the real files for.
        {
            const MediaOffer offer = media_offer_message();
            if (!offer.items.empty()) a.more.push_back(offer.encode());
        }

        a.log   = "'" + s.name + "' joined as " + scope_text(s.scope) + ", protocol " +
                std::to_string(s.protocol) + ", " + std::to_string(s.features.size()) +
                " features agreed" +
                (s.has_identity ? ", identity " + key_fingerprint(s.identity) : ", anonymous") +
                (s.admitted_by_ticket
                     ? ", on a ticket from " + key_fingerprint(s.ticket_issuer)
                     : "") +
                // How long the handshake took, because it is what decides how
                // long this session is given to answer its welcome -- and on
                // the link where that matters it is the only number either log
                // has about the link. See join_patience.
                ", handshake " + std::to_string((int)(s.joined_after * 1000.0)) + " ms";
        return a;
    }

    // Admitted. A second hello is not a client being helpful: it is a request
    // to change who this connection belongs to, halfway through a session.
    if (channel == (uint8_t)Channel::Handshake && peek_type(data, size) == MsgType::Hello)
        return refuse_and_close(RefuseReason::Malformed, "already connected",
                                "'" + s.name + "' sent a second hello");

    // Where the player says they are. Parsed here because the answer changes
    // session state -- the interest centre -- and for nothing else: it moves no
    // entity and grants no authority. That arrives with movement authority, and
    // until it does, a client lying about its position costs itself the chunks
    // it cannot see.
    if (channel == (uint8_t)Channel::Entity && peek_type(data, size) == MsgType::Move) {
        ByteReader r(data, size);
        Move       move;
        if (!Move::decode(r, move)) {
            // Not a refusal: an unreliable channel carries whatever arrives,
            // and one bad packet from a client that is otherwise fine is a
            // packet to drop, not a player to disconnect.
            SessionAction bad;
            return bad;
        }

        // Older than what we have already applied. Sequenced delivery makes
        // this rare and not impossible, and applying it would drag the interest
        // centre backwards and resend chunks the player has.
        if (s.has_moved && move.seq <= s.move_seq) {
            SessionAction stale;
            return stale;
        }

        // A claim made about a session this one no longer is. It is not old --
        // it is the newest claim there is, which is why the sequence number
        // above cannot catch it. It was made a round trip ago, before this
        // session changed from a window into a body or back, and applying it
        // means checking the centre of a portal as if it were a player standing
        // in one. Counted rather than logged every time: on a slow link there
        // is a burst of them around every crossing, and that is normal.
        if (move.scope != (uint8_t)s.scope) {
            ++s.stale_claims;
            SessionAction wrong_kind;
            if (s.stale_claims == 1 || s.stale_claims % 100 == 0)
                wrong_kind.log = "'" + s.name + "' sent a claim of the wrong kind: " +
                                 scope_text((Scope)move.scope) + " on a session that is " +
                                 scope_text(s.scope) + " (" + std::to_string(s.stale_claims) +
                                 " so far); ignored";
            return wrong_kind;
        }

        // A claim made before the client took the server's last placement of
        // this body -- a correction, a teleport. It describes where the body
        // no longer is, and refusing it would be a second correction for the
        // first, and a third for the second. Only a body's: a window is never
        // placed, and one that was a body keeps whatever it last took.
        if (s.scope == Scope::Enter && move.epoch != s.epoch) {
            ++s.stale_epochs;
            SessionAction before;
            if (s.stale_epochs == 1 || s.stale_epochs % 100 == 0)
                before.log = "'" + s.name + "' sent a claim from before it was put somewhere (" +
                             std::to_string(s.stale_epochs) + " so far); ignored";
            return before;
        }

        s.move_seq = move.seq;

        // Carried on another body: where it is is the seat's (step_riders),
        // so the claim says only where it looks.
        if (s.scope == Scope::Enter && s.riding != 0) {
            set_look(s.look(), move.look_x, move.look_y, move.look_z);
            SessionAction seated;
            return seated;
        }

        // Where they were, kept before the claim overwrites it: the portals
        // below need a velocity, and the only one the server has is the one it
        // can derive from two claims. The Move message does not carry one --
        // see cross_if_triggered for what that costs.
        const MovementState was = s.authority();

        // An observer has no body, so there is nothing to check and nothing to
        // correct: their claim is a request for chunks and that is all it is.
        // A speed cap on a session with no avatar would be a rule about a thing
        // that does not exist.
        //
        // Limit, stated here rather than discovered: DESIGN.md 4.4 wants an
        // observer's chunks confined to a sphere around the portal's exit, and
        // that sphere needs an exit point this server does not have yet. Until
        // it does, an observer can stream the whole world by claiming their way
        // across it -- which is the reason the setting to refuse observers
        // exists at all.
        if (s.scope == Scope::Observe) {
            s.has_moved = true;
            s.avatar.remove<ecs::Body>();   // a window has none, ever
            set_look(s.look(), move.look_x, move.look_y, move.look_z);
            s.interest.set_position(move.x, move.y, move.z, move.look_x, move.look_y,
                                    move.look_z);
            s.interest.forget_distant();

            SessionAction watched;
            return watched;
        }

        // The authority. Without a world to check against there is none, and
        // the claim is taken as given -- which is what a server that never
        // called set_world has silently signed up for.
        Drive drive = Drive::Walks;
        if (has_authority()) {
            // A kart, which the server drives too (TODO.md OPEN-47): judged by
            // where its own copy of the chassis went with the same keys, and
            // not by the rules below -- a copy that agrees has kept them all,
            // and inertia and the wheel with them, which the rules below
            // cannot see.
            std::string drive_why;
            if (s.scope == Scope::Enter) drive = drive_claim(s, move, now, drive_why);
            if (drive == Drive::Refuse) {
                ++s.corrections;

                // To where the server's copy is, at its speed and heading, and
                // both ends put the chassis down afresh there. With no copy yet,
                // back to where the body last was, stopped.
                DriveShadow &d = s.drive;
                Correct      c;
                c.seq   = move.seq;
                c.flags = kCorrectDrive;
                c.nitro = s.drive.body.nitro;
                if (d.body.vehicle) {
                    c.x = d.body.pos.x; c.y = d.body.pos.y; c.z = d.body.pos.z;
                    c.vx = d.body.velocity.x; c.vy = d.body.velocity.y; c.vz = d.body.velocity.z;
                    c.yaw = d.body.yaw;
                } else {
                    c.x = s.authority().x; c.y = s.authority().y; c.z = s.authority().z;
                    c.yaw = std::atan2(move.look_x, -move.look_z);
                }
                c.reason = drive_why;
                c.epoch  = placed(s, Vec3{c.x, c.y, c.z}, Vec3{c.vx, c.vy, c.vz}, c.yaw, true);

                s.authority().x      = c.x;
                s.authority().y      = c.y;
                s.authority().z      = c.z;
                s.authority().when   = now;
                s.authority().valid  = true;
                s.authority().placed = true;

                SessionAction refused;
                refused.reply         = c.encode();
                refused.reply_channel = (uint8_t)Channel::Event;
                if (s.corrections == 1 || s.corrections % 100 == 0)
                    refused.log = "'" + s.name + "' corrected (" +
                                  std::to_string(s.corrections) + " so far): " + c.reason;
                return refused;
            }

            // The nodes holding floor portals up come with the claim: without
            // them a player sinking through a portal they are standing on is
            // refused for being inside the block that holds it.
            SolidExemption   exempt[kMaxSolidExemptions];
            const size_t     exempt_count = floor_exemptions(exempt);

            // This body's limits and not the world's: a mod may have given it
            // numbers of its own (TODO.md OPEN-47).
            MovementVerdict v;
            if (drive == Drive::Accept) {
                // Where the copy is, with the budgets full for whatever claim
                // is judged by the rules next.
                const MovementLimits lim = limits_of(s, now);
                v.accepted = true;
                v.x = move.x; v.y = move.y; v.z = move.z;
                v.budget_h = lim.max_horizontal * lim.tolerance * (float)lim.max_burst;
                v.budget_v = lim.max_vertical * lim.tolerance * (float)lim.max_burst;
                v.ground_y = s.drive.body.on_ground ? move.y : s.authority().ground_y;
            } else {
                v = check_move(s.authority(), move.x, move.y, move.z, now, *m_world, *m_registry,
                               limits_of(s, now), exempt, exempt_count);
            }
            // The budget follows the claim either way: a refused claim spent
            // nothing, and writing it back is what stops a refusal from also
            // costing the player the allowance they never used.
            s.authority().budget_h = v.budget_h;
            s.authority().budget_v = v.budget_v;

            if (!v.accepted) {
                ++s.corrections;

                Correct c;
                c.seq    = move.seq;
                c.x      = v.x;
                c.y      = v.y;
                c.z      = v.z;
                c.reason = v.reason;
                // A kart stopped there, heading where the claim did, its chassis
                // put down afresh by both ends.
                const bool drives = s.movement.drives();
                if (drives) {
                    c.flags = kCorrectDrive;
                    c.nitro = s.drive.body.nitro;
                    c.yaw   = std::atan2(move.look_x, -move.look_z);
                }
                c.epoch = placed(s, Vec3{c.x, c.y, c.z}, Vec3{}, c.yaw, drives);

                SessionAction refused;
                refused.reply         = c.encode();
                refused.reply_channel = (uint8_t)Channel::Event;
                // Logged on the first one and then every hundredth: one is a
                // lag spike, four hundred is a modified client, and a line per
                // refusal is how a log stops being read.
                if (s.corrections == 1 || s.corrections % 100 == 0)
                    refused.log = "'" + s.name + "' corrected (" +
                                  std::to_string(s.corrections) + " so far): " + v.reason;
                return refused;
            }

            s.authority().x        = v.x;
            s.authority().y        = v.y;
            s.authority().z        = v.z;
            s.authority().when     = now;
            s.authority().valid    = true;
            s.authority().ground_y = v.ground_y;
            // A claim of its own: from here the body has been somewhere, and
            // the next claim is walked from it (TODO.md OPEN-46).
            s.authority().placed = false;
            s.flying = (move.flags & kMoveFlying) && s.movement.fly > 0.0f;

            // What that claim cost. Only ACCEPTED positions are watched: a
            // refused one is a claim about a place this body was never in, and
            // charging a fall to it would let a modified client hurt itself
            // into the ground with numbers the server had already thrown away.
            //
            // Below the world first, because a body down there is in a fall
            // that has no bottom, and waiting for it to land is waiting
            // forever.
            if (s.scope == Scope::Enter) {
                if (v.y < kBelowWorld) {
                    kill_body(s, now);
                } else if ((move.flags & kMoveFlying) && s.movement.fly > 0.0f &&
                           s.movement.accel <= 0.0f) {
                    // Flown here, on a world where this body flies: a flight
                    // down to the ground is not a fall, and one that ends in
                    // the air is a fall that starts where it ended.
                    fall_reset(s.fall, v.y);
                } else if (has_authority() &&
                           m_registry->is_liquid(m_world->get_block((int)std::floor(v.x),
                                                                    (int)std::floor(v.y),
                                                                    (int)std::floor(v.z)))) {
                    // In water: whatever fall this was ends in it, for nothing.
                    fall_reset(s.fall, v.y);
                } else if (has_authority() &&
                           box_on_climbable(*m_world, *m_registry,
                                            body_box(Vec3{v.x, v.y, v.z}, m_limits.half_width))) {
                    // On a ladder: held, so a climb down is not a fall.
                    fall_reset(s.fall, v.y);
                } else if (has_authority() && chute_at(*m_world, *m_registry, v.x, v.y, v.z, true).on) {
                    // Riding a slide: held by it all the way down.
                    fall_reset(s.fall, v.y);
                } else if (has_authority() &&
                           body_at_ledge(*m_world, *m_registry, Vec3{v.x, v.y, v.z},
                                         m_limits.half_width)) {
                    // Hanging from a ledge: the fall ended in the hands.
                    fall_reset(s.fall, v.y);
                } else {
                    const float fell = fall_step(s.fall, v.y);
                    if (fell > 0.0f) hurt_body(s, fall_damage(fell), now, false);
                }
            }
        }

        s.has_moved = true;
        // And now there is something to draw. Before the first claim there is a
        // session and no body: the tag is what the streaming query looks for, so
        // adding it here is the same statement the old `if (!other.has_moved)`
        // made, said once instead of at every place that reads the list.
        s.avatar.add<ecs::Body>();
        set_look(s.look(), move.look_x, move.look_y, move.look_z);
        s.heading          = s.drive.body.vehicle ? s.drive.body.yaw
                                                  : std::atan2(move.look_x, -move.look_z);
        s.interest.set_position(move.x, move.y, move.z, move.look_x, move.look_y, move.look_z);
        aim_windows(s, Vec3{move.x, move.y, move.z});
        s.interest.forget_distant();

        // The openings, after the claim has been accepted and the interest
        // moved: a crossing is a thing that happens to a player who is
        // somewhere legal, and the arrival becomes the new somewhere.
        // A pair that leads to another server is not one this server can walk
        // anybody through: the destination is somewhere it has no world for.
        // The crossing there is a swap of sessions on the client and a join on
        // the other side, and neither is a thing to do by moving a player's
        // coordinates. Until that exists, the mouth is something to look
        // through and not something to fall into -- said here rather than left
        // to be discovered as a teleport into the wrong world.
        // A world with mouths may also hold pairs of its own (local_pair),
        // and those are crossed here like any other world's.
        if (leads_away() && has_authority() && s.scope == Scope::Enter) {
            // The mouth. This server has no world on the other side to move
            // anybody into, so what it sends is the address instead of a
            // position. A client with a window already open onto that world
            // ignores it and crosses without a reconnect; one without takes it.
            SessionAction sent = redirect_if_triggered(s, move, was, now);
            if (!sent.reply.empty()) return sent;
        }
        if (has_portals() && has_authority()) {
            SessionAction crossed = cross_if_triggered(s, move, was, now);
            if (!crossed.reply.empty()) return crossed;
        }

        SessionAction moved;
        if (drive == Drive::Accept && (++s.drive.driven == 1 || s.drive.driven % 1000 == 0))
            moved.log = "'" + s.name + "' drives a kart the server drives too (" +
                        std::to_string(s.drive.driven) + " claims so far)";

        // The server's copy of the kart lost the thread -- records lost on the
        // way, or steps of a chassis it never saw put down -- and the claim
        // was fine by the rules: both ends put the chassis down afresh where
        // the claim is, at its speed, which gives the copy a thread again.
        if (drive == Drive::Resync) {
            Correct c;
            c.seq    = move.seq;
            c.x      = move.x;
            c.y      = move.y;
            c.z      = move.z;
            c.flags  = kCorrectDrive;
            c.nitro  = s.drive.body.nitro;
            c.vx     = move.vx;
            c.vy     = move.vy;
            c.vz     = move.vz;
            c.yaw    = std::atan2(move.look_x, -move.look_z);
            c.reason = "the server lost the thread of your kart";
            c.epoch  = placed(s, Vec3{c.x, c.y, c.z}, Vec3{c.vx, c.vy, c.vz}, c.yaw, true);
            ++s.drive.resyncs;
            moved.reply         = c.encode();
            moved.reply_channel = (uint8_t)Channel::Event;
            if (s.drive.resyncs == 1 || s.drive.resyncs % 100 == 0)
                moved.log = "'" + s.name + "' put down afresh (" +
                            std::to_string(s.drive.resyncs) + " so far): " + c.reason;
        }
        return moved;
    }

    // A client asking this world to vouch for it at the world its opening
    // leads to. Reliable, and answered either way: a client that asked and was
    // told nothing has to wait to find out, which is the defect ScopeDenied
    // exists to fix and there is no reason to reintroduce it here.
    if (channel == (uint8_t)Channel::Media && peek_type(data, size) == MsgType::MediaNeed) {
        ByteReader r(data, size);
        MediaNeed  req;
        if (!MediaNeed::decode(r, req)) {
            SessionAction a;
            a.log = "'" + s.name + "' sent a media request that does not parse";
            return a;
        }

        SessionAction a;
        a.reply_channel = (uint8_t)Channel::Media;

        // The hashes this world cannot serve, answered rather than ignored.
        //
        // Saying nothing left the client unable to tell "not coming" from
        // "slow", and the only tool it had then was a timeout -- which on a
        // lossy link fires on files that were merely late, and re-asking for
        // the batch while the first copy is still queued cost 55 files for 31
        // distinct at 400 ms and 25% loss. Same defect as a crossing with no
        // ScopeDenied behind it, and the same cure.
        MediaMissing missing;

        if (!m_media) {
            // Nothing offered, so nothing asked for can be served. A client
            // that asks a world with no media for a file is acting on somebody
            // else's offer -- and is still owed an answer.
            s.media_unknown += (long)req.hashes.size();
            missing.hashes = req.hashes;
            a.more.push_back(missing.encode());
            return a;
        }

        size_t served = 0;
        for (const ContentHash &h : req.hashes) {
            const MediaBlob *blob = m_media->find(h);
            if (!blob) {
                ++s.media_unknown;
                missing.hashes.push_back(h);
                continue;
            }
            MediaData msg;
            msg.hash  = blob->hash;
            msg.bytes = blob->bytes;
            a.more.push_back(msg.encode());

            ++s.media_sent;
            s.media_bytes += blob->bytes.size();
            ++served;
        }

        if (!missing.hashes.empty()) a.more.push_back(missing.encode());

        // One line per request, not per file. Thirty-one of those on every join
        // is a log nobody reads, which is the same failure as no log at all.
        a.log = "'" + s.name + "' asked for " + std::to_string(req.hashes.size()) +
                " media file(s), sent " + std::to_string(served);
        return a;
    }

    if (channel == (uint8_t)Channel::Event && peek_type(data, size) == MsgType::TicketRequest) {
        ByteReader    r(data, size);
        TicketRequest req;
        if (!TicketRequest::decode(r, req)) {
            SessionAction bad;
            bad.log = "'" + s.name + "' sent a ticket request that does not parse";
            return bad;
        }

        SessionAction a;
        Ticket        answer;
        RefuseReason  reason = RefuseReason::None;
        std::string   why;

        answer.granted = issue_ticket(peer, req, now, answer.ticket, reason, why);
        if (!answer.granted) {
            answer.reason = reason;
            answer.detail = why;
            a.log = "'" + s.name + "' asked for a ticket and does not get one: " + why;
        } else if (s.tickets_issued == 1 || s.tickets_issued % 20 == 0) {
            // One per portal is ordinary; a number that climbs is a client
            // asking faster than a portal needs one. Same rule as a correction.
            const PortalAway &away = m_away[req.opening < kMaxMouths ? req.opening : 0];
            a.log = "'" + s.name + "' issued a ticket for " + away.host + ":" +
                    std::to_string(away.port) + " opening " +
                    std::to_string(answer.ticket.portal_id) + " (" +
                    std::to_string(s.tickets_issued) + " so far)";
        }

        a.reply         = answer.encode();
        a.reply_channel = (uint8_t)Channel::Event;
        return a;
    }

    // A session changing what it is for.
    if (channel == (uint8_t)Channel::Event && peek_type(data, size) == MsgType::ChangeScope) {
        ByteReader  r(data, size);
        ChangeScope req;
        if (!ChangeScope::decode(r, req)) {
            SessionAction bad;
            bad.log = "'" + s.name + "' sent a scope change that does not parse";
            return bad;
        }

        SessionAction a;
        Teleport      arrived;
        RefuseReason  reason = RefuseReason::None;
        std::string   why;
        if (!change_scope(peer, req, now, arrived, reason, why)) {
            // Said out loud rather than left as silence. A client whose crossing
            // is refused and told nothing has to guess by waiting, and every
            // second it waits is a second the player stands in an opening that
            // is not taking them. This does not close the session: the request
            // failed, the session is fine, and it stays whatever it already was.
            ScopeDenied no;
            no.scope        = req.scope;
            no.reason       = reason;
            no.detail       = why;
            a.reply         = no.encode();
            a.reply_channel = (uint8_t)Channel::Event;
            a.log = "'" + s.name + "' cannot become a " + scope_text((Scope)req.scope) + ": " + why;
            return a;
        }

        a.has_entered     = (Scope)req.scope == Scope::Enter;
        a.has_stepped_out = !a.has_entered;
        if ((Scope)req.scope == Scope::Enter) {
            a.reply         = arrived.encode();
            a.reply_channel = (uint8_t)Channel::Event;
            a.log = "'" + s.name + "' came out of this world's opening " +
                    std::to_string(req.portal_id) + " at " +
                    std::to_string((int)arrived.x) + " " + std::to_string((int)arrived.y) + " " +
                    std::to_string((int)arrived.z);
        } else {
            a.log = "'" + s.name + "' stepped back out of this world and is watching it";
        }
        return a;
    }

    // A player putting an opening on a wall. Reliable, so a malformed one is a
    // client that is broken rather than a packet that got mangled -- and it is
    // still dropped rather than punished, because the alternative is
    // disconnecting somebody over one byte.
    if (channel == (uint8_t)Channel::Event && peek_type(data, size) == MsgType::PlacePortal) {
        ByteReader  r(data, size);
        PlacePortal req;
        if (!PlacePortal::decode(r, req)) {
            SessionAction bad;
            bad.log = "'" + s.name + "' sent a placement that does not parse";
            return bad;
        }

        SessionAction a;
        std::string   why;

        // An observer is a window, not a hand. Refused here rather than inside
        // place_portal, because the reason is about the session and not about
        // the placement: nothing an observer sends may change the world.
        if (s.scope == Scope::Observe) {
            ++s.placements_refused;
            if (s.placements_refused == 1)
                a.log = "'" + s.name + "' is watching, not playing, and cannot place anything";
            return a;
        }

        if (!place_portal(peer, req, why)) {
            ++s.placements_refused;
            // Same rule as a correction: one is a player clicking at the sky,
            // and a line per refusal is how a log stops being read.
            if (s.placements_refused == 1 || s.placements_refused % 100 == 0)
                a.log = "'" + s.name + "' placement refused (" +
                        std::to_string(s.placements_refused) + " so far): " + why;
            return a;
        }

        a.log = "'" + s.name + "' put opening " + std::to_string((int)(req.which & 1) + 1) +
                " on the face of " + std::to_string(req.x) + " " + std::to_string(req.y) + " " +
                std::to_string(req.z);
        return a;
    }

    if (channel == (uint8_t)Channel::Event && peek_type(data, size) == MsgType::CloseContainer) {
        ByteReader     r(data, size);
        CloseContainer req;
        if (!CloseContainer::decode(r, req)) {
            SessionAction bad;
            bad.log = "'" + s.name + "' sent a chest close that does not parse";
            return bad;
        }

        // No scope check and no refusal: shutting something you have open is
        // the one request that cannot be wrong. A session with nothing open
        // closes nothing, which is what close_container already does.
        //
        // BOTH of the things that can be above a bag, because there is one
        // gesture for it -- a player presses escape and expects the screen to
        // go. A second message for the table would be a second thing the client
        // has to decide between for one key, and getting that wrong leaves the
        // server holding a table open that nothing on screen shows.
        close_container(peer);
        close_craft(peer, now);

        SessionAction a;
        return a;
    }

    if (channel == (uint8_t)Channel::Event && peek_type(data, size) == MsgType::MoveItem) {
        ByteReader r(data, size);
        MoveItem   req;
        if (!MoveItem::decode(r, req)) {
            SessionAction bad;
            bad.log = "'" + s.name + "' sent a slot move that does not parse";
            return bad;
        }

        SessionAction a;
        a.reply_channel = (uint8_t)Channel::Event;

        // An observer has a bag and nothing ever puts anything in it, so there
        // is nothing here to rearrange. Refused on scope like every other reach
        // into the world, and answered with the bag it does have -- every
        // request gets a reply.
        if (s.scope != Scope::Enter) {
            a.reply = inventory_message(peer);
            return a;
        }

        // Taking the result, which is not a move at all: it is "make that, and
        // pay for it out of the grid". Answered first, because every check
        // below is about two squares and this one has only one.
        if (req.from_side == (uint8_t)Side::Craft && req.from == kCraftResultSlot) {
            // The square the drag ended on is ignored, and craft_take says why:
            // a result of four dragged onto a slot holding sixty-two of the
            // same thing has nowhere to put two of them, so it goes wherever it
            // fits or nowhere at all.
            if (m_items && m_recipes && craft_take(*m_items, *m_recipes, s.craft, s.bag()))
                s.craft_owed = true;

            a.reply = inventory_message(peer);
            return a;
        }

        // Which inventory each end of the move is in. Side::Container is "the
        // one I have open", resolved HERE against a cell the server holds --
        // the message never names a place, so there is no place in it to lie
        // about. Side::Craft is the same shape: the server holds the size, so
        // there is no size in the message either.
        Inventory *from_bag = &s.bag();
        Inventory *to_bag   = &s.bag();

        if (req.from_side == (uint8_t)Side::Container ||
            req.to_side == (uint8_t)Side::Container) {
            Inventory *chest = (s.container_open && m_containers)
                                   ? m_containers->find(s.container)
                                   : nullptr;

            // Naming a chest with none open changes nothing, and is still
            // answered: every request gets a reply, and a client whose picture
            // drifted is put straight by it.
            if (chest == nullptr) {
                a.reply = inventory_message(peer);
                return a;
            }

            if (req.from_side == (uint8_t)Side::Container) from_bag = chest;
            if (req.to_side == (uint8_t)Side::Container) to_bag = chest;
        }

        if (req.from_side == (uint8_t)Side::Craft || req.to_side == (uint8_t)Side::Craft) {
            // The grid is nine slots of which w * h are used, so a square past
            // the end of the CURRENT size is refused rather than clamped: it
            // would take a stack out of sight, where nothing draws it and no
            // recipe reads it, and give it back on the next resize in a
            // different square.
            const int used = s.craft.used();
            if ((req.from_side == (uint8_t)Side::Craft && (int)req.from >= used) ||
                (req.to_side == (uint8_t)Side::Craft && (int)req.to >= used)) {
                a.reply = inventory_message(peer);
                return a;
            }

            if (req.from_side == (uint8_t)Side::Craft) from_bag = &s.craft.slots;
            if (req.to_side == (uint8_t)Side::Craft) to_bag = &s.craft.slots;
        }

        // The arithmetic is core/item's, and it either happens or it does not:
        // a move that half-happens is how an inventory loses items. Either way
        // the answer is the WHOLE bag, so a client whose picture drifted for
        // any reason is put straight by the next thing it does.
        if (from_bag != &s.bag() && from_bag != &s.craft.slots)
            mark_owned(*from_bag, (int)req.from, owned_tag_for(s, s.container.x, s.container.y, s.container.z));
        bool moved = false;
        if (m_items)
            moved = move_between(*m_items, *from_bag, (int)req.from, *to_bag, (int)req.to,
                                 req.count);

        // Everybody with that chest open is now behind, this session included.
        // One Inventory and several viewers is the whole point of a container
        // -- see net::ContainerState.
        if (moved && s.container_open && m_containers &&
            (req.from_side == (uint8_t)Side::Container ||
             req.to_side == (uint8_t)Side::Container))
            m_containers->touched(s.container);

        // And the grid, which nobody but this player can see. What it MAKES
        // changed too, and that travels with it.
        if (moved && (req.from_side == (uint8_t)Side::Craft ||
                      req.to_side == (uint8_t)Side::Craft))
            s.craft_owed = true;

        a.reply = inventory_message(peer);
        return a;
    }

    if (channel == (uint8_t)Channel::Event && peek_type(data, size) == MsgType::QuickMove) {
        ByteReader r(data, size);
        QuickMove  req;
        if (!QuickMove::decode(r, req)) {
            SessionAction bad;
            bad.log = "'" + s.name + "' sent a shift-click that does not parse";
            return bad;
        }

        SessionAction a;
        a.reply_channel = (uint8_t)Channel::Event;

        // An observer has a bag nothing ever puts anything in. Refused on scope
        // like every other reach into the world, and still answered.
        if (s.scope != Scope::Enter) {
            a.reply = inventory_message(peer);
            return a;
        }

        // The answer square, which is not a square: make it until the grid runs
        // out or the bag does. Answered first, because everything below is
        // about two inventories and this is about one.
        if (req.side == (uint8_t)Side::Craft && req.slot == kCraftResultSlot) {
            if (m_items && m_recipes &&
                craft_take_all(*m_items, *m_recipes, s.craft, s.bag()) > 0)
                s.craft_owed = true;

            a.reply = inventory_message(peer);
            return a;
        }

        // There is deliberately NO check that the square is inside the current
        // size, though MoveItem has one. A move needs it because a stack can be
        // put INTO a square past the end, where nothing draws it; a shift-click
        // only takes, and a square past the end is always empty -- craft_resize
        // empties the grid every time the size changes -- so quick_move refuses
        // it for having nothing in it. The check written here as well was a
        // second guard on the same rule, and the sabotage that should have
        // turned it red stayed green: neither could be shown to be doing the
        // work. It is gone, and the loopback gate still says a square past the
        // end sends nothing anywhere.
        // The bag with nothing open in front of it: a piece of armour goes on,
        // or comes off (Inventory::swap_worn).
        if (req.side == (uint8_t)Side::Bag && !s.container_open) {
            if (m_items) s.bag().swap_worn(*m_items, (int)req.slot);
            a.reply = inventory_message(peer);
            return a;
        }

        Side to = Side::Bag;
        if (!quick_target((Side)req.side, s.container_open, to)) {
            a.reply = inventory_message(peer);
            return a;
        }

        Inventory *chest =
            (s.container_open && m_containers) ? m_containers->find(s.container) : nullptr;

        const auto pick = [&](Side which) -> Inventory * {
            switch (which) {
            case Side::Container: return chest;
            case Side::Craft: return &s.craft.slots;
            case Side::Bag: break;
            }
            return &s.bag();
        };

        Inventory *from_bag = pick((Side)req.side);
        Inventory *to_bag   = pick(to);

        if (from_bag != nullptr && from_bag == chest)
            mark_owned(*from_bag, (int)req.slot, owned_tag_for(s, s.container.x, s.container.y, s.container.z));
        bool moved = false;
        if (m_items && from_bag != nullptr && to_bag != nullptr)
            moved = quick_move(*m_items, *from_bag, (int)req.slot, *to_bag);

        // Everybody with that chest open is now behind, this session included.
        if (moved && chest != nullptr && (from_bag == chest || to_bag == chest) && m_containers)
            m_containers->touched(s.container);

        // And the grid, which nobody but this player can see. What it MAKES
        // changed too, and that travels with it.
        if (moved && req.side == (uint8_t)Side::Craft) s.craft_owed = true;

        a.reply = inventory_message(peer);
        return a;
    }

    if (channel == (uint8_t)Channel::Event && peek_type(data, size) == MsgType::DropItem) {
        ByteReader r(data, size);
        DropItem   req;
        if (!DropItem::decode(r, req)) {
            SessionAction bad;
            bad.log = "'" + s.name + "' sent a drop that does not parse";
            return bad;
        }

        SessionAction a;
        a.reply_channel = (uint8_t)Channel::Event;

        // Same scope rule as every other reach into the world, and the same
        // answer either way: a window looks in and does not put things down.
        if (s.scope != Scope::Enter || !m_items || !s.has_moved) {
            a.reply = inventory_message(peer);
            return a;
        }

        // A back held on by its arms stays on (Inventory::pinned).
        const ItemStack took = s.bag().pinned(*m_items, (int)req.slot)
                                   ? ItemStack{}
                                   : s.bag().take_from((int)req.slot, req.count);
        if (!took.empty()) {
            // In front of the player and at eye height, from the direction the
            // server already holds -- see Interest::look. Not from a point the
            // message carried: that is a field a modified client fills with
            // anything, and the server would have to check it against a reach
            // it already knows.
            const MovementState &at = s.authority();

            float lx = 0.0f, ly = 0.0f, lz = -1.0f;
            s.interest.look(lx, ly, lz);

            // Flattened and normalised: a player looking at their feet should
            // throw a stack forward, not into the floor they are standing on.
            const float len = std::sqrt(lx * lx + lz * lz);
            if (len > 1e-3f) { lx /= len; lz /= len; } else { lx = 0.0f; lz = -1.0f; }
            (void)ly;

            ecs::spawn_drop(m_ecs, took, at.x + lx * ecs::kThrowAhead,
                            at.y + eye_of(s), at.z + lz * ecs::kThrowAhead, now);
        }

        a.reply = inventory_message(peer);
        return a;
    }

    if (channel == (uint8_t)Channel::Event && peek_type(data, size) == MsgType::Hold) {
        ByteReader r(data, size);
        Hold       req;
        if (!Hold::decode(r, req)) {
            SessionAction bad;
            bad.log = "'" + s.name + "' sent a hand that does not parse";
            return bad;
        }
        // Only when there is a body to hold it: a window has no hands.
        if (s.avatar.is_alive() && s.avatar.has<Inventory>()) {
            s.bag().select((int)req.slot);
            if (req.working) s.avatar.add<ecs::Working>();
            else             s.avatar.remove<ecs::Working>();
            if (req.crouching) s.avatar.add<ecs::Crouching>();
            else               s.avatar.remove<ecs::Crouching>();
        }
        return SessionAction{};
    }

    if (channel == (uint8_t)Channel::Event && peek_type(data, size) == MsgType::Dress) {
        ByteReader r(data, size);
        Dress      req;
        if (!Dress::decode(r, req)) {
            SessionAction bad;
            bad.log = "'" + s.name + "' sent a livery that does not parse";
            return bad;
        }
        if (s.avatar.is_alive()) {
            if (req.livery != 0) s.avatar.set<ecs::Livery>({req.livery});
            else                 s.avatar.remove<ecs::Livery>();
        }
        return SessionAction{};
    }

    if (channel == (uint8_t)Channel::Event && peek_type(data, size) == MsgType::Hit) {
        ByteReader r(data, size);
        Hit        req;
        if (!Hit::decode(r, req)) {
            SessionAction bad;
            bad.log = "'" + s.name + "' sent a hit that does not parse";
            return bad;
        }

        SessionAction a;
        std::string   why;
        if (req.use) {
            // A right click on a body. What it may have spent goes back as the
            // bag, the way a throw's does; a refusal is a click at nothing.
            if (flecs::entity t = m_ecs.entity((flecs::entity_t)req.target);
                t.is_alive() && t.has<ecs::Act>()) {
                // A worker: nothing the engine does, its mod's to answer.
                const ecs::Position *tp = t.try_get<ecs::Position>();
                if (tp != nullptr && in_reach(s, t, *tp, why)) {
                    const ItemStack &held = s.bag().at(req.slot < Inventory::kHotbar ? req.slot : 0);
                    a.has_used_mob = true;
                    a.used_mob     = (uint32_t)req.target;
                    a.used_held    = held.empty() ? kNoItem : held.item;
                }
            } else if (use_body(s, req.target, req.slot, now, why)) {
                a.reply_channel = (uint8_t)Channel::Event;
                a.reply         = inventory_message(peer);
            } else if (flecs::entity t = m_ecs.entity((flecs::entity_t)req.target);
                       t.is_alive() && t.has<ecs::Mob>() && s.scope == Scope::Enter &&
                       t.try_get<ecs::Position>() != nullptr &&
                       in_reach(s, t, *t.try_get<ecs::Position>(), why)) {
                // A creature the engine did nothing with: its mod's to answer
                // (a cow milked, a sheep dyed), as for a worker.
                const ItemStack &held = s.bag().at(req.slot < Inventory::kHotbar ? req.slot : 0);
                a.has_used_mob = true;
                a.used_mob     = (uint32_t)req.target;
                a.used_held    = held.empty() ? kNoItem : held.item;
            } else if (++s.hits_refused == 1 || s.hits_refused % 100 == 0) {
                a.log = "'" + s.name + "' used nothing (" + std::to_string(s.hits_refused) +
                        " so far): " + why;
            }
            return a;
        }
        const HitOutcome out = strike(s, req.target, req.slot, now, why);

        if (!out.landed) {
            ++s.hits_refused;
            // The same rule the placements keep: a swing at nothing is what a
            // player does, and a line per swing is how a log stops being read.
            if (s.hits_refused == 1 || s.hits_refused % 100 == 0)
                a.log = "'" + s.name + "' hit nothing (" + std::to_string(s.hits_refused) +
                        " so far): " + why;
            return a;
        }

        // A blow spends a use of what struck it: the hand's, as a dig does.
        if (m_items && s.bag().wear_slot(*m_items, req.slot < Inventory::kHotbar ? req.slot : 0, 1)) {
            a.reply_channel = (uint8_t)Channel::Event;
            a.reply         = inventory_message(peer);
        }

        a.has_hit    = true;
        a.hit_target = req.target;
        a.hit_taken  = out.taken;
        a.hit_killed = out.killed;

        // Only the kill. A blow that lands is the ordinary case and happens
        // every 0.4 s for as long as somebody holds the button down.
        if (out.killed)
            a.log = "'" + s.name + "' killed body " + std::to_string(req.target);
        return a;
    }

    if (channel == (uint8_t)Channel::Event && peek_type(data, size) == MsgType::Chat) {
        ByteReader r(data, size);
        Chat       req;
        if (!Chat::decode(r, req)) {
            SessionAction bad;
            bad.log = "'" + s.name + "' sent a chat line that does not parse";
            return bad;
        }

        // A window onto this world has no mouth, and a line is printable text
        // on one line: a control character would draw as nothing or break the
        // log it is written to.
        SessionAction a;
        if (s.scope != Scope::Enter || now - s.chat_at < kChatGap) return a;
        std::string &t = req.text;
        t.erase(0, t.find_first_not_of(' '));
        t.erase(t.find_last_not_of(' ') + 1);
        if (t.empty() || t.size() > kMaxChatLine) return a;
        for (const char c : t)
            if ((uint8_t)c < 32 || c == 127) return a;

        s.chat_at  = now;
        a.has_chat = true;
        a.chat     = std::move(t);
        return a;
    }

    if (channel == (uint8_t)Channel::Event && peek_type(data, size) == MsgType::UiClick) {
        ByteReader r(data, size);
        UiClick    req;
        if (!UiClick::decode(r, req)) {
            SessionAction bad;
            bad.log = "'" + s.name + "' sent a panel click that does not parse";
            return bad;
        }

        SessionAction a;

        // The search box beside the chest this player has open, which is the
        // engine's and no mod's panel: good only while that chest has one.
        if (req.panel == "core:chest") {
            std::string q;
            if (s.scope == Scope::Enter && req.id == "search" && s.container_open && m_containers &&
                m_containers->search(s.container, q)) {
                a.has_ui_click = true;
                a.ui_click     = req;
            }
            return a;
        }

        // The panel that is actually up, not the one the message names. A
        // client whose panel is a moment out of date -- or is not this client
        // at all -- may name a panel that closed, and a mod that got the event
        // anyway would be answering a button nobody could see.
        const ui::Panel *up = m_panels ? m_panels->topmost_for((ui::PlayerId)peer) : nullptr;
        if (s.scope != Scope::Enter || up == nullptr || up->name != req.panel) return a;

        // And the button has to be ON it, and be a button. This is where the
        // ids earn their place over an index: the panel here is whatever the
        // mod last declared, so a click for a button that redefinition removed
        // finds nothing and stops, instead of landing on whatever now sits at
        // that position.
        bool found = req.id == ui::kPanelEscape;
        for (const ui::Element &e : up->elements) {
            if ((e.kind == ui::ElemKind::Button || e.kind == ui::ElemKind::Field ||
                 e.kind == ui::ElemKind::Item) &&
                e.id == req.id) {
                found = true;
                break;
            }
        }
        if (!found) return a;

        a.has_ui_click = true;
        a.ui_click     = req;
        return a;
    }

    if (channel == (uint8_t)Channel::Event && peek_type(data, size) == MsgType::Edit) {
        ByteReader r(data, size);
        Edit       req;
        if (!Edit::decode(r, req)) {
            SessionAction bad;
            bad.log = "'" + s.name + "' sent an edit that does not parse";
            return bad;
        }

        SessionAction a;
        EditPlan      plan;
        std::string   why;

        const bool allowed = plan_edit(peer, req, plan, why, now);

        // The use comes out whatever happened to the placement, because they
        // are two questions about one click: what did you touch, and what did
        // you want to put down. A mod that CANCELS the use has claimed the
        // click, and whoever owns the mod host is what knows that -- so both
        // halves go up and the decision is made there.
        // Except with Shift held and a placement that goes through: then the
        // click is only a placement, and the chest under it stays shut.
        if (plan.use && !(req.place_only && allowed)) {
            a.has_use = true;
            a.use     = plan;
        }

        if (!allowed) {
            // An empty sentence is not a refusal: it is "there was nothing to
            // put down", which is every right click with an empty hand. The
            // player asked for a use and got one; there is nothing to answer.
            if (why.empty()) return a;

            ++s.edits_refused;

            // Every edit gets an answer, refused or not -- M6_FINDINGS 25. The
            // answer to a refusal is the block that is STILL there, so a client
            // that drew the change optimistically puts it back and one that did
            // not is simply told nothing changed. And it carries the sentence,
            // because a refusal the player cannot read sends them to look in
            // the wrong place (M6_FINDINGS 31).
            //
            // Held rather than sent, because whether the player should see it
            // depends on the use: a mod that claimed the click was not refusing
            // anything, and a refusal after a chest opened is an answer to a
            // question nobody asked.
            a.edit_refusal = why;
            a.refused_x    = plan.x;
            a.refused_y    = plan.y;
            a.refused_z    = plan.z;

            if (s.edits_refused == 1 || s.edits_refused % 100 == 0)
                a.log = "'" + s.name + "' edit refused (" + std::to_string(s.edits_refused) +
                        " so far): " + why;
            return a;
        }

        // Allowed, and that is as far as this layer goes: the mod handlers, the
        // write and the relight belong to whoever owns the world. See EditPlan.
        a.has_edit = true;
        a.edit     = plan;
        return a;
    }

    // Everything else belongs to whichever subsystem owns the channel. Nothing
    // here parses it -- this file decides who is allowed to speak, not what
    // they said.
    SessionAction a;
    return a;
}

// --------------------------------------------------------------- tickets

void ServerSessions::set_ticket_key(const SignKeyPair &key)
{
    m_ticket_key     = key;
    m_has_ticket_key = true;
    std::memcpy(m_config.sign_public, key.public_key, kSignPublicBytes);
    m_config.has_sign_key = true;
}

void ServerSessions::set_destination_key(int opening, const uint8_t peer[kSignPublicBytes])
{
    if (opening < 0 || opening >= kMaxMouths) return;
    std::memcpy(m_away[opening].key, peer, kSignPublicBytes);
    m_away[opening].has_key = true;
    openings_changed();
}

bool ServerSessions::issue_ticket(PeerId peer, const TicketRequest &req, double now,
                                  PortalTicket &out, RefuseReason &reason, std::string &why)
{
    (void)now;
    reason = RefuseReason::BadTicket;

    Session *s = find_mutable(peer);
    if (!s || s->state != SessionState::Ready) {
        why = "not in the world";
        return false;
    }

    if (!leads_away()) {
        why = "this world's openings lead back into itself; there is nothing to ticket for";
        return false;
    }

    const PortalAway &away = m_away[req.opening < kMaxMouths ? req.opening : 0];
    if (!away.leads_away()) {
        why = "that opening leads nowhere else; there is nothing to ticket for";
        return false;
    }

    if (!m_has_ticket_key) {
        why = "this world has no key to sign tickets with";
        return false;
    }

    if (!away.has_key) {
        // Refused rather than signed with an empty audience. A ticket that
        // names no server is good at every server that trusts this one, which
        // is a promise nobody asked this world to make.
        why = "this world does not know the key of the server its opening leads to";
        return false;
    }

    if (!s->has_identity) {
        why = "a ticket names a player, and this session has not said which one";
        return false;
    }

    // The clock. A ticket's window is wall-clock time on two machines, which is
    // the one thing in this protocol that depends on something outside it.
    const uint64_t unix_now = (uint64_t)std::time(nullptr);

    out = PortalTicket{};
    std::memcpy(out.issuer, m_ticket_key.public_key, kSignPublicBytes);
    std::memcpy(out.subject, s->identity, kSignPublicBytes);
    std::memcpy(out.audience, away.key, kSignPublicBytes);
    out.portal_id = away.portal;
    out.scopes    = (uint8_t)(req.scopes & kTicketScopes);
    out.issued    = unix_now;
    out.expires   = unix_now + kTicketLifetime;

    if (out.scopes == 0) {
        why = "nothing was asked for";
        return false;
    }

    const std::vector<uint8_t> msg = ticket_challenge(out);
    sign_message(m_ticket_key, msg.data(), msg.size(), out.signature);

    ++s->tickets_issued;
    reason = RefuseReason::None;
    return true;
}

bool ServerSessions::ticket_admits(const PortalTicket &t, const Session &s, uint8_t scope_bit,
                                   uint32_t portal_id, std::string &why) const
{
    // Order matters only for what a log says, not for what is decided: every
    // one of these has to hold. The cheap comparisons come first so a flood of
    // forged tickets does not cost a signature check each.
    if ((t.scopes & scope_bit) == 0) {
        why = "the ticket does not cover that";
        return false;
    }

    if (t.portal_id != portal_id) {
        why = "the ticket is for a different opening";
        return false;
    }

    if (!m_config.has_sign_key) {
        why = "this world has no identity of its own to be the audience of a ticket";
        return false;
    }
    if (std::memcmp(t.audience, m_config.sign_public, kSignPublicBytes) != 0) {
        // The check that stops one server's ticket from being spent at another.
        why = "the ticket was issued for a different server";
        return false;
    }

    // Who it is for. Without this the ticket is a bearer token and anybody who
    // copied it off the wire could spend it -- which is exactly what the
    // identity proof exists to prevent for the session itself.
    if (!s.has_identity) {
        why = "a ticket names a player, and this session has not proven it is that player";
        return false;
    }
    if (std::memcmp(t.subject, s.identity, kSignPublicBytes) != 0) {
        why = "the ticket was issued for a different player";
        return false;
    }

    // The two ends of the ticket's window, each widened by the same allowance.
    //
    // They were not the same before: a ticket dated in the future was tolerated
    // by kTicketSkew and an expired one by nothing at all. That made the rule
    // the comment beside kTicketSkew claims -- "two machines that disagree by
    // more than the skew allowance cannot portal" -- true in one direction and
    // not the other, and worse, it reported a clock disagreement as "the ticket
    // has expired", which sends whoever is reading the log to look at
    // lifetimes.
    //
    // The numbers are in the message for the same reason. A refusal that names
    // the two clocks is a refusal somebody can act on; one that names neither
    // is a mystery with a plausible story attached.
    const uint64_t unix_now = (uint64_t)std::time(nullptr);
    if (t.expires + kTicketSkew <= unix_now) {
        why = "the ticket ran out " + std::to_string(unix_now - t.expires) +
              "s ago by this server's clock (allowing " + std::to_string(kTicketSkew) +
              "s of disagreement); if that is not true where it was signed, the two "
              "clocks are apart";
        return false;
    }
    if (t.issued > unix_now + kTicketSkew) {
        why = "the ticket is dated " + std::to_string(t.issued - unix_now) +
              "s ahead of this server's clock, which allows " + std::to_string(kTicketSkew) +
              "s";
        return false;
    }

    bool trusted = false;
    for (const auto &issuer : m_config.trusted_issuers) {
        if (std::memcmp(t.issuer, issuer.data(), kSignPublicBytes) == 0) {
            trusted = true;
            break;
        }
    }
    if (!trusted) {
        why = "this world does not trust the server that signed it";
        return false;
    }

    // Last, because it is the expensive one.
    if (!ticket_signed_by_issuer(t)) {
        why = "the signature on the ticket is not that issuer's";
        return false;
    }

    return true;
}

void ServerSessions::set_portals(const Portal &a, const Portal &b)
{
    m_portal[0]     = a;
    m_portal[1]     = b;
    m_portal_set[0] = true;
    m_portal_set[1] = true;
    openings_changed();
}

void ServerSessions::set_destination(int opening, const std::string &host, uint16_t port,
                                     uint16_t portal, const std::string &room)
{
    if (opening < 0 || opening >= kMaxMouths) return;
    m_away[opening].room   = room;
    m_away[opening].host   = host;
    m_away[opening].port   = port;
    m_away[opening].portal = portal < kMaxOpenings ? portal : 0;
    openings_changed();
}

namespace {

// The same opening, for a mod asking about one it opened: its centre, to well
// under a node. Frames cannot share a centre, so nothing closer is somebody else's.
bool same_centre(const Portal &p, Vec3 c) { return length(p.center - c) < 0.01f; }

} // namespace

bool ServerSessions::has_portals() const
{
    for (int i = 0; i < kMaxOpenings; ++i)
        if (m_portal_set[i] && m_link[i] >= 0 && m_portal_set[m_link[i]]) return true;
    return false;
}

int ServerSessions::open_portal(const Portal &p)
{
    int index = -1;
    for (int i = 0; i < kMaxOpenings && index < 0; ++i)
        if (m_portal_set[i] && m_portal[i].axis == p.axis && same_centre(m_portal[i], p.center))
            index = i;
    // Not a mouth's: that one is --portal-to's, placed by core.portal_place.
    for (int i = 0; i < kMaxOpenings && index < 0; ++i)
        if (!m_portal_set[i] && !(i < kMaxMouths && m_away[i].leads_away())) index = i;
    if (index < 0) return -1;

    m_portal[index]     = p;
    m_portal_set[index] = true;
    openings_changed();
    return index;
}

int ServerSessions::close_portal(Vec3 center)
{
    for (int i = 0; i < kMaxOpenings; ++i) {
        if (!m_portal_set[i] || !same_centre(m_portal[i], center)) continue;
        m_portal_set[i] = false;
        openings_changed();
        return i;
    }
    return -1;
}

void ServerSessions::close_opening(int index)
{
    if (index < 0 || index >= kMaxOpenings || !m_portal_set[index]) return;
    m_portal_set[index] = false;
    openings_changed();
}

int ServerSessions::free_openings() const
{
    int n = 0;
    for (int i = 0; i < kMaxOpenings; ++i)
        if (!m_portal_set[i] && !(i < kMaxMouths && m_away[i].leads_away())) ++n;
    return n;
}

bool ServerSessions::link_portals(int a, int b)
{
    if (a < 0 || a >= kMaxOpenings || b < -1 || b >= kMaxOpenings) return false;
    if (m_link[a] == b && (b < 0 || m_link[b] == a)) return true;

    // Whatever either one led to leads nowhere now: links are pairs, and a
    // third opening still pointing at one of these would be a one-way door the
    // image and the crossing disagree about.
    if (m_link[a] >= 0) m_link[m_link[a]] = -1;
    m_link[a] = -1;
    if (b >= 0 && b != a) {
        if (m_link[b] >= 0) m_link[m_link[b]] = -1;
        m_link[b] = a;
        m_link[a] = b;
    }
    openings_changed();
    return true;
}

const Portal *ServerSessions::opening(int index) const
{
    if (index < 0 || index >= kMaxOpenings) return nullptr;
    return m_portal_set[index] ? &m_portal[index] : nullptr;
}

// An opening that moved is a different opening, and every approach to the one
// that was there is about something that is not there any more. So the trigger
// record is cleared: "was in bounds, entered from the front" was said about an
// opening that has stopped existing.
//
// Cleared and NOT seeded, and the difference was measured rather than argued.
// Seeding is what happens after a crossing, and it means "you are already
// through this one, do not send them back". Applied to a placement it means a
// player who puts a floor portal at their own feet is standing in an opening
// that has already been used: the client's floor shift sinks them into the node
// holding it up, the trigger is suppressed, and they sink until the movement
// check refuses them. They never cross.
//
// Cleared, the ordinary rule decides from the next claim on: somebody in front
// of the plane goes through, somebody behind it waits until they step out and
// come back. The cost of that is stated -- an opening placed around a player
// who is standing in front of it takes them, without them having walked
// anywhere. That is what a portal placed on somebody does.
void ServerSessions::aim_windows(Session &s, Vec3 feet) const
{
    std::vector<PlayerInterest::Window> windows;
    const float reach = s.interest.config().window_reach;
    for (int i = 0; i < kMaxOpenings; ++i) {
        const int j = m_link[i];
        if (!local_pair(i, j)) continue;
        const float d = length(m_portal[i].center - feet);
        if (d > reach) continue;
        const Vec3 far = m_portal[j].center;
        windows.push_back(PlayerInterest::Window{far.x, far.y, far.z, d});
    }
    s.interest.set_windows(std::move(windows));
}

void ServerSessions::openings_changed()
{
    ++m_portals_version;

    for (auto &entry : m_sessions)
        for (PortalTriggerState &t : entry.second.portal_state) t = PortalTriggerState{};
}

// The nodes that hold floor portals up, as the movement check needs them.
//
// One node per horizontal opening: the opening IS a node, and the node it is is
// the one the player has to sink into. Bounded to that node on purpose -- a
// player a node below the opening is not passing through it any more, they are
// underground, and the check should say so.
size_t ServerSessions::floor_exemptions(SolidExemption *out) const
{
    size_t n = 0;

    for (int i = 0; i < kMaxOpenings && n < kMaxSolidExemptions; ++i) {
        if (!m_portal_set[i]) continue;
        const Portal &p = m_portal[i];
        if (!p.horizontal()) {
            // A wall opening set inside the wall: the wall round it, one cell
            // out every way, is passable for a body going in -- the client's
            // box is fitted into the hole (portal_wall_shift) and may still
            // touch the wall's cells. Only the wall's own layer: from just
            // behind the plane's cell to its face. An opening standing in the
            // air before a wall exempts nothing, its plane being in no wall.
            const Vec3 nn = p.normal(), r = p.right(), u = p.up();
            Vec3 lo{1e9f, 1e9f, 1e9f}, hi{-1e9f, -1e9f, -1e9f};
            for (int a = -1; a <= 1; a += 2)
                for (int b = -1; b <= 1; b += 2)
                    for (const float d : {-0.65f, 0.45f}) {
                        const Vec3 c = p.center + r * ((float)a * (p.half_w + 1.0f)) +
                                       u * ((float)b * (p.half_h + 1.0f)) + nn * d;
                        lo = Vec3{std::min(lo.x, c.x), std::min(lo.y, c.y), std::min(lo.z, c.z)};
                        hi = Vec3{std::max(hi.x, c.x), std::max(hi.y, c.y), std::max(hi.z, c.z)};
                    }
            SolidExemption &e = out[n++];
            e.min_x = lo.x, e.min_y = lo.y, e.min_z = lo.z;
            e.max_x = hi.x, e.max_y = hi.y, e.max_z = hi.z;
            continue;
        }

        // The lateral extent is the opening's own, in the axes its basis lies
        // in. Read off the basis rather than assumed, because a quarter turn
        // swaps which of half_w and half_h is the x one.
        const Vec3 r = p.right(), u = p.up();
        const float ex = std::fabs(r.x) * p.half_w + std::fabs(u.x) * p.half_h;
        const float ez = std::fabs(r.z) * p.half_w + std::fabs(u.z) * p.half_h;

        SolidExemption &e = out[n++];
        e.min_x = p.center.x - ex;
        e.max_x = p.center.x + ex;
        e.min_z = p.center.z - ez;
        e.max_z = p.center.z + ez;
        e.min_y = p.center.y - 0.5f;
        e.max_y = p.center.y + 0.5f;
    }

    return n;
}

MediaOffer ServerSessions::media_offer_message() const
{
    MediaOffer offer;
    if (!m_media) return offer;

    for (const auto &entry : m_media->table()) {
        const MediaBlob *blob = m_media->find(entry.second);
        if (!blob) continue;
        MediaEntry e;
        e.name = entry.first;
        e.hash = entry.second;
        e.size = (uint32_t)blob->bytes.size();
        offer.items.push_back(std::move(e));
    }
    return offer;
}

Heartbeat ServerSessions::heartbeat_message()
{
    // Recounted here rather than trusted: m_config.players is what the last
    // handshake was decided against, and a heartbeat is about now.
    m_config.players   = (int)player_count();
    m_config.observers = (int)observer_count();

    Heartbeat h;
    h.players       = (uint16_t)m_config.players;
    h.max_players   = (uint16_t)(m_config.max_players < 0 ? 0 : m_config.max_players);
    h.observers     = (uint16_t)m_config.observers;
    h.max_observers = (uint16_t)(m_config.max_observers < 0 ? 0 : m_config.max_observers);

    // Watchable covers both of DESIGN.md 4.4's doors: anybody may look in, or
    // there is somebody whose word this world would take. A world with neither
    // shows a portal nothing can be seen through, and saying so is the point.
    if (m_config.allow_observers || !m_config.trusted_issuers.empty())
        h.flags |= kWorldWatchable;
    if (m_config.players < m_config.max_players) h.flags |= kWorldEnterable;

    return h;
}

Portals ServerSessions::portals_message() const
{
    Portals msg;
    msg.version = m_portals_version;

    for (int i = 0; i < kMaxOpenings; ++i) {
        msg.link[i] = (int8_t)m_link[i];
        if (!m_portal_set[i]) continue;
        const Portal &p = m_portal[i];

        PortalDesc &d = msg.opening[i];
        d.cx          = p.center.x;
        d.cy          = p.center.y;
        d.cz          = p.center.z;
        d.axis        = (uint8_t)p.axis;
        d.normal_sign = (int8_t)p.normal_sign;
        d.rot         = (uint8_t)p.rot;
        d.half_w      = p.half_w;
        d.half_h      = p.half_h;
        d.kind        = (uint8_t)p.kind;
        d.colour      = p.colour;

        msg.present |= 1u << i;
    }

    // Where each leads, and who is over there. This server was told that key
    // so it could address tickets to it; the client needs it so that "the
    // world through this opening" and "the world I already have a session to"
    // can be the same answer when they are the same machine reached two ways.
    // Only the first ones that lead away: a mouth after a gap is not sent, the
    // decoder refuses it.
    for (int i = 0; i < kMaxMouths && m_away[i].leads_away(); ++i) msg.away[i] = m_away[i];

    return msg;
}

// One crossing, decided here and drawn by the client.
//
// The velocity is derived from two claims rather than sent. The Move message
// carries a position and a look and nothing else, and adding a velocity to it
// would be asking the client for a number the server then trusts -- the same
// mistake as a grace token a client can request. What it costs is stated: the
// derived velocity lags by one claim, and `portal_step_trigger` uses it only for
// the case of somebody already past the plane of a HORIZONTAL opening on the
// first frame they are in bounds. A fast faller may need one more claim to fire
// than the client's own physics would. (trigger_along: core/portal.)

// A player stepping into a mouth: an opening whose other side is not this
// server's to walk them through.
//
// The trigger is the same one a local crossing uses -- same plane, same
// direction test, same per-session record -- because it is the same event. What
// differs is the answer: a Teleport for a hole in this world, and an address for
// a hole in somebody else's.
//
// Every opening that leads away is a mouth of its own, with its own trigger
// record. An opening that leads nowhere beside a mouth is where a local crossing
// would have gone, and nobody crosses through it.
SessionAction ServerSessions::redirect_if_triggered(Session &s, const Move &move,
                                                    const MovementState &was, double now)
{
    SessionAction a;

    const Vec3 feet{move.x, move.y, move.z};

    Vec3         vel{};
    const double dt = now - was.when;
    if (was.valid && dt > 1e-4 && dt < 1.0) {
        vel = Vec3{(float)((feet.x - was.x) / dt), (float)((feet.y - was.y) / dt),
                   (float)((feet.z - was.z) / dt)};
    }

    const Vec3 from_feet = was.valid ? Vec3{was.x, was.y, was.z} : feet;

    for (int i = 0; i < kMaxMouths; ++i) {
        const PortalAway &away = m_away[i];
        if (!m_portal_set[i] || !away.leads_away() || (s.sealed >> i & 1u)) continue;
        if (!trigger_along(m_portal[i], s.portal_state[i], from_feet, feet, vel, eye_of(s)))
            continue;

        const std::string where = away.host.empty()
                                      ? "room '" + away.room + "'"
                                      : away.host + ":" + std::to_string(away.port) +
                                            (away.room.empty() ? "" : "/" + away.room);

        Redirect r;
        r.host      = away.host;
        r.port      = away.port;
        r.room      = away.room;
        r.portal_id = away.portal;
        r.reason    = "you walked into an opening onto " + where;

        // A ticket, when this world can make one. A redirect to a world that
        // requires one is otherwise a redirect to a refusal -- and the client
        // cannot ask for one afterwards, because by then it is talking to the
        // other server.
        if (can_issue(i) && s.has_identity) {
            TicketRequest want;
            want.scopes  = (uint8_t)(kTicketObserve | kTicketEnter);
            want.opening = (uint8_t)i;
            RefuseReason why_code = RefuseReason::None;
            std::string  why;
            PortalTicket t;
            if (issue_ticket(s.id, want, now, t, why_code, why)) {
                r.has_ticket = true;
                r.ticket     = t;
            }
        }

        a.reply         = r.encode();
        a.reply_channel = (uint8_t)Channel::Event;
        a.log = "'" + s.name + "' stepped into the opening onto " + where +
                "; sent them the address" + (r.has_ticket ? " and a ticket" : "");
        return a;
    }

    return a;
}

SessionAction ServerSessions::cross_if_triggered(Session &s, const Move &move,
                                                 const MovementState &was, double now)
{
    SessionAction a;

    const Vec3 feet{move.x, move.y, move.z};

    Vec3         vel{};
    const double dt = now - was.when;
    if (was.valid && dt > 1e-4 && dt < 1.0) {
        vel = Vec3{(float)((feet.x - was.x) / dt), (float)((feet.y - was.y) / dt),
                   (float)((feet.z - was.z) / dt)};
    }

    Vec3 look{move.look_x, move.look_y, move.look_z};
    if (length_sq(look) < 1e-6f) look = Vec3{0.0f, 0.0f, -1.0f};
    look = normalize(look);

    struct Crossing {
        const Portal       *from;
        const Portal       *to;
        PortalTriggerState *state;
        PortalTriggerState *other;
    };

    const Vec3 from_feet = was.valid ? Vec3{was.x, was.y, was.z} : feet;

    for (int i = 0; i < kMaxOpenings; ++i) {
        const int j = m_link[i];
        if (!local_pair(i, j)) continue;
        const Crossing c{&m_portal[i], &m_portal[j], &s.portal_state[i], &s.portal_state[j]};
        if (!trigger_along(*c.from, *c.state, from_feet, feet, vel, eye_of(s)) &&
            !portal_swallows(*c.from, feet, eye_of(s), vel))
            continue;

        const Traversal t = portal_traverse(*c.from, *c.to, feet, look, vel, m_world, m_registry,
                                            eye_of(s), limits_of(s, now).arrival_half);
        if (t.blocked) {
            // Refused rather than swallowed, and said once: a destination that
            // is walled in is a thing somebody built, not a fault.
            a.log = "'" + s.name + "' crossed into a blocked destination, refused";
            return a;
        }

        // The same window a teleport gets, for the same two reasons: the jump
        // and the speed it hands over.
        s.authority().x           = t.feet.x;
        s.authority().y           = t.feet.y;
        s.authority().z           = t.feet.z;
        s.authority().when        = now;
        s.authority().valid       = true;
        s.authority().grace_until = now + m_limits.portal_grace;
        s.authority().placed      = true;   // the server put them here (OPEN-46)

        // Seeded on the destination, or the arrival walks straight back.
        portal_seed_arrival(*c.to, *c.other, t.feet);
        ++s.crossings;

        // The interest centre goes with them, or the first chunks of where
        // they now are arrive a claim late.
        s.interest.set_position(t.feet.x, t.feet.y, t.feet.z, t.look.x, t.look.y, t.look.z);
        aim_windows(s, t.feet);

        // The speed is the last step's average: the speed at its middle.
        const Vec3 out_vel = exit_velocity(*c.from, *c.to, vel, vel + t.velocity_delta,
                                           (from_feet + feet) * 0.5f, t.feet.y, s.movement.gravity);

        Teleport tp;
        tp.seq    = move.seq;
        tp.x      = t.feet.x;
        tp.y      = t.feet.y;
        tp.z      = t.feet.z;
        tp.look_x = t.look.x;
        tp.look_y = t.look.y;
        tp.look_z = t.look.z;
        tp.vx     = out_vel.x;
        tp.vy     = out_vel.y;
        tp.vz     = out_vel.z;
        tp.roll   = t.roll;
        tp.reason = "portal";
        tp.epoch  = placed(s, t.feet, Vec3{tp.vx, tp.vy, tp.vz}, std::atan2(tp.look_x, -tp.look_z),
                           true);

        a.reply         = tp.encode();
        a.reply_channel = (uint8_t)Channel::Event;
        if (s.crossings == 1 || s.crossings % 100 == 0)
            a.log = "'" + s.name + "' crossed (" + std::to_string(s.crossings) + " so far)";
        return a;
    }

    return a;
}

int ServerSessions::cross_mobs(double now)
{
    int crossed = 0;
    std::vector<uint64_t> alive;
    m_ecs.each([&](flecs::entity e, ecs::Position &p, ecs::Brain &b) {
        alive.push_back(e.id());
        MobCrossing &mc  = m_mob_crossing[e.id()];
        Vec3         now_at{p.x, p.y, p.z};
        if (!mc.seen) {
            mc.seen = true, mc.last = now_at, mc.at = now;
            return;
        }
        const Vec3   from = mc.last;
        const double dt   = now - mc.at;
        mc.last = now_at, mc.at = now;
        // Put somewhere by its mod (core.mob_place), not moved there: carried,
        // or brought back. Nothing is crossed on the way, and the jump is no
        // speed.
        if (length_sq(now_at - from) > 4.0f) return;
        Vec3 vel{};
        if (dt > 1e-4 && dt < 1.0)
            vel = Vec3{(now_at.x - from.x) / (float)dt, (now_at.y - from.y) / (float)dt,
                       (now_at.z - from.z) / (float)dt};
        const float eye = b.height * 0.5f;
        for (int i = 0; i < kMaxOpenings; ++i) {
            const int j = m_link[i];
            if (!local_pair(i, j)) continue;
            // A floor opening is the node it is in, which holds a creature up:
            // one standing on it or falling onto it, its middle over the hole,
            // goes in -- as a player's box is let down into it (floor shift).
            const Portal &o    = m_portal[i];
            bool          into = false;
            if (o.horizontal() && o.normal().y > 0.5f) {
                const Vec3  r = o.right(), u = o.up();
                const float ex = std::fabs(r.x) * o.half_w + std::fabs(u.x) * o.half_h;
                const float ez = std::fabs(r.z) * o.half_w + std::fabs(u.z) * o.half_h;
                const float top = o.center.y + 0.5f;
                into = std::fabs(now_at.x - o.center.x) < ex && std::fabs(now_at.z - o.center.z) < ez &&
                       now_at.y >= top - 0.05f && now_at.y <= top + 0.3f && vel.y <= 0.5f;
                if (into) {
                    now_at.y = o.center.y - 0.3f;
                    vel.y    = std::min(vel.y, -3.0f);
                }
            } else if (!o.horizontal()) {
                // And against a wall's, across it and within its height,
                // moving at it: in, as a body's box is let into the hole.
                const Vec3  n   = o.normal();
                const Vec3  rel = now_at - o.center;
                const float depth = dot(rel, n);
                into = std::fabs(dot(rel, o.right())) < o.half_w &&
                       now_at.y >= o.center.y - o.half_h - 0.1f &&
                       now_at.y + b.height <= o.center.y + o.half_h + 0.2f && depth >= 0.0f &&
                       depth <= b.half + 0.6f && dot(vel, n) < -0.5f;
                if (into) now_at = now_at - n * (depth + 0.1f);
            }
            if (!into && !trigger_along(m_portal[i], mc.state[i], from, now_at, vel, eye)) continue;
            ecs::Look *look = e.try_get_mut<ecs::Look>();
            const float yaw = look ? look->yaw : 0.0f;
            const Vec3  facing{std::sin(yaw), 0.0f, -std::cos(yaw)};
            const Traversal t = portal_traverse(m_portal[i], m_portal[j], now_at, facing, vel, m_world,
                                                m_registry, eye, b.half);
            if (t.blocked) break;
            // Out of the far opening where a body of its size fits: in front
            // of a wall's at its foot, on a floor's, under a ceiling's. The
            // traversal's feet are a player's, eye and all, and a cube put
            // there came out on top of the ceiling.
            const Portal &d  = m_portal[j];
            const Vec3    dn = d.normal();
            Vec3          feet{d.center.x, d.center.y, d.center.z};
            if (!d.horizontal())
                feet = Vec3{d.center.x + dn.x * (b.half + 0.1f), d.center.y - d.half_h + 0.01f,
                            d.center.z + dn.z * (b.half + 0.1f)};
            else if (dn.y > 0.0f)
                feet.y = d.center.y + 0.51f;
            else
                feet.y = d.center.y - 0.5f - b.height - 0.01f;
            p.x = feet.x, p.y = feet.y, p.z = feet.z;
            // Its speed turned with it: a creature's own (its knock and its
            // fall) carries it on out of the far side.
            const Vec3 out{vel.x + t.velocity_delta.x, vel.y + t.velocity_delta.y,
                           vel.z + t.velocity_delta.z};
            b.kx = out.x, b.kz = out.z, b.vy = out.y;
            b.going = false;
            b.path.clear();
            if (look) {
                look->yaw = t.yaw;
                e.modified<ecs::Look>();
            }
            e.modified<ecs::Position>();
            portal_seed_arrival(m_portal[j], mc.state[j], feet);
            mc.last = feet;
            ++crossed;
            break;
        }
    });
    // Forgotten when gone.
    if (m_mob_crossing.size() > alive.size()) {
        std::sort(alive.begin(), alive.end());
        for (auto it = m_mob_crossing.begin(); it != m_mob_crossing.end();)
            it = std::binary_search(alive.begin(), alive.end(), it->first) ? std::next(it)
                                                                            : m_mob_crossing.erase(it);
    }
    return crossed;
}

bool ServerSessions::launch(PeerId peer, Vec3 vel, double now, double seconds)
{
    Session *s = find_mutable(peer);
    if (s == nullptr || !s->authority().valid) return false;
    (void)vel;
    s->authority().grace_until = std::max(s->authority().grace_until, now + seconds);
    return true;
}

// The opening a face of a node implies.
//
// The client sends a node and a face; everything about the hole is worked out
// here. That split is not tidiness -- it is the reason a modified client cannot
// hang a portal in mid-air a hundred blocks away. There is no field in the
// message for "where the opening is", so there is nothing to lie about beyond
// which block was clicked, and that is checked against the world and the
// player's reach.
//
// The shape follows what a player has to fit through. A wall gets a hole one
// node wide and two tall, standing on the cell in front of the face, because
// that is the size of the player. A floor or a ceiling gets one node by one:
// the hole is looked through from above or below, and a two-node-deep hole in a
// floor is a shaft, not an opening.
//
// WHICH node the opening occupies is different for the two, and that asymmetry
// is the whole of PORTAL_SYSTEM 2.3 rather than an oversight.
//
//   A wall opening lives in the AIR CELL in front of the face. Nothing has to
//   hold it up, and putting it in the wall node itself would put the hole
//   inside a solid block, where the player cannot stand.
//
//   A floor opening IS the clicked node. Something must hold a floor portal up,
//   and that something is solid ground exactly where the player has to fall
//   through -- which is why the physics override and the exemption below exist.
//   Put in the cell ABOVE instead, its surface lands a node over the player's
//   head: they stand half a node BEHIND the plane of a portal they placed at
//   their feet, the trigger never sees them approach from the front, and they
//   sink without ever crossing. Measured before it was moved.
bool ServerSessions::opening_from_face(const PlacePortal &req, Portal &out,
                                       std::string &why) const
{
    if (!has_authority()) {
        why = "this server has no world to place against";
        return false;
    }

    // The face is validated by the decoder: exactly one component is +/-1.
    const int nx = req.nx, ny = req.ny, nz = req.nz;

    if (!m_registry->is_solid(m_world->get_block(req.x, req.y, req.z))) {
        why = "a portal needs something to sit in, and that block is not solid";
        return false;
    }

    // The cell in front of the face, and -- for a wall -- the one above it.
    // Both have to be free, or the player would arrive inside a block on the
    // way out. This is the same question the movement check asks about a
    // claimed position, asked before the opening exists rather than after.
    const int cx = req.x + nx, cy = req.y + ny, cz = req.z + nz;
    const bool horizontal = (ny != 0);

    const int cells = horizontal ? 1 : 2;
    for (int i = 0; i < cells; ++i) {
        if (m_registry->is_solid(m_world->get_block(cx, cy + i, cz))) {
            why = "there is no room in front of that face for an opening";
            return false;
        }
    }

    // A floor opening in the node the player is standing IN would put them
    // inside their own portal with no approach. Refused for the wall case only;
    // for a floor the player standing on the node is the ordinary case and the
    // point of the whole override.

    out             = Portal{};
    out.axis        = horizontal ? PortalAxis::XZ : (nz != 0 ? PortalAxis::XY : PortalAxis::YZ);
    out.normal_sign = (nx + ny + nz) > 0 ? 1 : -1;
    out.rot         = 0;
    out.half_w      = 0.5f;
    out.half_h      = horizontal ? 0.5f : 1.0f;
    out.kind        = PortalKind::Frame;

    // Centred on what it occupies: the clicked node for a floor, the air cell
    // in front for a wall. A wall opening spans two cells, so its centre is on
    // the boundary between them -- one node above the bottom of the lower cell.
    out.center = horizontal
                     ? Vec3{(float)req.x + 0.5f, (float)req.y + 0.5f, (float)req.z + 0.5f}
                     : Vec3{(float)cx + 0.5f, (float)cy + 1.0f, (float)cz + 0.5f};
    return true;
}

// A window becomes a door, or the other way round.
//
// The arrival is computed HERE, by the same portal_traverse the local crossing
// uses, against THIS world's own opening. What the client sends is the opening
// it left and where it was standing in it, and both are checked: the pose has
// to be inside the bounds of the opening it claims, and the opening has to be
// a plausible size. That is what bounds the lie -- whatever a modified client
// sends, the arrival comes out relative to the hole that is actually here.
//
// It is not a claim that this is airtight. A client may choose WHERE IN the
// opening it arrives, exactly as it may choose where it stands when it walks
// into a local one. What it may not do is arrive somewhere else in the world.
bool ServerSessions::change_scope(PeerId peer, const ChangeScope &req, double now, Teleport &out,
                                  RefuseReason &reason, std::string &why)
{
    reason = RefuseReason::Malformed;

    Session *s = find_mutable(peer);
    if (!s || s->state != SessionState::Ready) {
        why = "not in the world";
        return false;
    }

    const Scope want = (Scope)req.scope;
    if (want == s->scope) {
        why = std::string("already a ") + scope_text(want);
        return false;
    }

    // Stepping back to watching. Nothing to check and nothing to compute: a
    // player who has left through a portal is a window onto the world they
    // left, and their body stops existing the moment the scope changes.
    if (want == Scope::Observe) {
        s->scope = Scope::Observe;
        s->authority().valid = false;
        s->avatar.remove<ecs::Body>();   // the body stops existing here
        return true;
    }

    // Who says they may. A window becoming a body is coming through a portal,
    // which is exactly the admission DESIGN.md 4.4 puts a ticket in front of --
    // and it is a separate permission from having been let in to look, because
    // a world may well be happy to be seen and picky about who walks out of its
    // openings.
    if (req.has_ticket) {
        if (!ticket_admits(req.ticket, *s, kTicketEnter, req.portal_id, why)) {
            reason = RefuseReason::BadTicket;
            return false;
        }
        s->admitted_by_ticket = true;   // and by whom: whose bag it is (src/server/admin.h)
        std::memcpy(s->ticket_issuer, req.ticket.issuer, kSignPublicBytes);
        if (m_crossing_seen) m_crossing_seen(req.ticket.issuer, peer);
    } else if (m_config.require_ticket) {
        reason = RefuseReason::BadTicket;
        why    = "this world only takes players it has been vouched for";
        return false;
    }

    if (m_config.observers > 0) m_config.observers = (int)observer_count();
    if ((int)player_count() >= m_config.max_players) {
        reason = RefuseReason::ServerFull;
        why    = "this world is full of players";
        return false;
    }

    if (!has_authority()) {
        reason = RefuseReason::NoSuchOpening;
        why    = "this server has no world to arrive in";
        return false;
    }

    // The opening the far side's link named. Bounded by the decoder to the two
    // this protocol has; refused here when this world has not put that one
    // there, which is the case a mouth pointed at a half-built pair produces.
    const Portal *here = opening((int)req.portal_id);
    if (!here) {
        reason = RefuseReason::NoSuchOpening;
        why    = "this world has no opening " + std::to_string(req.portal_id) +
                 " to arrive out of";
        return false;
    }

    Portal there;
    there.center      = Vec3{req.from.cx, req.from.cy, req.from.cz};
    there.axis        = (PortalAxis)req.from.axis;
    there.normal_sign = req.from.normal_sign;
    there.rot         = req.from.rot;
    there.half_w      = req.from.half_w;
    there.half_h      = req.from.half_h;
    there.kind        = (PortalKind)req.from.kind;

    const Vec3 feet{req.x, req.y, req.z};
    // Or a body caught in the opening's block by its eye (portal_swallows):
    // its feet may be a body's height under a floor's.
    if (!in_portal_bounds(there, feet) && !portal_swallows(there, feet, eye_of(*s), Vec3{})) {
        why = "that is not a pose inside the opening it says it is";
        return false;
    }

    Vec3 look{req.look_x, req.look_y, req.look_z};
    look = normalize(look);

    const Traversal t = portal_traverse(there, *here, feet, look, Vec3{req.vx, req.vy, req.vz},
                                        m_world, m_registry, eye_of(*s),
                                        limits_of(*s, now).arrival_half);
    if (t.blocked) {
        reason = RefuseReason::ArrivalBlocked;
        why    = "the way out of this world's opening is blocked";
        return false;
    }

    s->scope = Scope::Enter;

    s->authority().x           = t.feet.x;
    s->authority().y           = t.feet.y;
    s->authority().z           = t.feet.z;
    s->authority().when        = now;
    s->authority().valid       = true;
    s->authority().grace_until = now + m_limits.portal_grace;
    s->authority().placed      = true;   // the server put them here (OPEN-46)
    s->has_moved             = true;
    s->avatar.add<ecs::Body>();   // and starts existing again, over here
    set_look(s->look(), t.look.x, t.look.y, t.look.z);
    s->interest.set_position(t.feet.x, t.feet.y, t.feet.z, t.look.x, t.look.y, t.look.z);

    // Seeded, or the arrival walks straight back out through the opening it
    // just came out of. Which of the two trigger records depends on which
    // opening they came out of -- seeding the wrong one leaves the arrival
    // standing in an untouched trigger and sends them straight back.
    portal_seed_arrival(*here, s->portal_state[req.portal_id], t.feet);
    ++s->crossings;

    out        = Teleport{};
    out.x      = t.feet.x;
    out.y      = t.feet.y;
    out.z      = t.feet.z;
    out.look_x = t.look.x;
    out.look_y = t.look.y;
    out.look_z = t.look.z;
    // The client's own speed, at its own height.
    const Vec3 in_vel{req.vx, req.vy, req.vz};
    const Vec3 out_vel = exit_velocity(there, *here, in_vel, in_vel + t.velocity_delta, feet,
                                       t.feet.y, s->movement.gravity);
    out.vx     = out_vel.x;
    out.vy     = out_vel.y;
    out.vz     = out_vel.z;
    out.roll   = t.roll;
    out.reason = "arrived through a portal";
    out.epoch  = placed(*s, t.feet, Vec3{out.vx, out.vy, out.vz},
                        std::atan2(out.look_x, -out.look_z), true);
    return true;
}

bool ServerSessions::place_portal(PeerId peer, const PlacePortal &req, std::string &why)
{
    Session *s = find_mutable(peer);
    if (!s || s->state != SessionState::Ready) {
        why = "not in the world";
        return false;
    }

    if (!s->authority().valid) {
        why = "the server does not know where you are yet";
        return false;
    }

    // Reach, against the position the SERVER accepted rather than the one the
    // client would like to be at. DESIGN.md 3.4 asks for placement validated by
    // distance, and this is the whole of it: measured from the eye, because
    // that is what the ray the client cast came out of.
    const Vec3 eye{s->authority().x, s->authority().y + eye_of(*s), s->authority().z};
    const Vec3 node{(float)req.x + 0.5f, (float)req.y + 0.5f, (float)req.z + 0.5f};
    const float reach = length(node - eye);
    if (reach > m_limits.place_reach) {
        char buf[96];
        std::snprintf(buf, sizeof(buf), "that block is %.1f nodes away, further than %.1f",
                      (double)reach, (double)m_limits.place_reach);
        why = buf;
        return false;
    }

    Portal p;
    if (!opening_from_face(req, p, why)) return false;

    const int index = (int)(req.which & 1);
    const int other = 1 - index;

    // An opening linked to itself is not a portal. Everything else -- a pair a
    // node apart, a pair back to back through one wall -- is allowed and is not
    // claimed to have been measured.
    if (m_portal_set[other] && length(m_portal[other].center - p.center) < 0.5f) {
        why = "the other opening is already there";
        return false;
    }

    m_portal[index]     = p;
    m_portal_set[index] = true;
    openings_changed();
    return true;
}

SessionAction ServerSessions::teleport(PeerId peer, float x, float y, float z, double now,
                                       const std::string &why, const Vec3 *look)
{
    SessionAction a;

    Session *s = find_mutable(peer);
    if (!s || s->state != SessionState::Ready) return a;

    s->authority().x     = x;
    s->authority().y     = y;
    s->authority().z     = z;
    s->authority().when  = now;
    s->authority().valid = true;
    s->authority().placed = true;   // the server put them here (OPEN-46)

    // From now, not from the last claim: the window has to cover the frames
    // AFTER the client has been told, and the client has not been told yet.
    s->authority().grace_until = now + m_limits.portal_grace;

    if (look) {
        Teleport t;
        t.seq    = s->move_seq;
        t.x      = x;
        t.y      = y;
        t.z      = z;
        t.look_x = look->x;
        t.look_y = look->y;
        t.look_z = look->z;
        t.reason = why;
        t.epoch  = placed(*s, Vec3{x, y, z}, Vec3{}, std::atan2(t.look_x, -t.look_z), true);
        a.reply  = t.encode();
    } else {
        Correct c;
        c.seq    = s->move_seq;
        c.x      = x;
        c.y      = y;
        c.z      = z;
        c.reason = why;
        c.epoch  = placed(*s, Vec3{x, y, z}, Vec3{}, 0.0f, false);
        a.reply  = c.encode();
    }
    a.reply_channel = (uint8_t)Channel::Event;
    a.log           = "'" + s->name + "' moved by the server: " + why;
    return a;
}

SessionAction ServerSessions::ride(PeerId peer, PeerId host, Vec3 seat, double now)
{
    SessionAction a;
    Session      *s = find_mutable(peer);
    if (!s || s->state != SessionState::Ready || s->scope != Scope::Enter) return a;

    Ride r;
    if (host != 0) {
        const Session *h = find(host);
        if (!h || host == peer || h->state != SessionState::Ready || h->scope != Scope::Enter ||
            h->riding != 0)
            return a;
        for (const float v : {seat.x, seat.y, seat.z})
            if (!std::isfinite(v) || std::fabs(v) > Ride::kMax) return a;
        r.host = h->avatar.id();
        const MovementState &at = h->authority();
        r.x = at.x; r.y = at.y; r.z = at.z;   // until the first step_riders puts it on the seat
        r.seat_x = seat.x; r.seat_y = seat.y; r.seat_z = seat.z;
    }

    const bool was = s->riding != 0;
    s->riding      = host;
    s->riding_thing = 0;
    s->seat        = seat;

    // Off: walking again from the seat, told as a placement so the claims made
    // while seated -- which the server never looked at -- are not judged.
    if (host == 0 && was) {
        const MovementState &at = s->authority();
        s->authority().grace_until = now + m_limits.portal_grace;
        Correct c;
        c.seq    = s->move_seq;
        c.x      = at.x;
        c.y      = at.y;
        c.z      = at.z;
        c.reason = "got off";
        c.epoch  = placed(*s, Vec3{at.x, at.y, at.z}, Vec3{}, s->heading, false);
        // Ride first, on the same reliable channel: the client stops following
        // before it is put down.
        a.reply = r.encode();
        a.more.push_back(c.encode());
    } else {
        a.reply = r.encode();
    }
    a.reply_channel = (uint8_t)Channel::Event;
    a.log = "'" + s->name + "' " + (host ? "is carried on another body" : "gets off");
    return a;
}

SessionAction ServerSessions::ride_thing(PeerId peer, uint64_t thing, Vec3 seat, double now)
{
    SessionAction a;
    Session      *s = find_mutable(peer);
    if (!s || s->state != SessionState::Ready || s->scope != Scope::Enter) return a;
    for (const float v : {seat.x, seat.y, seat.z})
        if (!std::isfinite(v) || std::fabs(v) > Ride::kMax) return a;
    flecs::entity e = m_ecs.get_alive((flecs::entity_t)thing);
    if (!e.is_valid() || !e.has<ecs::Thing>()) return a;
    const ecs::Position *p = e.try_get<ecs::Position>();
    if (p == nullptr) return a;
    (void)now;

    s->riding       = Session::kRideThing;
    s->riding_thing = thing;
    s->seat         = seat;
    Ride r;
    r.host = thing;
    r.x = p->x; r.y = p->y; r.z = p->z;   // until the first step_riders puts it on the seat
    r.seat_x = seat.x; r.seat_y = seat.y; r.seat_z = seat.z;
    a.reply         = r.encode();
    a.reply_channel = (uint8_t)Channel::Event;
    a.log           = "'" + s->name + "' gets into a machine";
    return a;
}

SessionAction ServerSessions::lie(PeerId peer, bool on, Vec3 at, float head, double now)
{
    Session *s = find_mutable(peer);
    if (!s || s->state != SessionState::Ready || s->scope != Scope::Enter || s->riding != 0)
        return SessionAction{};
    for (const float v : {at.x, at.y, at.z, head})
        if (!std::isfinite(v)) return SessionAction{};

    Lie l;
    l.on = on;
    if (!on) {
        s->avatar.remove<ecs::Lying>();
        const MovementState &me = s->authority();
        l.x = me.x; l.y = me.y; l.z = me.z;
        SessionAction a;
        a.reply         = l.encode();
        a.reply_channel = (uint8_t)Channel::Event;
        a.log           = "'" + s->name + "' gets up";
        return a;
    }

    // Put there as a teleport puts a body, then told it lies: the Lie after
    // the correction, on the same reliable channel, so it is never drawn lying
    // where it was before.
    l.x = at.x; l.y = at.y; l.z = at.z;
    l.head = std::remainder(head, 6.2831853f);
    SessionAction a = teleport(peer, at.x, at.y, at.z, now, "lies down");
    if (a.reply.empty()) return a;
    a.more.push_back(l.encode());
    s->avatar.set<ecs::Lying>(ecs::Lying{l.head});
    return a;
}

std::vector<std::pair<PeerId, SessionAction>> ServerSessions::step_riders(double now)
{
    std::vector<std::pair<PeerId, SessionAction>> out;
    for (auto &kv : m_sessions) {
        Session &s = kv.second;
        if (s.riding == 0) continue;

        if (s.riding == Session::kRideThing) {
            flecs::entity t = m_ecs.get_alive((flecs::entity_t)s.riding_thing);
            const ecs::Position *tp = t.is_valid() ? t.try_get<ecs::Position>() : nullptr;
            const ecs::Look     *tl = t.is_valid() ? t.try_get<ecs::Look>() : nullptr;
            if (tp == nullptr || tl == nullptr) {
                out.emplace_back(s.id, ride(s.id, 0, Vec3{}, now));   // it is gone: off
                continue;
            }
            const ecs::Roll *tr  = t.try_get<ecs::Roll>();
            const Vec3       at  = craft_point(Vec3{tp->x, tp->y, tp->z}, tl->yaw, tl->pitch,
                                               tr ? tr->roll : 0.0f, s.seat);
            SessionAction seat;
            Ride          r;
            r.host = s.riding_thing;
            r.x = at.x; r.y = at.y; r.z = at.z;
            r.seat_x = s.seat.x; r.seat_y = s.seat.y; r.seat_z = s.seat.z;
            seat.reply         = r.encode();
            seat.reply_channel = (uint8_t)Channel::Entity;
            out.emplace_back(s.id, std::move(seat));

            MovementState &me = s.authority();
            me.x = at.x; me.y = at.y; me.z = at.z;
            me.when  = now;
            me.valid = true;
            s.heading   = tl->yaw;
            s.has_moved = true;
            s.avatar.add<ecs::Body>();
            // Looking where they look, which their claims still say: what
            // core.player_look answers, a gunner's aim.
            const ecs::Look me_look = s.look();
            s.interest.set_position(at.x, at.y, at.z, std::sin(me_look.yaw) * std::cos(me_look.pitch),
                                    std::sin(me_look.pitch), -std::cos(me_look.yaw) * std::cos(me_look.pitch));
            s.interest.forget_distant();
            continue;
        }

        const Session *h = find(s.riding);
        if (!h || h->state != SessionState::Ready || h->scope != Scope::Enter) {
            out.emplace_back(s.id, ride(s.id, 0, Vec3{}, now));
            continue;
        }
        if (!h->has_moved) continue;   // joined and not yet anywhere: the seat waits

        // The seat in the host's frame: x to its right, z to its front.
        const float          c = std::cos(h->heading), sn = std::sin(h->heading);
        const MovementState &at = h->authority();
        const float x = at.x + s.seat.x * c + s.seat.z * sn;
        const float y = at.y + s.seat.y;
        const float z = at.z + s.seat.x * sn - s.seat.z * c;

        // Where the seat is now, to the one sitting in it, on the entity
        // channel: the next tick's replaces a lost one.
        SessionAction seat;
        Ride          r;
        r.host = h->avatar.id();
        r.x = x; r.y = y; r.z = z;
        r.seat_x = s.seat.x; r.seat_y = s.seat.y; r.seat_z = s.seat.z;
        seat.reply         = r.encode();
        seat.reply_channel = (uint8_t)Channel::Entity;
        out.emplace_back(s.id, std::move(seat));

        MovementState &me = s.authority();
        me.x = x; me.y = y; me.z = z;
        me.when  = now;
        me.valid = true;
        s.heading   = h->heading;
        s.has_moved = true;
        s.avatar.add<ecs::Body>();
        // Looking where they look, which their claims still say (the Move
        // handler keeps it while they are carried): core.player_look.
        const ecs::Look me_look = s.look();
        s.interest.set_position(x, y, z, std::sin(me_look.yaw) * std::cos(me_look.pitch),
                                std::sin(me_look.pitch), -std::cos(me_look.yaw) * std::cos(me_look.pitch));
        s.interest.forget_distant();
    }
    return out;
}

std::vector<PeerId> ServerSessions::expired(double now) const
{
    std::vector<PeerId> out;
    for (const auto &kv : m_sessions) {
        const Session &s = kv.second;
        if (s.state == SessionState::Pending && now - s.since > kHandshakeTimeout)
            out.push_back(s.id);
    }
    return out;
}

namespace {

// The faster of two sets of numbers, speed by speed. The gravity is the first
// set's: what a change window widens is what the validator checks, and the
// gravity is not one of those.
MovementRules widest(const MovementRules &a, const MovementRules &b)
{
    MovementRules w = a;
    if (b.walk > w.walk) w.walk = b.walk;
    if (b.sprint > w.sprint) w.sprint = b.sprint;
    if (b.fly > w.fly) w.fly = b.fly;
    if (b.jump > w.jump) w.jump = b.jump;

    // A body that drives in one set and walks in the other is checked as the
    // walker, whose flight is the wider allowance of the two.
    w.accel = (a.drives() && b.drives()) ? std::max(a.accel, b.accel) : 0.0f;

    // And the NARROWER box, which is the one fewer walls stop.
    w.width = std::min(a.width, b.width);

    // And the higher of the two ceilings, as each set works it out: a negative
    // one here means "from the jump", and the jump above is already the wider.
    w.rise = std::max(limits_for(a).rise, limits_for(b).rise);

    // And parkour if either has it, which allows more than not.
    w.parkour = a.parkour || b.parkour;
    return w;
}

} // namespace

bool ServerSessions::set_body_movement(PeerId peer, const MovementRules &rules, double now)
{
    Session *s = find_mutable(peer);
    if (!s || s->state != SessionState::Ready || s->scope != Scope::Enter) return false;
    if (!movement_rules_problem(rules).empty()) return false;

    // A second change inside the window of the first keeps the widest of all
    // three: the client may still be moving by the first set.
    const bool in_window =
        s->movement_since >= 0.0 && now <= s->movement_since + m_limits.portal_grace;
    s->movement_before = in_window ? widest(s->movement_before, s->movement) : s->movement;

    // And the numbers themselves, by which a claim's records may still have
    // been driven for a while.
    for (int i = 3; i > 0; --i) s->movement_past[i] = s->movement_past[i - 1];
    s->movement_past[0] = Session::PastMovement{s->movement_gen, s->movement, now};
    ++s->movement_gen;

    s->movement        = rules;
    s->movement_since  = now;
    s->movement_owed   = true;
    return true;
}

bool ServerSessions::adopt_body_movement(PeerId peer, const MovementRules &rules)
{
    Session *s = find_mutable(peer);
    if (!s || s->state != SessionState::Ready || s->scope != Scope::Enter) return false;
    if (!movement_rules_problem(rules).empty()) return false;
    s->movement       = rules;
    s->movement_owed  = true;   // said again, under the same generation
    return true;
}

std::vector<PeerId> ServerSessions::movement_to_send()
{
    std::vector<PeerId> owed;
    for (auto &kv : m_sessions) {
        Session &s = kv.second;
        if (s.state != SessionState::Ready || s.scope != Scope::Enter) continue;
        if (!s.movement_owed) continue;

        s.movement_owed = false;
        owed.push_back(kv.first);
    }
    return owed;
}

std::vector<uint8_t> ServerSessions::movement_message(PeerId peer) const
{
    const Session *s = find(peer);

    BodyMovement m;
    m.rules = s != nullptr ? s->movement : m_config.movement;
    m.gen   = s != nullptr ? s->movement_gen : 0;
    m.dig   = s != nullptr ? s->dig_factor : 1.0f;
    return m.encode();
}

MovementLimits ServerSessions::limits_of(const Session &s, double now) const
{
    // The window is the portal's grace, and the same second for the same
    // reason: the server changed something about this body, and its client has
    // not been told yet.
    const bool in_window =
        s.movement_since >= 0.0 && now <= s.movement_since + m_limits.portal_grace;
    const MovementRules rules = in_window ? widest(s.movement, s.movement_before) : s.movement;

    // Everything that is not a speed -- the tolerance, the burst, the grace, the
    // reach, whether a wall is a wall -- is the world's.
    MovementLimits       limits = m_limits;
    const MovementLimits speeds = limits_for(rules);
    limits.max_horizontal = speeds.max_horizontal;
    limits.max_vertical   = speeds.max_vertical;
    limits.half_width     = speeds.half_width;   // and the body's size and height
    limits.solid_half     = speeds.solid_half;
    limits.arrival_half   = speeds.arrival_half;
    limits.rise           = speeds.rise;
    limits.height         = speeds.height;
    return limits;
}

uint32_t ServerSessions::placed(Session &s, Vec3 at, Vec3 vel, float yaw, bool yaw_known)
{
    DriveShadow &d = s.drive;
    d.body.vehicle.reset();
    d.body.pos        = at;
    d.body.velocity   = vel;
    d.body.yaw        = yaw;
    d.pending         = true;
    d.pending_pos     = at;
    d.pending_vel     = vel;
    d.pending_yaw     = yaw;
    d.pending_yaw_known = yaw_known;
    d.next_known      = false;
    return ++s.epoch;
}

const MovementRules &ServerSessions::rules_of_gen(const Session &s, uint8_t gen, double now) const
{
    if (gen == s.movement_gen) return s.movement;
    for (const Session::PastMovement &p : s.movement_past)
        if (p.until >= 0.0 && p.gen == gen && now <= p.until + m_limits.portal_grace)
            return p.rules;
    return s.movement;
}

namespace {

// How far a claim may be from the server's copy of its kart: the client draws
// it between the last two steps, and lifts feet a wheel pressed into the road
// onto the road's top, less than a wheel's radius (vehicle_update).
constexpr float kDriveClaimSlop = 0.35f;

// How far a put nobody asked for may be from the chassis it replaces, and from
// where the body last was when there is no chassis yet (a body getting into a
// kart where it stands, a claim or two of walking after its last one).
constexpr float kPutSlop  = 0.5f;
constexpr float kPutReach = 2.0f;

// How much a put nobody asked for may move a body, in nodes, and how fast
// that fills: the client holding a kart over ground it has not been sent puts
// its chassis down afresh every frame, by as little as a frame moves it.
constexpr float kPutBank     = 2.0f;
constexpr float kPutBankRate = 1.0f;

// How long a chunk that has been sent may still be on its way. Handed to ENet is
// not arrived: on a slow link a reliable channel with a backlog holds one for
// seconds.
constexpr double kChunkInFlight = 5.0;

float turn_between(float a, float b)
{
    float d = a - b;
    while (d > 3.14159265f) d -= 6.2831853f;
    while (d < -3.14159265f) d += 6.2831853f;
    return std::fabs(d);
}

Vec3 nearest_on(Vec3 p, Vec3 a, Vec3 b)
{
    const Vec3  ab = b - a;
    const float l2 = length_sq(ab);
    const float t  = l2 > 1e-12f ? std::clamp(dot(p - a, ab) / l2, 0.0f, 1.0f) : 0.0f;
    return a + ab * t;
}

} // namespace

bool ServerSessions::put_fits(Session &s, const DriveEvent &e, double now, std::string &why)
{
    DriveShadow &d = s.drive;
    char         buf[160];

    Vec3  ref;
    float reach = kPutSlop;
    if (d.pending) {
        // Where the server put it, as it was sent: the client puts its chassis
        // down there before anything else can move it.
        const bool here   = length(e.pos - d.pending_pos) <= 1e-3f;
        const bool slower = length(e.vel) <= length(d.pending_vel) + 1e-3f;
        const bool facing = !d.pending_yaw_known || turn_between(e.yaw, d.pending_yaw) <= 1e-3f;
        if (here && slower && facing) return true;
        ref = d.pending_pos;
        if (length(e.vel) > length(d.pending_vel) + kPutSlop) {
            why = "a kart put down faster than the server put it there";
            return false;
        }
    } else if (d.body.vehicle) {
        // Put down afresh by something other than its wheels: no faster and
        // turned no further than the chassis it replaces.
        Vec3 was, at;
        vehicle_feet(d.body, was, at);
        ref = nearest_on(e.pos, was, at);
        if (turn_between(e.yaw, d.body.yaw) > 0.1f) {
            std::snprintf(buf, sizeof buf, "a kart turned %.2f rad by something other than its wheel",
                          (double)turn_between(e.yaw, d.body.yaw));
            why = buf;
            return false;
        }
        if (length(e.vel) > length(d.body.velocity) + kPutSlop) {
            std::snprintf(buf, sizeof buf, "a kart put down at %.1f m/s when it was going %.1f",
                          (double)length(e.vel), (double)length(d.body.velocity));
            why = buf;
            return false;
        }
    } else {
        // A body getting into a kart: where it last was, at no more than it
        // was allowed to go.
        ref   = Vec3{s.authority().x, s.authority().y, s.authority().z};
        reach = kPutReach;
        const MovementLimits lim = limits_of(s, now);
        if (std::sqrt(e.vel.x * e.vel.x + e.vel.z * e.vel.z) > lim.max_horizontal * lim.tolerance ||
            std::fabs(e.vel.y) > lim.max_vertical) {
            why = "a kart put down faster than the body was allowed to go";
            return false;
        }
    }

    const float moved = length(e.pos - ref);
    if (moved > reach || moved > d.put_bank) {
        std::snprintf(buf, sizeof buf, "a kart put down %.2f from where it was (%.2f %.2f %.2f)%s",
                      (double)moved, (double)ref.x, (double)ref.y, (double)ref.z,
                      moved > reach ? "" : ", again and again");
        why = buf;
        return false;
    }
    d.put_bank -= moved;
    return true;
}

ServerSessions::Drive ServerSessions::drive_claim(Session &s, const Move &move, double now,
                                                  std::string &why)
{
    DriveShadow &d = s.drive;

    // The records this server has not driven yet. A claim that starts past the
    // next one lost some on the way -- this channel drops packets, and a claim
    // carries only a few claims' worth -- and a chassis cannot be driven
    // through a gap. The first claim after a placement says where numbering is.
    size_t from = 0;
    bool   lost = false;
    if (d.next_known) {
        if (move.first_drive > d.next)
            lost = true;
        else
            from = std::min<size_t>(d.next - move.first_drive, move.drive.size());
    }
    d.next       = move.first_drive + (uint32_t)move.drive.size();
    d.next_known = true;

    // The time the steps are charged against, and the room puts have.
    const double burst = m_limits.max_burst;
    d.bank    = d.bank_at < 0.0 ? burst : std::min(d.bank + (now - d.bank_at), burst);
    d.bank_at = now;
    d.put_bank = d.put_bank_at < 0.0
                     ? kPutBank
                     : std::min(d.put_bank + kPutBankRate * (float)(now - d.put_bank_at), kPutBank);
    d.put_bank_at = now;

    // A chunk the client says it built a kart's blocks without may have been
    // on its way, or never sent: not one it has had for a while, which would
    // be a client driving through walls it has been shown.
    const MayMiss may_miss = [&](ChunkPos c) {
        const double at = s.interest.sent_at(c);
        return at < 0.0 || now - at < kChunkInFlight;
    };

    for (size_t i = from; i < move.drive.size() && !lost; ++i) {
        const DriveRecord   &r     = move.drive[i];
        const MovementRules &rules = rules_of_gen(s, r.gen, now);
        if (!rules.drives()) {
            why = "a kart driven by the numbers of a body on foot";
            return Drive::Refuse;
        }
        if (r.event.put) {
            if (!put_fits(s, r.event, now, why)) return Drive::Refuse;
            d.body.pos      = r.event.pos;
            d.body.velocity = r.event.vel;
            d.body.yaw      = r.event.yaw;
            vehicle_put(d.body, *m_world, *m_registry, rules, r.event, may_miss);
            d.pending = false;
            continue;
        }
        if (!d.body.vehicle) {
            lost = true;   // steps of a chassis this server never saw put down
            break;
        }
        d.bank -= vehicle_step_seconds();
        if (d.bank < -0.05) {
            why = "more of a kart's steps than time has passed";
            return Drive::Refuse;
        }
        vehicle_step(d.body, *m_world, *m_registry, rules, r.event, may_miss);
    }

    if (!rules_of_gen(s, move.gen, now).drives()) {
        d.body.vehicle.reset();
        d.pending = false;
        return Drive::Walks;
    }
    if (lost) {
        d.body.vehicle.reset();
        return Drive::Resync;
    }
    if (!d.body.vehicle) return Drive::Unknown;

    Vec3 was, at;
    vehicle_feet(d.body, was, at);
    const Vec3  claim{move.x, move.y, move.z};
    const float off = length(claim - nearest_on(claim, was, at));
    if (off > kDriveClaimSlop) {
        char buf[256];
        std::snprintf(buf, sizeof buf,
                      "%.2f from where your keys drove the server's copy of your kart (%.2f %.2f "
                      "%.2f, a step before %.2f %.2f %.2f; the claim %.2f %.2f %.2f, %zu new records)",
                      (double)off, (double)at.x, (double)at.y, (double)at.z, (double)was.x,
                      (double)was.y, (double)was.z, (double)move.x, (double)move.y, (double)move.z,
                      move.drive.size() - from);
        why = buf;
        return Drive::Refuse;
    }
    return Drive::Accept;
}

double join_patience(const Session &s)
{
    // A session that has spoken since its welcome is a player, and a player
    // sends a position claim twenty times a second: kSessionSilence is two
    // hundred and forty missed ones, which is absence and not quiet.
    if (s.spoke_since_welcome) return kSessionSilence;

    // One that has NOT is not quiet, it is waiting. The welcome is the biggest
    // message this protocol sends -- a block table and a media offer,
    // fragmented -- and on a link that drops packets it is still being
    // retransmitted while this timer runs. Dropping the seat then costs the
    // player a join they were in the middle of, and they cannot retry a server
    // that has already forgotten them. That is OPEN-12, seen once: the server
    // logged the join, sent the welcome, and dropped the player twelve seconds
    // later while the client was still waiting for it.
    //
    // The client's own patience for a join is kHandshakeRtts round trips, so
    // the server has to outlast that or its own window can never do anything.
    // It has no round trip to multiply -- it is not the side that measures one
    // -- but it does have how long THIS handshake took, on this connection,
    // which is ENet's exchange and the key exchange over the same link. On
    // loopback that is milliseconds and the floor below is what applies.
    const double by_link = s.joined_after * kJoinSilenceHandshakes;
    return by_link > kSessionSilence ? by_link : kSessionSilence;
}

double quiet_for(const Session &s, double now)
{
    // The later of the two, because a stretch in which the server was still
    // sending is a stretch in which the client had nothing to answer with. Ten
    // seconds of that followed by two seconds of real quiet is two seconds of
    // silence, not twelve.
    const double since = s.last_owed_at > s.last_heard_at ? s.last_owed_at : s.last_heard_at;
    return now - since;
}

void ServerSessions::note_owed(PeerId peer, double now)
{
    const auto it = m_sessions.find(peer);
    if (it == m_sessions.end()) return;
    it->second.last_owed_at = now;
}

std::vector<PeerId> ServerSessions::silent(double now) const
{
    // The server's own answer to "is anybody still there", which until now it
    // did not have: it left the question to ENet, and ENet's answer is a peer
    // timeout meant for a different kind of link. Now that this code holds that
    // timeout open -- so ENet stops closing sessions the engine still considers
    // alive -- a seat has to be freed by something, and this is it.
    //
    // A playing client sends a position claim twenty times a second, so any
    // gap this long is a client that is gone rather than one that is quiet.
    //
    // With quiet_for in it, this rule and ENet's timeout divide the two ways a
    // peer can stop answering, and they divide them exactly:
    //
    //   nothing outstanding, peer says nothing  -> this rule decides, because
    //       ENet's timeout only ever looks at reliable commands it is waiting
    //       for an acknowledgement of, and there are none.
    //   something outstanding, peer stops acknowledging -> ENet decides, at
    //       the wider window hold_peer gives it. That is what stops this from
    //       being a rule that never lets go: quiet_for holds the seat open only
    //       for as long as ENet is still willing to retransmit into it.
    //
    // No gap and no overlap, which is the only reason it is safe for this one
    // to stand aside while the server is talking.
    std::vector<PeerId> out;
    for (const auto &kv : m_sessions) {
        const Session &s = kv.second;
        if (s.state != SessionState::Ready) continue;
        if (s.last_heard_at <= 0.0) continue;
        if (quiet_for(s, now) > join_patience(s)) out.push_back(s.id);
    }
    return out;
}

const Session *ServerSessions::find(PeerId peer) const
{
    const auto it = m_sessions.find(peer);
    return it == m_sessions.end() ? nullptr : &it->second;
}

bool quick_target(Side from, bool container_open, Side &to)
{
    switch (from) {
    case Side::Container:
    case Side::Craft:
        // Both of them empty into the bag, which is the only place a player
        // can carry anything away in.
        to = Side::Bag;
        return true;

    case Side::Bag:
        // And the bag empties into whatever is open in front of it. With
        // nothing open there is nowhere to send it: shift-clicking inside one
        // bag would have to mean "the other half of the bag", and this
        // inventory has no halves -- the hotbar is the first nine squares of
        // the same thing, not a second container.
        if (!container_open) return false;
        to = Side::Container;
        return true;
    }

    return false;
}

const char *refused_by_mod(bool breaking)
{
    // Deliberately vague about WHICH mod, and deliberately specific about
    // there being one. "Something here will not let that be placed" tells a
    // player the world is not broken and that trying the next cell along is
    // worth doing; naming the mod would be naming an implementation detail at
    // somebody standing in a field.
    return breaking ? "something here will not let that be broken"
                    : "something here will not let that be placed";
}

std::string refused_for_tool(uint8_t needs, uint8_t tool_level, const std::string &material)
{
    char buf[160];
    if (material.empty()) {
        std::snprintf(buf, sizeof(buf), "that needs a level %u tool, and this one is level %u",
                      (unsigned)needs, (unsigned)tool_level);
        return buf;
    }

    // With the material in it, because the number alone reads as a lie to
    // somebody holding a level 3 axe in front of stone: the axe IS level 3, and
    // what it is not is a tool for this.
    std::snprintf(buf, sizeof(buf),
                  "that needs a level %u tool for %s, and this one is level %u against %s",
                  (unsigned)needs, material.c_str(), (unsigned)tool_level, material.c_str());
    return buf;
}

// One question asked by both: is this tool the right tool for this work? A tool
// that names nothing is right for everything, and a block that names nothing is
// the right work for every tool -- the two ways of saying "nobody said", and
// what everything registered before materials existed is.
// How early the Edit that ends a dig may arrive and still break the block. A
// client that times a dig exactly -- with the server's numbers, since protocol
// 36 -- sends its two Edits exactly that far apart, and the server sees them a
// little closer: measured on one machine, 41 digs arrived 0 to 4 ms early.
// Refused in silence, about half filled the bar a second time (reported from
// play); with this all 41 broke at the first. Wide enough for a real link's
// jitter, and a client that hurries gains this much a block and no more.
static constexpr double kDigSlack = 0.05;

static bool tool_is_for(const ItemDef &tool, const std::string &material)
{
    return tool.digs.empty() || material.empty() || tool.digs == material;
}

uint8_t tool_level_for(const ItemDef &tool, const std::string &material)
{
    return tool_is_for(tool, material) ? tool.dig_level : (uint8_t)0;
}

float tool_speed_for(const ItemDef &tool, const std::string &material, uint8_t ench)
{
    // Efficiency: half as fast again a level, where the tool is for the block.
    const float lv = (float)enchant_level_for(ench, Enchant::Efficiency);
    return tool_is_for(tool, material) ? tool.dig_speed * (1.0f + 0.5f * lv) : 1.0f;
}

int best_dig_slot(const ItemRegistry &items, const std::vector<ItemStack> &slots, int hand,
                  const std::string &material)
{
    const int arms = Inventory::kDefaultSlots + Inventory::kWorn;
    const int back = Inventory::kDefaultSlots + kWornBack - 1;
    if ((int)slots.size() < arms + Inventory::kArms || slots[(size_t)back].empty()) return hand;
    const auto worth = [&](int i) {
        if (i < 0 || (size_t)i >= slots.size() || slots[(size_t)i].empty()) return std::make_pair(0, 1.0f);
        const ItemStack &st = slots[(size_t)i];
        const ItemDef   &d  = items.get(st.item);
        return std::make_pair((int)tool_level_for(d, material), tool_speed_for(d, material, st.ench));
    };
    int best = hand;
    for (int a = 0; a < std::min<int>(items.get(slots[(size_t)back].item).arms, Inventory::kArms); ++a)
        if (worth(arms + a) > worth(best)) best = arms + a;
    return best;
}

uint16_t hit_damage(const ItemRegistry &items, const std::vector<ItemStack> &slots, int hand)
{
    const auto weapon = [&](int i) -> int {
        if (i < 0 || (size_t)i >= slots.size() || slots[(size_t)i].empty()) return 0;
        const ItemStack &st  = slots[(size_t)i];
        const int        own = items.get(st.item).damage;
        return own > 0 ? own + enchant_level_for(st.ench, Enchant::Sharpness) : 0;
    };
    int dmg = weapon(hand);
    if (dmg == 0) dmg = ecs::kPunch;
    const int arms = Inventory::kDefaultSlots + Inventory::kWorn;
    const int back = Inventory::kDefaultSlots + kWornBack - 1;
    if ((size_t)back < slots.size() && !slots[(size_t)back].empty())
        for (int a = 0; a < std::min<int>(items.get(slots[(size_t)back].item).arms, Inventory::kArms); ++a)
            dmg += weapon(arms + a);
    return (uint16_t)std::min(dmg, 65535);
}

bool ServerSessions::plan_edit(PeerId peer, const Edit &req, EditPlan &out, std::string &why,
                               double now)
{
    out = EditPlan{};

    Session *s = find_mutable(peer);
    if (!s || s->state != SessionState::Ready) {
        why = "not in the world";
        return false;
    }

    // DESIGN.md 4.4: a window looks in and changes nothing. Checked on scope
    // and not on whether the session has a bag, because every session has one --
    // an observer's is simply always empty, and a rule written as "you have no
    // inventory" would become false the day something puts an item in it.
    if (s->scope != Scope::Enter) {
        why = "a window may look into this world and not reach into it";
        return false;
    }

    if (!has_authority()) {
        why = "this server has no world to change";
        return false;
    }

    if (!s->authority().valid) {
        why = "the server does not know where you are yet";
        return false;
    }

    // Reach, from the position the SERVER accepted rather than the one the
    // client would like. Same rule and same number as placing an opening.
    const Vec3 eye{s->authority().x, s->authority().y + eye_of(*s), s->authority().z};
    const Vec3 node{(float)req.x + 0.5f, (float)req.y + 0.5f, (float)req.z + 0.5f};
    const float reach = length(node - eye);
    if (reach > m_limits.place_reach) {
        char buf[96];
        std::snprintf(buf, sizeof(buf), "that block is %.1f nodes away, further than %.1f",
                      (double)reach, (double)m_limits.place_reach);
        why = buf;
        return false;
    }

    if (req.breaking) {
        out.x = req.x;
        out.y = req.y;
        out.z = req.z;
        out.breaking = true;
        out.had  = m_world->get_block(req.x, req.y, req.z);
        out.want = kAir;

        if (out.had == kAir) {
            why = "there is nothing there";
            return false;
        }
        if (!m_registry->is_breakable(out.had)) {
            why = "that block does not break";
            return false;
        }

        // What is in the hand, read the same way the placement below reads it:
        // off the slot the request names, because a player digs with what they
        // are holding and the hotbar is what they are holding.
        // Or an arm's, when one of them digs this block better.
        const std::string &material = m_registry->get(out.had).material;
        const int          hand = (int)req.slot < Inventory::kHotbar ? (int)req.slot : 0;
        const int          dug_with = m_items ? best_dig_slot(*m_items, s->bag().slots(), hand, material) : hand;
        const ItemStack   &tool     = s->bag().at(dug_with);
        const bool has_tool = m_items && !tool.empty();

        // What the hand is worth AGAINST THIS BLOCK, which is not the same as
        // what it is worth: a level 3 axe is a bare hand in front of stone.
        const uint8_t      level =
            has_tool ? tool_level_for(m_items->get(tool.item), material) : (uint8_t)0;
        // And the body's own (core.player_factors): the faster of the old and
        // the new for the second its client may still dig by the old.
        const bool dig_window =
            s->movement_since >= 0.0 && now <= s->movement_since + m_limits.portal_grace;
        const float factor = dig_window ? std::max(s->dig_factor, s->dig_before) : s->dig_factor;
        const float speed =
            (has_tool ? tool_speed_for(m_items->get(tool.item), material, tool.ench) : 1.0f) * factor;

        // Whether it is enough. After `breakable` and not folded into it: the
        // floor of the world is not a very hard block, it is the floor, and no
        // tool that could ever exist takes it out -- so it must not be refused
        // with a sentence that says to go and find a better one.
        const uint8_t needs = m_registry->get(out.had).dig_level;
        if (!tool_digs(level, needs)) {
            why = refused_for_tool(needs, level, material);
            return false;
        }

        // And then how LONG it takes. Zero is instant, which is every block
        // that does not say otherwise, so this costs nothing until a mod says
        // a block is hard.
        //
        // The first swing at a cell starts the dig and changes nothing; a swing
        // before the seconds are up changes nothing either; the one after them
        // breaks the block. That is two Edits for a dig and not sixty, no
        // message of its own, and no way for a client to hurry it: the seconds
        // are counted here.
        //
        // The sentence is EMPTY on purpose, and it is the second place in this
        // engine where that means "this click is not finished" rather than "you
        // were told no" -- the first being a right click with an empty hand. A
        // dig that has started is not a refusal to read, and answering every
        // swing of it with a sentence would put one on the player's screen
        // twice a second. The player is not waiting on this answer; they are
        // waiting on their own clock, and the block coming away is the reply.
        const float hardness = m_registry->get(out.had).hardness;
        if (hardness > 0.0f) {
            const double need = dig_seconds(hardness, speed);
            const bool   same = s->digging && s->dig_block == out.had && s->dig_x == out.x &&
                              s->dig_y == out.y && s->dig_z == out.z;

            if (!same) {
                s->digging   = true;
                s->dig_x     = out.x;
                s->dig_y     = out.y;
                s->dig_z     = out.z;
                s->dig_block = out.had;
                s->dig_since = now;
                why.clear();
                return false;
            }
            if (now - s->dig_since < need - kDigSlack) {
                why.clear();
                return false;
            }
            s->digging = false;
            out.wears  = has_tool ? dug_with : -1;   // a tool spends a use on what took time
        }

        // What the bag gains, if anything. An unobtainable block -- one whose
        // mod stopped defining its item -- breaks and drops nothing, and that
        // is not a refusal.
        // The id straight, because a block with a front is one block: the cell
        // carries the turn, and the id in it is the one that has an item.
        out.drop = m_items ? m_items->of_block(out.had) : kNoItem;
        return true;
    }

    // Every right click is a USE of the block it landed on, whatever is in the
    // hand. Filled before any of the placement checks, so a refused placement
    // still leaves a use for a mod to claim -- a chest that will not open
    // because the cell above it is full is a chest that works only when the
    // player is holding nothing.
    //
    // The reach check and the "there is something there" check below have
    // already run by the time anybody reads this, which is what makes it a cell
    // the player can actually touch.
    out.use     = true;
    out.use_x   = req.x;
    out.use_y   = req.y;
    out.use_z   = req.z;
    out.use_had = m_world->get_block(req.x, req.y, req.z);

    // What is in the named slot, read here because here is where the slot is
    // known. Before every refusal below, so a mod that claims a click it would
    // have refused anyway still knows what was being offered.
    //
    // And the bag's hand follows the click, so a mod that takes from or gives
    // to the hand while answering the use (core.take_item) finds the slot the
    // player is holding and not the one of their last placement.
    {
        Inventory &holding = s->bag();
        const int  which   = (int)req.slot < Inventory::kHotbar ? (int)req.slot : 0;
        holding.select(which);
        out.use_holding = holding.at(which).empty() ? kNoItem : holding.at(which).item;
    }

    // Placing. The cell is DERIVED from the node that was hit and its face,
    // never taken from the wire: with a target cell in the message there is a
    // field in which a modified client can hang a block in mid-air.
    out.x = req.x + req.nx;
    out.y = req.y + req.ny;
    out.z = req.z + req.nz;
    out.breaking = false;
    out.slot     = (int)req.slot;

    if (!m_items) {
        why = "this world has no item table, so it can place nothing";
        return false;
    }

    // The face has to be a face of something. Building against thin air is how
    // a client reaches past the block it can actually see -- and there is
    // nothing to USE there either, so the use goes with it.
    if (out.use_had == kAir) {
        out.use = false;
        why     = "there is nothing there to build against";
        return false;
    }

    // Water is something to use -- a bucket dips into it -- and nothing to
    // build against: the use stands, the placement does not.
    if (m_registry->is_liquid(out.use_had)) {
        why = "water is not something to build against";
        return false;
    }

    // The hand before the cell, and the order is the message. Both can be
    // wrong at once -- an empty hand aimed at an occupied cell -- and "there is
    // already something there" sends the player to try another cell, where the
    // same thing happens, because the problem was never the cell.
    const Inventory &bag = s->bag();
    const ItemStack &held = bag.at((int)req.slot < Inventory::kHotbar ? (int)req.slot : 0);
    if (held.empty()) {
        // Not a refusal, and not a placement: there is nothing to put down, and
        // the use above is the whole of what this click was. Reported as a
        // refusal with an empty sentence, which the caller reads as "the use
        // stands, nothing else does" -- see SessionAction::edit_refusal.
        why = "";
        return false;
    }

    out.want = m_items->get(held.item).places;
    if (out.want == kAir) {
        why = "that is not something you can put down";
        return false;
    }

    // A block with a front is turned to face whoever put it down. From the
    // placer's POSITION, which is the authority's own state, and not from where
    // they were looking -- there is no look direction on the wire, and inventing
    // one for this would be a field a modified client fills in. The client's
    // single-process path uses the same rule from the same function, so the two
    // put the block down the same way round.
    //
    // The CELL is turned and not the id, since OPEN-27. A block with no front
    // is left at 0 rather than turned to face nobody: a facing on a block that
    // does not use one would still permute its side pictures, and a mod that
    // gave one four different sides would find them spinning under it.
    out.facing = node_placement_facing(m_registry->get(out.want).turn,
                                       (float)out.x + 0.5f - s->authority().x,
                                       (float)out.z + 0.5f - s->authority().z,
                                       req.nx, req.ny, req.nz);
    if (m_registry->get(out.want).piece.plan != 0) out.facing = (uint8_t)((out.facing + req.turn) & 3);

    out.had = m_world->get_block(out.x, out.y, out.z);

    // Air, or a liquid: a block put down in a lake takes the place of the water.
    if (out.had != kAir && !m_registry->is_liquid(out.had)) {
        why = "there is already something there";
        return false;
    }

    // Nobody may be built into. Every body, not only the one asking: a player
    // who can seal a neighbour inside a block can kill with it, and the neighbour
    // has no way to say no.
    // A track piece is every cell of it, and one turned (Edit::turn) may lie
    // back over whoever put it down.
    // Its cells, not its box: a turn's empty inner corner is nobody's.
    std::vector<AABB> cells{AABB{Vec3{(float)out.x, (float)out.y, (float)out.z},
                                 Vec3{(float)out.x + 1.0f, (float)out.y + 1.0f, (float)out.z + 1.0f}}};
    if (const PieceShape &p = m_registry->get(out.want).piece; p.plan != 0) {
        int ox, oz;
        piece_origin(p, out.facing, out.x, out.z, ox, oz);
        std::vector<PieceCell> pc;
        piece_cells(p, out.want, out.facing, ox, out.y, oz, pc);
        cells.clear();
        for (const PieceCell &c : pc)
            cells.push_back(AABB{Vec3{(float)c.x, (float)c.y, (float)c.z},
                                 Vec3{(float)c.x + 1.0f, (float)c.y + 1.0f, (float)c.z + 1.0f}});
    }
    for (const auto &kv : m_sessions) {
        const Session &other = kv.second;
        if (other.state != SessionState::Ready || other.scope != Scope::Enter) continue;
        if (!other.authority().valid) continue;

        const AABB box = body_box(Vec3{other.authority().x, other.authority().y,
                                       other.authority().z},
                                  other.movement.width * 0.5f);
        if (std::any_of(cells.begin(), cells.end(),
                        [&](const AABB &cell) { return boxes_overlap(box, cell); })) {
            why = (other.id == peer) ? "you are standing there"
                                     : "somebody is standing there";
            return false;
        }
    }

    return true;
}

void ServerSessions::edit_done(PeerId peer, const EditPlan &plan, double now)
{
    Session *s = find_mutable(peer);
    if (!s || !m_items) return;

    if (plan.breaking) {
        if (plan.wears >= 0) s->bag().wear_slot(*m_items, plan.wears, 1);
        if (plan.drop == kNoItem) return;

        // A full bag used to break the block and lose the drop -- M7_FINDINGS
        // 1.3 said so out loud and called it a decision. It stops being one now
        // that there is a ground for a stack to land on: what does not fit goes
        // where the block was.
        // Broken on somebody's ground, it is theirs (owned_tag).
        const uint32_t tag  = plan.own ? 0 : owned_tag_for(*s, plan.x, plan.y, plan.z);
        const uint16_t left = s->bag().add(*m_items, plan.drop, 1, -1, 0, 0, tag);
        if (left > 0)
            ecs::spawn_drop(m_ecs, ItemStack{plan.drop, left, 0, 0, tag}, (float)plan.x + 0.5f,
                            (float)plan.y + 0.5f, (float)plan.z + 0.5f, now);
        return;
    }

    s->bag().select(plan.slot);
    s->bag().spend_held(1);
}

void ServerSessions::set_owned(int x1, int z1, int x2, int z2, uint32_t tag)
{
    const OwnedArea a{std::min(x1, x2), std::min(z1, z2), std::max(x1, x2), std::max(z1, z2), tag};
    for (size_t i = 0; i < m_owned.size(); ++i) {
        OwnedArea &o = m_owned[i];
        if (o.x1 == a.x1 && o.z1 == a.z1 && o.x2 == a.x2 && o.z2 == a.z2) {
            if (tag == 0) m_owned.erase(m_owned.begin() + (ptrdiff_t)i);
            else          o.tag = tag;
            return;
        }
    }
    if (tag != 0) m_owned.push_back(a);
}

// ponytail: a scan of every area, a few hundred villages at most; a grid of
// regions when worlds own thousands.
uint32_t ServerSessions::owned_tag(int x, int y, int z) const
{
    const uint32_t tag = owned_at(x, z);
    return tag != 0 && m_own_cell && m_own_cell(x, y, z) ? 0 : tag;
}

static uint64_t column_key(int x, int z)
{
    return ((uint64_t)(uint32_t)x << 32) | (uint32_t)z;
}

void ServerSessions::set_owned_cell(int x, int z, uint32_t tag)
{
    if (tag == 0) m_owned_cells.erase(column_key(x, z));
    else          m_owned_cells[column_key(x, z)] = tag;
}

uint32_t ServerSessions::owned_at(int x, int z) const
{
    if (auto it = m_owned_cells.find(column_key(x, z)); it != m_owned_cells.end()) return it->second;
    for (const OwnedArea &o : m_owned)
        if (x >= o.x1 && x <= o.x2 && z >= o.z1 && z <= o.z2) return o.tag;
    return 0;
}

uint32_t ServerSessions::owned_tag_for(const Session &s, int x, int y, int z) const
{
    const uint32_t tag = owned_tag(x, y, z);
    for (uint32_t t : s.owner_of)
        if (t == tag) return 0;
    return tag;
}

std::vector<ContainerAt> ServerSessions::open_cells() const
{
    std::vector<ContainerAt> out;
    for (const auto &entry : m_sessions) {
        const Session &s = entry.second;
        if (!s.container_open) continue;

        bool already = false;
        for (const ContainerAt &at : out) already = already || at == s.container;
        if (!already) out.push_back(s.container);
    }
    return out;
}

std::vector<CellOpen> ServerSessions::open_cell_changes()
{
    std::vector<CellOpen> changed;

    std::unordered_set<Cell, CellHash> now_open;
    for (const ContainerAt &at : open_cells()) now_open.insert(at);

    for (const Cell &at : now_open)
        if (m_open_cells.find(at) == m_open_cells.end())
            changed.push_back(CellOpen{at.x, at.y, at.z, 1});

    for (const Cell &at : m_open_cells)
        if (now_open.find(at) == now_open.end())
            changed.push_back(CellOpen{at.x, at.y, at.z, 0});

    m_open_cells.swap(now_open);
    return changed;
}

std::vector<uint8_t> ServerSessions::block_change_message(int32_t x, int32_t y, int32_t z,
                                                          BlockId id, uint8_t facing,
                                                          const std::string &why)
{
    BlockChange m;
    m.x      = x;
    m.y      = y;
    m.z      = z;
    m.id     = (uint16_t)id;
    m.facing = facing;
    m.why    = why;
    return m.encode();
}

std::vector<PeerId> ServerSessions::inventories_to_send()
{
    std::vector<PeerId> owed;

    for (const PeerId peer : ready_peers()) {
        Session *s = find_mutable(peer);
        if (!s || s->inventory_sent) continue;

        s->inventory_sent = true;
        owed.push_back(peer);
    }
    return owed;
}

std::vector<uint8_t> ServerSessions::panel_message(const ui::Panel *p)
{
    PanelShow m;
    if (p == nullptr) return m.encode();   // open stays false: nothing is up

    m.open  = true;
    m.name  = p->name;
    m.owner = p->owner;
    m.w     = p->w;
    m.h     = p->h;
    m.elements.reserve(p->elements.size());

    for (const ui::Element &e : p->elements) {
        PanelShow::Elem out;
        out.kind = (uint8_t)e.kind;
        out.id   = e.id;
        out.text = e.text;
        out.x    = e.x;
        out.y    = e.y;
        out.w    = e.w;
        out.h    = e.h;
        out.r    = e.r;
        out.g    = e.g;
        out.b    = e.b;
        m.elements.push_back(std::move(out));
    }
    return m.encode();
}

std::vector<PeerId> ServerSessions::panels_to_send()
{
    std::vector<PeerId> owed;
    if (m_panels == nullptr) return owed;

    for (const PeerId peer : ready_peers()) {
        Session *s = find_mutable(peer);
        if (!s) continue;

        // Their own revision, which is the world's plus theirs: a notice
        // shown to everybody reaches this session, and somebody else's chest
        // does not.
        const uint64_t rev = m_panels->revision_for((ui::PlayerId)peer);
        if (s->panel_rev == rev) continue;

        s->panel_rev = rev;
        owed.push_back(peer);
    }
    return owed;
}

bool ServerSessions::open_container(PeerId peer, const ContainerAt &at)
{
    Session *s = find_mutable(peer);
    if (!s || s->state != SessionState::Ready || s->scope != Scope::Enter) return false;
    if (!m_containers || m_containers->find(at) == nullptr) return false;

    s->container_open = true;
    s->container      = at;

    s->container_owed = true;
    return true;
}

void ServerSessions::close_container(PeerId peer)
{
    Session *s = find_mutable(peer);
    if (!s || !s->container_open) return;

    s->container_open = false;

    // And owed, so the closing itself is sent: a client cannot tell "it
    // closed" from "nothing arrived this tick".
    s->container_owed = true;
}

void ServerSessions::erase_container(const ContainerAt &at)
{
    if (m_containers) m_containers->erase(at);

    // Everybody who had it open, whether or not there was one to erase: a
    // session pointing at a cell with no container is exactly the state that
    // makes the NEXT container there open by itself.
    for (auto &kv : m_sessions) {
        Session &s = kv.second;
        if (!s.container_open || !(s.container == at)) continue;
        s.container_open = false;
        s.container_owed = true;
    }
}

// Everything in a session's craft grid, back where a player can get at it.
//
// The bag first and the ground for what will not fit, which is the same
// bargain a break with a full bag strikes: half a rescue is better than none,
// and a stack that vanished because a window closed is the one thing a player
// cannot argue with.
//
// A member and not a free function because it needs the ECS to drop into, and
// the ECS here is the one the bodies live in.
void ServerSessions::set_spawn(float x, float y, float z)
{
    m_spawn.x = x;
    m_spawn.y = y;
    m_spawn.z = z;
}

// Whether a player's hand reaches a body: within place_reach of the eye of the
// position the server accepted, with nothing solid in between. For a blow and
// for a use alike -- one arm, one rule.
bool ServerSessions::in_reach(Session &s, flecs::entity e, const ecs::Position &pos,
                              std::string &why)
{
    const ecs::Position *p = &pos;
    // Reach, from the eye of the position the SERVER accepted to the middle of
    // the body -- the same measurement place_reach makes for a block, against
    // the same limit, because two reaches is two rules about one arm. The
    // message carries no position of its own for the reason Edit carries none.
    //
    // To the MIDDLE and not to the feet: a body is measured where it is, and a
    // player standing over a mob would otherwise be further from it than a
    // player standing beside it.
    // A creature by its kind's body (a dog is not a player's height).
    const ecs::Brain    *brain  = e.try_get<ecs::Brain>();
    const float          height = brain ? brain->height : e.has<ecs::Mob>() ? ecs::kMobHeight : kPlayerHeight;
    const MovementState &at     = s.authority();
    const Vec3           eye{at.x, at.y + eye_of(s), at.z};
    const Vec3           middle{p->x, p->y + height * 0.5f, p->z};
    const float          away = length(middle - eye);
    if (away > m_limits.place_reach) {
        char buf[96];
        std::snprintf(buf, sizeof(buf), "that is %.1f nodes away, further than %.1f", (double)away,
                      (double)m_limits.place_reach);
        why = buf;
        return false;
    }

    // And nothing solid in the way (TODO.md OPEN-15). The client chooses the
    // body by casting its own ray, and the server used to take that choice as
    // given if the body was within reach -- so a modified client could name one
    // round a corner or behind a wall. The server casts from the eye of the
    // position it accepted towards the middle it has just measured reach to,
    // and a solid block before the ray enters the body's box is a refusal.
    //
    // Towards the middle, because the message carries a target and no aim
    // point, and a server that took an aim point from the client would be
    // taking exactly what this rule is about. The price, said: a body whose
    // middle is hidden and whose head shows over a wall is refused.
    if (has_authority() && away > 1e-3f) {
        const Vec3  dir  = (middle - eye) * (1.0f / away);
        const float half = brain ? brain->half : e.has<ecs::Mob>() ? ecs::kMobHalfWidth : m_limits.half_width;
        const AABB  box{Vec3{p->x - half, p->y, p->z - half},
                       Vec3{p->x + half, p->y + height, p->z + half}};

        float into = away;   // where the ray enters the body; its middle if it never does
        ray_hits_box(box, eye, dir, away + 1.0f, into);

        const RaycastHit wall = raycast(*m_world, *m_registry, eye, dir, into);
        if (wall.hit && wall.distance < into) {
            char buf[128];
            std::snprintf(buf, sizeof(buf),
                          "there is something solid between you and that, %.1f nodes out",
                          (double)wall.distance);
            why = buf;
            return false;
        }
    }

    return true;
}

// A right click on a body: a sheep shorn (ecs::shear_mob), an animal fed
// (ecs::feed_mob), or a pet tamed with what is in the hand, or told to sit or
// stand (ecs::use_pet). The food or the item it took is spent here, and the
// shears wear.
bool ServerSessions::use_body(Session &s, uint64_t target, int slot, double now, std::string &why)
{
    if (s.scope != Scope::Enter) {
        why = "watching, not playing";
        return false;
    }
    if (!s.has_moved || !s.authority().valid) {
        why = "the server does not know where you are yet";
        return false;
    }
    flecs::entity e = m_ecs.entity((flecs::entity_t)target);
    const ecs::Position *p = e.is_alive() ? e.try_get<ecs::Position>() : nullptr;
    if (p == nullptr) {
        why = "there is nothing with that id";
        return false;
    }
    if (!in_reach(s, e, *p, why)) return false;

    const int        hand = slot < Inventory::kHotbar ? slot : 0;
    const ItemStack &held = s.bag().at(hand);
    if (!held.empty() && ecs::shear_mob(m_ecs, e, held.item, now)) {
        s.bag().wear_slot(*m_items, hand, 1);
        return true;
    }
    if (!held.empty() && ecs::feed_mob(e, held.item, now)) {
        s.bag().take_from(hand, 1);
        return true;
    }
    bool             took = false;
    if (!ecs::use_pet(e, s.name, held.empty() ? kNoItem : held.item, took, why)) return false;
    if (took) s.bag().take_from(hand, 1);
    return true;
}

ServerSessions::HitOutcome ServerSessions::strike(Session &s, uint64_t target, int slot,
                                                 double now, std::string &why)
{
    HitOutcome out;

    // A window is not a hand. The same scope rule every other reach into the
    // world makes, refused here rather than deeper because the reason is about
    // the session and not about the target.
    if (s.scope != Scope::Enter) {
        why = "watching, not playing";
        return out;
    }
    if (!s.has_moved || !s.authority().valid) {
        why = "the server does not know where you are yet";
        return out;
    }

    // How fast a hand may swing. On the server, because a client that sends one
    // of these every frame is the whole reason there is a rule.
    if (now - s.last_hit_at < ecs::kHitCooldown) {
        why = "too soon after the last one";
        return out;
    }

    flecs::entity e = m_ecs.entity((flecs::entity_t)target);
    if (!e.is_alive()) {
        why = "there is nothing with that id";
        return out;
    }
    if (e.id() == s.avatar.id()) {
        why = "you cannot strike yourself";
        return out;
    }
    if (!e.has<ecs::Body>()) {
        why = "that has no body";
        return out;
    }

    // A stack on the ground is walked into, not hit. It has no health, so the
    // check below would refuse it anyway; it is refused by name because "that
    // cannot be hurt" is the wrong thing to say about a thing you pick up.
    if (e.has<ecs::Dropped>()) {
        why = "a stack on the ground is picked up, not struck";
        return out;
    }

    if (!e.has<ecs::Health>()) {
        why = "that cannot be hurt";
        return out;
    }

    const ecs::Position *p = e.try_get<ecs::Position>();
    if (p == nullptr) {
        why = "that is nowhere";
        return out;
    }

    if (!in_reach(s, e, *p, why)) return out;

    const uint16_t before = e.get<ecs::Health>().hp;
    if (before == 0) {
        why = "that is already dead";
        return out;
    }

    // What the hand takes off: the item in the slot the hit names, off this
    // server's table, and a fist's for anything that says nothing. A slot past
    // the hotbar is the first square, as for an Edit.
    // Sharpness: a point more a level, on something that hits at all. And the
    // weapons in the pack's arms, added (hit_damage).
    const uint16_t dmg = m_items ? hit_damage(*m_items, s.bag().slots(), slot < Inventory::kHotbar ? slot : 0)
                                 : ecs::kPunch;

    out.landed = true;
    out.taken  = before < dmg ? before : dmg;
    out.killed = before <= dmg;
    s.last_hit_at = now;

    // A body with a session behind it goes through the same door a fall does:
    // hurt_body owes that player their Vitals, stops their regeneration, and
    // puts them back on their feet if that was the last point. Reaching into
    // the component here instead would be the second copy of a rule that
    // already exists.
    if (const ecs::DrivenBy *by = e.try_get<ecs::DrivenBy>()) {
        const auto it = m_sessions.find((PeerId)by->peer);
        if (it == m_sessions.end()) {
            // A body driven by a session that is gone. Nothing to hurt and
            // nothing to tell; said out loud rather than left to hurt a body
            // whose owner will never hear about it.
            out = HitOutcome{};
            why = "that body has no session any more";
            return out;
        }
        hurt_body(it->second, dmg, now);
        return out;
    }

    // And a body with nobody behind it. There is no Vitals to send -- nobody is
    // watching that bar -- and reap_dead is what turns the zero into a drop, on
    // the tick, so that dying is one rule in one place whatever did the killing.
    ecs::Health &h = e.get_mut<ecs::Health>();
    ecs::hurt(h, dmg);
    e.modified<ecs::Health>();

    // And a creature is knocked away from whoever struck it.
    if (ecs::Brain *b = e.try_get_mut<ecs::Brain>()) ecs::knock_back(*b, s.authority().x, s.authority().z, p->x, p->z);
    return out;
}

bool ServerSessions::hurt_body(Session &s, uint16_t amount, double now, bool armoured)
{
    if (amount == 0) return false;

    ecs::Health &h = s.vitals();
    if (!h.alive()) return false;   // a body at zero is not a bank

    if (armoured && m_items) {
        // Each worn piece takes a quarter of the blow as wear, at least one.
        const uint16_t wear = amount / 4 > 1 ? (uint16_t)(amount / 4) : (uint16_t)1;
        amount = ecs::armoured(amount, s.bag().armor(*m_items));
        if (s.bag().wear_out(*m_items, wear)) s.inventory_sent = false;
    }
    // core.player_factors: at least a point of a blow, unless they take none.
    if (s.hurt_factor != 1.0f && amount > 0)
        amount = s.hurt_factor <= 0.0f ? (uint16_t)0
                                       : (uint16_t)std::max(1.0f, std::round((float)amount * s.hurt_factor));
    if (amount == 0) return false;

    ecs::hurt(h, amount);
    ecs::regen_interrupt(s.regen);
    s.vitals_owed = true;

    if (h.alive()) return false;
    kill_body(s, now);
    return true;
}

void ServerSessions::kill_body(Session &s, double now)
{
    ecs::Health &h = s.vitals();

    // Whole again, here rather than on arrival. A body that walked around at
    // zero between dying and being put back is a body that anything could kill
    // a second time, and the second death would be the one the player never
    // saw coming.
    h.hp = h.max;
    ecs::regen_interrupt(s.regen);
    s.vitals_owed = true;

    // The bag is NOT spilled. That is a decision and not an omission: this
    // engine gives a player no way back to where they died -- no coordinates on
    // the screen they cannot turn off, no marker, no grave -- and a stack on
    // the ground lasts five minutes. A death that emptied the bag would delete
    // a session's work with no way to argue with it. Dying costs the place, and
    // the place is the thing this world has plenty of.

    // And back on their feet. There is no early return here for a table with
    // no spawn, because there is no such table: see SpawnPoint. The one that
    // used to be here is what let a body below the world be killed and healed
    // on every claim without ever being moved.
    MovementState &at = s.authority();
    at.x = s.has_bed ? s.bed[0] : m_spawn.x;
    at.y = s.has_bed ? s.bed[1] : m_spawn.y;
    at.z = s.has_bed ? s.bed[2] : m_spawn.z;

    // The speed check has to be told, or the claim that follows -- twenty nodes
    // from the last one -- is a cheat by every measure it has. The same grace a
    // portal crossing gets, because it is the same fact: the SERVER moved them.
    at.grace_until = now + m_limits.portal_grace;
    at.placed      = true;   // and the path starts from their first claim (OPEN-46)

    // And the fall in progress is not one this body took. Without this the
    // player is charged for the drop that killed them a second time, the moment
    // they land at the spawn.
    fall_reset(s.fall, at.y);

    s.interest.set_position(at.x, at.y, at.z, 0.0f, 0.0f, 1.0f);
    s.death_owed = true;
}

uint16_t ServerSessions::hurt(PeerId peer, uint16_t amount, double now, uint16_t bare)
{
    Session *s = find_mutable(peer);
    if (!s || s->state != SessionState::Ready || s->scope != Scope::Enter) return 0;

    bare = std::min(bare, amount);
    // A killing blow answers 0, as core.damage promises: the body is already
    // whole again for its way back, and its health would say nothing happened.
    if (hurt_body(*s, (uint16_t)(amount - bare), now)) return 0;
    if (hurt_body(*s, bare, now, false)) return 0;
    return s->vitals().hp;
}

uint16_t ServerSessions::heal(PeerId peer, uint16_t amount)
{
    Session *s = find_mutable(peer);
    if (!s || s->state != SessionState::Ready || s->scope != Scope::Enter) return 0;

    if (ecs::heal(s->vitals(), amount) > 0) s->vitals_owed = true;
    return s->vitals().hp;
}

bool ServerSessions::set_max_health(PeerId peer, uint16_t max)
{
    Session *s = find_mutable(peer);
    if (!s || s->state != SessionState::Ready || s->scope != Scope::Enter || max == 0) return false;

    ecs::Health &h = s->vitals();
    if (h.max == max) return true;
    h.max = max;
    h.hp  = std::min(h.hp, max);
    s->vitals_owed = true;
    return true;
}

uint16_t ServerSessions::health_of(PeerId peer) const
{
    const Session *s = find(peer);
    if (!s || s->state != SessionState::Ready || s->scope != Scope::Enter) return 0;

    return s->vitals().hp;
}

void ServerSessions::step_health(double dt)
{
    for (auto &kv : m_sessions) {
        Session &s = kv.second;
        if (s.state != SessionState::Ready || s.scope != Scope::Enter) continue;
        if (s.no_regen || s.regen_factor <= 0.0f) continue;

        const uint16_t back = ecs::regen_step(s.regen, s.vitals(), dt * s.regen_factor);
        if (back == 0) continue;

        ecs::heal(s.vitals(), back);
        s.vitals_owed = true;
    }
}

std::vector<PeerId> ServerSessions::vitals_to_send()
{
    std::vector<PeerId> owed;

    for (auto &kv : m_sessions) {
        Session &s = kv.second;
        if (s.state != SessionState::Ready || s.scope != Scope::Enter) continue;
        if (!s.vitals_owed) continue;

        s.vitals_owed = false;
        owed.push_back(kv.first);
    }

    return owed;
}

std::vector<uint8_t> ServerSessions::vitals_message(PeerId peer) const
{
    const Session *s = find(peer);

    Vitals v;
    if (s != nullptr && s->state == SessionState::Ready && s->scope == Scope::Enter) {
        v.health = s->vitals().hp;
        v.max    = s->vitals().max;
    } else {
        // Somebody with no body still gets a well-formed message: a maximum of
        // nothing does not decode, and a reply that cannot be read is worse
        // than one that says "none of it".
        v.health = 0;
        v.max    = ecs::kFullHealth;
    }

    return v.encode();
}

void ServerSessions::set_time(double t)
{
    m_time = clock_wrap(t);

    // Everybody hears about a time somebody SET at once. Drift is worth a
    // second of latency; a mod that says "it is night now" is not.
    for (auto &kv : m_sessions) kv.second.time_owed = true;
    m_since_told = 0.0;
}

void ServerSessions::set_day_length(double seconds)
{
    if (seconds > 0.0) m_day_length = seconds;
}

void ServerSessions::step_time(double dt)
{
    m_time = clock_advance(m_time, dt, m_day_length);
    m_wind += dt * kWindSpeed;

    // How long to wait is tell_interval's to say, and it says it once. Counted
    // forward here rather than by comparing two times, so there is no wrap to
    // get wrong when a server stalls for a while.
    m_since_told += dt;
    if (m_since_told < tell_interval(m_day_length)) return;

    m_since_told = 0.0;
    for (auto &kv : m_sessions) kv.second.time_owed = true;
}

std::vector<PeerId> ServerSessions::time_to_send()
{
    std::vector<PeerId> owed;

    for (auto &kv : m_sessions) {
        Session &s = kv.second;
        if (s.state != SessionState::Ready) continue;
        if (!s.time_owed) continue;

        s.time_owed = false;
        owed.push_back(kv.first);
    }

    return owed;
}

std::vector<uint8_t> ServerSessions::time_message() const
{
    Time t;
    t.tod  = clock_to_wire(m_time);
    t.wind = (uint32_t)(uint64_t)(m_wind * 100.0);   // wraps after a year of wind
    return t.encode();
}

std::vector<ServerSessions::Death> ServerSessions::deaths_to_serve()
{
    std::vector<Death> dead;

    for (auto &kv : m_sessions) {
        Session &s = kv.second;
        if (!s.death_owed) continue;

        s.death_owed = false;

        Death d;
        d.peer        = kv.first;
        d.back.x      = s.authority().x;
        d.back.y      = s.authority().y;
        d.back.z      = s.authority().z;
        d.back.look_x = 0.0f;
        d.back.look_y = 0.0f;
        d.back.look_z = 1.0f;
        d.back.reason = "you died";
        d.back.epoch  = placed(s, Vec3{d.back.x, d.back.y, d.back.z}, Vec3{},
                               std::atan2(d.back.look_x, -d.back.look_z), true);
        dead.push_back(d);
    }

    return dead;
}

void ServerSessions::empty_craft(Session &s, double now, int w, int h)
{
    if (!m_items) return;

    const std::vector<ItemStack> spilled = craft_resize(*m_items, s.craft, s.bag(), w, h);

    // What would not fit, where the player is standing. core/item has no world
    // to drop into and no idea what a drop is, so it hands them back and this
    // is the layer that can -- the same split ContainerSet::erase makes.
    const MovementState &at = s.authority();
    for (const ItemStack &rest : spilled)
        ecs::spawn_drop(m_ecs, rest, at.x, at.y + eye_of(s), at.z, now);

    s.inventory_sent = false;   // the bag changed, and it changed for a reason
}

bool ServerSessions::open_craft(PeerId peer, const ContainerAt &at, double now)
{
    Session *s = find_mutable(peer);
    if (!s || s->state != SessionState::Ready || s->scope != Scope::Enter) return false;

    // Emptied as it resizes, never after: the same nine slots are read as a
    // 2x2 or a 3x3, so anything still in there would come out of the resize in
    // a different square from the one the player put it in.
    empty_craft(*s, now, kCraftMax, kCraftMax);

    s->craft.table = true;
    s->craft.at    = at;
    s->craft_owed  = true;
    return true;
}

void ServerSessions::close_craft(PeerId peer, double now)
{
    Session *s = find_mutable(peer);
    if (!s) return;

    empty_craft(*s, now, 2, 2);

    s->craft.table = false;
    s->craft_owed  = true;
}

std::vector<PeerId> ServerSessions::close_far_crafts(double now)
{
    std::vector<PeerId> closed;

    // Collected first, because close_craft looks the session up again and may
    // drop stacks into the ECS -- neither of which belongs inside a loop over
    // the session map.
    std::vector<PeerId> walked;
    for (const auto &kv : m_sessions) {
        const Session &s = kv.second;
        if (s.state != SessionState::Ready || !s.authority().valid) continue;

        // "A table you can no longer reach is a table you are no longer at",
        // and the rule is craft_grid's so that the single-process loop asks the
        // same question -- see craft_out_of_reach.
        const MovementState &at = s.authority();
        if (!craft_out_of_reach(s.craft, at.x, at.y + eye_of(s), at.z,
                                m_limits.place_reach))
            continue;
        walked.push_back(kv.first);
    }

    for (const PeerId peer : walked) {
        close_craft(peer, now);
        closed.push_back(peer);
    }
    return closed;
}

ItemStack ServerSessions::craft_result(PeerId peer) const
{
    const Session *s = find(peer);
    if (!s || !m_recipes || !m_items) return ItemStack{};

    return m_recipes->craft(*m_items, s->craft.slots, s->craft.w, s->craft.h);
}

std::vector<uint8_t> ServerSessions::craft_message(PeerId peer) const
{
    CraftState m;

    const Session *s = find(peer);
    if (s == nullptr) return m.encode();

    m.w = (uint8_t)s->craft.w;
    m.h = (uint8_t)s->craft.h;
    if (s->craft.table) {
        m.x = s->craft.at.x;
        m.y = s->craft.at.y;
        m.z = s->craft.at.z;
    }

    m.slots.reserve((size_t)s->craft.used());
    for (int i = 0; i < s->craft.used(); ++i) {
        InventoryState::Slot sl;
        sl.item  = s->craft.slots.at(i).item;
        sl.count = s->craft.slots.at(i).count;
        sl.tag   = s->craft.slots.at(i).tag;
        m.slots.push_back(sl);
    }

    const ItemStack made = craft_result(peer);
    m.result.item  = made.item;
    m.result.count = made.count;
    m.result.tag   = made.tag;
    return m.encode();
}

std::vector<PeerId> ServerSessions::crafts_to_send()
{
    std::vector<PeerId> owed;

    for (const PeerId peer : ready_peers()) {
        Session *s = find_mutable(peer);
        if (!s) continue;

        // A number over what a viewer can SEE, and there is exactly one viewer:
        // a craft grid belongs to one player and nobody else can change it. So
        // the revision is bumped where the grid is written, and the flag covers
        // the two changes that write nothing -- opening and closing a table.
        if (!s->craft_owed) continue;

        s->craft_owed = false;
        owed.push_back(peer);
    }
    return owed;
}

std::vector<PeerId> ServerSessions::close_far_containers()
{
    std::vector<PeerId> closed;

    for (auto &kv : m_sessions) {
        Session &s = kv.second;
        if (s.state != SessionState::Ready || !s.container_open) continue;
        if (!s.authority().valid) continue;

        const MovementState &at = s.authority();
        if (container_in_reach(s.container, at.x, at.y + eye_of(s), at.z,
                               m_limits.place_reach))
            continue;

        s.container_open = false;
        s.container_owed = true;
        closed.push_back(kv.first);
    }
    return closed;
}

std::vector<uint8_t> ServerSessions::container_message(PeerId peer, double now) const
{
    ContainerState m;

    const Session *s = find(peer);
    const Inventory *chest =
        (s && s->container_open && m_containers) ? m_containers->find(s->container) : nullptr;
    if (chest == nullptr) return m.encode();   // open stays false: nothing is open

    m.open = true;
    m.x    = s->container.x;
    m.y    = s->container.y;
    m.z    = s->container.z;
    m.slots.reserve((size_t)chest->size());
    for (int i = 0; i < chest->size(); ++i) {
        InventoryState::Slot sl;
        sl.item  = chest->at(i).item;
        sl.count = chest->at(i).count;
        sl.used  = chest->at(i).used;
        sl.ench  = chest->at(i).ench;
        sl.tag   = chest->at(i).tag;
        m.slots.push_back(sl);
        m.roles.push_back(chest->role(i));
    }
    float done = 0.0f, rate = 0.0f;
    if (m_containers->progress(s->container, now, done, rate)) {
        m.progress = done;
        m.rate     = rate;
    }
    float pos = 0.0f, size = 0.0f;
    if (m_containers->scroll(s->container, pos, size)) {
        m.scroll      = pos;
        m.scroll_size = size;
    }
    m.search = m_containers->search(s->container, m.query);
    return m.encode();
}

std::vector<PeerId> ServerSessions::containers_to_send()
{
    std::vector<PeerId> owed;

    for (const PeerId peer : ready_peers()) {
        Session *s = find_mutable(peer);
        if (!s) continue;

        const uint64_t want =
            (s->container_open && m_containers) ? m_containers->revision(s->container) : 0;

        // The flag as well as the number, because opening and closing are
        // messages that do not change any contents: a session that just closed
        // a chest and one that has never opened one are at the same revision
        // and are owed different things. Somebody who has never opened
        // anything is owed nothing at all, which is why this is not a flag
        // alone either.
        if (!s->container_owed && s->container_rev == want) continue;

        s->container_owed = false;
        s->container_rev  = want;
        owed.push_back(peer);
    }
    return owed;
}

std::vector<uint8_t> ServerSessions::panel_message_for(PeerId peer) const
{
    return panel_message(m_panels ? m_panels->topmost_for((ui::PlayerId)peer) : nullptr);
}

std::vector<uint8_t> ServerSessions::inventory_message(PeerId peer) const
{
    const Session *s = find(peer);
    if (!s) return {};

    const Inventory &bag = s->bag();

    InventoryState m;
    m.selected = (uint8_t)bag.selected();
    m.slots.reserve((size_t)bag.size());
    for (int i = 0; i < bag.size(); ++i) {
        InventoryState::Slot sl;
        sl.item  = bag.at(i).item;
        sl.count = bag.at(i).count;
        sl.used  = bag.at(i).used;
        sl.ench  = bag.at(i).ench;
        sl.tag   = bag.at(i).tag;
        m.slots.push_back(sl);
    }
    return m.encode();
}

void ServerSessions::set_world(const World *world, const NodeRegistry *registry)
{
    m_world    = world;
    m_registry = registry;
}

std::vector<EntityState> ServerSessions::entities_for(PeerId peer, float radius) const
{
    std::vector<EntityState> out;

    const Session *me = find(peer);
    if (!me || !me->has_moved) return out;

    // Everything with a body near this player, and the sentence is the whole
    // reason for the ECS being here.
    //
    // What it used to say was "the other sessions, skipping the ones that are
    // not Ready, skipping the observers, skipping the ones that have not said
    // where they are" -- four facts about connections, and a mob would have
    // satisfied none of them. Those four are now one tag, put on when there is
    // something to draw and taken off when there is not, and this asks for the
    // tag. Anything else somebody adds to this world is streamed by the same
    // code with no change to it.
    const MovementState &here   = me->authority();
    const flecs::entity_t self  = me->avatar.id();

    m_ecs.each([&](flecs::entity e, const ecs::Position &p, const ecs::Look &look, ecs::Body) {
        if (e.id() == self) return;   // not yourself
        // What this player carries their own client draws: sent held (91).
        const ecs::UnseenBy *unseen = e.try_get<ecs::UnseenBy>();
        const bool held = unseen && unseen->peer == (uint32_t)peer;

        const float dx = p.x - here.x;
        const float dy = p.y - here.y;
        const float dz = p.z - here.z;
        if (dx * dx + dy * dy + dz * dz > radius * radius) return;

        EntityState s;
        s.id    = e.id();   // the whole id: index AND generation (OPEN-48)
        s.x     = p.x;
        s.y     = p.y;
        s.z     = p.z;
        s.yaw   = look.yaw;
        s.pitch = look.pitch;

        // And what it is. Read off the entity here rather than sent as its own
        // kind of message: this loop is already the one place that decides what
        // a player can see, and the alternative is the interest and distance
        // rules written a second time to say the same thing.
        if (const ecs::Dropped *d = e.try_get<ecs::Dropped>()) {
            // A stack of nothing is not a thing to draw, and it is not a thing
            // to send either: the wire says a Drop names an item, so one that
            // named none would be refused by the client that received it.
            if (d->stack.item == kNoItem) return;
            s.kind = EntityKind::Drop;
            s.item = d->stack.item;
        } else if (const ecs::Falling *f = e.try_get<ecs::Falling>()) {
            s.kind    = EntityKind::Drop;   // a block falling (73): the block's id, not an item
            s.item    = f->block;
            s.falling = true;
        } else if (e.has<ecs::Mob>()) {
            s.kind    = EntityKind::Mob;
            s.working = e.has<ecs::Working>();   // swinging or drawing (protocol 52)
            s.burning = e.has<ecs::Burning>();   // in the sun (55)
            s.held    = held;
            if (const ecs::Act *act = e.try_get<ecs::Act>()) {   // a worker's (81)
                s.item = act->held;
                s.clip = act->clip;
            }
        } else if (e.has<ecs::Thing>()) {
            // Something a mod made visible (TODO.md OPEN-20): not a person and
            // not a creature, and the client is told so rather than left to
            // draw it as whichever it guesses.
            s.kind = EntityKind::Thing;
            if (const ecs::Act *act = e.try_get<ecs::Act>()) s.clip = act->clip;   // core.ecs.clip (88)
            // A machine, or what rides one, rolled (98): its pitch is the look's.
            if (const ecs::Roll *r = e.try_get<ecs::Roll>()) {
                s.chassis   = true;
                s.body_roll = r->roll;
            }
            // And a machine's hull, to stand on (100).
            if (const ecs::Machine *m = e.try_get<ecs::Machine>(); m && m->sim) {
                const CraftSpec &spec = craft_spec(*m->sim);
                s.hull_w    = spec.half_w;
                s.hull_h    = spec.half_h;
                s.hull_l    = spec.half_l;
                s.hull_ride = craft_ride(*m->sim);
            }
        } else if (const Inventory *bag = e.try_get<Inventory>()) {
            // A person, and what is in their hand (protocol 48): the square
            // Hold last named, or the one an Edit or a hit last used.
            s.item      = bag->held().item;
            s.item_tint = (uint16_t)(bag->held().tag >> kTagIdBits);
            s.working   = e.has<ecs::Working>();
            s.crouching = e.has<ecs::Crouching>();
            if (const ecs::Posed *pose = e.try_get<ecs::Posed>()) s.clip = pose->clip;   // core.player_clip (92)
            if (const ecs::Livery *l = e.try_get<ecs::Livery>()) s.livery = l->bits;     // 93
            if (const ecs::Dressed *d = e.try_get<ecs::Dressed>())                     // 102
                for (int k = 0; k < ecs::kDressSlots; ++k) {
                    s.dress[k]      = d->model[k];
                    s.dress_tint[k] = d->tint[k];
                }
            // Carried on a seat: facing the way what carries them faces,
            // whatever they look at (their look is their head's).
            if (const ecs::DrivenBy *by = e.try_get<ecs::DrivenBy>())
                if (const Session *rider = find((PeerId)by->peer); rider && rider->riding != 0)
                    s.yaw = rider->heading;
            // Sat in a slide: facing down it, whatever they look at.
            if (has_authority()) {
                const Chute c = chute_at(*m_world, *m_registry, p.x, p.y, p.z);
                if (c.on) s.yaw = std::atan2(c.tx, -c.tz);
            }
            // Lying (75): turned the way the head points, whatever they look at.
            if (const ecs::Lying *l = e.try_get<ecs::Lying>()) {
                s.lying     = true;
                s.yaw       = l->head;
                s.crouching = false;
            }
            // And what they wear (protocol 58), head to feet.
            for (int k = 0; k < Inventory::kWorn; ++k) {
                s.worn[k]      = bag->at(Inventory::kDefaultSlots + k).item;
                s.worn_tint[k] = (uint16_t)(bag->at(Inventory::kDefaultSlots + k).tag >> kTagIdBits);
            }
            for (int k = 0; k < Inventory::kArms; ++k)   // 96
                s.arm[k] = bag->at(Inventory::kDefaultSlots + Inventory::kWorn + k).item;
            // And in a kart, how it sits (66): off the server's own copy of
            // the chassis, which its claims have to agree with to the bit.
            const Session *o = driver_of(e.id());
            s.flying = o != nullptr && o->flying && !s.lying;   // 97
            // On a mod's machine: at its wheel (a clip of driving alone,
            // drive_*) facing the way it does; anywhere else on it -- a
            // gunner, a passenger with a gun -- the way they look.
            if (o != nullptr && o->riding_thing != 0 && s.clip.rfind("drive_", 0) == 0 &&
                s.clip.find('+') == std::string::npos)
                s.yaw = o->heading;
            if (o != nullptr && o->movement.drives() && o->drive.body.vehicle) {
                const Player &b = o->drive.body;
                s.chassis    = true;
                s.body_pitch = b.body_pitch;
                s.body_roll  = b.body_roll;
                s.base_dx    = b.body_base.x - b.pos.x;
                s.base_dy    = b.body_base.y - b.pos.y;
                s.base_dz    = b.body_base.z - b.pos.z;
                const DriveLook look = vehicle_look(b);   // 103
                s.boosting = look.boosting;
                s.sliding  = look.sliding;
            }
        }

        // And what it looks like, read off the body the same way. Never on a
        // drop: a stack on the ground is drawn as the stack it is, and the
        // decoder on the other side refuses a drop that wears anything -- so
        // putting one here would cost the client the whole message, not the
        // costume.
        if (s.kind != EntityKind::Drop)
            if (const ecs::Wears *w = e.try_get<ecs::Wears>()) s.model = w->model;

        out.push_back(s);
    });

    // More than a message holds: people and a mod's things first -- a
    // machine, the target a world's fight is about -- then the nearest. Cut
    // by id, as this used to be, the newest went: in a war world every
    // machine bought and every target put up late was the one never drawn,
    // behind a hundred soldiers made before them.
    // And what was sent last time is kept a little longer than the distance
    // alone would keep it, so the bodies at the edge do not come and go from
    // one tick to the next.
    std::unordered_set<uint64_t> &last = m_last_sent[peer];
    if (out.size() > kMaxEntities) {
        auto rank = [&](const EntityState &s) {
            const float dx = s.x - here.x, dy = s.y - here.y, dz = s.z - here.z;
            const bool  first = s.kind == EntityKind::Player || s.kind == EntityKind::Thing;
            const float d2    = dx * dx + dy * dy + dz * dz;
            return std::make_pair(first ? 0 : 1, last.count(s.id) ? d2 * 0.5f : d2);
        };
        std::nth_element(out.begin(), out.begin() + kMaxEntities, out.end(),
                         [&](const EntityState &a, const EntityState &b) { return rank(a) < rank(b); });
        out.resize(kMaxEntities);
    }
    last.clear();
    for (const EntityState &s : out) last.insert(s.id);

    // By id, so two ticks in a row put the same player in the same slot and a
    // client interpolating between messages is not matching up strangers.
    std::sort(out.begin(), out.end(),
              [](const EntityState &a, const EntityState &b) { return a.id < b.id; });
    return out;
}

const Session *ServerSessions::driver_of(uint64_t entity_id) const
{
    for (const auto &kv : m_sessions)
        if (kv.second.avatar.id() == entity_id) return &kv.second;
    return nullptr;
}

Session *ServerSessions::find_mutable(PeerId peer)
{
    const auto it = m_sessions.find(peer);
    return it == m_sessions.end() ? nullptr : &it->second;
}

std::vector<PeerId> ServerSessions::ready_peers() const
{
    std::vector<PeerId> out;
    for (const auto &kv : m_sessions)
        if (kv.second.state == SessionState::Ready) out.push_back(kv.first);
    std::sort(out.begin(), out.end());
    return out;
}

size_t ServerSessions::player_count() const
{
    size_t n = 0;
    for (const auto &kv : m_sessions)
        if (kv.second.state == SessionState::Ready && kv.second.scope == Scope::Enter) ++n;
    return n;
}

size_t ServerSessions::observer_count() const
{
    size_t n = 0;
    for (const auto &kv : m_sessions)
        if (kv.second.state == SessionState::Ready && kv.second.scope == Scope::Observe) ++n;
    return n;
}

size_t ServerSessions::ready_count() const
{
    size_t n = 0;
    for (const auto &kv : m_sessions)
        if (kv.second.state == SessionState::Ready) ++n;
    return n;
}

} // namespace net
} // namespace cnp
