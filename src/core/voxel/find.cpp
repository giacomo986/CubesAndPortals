#include "core/voxel/find.h"

#include <algorithm>

namespace cnp {

int find_nodes(const World &world, const NodeRegistry &registry, int x1, int y1, int z1, int x2,
               int y2, int z2, const char *name, int *out, int max, std::string &why)
{
    if (!out || max < 3) return 0;

    const BlockId want = registry.find(name ? name : "");
    if (want == kAir) {
        // Air included: a mod searching for air over a box is asking for
        // everything that is not there, which is a different call and not this
        // one. Refusing it here is cheaper than the answer.
        why = std::string("no block is called '") + (name ? name : "") + "'";
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
                if (world.get_block(x, y, z) != want) continue;
                if (written + 3 > max) return written;   // full: the caller is told by the count
                out[written++] = x;
                out[written++] = y;
                out[written++] = z;
            }
    return written;
}

} // namespace cnp
