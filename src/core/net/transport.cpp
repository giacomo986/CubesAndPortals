#include "core/net/transport.h"

#include <enet/enet.h>

#include <atomic>
#include <cstring>

namespace cnp {
namespace net {
namespace {

// enet_initialize is global and refcounted by hand, because a process with a
// server and a client in it -- which is exactly what the loopback test is --
// would otherwise deinitialise the library out from under the other half.
std::atomic<int> g_enet_users{0};

bool enet_acquire(std::string &err)
{
    if (g_enet_users.fetch_add(1) == 0) {
        if (enet_initialize() != 0) {
            g_enet_users.fetch_sub(1);
            err = "enet_initialize failed";
            return false;
        }
    }
    return true;
}

void enet_release()
{
    if (g_enet_users.fetch_sub(1) == 1) enet_deinitialize();
}

PeerId peer_id_of(ENetPeer *p)
{
    // ENet gives every peer a void* of ours to carry. Our own ids rather than
    // the pointer: ENet reuses peer slots, so a pointer that meant player 3 a
    // second ago can mean player 9 now, and an id that is never reused makes
    // "that peer is gone" a fact instead of a race.
    return (PeerId)(uintptr_t)p->data;
}

} // namespace

Transport::~Transport()
{
    close();
}

void Transport::set_identity(const KeyPair &identity)
{
    m_identity     = identity;
    m_has_identity = true;
}

void Transport::pin_server_key(const uint8_t key[kKeyBytes])
{
    std::memcpy(m_pinned, key, kKeyBytes);
    m_has_pin = true;
}

SecureChannel *Transport::channel_for(PeerId peer)
{
    const auto it = m_channels.find(peer);
    return it == m_channels.end() ? nullptr : it->second.get();
}

const SecureChannel *Transport::channel_for(PeerId peer) const
{
    const auto it = m_channels.find(peer);
    return it == m_channels.end() ? nullptr : it->second.get();
}

bool Transport::authenticated(PeerId peer) const
{
    const SecureChannel *c = channel_for(peer);
    return c && c->authenticated();
}

std::string Transport::peer_key_hex(PeerId peer) const
{
    const SecureChannel *c = channel_for(peer);
    if (!c || !c->ready()) return {};
    return to_hex(c->peer_identity(), kKeyBytes);
}

bool Transport::channel_binding(PeerId peer, uint8_t out[kKeyBytes]) const
{
    const SecureChannel *c = channel_for(peer);
    if (!c || !c->ready()) return false;
    std::memcpy(out, c->binding(), kKeyBytes);
    return true;
}

bool Transport::listen(uint16_t port, int max_peers, std::string &err, const std::string &bind)
{
    if (m_host) {
        err = "already open";
        return false;
    }

    // Refused rather than papered over with a key generated on the spot. A
    // server whose identity changes every restart cannot be pinned by anybody,
    // which turns "encrypted and authenticated" into "encrypted" without ever
    // saying so.
    if (m_encrypt && !m_has_identity) {
        err = "no server identity key: call set_identity() or set_encryption(false)";
        return false;
    }

    if (!enet_acquire(err)) return false;

    ENetAddress addr;
    addr.host = ENET_HOST_ANY;
    addr.port = port;
    if (!bind.empty() && enet_address_set_host(&addr, bind.c_str()) != 0) {
        enet_release();
        err = "cannot resolve '" + bind + "'";
        return false;
    }

    m_host = (_ENetHost *)enet_host_create(&addr, (size_t)max_peers, kMaxChannels, 0, 0);
    if (!m_host) {
        enet_release();
        err = "could not bind UDP port " + std::to_string(port);
        return false;
    }

    m_is_server = true;
    return true;
}

bool Transport::connect(const std::string &host, uint16_t port, std::string &err)
{
    if (m_host) {
        err = "already open";
        return false;
    }
    if (!enet_acquire(err)) return false;

    // A client host binds no port and has room for one peer.
    m_host = (_ENetHost *)enet_host_create(nullptr, 1, kMaxChannels, 0, 0);
    if (!m_host) {
        enet_release();
        err = "could not create a client host";
        return false;
    }

    ENetAddress addr;
    if (enet_address_set_host((ENetAddress *)&addr, host.c_str()) != 0) {
        enet_host_destroy((ENetHost *)m_host);
        m_host = nullptr;
        enet_release();
        err = "cannot resolve '" + host + "'";
        return false;
    }
    addr.port = port;

    ENetPeer *peer = enet_host_connect((ENetHost *)m_host, &addr, kMaxChannels, 0);
    if (!peer) {
        enet_host_destroy((ENetHost *)m_host);
        m_host = nullptr;
        enet_release();
        err = "no peer slot available";
        return false;
    }

    m_server_peer = m_next_id++;
    peer->data    = (void *)(uintptr_t)m_server_peer;
    m_is_server   = false;
    return true;
}

void Transport::close()
{
    if (!m_host) return;

    // Say goodbye before vanishing. Without this the other side keeps the
    // session -- the player, the slot, the interest set -- until ENet's peer
    // timeout expires, which is tens of seconds. The loopback test caught it
    // as "the server still counts a player that closed its client".
    //
    // Best effort, and it has to be: the disconnect is one packet and a packet
    // can be lost, and a client whose power goes out sends nothing at all. So
    // the other side must survive a peer that just stops either way, and ENet's
    // timeout is what covers that case. This only makes the ordinary case
    // immediate instead of slow.
    ENetHost *h = (ENetHost *)m_host;
    for (size_t i = 0; i < h->peerCount; ++i) {
        ENetPeer *p = &h->peers[i];
        if (p->state == ENET_PEER_STATE_CONNECTED) enet_peer_disconnect(p, 0);
    }
    enet_host_flush(h);

    enet_host_destroy((ENetHost *)m_host);
    m_host        = nullptr;
    m_server_peer = kNoPeer;
    m_channels.clear();   // every session key wiped with its channel
    enet_release();
}

namespace {

// A packet that goes out as it is: the two key-exchange frames, and nothing
// else. Everything after them is sealed, and this function is deliberately not
// reachable from outside this file.
bool raw_send(ENetPeer *peer, uint8_t channel, const std::vector<uint8_t> &data)
{
    ENetPacket *packet = enet_packet_create(data.data(), data.size(), ENET_PACKET_FLAG_RELIABLE);
    if (!packet) return false;
    if (enet_peer_send(peer, channel, packet) != 0) {
        enet_packet_destroy(packet);
        return false;
    }
    return true;
}

} // namespace

bool Transport::poll(NetEvent &out, int timeout_ms)
{
    out = NetEvent{};
    if (!m_host) return false;

    // A loop rather than one call, because a peer whose key exchange is still
    // running produces ENet events that the caller must not see. Those get
    // swallowed here; only the first service call is allowed to wait.
    for (;;) {
        ENetEvent ev;
        const int n = enet_host_service((ENetHost *)m_host, &ev, (enet_uint32)timeout_ms);
        timeout_ms  = 0;
        if (n <= 0) return false;   // 0 is a quiet tick, negative is an error ENet has logged

        switch (ev.type) {
        case ENET_EVENT_TYPE_CONNECT: {
            // A server assigns the id here; a client already assigned one in
            // connect(), and reassigning would give the same connection two
            // names.
            PeerId id;
            if (m_is_server) {
                id            = m_next_id++;
                ev.peer->data = (void *)(uintptr_t)id;
            } else {
                id = peer_id_of(ev.peer);
            }

            // ENet's packet throttle, off: it never slows down. It throws
            // unreliable packets away once the round trip grows past its own
            // jitter, and on a link of a few milliseconds a client going from
            // 60 frames a second to 40 is growth enough. Measured: getting into
            // a kart took the throttle to 0 -- every heartbeat, entity update
            // and position claim dropped for four seconds and more -- and the
            // client gave the server up as silent, seven runs in 21. What
            // goes unreliable here is small and replaced by the next one, and
            // losing it is the network's to do, not a guess at congestion.
            enet_peer_throttle_configure(ev.peer, ENET_PEER_PACKET_THROTTLE_INTERVAL,
                                         ENET_PEER_PACKET_THROTTLE_SCALE, 0);

            if (!m_encrypt) {
                out.type = NetEvent::Type::Connected;
                out.peer = id;
                return true;
            }

            // Connected on the wire is not connected as far as the caller is
            // concerned. The event is held back until the key exchange has
            // finished, which is what makes it impossible for the layers above
            // to send anything before there is a key to send it under.
            auto ch = std::make_unique<SecureChannel>();
            if (m_is_server) {
                ch->begin_server(m_identity);
                m_channels[id] = std::move(ch);
            } else {
                std::vector<uint8_t> offer;
                std::string          err;
                if (!ch->begin_client(m_has_pin ? m_pinned : nullptr, offer, err)) {
                    enet_peer_disconnect_now(ev.peer, 0);
                    ev.peer->data = nullptr;
                    out.type      = NetEvent::Type::Disconnected;
                    out.peer      = id;
                    out.message   = err;
                    return true;
                }
                m_channels[id] = std::move(ch);
                raw_send(ev.peer, 0, offer);
            }
            break;   // swallowed; keep polling
        }

        case ENET_EVENT_TYPE_DISCONNECT: {
            const PeerId id = peer_id_of(ev.peer);
            ev.peer->data   = nullptr;

            // A peer that never got through the key exchange was never reported
            // as connected, so reporting it as disconnected would be an event
            // for a peer the caller has never heard of.
            const bool was_visible = !m_encrypt || (channel_for(id) && channel_for(id)->ready());
            m_channels.erase(id);
            if (!was_visible) break;

            out.type = NetEvent::Type::Disconnected;
            out.peer = id;
            return true;
        }

        case ENET_EVENT_TYPE_RECEIVE: {
            const PeerId id = peer_id_of(ev.peer);

            if (!m_encrypt) {
                out.type    = NetEvent::Type::Received;
                out.peer    = id;
                out.channel = ev.channelID;
                // Copied out before the packet is destroyed. It is a copy per
                // message, which is the wrong shape for chunk streaming and the
                // right shape for everything that exists today.
                out.data.assign(ev.packet->data, ev.packet->data + ev.packet->dataLength);
                enet_packet_destroy(ev.packet);
                return true;
            }

            SecureChannel *ch = channel_for(id);
            if (!ch) {
                enet_packet_destroy(ev.packet);
                break;   // a peer we have already forgotten
            }

            if (!ch->ready()) {
                std::string err;
                bool        ok = false;

                if (m_is_server) {
                    std::vector<uint8_t> accept;
                    ok = ch->accept_offer(ev.packet->data, ev.packet->dataLength, accept, err);
                    if (ok) raw_send(ev.peer, 0, accept);
                } else {
                    ok = ch->accept_accept(ev.packet->data, ev.packet->dataLength, err);
                }
                enet_packet_destroy(ev.packet);

                if (!ok) {
                    // Flushed first, or the peer learns nothing about why it was
                    // cut off -- the same reason the refusal path flushes.
                    enet_host_flush((ENetHost *)m_host);
                    enet_peer_disconnect_now(ev.peer, 0);
                    ev.peer->data = nullptr;
                    m_channels.erase(id);

                    out.type    = NetEvent::Type::Disconnected;
                    out.peer    = id;
                    out.message = err;
                    return true;
                }

                // Now the caller may know about it.
                out.type = NetEvent::Type::Connected;
                out.peer = id;
                return true;
            }

            std::vector<uint8_t> plain;
            std::string          err;
            const bool opened =
                ch->open(ev.channelID, ev.packet->data, ev.packet->dataLength, plain, err);
            enet_packet_destroy(ev.packet);

            if (!opened) {
                // Dropped and counted, not fatal. UDP delivers corrupted
                // packets, and anybody who can guess an address can send one:
                // killing the session on the first bad frame would hand a denial
                // of service to whoever sends it.
                ++m_rejected;
                break;
            }

            out.type    = NetEvent::Type::Received;
            out.peer    = id;
            out.channel = ev.channelID;
            out.data    = std::move(plain);
            return true;
        }

        default:
            break;
        }
    }
}

namespace {

ENetPeer *find_peer(_ENetHost *host, PeerId id)
{
    ENetHost *h = (ENetHost *)host;
    for (size_t i = 0; i < h->peerCount; ++i) {
        ENetPeer *p = &h->peers[i];
        if (p->state == ENET_PEER_STATE_DISCONNECTED) continue;
        if (peer_id_of(p) == id) return p;
    }
    return nullptr;
}

} // namespace

void Transport::hold_peer(PeerId peer, double seconds)
{
    if (!m_host) return;

    ENetPeer *p = find_peer(m_host, peer);
    if (!p) return;

    // Never below ENet's own defaults: this exists to stop it giving up early,
    // and a caller with a short window must not turn it into a tighter rule
    // than the one it replaced.
    enet_uint32 ms = (enet_uint32)(seconds * 1000.0);
    if (ms < 5000) ms = 5000;

    // Minimum and maximum together mean "outstanding this long is gone",
    // whatever the backoff has done in the meantime. The limit stays ENet's.
    enet_peer_timeout(p, 0, ms, ms * 3);
}

bool Transport::send(PeerId peer, Channel channel, const void *data, size_t size, bool reliable)
{
    if (!m_host) return false;

    ENetPeer *p = find_peer(m_host, peer);
    if (!p) return false;   // it left between the decision and the send

    // Sealed here, at the last possible moment, so there is no path from any
    // caller to enet_peer_send that skips it. The vector lives until after
    // enet_packet_create, which copies.
    std::vector<uint8_t> sealed;
    if (m_encrypt) {
        SecureChannel *ch = channel_for(peer);
        if (!ch || !ch->ready()) return false;
        if (!ch->seal((uint8_t)channel, data, size, sealed)) return false;
        data = sealed.data();
        size = sealed.size();
    }

    const enet_uint32 flags = reliable ? ENET_PACKET_FLAG_RELIABLE
                                       : ENET_PACKET_FLAG_UNSEQUENCED;
    ENetPacket *packet = enet_packet_create(data, size, flags);
    if (!packet) return false;

    if (enet_peer_send(p, (enet_uint8)channel, packet) != 0) {
        enet_packet_destroy(packet);   // enet_peer_send only takes ownership on success
        return false;
    }
    return true;
}

double Transport::round_trip(PeerId peer) const
{
    if (!m_host) return 0.0;
    ENetPeer *p = find_peer((ENetHost *)m_host, peer);
    if (!p || p->state != ENET_PEER_STATE_CONNECTED) return 0.0;
    return (double)p->roundTripTime / 1000.0;
}

size_t Transport::owed_bytes(PeerId peer) const
{
    if (!m_host) return 0;
    ENetPeer *p = find_peer((ENetHost *)m_host, peer);
    if (!p || p->state != ENET_PEER_STATE_CONNECTED) return 0;

    // Sent and unacknowledged. ENet counts this itself because it is what the
    // send window is measured against.
    size_t owed = (size_t)p->reliableDataInTransit;

    // And what has not left yet. A welcome is fragmented into far more than one
    // window's worth, so most of it sits here for most of the transfer; leaving
    // it out would report zero owed in the middle of sending a world.
    //
    // Each byte is counted once: a command moves out of this list as it is sent
    // and into reliableDataInTransit, and back here only if it is dropped and
    // resent.
    for (ENetListIterator it = enet_list_begin((ENetList *)&p->outgoingSendReliableCommands);
         it != enet_list_end((ENetList *)&p->outgoingSendReliableCommands);
         it = enet_list_next(it)) {
        const ENetOutgoingCommand *c = (const ENetOutgoingCommand *)it;
        if (c->packet != nullptr) owed += (size_t)c->fragmentLength;
    }
    return owed;
}

void Transport::flush()
{
    if (m_host) enet_host_flush((ENetHost *)m_host);
}

void Transport::disconnect(PeerId peer, bool now)
{
    if (!m_host) return;

    ENetPeer *p = find_peer(m_host, peer);
    if (!p) return;

    if (now) {
        enet_peer_disconnect_now(p, 0);
        p->data = nullptr;   // disconnect_now raises no event, so nothing else will clear it
    } else {
        enet_peer_disconnect_later(p, 0);
    }
}

size_t Transport::peer_count() const
{
    if (!m_host) return 0;

    ENetHost *h = (ENetHost *)m_host;
    size_t    n = 0;
    for (size_t i = 0; i < h->peerCount; ++i)
        if (h->peers[i].state == ENET_PEER_STATE_CONNECTED) ++n;
    return n;
}

uint64_t Transport::bytes_sent() const
{
    return m_host ? ((ENetHost *)m_host)->totalSentData : 0;
}

uint64_t Transport::bytes_received() const
{
    return m_host ? ((ENetHost *)m_host)->totalReceivedData : 0;
}

} // namespace net
} // namespace cnp
