#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

// Session encryption, because ENet does not encrypt and DESIGN.md 3.2 says that
// is not optional once portals lead to somebody else's server.
//
// This sits UNDER the protocol rather than inside it. Nothing in
// core/net/protocol.h or core/net/session.h knows this file exists: the
// transport finishes a key exchange before it reports a peer as connected, and
// from that moment every byte in either direction is sealed. The whole
// handshake, the block table, the chunks, the position claims -- all of it
// travels inside AEAD frames, and the protocol tests keep running on plaintext
// buffers with no socket, which is the point of that split.
//
// What it does and does not promise:
//
//   - **Against somebody listening**: an eavesdropper learns packet sizes and
//     timing and nothing else. That holds with or without a pinned key.
//   - **Against somebody in the middle**: only if the client pinned the
//     server's long-term key (`--server-key`). Without a pin, an attacker who
//     can intercept can offer their own static key and the client has no way to
//     tell -- exactly like SSH on a host you have never visited. The client is
//     told which of the two situations it is in, in words, rather than being
//     left to assume the better one.
//   - **Forward secrecy**: yes. The ephemeral pair is thrown away with the
//     session, so recording traffic today and stealing the server's key
//     tomorrow does not decrypt it.
//
// The other direction -- a *player* proving who they are to a server -- is the
// signature half below. DESIGN.md 4.4: the identity is a public key and the
// name is only a label. It had to come second, because what a player signs is
// the session this file established.

namespace cnp {
namespace net {

inline constexpr size_t kKeyBytes   = 32;   // X25519 public and secret keys
inline constexpr size_t kMacBytes   = 16;   // Poly1305 tag
inline constexpr size_t kNonceBytes = 24;   // XChaCha20

// What a sealed frame costs over the plaintext: the tag byte, the counter, and
// the MAC. Measured rather than guessed at anywhere that reports bandwidth.
inline constexpr size_t kSealOverhead = 1 + 8 + kMacBytes;

// The most that can go inside one sealed frame. The largest message this
// protocol produces is an incompressible chunk at about 91 KB; this is far
// above that and far below where adding kSealOverhead to it could wrap.
inline constexpr size_t kMaxSealedPayload = 4u * 1024u * 1024u;

// Fills `out` with bytes from the operating system's generator. Returns false
// if it could not -- which is not a case to paper over with a fallback, because
// a fallback that quietly produces predictable keys is worse than a refusal.
bool random_bytes(void *out, size_t size);

// A long-term X25519 pair. The server keeps one on disk; its public half is the
// server's identity, the thing a client pins.
struct KeyPair {
    uint8_t public_key[kKeyBytes]{};
    uint8_t secret_key[kKeyBytes]{};

    static bool generate(KeyPair &out);

    // From a secret key alone: the public half is derived, never stored twice.
    static void from_secret(const uint8_t secret[kKeyBytes], KeyPair &out);
};

// --------------------------------------------------------------- signatures
//
// A player's identity, from DESIGN.md 4.4. Separate from KeyPair above and not
// a reuse of it: X25519 does key exchange and EdDSA does signatures, and using
// one key for both is the classic way to turn two sound primitives into one
// unsound one.
//
// EdDSA over Curve25519 with BLAKE2b, which is what monocypher's core offers.
// Not RFC 8032 ed25519 (that is the SHA-512 variant in monocypher's optional
// half): nothing here interoperates with another implementation, so the choice
// is between two internally consistent options and this one is already linked.
// Stated because "ed25519" in a comment would be a claim about interop that is
// not true.
inline constexpr size_t kSignPublicBytes = 32;
inline constexpr size_t kSignSecretBytes = 64;   // monocypher keeps the public half in it
inline constexpr size_t kSignatureBytes  = 64;
inline constexpr size_t kSignSeedBytes   = 32;   // what goes on disk

struct SignKeyPair {
    uint8_t public_key[kSignPublicBytes]{};
    uint8_t secret_key[kSignSecretBytes]{};

    static bool generate(SignKeyPair &out);
    static void from_seed(const uint8_t seed[kSignSeedBytes], SignKeyPair &out,
                          uint8_t *seed_out = nullptr);
};

void sign_message(const SignKeyPair &key, const void *message, size_t size,
                  uint8_t signature[kSignatureBytes]);

// Returns false for a bad signature, a wrong key, or a message that changed.
// Never throws, never logs: a caller decides what a failed proof means.
bool verify_message(const uint8_t public_key[kSignPublicBytes], const void *message, size_t size,
                    const uint8_t signature[kSignatureBytes]);

// The identity a name and a password make, and nothing else: Argon2id over the
// password, salted with the name, into the seed of a signing pair. The same two
// words give the same identity on any machine, so a player carries no file
// around; and there is no file to steal either, which is the other half of it.
//
// Slow on purpose -- 64 MiB and three passes, a good fraction of a second --
// because how long guessing takes is the whole of what a password is worth.
// False only when the memory for it could not be had.
bool sign_key_from_password(const std::string &name, const std::string &password,
                            SignKeyPair &out);

// Reads a player's signing key from `path`, creating it if it is not there.
// Same file discipline as the server's: the SEED as hex, 0600, and a warning
// rather than a fix when the permissions are wider.
bool load_or_create_sign_key(const std::string &path, SignKeyPair &out, bool &created,
                             std::string &err);

// Hex, for printing a fingerprint and for reading one off a command line.
std::string to_hex(const uint8_t *data, size_t size);
bool        from_hex(const std::string &hex, uint8_t *out, size_t size);

// A shorter form for a human to compare by eye. The full key is what gets
// checked; this is what gets read aloud.
std::string key_fingerprint(const uint8_t key[kKeyBytes]);

// Reads the server's key from `path`, creating it if it is not there. The file
// holds the secret key as hex and is created readable only by its owner; if it
// exists with wider permissions that is reported through `err` rather than
// silently accepted.
bool load_or_create_key(const std::string &path, KeyPair &out, bool &created, std::string &err);

// One end of one encrypted session.
//
// Frames on the wire, first byte first:
//
//   1  offer   : [1][magic u32][client ephemeral public 32]
//   2  accept  : [2][magic u32][server static public 32][server ephemeral 32]
//   3  sealed  : [3][counter u64 LE][mac 16][ciphertext]
//
// The counter is per channel and per direction, and it goes into the nonce
// along with the channel number, so a frame cannot be replayed on a different
// channel than the one it was sealed for. The two directions use different
// keys, so a frame cannot be reflected back at its sender either.
class SecureChannel {
public:
    enum class State : uint8_t { Idle = 0, Offered, Ready, Failed };

    // --- client side -------------------------------------------------------
    // Starts a session. `pinned` is the server's long-term public key if the
    // caller has one, or null to accept whatever key the server presents.
    bool begin_client(const uint8_t *pinned, std::vector<uint8_t> &offer_out, std::string &err);
    bool accept_accept(const uint8_t *data, size_t size, std::string &err);

    // --- server side -------------------------------------------------------
    void begin_server(const KeyPair &identity);
    bool accept_offer(const uint8_t *data, size_t size, std::vector<uint8_t> &accept_out,
                      std::string &err);

    // --- both --------------------------------------------------------------
    State state() const { return m_state; }
    bool  ready() const { return m_state == State::Ready; }

    // True when the peer's long-term key was checked against one the caller
    // supplied. False means encrypted but unauthenticated, and callers are
    // expected to say so out loud rather than keep it to themselves.
    bool authenticated() const { return m_authenticated; }

    // The key the server presented, valid on the client once ready. What a
    // person pins after looking at it.
    const uint8_t *peer_identity() const { return m_peer_identity; }

    // A value both ends of THIS session know and nobody else can predict,
    // derived from the same master secret as the traffic keys.
    //
    // It is what a player signs to prove they are here now. Signing their own
    // name instead would produce a proof that works forever and on every
    // server: anybody who ever saw it could replay it. Signing this binds the
    // proof to one session with one server, so a recorded proof is worth
    // nothing anywhere else -- including to the server that collected it.
    //
    // All zeroes before the exchange finishes, which is a state a caller must
    // not sign over; ready() says when it is real.
    const uint8_t *binding() const { return m_binding; }

    bool seal(uint8_t channel, const void *plain, size_t size, std::vector<uint8_t> &out);
    bool open(uint8_t channel, const uint8_t *data, size_t size, std::vector<uint8_t> &out,
              std::string &err);

    ~SecureChannel();

private:
    static constexpr size_t kChannels = 32;   // kMaxChannels in protocol.h

    void derive(const uint8_t *ephemeral_secret, const uint8_t *client_eph_pub,
                const uint8_t *server_eph_pub, const uint8_t *server_static_pub);

    State m_state = State::Idle;
    bool  m_is_server     = false;
    bool  m_authenticated = false;

    uint8_t m_identity_secret[kKeyBytes]{};   // server only
    uint8_t m_identity_public[kKeyBytes]{};
    uint8_t m_peer_identity[kKeyBytes]{};
    uint8_t m_pinned[kKeyBytes]{};
    bool    m_has_pin = false;

    uint8_t m_eph_secret[kKeyBytes]{};
    uint8_t m_eph_public[kKeyBytes]{};

    uint8_t m_send_key[kKeyBytes]{};
    uint8_t m_recv_key[kKeyBytes]{};
    uint8_t m_binding[kKeyBytes]{};

    uint64_t m_send_counter[kChannels]{};

    // Replay defence, one window per channel. The newest counter seen, and a
    // bitmap of the sixty-four below it. Per channel rather than one shared
    // window because channel 2 is unreliable and reorders freely: a single
    // window would throw away entity updates every time a burst of chunks went
    // out on channel 1.
    uint64_t m_recv_newest[kChannels]{};
    uint64_t m_recv_seen[kChannels]{};
    bool     m_recv_any[kChannels]{};
};

} // namespace net
} // namespace cnp
