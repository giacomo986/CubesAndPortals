#pragma once

#include "core/media/content_hash.h"

#include <string>
#include <vector>

// Media on the client's disk, keyed by what they are.
//
// DESIGN.md 4.5: "cache condivisa su disco tra tutte le sessioni. Due server
// che spediscono la stessa texture la scaricano una volta sola." One directory
// for the whole client, not one per world and not one per session -- the
// sharing IS the feature, and a per-session cache would satisfy every other
// sentence in this file while missing the point of it.
//
// **Every read is verified.** The file is hashed again and compared with the
// key before its bytes are handed out. A content-addressed cache that trusts
// its own filenames is a directory where anything anybody can write becomes
// something the renderer draws -- and this cache is shared between worlds, so
// the thing writing to it is not necessarily the world reading from it. The
// cost is one BLAKE2b pass over a few kilobytes, which is microseconds; the
// alternative is a class of bug that cannot be found by looking at the wire.
//
// A file that fails that check is deleted and reported as a miss, so the next
// session fetches it again rather than failing forever on a corpse.
//
// **It has a size, and it stays under it.** A cache shared by every world a
// player has ever looked into is a directory that only ever grows, and the day
// that becomes a problem is a day months from now on somebody else's disk. The
// oldest files go first, and "oldest" means least recently USED and not least
// recently written -- a texture every world sends is a texture worth keeping
// however long ago it first arrived.

namespace cnp {

// What the cache is allowed to hold, when nobody says otherwise.
//
// Measured rather than picked: one of this engine's worlds is 31 files and 127
// KB of generated placeholders, so this is about four thousand of them. Real
// art out of a real encoder is smaller per file and there is more of it -- a
// heavily modded game with a texture per face is a few megabytes -- which puts
// the same number at something like a hundred and fifty worlds. That is a
// player's history, not a session's, which is the scale this is for.
inline constexpr size_t kMediaCacheBudget = 256u * 1024u * 1024u;

// How stale a file's timestamp may be before a hit refreshes it.
//
// Least-recently-used needs a "used" that reaches the disk, and the obvious way
// -- touch on every hit -- turns every read of the cache into a write. An hour
// is far finer than the granularity eviction cares about and costs at most one
// write per file per hour.
inline constexpr double kMediaTouchInterval = 3600.0;

// How long one session may hold the right to fetch a file before another
// session may take it over. Longer than the request timeout the session itself
// uses, so the ordinary case is that the fetcher gives it back rather than
// having it taken.
inline constexpr double kMediaClaimTimeout = 20.0;

class MediaCache {
public:
    // Creates the directory if it is not there, and counts what is already in
    // it. Returns false and fills `err` for a path that cannot be made --
    // which is not fatal for a client: a session with no cache still works, it
    // just downloads everything every time, and it should say so rather than
    // pretend.
    //
    // The scan is what makes the budget cheap afterwards: the size is counted
    // once here and maintained from then on, so a store does not walk the
    // directory and a world arriving does not walk it once per file.
    bool open(const std::string &dir, std::string &err, size_t budget = kMediaCacheBudget);

    bool ready() const { return m_ready; }
    const std::string &dir() const { return m_dir; }

    // Whether the file is there. Cheap: it stats, it does not hash. The hash
    // check happens on load(), because that is where the bytes are actually
    // used and where being wrong would matter.
    bool has(const ContentHash &hash) const;

    // Reads and verifies. Returns false for absent, unreadable, or wrong.
    bool load(const ContentHash &hash, std::vector<uint8_t> &out);

    // Writes, after checking the bytes hash to the key. A caller that passes
    // mismatched arguments is a caller that just took a server at its word.
    bool store(const ContentHash &hash, const std::vector<uint8_t> &bytes, std::string &err);

    // ------------------------------------------------- one fetch, one file
    //
    // "Two servers that send the same texture download it once" is a claim
    // about two SESSIONS, and until this it only held when their transfers did
    // not overlap. On loopback the first window's files always landed before
    // the second window's offer arrived; at 400 ms round trip they overlap, and
    // two sessions asked for the same twenty files -- 51 fetched where 31 were
    // distinct.
    //
    // So the cache tracks what is being fetched as well as what is held. claim()
    // returns true to exactly one caller per hash; everybody else waits and
    // finds it on disk.
    //
    // A claim expires after `kMediaClaimTimeout`, so a session that asked and
    // died does not make a file unfetchable for the rest of the process. That
    // is what renew() is for: a session that is still receiving says so, and
    // twenty seconds stops meaning "this transfer is too slow to be real". At
    // 400 ms and 25% loss a batch takes longer than that, the claim expired
    // under a session that was working fine, and the second window fetched the
    // same files again -- 52 for 31 distinct.
    bool claim(const ContentHash &hash, double now);
    void renew(const ContentHash &hash, double now);
    void release(const ContentHash &hash);
    size_t claims_held() const { return m_claims.size(); }

    // Claims that expired under whoever held them and were taken over. Should
    // be zero on a link where a transfer finishes inside the timeout, and it is
    // the number that says whether a duplicate download came from an expiry or
    // from somewhere else entirely.
    long   claims_taken_over() const { return m_claims_taken; }

    // Counters, for the gate that has to show a second world downloading
    // nothing. Per cache and therefore per client, which is the scope the
    // sharing claim is made at.
    // What is in it and what it is allowed to be. Reported rather than
    // assumed: "the cache works" and "the cache is not quietly eating the
    // player's disk" are different claims.
    size_t bytes_held() const { return m_bytes_held; }
    size_t files_held() const { return m_files_held; }
    size_t budget() const { return m_budget; }
    double scan_seconds() const { return m_scan_seconds; }

    // What eviction has thrown away this session, and whether it could not
    // throw away enough. The second one matters: a single world whose media are
    // bigger than the whole budget would otherwise evict its own files as they
    // arrive and thrash forever, so files this session has touched are never
    // evicted -- and when that leaves the cache over its budget it says so
    // instead of pretending.
    long   evicted_files() const { return m_evicted_files; }
    size_t evicted_bytes() const { return m_evicted_bytes; }
    bool   over_budget() const { return m_bytes_held > m_budget; }

    long   hits() const { return m_hits; }
    long   misses() const { return m_misses; }
    long   stores() const { return m_stores; }
    long   rejected() const { return m_rejected; }
    size_t bytes_stored() const { return m_bytes_stored; }
    size_t bytes_served() const { return m_bytes_served; }

    void reset_counters();

    // Throws away the least recently used unpinned files until the cache is at
    // or under `target`. Called by store() when the budget is passed; exposed
    // because a test that could only reach it through a real overflow would be
    // a test that has to write a quarter of a gigabyte.
    void evict_to(size_t target);

private:
    std::string path_for(const ContentHash &hash) const;

    // Counts what is on disk and, on the way, drops anything that is not a
    // file this cache would have written.
    void scan();

    // Files this session has stored or read. Never evicted: the world being
    // looked at now is the one whose textures are about to be drawn, and a
    // cache that evicted them to make room for the rest of the same world
    // would download it forever.
    bool pinned(const ContentHash &hash) const;
    void pin(const ContentHash &hash);

    std::string m_dir;
    bool        m_ready = false;

    size_t m_budget        = kMediaCacheBudget;
    size_t m_bytes_held    = 0;
    size_t m_files_held    = 0;
    double m_scan_seconds  = 0.0;
    long   m_evicted_files = 0;
    size_t m_evicted_bytes = 0;

    std::vector<ContentHash> m_pinned;

    // Hash and when it was claimed, sorted by hash. Small -- at most one
    // world's content set in flight -- and searched the same way m_pinned is.
    std::vector<std::pair<ContentHash, double>> m_claims;
    long                                        m_claims_taken = 0;

    long   m_hits         = 0;
    long   m_misses       = 0;
    long   m_stores       = 0;
    long   m_rejected     = 0;
    size_t m_bytes_stored = 0;
    size_t m_bytes_served = 0;
};

// Creates a directory and every parent it needs. Here because the cache needs
// it and mkdir does not do it; exposed because the texture baker needs the same
// thing and two copies of it is two things to get wrong.
bool make_dirs(const std::string &path, std::string &err);

} // namespace cnp
