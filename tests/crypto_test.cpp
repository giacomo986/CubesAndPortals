// The session encryption, decided on buffers.
//
// Same split as protocol_test: no socket, no ENet, no ports. Two SecureChannels
// in one process, handed each other's frames by hand, which is what makes it
// possible to ask the questions that matter -- what happens to a frame with one
// bit changed, a frame sent twice, a frame moved to another channel, a frame
// reflected back at its sender. None of those are things a working network
// produces, so a test that only runs over a working network never asks them.
//
// The end-to-end story is elsewhere: net_loopback runs the real handshake over
// a real socket with encryption on, and two_process_join and four_clients run
// it between real processes.

#include "core/net/crypto.h"

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include <sys/stat.h>

using namespace cnp;
using namespace cnp::net;

namespace {

int g_failures = 0;

void check(bool cond, const char *what)
{
    std::printf(cond ? "  ok    %s\n" : "  FAIL  %s\n", what);
    if (!cond) ++g_failures;
}

// Runs a full exchange between a fresh pair. `pin` is what the client insists
// on, or null. Returns whether both ends came out ready.
bool handshake(SecureChannel &client, SecureChannel &server, const KeyPair &identity,
               const uint8_t *pin, std::string &err)
{
    std::vector<uint8_t> offer, accept;

    server.begin_server(identity);
    if (!client.begin_client(pin, offer, err)) return false;
    if (!server.accept_offer(offer.data(), offer.size(), accept, err)) return false;
    if (!client.accept_accept(accept.data(), accept.size(), err)) return false;
    return client.ready() && server.ready();
}

std::vector<uint8_t> bytes(const char *s)
{
    return std::vector<uint8_t>(s, s + std::strlen(s));
}

} // namespace

int main()
{
    std::printf("session encryption\n");

    KeyPair identity;
    check(KeyPair::generate(identity), "an identity key can be generated");

    // The public half is derived from the secret half, never stored twice. Two
    // KeyPairs made from the same secret must agree, or a server that reloads
    // its key file becomes a different server.
    {
        KeyPair again;
        KeyPair::from_secret(identity.secret_key, again);
        check(std::memcmp(again.public_key, identity.public_key, kKeyBytes) == 0,
              "the public key is derived from the secret, not remembered alongside it");
    }

    // ---------------------------------------------------------------- basics
    {
        SecureChannel client, server;
        std::string   err;
        check(handshake(client, server, identity, nullptr, err),
              "a client and a server agree on a key");

        const std::vector<uint8_t> message = bytes("the block table and everything after it");

        std::vector<uint8_t> sealed;
        check(client.seal(0, message.data(), message.size(), sealed),
              "the client can seal a message");
        check(sealed.size() == message.size() + kSealOverhead,
              "a sealed frame costs exactly the tag, the counter and the MAC");

        // The plaintext must not be visible in the frame. Obvious, and worth a
        // check anyway: an "encryption" layer that forgot to encrypt would pass
        // every round trip below.
        bool found = false;
        for (size_t i = 0; i + message.size() <= sealed.size(); ++i)
            if (std::memcmp(sealed.data() + i, message.data(), message.size()) == 0) found = true;
        check(!found, "and the plaintext is not in it");

        std::vector<uint8_t> opened;
        check(server.open(0, sealed.data(), sealed.size(), opened, err) && opened == message,
              "the server opens it and gets back what was sent");
    }

    // ------------------------------------------------------------- integrity
    {
        SecureChannel client, server;
        std::string   err;
        handshake(client, server, identity, nullptr, err);

        const std::vector<uint8_t> message = bytes("a chunk");
        std::vector<uint8_t>       sealed;
        client.seal(1, message.data(), message.size(), sealed);

        // One bit, in the ciphertext.
        std::vector<uint8_t> tampered = sealed;
        tampered[tampered.size() - 1] ^= 0x01;
        std::vector<uint8_t> out;
        check(!server.open(1, tampered.data(), tampered.size(), out, err),
              "a frame with one bit changed does not open");

        // One bit, in the MAC.
        tampered = sealed;
        tampered[10] ^= 0x01;
        check(!server.open(1, tampered.data(), tampered.size(), out, err),
              "a frame with a changed MAC does not open");

        // Truncated.
        tampered.assign(sealed.begin(), sealed.end() - 1);
        check(!server.open(1, tampered.data(), tampered.size(), out, err),
              "a truncated frame does not open");

        // Nothing at all, and one byte short of a header.
        check(!server.open(1, nullptr, 0, out, err), "an empty frame does not open");
        std::vector<uint8_t> stub(kSealOverhead - 1, 3);
        check(!server.open(1, stub.data(), stub.size(), out, err),
              "a frame shorter than the header does not open");

        // And after all that, the real one still opens: none of the rejections
        // above may have moved the replay window, or an attacker sending
        // rubbish could lock out the peer they are attacking.
        check(server.open(1, sealed.data(), sealed.size(), out, err) && out == message,
              "and the genuine frame still opens afterwards");
    }

    // ---------------------------------------------------------------- replay
    {
        SecureChannel client, server;
        std::string   err;
        handshake(client, server, identity, nullptr, err);

        const std::vector<uint8_t> message = bytes("i am here");
        std::vector<uint8_t>       sealed;
        client.seal(2, message.data(), message.size(), sealed);

        std::vector<uint8_t> out;
        check(server.open(2, sealed.data(), sealed.size(), out, err),
              "a position claim opens once");
        check(!server.open(2, sealed.data(), sealed.size(), out, err),
              "and the same frame sent again is refused");

        // Out of order, but new: channel 2 is unreliable and reorders, so this
        // has to be accepted or every burst would cost entity updates.
        std::vector<uint8_t> a, b, c;
        client.seal(2, message.data(), message.size(), a);
        client.seal(2, message.data(), message.size(), b);
        client.seal(2, message.data(), message.size(), c);
        check(server.open(2, c.data(), c.size(), out, err), "a newer frame opens");
        check(server.open(2, a.data(), a.size(), out, err),
              "and one that overtook it still opens afterwards");
        check(server.open(2, b.data(), b.size(), out, err), "as does the one between them");
        check(!server.open(2, a.data(), a.size(), out, err),
              "but none of them a second time");

        // Far behind: past the window, and refused rather than accepted on the
        // grounds that it authenticates.
        std::vector<uint8_t> old_frame;
        client.seal(3, message.data(), message.size(), old_frame);
        std::vector<uint8_t> scratch;
        for (int i = 0; i < 200; ++i) {
            client.seal(3, message.data(), message.size(), scratch);
            server.open(3, scratch.data(), scratch.size(), out, err);
        }
        check(!server.open(3, old_frame.data(), old_frame.size(), out, err),
              "a frame from two hundred packets ago is too old to be judged, and refused");
    }

    // ------------------------------------------------------------- direction
    {
        SecureChannel client, server;
        std::string   err;
        handshake(client, server, identity, nullptr, err);

        const std::vector<uint8_t> message = bytes("hello");
        std::vector<uint8_t>       sealed;
        client.seal(0, message.data(), message.size(), sealed);

        // Reflected back at the sender. The two directions use different keys
        // precisely so this cannot work; with one shared key it would open and
        // authenticate perfectly.
        std::vector<uint8_t> out;
        check(!client.open(0, sealed.data(), sealed.size(), out, err),
              "a frame reflected back at its sender does not open");

        // Moved to another channel. The channel number is in the nonce, so a
        // position claim cannot be replayed as a chunk.
        check(!server.open(1, sealed.data(), sealed.size(), out, err),
              "a frame moved to another channel does not open");
        check(server.open(0, sealed.data(), sealed.size(), out, err),
              "and opens on the channel it was sealed for");
    }

    // ----------------------------------------------------------- independence
    {
        // Two sessions with the same server must not share a key, or one player
        // could read another's traffic. The ephemeral halves are what makes them
        // differ.
        SecureChannel c1, s1, c2, s2;
        std::string   err;
        handshake(c1, s1, identity, nullptr, err);
        handshake(c2, s2, identity, nullptr, err);

        const std::vector<uint8_t> message = bytes("private");
        std::vector<uint8_t>       sealed;
        c1.seal(0, message.data(), message.size(), sealed);

        std::vector<uint8_t> out;
        check(!s2.open(0, sealed.data(), sealed.size(), out, err),
              "a second session cannot open the first session's frames");
    }

    // ------------------------------------------------------------------ pins
    {
        SecureChannel client, server;
        std::string   err;
        check(handshake(client, server, identity, identity.public_key, err),
              "a client that pins the right key gets in");
        check(client.authenticated(), "and knows the server proved who it is");
    }
    {
        SecureChannel client, server;
        std::string   err;
        check(handshake(client, server, identity, nullptr, err),
              "a client that pins nothing also gets in");
    }
    {
        SecureChannel client, server;
        std::string   err;
        handshake(client, server, identity, nullptr, err);
        check(!client.authenticated(),
              "but is told it has no proof of which server it reached");
    }
    {
        // The impostor: a different server, presenting its own honest key, to a
        // client that asked for this one.
        KeyPair other;
        KeyPair::generate(other);

        SecureChannel client, server;
        std::string   err;
        check(!handshake(client, server, other, identity.public_key, err),
              "a client that pinned one key refuses a server presenting another");
        check(err.find("pinned") != std::string::npos,
              "and the message says so rather than blaming the network");
        check(client.state() == SecureChannel::State::Failed,
              "and the channel is dead rather than half open");
    }

    // ------------------------------------------------------------ degenerate
    {
        // An all-zero public key drives X25519 to an all-zero shared secret. A
        // peer offering one is either broken or trying to key the session on a
        // number everybody knows.
        SecureChannel server;
        server.begin_server(identity);

        std::vector<uint8_t> offer;
        offer.push_back(1);
        // magic, little endian, then thirty-two zero bytes
        offer.push_back(0x43); offer.push_back(0x4E); offer.push_back(0x50); offer.push_back(0x31);
        for (size_t i = 0; i < kKeyBytes; ++i) offer.push_back(0);

        std::vector<uint8_t> accept;
        std::string          err;
        check(!server.accept_offer(offer.data(), offer.size(), accept, err),
              "an all-zero public key is refused");
    }
    {
        SecureChannel server;
        server.begin_server(identity);

        std::vector<uint8_t> junk(37, 0x41);   // right length, wrong everything
        std::vector<uint8_t> accept;
        std::string          err;
        check(!server.accept_offer(junk.data(), junk.size(), accept, err),
              "a first packet that is not a key offer is refused");

        SecureChannel server2;
        server2.begin_server(identity);
        std::vector<uint8_t> empty;
        check(!server2.accept_offer(empty.data(), empty.size(), accept, err),
              "an empty first packet is refused");
    }

    // ----------------------------------------------------------------- order
    {
        // Nothing can be sealed before the exchange finishes. Without this a
        // caller could send in the clear by asking at the wrong moment and
        // getting an empty frame it did not check.
        SecureChannel client;
        std::vector<uint8_t> offer, out;
        std::string          err;
        client.begin_client(nullptr, offer, err);
        const std::vector<uint8_t> message = bytes("too early");
        check(!client.seal(0, message.data(), message.size(), out),
              "nothing can be sealed before the key exchange finishes");
        check(!client.open(0, offer.data(), offer.size(), out, err),
              "and nothing can be opened either");
    }

    // ------------------------------------------------------------------- hex
    {
        uint8_t key[kKeyBytes];
        for (size_t i = 0; i < kKeyBytes; ++i) key[i] = (uint8_t)(i * 7);
        const std::string hex = to_hex(key, kKeyBytes);
        check(hex.size() == kKeyBytes * 2, "a key is 64 hex characters");

        uint8_t back[kKeyBytes];
        check(from_hex(hex, back, kKeyBytes) && std::memcmp(back, key, kKeyBytes) == 0,
              "and reading them back gives the same key");
        check(!from_hex(hex.substr(1), back, kKeyBytes), "a short one is refused");
        check(!from_hex(hex + "00", back, kKeyBytes), "a long one is refused");

        std::string bad = hex;
        bad[5] = 'z';
        check(!from_hex(bad, back, kKeyBytes), "and one with a character that is not hex");
    }

    // ------------------------------------------------------------- key files
    {
        const std::string path = "cnp_test_key.tmp";
        std::remove(path.c_str());

        KeyPair     first;
        bool        created = false;
        std::string err;
        check(load_or_create_key(path, first, created, err) && created,
              "a key file is created when it is not there");

        KeyPair     second;
        bool        created2 = true;
        check(load_or_create_key(path, second, created2, err) && !created2,
              "and read back rather than replaced the next time");
        check(std::memcmp(first.public_key, second.public_key, kKeyBytes) == 0,
              "and it is the same server both times");

        struct stat st{};
        check(::stat(path.c_str(), &st) == 0 && (st.st_mode & 0077) == 0,
              "and no other user can read it");

        // Rubbish in the file is refused rather than turned into a key.
        std::FILE *f = std::fopen(path.c_str(), "wb");
        std::fputs("not a key\n", f);
        std::fclose(f);
        KeyPair broken;
        check(!load_or_create_key(path, broken, created, err),
              "a key file that does not hold a key is refused");

        std::remove(path.c_str());
    }

    std::printf(g_failures == 0 ? "session encryption passed\n" : "SESSION ENCRYPTION FAILED\n");
    return g_failures == 0 ? 0 : 1;
}
