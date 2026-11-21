#pragma once

#include <cstdint>

namespace cnp {

// The colour of block light, one byte a cell (3-3-2 bits). Every channel runs
// from 0.3 to 1: light is paler than the thing that gives it, so no colour
// leaves a surface black, and three bits are enough for what is left.
inline constexpr uint8_t kWhiteTint = 0xFF;

inline uint8_t pack_tint(uint8_t r, uint8_t g, uint8_t b)
{
    const int top = r > g ? (r > b ? r : b) : (g > b ? g : b);
    if (top == 0) return kWhiteTint;
    // Brightest channel to 1, then 30% of the way to white is already there.
    const auto bits = [top](int c, int steps) { return (c * steps + top / 2) / top; };
    return (uint8_t)(bits(r, 7) << 5 | bits(g, 7) << 2 | bits(b, 3));
}

inline float tint_channel(uint8_t tint, int k)
{
    const float v = k == 0 ? (float)(tint >> 5) / 7.0f : k == 1 ? (float)(tint >> 2 & 7) / 7.0f : (float)(tint & 3) / 3.0f;
    return 0.3f + 0.7f * v;
}

// Halfway between two: where two lights of the same strength meet.
inline uint8_t mix_tint(uint8_t a, uint8_t b)
{
    return (uint8_t)((((a >> 5) + (b >> 5) + 1) / 2) << 5 | (((a >> 2 & 7) + (b >> 2 & 7) + 1) / 2) << 2 |
                     (((a & 3) + (b & 3) + 1) / 2));
}

// Light through a coloured pane: each channel of the light times the pane's,
// the way a red window lets only the red through. In the same 0.3..1 scale,
// so a channel the pane takes most of comes out at the floor and not at a
// black no lamp could make. Two panes in a row take twice.
inline uint8_t filter_tint(uint8_t light, uint8_t pane)
{
    if (pane == kWhiteTint) return light;
    const auto through = [&](int k, int steps) {
        const float v = (tint_channel(light, k) * tint_channel(pane, k) - 0.3f) / 0.7f;
        const int   q = (int)(v * (float)steps + 0.5f);
        return q < 0 ? 0 : q > steps ? steps : q;
    };
    return (uint8_t)(through(0, 7) << 5 | through(1, 7) << 2 | through(2, 3));
}

} // namespace cnp
