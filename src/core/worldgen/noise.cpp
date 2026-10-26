#include "core/worldgen/noise.h"

namespace cnp {
namespace {

float smoothstep01(float t)
{
    return t * t * (3.0f - 2.0f * t);
}

} // namespace

uint32_t noise_hash(int x, int z, uint32_t seed)
{
    // The multiplies happen in unsigned arithmetic. Doing them in int overflows
    // for any x past five, and signed overflow is undefined -- the optimiser is
    // entitled to assume it cannot happen and conclude whatever follows from
    // that. That produced terrain thousands of nodes above and below the world,
    // and it survived a whole milestone before anyone noticed.
    const uint32_t ux = (uint32_t)x;
    const uint32_t uz = (uint32_t)z;
    uint32_t h = (ux * 374761393u) ^ (uz * 668265263u) ^ (seed * 2246822519u);
    h = (h ^ (h >> 13)) * 1274126177u;
    return h ^ (h >> 16);
}

float noise_unit(int x, int z, uint32_t seed)
{
    return (float)(noise_hash(x, z, seed) & 0xFFFFFF) / (float)0xFFFFFF;
}

float value_noise(int x, int z, int period, uint32_t seed)
{
    const int   gx = (x >= 0) ? x / period : (x - period + 1) / period;
    const int   gz = (z >= 0) ? z / period : (z - period + 1) / period;
    const float fx = (float)(x - gx * period) / (float)period;
    const float fz = (float)(z - gz * period) / (float)period;

    const float n00 = noise_unit(gx, gz, seed);
    const float n10 = noise_unit(gx + 1, gz, seed);
    const float n01 = noise_unit(gx, gz + 1, seed);
    const float n11 = noise_unit(gx + 1, gz + 1, seed);

    const float sx = smoothstep01(fx);
    const float sz = smoothstep01(fz);
    const float a  = n00 + (n10 - n00) * sx;
    const float b  = n01 + (n11 - n01) * sx;
    return a + (b - a) * sz;
}

} // namespace cnp
