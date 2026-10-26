#include "core/audio/tone.h"

#include <cmath>

namespace cnp {
namespace {

// A small deterministic generator, so "noise" is the same noise everywhere.
// Seeded from the name, so two sounds that differ only in their name are
// different sounds rather than the same hiss twice.
uint32_t seed_of(const std::string &name)
{
    uint32_t h = 2166136261u;
    for (const char c : name) {
        h ^= (uint32_t)(unsigned char)c;
        h *= 16777619u;
    }
    return h ? h : 1u;
}

uint32_t next(uint32_t &s)
{
    s ^= s << 13;
    s ^= s >> 17;
    s ^= s << 5;
    return s;
}

// Rises over the first few milliseconds and falls to nothing at the end.
//
// Not a nicety. A buffer that begins at full amplitude is a step from silence,
// and a step is a click -- the loudest thing in the sound, in every sound, at
// exactly the moment it starts. The gate measures the biggest step between two
// samples for that reason rather than looking at the shape.
float envelope(float t)
{
    constexpr float kAttack = 0.06f;   // of the whole, not of a second

    if (t < kAttack) return t / kAttack;
    const float d = (t - kAttack) / (1.0f - kAttack);
    return (1.0f - d) * (1.0f - d);
}

} // namespace

std::vector<int16_t> bake_sound(const SoundDef &def)
{
    const int count = (int)(def.seconds * (float)kSampleRate);
    if (count <= 1) return {};

    std::vector<int16_t> out;
    out.reserve((size_t)count);

    uint32_t rng   = seed_of(def.name);
    float    phase = 0.0f;

    for (int i = 0; i < count; ++i) {
        const float t = (float)i / (float)(count - 1);

        float v;
        if (def.noise) {
            v = (float)(int32_t)(next(rng) & 0xFFFF) / 32768.0f - 1.0f;
        } else {
            // The phase is accumulated rather than computed from i * hz,
            // because the pitch moves: sin(2 pi f(t) t) sweeps twice as fast as
            // the frequency it is being given, which is audible and wrong.
            const float hz = (def.hz_end > 0.0f) ? def.hz + (def.hz_end - def.hz) * t : def.hz;
            phase += 6.283185307179586f * hz / (float)kSampleRate;
            v = std::sin(phase);
        }

        const float s = v * envelope(t) * (float)kBakePeak;
        out.push_back((int16_t)s);
    }
    // There is no line here forcing the first and last samples to zero. There
    // was, and its sabotage came back green: the envelope is zero at both ends
    // exactly -- t/kAttack at t=0, and (1-d)^2 with d exactly one at the end --
    // so the extra line was a second rule saying what the first already said,
    // and it hid the first one's sabotage while it existed.
    return out;
}

} // namespace cnp
