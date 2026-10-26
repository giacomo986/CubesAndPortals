#include "core/audio/attenuation.h"

#include <cmath>

namespace cnp {
namespace {

float clamp(float v, float lo, float hi)
{
    return (v < lo) ? lo : (v > hi ? hi : v);
}

} // namespace

float sound_gain(float distance, float range)
{
    // A range of nothing is a sound nobody can hear. Said as a rule rather than
    // left to a division: the alternative is a NaN travelling into a mixer.
    if (!(range > 0.0f)) return 0.0f;
    if (distance <= 0.0f) return 1.0f;
    if (distance >= range) return 0.0f;

    const float t = distance / range;

    // Inverse distance, shifted so it reaches exactly zero AT the range instead
    // of trailing off past it. The shift is not cosmetic: a curve that is still
    // 0.11 at its own range either cuts off audibly or leaves every sound ever
    // played holding a voice for ever. With kRolloff at two this is a quarter
    // of the loudness at half the range, which is the shape of the inverse
    // square law and a sentence somebody can check by ear.
    const float here = 1.0f / (1.0f + kRolloff * t);
    const float edge = 1.0f / (1.0f + kRolloff);
    return clamp((here - edge) / (1.0f - edge), 0.0f, 1.0f);
}

float sound_pan(const Listener &l, Vec3 source)
{
    const Vec3 to = source - l.pos;
    if (length_sq(to) <= 1e-12f) return 0.0f;   // at the ear: not left, not right
    return clamp(dot(normalize(to), l.right), -1.0f, 1.0f);
}

Heard hear(const Listener &l, const Emitter &e)
{
    Heard h;
    h.gain = clamp(e.gain, 0.0f, 1.0f) * sound_gain(length(e.pos - l.pos), e.range);
    h.pan  = (h.gain > 0.0f) ? sound_pan(l, e.pos) : 0.0f;
    return h;
}

Vec3 nearest_on_opening(const PortalSurface &s, Vec3 p)
{
    const Vec3 r = s.right();
    const Vec3 d = p - s.center;

    // Clamped in the plane's own axes, then put back. The component along the
    // normal is dropped on purpose: the nearest point of a rectangle is on the
    // rectangle.
    const float u = clamp(dot(d, r), -s.half_w, s.half_w);
    const float v = clamp(dot(d, s.up), -s.half_h, s.half_h);
    return s.center + r * u + s.up * v;
}

Heard hear_through(const Listener &l, const PortalSurface &here, const PortalSurface &there,
                   const Emitter &e)
{
    const Vec3  mouth = nearest_on_opening(here, l.pos);
    const float leg_a = length(mouth - l.pos);
    const float leg_b = length(e.pos - nearest_on_opening(there, e.pos));

    // The way round. Adding the two legs is what stops a source that happens to
    // share the listener's coordinates -- in another world -- from arriving at
    // full volume out of nowhere.
    Heard h;
    h.gain = clamp(e.gain, 0.0f, 1.0f) * sound_gain(leg_a + leg_b, e.range);

    // And the direction is the opening's. A sound coming through a door on your
    // left is on your left, wherever the room behind it happens to be.
    h.pan = (h.gain > 0.0f) ? sound_pan(l, mouth) : 0.0f;
    return h;
}

} // namespace cnp
