#pragma once

#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace cnp {

inline constexpr size_t kMaxStoreKeyBytes   = 64;
inline constexpr size_t kMaxStoreValueBytes = 4096;
// What one mod may keep in all, its keys and values counted together. Bytes and
// not keys: a thousand small keys -- a chest each, a player's best lap each --
// are a smaller file than ten full values, and a count of keys refused the
// first while allowing the second (it was 1024 keys until 2026-09-28).
inline constexpr size_t kMaxStoreBytesPerMod = 4u << 20;

// What mods keep across a restart (core.storage_set): strings under names, each
// mod's its own. A server keeps it in the world file (WorldDb::save_mod_store);
// the single-process client, which saves nothing, for as long as it runs.
//
// Limits for the reason node_meta has them: a mod is a stranger's code, and a
// mod writing a new key every tick would otherwise grow the file without end.
// That one stops at kMaxStoreBytesPerMod, told so in the log.
class ModStore {
public:
    struct Row {
        std::string mod, key, value;
    };

    // An empty value erases, as core.meta_set does: reading a key nobody wrote
    // gives back the same empty string.
    bool set(const std::string &mod, const std::string &key, const std::string &value,
             std::string &why)
    {
        if (key.empty() || key.size() > kMaxStoreKeyBytes) {
            why = "a key is 1 to " + std::to_string(kMaxStoreKeyBytes) + " bytes";
            return false;
        }
        if (value.size() > kMaxStoreValueBytes) {
            why = "a value is at most " + std::to_string(kMaxStoreValueBytes) + " bytes";
            return false;
        }
        std::map<std::string, std::string> &mine  = m_mods[mod];
        size_t                             &bytes = m_bytes[mod];
        const auto                          was   = mine.find(key);
        const size_t                        before = was == mine.end() ? 0 : key.size() + was->second.size();
        if (value.empty()) {
            if (was == mine.end()) return true;
            mine.erase(was);
            bytes -= before;
            m_touched.emplace(mod, key);
            return true;
        }
        if (was != mine.end() && was->second == value) return true;
        const size_t after = bytes - before + key.size() + value.size();
        if (after > kMaxStoreBytesPerMod) {
            why = "a mod keeps at most " + std::to_string(kMaxStoreBytesPerMod) +
                  " bytes of keys and values";
            return false;
        }
        bytes = after;
        mine[key] = value;
        m_touched.emplace(mod, key);
        return true;
    }

    // What changed since the last call, each key with its value now -- empty
    // for one that was erased -- so a save writes those rows and not every row.
    // A mod bumping one counter every tick costs one row a save.
    std::vector<Row> take_touched()
    {
        std::vector<Row> out;
        for (const auto &mk : m_touched)
            out.push_back(Row{mk.first, mk.second, get(mk.first, mk.second)});
        m_touched.clear();
        return out;
    }

    // A save that failed: owed again, or the change is never written.
    void touch(const Row &r) { m_touched.emplace(r.mod, r.key); }

    const std::string &get(const std::string &mod, const std::string &key) const
    {
        static const std::string nothing;
        const auto m = m_mods.find(mod);
        if (m == m_mods.end()) return nothing;
        const auto k = m->second.find(key);
        return k == m->second.end() ? nothing : k->second;
    }

private:
    std::map<std::string, std::map<std::string, std::string>> m_mods;
    std::map<std::string, size_t>                             m_bytes;   // each mod's keys and values
    std::set<std::pair<std::string, std::string>>             m_touched;
};

} // namespace cnp
