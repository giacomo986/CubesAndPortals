// Light through stained glass, and what lighting costs.
//
// A lamp behind a wall of red glass lights the far side red, as bright as it
// would through clear glass; the same wall in clear glass leaves it white, and
// so does a red lamp's light that never met a pane. Then the numbers: a column
// of a generated world lit, and a relight round one edit, timed, because the
// filter sits on the inner loop of the flood fill and a feature that made every
// relight slower would be paid for by every world without a pane in it.

#include "core/registry/core_nodes.h"
#include "core/voxel/chunk.h"
#include "core/voxel/light.h"
#include "core/worldgen/worldgen.h"

#include <chrono>
#include <cstdio>

using namespace cnp;

namespace {

int g_failures = 0;

void check(bool cond, const char *what)
{
    std::printf(cond ? "  ok    %s\n" : "  FAIL  %s\n", what);
    if (!cond) ++g_failures;
}

uint8_t lamp_level(const World &w, int x, int y, int z) { return w.get_light(x, y, z) & 15; }

// Reddish: the red channel well above the other two.
bool reddish(uint8_t tint)
{
    return tint_channel(tint, 0) > tint_channel(tint, 1) + 0.3f &&
           tint_channel(tint, 0) > tint_channel(tint, 2) + 0.3f;
}

BlockId pane(NodeRegistry &reg, const char *name, uint8_t r, uint8_t g, uint8_t b, bool tint)
{
    NodeDef d;
    d.name      = name;
    d.colour[0] = r;
    d.colour[1] = g;
    d.colour[2] = b;
    d.opaque    = false;
    d.opacity   = 150;
    d.light_tint = tint ? pack_tint(r, g, b) : kWhiteTint;
    return reg.register_node(d);
}

// A stone floor, a lamp at x 10 and a wall at x 12 tall and wide enough that
// no light gets round it inside the lamp's reach.
void build(World &w, const CoreNodes &core, BlockId wall, BlockId lamp)
{
    for (int z = -20; z <= 40; ++z)
        for (int x = -10; x <= 30; ++x) w.set_block(x, 10, z, core.stone);
    for (int y = 11; y <= 40; ++y)
        for (int z = -10; z <= 30; ++z) w.set_block(12, y, z, wall);
    w.set_block(10, 11, 10, lamp);
}

} // namespace

int main()
{
    NodeRegistry    reg;
    const CoreNodes core = register_core_nodes(reg);

    const BlockId red   = pane(reg, "test:red_glass", 160, 38, 34, true);
    const BlockId clear = pane(reg, "test:clear_glass", 160, 38, 34, false);
    NodeDef       l;
    l.name         = "test:lamp";
    l.light_source = 14;
    l.opaque       = false;
    const BlockId lamp = reg.register_node(l);
    l.name       = "test:red_lamp";
    l.light_tint = pack_tint(255, 0, 0);
    const BlockId red_lamp = reg.register_node(l);

    std::printf("the colour of light through a pane\n");
    {
        const uint8_t r = pack_tint(160, 38, 34);
        check(filter_tint(kWhiteTint, r) == r, "white light through a red pane is the pane's red");
        check(filter_tint(r, kWhiteTint) == r, "and a clear pane changes nothing");
        const uint8_t blue = pack_tint(30, 30, 200);
        const uint8_t both = filter_tint(r, blue);
        check(tint_channel(both, 0) < tint_channel(r, 0) && tint_channel(both, 2) < tint_channel(blue, 2),
              "red through blue keeps less of either");
        check(reg.light_filter(red) == r && reg.light_filter(clear) == kWhiteTint &&
                  reg.light_filter(red_lamp) == kWhiteTint,
              "only a pane that gives no light of its own filters");
        check(!light_unchanged(reg, red, clear), "swapping a red pane for a clear one is a relight");
    }

    std::printf("a lamp behind a wall\n");
    {
        World w;
        build(w, core, red, lamp);
        compute_lighting(w, reg, -10, 0, -20, 30, 50, 40);
        check(lamp_level(w, 14, 11, 10) == 10, "the far side is as bright as through clear glass");
        check(reddish(w.get_tint(14, 11, 10)), "and red");
        check(reddish(w.get_tint(20, 11, 10)), "and stays red as it spreads");
        check(w.get_tint(9, 11, 10) == kWhiteTint, "while the lamp's own side is white");

        World c;
        build(c, core, clear, lamp);
        compute_lighting(c, reg, -10, 0, -20, 30, 50, 40);
        check(lamp_level(c, 14, 11, 10) == 10 && c.get_tint(14, 11, 10) == kWhiteTint,
              "clear glass lets the same light through, white");

        // A relight after an edit is the same pass, so it agrees.
        c.set_block(12, 20, 10, red);
        relight_around(c, reg, 12, 20, 10);
        c.set_block(12, 20, 10, clear);
        relight_around(c, reg, 12, 20, 10);
        check(c.get_tint(14, 11, 10) == kWhiteTint, "and a pane put in and taken out leaves no stain");

        World r;
        build(r, core, red, red_lamp);
        compute_lighting(r, reg, -10, 0, -20, 30, 50, 40);
        check(reddish(r.get_tint(9, 11, 10)) && reddish(r.get_tint(14, 11, 10)),
              "a red lamp is red on both sides");
    }

    std::printf("what it costs\n");
    {
        World         w;
        NodeRegistry  greg;
        register_core_nodes(greg);
        constexpr int kExtent = 128;
        Mapgen        mapgen;
        mapgen.generate(w, greg, 1, kExtent, kWorldHeight);

        using clock = std::chrono::steady_clock;
        auto t0     = clock::now();
        int  n      = 0;
        for (int cz = 1; cz < kExtent / kChunkSize - 1; ++cz)
            for (int cx = 1; cx < kExtent / kChunkSize - 1; ++cx, ++n)
                compute_column_lighting(w, greg, cx, cz, kWorldHeight);
        const double column = std::chrono::duration<double, std::milli>(clock::now() - t0).count() / n;

        t0 = clock::now();
        constexpr int kEdits = 200;
        for (int i = 0; i < kEdits; ++i) relight_around(w, greg, 40 + i % 40, 60, 40 + i / 40 * 8);
        const double edit = std::chrono::duration<double, std::milli>(clock::now() - t0).count() / kEdits;
        std::printf("        a column lit in %.2f ms, a relight round one edit in %.3f ms\n", column, edit);
    }

    if (g_failures != 0) {
        std::printf("\n%d failed\n", g_failures);
        return 1;
    }
    std::printf("\nall passed\n");
    return 0;
}
