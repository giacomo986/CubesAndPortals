#include "core/media/media_cache.h"

#include "core/media/media_limits.h"

#include <algorithm>
#include <chrono>
#include <cstring>
#include <cstdio>
#include <ctime>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <utime.h>

#ifdef _WIN32
#include <direct.h>
#define mkdir(path, mode) _mkdir(path)   // Windows has no permission bits to give
#endif

namespace cnp {

bool make_dirs(const std::string &path, std::string &err)
{
    if (path.empty()) {
        err = "empty path";
        return false;
    }

    std::string so_far;
    size_t      at = 0;
    if (path[0] == '/') {
        so_far = "/";
        at     = 1;
    }

    while (at <= path.size()) {
        const size_t slash = path.find('/', at);
        const size_t end   = slash == std::string::npos ? path.size() : slash;
        if (end > at) {
            so_far += path.substr(at, end - at);
            if (::mkdir(so_far.c_str(), 0755) != 0) {
                struct stat st;
                if (::stat(so_far.c_str(), &st) != 0 || !S_ISDIR(st.st_mode)) {
                    err = "cannot create " + so_far;
                    return false;
                }
            }
            so_far += "/";
        }
        if (slash == std::string::npos) break;
        at = slash + 1;
    }
    return true;
}

bool MediaCache::open(const std::string &dir, std::string &err, size_t budget)
{
    m_ready  = false;
    m_dir    = dir;
    m_budget = budget;
    m_pinned.clear();
    if (dir.empty()) {
        err = "no cache directory";
        return false;
    }
    if (!make_dirs(dir, err)) return false;
    m_ready = true;

    scan();

    // A budget that shrank between two runs, or a directory somebody filled by
    // other means. Nothing is pinned yet, so this is the one moment eviction
    // can reach everything.
    if (m_bytes_held > m_budget) evict_to(m_budget);
    return true;
}

// The size of the cache, counted once.
//
// Two levels of hex fan-out and nothing else, which is what makes this cheap
// enough to do at startup: it is a fixed 256 directories however many files
// they hold, and anything whose name is not a hash is not something this cache
// wrote and is not counted.
void MediaCache::scan()
{
    const auto t0 = std::chrono::steady_clock::now();

    m_bytes_held = 0;
    m_files_held = 0;

    DIR *top = ::opendir(m_dir.c_str());
    if (top == nullptr) return;

    while (const dirent *outer = ::readdir(top)) {
        const std::string name = outer->d_name;
        if (name.size() != 2) continue;   // "." and ".." fall out here too

        const std::string sub = m_dir + "/" + name;
        DIR              *d   = ::opendir(sub.c_str());
        if (d == nullptr) continue;

        while (const dirent *inner = ::readdir(d)) {
            const std::string file = inner->d_name;
            if (file.size() != kContentHashBytes * 2) continue;

            ContentHash h;
            if (!content_hash_from_hex(file, h)) continue;

            struct stat st;
            if (::stat((sub + "/" + file).c_str(), &st) != 0 || !S_ISREG(st.st_mode)) continue;

            m_bytes_held += (size_t)st.st_size;
            ++m_files_held;
        }
        ::closedir(d);
    }
    ::closedir(top);

    m_scan_seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
}

// Kept sorted, and searched rather than scanned. A world of thirty-one files
// would not notice the difference; eviction walks every file in the cache and
// asks this about each one, so a linear list turns "a big cache meets a big
// world" into tens of millions of comparisons for no reason.
static bool hash_less(const ContentHash &a, const ContentHash &b)
{
    return std::memcmp(a.bytes, b.bytes, kContentHashBytes) < 0;
}

bool MediaCache::pinned(const ContentHash &hash) const
{
    const auto it = std::lower_bound(m_pinned.begin(), m_pinned.end(), hash, hash_less);
    return it != m_pinned.end() && *it == hash;
}

void MediaCache::pin(const ContentHash &hash)
{
    const auto it = std::lower_bound(m_pinned.begin(), m_pinned.end(), hash, hash_less);
    if (it == m_pinned.end() || !(*it == hash)) m_pinned.insert(it, hash);
}

void MediaCache::evict_to(size_t target)
{
    if (!m_ready || m_bytes_held <= target) return;

    // Everything that may go, with when it was last used. Collected in one
    // pass and sorted, rather than repeatedly hunting for the oldest: eviction
    // happens when a world's media overflow the budget, which is a burst of
    // files and not a trickle.
    struct Entry {
        std::string path;
        size_t      size = 0;
        time_t      used = 0;
    };
    std::vector<Entry> entries;

    DIR *top = ::opendir(m_dir.c_str());
    if (top == nullptr) return;

    while (const dirent *outer = ::readdir(top)) {
        const std::string name = outer->d_name;
        if (name.size() != 2) continue;

        const std::string sub = m_dir + "/" + name;
        DIR              *d   = ::opendir(sub.c_str());
        if (d == nullptr) continue;

        while (const dirent *inner = ::readdir(d)) {
            const std::string file = inner->d_name;
            if (file.size() != kContentHashBytes * 2) continue;

            ContentHash h;
            if (!content_hash_from_hex(file, h)) continue;
            if (pinned(h)) continue;   // this session is using it

            const std::string path = sub + "/" + file;
            struct stat       st;
            if (::stat(path.c_str(), &st) != 0 || !S_ISREG(st.st_mode)) continue;

            entries.push_back(Entry{path, (size_t)st.st_size, st.st_mtime});
        }
        ::closedir(d);
    }
    ::closedir(top);

    // Least recently used first. The timestamp is a USE and not a write: load()
    // refreshes it, so a texture every world sends stays however long ago it
    // first arrived -- which is the whole difference between an LRU and a
    // first-in-first-out that would throw away exactly the shared files this
    // cache exists to keep.
    std::sort(entries.begin(), entries.end(),
              [](const Entry &a, const Entry &b) { return a.used < b.used; });

    for (const Entry &e : entries) {
        if (m_bytes_held <= target) break;
        if (std::remove(e.path.c_str()) != 0) continue;
        m_bytes_held -= e.size < m_bytes_held ? e.size : m_bytes_held;
        if (m_files_held > 0) --m_files_held;
        ++m_evicted_files;
        m_evicted_bytes += e.size;
    }
}

std::string MediaCache::path_for(const ContentHash &hash) const
{
    // Two hex digits of fan-out. A cache shared by every world a player has
    // ever looked into is a directory that keeps growing, and some filesystems
    // walk a directory linearly to open one file in it. Two levels is the
    // difference between 256 entries per directory and all of them in one.
    const std::string hex = hash.hex();
    return m_dir + "/" + hex.substr(0, 2) + "/" + hex;
}

bool MediaCache::has(const ContentHash &hash) const
{
    if (!m_ready || hash.empty()) return false;
    struct stat st;
    return ::stat(path_for(hash).c_str(), &st) == 0 && S_ISREG(st.st_mode);
}

bool MediaCache::load(const ContentHash &hash, std::vector<uint8_t> &out)
{
    if (!m_ready || hash.empty()) {
        ++m_misses;
        return false;
    }

    const std::string path = path_for(hash);
    FILE             *f    = std::fopen(path.c_str(), "rb");
    if (!f) {
        ++m_misses;
        return false;
    }

    out.clear();
    uint8_t buf[8192];
    size_t  n;
    bool    too_big = false;
    while ((n = std::fread(buf, 1, sizeof(buf), f)) > 0) {
        out.insert(out.end(), buf, buf + n);
        if (out.size() > kMaxMediaBytes) {
            too_big = true;
            break;
        }
    }
    std::fclose(f);

    // The check that makes this a content-addressed cache rather than a folder
    // with hex names in it.
    if (too_big || out.empty() || hash_content(out) != hash) {
        struct stat st;
        const size_t was = ::stat(path.c_str(), &st) == 0 ? (size_t)st.st_size : 0;
        std::remove(path.c_str());
        m_bytes_held -= was < m_bytes_held ? was : m_bytes_held;
        if (m_files_held > 0) --m_files_held;
        out.clear();
        ++m_rejected;
        ++m_misses;
        return false;
    }

    ++m_hits;
    m_bytes_served += out.size();
    pin(hash);

    // Used, not merely present. Written to the file's timestamp so the next
    // session's eviction can see it, and only when the one on disk is already
    // stale -- otherwise every read of this cache would be a write.
    {
        struct stat st;
        if (::stat(path.c_str(), &st) == 0) {
            const time_t age = ::time(nullptr) - st.st_mtime;
            if ((double)age > kMediaTouchInterval) {
                utimbuf when;
                when.actime  = ::time(nullptr);
                when.modtime = ::time(nullptr);
                ::utime(path.c_str(), &when);
            }
        }
    }
    return true;
}

bool MediaCache::store(const ContentHash &hash, const std::vector<uint8_t> &bytes,
                       std::string &err)
{
    if (!m_ready) {
        err = "no cache";
        return false;
    }
    if (bytes.empty() || bytes.size() > kMaxMediaBytes) {
        err = "not a media file";
        return false;
    }
    if (hash_content(bytes) != hash) {
        err = "the bytes are not what the hash says they are";
        ++m_rejected;
        return false;
    }

    const std::string path = path_for(hash);
    std::string       ignored;
    make_dirs(path.substr(0, path.rfind('/')), ignored);

    // Written under another name and renamed into place. A client killed
    // mid-write otherwise leaves a short file under a name that says what its
    // contents should be, and every later session reads it, fails the hash
    // check, and downloads it again -- correct, and slow forever.
    const std::string tmp = path + ".part";
    FILE             *f   = std::fopen(tmp.c_str(), "wb");
    if (!f) {
        err = "cannot write " + tmp;
        return false;
    }
    const size_t wrote = std::fwrite(bytes.data(), 1, bytes.size(), f);
    std::fclose(f);
    if (wrote != bytes.size()) {
        std::remove(tmp.c_str());
        err = "short write";
        return false;
    }
    if (std::rename(tmp.c_str(), path.c_str()) != 0) {
        std::remove(tmp.c_str());
        err = "cannot rename into place";
        return false;
    }

    ++m_stores;
    m_bytes_stored += bytes.size();
    m_bytes_held += bytes.size();
    ++m_files_held;
    pin(hash);

    // And back under the line. A tenth below rather than exactly at it, so a
    // world arriving one file at a time does not run a directory walk per file
    // for the rest of its content set.
    if (m_bytes_held > m_budget) evict_to(m_budget - m_budget / 10);
    return true;
}

bool MediaCache::claim(const ContentHash &hash, double now)
{
    if (!m_ready || hash.empty()) return true;   // no cache: everybody fetches
    if (has(hash)) return false;                 // already here, nobody needs to

    const auto it = std::lower_bound(m_claims.begin(), m_claims.end(), hash,
                                     [](const std::pair<ContentHash, double> &e,
                                        const ContentHash &h) { return hash_less(e.first, h); });

    if (it != m_claims.end() && it->first == hash) {
        // Somebody has it. Unless they have had it too long, in which case they
        // are gone and this caller takes over.
        if (now - it->second < kMediaClaimTimeout) return false;
        ++m_claims_taken;
        it->second = now;
        return true;
    }

    m_claims.insert(it, std::make_pair(hash, now));
    return true;
}

void MediaCache::renew(const ContentHash &hash, double now)
{
    if (!m_ready || hash.empty()) return;
    const auto it = std::lower_bound(m_claims.begin(), m_claims.end(), hash,
                                     [](const std::pair<ContentHash, double> &e,
                                        const ContentHash &h) { return hash_less(e.first, h); });
    // Only an existing claim. A caller that never held one does not get one by
    // saying it is still working.
    if (it != m_claims.end() && it->first == hash) it->second = now;
}

void MediaCache::release(const ContentHash &hash)
{
    if (!m_ready) return;
    const auto it = std::lower_bound(m_claims.begin(), m_claims.end(), hash,
                                     [](const std::pair<ContentHash, double> &e,
                                        const ContentHash &h) { return hash_less(e.first, h); });
    if (it != m_claims.end() && it->first == hash) m_claims.erase(it);
}

void MediaCache::reset_counters()
{
    m_hits = m_misses = m_stores = m_rejected = 0;
    m_bytes_stored = m_bytes_served = 0;
    m_evicted_files = 0;
    m_evicted_bytes = 0;
}

} // namespace cnp
