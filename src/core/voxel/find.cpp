#include "core/voxel/find.h"

#include <algorithm>
#include <vector>

namespace cnp {

int find_nodes(const World &world, const NodeRegistry &registry, int x1, int y1, int z1, int x2,
               int y2, int z2, const char *name, int *out, int max, std::string &why)
{
    if (!out || max < 3) return 0;

    // A name ending in '*' is every block whose name starts with what comes
    // before it: a block drawn in many shapes ("cinnabar:dust_0121", ...) is one
    // search and one pass over the box, not one per shape.
    const std::string asked = name ? name : "";
    std::vector<bool> wanted(registry.size(), false);
    bool any = false;
    if (!asked.empty() && asked.back() == '*') {
        const std::string prefix = asked.substr(0, asked.size() - 1);
        for (size_t id = 1; id < registry.size(); ++id)
            if (registry.get((BlockId)id).name.compare(0, prefix.size(), prefix) == 0)
                wanted[id] = any = true;
    } else {
        const BlockId want = registry.find(asked);
        if (want != kAir) wanted[want] = any = true;
    }
    if (!any) {
        // Air included: a mod searching for air over a box is asking for
        // everything that is not there, which is a different call and not this
        // one. Refusing it here is cheaper than the answer.
        why = "no block is called '" + asked + "'";
        return 0;
    }

    const int lox = std::min(x1, x2), hix = std::max(x1, x2);
    const int loy = std::min(y1, y2), hiy = std::max(y1, y2);
    const int loz = std::min(z1, z2), hiz = std::max(z1, z2);

    const long long cells = (long long)(hix - lox + 1) * (long long)(hiy - loy + 1) *
                            (long long)(hiz - loz + 1);
    if (cells > kFindNodesMaxCells) {
        why = "that box is " + std::to_string(cells) + " cells, more than the " +
              std::to_string(kFindNodesMaxCells) + " one search may look at";
        return 0;
    }

    int written = 0;
    for (int y = loy; y <= hiy; ++y)
        for (int z = loz; z <= hiz; ++z)
            for (int x = lox; x <= hix; ++x) {
                // One comparison, because a block with a front is one block:
                // a cell a player put down facing east holds the same id as one
                // facing north, and the turn is a byte beside it. This used to
                // have to go through base_of, and a mod that forgot would have
                // been right to conclude nobody had placed any.
                //
                // What went with the ids is searching for an ORIENTATION -- the
                // old "mymod:forno^2" finds only that turn. Nothing asked for
                // it; when something does, it is a facing argument here and not
                // four names.
                const BlockId id = world.get_block(x, y, z);
                if (id >= wanted.size() || !wanted[id]) continue;
                if (written + 3 > max) return written;   // full: the caller is told by the count
                out[written++] = x;
                out[written++] = y;
                out[written++] = z;
            }
    return written;
}

} // namespace cnp
