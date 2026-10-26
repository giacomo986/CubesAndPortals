#include "core/world/node_meta.h"

namespace cnp {

bool NodeMetaSet::set(const Cell &at, const std::string &key, const std::string &value,
                      std::string &why)
{
    if (key.empty() || key.size() > kMaxMetaKeyBytes) {
        why = "a field name must be 1 to " + std::to_string(kMaxMetaKeyBytes) + " bytes";
        return false;
    }
    if (value.size() > kMaxMetaValueBytes) {
        why = "a field is at most " + std::to_string(kMaxMetaValueBytes) + " bytes";
        return false;
    }

    auto it = m_cells.find(at);

    // Erasing. Before the limits, and that is deliberate: a world at its cell
    // ceiling has to be able to get back under it, and a mod cleaning up after
    // itself must not be the one call that is refused.
    if (value.empty()) {
        if (it == m_cells.end()) return true;
        it->second.erase(key);
        if (it->second.empty()) m_cells.erase(it);
        return true;
    }

    if (it == m_cells.end()) {
        if (m_cells.size() >= kMaxMetaCells) {
            why = "this world already remembers " + std::to_string(kMaxMetaCells) + " cells";
            return false;
        }
        it = m_cells.emplace(at, Fields{}).first;
    }

    // Replacing a field that is already there is not a new field, so a cell at
    // its ceiling can still be written to.
    if (it->second.size() >= kMaxMetaKeysPerCell && it->second.find(key) == it->second.end()) {
        why = "a cell remembers at most " + std::to_string(kMaxMetaKeysPerCell) + " things";
        if (it->second.empty()) m_cells.erase(it);
        return false;
    }

    it->second[key] = value;
    return true;
}

std::string NodeMetaSet::get(const Cell &at, const std::string &key) const
{
    const auto it = m_cells.find(at);
    if (it == m_cells.end()) return std::string();
    const auto f = it->second.find(key);
    return (f == it->second.end()) ? std::string() : f->second;
}

bool NodeMetaSet::clear(const Cell &at)
{
    return m_cells.erase(at) != 0;
}

size_t NodeMetaSet::fields() const
{
    size_t n = 0;
    for (const auto &c : m_cells) n += c.second.size();
    return n;
}

std::vector<NodeMetaSet::Field> NodeMetaSet::all() const
{
    std::vector<Field> out;
    out.reserve(fields());
    for (const auto &c : m_cells)
        for (const auto &f : c.second) out.push_back(Field{c.first, f.first, f.second});
    return out;
}

size_t NodeMetaSet::load(const std::vector<Field> &fields)
{
    size_t      took = 0;
    std::string why;
    for (const Field &f : fields)
        if (set(f.at, f.key, f.value, why)) ++took;
    return took;
}

} // namespace cnp
