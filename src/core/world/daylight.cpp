#include "core/world/daylight.h"

#include <cmath>

namespace cnp {
namespace {

float smoothstep01(float t)
{
    return t * t * (3.0f - 2.0f * t);
}

float clamp01(float t)
{
    return (t < 0.0f) ? 0.0f : (t > 1.0f ? 1.0f : t);
}

float mix(float a, float b, float t)
{
    return a + (b - a) * t;
}

// The sun's height above the horizon, -1 at midnight and +1 at noon. Everything
// visible is derived from this one number, so dawn and dusk cannot drift apart
// from each other or from the sky.
double sun_altitude(double t)
{
    constexpr double kTau = 6.283185307179586;
    return -std::cos(kTau * t);
}

// How much of the day the sun spends crossing the horizon, as a band on the
// altitude. Half means the transition runs from altitude -0.5 to +0.5, which is
// a sixth of the day at each end -- about three and a half minutes of a
// twenty-minute day. Narrower and dawn is a light switch.
constexpr double kHorizonBand = 0.5;

// The shape of the day, 0 at night and 1 in full sun, before the night floor is
// added back in. Kept separate because the SKY wants the shape and the sun
// wants the floor: mixing a night sky in by a number that never goes below 0.22
// would mean the night sky is never quite reached.
float day_shape(double t)
{
    const double a = (sun_altitude(t) + kHorizonBand) / (2.0 * kHorizonBand);
    return smoothstep01(clamp01((float)a));
}

} // namespace

double clock_wrap(double t)
{
    t = std::fmod(t, 1.0);
    if (t < 0.0) t += 1.0;

    // fmod of a tiny negative gives back 1.0 exactly after the add, and a time
    // of 1.0 is a time outside the range every caller was promised.
    return (t >= 1.0) ? 0.0 : t;
}

double clock_advance(double t, double dt, double day_length)
{
    if (!(day_length > 0.0)) return clock_wrap(t);
    return clock_wrap(t + dt / day_length);
}

double clock_gap(double from, double to)
{
    double d = clock_wrap(to) - clock_wrap(from);
    if (d > 0.5) d -= 1.0;
    if (d < -0.5) d += 1.0;
    return d;
}

float daylight_at(double t)
{
    return kNightFloor + (1.0f - kNightFloor) * day_shape(t);
}

float sunlight_at(double t)
{
    return kMoonlight + (1.0f - kMoonlight) * day_shape(t);
}

SkyLight sky_light_at(double t)
{
    constexpr double kTau = 6.283185307179586;
    const float      alt  = (float)sun_altitude(t);
    const float      east = (float)std::sin(kTau * t);
    const float      lean = 0.35f;
    const float      len  = std::sqrt(alt * alt + east * east + lean * lean);

    SkyLight l;
    const bool  moon = alt < 0.0f;
    const float sign = moon ? -1.0f : 1.0f;
    l.moon           = moon;
    l.dir[0] = sign * east / len;
    l.dir[1] = sign * alt / len;
    l.dir[2] = lean / len;

    // Up from the horizon over the first tenth of the arc, and warm while low.
    const float up   = smoothstep01(clamp01(std::fabs(alt) / 0.2f));
    const float high = smoothstep01(clamp01(std::fabs(alt) / 0.45f));
    static const float kLow[3]  = {1.00f, 0.62f, 0.38f};
    static const float kHigh[3] = {1.00f, 0.97f, 0.92f};
    static const float kMoon[3] = {0.55f, 0.65f, 1.00f};
    const float        d        = day_shape(t);
    for (int i = 0; i < 3; ++i) {
        l.direct[i]  = moon ? 0.12f * up * kMoon[i] : 0.85f * up * mix(kLow[i], kHigh[i], high);
        l.ambient[i] = 0.32f * sunlight_at(t) * mix(kMoon[i], 1.0f, d);
    }
    return l;
}

void sky_at(double t, float out[3])
{
    const float d = day_shape(t);

    // Warmest halfway through the change and gone at both ends, which is where
    // dawn and dusk are. Derived from the same shape as the light, so the
    // orange cannot end up on the wrong side of sunrise.
    const float horizon = 1.0f - std::fabs(2.0f * d - 1.0f);
    const float warm    = 0.55f * horizon;

    for (int i = 0; i < 3; ++i)
        out[i] = mix(mix(kNightSky[i], kDaySky[i], d), kDuskSky[i], warm);
}

uint16_t clock_to_wire(double t)
{
    const double w = clock_wrap(t) * 65536.0;
    int          i = (int)w;
    if (i < 0) i = 0;
    if (i > 65535) i = 65535;   // a time of exactly 1.0 does not exist, but rounding does
    return (uint16_t)i;
}

double clock_from_wire(uint16_t w)
{
    return (double)w / 65536.0;
}

double tell_interval(double day_length)
{
    // A world with no day at all still says the hour, on the seconds rule: a
    // day length of zero means a sky that never moves, and a client that missed
    // the one telling would sit at the wrong hour for ever.
    const double by_day = kTellEvery * (day_length > 0.0 ? day_length : 0.0);
    if (by_day <= 0.0) return kTellAtLeastEvery;
    return by_day < kTellAtLeastEvery ? by_day : kTellAtLeastEvery;
}

} // namespace cnp
