#pragma once

#include "core/registry/node_registry.h"

#include <cstdint>
#include <string>
#include <vector>

// Biomes (DESIGN.md 8, M7: "mapgen con biomi").
//
// A biome here is a *climate*, not a place. It is a point in a two-axis space --
// temperature and humidity, both 0..1 -- plus what the ground is made of there
// and what shape it takes. Two noise fields give every column a climate; the
// nearest biome in that space owns the column.
//
// Nearest-point rather than a table of rectangles, for one reason: a rectangle
// table has holes. Some climate falls in no rectangle, and that column has no
// surface block at all -- a hole in the world produced by a gap in a table
// somebody wrote by hand. Every point has a nearest neighbour, so with this
// there is nothing to forget.
//
// The blocks are chosen; the SHAPE is mixed. Which biome owns a column is a
// step function, and a step in the surface block is exactly right -- grass
// stops and sand starts, and you can see the edge. A step in the ground HEIGHT
// is a cliff you fall off, invented by a table rather than by the terrain. So
// the height parameters of every biome are blended by distance in climate
// space, which is continuous because the climate itself is: no seam can appear
// in the ground that was not already in the noise.

namespace cnp {

struct BiomeDef {
    std::string name;   // namespaced, like a node: "core:desert"

    // Where it sits in climate space. Nothing stops two biomes sharing a point;
    // the one that wins is then decided by name, so that at least it is decided
    // the same way on every machine.
    float temperature = 0.5f;
    float humidity    = 0.5f;

    std::string top;            // the block at the surface
    std::string filler;         // the few blocks under it, and what scatter builds with
    int         filler_depth = 3;

    // How the ground sits and how much it moves. `base` lifts the whole biome
    // off the plaza height, `relief` multiplies the terrain noise. Both are
    // blended between biomes rather than picked -- see the header comment.
    float base   = 0.0f;
    float relief = 1.0f;

    // False for the biomes of water and shore (core:ocean, core:beach...): they
    // are not a climate but where the ground lies against the sea level, so
    // pick() and shape() leave them out and mapgen_biome_at hands them out.
    bool climate = true;
};

// The blended height parameters at a column: no biome's own numbers, but the
// mix that keeps the ground continuous across a border.
struct BiomeShape {
    float base   = 0.0f;
    float relief = 1.0f;
};

class BiomeSet {
public:
    // Registers the engine's four climates and its six waters, shores and gorges.
    // There is no empty BiomeSet: a set with
    // nothing in it would mean a column with no surface, and the terrain stage
    // would need a second code path to survive it.
    BiomeSet();

    void add(const BiomeDef &def);

    // Which biome owns this climate. Ties break on the name, so the answer does
    // not depend on the order biomes were registered in -- unlike mapgen
    // stages, where the order IS the contract.
    size_t pick(float temperature, float humidity) const;

    // The mix at this climate, by inverse fourth power of the distance to each
    // biome's point. Sharp enough that a biome's middle is nearly its own
    // numbers, smooth enough that no border is a step.
    BiomeShape shape(float temperature, float humidity) const;

    size_t          size() const { return m_biomes.size(); }
    const BiomeDef &at(size_t i) const { return m_biomes[i < m_biomes.size() ? i : 0]; }

    // Index of a biome by name, or size() if there is none.
    size_t find(const std::string &name) const;

    // Whether the world has the flat plaza at x,z 20..100 that the portal
    // gates stand on. Kept here because the ground is a function of the set
    // and the seed and nothing else, and this is the one thing that changes
    // it: a survival world (--survival) has no plaza and spawns somewhere on
    // the land instead (spawn_on_land).
    bool plaza = true;

private:
    std::vector<BiomeDef> m_biomes;
};

// The two climate fields, 0..1. Wide and slow on purpose: a biome you cross in
// four steps is a stain, not a place. Since 2026-10-10 a biome is some two
// hundred blocks across, its edge bent by a second, finer octave.
float biome_temperature(int x, int z, uint32_t seed);
float biome_humidity(int x, int z, uint32_t seed);

} // namespace cnp
