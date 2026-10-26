#include "client/render/mod_pass.h"

#include <algorithm>

namespace cnp {

bool ModPasses::add(const std::string &name, int order, script_vm *vm, int handler_ref,
                    const std::string &owner, std::string &error)
{
    if (name.empty()) {
        error = "a render pass needs a name";
        return false;
    }

    for (ModPass &p : m_passes) {
        if (p.name != name) continue;
        if (p.owner != owner) {
            error = "'" + name + "' belongs to mod '" + p.owner + "'";
            return false;
        }
        // The same mod in another VM is another world's copy of it (the
        // sandbox's hunger and the Inferno's), each drawn in its own world;
        // a reload's old VM takes its passes with it (ModHost::forget_vm).
        if (p.vm != vm) continue;
        p.order = order;
        p.vm    = vm;
        p.ref   = handler_ref;
        return true;
    }

    ModPass p;
    p.name  = name;
    p.owner = owner;
    p.order = order;
    p.vm    = vm;
    p.ref   = handler_ref;
    p.seq   = m_seq++;
    m_passes.push_back(std::move(p));
    return true;
}

void ModPasses::drop_vm(script_vm *vm)
{
    m_passes.erase(std::remove_if(m_passes.begin(), m_passes.end(),
                                  [vm](const ModPass &p) { return p.vm == vm; }),
                   m_passes.end());
}

std::vector<const ModPass *> ModPasses::in_order() const
{
    std::vector<const ModPass *> out;
    out.reserve(m_passes.size());
    for (const ModPass &p : m_passes) out.push_back(&p);
    std::sort(out.begin(), out.end(), [](const ModPass *a, const ModPass *b) {
        if (a->order != b->order) return a->order < b->order;
        return a->seq < b->seq;
    });
    return out;
}

} // namespace cnp
