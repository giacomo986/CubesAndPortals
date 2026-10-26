#pragma once

#include "core/net/crypto.h"
#include "core/net/protocol.h"
#include "core/net/session.h"

#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

// The transport: ENet, wrapped thinly enough that the wrapper has no opinions.
//
// It lives in its own library (cnp_net) rather than in cnp_core, and that split
// is the point rather than housekeeping: the codec, the version rules and the
// session state machine are all in cnp_core and are tested by a binary that
// links no socket library at all. If any of that logic drifts down here it
// stops being testable that way, and the link error is what says so.
//
// What this file owns: sockets, ENet's event loop, and the mapping from an
// ENetPeer to an id of ours. What it does not own: any decision. It hands bytes
// to ServerSessions and does what comes back.

struct _ENetHost;
struct _ENetPeer;

namespace cnp {
namespace net {

struct NetEvent {
    enum class Type : uint8_t { None = 0, Connected, Disconnected, Received };

    Type                 type    = Type::None;
    PeerId               peer    = kNoPeer;
    uint8_t              channel = 0;
    std::vector<uint8_t> data;

    // Why, when a Disconnected event has a reason worth telling the player --
    // today that means the key exchange refused. Empty for an ordinary
    // goodbye, and "the connection closed" is the right thing to say then.
    std::string message;
};

// One class for both ends, because ENet has one: a client is a host with no
// bound port and one peer.
class Transport {
public:
    Transport() = default;
    ~Transport();

    Transport(const Transport &)            = delete;
    Transport &operator=(const Transport &) = delete;

    // Encryption, set before listen() or connect() and not changeable after.
    //
    // The transport owns this rather than the session layer, and that is the
    // decision: a Connected event is not reported to the caller until the key
    // exchange for that peer has finished, so nothing above here can send a
    // byte in the clear by forgetting to. ServerSessions and ClientSession
    // contain no crypto at all and did not change when this arrived.
    //
    // Server: its long-term identity, whose public half clients pin.
    void set_identity(const KeyPair &identity);

    // Client: the server key it insists on, or nothing to take what it is
    // given. Without a pin the session is still encrypted -- an eavesdropper
    // learns nothing -- but an attacker who can intercept can substitute their
    // own key, and authenticated() is how a caller finds out which it got.
    void pin_server_key(const uint8_t key[kKeyBytes]);

    // Off entirely. Exists for the in-process loopback test, which needs to
    // exercise the protocol without two key exchanges per connection, and for
    // measuring what the encryption costs by running with and without it.
    // Never a default: a caller that says nothing gets encryption.
    void set_encryption(bool on) { m_encrypt = on; }
    bool encrypted() const { return m_encrypt; }

    // Whether the peer's long-term key was checked against a pin. Only ever
    // true on a client that supplied one.
    bool authenticated(PeerId peer) const;

    // The long-term key the peer presented, empty if there is none. What a
    // person reads before deciding to pin it.
    std::string peer_key_hex(PeerId peer) const;

    // The value both ends of this session share and nobody else can predict.
    // See SecureChannel::binding: it is what a player signs to prove they are
    // here now rather than replaying a proof from somewhere else.
    //
    // Returns false, leaving `out` untouched, when there is no encrypted
    // channel for that peer -- which includes every session run with
    // encryption off. A caller that needs a proof to mean something has to
    // treat that as "no proof is possible", not as "the proof is zero".
    bool channel_binding(PeerId peer, uint8_t out[kKeyBytes]) const;

    // Server. `max_peers` is players, not sockets -- see the note about pending
    // peers in session.cpp. `bind` is the address to listen on, every one
    // this machine has when empty.
    bool listen(uint16_t port, int max_peers, std::string &err, const std::string &bind = {});

    // Client. Returns once the connection attempt is under way; the Connected
    // event arrives from poll(), or does not, and `connect_timeout` decides.
    bool connect(const std::string &host, uint16_t port, std::string &err);

    void close();
    bool open() const { return m_host != nullptr; }

    // One event per call, `timeout_ms` spent waiting for it. Returns false when
    // nothing happened, which is the normal case on a quiet tick.
    bool poll(NetEvent &out, int timeout_ms);

    // How long ENet may leave a reliable command outstanding on this peer
    // before deciding the peer is gone.
    //
    // Set, rather than left at ENet's defaults, because those defaults were
    // deciding something this engine has its own answer for. ENet drops a peer
    // once a command's retransmission backoff has run past its limit and the
    // command is five seconds old, or thirty seconds regardless -- and on a
    // link at 400 ms and 25% loss that happened 1.2 seconds after the last
    // message arrived, on a session everything else said was healthy.
    //
    // The rule: ENet is the OUTER bound. It must not decide before the code
    // that can say why has decided, so the caller passes its own liveness
    // window and this holds ENet open past it.
    void hold_peer(PeerId peer, double seconds);

    bool send(PeerId peer, Channel channel, const void *data, size_t size, bool reliable = true);
    bool send(PeerId peer, Channel channel, const std::vector<uint8_t> &data,
              bool reliable = true)
    {
        return send(peer, channel, data.data(), data.size(), reliable);
    }

    // Sends anything queued now instead of on the next poll. Used before a
    // deliberate disconnect, so a refusal actually reaches the peer that is
    // about to be cut off.
    void flush();

    // Graceful: the peer is told, the queue drains, and Disconnected arrives
    // later. `now` disconnects immediately and is for peers that have proved
    // they are not worth the round trip.
    void disconnect(PeerId peer, bool now = false);

    size_t peer_count() const;

    // The client's single peer, or kNoPeer before the connection is up.
    PeerId server_peer() const { return m_server_peer; }

    // Bytes and packets, both directions, since ENet's connection start. For
    // the chunk-streaming work that comes next: a compression ratio has to be
    // measured against something.
    uint64_t bytes_sent() const;
    uint64_t bytes_received() const;

    // What ENet thinks the round trip to this peer is, in seconds. Zero for a
    // peer that is not connected, and roughly zero on loopback.
    //
    // Exposed because a constant expressed in wall-clock seconds is a constant
    // that was chosen on somebody's link. The heartbeat silence window is the
    // first: five seconds is generous at 0.3 ms and not generous at 400 ms with
    // packets going missing, where a reliable message is retransmitted with a
    // doubling timeout and one heartbeat in forty takes several seconds.
    double round_trip(PeerId peer) const;

    // Bytes of reliable data this end still owes that peer: sent and not yet
    // acknowledged, plus whatever is queued behind a window that is full.
    //
    // Exposed because a silence the server itself caused is not evidence that
    // the client is gone. The liveness rule above this reads it once a tick and
    // stands aside while it is above zero -- see net::quiet_for.
    //
    // Data, deliberately, and not enet_peer_has_outgoing_commands: that counts
    // a ping awaiting its acknowledgement, and ENet pings twice a second, so a
    // rule built on it would never let go of anything. Bytes owed go to zero on
    // a link with nothing to say.
    size_t owed_bytes(PeerId peer) const;

    // Frames that arrived sealed and were thrown away: a bad tag, a replay, or
    // a counter too far behind to judge. Never a reason to drop the session on
    // its own -- UDP delivers corrupted packets and anybody can spoof one -- but
    // a number that should stay at zero on a healthy link, so it is countable
    // rather than invisible.
    uint64_t rejected_frames() const { return m_rejected; }

private:
    // A peer whose key exchange has not finished yet. Its Connected event is
    // held back until it has, so the caller never sees a peer it could talk to
    // in the clear.
    SecureChannel *channel_for(PeerId peer);
    const SecureChannel *channel_for(PeerId peer) const;

    _ENetHost *m_host        = nullptr;
    PeerId     m_next_id     = 1;
    PeerId     m_server_peer = kNoPeer;
    bool       m_is_server   = false;

    bool    m_encrypt = true;
    KeyPair m_identity;
    bool    m_has_identity = false;
    uint8_t m_pinned[kKeyBytes]{};
    bool    m_has_pin = false;

    uint64_t m_rejected = 0;

    std::unordered_map<PeerId, std::unique_ptr<SecureChannel>> m_channels;
};

} // namespace net
} // namespace cnp
