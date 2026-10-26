#include "core/item/container.h"

#include <string>

namespace cnp {

Inventory *ContainerSet::open(const ContainerAt &at, int slots, std::string &err)
{
    if (slots <= 0 || slots > kMaxContainerSlots) {
        err = "a container holds between 1 and " + std::to_string(kMaxContainerSlots) +
              " slots, and that is not one of them";
        return nullptr;
    }

    const auto it = m_map.find(at);
    if (it != m_map.end()) {
        if (slots > it->second.bag.size()) {
            it->second.bag.resize(slots);
            ++it->second.rev;
        }
        return with_roles(at, &it->second.bag);
    }
    if (m_map.size() >= kMaxContainers) {
        err = "this world already holds as many containers as it may";
        return nullptr;
    }

    Inventory *bag = &m_map.emplace(at, Held(slots)).first->second.bag;
    bag->carried(at.y <= kCarriedBelow);
    return with_roles(at, bag);
}

Inventory *ContainerSet::with_roles(const ContainerAt &at, Inventory *bag)
{
    if (m_roles_of) {
        const SlotRules *r = m_roles_of(at);
        if (r != nullptr && !(*r == bag->rules())) bag->set_rules(*r);
    }
    return bag;
}

Inventory *ContainerSet::find(const ContainerAt &at)
{
    const auto it = m_map.find(at);
    return it == m_map.end() ? nullptr : with_roles(at, &it->second.bag);
}

const Inventory *ContainerSet::find(const ContainerAt &at) const
{
    const auto it = m_map.find(at);
    return it == m_map.end() ? nullptr : &it->second.bag;
}

uint64_t ContainerSet::revision(const ContainerAt &at) const
{
    const auto it = m_map.find(at);
    return it == m_map.end() ? 0 : it->second.rev;
}

void ContainerSet::touched(const ContainerAt &at)
{
    const auto it = m_map.find(at);
    if (it != m_map.end()) ++it->second.rev;
}

bool ContainerSet::erase(const ContainerAt &at)
{
    return m_map.erase(at) > 0;
}

bool container_in_reach(const ContainerAt &at, float eye_x, float eye_y, float eye_z, float reach)
{
    if (at.y <= kCarriedBelow) return true;   // carried: wherever its player is
    const float dx = ((float)at.x + 0.5f) - eye_x;
    const float dy = ((float)at.y + 0.5f) - eye_y;
    const float dz = ((float)at.z + 0.5f) - eye_z;

    // Squared, so this never takes a square root to answer a comparison.
    return dx * dx + dy * dy + dz * dz <= reach * reach;
}

std::vector<ContainerAt> ContainerSet::cells() const
{
    std::vector<ContainerAt> out;
    out.reserve(m_map.size());
    for (const auto &kv : m_map) out.push_back(kv.first);
    return out;
}

void ContainerSet::set_progress(const ContainerAt &at, double since, float secs)
{
    const auto it = m_map.find(at);
    if (it == m_map.end()) return;
    it->second.prog_since = since;
    it->second.prog_secs  = secs > 0.0f ? secs : 0.0f;
    ++it->second.rev;
}

bool ContainerSet::progress(const ContainerAt &at, double now, float &done, float &rate) const
{
    const auto it = m_map.find(at);
    if (it == m_map.end() || it->second.prog_secs <= 0.0f) return false;
    const double t = (now - it->second.prog_since) / (double)it->second.prog_secs;
    done = (float)(t < 0.0 ? 0.0 : t > 1.0 ? 1.0 : t);
    rate = 1.0f / it->second.prog_secs;
    return true;
}

void ContainerSet::set_scroll(const ContainerAt &at, float pos, float size)
{
    const auto it = m_map.find(at);
    if (it == m_map.end()) return;
    it->second.scroll_pos  = pos < 0.0f ? 0.0f : pos > 1.0f ? 1.0f : pos;
    it->second.scroll_size = size < 0.0f ? 0.0f : size > 1.0f ? 1.0f : size;
    ++it->second.rev;
}

bool ContainerSet::scroll(const ContainerAt &at, float &pos, float &size) const
{
    const auto it = m_map.find(at);
    if (it == m_map.end() || it->second.scroll_size <= 0.0f) return false;
    pos  = it->second.scroll_pos;
    size = it->second.scroll_size;
    return true;
}

void ContainerSet::set_search(const ContainerAt &at, const std::string &text)
{
    const auto it = m_map.find(at);
    if (it == m_map.end()) return;
    it->second.searchable = true;
    it->second.query      = text;
    ++it->second.rev;
}

bool ContainerSet::search(const ContainerAt &at, std::string &text) const
{
    const auto it = m_map.find(at);
    if (it == m_map.end() || !it->second.searchable) return false;
    text = it->second.query;
    return true;
}

} // namespace cnp
