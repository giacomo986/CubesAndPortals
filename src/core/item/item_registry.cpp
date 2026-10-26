#include "core/item/item_registry.h"

namespace cnp {

namespace {

// The item form of a block. One place, because three of them derived it and one
// of the three left a field out.
//
// The item carries the block's own name and its first face's texture. Not a new
// name like "core:stone_item": the two are the same thing seen from two sides,
// and giving them different names is how a registry ends up needing a table to
// map between them.
ItemDef block_item(const NodeDef &node, BlockId id)
{
    ItemDef d;
    d.name   = node.name;
    d.owner  = node.owner;
    d.places = id;
    d.tex    = node.tex[0];
    return d;
}

} // namespace

ItemRegistry::ItemRegistry()
{
    // Id 0 is nothing, and it is a definition rather than a hole so that
    // get(kNoItem) answers instead of being a case every caller has to know
    // about. It places nothing and stacks to nothing.
    ItemDef nothing;
    nothing.name      = "";
    nothing.max_stack = 0;
    nothing.places    = kAir;

    m_defs.push_back(nothing);
    m_block_of.push_back(kAir);
    m_stale.push_back(0);
    m_orphan.push_back(0);
}

ItemId ItemRegistry::register_item(const ItemDef &def)
{
    auto it = m_by_name.find(def.name);
    if (it != m_by_name.end()) {
        const ItemId id = it->second;
        m_defs[id]      = def;
        m_block_of[id]  = def.places;
        m_stale[id]     = 0;
        m_orphan[id]    = 0;
        return id;
    }

    const ItemId id = (ItemId)m_defs.size();
    m_defs.push_back(def);
    m_block_of.push_back(def.places);
    m_stale.push_back(0);
    m_orphan.push_back(0);
    m_by_name[def.name] = id;
    return id;
}

size_t ItemRegistry::adopt_blocks(const NodeRegistry &nodes)
{
    size_t added = 0;
    m_drops_as.clear();
    m_drops_item.clear();

    for (BlockId id = 1; id < (BlockId)nodes.size(); ++id) {
        const NodeDef &node = nodes.get(id);
        if (node.name.empty()) continue;
        if (nodes.is_orphan(id)) continue;   // a magenta block is nobody's item
        if (node.drops_as != kAir) {         // another block's item, and none of its own
            m_drops_as[id] = node.drops_as;
            continue;
        }
        if (!node.drops_item.empty()) {      // an item that is no block, likewise
            m_drops_item[id] = node.drops_item;
            continue;
        }

        auto known = m_by_name.find(node.name);
        if (known != m_by_name.end()) {
            // The name is taken. Leave it alone unless it is an orphan: then
            // the block it names has come back, and the item comes back with
            // it -- on the same id, so a stack of it that sat in a bag through
            // the dead spell is still that stack.
            const ItemId have = known->second;
            if (!m_orphan[have]) continue;

            // Rebuilt whole, not patched field by field. A revived item has to
            // be what a freshly minted one would be, and the field this used to
            // leave alone is max_stack: a row minted by adopt_stored carries
            // "clamp nothing" (65535) because the mod that knew the real number
            // was gone, and patching around it left core:stone stacking to
            // 65535 after a world file had been read with the mod missing.
            m_defs[have]     = block_item(node, id);
            m_block_of[have] = id;
            m_orphan[have]   = 0;
            ++added;
            continue;
        }

        register_item(block_item(node, id));
        ++added;
    }

    return added;
}

ItemId ItemRegistry::adopt_stored(const std::string &name)
{
    if (name.empty()) return kNoItem;

    auto known = m_by_name.find(name);
    if (known != m_by_name.end()) return known->second;

    const auto renamed = m_alias.find(name);
    if (renamed != m_alias.end()) {
        const ItemId now = find(renamed->second);
        if (now != kNoItem) return now;
    }

    // Owner left empty on purpose. A mod's reload marks its own rows stale by
    // owner, and a row minted here belongs to no mod that is running: guessing
    // the owner from the name would make the next reload of whatever mod that
    // is orphan a row it never registered.
    // How big a stack of it may be is not knowable -- the mod that said so is
    // not here. The maximum means "clamp nothing", which keeps what was stored
    // exactly as it was stored and still lets a player drag the whole stack
    // somewhere. A small guess would move it one at a time; a guess of 64 would
    // silently split a mod's stack of 200.
    ItemDef d;
    d.name      = name;
    d.max_stack = 65535;
    d.places    = kAir;

    const ItemId id = register_item(d);
    m_orphan[id]    = 1;
    return id;
}

ItemId ItemRegistry::of_block(BlockId id) const
{
    if (id == kAir) return kNoItem;
    if (const auto it = m_drops_item.find(id); it != m_drops_item.end()) return find(it->second);
    if (const auto as = m_drops_as.find(id); as != m_drops_as.end()) id = as->second;

    // Linear, and deliberately: this runs when a block breaks, which is a
    // player action and not a hot path, and a second index would be a second
    // thing to keep in step with the first. If breaking ever becomes hot, the
    // index goes in then and this comment is the note about why it was not
    // there first.
    for (size_t i = 1; i < m_block_of.size(); ++i) {
        if (m_block_of[i] == id && !m_orphan[i]) return (ItemId)i;
    }
    return kNoItem;
}

void ItemRegistry::begin_reload(const std::string &owner)
{
    for (size_t i = 0; i < m_defs.size(); ++i) {
        if (m_defs[i].owner == owner) m_stale[i] = 1;
    }
}

void ItemRegistry::cancel_reload(const std::string &owner)
{
    for (size_t i = 0; i < m_defs.size(); ++i) {
        if (m_defs[i].owner == owner) m_stale[i] = 0;
    }
}

size_t ItemRegistry::end_reload(const std::string &owner, const NodeRegistry &nodes)
{
    size_t orphans = 0;

    for (size_t i = 0; i < m_defs.size(); ++i) {
        if (!m_stale[i] || m_defs[i].owner != owner) continue;
        m_stale[i] = 0;

        // A block form is derived, not declared: the mod registers the block
        // and this table mints the item. So "the mod did not register it again"
        // is the wrong question to ask about one -- the mod never registered it
        // in the first place. Ask the blocks instead: while the block is there,
        // the item is real, whatever the mod's item calls did.
        const BlockId back = nodes.find(m_defs[i].name);
        if (back != kAir && !nodes.is_orphan(back)) {
            // Whole, for the reason adopt_blocks gives: a revived item is a
            // minted one, and a field left behind here is a field that keeps
            // whatever a dead row happened to hold.
            m_defs[i]     = block_item(nodes.get(back), back);
            m_block_of[i] = back;
            m_orphan[i]   = 0;
            continue;
        }

        // Already unobtainable: it does not become unobtainable twice, and a
        // count that says it did makes every reload after the first look like
        // it lost something.
        if (m_orphan[i]) continue;

        m_orphan[i] = 1;
        ++orphans;

        // It stops being obtainable and stops placing anything. The id and the
        // name stay: a stack of it may be sitting in somebody's inventory, and
        // renumbering to tidy that up would rewrite every container in the
        // world -- the same bargain NodeRegistry strikes, minus the magenta,
        // because an item nobody can get hold of does not need to be visible
        // from across the map.
        m_defs[i].places = kAir;
        m_block_of[i]    = kAir;
    }

    return orphans;
}

} // namespace cnp
