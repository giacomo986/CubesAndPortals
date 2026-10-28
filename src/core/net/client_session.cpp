#include "core/net/client_session.h"

#include "core/net/bytes.h"
#include "core/net/chunk_codec.h"
#include "core/registry/core_nodes.h"
#include "core/version.h"

#include <algorithm>
#include <chrono>

namespace cnp {
namespace net {

namespace {
double steady_seconds()
{
    return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
}
} // namespace

double ClientSession::wind() const
{
    return m_has_time ? m_wind + (steady_seconds() - m_wind_at) * kWindSpeed : 0.0;
}

const char *client_state_text(ClientState s)
{
    switch (s) {
    case ClientState::Idle:        return "idle";
    case ClientState::Connecting:  return "connecting";
    case ClientState::Handshaking: return "handshaking";
    case ClientState::Playing:     return "playing";
    case ClientState::Refused:     return "refused";
    case ClientState::TimedOut:    return "timed out";
    case ClientState::Failed:      return "failed";
    case ClientState::Closed:      return "closed";
    }
    return "?";
}

Hello make_client_hello(const std::string &player_name)
{
    Hello h;
    h.engine_version = kEngineVersion;

    // Derived, not declared. The version string is what a person reads; this is
    // what the handshake decides on, and it is computed from the block table
    // this build actually has rather than from what it calls itself.
    NodeRegistry local;
    register_core_nodes(local);
    h.content_hash = registry_content_hash(local);

    h.player_name = player_name;
    h.livery      = g_client_livery;

    // Only what is implemented. A client that lists a feature it does not have
    // is asking a server to send it something it cannot read.
    h.features = {kFeatureContentAdoption};
    return h;
}

ClientSession::~ClientSession()
{
    close();
}

bool ClientSession::connect(const ClientConfig &cfg, const Hello &hello, double now,
                            std::string &err)
{
    close();

    m_cfg     = cfg;
    m_hello   = hello;
    m_started   = now;
    m_joined_in = -1.0;
    m_error.clear();
    m_chunk_error.clear();
    m_players.clear();
    m_chunks          = 0;
    m_chunks_repeated = 0;
    m_entity_messages = 0;
    m_corrections     = 0;
    m_has_correction  = false;
    m_move_seq        = 0;
    m_last_move       = -1.0;
    m_epoch           = 0;
    m_epoch_heard     = 0;
    m_gen             = 0;
    m_drive.clear();
    m_drive_first     = 0;
    for (uint32_t &at : m_drive_sent) at = 0;
    m_unknown         = 0;

    // A new connection has heard nothing yet. Without this a reconnect starts
    // holding the previous server's last heartbeat and a last-heard time from
    // before the outage -- which reads as a world that is already stale, and as
    // a world state that was never sent by the session showing it.
    m_beat       = Heartbeat{};
    m_beats      = 0;
    m_last_beat  = 0.0;
    m_beat_seq   = 0;
    m_beats_lost = 0;
    m_beat_gap_max = 0.0;

    // Same rule for the media. The offer belongs to the world that made it, and
    // a reconnect that kept the previous one would resolve a block's texture
    // through a table the new world never sent -- which is exactly the mistake
    // per-session content ids exist to prevent. The CACHE is not cleared: it is
    // shared on purpose, and everything in it is named by what it is.
    // Anything this session had the right to fetch and did not: given back, or
    // no other session on this client could ever fetch it until the claim aged
    // out.
    if (m_cfg.media_cache)
        for (size_t i = 0; i < m_media_state.size() && i < m_media_offer.size(); ++i)
            if (m_media_state[i].asked && !m_media_state[i].have)
                m_cfg.media_cache->release(m_media_offer[i].hash);

    m_media_offer.clear();
    m_media_state.clear();
    m_media_had       = 0;
    m_media_received  = 0;
    m_media_missing   = 0;
    m_media_rejected  = 0;
    m_media_refused_by_server = 0;
    m_media_in_flight = 0;
    m_media_looked_at = 0.0;
    m_media_renewed_at = 0.0;
    m_hold_set_at      = 0.0;
    m_media_version   = 0;
    m_media_bytes     = 0;

    m_transport.set_encryption(cfg.encrypt);
    if (cfg.encrypt && cfg.pin_server) m_transport.pin_server_key(cfg.server_key);

    ++m_attempts;
    m_retry_at         = 0.0;
    m_closed_by_caller = false;

    if (!m_transport.connect(cfg.host, cfg.port, err)) {
        m_state = ClientState::Failed;
        m_error = err;
        return false;
    }

    m_state = ClientState::Connecting;
    return true;
}

bool ClientSession::given_up() const
{
    if (m_gave_up || m_closed_by_caller) return true;
    // Nothing scheduled and nothing live: a caller that asked for no retries,
    // after the one attempt it did ask for.
    return m_cfg.retry_first <= 0.0 && m_state != ClientState::Idle &&
           m_state != ClientState::Playing && m_state != ClientState::Connecting &&
           m_state != ClientState::Handshaking;
}

// Whether this session should open another socket, and when.
//
// The rule is one sentence: a session that is supposed to exist and does not
// tries again, at a rate that goes down. Everything else here is what "supposed
// to exist" excludes -- a session the caller closed, and a refusal that trying
// again cannot change.
void ClientSession::consider_retry(double now)
{
    if (m_gave_up || m_cfg.retry_first <= 0.0) return;

    // Still going, or deliberately shut: neither is a thing to reconnect. A
    // session the caller closed stays closed -- reconnecting it would be this
    // file second-guessing the caller.
    if (m_state == ClientState::Idle || m_state == ClientState::Connecting ||
        m_state == ClientState::Handshaking || m_state == ClientState::Playing)
        return;
    // Closed by the caller stays closed; closed because the other end went
    // away is the outage this exists for.
    if (m_closed_by_caller) return;

    if (m_state == ClientState::Refused && refusal_is_permanent(m_refusal.reason)) {
        // A decision, not an outage. Reconnecting every minute to be told the
        // same thing costs the server a handshake a minute forever and puts the
        // same line in both logs.
        m_gave_up  = true;
        m_retry_at = 0.0;
        return;
    }

    if (m_retry_at == 0.0) {
        // First failure since the last success: start the clock. The delay is
        // NOT reset here -- it is reset when a session reaches Playing, so a
        // server that flaps does not get a fresh one-second retry every time.
        if (m_retry_delay <= 0.0) m_retry_delay = m_cfg.retry_first;
        m_retry_at = now + m_retry_delay;
        return;
    }

    if (now < m_retry_at) return;

    // Doubling before the attempt rather than after it, so `retry_delay()` is
    // the wait that is about to be used -- which is what a caller logs.
    const ClientConfig cfg   = m_cfg;
    const Hello        hello = m_hello;
    const double       next  = m_retry_delay * 2.0 > m_cfg.retry_max ? m_cfg.retry_max
                                                                     : m_retry_delay * 2.0;

    std::string err;
    connect(cfg, hello, now, err);   // resets the session; the counters survive it
    m_retry_delay = next;
}

void ClientSession::use(World &world, NodeRegistry &registry)
{
    m_world    = &world;
    m_registry = &registry;
}

void ClientSession::use(World &world, NodeRegistry &registry, MovementRules &movement)
{
    use(world, registry);
    m_movement = &movement;
}

void ClientSession::close()
{
    if (m_transport.open()) m_transport.close();
    if (m_state == ClientState::Playing) m_state = ClientState::Closed;
    m_closed_by_caller = true;
    m_retry_at         = 0.0;
}

void ClientSession::fail(std::string why)
{
    // The first reason is the true one. A failure often produces a second event
    // -- the server closing the connection -- and overwriting the reason with
    // "the connection was closed" is how a diagnosis gets lost.
    if (m_state == ClientState::Failed || m_state == ClientState::Refused ||
        m_state == ClientState::TimedOut)
        return;
    m_state = ClientState::Failed;
    m_error = std::move(why);
}

void ClientSession::pump(double now, int timeout_ms)
{
    NetEvent ev;
    while (m_transport.poll(ev, timeout_ms)) {
        timeout_ms = 0;   // wait once; drain the rest without blocking

        switch (ev.type) {
        case NetEvent::Type::Connected: {
            m_state = ClientState::Handshaking;

            // The proof is signed HERE and not by whoever built the hello,
            // because what it is signed over does not exist until now: the
            // channel binding is a product of the key exchange that just
            // finished. A hello prepared in advance carries a proof of nothing.
            Hello hello = m_hello;
            hello.scope = m_cfg.scope;
            hello.room  = m_cfg.room;

            // The ticket, when the caller has one for this world. Nothing about
            // it depends on the session -- it was signed elsewhere, about a
            // player and a server, and that is exactly what makes it worth
            // carrying: it is checked against a key this server already holds,
            // not against anything either end of this connection just agreed.
            if (m_cfg.has_ticket) {
                hello.has_ticket = true;
                hello.ticket     = m_cfg.ticket;
            }
            if (m_cfg.has_identity) {
                uint8_t binding[kKeyBytes];
                if (m_transport.channel_binding(ev.peer, binding)) {
                    const std::vector<uint8_t> challenge =
                        identity_challenge(binding, hello.player_name);

                    uint8_t signature[kSignatureBytes];
                    sign_message(m_cfg.identity, challenge.data(), challenge.size(), signature);

                    hello.identity.assign(m_cfg.identity.public_key,
                                          m_cfg.identity.public_key + kSignPublicBytes);
                    hello.proof.assign(signature, signature + kSignatureBytes);
                } else {
                    // No binding means no encryption. Sending the key with a
                    // proof of nothing would be worse than joining anonymously:
                    // it would put this player's public key on a wire where
                    // anybody can pick it up and claim it on a server that also
                    // does not check. The server refuses that case anyway; this
                    // is the client not asking for it.
                    m_identity_unproven = true;
                }
            }

            m_transport.send(ev.peer, Channel::Handshake, hello.encode());
            break;
        }

        case NetEvent::Type::Disconnected:
            if (m_state == ClientState::Playing) {
                // Nobody here decided this: ENet did, on a peer that had a
                // reliable command outstanding for too long. Say how long since
                // the last thing that arrived, because "closed" on its own is a
                // report with the one useful number missing.
                m_error = "ENet gave up on the peer; last heard from " +
                          std::to_string((int)((now - m_last_beat) * 1000.0)) +
                          " ms ago, round trip " +
                          std::to_string((int)(round_trip() * 1000.0)) + " ms";
                m_state = ClientState::Closed;
            }
            else if (!ev.message.empty())
                // The transport had a reason -- today that means the key
                // exchange refused, and "the connection closed" would throw
                // away the only sentence worth showing the player.
                fail(ev.message);
            else if (m_state != ClientState::Refused && m_state != ClientState::Failed)
                fail("the server closed the connection without answering");
            break;

        case NetEvent::Type::Received:
            handle(ev.data, now, ev.channel);
            break;

        case NetEvent::Type::None:
            break;
        }
    }

    // A server that accepts the socket and then says nothing is a case the
    // caller cannot distinguish from a slow one without a clock. It gets its own
    // failure rather than a hang.
    if ((m_state == ClientState::Connecting || m_state == ClientState::Handshaking) &&
        now - m_started > handshake_window()) {
        // Which half it died in, because the two have different cures. Still
        // "connecting" means ENet never finished its own handshake and this
        // code has not spoken yet; "handshaking" means the socket is up and the
        // hello, the key exchange or the welcome went missing.
        m_error = std::string("no answer in ") + std::to_string((int)handshake_window()) +
                  " seconds, still " + client_state_text(m_state) + " (round trip " +
                  std::to_string((int)(round_trip() * 1000.0)) + " ms)";
        m_state = ClientState::TimedOut;
    }

    // ENet's patience, kept wider than this session's own.
    //
    // Both grow with the round trip, and this one has to stay outside: the
    // silence window below can say WHY a world stopped answering, and a peer
    // ENet closed first says only "closed". Set once a second because the round
    // trip it is derived from moves.
    if (m_state == ClientState::Playing && now - m_hold_set_at > 1.0) {
        m_hold_set_at = now;
        m_transport.hold_peer(m_transport.server_peer(), 2.0 * heartbeat_window());
    }

    // Silence from a server that had been speaking.
    //
    // This is the case ENet cannot answer: a server whose loop has wedged keeps
    // answering pings while streaming nothing, and to the socket that is a
    // healthy connection. Only checked once a heartbeat has arrived, so a
    // server that does not send them is never declared dead for not sending
    // them.
    if (m_state == ClientState::Playing && m_beats > 0 && m_cfg.heartbeat_silence > 0.0) {
        m_silence_window = heartbeat_window();
        if (now - m_last_beat > m_silence_window) {
            m_transport.close();
            fail("the server stopped answering for " +
                 std::to_string((int)m_silence_window) + " seconds (round trip " +
                 std::to_string((int)(round_trip() * 1000.0)) + " ms)");
        }
    }

    // A session that got in is a session whose next outage starts from the
    // beginning again. Without this a server that comes back after an hour is
    // retried an hour later the next time it hiccups.
    if (m_state == ClientState::Playing) {
        m_retry_delay = 0.0;
        m_retry_at    = 0.0;
    }

    // A media request that was never answered. A world only offers what it
    // holds, so this does not happen against a correct server -- and a client
    // that has no bound on it is a client whose renderer waits for a file that
    // is never coming, on a server that simply stopped answering.
    if (m_media_in_flight > 0 && now - m_media_asked_at > kMediaSilence) {
        // A backstop, and only that. Every hash asked for is now answered --
        // with the file or with a MediaMissing -- so this cannot be how the
        // client learns a file is not coming, and it must not ask again: the
        // first copy is still queued on a reliable channel, and asking twice is
        // how one stall became 24 duplicate files. What it does is stop
        // WAITING, so the renderer draws what did arrive.
        m_media_dropped += m_media_in_flight;
        m_media_in_flight = 0;
    }

    // The claims this session holds, kept alive because this session is.
    //
    // Their timeout exists for a session that asked and then died -- a process
    // that went away mid-download must not make a file unfetchable. It is not
    // there to decide that a transfer is taking too long: at 400 ms and 25%
    // loss a batch can go twenty seconds with nothing landing while ENet works
    // through a retransmission, and letting the claims lapse under it let the
    // other window fetch the same files, 50 for 31 distinct.
    //
    // So the claim follows the session and not the transfer. Once a second,
    // because the only thing on the other end of it is a twenty second timer.
    if (m_state == ClientState::Playing && m_cfg.media_cache && m_media_in_flight > 0 &&
        now - m_media_renewed_at > 1.0) {
        m_media_renewed_at = now;
        for (size_t i = 0; i < m_media_state.size(); ++i)
            if (m_media_state[i].asked && !m_media_state[i].have)
                m_cfg.media_cache->renew(m_media_offer[i].hash, now);
    }

    // Files another session on this client was fetching. Nothing tells this one
    // when they land, so it looks again -- four times a second, and only while
    // there is something it has neither got nor asked for.
    if (m_media_in_flight == 0 && media_unasked() && now - m_media_looked_at > 0.25) {
        m_media_looked_at = now;
        request_missing_media(now);
    }

    consider_retry(now);

    // Players nothing has been heard about stop being drawn. Entity updates are
    // unreliable, so a gap is normal and only a long one means anything.
    if (m_cfg.forget_players > 0.0) {
        const double cutoff = now - m_cfg.forget_players;
        m_players.erase(std::remove_if(m_players.begin(), m_players.end(),
                                       [&](const RemotePlayer &p) { return p.last_seen < cutoff; }),
                        m_players.end());
    }
}

double ClientSession::round_trip() const
{
    const double rtt = m_transport.round_trip(m_transport.server_peer());
    if (rtt > 0.0) m_last_rtt = rtt;

    // The last one measured, for a session that has already been closed. A
    // report that says "round trip 0 ms" about the link it just gave up on is
    // a report that lost the one number that explains the decision.
    return rtt > 0.0 ? rtt : m_last_rtt;
}

double ClientSession::heartbeat_window() const
{
    // A floor in seconds and a multiple of the round trip, whichever is larger.
    //
    // The floor is what this always was, and it keeps every loopback number
    // identical -- at 0.3 ms the multiple is nothing. What it could not do is
    // survive a real link: at 10% loss the worst gap measured over forty
    // seconds was 2.7 s against a 5 s window, and at 20% it was 9.2 s.
    const double rtt = round_trip();
    const double by_rtt = rtt * kHeartbeatSilenceRtts;
    return by_rtt > m_cfg.heartbeat_silence ? by_rtt : m_cfg.heartbeat_silence;
}

double ClientSession::handshake_window() const
{
    // A join is several reliable exchanges in a row: ENet's own connect, the key
    // offer and accept, then the hello and the welcome. On a link that drops
    // packets each of those can need a retransmission, and the retransmission
    // timeout starts near the round trip and doubles -- so what this has to
    // cover grows with the link, which a constant cannot do.
    //
    // The round trip is not known until ENet's connect has completed, so early
    // on this is the floor and nothing else. That is the right way round: the
    // part that has no measurement yet is the part a fixed number still guards.
    const double by_rtt = round_trip() * kHandshakeRtts;
    return by_rtt > m_cfg.handshake_timeout ? by_rtt : m_cfg.handshake_timeout;
}

bool ClientSession::send_mod(const std::string &mod, const std::string &text)
{
    if (m_state != ClientState::Playing) return false;

    const uint8_t channel = channel_for(mod);
    if (channel < kModChannelBase) return false;
    if (text.size() > kMaxWireModText) return false;

    m_transport.send(m_transport.server_peer(), (Channel)channel, ModData{text}.encode(), true);
    m_transport.flush();
    return true;
}

void ClientSession::handle(const std::vector<uint8_t> &data, double now, uint8_t channel)
{
    ByteReader r(data.data(), data.size());

    // A mod's own channel, and this layer does not read what is on it. Kept for
    // whoever owns the mods to hand to them: the CHANNEL says which mod, which
    // is what the handshake handed out, and this turns that number back into
    // the name the mod knows itself by.
    if (channel >= kModChannelBase) {
        ModData said;
        if (!ModData::decode(r, said)) return;
        for (const ChannelAssignment &c : m_welcome.channels)
            if (c.id == channel) {
                if (m_mod_messages.size() < 256) m_mod_messages.push_back({c.name, said.text});
                return;
            }
        return;
    }

    switch (peek_type(data.data(), data.size())) {
    case MsgType::Welcome: {
        if (m_state == ClientState::Playing) {
            fail("a second welcome arrived after the join");
            break;
        }
        if (!Welcome::decode(r, m_welcome)) {
            fail("the welcome does not parse");
            break;
        }

        // The client checks the answer before it believes it. Defence in depth:
        // the server has already refused everything it should have, and this
        // catches the case where it did not.
        std::string why;
        if (!accept_welcome(m_welcome, m_hello, why)) {
            fail(std::move(why));
            break;
        }
        if (m_cfg.expect_signer &&
            !(m_welcome.has_signing_key &&
              std::memcmp(m_welcome.signing_key, m_cfg.signer, kSignPublicBytes) == 0)) {
            fail("not the server the portal leads to: its key is not the one the link names");
            break;
        }

        // Adopt the server's block table. From here the ids in every chunk are
        // the server's ids, not this build's, and that is the point: a name this
        // build has never heard of becomes a magenta placeholder instead of a
        // reason to refuse the join.
        NodeRegistry local;
        register_core_nodes(local);
        m_unknown = adopt_node_table(*m_registry, m_welcome.nodes, local);

        // And their shapes, which arrive beside the names since protocol 27.
        //
        // Over whatever the local definition said, including for a name this
        // build knows: the world being joined is the one that decides what is
        // in it, and two copies of one mod that disagree about a box are a
        // player standing where the server says they cannot. A block the
        // welcome says nothing about is a cube that does not turn, which is
        // what the placeholder and every plain block already are.
        for (const net::NodeShapeDesc &d : m_welcome.shapes) {
            if (d.node >= m_registry->size()) continue;

            std::vector<NodeDef::Box> boxes;
            boxes.reserve(d.boxes.size());
            for (const net::NodeShapeDesc::Box &b : d.boxes) {
                NodeDef::Box out;
                for (int a = 0; a < 3; ++a) {
                    out.min[a] = b.min[a];
                    out.max[a] = b.max[a];
                }
                out.part = b.part;
                boxes.push_back(std::move(out));
            }

            // A kind of turn this build has no name for is None, not a number
            // the mesher would index a table with.
            const NodeTurn turn =
                (d.turn <= (uint8_t)NodeTurn::Height) ? (NodeTurn)d.turn : NodeTurn::None;
            // The model is a position in this welcome's model table plus one,
            // the same number a body carries, and the decode already refused
            // one past the end of it.
            m_registry->set_shape((BlockId)d.node, turn, std::move(boxes), (ModelId)d.model, d.piece);
        }

        // And how each of them digs, since protocol 36, over this build's own
        // numbers for the same reason as the shapes: the bar under the
        // crosshair is timed with these, and the server breaks the block by its.
        // And what each block does to a body and to light (protocol 38): a lake
        // a mod declared is water on this client too, with or without the mod.
        for (size_t id = 0; id < m_welcome.node_bodies.size() && id < m_registry->size(); ++id) {
            const NodeBodyDesc &b = m_welcome.node_bodies[id];
            m_registry->set_body((BlockId)id, b.solid, b.opaque, b.opacity, b.liquid, b.fill,
                                 b.climbable);
            m_registry->set_conveyor((BlockId)id, b.conveyor, b.conveyor_height, b.conveyor_rise, b.rail);
            m_registry->set_light_source((BlockId)id, b.light);
        }

        for (size_t id = 0; id < m_welcome.node_digs.size() && id < m_registry->size(); ++id) {
            const NodeDigDesc &d = m_welcome.node_digs[id];
            m_registry->set_dig((BlockId)id, d.hardness, d.dig_level, d.material);
        }

        // The item table, in the same pass and by the same rule: register in
        // arrival order and the ids come out identical, because a registry
        // hands out the next free one each time. Nothing is invented for an
        // item this build has never heard of -- unlike a block, it needs no
        // placeholder: what it PLACES is a block in the table just adopted, and
        // that block already has a picture.
        m_own_items = ItemRegistry{};
        for (size_t i = 1; i < m_welcome.items.size(); ++i) {
            const ItemDesc &d = m_welcome.items[i];
            ItemDef         def;
            def.name      = d.name;
            def.owner     = "server";
            def.places    = (BlockId)d.places;
            def.max_stack = d.max_stack;
            def.dig_level = d.dig_level;
            def.dig_speed = d.dig_speed;
            def.digs      = d.digs;
            def.wear       = d.wear;
            def.armor      = d.armor;
            def.durability = d.durability;
            def.arms       = d.arms;
            m_own_items.register_item(def);
        }

        // And how a body moves here, over whatever this process's own mods
        // said: the decode refused numbers this engine cannot run, and the
        // server is the side that checks a body against them.
        *m_movement = m_welcome.movement;
        m_gen       = 0;

        m_state     = ClientState::Playing;
        m_joined_in = now - m_started;
        break;
    }

    case MsgType::Refuse:
        if (Refuse::decode(r, m_refusal)) {
            m_state = ClientState::Refused;
            m_error = std::string(refuse_reason_text(m_refusal.reason)) +
                      (m_refusal.detail.empty() ? "" : (": " + m_refusal.detail));
        } else {
            fail("the refusal does not parse");
        }
        break;

    case MsgType::Chunk: {
        if (m_state != ClientState::Playing) {
            fail("a chunk arrived before the welcome");
            break;
        }
        // Past the type byte, then the codec.
        ByteReader cr(data.data() + 1, data.size() - 1);
        auto       chunk = decode_chunk(cr, m_chunk_error);
        if (!chunk) break;
        // A chunk this world already had. On a link that loses packets a stream
        // can still be running when the player walks through, so "more chunks
        // arrived after the crossing" does not mean the world was streamed
        // again -- but the same chunk arriving twice does, and that is the
        // thing worth counting.
        if (m_world->find_chunk(chunk->pos()) != nullptr) ++m_chunks_repeated;
        install_chunk(*m_world, std::move(chunk));
        ++m_chunks;
        break;
    }

    case MsgType::Notice: {
        if (m_state != ClientState::Playing) break;

        ByteReader nr(data.data(), data.size());
        Notice     n;
        if (!Notice::decode(nr, n)) break;

        m_notice = std::move(n.text);
        break;
    }

    case MsgType::Chat: {
        if (m_state != ClientState::Playing) break;

        ByteReader cr(data.data(), data.size());
        Chat       c;
        // A backlog nobody drains -- a window onto this world, never the one
        // being played -- stops at a screenful rather than growing forever.
        if (Chat::decode(cr, c) && m_chat.size() < 100) m_chat.push_back(std::move(c.text));
        break;
    }

    case MsgType::ChunksGone: {
        if (m_state != ClientState::Playing) break;

        ByteReader gr(data.data(), data.size());
        ChunksGone gone;
        if (!ChunksGone::decode(gr, gone)) break;

        // The server has forgotten sending these (TODO.md OPEN-38), so this
        // world lets them go and is sent them again if the player comes back.
        // The neighbours are rebuilt: a face that was hidden against a chunk
        // that is no longer there is a face at the edge of the world now.
        for (const ChunkPos p : gone.chunks) {
            if (!m_world->erase_chunk(p)) continue;
            ++m_chunks_dropped;
            m_world->mark_dirty(ChunkPos{p.x - 1, p.y, p.z});
            m_world->mark_dirty(ChunkPos{p.x + 1, p.y, p.z});
            m_world->mark_dirty(ChunkPos{p.x, p.y - 1, p.z});
            m_world->mark_dirty(ChunkPos{p.x, p.y + 1, p.z});
            m_world->mark_dirty(ChunkPos{p.x, p.y, p.z - 1});
            m_world->mark_dirty(ChunkPos{p.x, p.y, p.z + 1});
        }
        break;
    }

    case MsgType::CellOpen: {
        if (m_state != ClientState::Playing) break;

        ByteReader br(data.data(), data.size());
        CellOpen   co;
        if (!CellOpen::decode(br, co)) break;

        const Cell at{co.x, co.y, co.z};
        if (co.open) m_open_cells.insert(at);
        else         m_open_cells.erase(at);
        break;
    }

    case MsgType::BlockChange: {
        if (m_state != ClientState::Playing) break;

        ByteReader  br(data.data(), data.size());
        BlockChange bc;
        if (!BlockChange::decode(br, bc)) break;

        // A refusal, which the server sends only to whoever asked. It carries
        // the block that is STILL there, so writing it is right either way --
        // it puts back a cell a client had drawn optimistically and is a no-op
        // for one that had not.
        if (!bc.why.empty()) {
            m_edit_refusal = bc.why;
            ++m_edits_refused;
        } else if (m_edit_asked && bc.x == m_edit_x && bc.y == m_edit_y && bc.z == m_edit_z) {
            // The cell this session last asked about, so this is the answer to
            // the question the refusal was about: the block went, or the dig is
            // running. Any OTHER cell is somebody else's news -- a mod's, or a
            // player's on the far side of the world -- and it says nothing
            // about whether this body was told no.
            m_edit_refusal.clear();
        }

        // Only into a chunk this world holds. A change can be on its way on the
        // block channel while the chunk it is in is let go on the chunk
        // channel, and ENet orders neither against the other: written anyway,
        // it would make a chunk of air around one block, far from anybody,
        // that nothing ever sends again.
        bc.had = bc.id;   // a cell not held changes no light here
        if (m_world->find_chunk(World::chunk_of(bc.x, bc.y, bc.z)) != nullptr) {
            bc.had = m_world->get_block(bc.x, bc.y, bc.z);
            m_world->set_block(bc.x, bc.y, bc.z, (BlockId)bc.id);
            m_world->set_facing(bc.x, bc.y, bc.z, bc.facing);
        }
        m_block_changes.push_back(std::move(bc));
        break;
    }

    case MsgType::Inventory: {
        if (m_state != ClientState::Playing) break;

        ByteReader     ir(data.data(), data.size());
        InventoryState st;
        if (!InventoryState::decode(ir, st)) break;

        m_carried       = std::move(st);
        m_has_inventory = true;
        break;
    }

    case MsgType::Container: {
        if (m_state != ClientState::Playing) break;

        ByteReader     cr(data.data(), data.size());
        ContainerState cs;
        if (!ContainerState::decode(cr, cs)) break;

        m_container = std::move(cs);
        break;
    }

    case MsgType::Craft: {
        if (m_state != ClientState::Playing) break;

        ByteReader fr(data.data(), data.size());
        CraftState cs;
        if (!CraftState::decode(fr, cs)) break;

        m_craft     = std::move(cs);
        m_has_craft = true;
        break;
    }

    case MsgType::Vitals: {
        if (m_state != ClientState::Playing) break;

        ByteReader vr(data.data(), data.size());
        Vitals     v;
        if (!Vitals::decode(vr, v)) break;

        m_vitals     = v;
        m_has_vitals = true;
        break;
    }

    case MsgType::Push: {
        if (m_state != ClientState::Playing) break;

        ByteReader pr(data.data(), data.size());
        Push       p;
        if (!Push::decode(pr, p)) break;

        // Knocks add up until the frame takes them; a launch (set) is the
        // speed itself, whatever came before it in the same frame.
        if (p.set) {
            m_push = p;
        } else {
            m_push.x += p.x;
            m_push.y += p.y;
            m_push.z += p.z;
        }
        m_has_push = true;
        break;
    }

    case MsgType::Lie: {
        if (m_state != ClientState::Playing) break;
        ByteReader rr(data.data(), data.size());
        Lie        l;
        if (!Lie::decode(rr, l)) break;
        m_lie     = l;
        m_has_lie = true;
        break;
    }

    case MsgType::Sound: {
        if (m_state != ClientState::Playing) break;
        ByteReader rr(data.data(), data.size());
        Sound      snd;
        if (!Sound::decode(rr, snd)) break;
        if (m_sounds.size() < 64) m_sounds.push_back(std::move(snd));   // a flood is dropped, not kept
        break;
    }

    case MsgType::Ride: {
        if (m_state != ClientState::Playing) break;
        ByteReader rr(data.data(), data.size());
        Ride       r;
        if (!Ride::decode(rr, r)) break;

        // Taken and left reliably; the seat moved on the entity channel, which
        // only moves a seat already taken -- one arriving after the message
        // that let go of it must not sit the body down again.
        if (channel != (uint8_t)Channel::Entity) {
            m_ride   = r;
            m_riding = r.host != 0;
        } else if (m_riding && r.host == m_ride.host) {
            m_ride = r;
        }
        break;
    }

    case MsgType::BodyMovement: {
        if (m_state != ClientState::Playing) break;

        // How THIS body moves from now on (TODO.md OPEN-47), written where the
        // welcome's numbers went and over them: the server has just started
        // checking this body against these.
        ByteReader   br(data.data(), data.size());
        BodyMovement bm;
        if (!BodyMovement::decode(br, bm)) break;

        *m_movement = bm.rules;
        m_gen       = bm.gen;
        m_dig       = bm.dig;
        ++m_body_movements;
        break;
    }

    case MsgType::Time: {
        if (m_state != ClientState::Playing) break;

        ByteReader tr(data.data(), data.size());
        Time       t;
        if (!Time::decode(tr, t)) break;

        m_time     = clock_from_wire(t.tod);
        m_has_time = true;
        m_wind     = (double)t.wind / 100.0;
        m_wind_at  = steady_seconds();
        break;
    }

    case MsgType::PanelShow: {
        if (m_state != ClientState::Playing) break;

        ByteReader pr(data.data(), data.size());
        PanelShow  ps;
        if (!PanelShow::decode(pr, ps)) break;

        m_panel = std::move(ps);
        ++m_panel_rev;
        break;
    }

    case MsgType::Correct: {
        Correct c;
        if (!Correct::decode(r, c)) break;
        if (stale_placement(c.epoch)) break;
        m_correction     = c;
        m_has_correction = true;
        m_has_teleport   = false;   // older: both come on this channel, in order
        ++m_corrections;
        break;
    }

    case MsgType::Seal: {
        Seal seal;
        if (Seal::decode(r, seal)) m_sealed = seal.mask;
        break;
    }

    case MsgType::Teleport: {
        Teleport tp;
        if (!Teleport::decode(r, tp)) break;
        if (stale_placement(tp.epoch)) break;
        m_teleport       = tp;
        m_has_teleport   = true;
        m_has_correction = false;   // older, the same way
        ++m_teleports;
        break;
    }

    case MsgType::Heartbeat: {
        Heartbeat h;
        if (!Heartbeat::decode(r, h)) break;
        // Unsequenced on the wire, so one may arrive behind a newer one. An old
        // heartbeat says nothing a newer one has not already said, and taking
        // it would move the clock backwards: dropped, not applied.
        if (m_beats > 0 && h.seq <= m_beat_seq) break;
        if (m_beats > 0 && h.seq > m_beat_seq + 1) m_beats_lost += (long)(h.seq - m_beat_seq - 1);
        if (m_beats > 0) {
            const double gap = now - m_last_beat;
            if (gap > m_beat_gap_max) m_beat_gap_max = gap;
        }
        m_beat     = h;
        m_beat_seq = h.seq;
        m_last_beat = now;
        ++m_beats;
        break;
    }

    case MsgType::MediaMissing: {
        MediaMissing gone;
        if (!MediaMissing::decode(r, gone)) break;

        // Answered, and the answer is no. The entry stays `asked` so nothing
        // asks again -- a world that does not hold a file now will not hold it
        // in four seconds either -- and stops being counted as missing, so the
        // renderer is not waiting on it.
        for (const ContentHash &h : gone.hashes) {
            for (size_t i = 0; i < m_media_offer.size(); ++i) {
                if (!(m_media_offer[i].hash == h)) continue;
                if (!m_media_state[i].have && m_media_state[i].asked) {
                    ++m_media_refused_by_server;
                    if (m_media_missing > 0) --m_media_missing;
                    if (m_cfg.media_cache) m_cfg.media_cache->release(h);
                }
                break;
            }
            if (m_media_in_flight > 0) --m_media_in_flight;
        }
        m_media_asked_at = now;
        if (m_media_in_flight == 0) request_missing_media(now);
        break;
    }

    case MsgType::MediaOffer: {
        MediaOffer offer;
        if (!MediaOffer::decode(r, offer)) break;

        m_media_offer    = std::move(offer.items);
        m_media_had       = 0;
        m_media_received  = 0;
        m_media_missing   = 0;
        m_media_bytes     = 0;
        m_media_in_flight = 0;

        // What is already on disk is already on disk, whichever world put it
        // there. This loop is the entire "two servers, one download" claim.
        ++m_media_version;
        m_media_state.assign(m_media_offer.size(), MediaWant{});
        for (size_t i = 0; i < m_media_offer.size(); ++i) {
            if (m_cfg.media_cache && m_cfg.media_cache->has(m_media_offer[i].hash)) {
                m_media_state[i].have = true;
                ++m_media_had;
            } else {
                ++m_media_missing;
            }
        }

        request_missing_media(now);
        break;
    }

    case MsgType::MediaData: {
        MediaData blob;
        // The decoder already checked that the bytes hash to the hash the
        // message carries; what it cannot check is whether this world ever
        // offered it.
        if (!MediaData::decode(r, blob)) {
            ++m_media_rejected;
            break;
        }

        size_t at = m_media_offer.size();
        for (size_t i = 0; i < m_media_offer.size(); ++i)
            if (m_media_offer[i].hash == blob.hash) {
                at = i;
                break;
            }
        if (at == m_media_offer.size()) {
            // A file nobody asked for, written into a cache shared with every
            // other world. Refused, and counted where a person can see it.
            ++m_media_rejected;
            break;
        }

        if (m_cfg.media_cache) {
            std::string err;
            const bool  stored = m_cfg.media_cache->store(blob.hash, blob.bytes, err);
            m_cfg.media_cache->release(blob.hash);   // held or not, this fetch is over
            if (!stored) {
                ++m_media_rejected;
                break;
            }
        }

        ++m_media_received;
        ++m_media_version;
        m_media_bytes += blob.bytes.size();

        // The batch is alive, and everything that follows from that.
        //
        // The backstop below asks "is anything still coming", not "has all of
        // it come": a set of files big enough, or a link slow enough, takes
        // longer than any fixed number, and giving up in the middle used to
        // mean asking for the whole batch again while the first copy was still
        // on its way -- 36 files for 31 distinct at 20% loss.
        //
        // And the claims this session holds are alive too. Their timeout is
        // there for a session that asked and died, not for one that is being
        // served slowly: letting them lapse under a working transfer let the
        // other window fetch the same files, 52 for 31 distinct at 25%.
        m_media_asked_at = now;
        if (!m_media_state[at].have) {
            m_media_state[at].have = true;
            if (m_media_missing > 0) --m_media_missing;
        }

        // The next batch, when this one is done. Nothing is in flight at that
        // point, so a world with four thousand blocks costs sixty-four round
        // trips and never sixty-four megabytes of queued messages.
        if (m_media_in_flight > 0 && --m_media_in_flight == 0) request_missing_media(now);
        break;
    }

    case MsgType::Ticket: {
        Ticket t;
        if (!Ticket::decode(r, t)) break;
        m_ticket            = t;
        m_has_ticket_answer = true;
        ++m_tickets_received;
        break;
    }

    case MsgType::Redirect: {
        Redirect rd;
        if (!Redirect::decode(r, rd)) break;
        m_redirect     = rd;
        m_has_redirect = true;
        ++m_redirects;
        break;
    }

    case MsgType::ScopeDenied: {
        ScopeDenied no;
        if (!ScopeDenied::decode(r, no)) break;
        m_denial     = no;
        m_has_denial = true;
        ++m_scope_denials;

        // The request did not happen, so neither did the scope change this
        // client wrote down when it sent it. Without this the client believes
        // it is a player on a server that still has an observer -- which is
        // exactly the state the two-second timeout used to leave behind, only
        // now there is a message that says so.
        m_cfg.scope = m_scope_before_request;
        break;
    }

    case MsgType::Portals: {
        Portals ps;
        if (!Portals::decode(r, ps)) break;

        // Older than what is already held. The message is reliable and ordered,
        // so this is not expected -- it is here because "the openings moved
        // backwards" is a bug that would show as a portal drawn in a place
        // nobody put one, and a version number costs four bytes.
        if (m_portals_version != 0 && ps.version < m_portals_version) break;

        m_portals         = ps;
        m_portals_version = ps.version;
        break;
    }

    case MsgType::Entities: {
        Entities e;
        if (!Entities::decode(r, e)) break;
        ++m_entity_messages;

        // A frame's parts (protocol 109): once every part of one came, a
        // body in none of them has gone -- hidden, killed, out of range, or
        // further than the bodies sent. A part lost, and nothing is taken
        // out on that frame: the next whole one does it.
        if (e.frame != m_ent_frame || m_ent_mask == 0) {
            m_ent_frame = e.frame;
            m_ent_mask  = 0;
            m_ent_seen.clear();
        }
        for (const EntityState &s : e.players) m_ent_seen.push_back(s.id);
        m_ent_mask |= 1u << e.part;
        if (m_ent_mask == (1u << e.parts) - 1u) {
            std::sort(m_ent_seen.begin(), m_ent_seen.end());
            m_players.erase(std::remove_if(m_players.begin(), m_players.end(),
                                           [&](const RemotePlayer &p) {
                                               return !std::binary_search(m_ent_seen.begin(), m_ent_seen.end(),
                                                                          p.id);
                                           }),
                            m_players.end());
            m_ent_mask = 0;
        }

        for (const EntityState &s : e.players) {
            auto it = std::find_if(m_players.begin(), m_players.end(),
                                   [&](const RemotePlayer &p) { return p.id == s.id; });
            if (it == m_players.end()) {
                m_players.push_back(RemotePlayer{});
                it = m_players.end() - 1;
                it->id = s.id;
            }
            it->x = s.x; it->y = s.y; it->z = s.z;
            it->yaw = s.yaw; it->pitch = s.pitch;
            it->kind = s.kind;
            it->item = s.item;
            it->item_tint = s.item_tint;
            it->livery    = s.livery;
            it->model = s.model;
            it->working   = s.working;
            it->crouching = s.crouching;
            it->clip      = s.clip;
            it->lying     = s.lying;
            it->flying    = s.flying;
            it->held      = s.held;
            for (int k = 0; k < 5; ++k) it->worn[k] = s.worn[k];
            for (int k = 0; k < 5; ++k) it->worn_tint[k] = s.worn_tint[k];
            for (int k = 0; k < 2; ++k) it->arm[k] = s.arm[k];
            for (int k = 0; k < 10; ++k) it->dress[k] = s.dress[k];
            for (int k = 0; k < 10; ++k) it->dress_tint[k] = s.dress_tint[k];
            it->burning   = s.burning;
            it->falling   = s.falling;
            it->chassis    = s.chassis;
            it->body_pitch = s.body_pitch;
            it->body_roll  = s.body_roll;
            it->boosting   = s.boosting;
            it->sliding    = s.sliding;
            it->base_dx    = s.base_dx;
            it->base_dy    = s.base_dy;
            it->base_dz    = s.base_dz;
            it->hull_w     = s.hull_w;
            it->hull_h     = s.hull_h;
            it->hull_l     = s.hull_l;
            it->hull_ride  = s.hull_ride;
            it->last_seen = now;
        }
        break;
    }

    default:
        // Anything else is either a message this client does not know or one
        // the server had no business sending. Neither is a reason to keep
        // playing with a peer that is not running the protocol.
        fail("unexpected message from the server");
        break;
    }
}

void ClientSession::record_drive(const std::vector<DriveEvent> &events)
{
    for (const DriveEvent &e : events) m_drive.push_back(DriveRecord{e, m_gen});

    // A session that claims nothing (it is not playing yet) keeps a second.
    if (m_drive.size() > 4 * kMaxDriveRecords) {
        const size_t drop = m_drive.size() - kMaxDriveRecords;
        m_drive.erase(m_drive.begin(), m_drive.begin() + (long)drop);
        m_drive_first += (uint32_t)drop;
    }
}

bool ClientSession::send_move(float x, float y, float z, float look_x, float look_y, float look_z,
                              double now, bool flying, Vec3 velocity)
{
    if (m_state != ClientState::Playing) return false;
    if (m_last_move >= 0.0 && now - m_last_move < m_cfg.move_interval) return false;

    m_last_move = now;

    Move m;
    m.seq = ++m_move_seq;
    m.x = x; m.y = y; m.z = z;
    m.look_x = look_x; m.look_y = look_y; m.look_z = look_z;

    // What this session is at the moment the claim is made, not at the moment
    // it lands. Those are different on any link with a round trip in it, and
    // the difference is what this field exists to let the server see.
    m.scope = (uint8_t)m_cfg.scope;
    m.flags = flying ? kMoveFlying : 0;
    m.epoch = m_epoch;
    m.gen   = m_gen;
    m.vx = velocity.x; m.vy = velocity.y; m.vz = velocity.z;

    // From where the claim three back began, or the newest that fit.
    const uint32_t end   = m_drive_first + (uint32_t)m_drive.size();
    uint32_t       first = std::max(m_drive_sent[0], m_drive_first);
    if (end - first > kMaxDriveRecords) first = end - (uint32_t)kMaxDriveRecords;
    m.first_drive = first;
    m.drive.assign(m_drive.begin() + (first - m_drive_first), m_drive.end());
    m_drive_sent[0] = m_drive_sent[1];
    m_drive_sent[1] = m_drive_sent[2];
    m_drive_sent[2] = end;
    if (m_drive_sent[0] > m_drive_first) {
        m_drive.erase(m_drive.begin(), m_drive.begin() + (long)(m_drive_sent[0] - m_drive_first));
        m_drive_first = m_drive_sent[0];
    }

    // Unreliable, on the entity channel. A position claim that arrives late is
    // worse than one that never arrives: by the time it lands, the next one is
    // already truer.
    m_transport.send(m_transport.server_peer(), Channel::Entity, m.encode(), false);
    m_transport.flush();
    return true;
}

bool ClientSession::send_edit(int x, int y, int z, int nx, int ny, int nz, bool breaking,
                              int slot, int turn, bool place_only)
{
    if (m_state != ClientState::Playing) return false;

    Edit e;
    e.x = x; e.y = y; e.z = z;
    e.nx = (int8_t)nx; e.ny = (int8_t)ny; e.nz = (int8_t)nz;
    e.breaking = breaking ? 1 : 0;
    e.slot     = (uint8_t)(slot < 0 ? 0 : slot);
    e.turn     = (uint8_t)(turn & 3);
    e.place_only = place_only;

    m_edit_x = x; m_edit_y = y; m_edit_z = z;
    m_edit_asked = true;

    m_transport.send(m_transport.server_peer(), Channel::Event, e.encode(), true);
    m_transport.flush();
    return true;
}

bool ClientSession::send_drop_item(int slot, int count)
{
    if (m_state != ClientState::Playing) return false;
    if (slot < 0) return false;

    DropItem m;
    m.slot  = (uint16_t)slot;
    m.count = (uint16_t)(count < 0 ? 0 : count);

    m_transport.send(m_transport.server_peer(), Channel::Event, m.encode(), true);
    m_transport.flush();
    return true;
}

bool ClientSession::send_hold(int slot, bool working, bool crouching)
{
    if (m_state != ClientState::Playing || slot < 0 || slot >= 9) return false;
    Hold m;
    m.slot    = (uint8_t)slot;
    m.working   = working;
    m.crouching = crouching;
    m_transport.send(m_transport.server_peer(), Channel::Event, m.encode(), true);
    return true;
}

bool ClientSession::send_livery(uint32_t livery)
{
    if (m_state != ClientState::Playing || livery >= kLiveryMax) return false;
    m_transport.send(m_transport.server_peer(), Channel::Event, Dress{livery}.encode(), true);
    return true;
}

bool ClientSession::send_chat(const std::string &text)
{
    if (m_state != ClientState::Playing || text.empty() || text.size() > kMaxChatLine) return false;
    m_transport.send(m_transport.server_peer(), Channel::Event, Chat{text}.encode(), true);
    return true;
}

bool ClientSession::send_hit(uint64_t target, int slot, bool use)
{
    if (m_state != ClientState::Playing) return false;

    // Zero is not an entity, and the decoder on the other end refuses it. Sent
    // anyway it would cost a round trip to be told what this side already knew.
    if (target == 0) return false;

    Hit m;
    m.target = target;
    m.slot   = (uint8_t)std::clamp(slot, 0, 255);
    m.use    = use;

    m_transport.send(m_transport.server_peer(), Channel::Event, m.encode(), true);
    m_transport.flush();
    return true;
}

bool ClientSession::send_ui_click(const std::string &panel, const std::string &id,
                                  const std::string &text)
{
    if (m_state != ClientState::Playing) return false;

    // Refused here rather than sent and refused there. A click with no panel or
    // no button is not a click that failed, it is a call this client had no
    // business making, and the server's copy of that check exists for peers
    // that are not this code.
    if (panel.empty() || id.empty()) return false;

    UiClick m;
    m.panel = panel;
    m.id    = id;
    m.text  = text.substr(0, kMaxWirePanelText);

    m_transport.send(m_transport.server_peer(), Channel::Event, m.encode(), true);
    m_transport.flush();
    return true;
}

bool ClientSession::send_close_container()
{
    if (m_state != ClientState::Playing) return false;

    CloseContainer m;
    m_transport.send(m_transport.server_peer(), Channel::Event, m.encode(), true);
    m_transport.flush();
    return true;
}

bool ClientSession::send_move_item(int from, int to, int count, Side from_side, Side to_side)
{
    if (m_state != ClientState::Playing) return false;
    if (from < 0 || to < 0) return false;

    MoveItem m;
    m.from  = (uint16_t)from;
    m.to    = (uint16_t)to;
    m.count = (uint16_t)(count < 0 ? 0 : count);

    // A side and not a place. "The container I have open" and "the grid I am
    // crafting in" are both things the server already knows, so there is no
    // cell and no size in this message to lie about.
    m.from_side = (uint8_t)from_side;
    m.to_side   = (uint8_t)to_side;

    m_transport.send(m_transport.server_peer(), Channel::Event, m.encode(), true);
    m_transport.flush();
    return true;
}

bool ClientSession::send_take_craft()
{
    if (m_state != ClientState::Playing) return false;

    MoveItem m;
    m.from_side = (uint8_t)Side::Craft;
    m.from      = kCraftResultSlot;
    m.to_side   = (uint8_t)Side::Bag;
    m.to        = 0;   // ignored: the server puts it wherever it fits

    m_transport.send(m_transport.server_peer(), Channel::Event, m.encode(), true);
    m_transport.flush();
    return true;
}

bool ClientSession::send_quick_move(int slot, Side side)
{
    if (m_state != ClientState::Playing) return false;
    if (slot < 0) return false;

    QuickMove m;
    m.side = (uint8_t)side;
    m.slot = (uint16_t)slot;

    m_transport.send(m_transport.server_peer(), Channel::Event, m.encode(), true);
    m_transport.flush();
    return true;
}

bool ClientSession::send_quick_craft()
{
    if (m_state != ClientState::Playing) return false;

    QuickMove m;
    m.side = (uint8_t)Side::Craft;
    m.slot = kCraftResultSlot;

    m_transport.send(m_transport.server_peer(), Channel::Event, m.encode(), true);
    m_transport.flush();
    return true;
}

bool ClientSession::next_block_change(BlockChange &out)
{
    if (m_block_changes.empty()) return false;

    out = std::move(m_block_changes.front());
    m_block_changes.erase(m_block_changes.begin());
    return true;
}

bool ClientSession::send_place_portal(int which, int x, int y, int z, int nx, int ny, int nz)
{
    if (m_state != ClientState::Playing) return false;

    PlacePortal p;
    p.which = (uint8_t)(which & 1);
    p.x = x; p.y = y; p.z = z;
    p.nx = (int8_t)nx; p.ny = (int8_t)ny; p.nz = (int8_t)nz;

    // Reliable, on the event channel. A placement that is dropped is a portal
    // the player asked for and did not get, with nothing on screen to say so.
    m_transport.send(m_transport.server_peer(), Channel::Event, p.encode(), true);
    m_transport.flush();
    return true;
}

bool ClientSession::send_enter(uint32_t portal_id, const PortalDesc &from, float x, float y,
                               float z, float look_x, float look_y, float look_z, float vx,
                               float vy, float vz, const PortalTicket *ticket)
{
    if (m_state != ClientState::Playing) return false;

    ChangeScope msg;
    msg.scope     = (uint8_t)Scope::Enter;
    msg.portal_id = portal_id;
    if (ticket) {
        msg.has_ticket = true;
        msg.ticket     = *ticket;
    }
    msg.from      = from;
    msg.x = x; msg.y = y; msg.z = z;
    msg.look_x = look_x; msg.look_y = look_y; msg.look_z = look_z;
    msg.vx = vx; msg.vy = vy; msg.vz = vz;

    m_transport.send(m_transport.server_peer(), Channel::Event, msg.encode(), true);
    m_transport.flush();
    m_scope_before_request = m_cfg.scope;
    m_cfg.scope            = Scope::Enter;
    return true;
}

bool ClientSession::send_observe()
{
    if (m_state != ClientState::Playing) return false;

    ChangeScope msg;
    msg.scope = (uint8_t)Scope::Observe;

    m_transport.send(m_transport.server_peer(), Channel::Event, msg.encode(), true);
    m_transport.flush();
    m_scope_before_request = m_cfg.scope;
    m_cfg.scope            = Scope::Observe;
    return true;
}

bool ClientSession::take_correction(Correct &out)
{
    if (!m_has_correction) return false;
    out              = m_correction;
    m_has_correction = false;
    placed(out.epoch);
    return true;
}

// A placement older than one already heard: the server made them in that
// order and sent them in another (a mod that lets a body off a seat and kills
// it, or puts it down, in one tick). Taking it would put the body back where
// the server no longer has it and claim from an epoch it has left behind:
// every claim after refused. Dropped, as an old heartbeat is.
bool ClientSession::stale_placement(uint32_t epoch)
{
    if (m_epoch_heard != 0 && (int32_t)(epoch - m_epoch_heard) < 0) return true;
    m_epoch_heard = epoch;
    return false;
}

void ClientSession::placed(uint32_t epoch)
{
    // What the chassis did before this is about where the body no longer is.
    m_epoch       = epoch;
    m_drive_first += (uint32_t)m_drive.size();
    m_drive.clear();
    for (uint32_t &at : m_drive_sent) at = m_drive_first;
}

bool ClientSession::take_teleport(Teleport &out)
{
    if (!m_has_teleport) return false;
    out            = m_teleport;
    m_has_teleport = false;
    placed(out.epoch);
    return true;
}

void ClientSession::request_missing_media(double now)
{
    if (!m_cfg.media_cache || m_media_offer.empty()) return;

    MediaNeed req;
    for (size_t i = 0; i < m_media_offer.size(); ++i) {
        if (m_media_state[i].have || m_media_state[i].asked) continue;

        // It may have arrived since the offer landed -- another session on this
        // client fetching the same texture is the whole point of a shared
        // cache, and it is why this re-checks rather than trusting the answer
        // it got when the offer arrived.
        if (m_cfg.media_cache->has(m_media_offer[i].hash)) {
            m_media_state[i].have = true;
            ++m_media_had;
            if (m_media_missing > 0) --m_media_missing;
            continue;
        }

        // Somebody else is already fetching it. Left unasked, so the retry
        // below picks it up out of the cache when they are done.
        if (!m_cfg.media_cache->claim(m_media_offer[i].hash, now)) continue;

        m_media_state[i].asked = true;
        req.hashes.push_back(m_media_offer[i].hash);
        if (req.hashes.size() >= kMediaPerRequest) break;
    }
    if (req.hashes.empty()) return;

    m_media_in_flight = (int)req.hashes.size();
    m_media_asked_at  = now;
    m_transport.send(m_transport.server_peer(), Channel::Media, req.encode(), true);
    m_transport.flush();
}

bool ClientSession::media_file(const std::string &name, std::vector<uint8_t> &out)
{
    if (!m_cfg.media_cache) return false;

    for (const MediaEntry &e : m_media_offer)
        if (e.name == name) return m_cfg.media_cache->load(e.hash, out);

    return false;
}

bool ClientSession::send_ticket_request(uint8_t scopes, uint8_t opening)
{
    if (m_state != ClientState::Playing) return false;

    TicketRequest req;
    req.scopes  = scopes;
    req.opening = opening;
    m_transport.send(m_transport.server_peer(), Channel::Event, req.encode(), true);
    m_transport.flush();
    return true;
}

bool ClientSession::take_ticket(Ticket &out)
{
    if (!m_has_ticket_answer) return false;
    out                 = m_ticket;
    m_has_ticket_answer = false;
    return true;
}

bool ClientSession::take_redirect(Redirect &out)
{
    if (!m_has_redirect) return false;
    out            = m_redirect;
    m_has_redirect = false;
    return true;
}

bool ClientSession::take_scope_denial(ScopeDenied &out)
{
    if (!m_has_denial) return false;
    out          = m_denial;
    m_has_denial = false;
    return true;
}

// --------------------------------------------------------- many of them
//
// See the note above ClientSessionSet: the ids have to outlive the sessions
// beside them, and the pump must not spend the timeout once per socket.

int ClientSessionSet::open(const ClientConfig &cfg, const Hello &hello, double now,
                           std::string &err)
{
    auto session = std::make_unique<ClientSession>();
    if (!session->connect(cfg, hello, now, err)) return kNoSession;

    m_sessions.push_back(std::move(session));
    return (int)m_sessions.size() - 1;
}

ClientSession *ClientSessionSet::get(int id)
{
    if (id < 0 || id >= (int)m_sessions.size()) return nullptr;
    return m_sessions[(size_t)id].get();
}

const ClientSession *ClientSessionSet::get(int id) const
{
    if (id < 0 || id >= (int)m_sessions.size()) return nullptr;
    return m_sessions[(size_t)id].get();
}

void ClientSessionSet::pump(double now, int timeout_ms)
{
    // The first one may wait; the rest take what has already arrived. Anything
    // else multiplies the wait by the number of servers, and a frame that
    // budgeted 5 ms for the network would spend 25 with five of them.
    for (auto &s : m_sessions) {
        s->pump(now, timeout_ms);
        timeout_ms = 0;
    }
}

void ClientSessionSet::close(int id)
{
    if (ClientSession *s = get(id)) s->close();
}

void ClientSessionSet::close_all()
{
    for (auto &s : m_sessions) s->close();
}

size_t ClientSessionSet::playing() const
{
    size_t n = 0;
    for (const auto &s : m_sessions)
        if (s->playing()) ++n;
    return n;
}

uint64_t ClientSessionSet::bytes_received() const
{
    uint64_t n = 0;
    for (const auto &s : m_sessions) n += s->bytes_received();
    return n;
}

uint64_t ClientSessionSet::bytes_sent() const
{
    uint64_t n = 0;
    for (const auto &s : m_sessions) n += s->bytes_sent();
    return n;
}

} // namespace net
} // namespace cnp
