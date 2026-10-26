#include "core/registry/node_registry.h"

namespace cnp {

NodeRegistry::NodeRegistry()
{
    // Air is id 0 and always exists. Every "is there a block here" question in
    // the engine compares against it, and an ungenerated chunk reads as air, so
    // it cannot be something a mod has to remember to define.
    NodeDef air;
    air.name   = "core:air";
    air.solid  = false;
    air.opaque = false;
    register_node(air);
}

BlockId NodeRegistry::register_node(const NodeDef &def)
{
    BlockId id;

    auto it = m_by_name.find(def.name);
    if (it != m_by_name.end()) {
        // Re-registering an existing name replaces the definition and keeps the
        // id. That is what lets a mod be reloaded while a world is loaded: the
        // blocks already placed in it stay the blocks they were.
        id = it->second;
        m_defs[id] = def;
    } else {
        id = (BlockId)m_defs.size();
        m_defs.push_back(def);
        m_by_name.emplace(def.name, id);
    }

    // A node that did not say which textures it wants gets its own id as the
    // layer on every face. The atlas has one tile per node, so this is the
    // identity mapping and the common case costs the registrar nothing.
    bool any_tex = false;
    for (int f = 0; f < kFaceCount; ++f) any_tex = any_tex || (m_defs[id].tex[f] != 0);
    if (!any_tex)
        for (int f = 0; f < kFaceCount; ++f) m_defs[id].tex[f] = id;

    if (m_solid.size() <= id) {
        m_solid.resize((size_t)id + 1, 0);
        m_opaque.resize((size_t)id + 1, 0);
        m_light.resize((size_t)id + 1, 0);
        m_opacity.resize((size_t)id + 1, 255);
        m_liquid.resize((size_t)id + 1, 0);
        m_stale.resize((size_t)id + 1, 0);
        m_orphan.resize((size_t)id + 1, 0);
    }
    m_stale[id]  = 0;   // registered again, so not an orphan of a reload
    m_orphan[id] = 0;   // and if it WAS one, it is a block again
    m_solid[id]  = def.solid ? 1 : 0;
    // A block drawn as boxes does not fill its cell, so it hides nothing --
    // whatever it said about being opaque. One truth and not two: the
    // alternative is every mod remembering to turn opacity off beside every
    // shape it declares, and the one that forgets gets a hole in the world.
    // A block that is not a cube hides nothing: neither a set of boxes that
    // fills part of the cell nor a model that fills whatever it fills.
    m_opaque[id] = (def.opaque && def.is_cube()) ? 1 : 0;
    m_light[id]  = def.light_source;
    m_opacity[id] = def.opacity;
    m_liquid[id]  = def.liquid ? 1 : 0;

    // A block with a front adds nothing here. It used to add three: a name with
    // a ^1, ^2 or ^3 on it, wired to the base by reading that suffix back off
    // the name. The turn is a byte on the CELL since OPEN-27, so a block with a
    // front is one row like any other.
    return id;
}

void NodeRegistry::begin_reload(const std::string &owner)
{
    if (m_stale.size() < m_defs.size()) m_stale.resize(m_defs.size(), 0);
    for (size_t i = 0; i < m_defs.size(); ++i) {
        if (m_defs[i].owner == owner) m_stale[i] = 1;
    }
}

void NodeRegistry::cancel_reload(const std::string &owner)
{
    for (size_t i = 0; i < m_defs.size(); ++i) {
        if (m_defs[i].owner == owner) m_stale[i] = 0;
    }
}

BlockId NodeRegistry::register_unknown(const std::string &name)
{
    NodeDef d;
    d.name      = name;
    d.owner     = "unknown";
    d.colour[0] = 255;
    d.colour[1] = 0;
    d.colour[2] = 255;
    d.checker   = true;
    const BlockId id = register_node(d);
    m_orphan[id]     = 1;
    return id;
}

size_t NodeRegistry::end_reload(const std::string &owner)
{
    size_t orphans = 0;
    if (m_orphan.size() < m_defs.size()) m_orphan.resize(m_defs.size(), 0);

    for (size_t i = 0; i < m_defs.size(); ++i) {
        if (!m_stale[i] || m_defs[i].owner != owner) continue;
        m_stale[i]  = 0;
        m_orphan[i] = 1;
        ++orphans;

        // The id cannot be reclaimed: the world is full of it, and renumbering
        // would rewrite every chunk. What it can be is obvious. Magenta, no
        // checker, still solid so nobody falls through a floor that used to be
        // there -- the same bargain the classic block game and the original engine strike with an unknown
        // node, for the same reason.
        NodeDef &d = m_defs[i];
        d.colour[0] = 255;
        d.colour[1] = 0;
        d.colour[2] = 220;
        d.checker   = false;
        d.solid     = true;
        d.opaque    = true;
        d.light_source = 0;
        d.opacity      = 255;
        d.liquid       = false;

        m_solid[i]  = 1;
        m_opaque[i] = 1;
        m_light[i]  = 0;
        m_opacity[i] = 255;
        m_liquid[i]  = 0;
    }

    return orphans;
}

} // namespace cnp
