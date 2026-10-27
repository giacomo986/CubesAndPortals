#include "core/net/crypto.h"

#include "core/net/protocol.h"

#include <monocypher.h>

#include <cerrno>
#include <cstdio>
#include <new>
#include <algorithm>
#include <cstring>
#include <fcntl.h>
#ifdef _WIN32
#include <windows.h>
#include <bcrypt.h>
#else
#include <sys/random.h>
#endif
#include <sys/stat.h>
#include <unistd.h>

namespace cnp {
namespace net {
namespace {

constexpr uint8_t kFrameOffer  = 1;
constexpr uint8_t kFrameAccept = 2;
constexpr uint8_t kFrameSealed = 3;

// Personalisation, so a key derived here can never coincide with one derived by
// some other protocol that happens to use the same primitives.
constexpr char kLabelC2S[] = "cnp1 client-to-server";
constexpr char kLabelS2C[] = "cnp1 server-to-client";
constexpr char kLabelBind[] = "cnp1 channel binding";

void put_u32(std::vector<uint8_t> &out, uint32_t v)
{
    out.push_back((uint8_t)(v & 0xFF));
    out.push_back((uint8_t)((v >> 8) & 0xFF));
    out.push_back((uint8_t)((v >> 16) & 0xFF));
    out.push_back((uint8_t)((v >> 24) & 0xFF));
}

uint32_t get_u32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) |
           ((uint32_t)p[3] << 24);
}

// An all-zero shared secret is what X25519 returns for a small-order public
// key. Monocypher does not reject those for us, and a session whose secret is
// zero is a session anybody can read.
bool all_zero(const uint8_t *p, size_t n)
{
    uint8_t acc = 0;
    for (size_t i = 0; i < n; ++i) acc = (uint8_t)(acc | p[i]);
    return acc == 0;
}

void make_nonce(uint8_t nonce[kNonceBytes], uint64_t counter, uint8_t channel)
{
    std::memset(nonce, 0, kNonceBytes);
    for (int i = 0; i < 8; ++i) nonce[i] = (uint8_t)((counter >> (8 * i)) & 0xFF);
    nonce[8] = channel;
}

} // namespace

bool random_bytes(void *out, size_t size)
{
#ifdef _WIN32
    return BCryptGenRandom(nullptr, (PUCHAR)out, (ULONG)size, BCRYPT_USE_SYSTEM_PREFERRED_RNG) == 0;
#else
    // getentropy rather than getrandom: it is the one both Linux and macOS
    // have, and it never returns short -- only 256 bytes a call.
    uint8_t *p = (uint8_t *)out;
    for (size_t done = 0; done < size;) {
        const size_t n = std::min<size_t>(size - done, 256);
        if (getentropy(p + done, n) != 0) return false;
        done += n;
    }
    return true;
#endif
}

bool KeyPair::generate(KeyPair &out)
{
    if (!random_bytes(out.secret_key, kKeyBytes)) return false;
    crypto_x25519_public_key(out.public_key, out.secret_key);
    return true;
}

void KeyPair::from_secret(const uint8_t secret[kKeyBytes], KeyPair &out)
{
    std::memcpy(out.secret_key, secret, kKeyBytes);
    crypto_x25519_public_key(out.public_key, out.secret_key);
}

std::string to_hex(const uint8_t *data, size_t size)
{
    static const char *digits = "0123456789abcdef";
    std::string        s;
    s.reserve(size * 2);
    for (size_t i = 0; i < size; ++i) {
        s.push_back(digits[data[i] >> 4]);
        s.push_back(digits[data[i] & 0x0F]);
    }
    return s;
}

bool from_hex(const std::string &hex, uint8_t *out, size_t size)
{
    if (hex.size() != size * 2) return false;
    for (size_t i = 0; i < size; ++i) {
        uint8_t byte = 0;
        for (int half = 0; half < 2; ++half) {
            const char c = hex[i * 2 + (size_t)half];
            uint8_t    v;
            if (c >= '0' && c <= '9')      v = (uint8_t)(c - '0');
            else if (c >= 'a' && c <= 'f') v = (uint8_t)(c - 'a' + 10);
            else if (c >= 'A' && c <= 'F') v = (uint8_t)(c - 'A' + 10);
            else return false;
            byte = (uint8_t)((byte << 4) | v);
        }
        out[i] = byte;
    }
    return true;
}

std::string key_fingerprint(const uint8_t key[kKeyBytes])
{
    // The first eight bytes, in pairs. Not a substitute for comparing the whole
    // key -- which is what --server-key does -- but something a person can read
    // over the phone.
    const std::string hex = to_hex(key, 8);
    std::string       out;
    for (size_t i = 0; i < hex.size(); i += 4) {
        if (!out.empty()) out.push_back(':');
        out.append(hex, i, 4);
    }
    return out;
}

namespace {

// The file half of both key kinds: a hex secret, 0600, created if absent.
//
// One copy because there is one rule, and the rule is the part that matters --
// a private key other users can read is a private key that has already leaked,
// and a chmod after the write leaves a window in which it was.
bool load_or_create_secret(const std::string &path, uint8_t *secret, size_t size, bool &created,
                           std::string &err)
{
    created = false;

    // Read it; if it is not there, create it; and if the create loses a race
    // with another process, read the winner's. Two servers started at once in
    // one directory is not worth failing on, and O_EXCL is what makes that race
    // detectable rather than a silent overwrite of somebody's key.
    for (int attempt = 0; attempt < 2; ++attempt) {
        std::FILE *f = std::fopen(path.c_str(), "rb");
        if (f) {
            char         buf[128];
            const size_t n = std::fread(buf, 1, sizeof(buf) - 1, f);
            std::fclose(f);

            std::string hex(buf, n);
            while (!hex.empty() &&
                   (hex.back() == '\n' || hex.back() == '\r' || hex.back() == ' '))
                hex.pop_back();

            if (!from_hex(hex, secret, size)) {
                err = "'" + path + "' does not hold a " + std::to_string(size) +
                      "-byte key in hex";
                return false;
            }

            // A private key other users can read is a private key that has
            // already leaked. Reported rather than fixed: changing the mode of a
            // file somebody else set up is not this program's decision to make.
            struct stat st{};
            if (::stat(path.c_str(), &st) == 0 && (st.st_mode & 0077) != 0)
                err = "warning: '" + path + "' is readable by other users";
            return true;
        }

        if (!random_bytes(secret, size)) {
            err = "the system random generator refused";
            return false;
        }

        // 0600 from the moment it exists, not afterwards: a chmod after the
        // write leaves a window in which the key is world readable.
        const int fd = ::open(path.c_str(), O_WRONLY | O_CREAT | O_EXCL, 0600);
        if (fd < 0) {
            if (errno == EEXIST) continue;   // somebody else got there first
            err = "cannot create '" + path + "': " + std::strerror(errno);
            return false;
        }
        const std::string hex = to_hex(secret, size) + "\n";
        const ssize_t     w   = ::write(fd, hex.data(), hex.size());
        ::close(fd);
        if (w != (ssize_t)hex.size()) {
            err = "could not write '" + path + "'";
            return false;
        }

        created = true;
        return true;
    }

    err = "'" + path + "' appeared and vanished while it was being created";
    return false;
}

} // namespace

bool load_or_create_key(const std::string &path, KeyPair &out, bool &created, std::string &err)
{
    uint8_t secret[kKeyBytes];
    if (!load_or_create_secret(path, secret, kKeyBytes, created, err)) return false;
    KeyPair::from_secret(secret, out);
    crypto_wipe(secret, sizeof(secret));
    return true;
}

// --------------------------------------------------------------- signatures

bool SignKeyPair::generate(SignKeyPair &out)
{
    uint8_t seed[kSignSeedBytes];
    if (!random_bytes(seed, sizeof(seed))) return false;
    from_seed(seed, out);
    crypto_wipe(seed, sizeof(seed));
    return true;
}

void SignKeyPair::from_seed(const uint8_t seed[kSignSeedBytes], SignKeyPair &out,
                            uint8_t *seed_out)
{
    // monocypher wipes the seed it is handed, so a caller that wants to keep it
    // -- to write it to a file -- has to say so and gets a copy back.
    uint8_t work[kSignSeedBytes];
    std::memcpy(work, seed, sizeof(work));
    if (seed_out) std::memcpy(seed_out, seed, kSignSeedBytes);

    crypto_eddsa_key_pair(out.secret_key, out.public_key, work);
}

bool sign_key_from_password(const std::string &name, const std::string &password,
                            SignKeyPair &out)
{
    // The name is the salt, hashed to a fixed size: two players who pick the
    // same password still get different identities, and the same player gets
    // the same one wherever they type it.
    uint8_t salt[16];
    crypto_blake2b(salt, sizeof(salt), (const uint8_t *)name.data(), name.size());

    constexpr uint32_t kBlocks = 65536;   // 1 KiB each: 64 MiB
    std::vector<uint8_t> work;
    try {
        work.resize((size_t)kBlocks * 1024);
    } catch (const std::bad_alloc &) {
        return false;
    }

    uint8_t seed[kSignSeedBytes];
    crypto_argon2(seed, sizeof(seed), work.data(),
                  crypto_argon2_config{CRYPTO_ARGON2_ID, kBlocks, 3, 1},
                  crypto_argon2_inputs{(const uint8_t *)password.data(), salt,
                                       (uint32_t)password.size(), sizeof(salt)},
                  crypto_argon2_no_extras);

    SignKeyPair::from_seed(seed, out);
    crypto_wipe(seed, sizeof(seed));
    crypto_wipe(work.data(), work.size());
    return true;
}

void sign_message(const SignKeyPair &key, const void *message, size_t size,
                  uint8_t signature[kSignatureBytes])
{
    crypto_eddsa_sign(signature, key.secret_key, (const uint8_t *)message, size);
}

bool verify_message(const uint8_t public_key[kSignPublicBytes], const void *message, size_t size,
                    const uint8_t signature[kSignatureBytes])
{
    return crypto_eddsa_check(signature, public_key, (const uint8_t *)message, size) == 0;
}

namespace {

// This curve's EdDSA scalar is the first half of BLAKE2b(seed), which is an
// X25519 secret as it stands: crypto_x25519 trims it the same way.
bool shared_key(const SignKeyPair &me, const uint8_t other[kSignPublicBytes], uint8_t key[32])
{
    uint8_t hash[64], mine[32], theirs[32], check[32], raw[32];
    crypto_blake2b(hash, sizeof(hash), me.secret_key, kSignSeedBytes);
    std::memcpy(mine, hash, 32);
    crypto_eddsa_to_x25519(theirs, other);
    crypto_eddsa_to_x25519(check, me.public_key);
    crypto_x25519_public_key(hash, mine);
    const bool ok = crypto_verify32(hash, check) == 0;
    crypto_x25519(raw, mine, theirs);
    // Both public keys in, in the same order at both ends.
    uint8_t in[32 * 3];
    std::memcpy(in, raw, 32);
    const bool first = std::memcmp(me.public_key, other, 32) < 0;
    std::memcpy(in + 32, first ? me.public_key : other, 32);
    std::memcpy(in + 64, first ? other : me.public_key, 32);
    crypto_blake2b(key, 32, in, sizeof(in));
    crypto_wipe(hash, sizeof(hash));
    crypto_wipe(mine, sizeof(mine));
    crypto_wipe(raw, sizeof(raw));
    crypto_wipe(in, sizeof(in));
    return ok;
}

} // namespace

bool seal_for(const SignKeyPair &me, const uint8_t to[kSignPublicBytes], const std::string &plain,
              std::string &out)
{
    uint8_t key[32];
    out.assign(kNonceBytes + kMacBytes + plain.size(), '\0');
    uint8_t *o = (uint8_t *)out.data();
    if (!shared_key(me, to, key) || !random_bytes(o, kNonceBytes)) return false;
    crypto_aead_lock(o + kNonceBytes + kMacBytes, o + kNonceBytes, key, o, nullptr, 0,
                     (const uint8_t *)plain.data(), plain.size());
    crypto_wipe(key, sizeof(key));
    return true;
}

bool open_sealed(const SignKeyPair &me, const uint8_t from[kSignPublicBytes],
                 const std::string &sealed, std::string &plain)
{
    if (sealed.size() < kNonceBytes + kMacBytes) return false;
    uint8_t key[32];
    if (!shared_key(me, from, key)) return false;
    const uint8_t *s = (const uint8_t *)sealed.data();
    plain.assign(sealed.size() - kNonceBytes - kMacBytes, '\0');
    const bool ok = crypto_aead_unlock((uint8_t *)plain.data(), s + kNonceBytes, key, s, nullptr, 0,
                                       s + kNonceBytes + kMacBytes, plain.size()) == 0;
    crypto_wipe(key, sizeof(key));
    if (!ok) plain.clear();
    return ok;
}

bool load_or_create_sign_key(const std::string &path, SignKeyPair &out, bool &created,
                             std::string &err)
{
    uint8_t seed[kSignSeedBytes];
    if (!load_or_create_secret(path, seed, kSignSeedBytes, created, err)) return false;
    SignKeyPair::from_seed(seed, out);
    crypto_wipe(seed, sizeof(seed));
    return true;
}

SecureChannel::~SecureChannel()
{
    crypto_wipe(m_identity_secret, sizeof(m_identity_secret));
    crypto_wipe(m_eph_secret, sizeof(m_eph_secret));
    crypto_wipe(m_send_key, sizeof(m_send_key));
    crypto_wipe(m_recv_key, sizeof(m_recv_key));
    crypto_wipe(m_binding, sizeof(m_binding));
}

void SecureChannel::derive(const uint8_t *ephemeral_secret, const uint8_t *client_eph_pub,
                           const uint8_t *server_eph_pub, const uint8_t *server_static_pub)
{
    // Two exchanges, and each one buys something different:
    //
    //   ephemeral x ephemeral  -- forward secrecy. Neither long-term key can
    //                             recover this later, because both halves are
    //                             thrown away with the session.
    //   ephemeral x static     -- the server's identity. Only the holder of the
    //                             static secret can compute it, so a session
    //                             that decrypts is a session with that server.
    //
    // Both go into the hash. Using only the first would give a channel anybody
    // could impersonate; using only the second would give a channel that a
    // stolen key decrypts retroactively.
    uint8_t ss_ee[kKeyBytes];
    uint8_t ss_es[kKeyBytes];

    if (m_is_server) {
        crypto_x25519(ss_ee, m_eph_secret, client_eph_pub);
        crypto_x25519(ss_es, m_identity_secret, client_eph_pub);
    } else {
        crypto_x25519(ss_ee, ephemeral_secret, server_eph_pub);
        crypto_x25519(ss_es, ephemeral_secret, server_static_pub);
    }

    // The transcript goes in with the secrets, so two sessions that saw
    // different public keys can never derive the same key even if an attacker
    // could force the shared secrets to match.
    uint8_t transcript[kKeyBytes * 5];
    std::memcpy(transcript + 0 * kKeyBytes, ss_ee, kKeyBytes);
    std::memcpy(transcript + 1 * kKeyBytes, ss_es, kKeyBytes);
    std::memcpy(transcript + 2 * kKeyBytes, client_eph_pub, kKeyBytes);
    std::memcpy(transcript + 3 * kKeyBytes, server_eph_pub, kKeyBytes);
    std::memcpy(transcript + 4 * kKeyBytes, server_static_pub, kKeyBytes);

    uint8_t master[64];
    crypto_blake2b(master, sizeof(master), transcript, sizeof(transcript));

    // The binding, from the same master and a label of its own. A separate
    // label rather than reusing a traffic key: what gets signed and shown to a
    // second party must not be something an attacker can turn back into the key
    // that seals the traffic.
    crypto_blake2b_keyed(m_binding, kKeyBytes, master, sizeof(master), (const uint8_t *)kLabelBind,
                         sizeof(kLabelBind) - 1);

    uint8_t c2s[kKeyBytes], s2c[kKeyBytes];
    crypto_blake2b_keyed(c2s, kKeyBytes, master, sizeof(master), (const uint8_t *)kLabelC2S,
                         sizeof(kLabelC2S) - 1);
    crypto_blake2b_keyed(s2c, kKeyBytes, master, sizeof(master), (const uint8_t *)kLabelS2C,
                         sizeof(kLabelS2C) - 1);

    // Different keys per direction. Without this a frame could be reflected
    // back at whoever sent it and would authenticate perfectly.
    if (m_is_server) {
        std::memcpy(m_send_key, s2c, kKeyBytes);
        std::memcpy(m_recv_key, c2s, kKeyBytes);
    } else {
        std::memcpy(m_send_key, c2s, kKeyBytes);
        std::memcpy(m_recv_key, s2c, kKeyBytes);
    }

    crypto_wipe(ss_ee, sizeof(ss_ee));
    crypto_wipe(ss_es, sizeof(ss_es));
    crypto_wipe(transcript, sizeof(transcript));
    crypto_wipe(master, sizeof(master));
    crypto_wipe(c2s, sizeof(c2s));
    crypto_wipe(s2c, sizeof(s2c));
    crypto_wipe(m_eph_secret, sizeof(m_eph_secret));
}

bool SecureChannel::begin_client(const uint8_t *pinned, std::vector<uint8_t> &offer_out,
                                 std::string &err)
{
    m_is_server = false;

    if (pinned) {
        std::memcpy(m_pinned, pinned, kKeyBytes);
        m_has_pin = true;
    }

    if (!random_bytes(m_eph_secret, kKeyBytes)) {
        m_state = State::Failed;
        err     = "the system random generator refused";
        return false;
    }
    crypto_x25519_public_key(m_eph_public, m_eph_secret);

    offer_out.clear();
    offer_out.push_back(kFrameOffer);
    put_u32(offer_out, kMagic);
    offer_out.insert(offer_out.end(), m_eph_public, m_eph_public + kKeyBytes);

    m_state = State::Offered;
    return true;
}

void SecureChannel::begin_server(const KeyPair &identity)
{
    m_is_server = true;
    std::memcpy(m_identity_secret, identity.secret_key, kKeyBytes);
    std::memcpy(m_identity_public, identity.public_key, kKeyBytes);
    m_state = State::Offered;   // waiting for the client's offer
}

bool SecureChannel::accept_offer(const uint8_t *data, size_t size, std::vector<uint8_t> &accept_out,
                                 std::string &err)
{
    if (size != 1 + 4 + kKeyBytes || data[0] != kFrameOffer) {
        m_state = State::Failed;
        err     = "the first packet is not a key offer";
        return false;
    }
    if (get_u32(data + 1) != kMagic) {
        m_state = State::Failed;
        err     = "the key offer is not this protocol";
        return false;
    }

    uint8_t client_eph[kKeyBytes];
    std::memcpy(client_eph, data + 5, kKeyBytes);

    if (!random_bytes(m_eph_secret, kKeyBytes)) {
        m_state = State::Failed;
        err     = "the system random generator refused";
        return false;
    }
    crypto_x25519_public_key(m_eph_public, m_eph_secret);

    // Rejected before the key is derived, not after: a small-order public key
    // makes the shared secret all zeroes, and a session keyed on zero is a
    // session anybody who knows the trick can read.
    uint8_t probe[kKeyBytes];
    crypto_x25519(probe, m_eph_secret, client_eph);
    if (all_zero(probe, kKeyBytes)) {
        crypto_wipe(probe, sizeof(probe));
        m_state = State::Failed;
        err     = "the key offer is degenerate";
        return false;
    }
    crypto_wipe(probe, sizeof(probe));

    derive(nullptr, client_eph, m_eph_public, m_identity_public);

    accept_out.clear();
    accept_out.push_back(kFrameAccept);
    put_u32(accept_out, kMagic);
    accept_out.insert(accept_out.end(), m_identity_public, m_identity_public + kKeyBytes);
    accept_out.insert(accept_out.end(), m_eph_public, m_eph_public + kKeyBytes);

    // The server never authenticates the client here. It cannot: a player's
    // identity key is M6, and until then the name in the hello is all there is.
    m_authenticated = false;
    m_state         = State::Ready;
    return true;
}

bool SecureChannel::accept_accept(const uint8_t *data, size_t size, std::string &err)
{
    if (m_state != State::Offered) {
        m_state = State::Failed;
        err     = "a key acceptance arrived at the wrong time";
        return false;
    }
    if (size != 1 + 4 + 2 * kKeyBytes || data[0] != kFrameAccept) {
        m_state = State::Failed;
        err     = "the answer is not a key acceptance";
        return false;
    }
    if (get_u32(data + 1) != kMagic) {
        m_state = State::Failed;
        err     = "the key acceptance is not this protocol";
        return false;
    }

    const uint8_t *server_static = data + 5;
    const uint8_t *server_eph    = data + 5 + kKeyBytes;

    std::memcpy(m_peer_identity, server_static, kKeyBytes);

    // The pin is checked before anything is derived. A client that asked for a
    // particular server and got another one has nothing to talk about.
    if (m_has_pin) {
        if (crypto_verify32(m_pinned, server_static) != 0) {
            m_state = State::Failed;
            err     = "the server's key is not the one that was pinned (it presented " +
                      key_fingerprint(server_static) + ", expected " + key_fingerprint(m_pinned) +
                      ")";
            return false;
        }
        m_authenticated = true;
    }

    uint8_t probe[kKeyBytes];
    crypto_x25519(probe, m_eph_secret, server_eph);
    const bool degenerate = all_zero(probe, kKeyBytes);
    crypto_wipe(probe, sizeof(probe));
    if (degenerate) {
        m_state = State::Failed;
        err     = "the server's ephemeral key is degenerate";
        return false;
    }

    uint8_t eph_secret[kKeyBytes];
    std::memcpy(eph_secret, m_eph_secret, kKeyBytes);
    derive(eph_secret, m_eph_public, server_eph, server_static);
    crypto_wipe(eph_secret, sizeof(eph_secret));

    m_state = State::Ready;
    return true;
}

bool SecureChannel::seal(uint8_t channel, const void *plain, size_t size,
                         std::vector<uint8_t> &out)
{
    if (m_state != State::Ready || channel >= kChannels) return false;

    // A ceiling on what can be sealed, because the size below is added to a
    // constant and an addition that wraps would allocate a buffer smaller than
    // what gets written into it. The largest message this protocol produces is
    // an incompressible chunk at about 91 KB, so four megabytes is far above
    // anything real and far below where the arithmetic gets interesting.
    if (size > kMaxSealedPayload) return false;

    const uint64_t counter = m_send_counter[channel]++;

    uint8_t nonce[kNonceBytes];
    make_nonce(nonce, counter, channel);

    out.clear();
    out.resize(kSealOverhead + size);
    out[0] = kFrameSealed;
    for (int i = 0; i < 8; ++i) out[1 + (size_t)i] = (uint8_t)((counter >> (8 * i)) & 0xFF);

    crypto_aead_lock(out.data() + 1 + 8 + kMacBytes, out.data() + 1 + 8, m_send_key, nonce,
                     nullptr, 0, (const uint8_t *)plain, size);
    return true;
}

bool SecureChannel::open(uint8_t channel, const uint8_t *data, size_t size,
                         std::vector<uint8_t> &out, std::string &err)
{
    if (m_state != State::Ready) {
        err = "a sealed frame arrived before the key exchange finished";
        return false;
    }
    if (channel >= kChannels) {
        err = "a sealed frame arrived on a channel that does not exist";
        return false;
    }
    if (size < kSealOverhead || data[0] != kFrameSealed) {
        err = "not a sealed frame";
        return false;
    }

    uint64_t counter = 0;
    for (int i = 0; i < 8; ++i) counter |= (uint64_t)data[1 + (size_t)i] << (8 * i);

    // Replay, checked before the decryption rather than after. A frame that has
    // already been accepted is not made acceptable by being authentic -- that
    // is exactly what a replay is.
    if (m_recv_any[channel]) {
        const uint64_t newest = m_recv_newest[channel];
        if (counter <= newest) {
            const uint64_t age = newest - counter;
            if (age >= 64) {
                err = "a sealed frame arrived too far behind to be checked for replay";
                return false;
            }
            if (m_recv_seen[channel] & (1ull << age)) {
                err = "a sealed frame was replayed";
                return false;
            }
        }
    }

    uint8_t nonce[kNonceBytes];
    make_nonce(nonce, counter, channel);

    out.assign(size - kSealOverhead, 0);
    if (crypto_aead_unlock(out.data(), data + 1 + 8, m_recv_key, nonce, nullptr, 0,
                           data + kSealOverhead, size - kSealOverhead) != 0) {
        out.clear();
        err = "a sealed frame did not authenticate";
        return false;
    }

    // Only now does the window move. Updating it before the tag was checked
    // would let anybody who can send packets burn counters and lock out the
    // real peer.
    // Bit 0 of the window is the newest counter accepted, bit k is k behind it.
    if (!m_recv_any[channel]) {
        m_recv_seen[channel]   = 1;
        m_recv_newest[channel] = counter;
        m_recv_any[channel]    = true;
    } else if (counter > m_recv_newest[channel]) {
        const uint64_t shift = counter - m_recv_newest[channel];
        // A jump of 64 or more retires the whole window. Shifting by 64 is
        // undefined rather than zero, which is why this is a branch.
        m_recv_seen[channel]   = shift >= 64 ? 1ull : ((m_recv_seen[channel] << shift) | 1ull);
        m_recv_newest[channel] = counter;
    } else {
        m_recv_seen[channel] |= (1ull << (m_recv_newest[channel] - counter));
    }

    return true;
}

} // namespace net
} // namespace cnp
