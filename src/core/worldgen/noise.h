#pragma once

#include <cstdint>

// The noise the world is built from, in one place.
//
// It was private to worldgen.cpp until biomes needed the same functions to
// decide a climate. Two copies of a hash is two worlds: the copies drift, and
// the symptom is a client and a server that disagree about the ground -- which
// is the one thing the mapgen exists to prevent.

namespace cnp {

// The seed is an argument, never state. A generator that remembers something
// between calls is a generator that produces a different world the second time
// it is asked.
uint32_t noise_hash(int x, int z, uint32_t seed);

// The same hash as a number in 0..1.
float noise_unit(int x, int z, uint32_t seed);

// Value noise on a grid of `period` blocks, bilinear with a smoothstep. Enough
// for a landscape that is not a plane, cheap enough to run per column, and
// continuous -- which is what lets the biome shapes be blended without a seam.
float value_noise(int x, int z, int period, uint32_t seed);

} // namespace cnp
