#pragma once

#include "core/registry/node_registry.h"

#include <string>
#include <vector>

// The engine's own blocks, registered in one place.
//
// This exists because of a specific hazard, not for tidiness. Ids are assigned
// in registration order (`node_registry.cpp`: `id = m_defs.size()`), and the
// world hash a joining client checks is computed over ids. So the sentence
// "the server and the client agree about what block 3 is" was, until this file,
// a contract signed in four separate copies -- the client's main, the server's
// main, and two tests -- by nobody in particular. Add a core node to one of
// them and every id after it shifts, on that side only.
//
// The visible symptom would have been a false desync at join. The real one
// arrives with chunk streaming, where a chunk on the wire is an array of ids:
// the server means core:water, the client draws core:bedrock, and nothing
// anywhere reports an error.
//
// This is the same argument as one mapgen instead of two, applied to the block
// table. It is not the whole fix -- that is a name-to-id table in the welcome,
// which arrives with chunk streaming because chunk streaming cannot work
// without it. It is the half that removes the duplication rather than
// documenting it.

namespace cnp {

// Ids of the engine's own blocks. Filled in by register_core_nodes, because
// nothing is allowed to assume them -- including this header.
struct CoreNodes {
    BlockId stone   = kAir;
    BlockId grass   = kAir;
    BlockId bedrock = kAir;
    BlockId frame   = kAir;
    BlockId glass   = kAir;

    // Added for the biomes, and added at the END of the list on purpose: ids
    // are positions, so inserting one in the middle renumbers every block after
    // it and every world already saved with the old numbers.
    BlockId dirt    = kAir;
    BlockId sand    = kAir;
    BlockId snow    = kAir;
};

// Registers them, in the order that is the contract. Call it once, before any
// mod, on every side that has a registry.
CoreNodes register_core_nodes(NodeRegistry &reg);

// The same names in the same order, for a test that wants to check the order
// rather than trust it.
const std::vector<std::string> &core_node_names();

// One number for "what this process thinks the block table is": every name, in
// id order. Two processes with the same number have the same table, including
// the mods that added to it; two with different numbers disagree somewhere, and
// the point of a number this small is that it fits in a handshake.
//
// This is what a version check is actually for. "Both sides are version 0.3.0"
// is a claim about a tag; this is a claim about the thing that breaks. A client
// built before a core node was inserted and a server built after it have the
// same version string and different tables, and it is the table that decides
// what block 3 is.
uint64_t registry_content_hash(const NodeRegistry &reg);

// Every node name in id order. This is the block table as a thing that can be
// sent: the ids are the positions, so no pairs and no numbers on the wire.
std::vector<std::string> node_table(const NodeRegistry &reg);

// Rebuilds `dst` so that it has exactly the table `names` describes, with the
// same ids. The definition for a name the local build already knows is taken
// from `local` -- its colours, whether it is solid, whether it breaks. A name
// it has never heard of gets a placeholder: visible, solid, and obviously
// wrong, which is the same bargain the registry already makes for a block whose
// mod was removed.
//
// This is how a client joins a server whose content it does not have. Adopting
// the server's ids beats translating them on every chunk: a chunk is 32768 ids
// and the translation would run on all of them.
//
// Returns how many names were placeholders, which the caller reports rather
// than swallows -- a player should know they are looking at blocks their build
// does not understand.
size_t adopt_node_table(NodeRegistry &dst, const std::vector<std::string> &names,
                        const NodeRegistry &local);

} // namespace cnp
