#pragma once

#include <cstdint>

// The clock a world keeps, and what the sky and the sun do with it
// (DESIGN.md 8, M7: "ciclo giorno/notte").
//
// In core rather than in the client for the usual reason: the server owns the
// time and the client draws it, so the two have to agree about what 0.31 means
// down to the colour on the screen. A copy of this arithmetic on each side is
// two clocks, and the symptom of two clocks is a player standing in daylight
// telling another player it is night.
//
// The renderer has been ready for this since M1 -- `uDaylight` in shaders.h
// scales the sun channel and leaves block light alone, and takes the max of the
// two so a torch never blows out a lit surface. Nothing ever told it what time
// it was.

namespace cnp {

// A day, in seconds of wall clock. The classic block game's twenty minutes: long enough that
// noon is not a flicker, short enough that somebody who wants to see the night
// does not have to book the afternoon.
inline constexpr double kDayLength = 1200.0;

// Time of day is a fraction of a day, always in [0, 1):
//   0.00 midnight    0.25 dawn    0.50 noon    0.75 dusk
inline constexpr double kMidnight = 0.0;
inline constexpr double kDawn     = 0.25;
inline constexpr double kNoon     = 0.5;
inline constexpr double kDusk     = 0.75;

// How much sun the darkest night keeps. Not zero, and not a taste: a player who
// cannot see cannot play, and a world that goes black once every twenty minutes
// is a world that gets closed. Torches and lava still read as bright against it
// because block light does not dim at all.
inline constexpr float kNightFloor = 0.22f;

// The same for what the eye sees, which is not what the game counts: a night
// that is nearly black outdoors, with torches the only real light. Rules that
// ask "is it dark here" (spawning, core.light) keep daylight_at.
inline constexpr float kMoonlight = 0.05f;

// How fast the wind carries the clouds, in nodes a second. The server keeps how
// far it has carried them and tells it with the time; a client runs it on at
// this speed between tellings, so every player sees the same clouds moving
// smoothly rather than one sky each.
inline constexpr double kWindSpeed = 1.2;

// The three skies the cycle mixes. Here rather than in the renderer because the
// client's kSkyColour is defined from them: one definition, so a world at noon
// and a world that never learned about the clock are the same blue.
inline constexpr float kDaySky[3]   = {0.45f, 0.62f, 0.85f};
inline constexpr float kNightSky[3] = {0.04f, 0.05f, 0.11f};
inline constexpr float kDuskSky[3]  = {0.82f, 0.42f, 0.24f};

// Brings any time back into [0, 1). Every function here promises to return a
// time in that range, so nothing downstream has to ask.
double clock_wrap(double t);

// Moves a clock on by `dt` seconds.
double clock_advance(double t, double dt, double day_length = kDayLength);

// Signed distance from `from` to `to` the short way around the circle, in
// [-0.5, 0.5]. This is the whole reason a time is not a number: 0.99 and 0.01
// are two hundredths apart, and a subtraction says ninety-eight.
double clock_gap(double from, double to);

// How much a world's clock may move before the people in it are told again.
//
// A fraction of a DAY and not a number of seconds, so it scales with the day:
// a server running a twenty-second day changes its sky sixty times faster and
// tells people sixty times as often, without a second rule saying so.
//
// This is also why there is no clock on the client side and nothing to
// interpolate. A client holds the last time it was told until the next one, and
// the whole cost of that is measured in the gate: across one of these intervals
// the sun changes by well under a hundredth of its range, which is less than
// the step between two of the sixteen light levels a block can have. A second
// clock running on the client to smooth over something invisible would be a
// second clock to disagree with.
inline constexpr double kTellEvery = 1.0 / 1200.0;

// And never less often than this many SECONDS, whatever the day length is.
//
// kTellEvery bounds how stale the sky may get; this bounds how long a LOST
// telling stays lost. The message goes out unreliable -- a retransmitted hour
// arrives already wrong, and sent reliably it would leave the server owing
// every session an answer once a second for ever, which is what killed the
// silence rule the first time (see net::quiet_for) -- so what repairs a loss is
// the next telling, not a retransmission.
//
// On the 1200 s day the two rules agree to the second. On a day of eleven days
// kTellEvery alone would leave a lost hour lost for fourteen minutes, and the
// hour a joining client is told is exactly the one there is no second chance
// at. A cap and not a replacement: a server running a short day still tells
// people faster than this, because its sky moves faster.
inline constexpr double kTellAtLeastEvery = 1.0;

// The two rules above, as one number: how many seconds a world waits before it
// says the hour again. The smaller of "a fraction of this world's day" and "one
// second", so a short day is told faster and a long one is still told often
// enough for a lost telling to be repaired.
double tell_interval(double day_length);

// How much of the sun reaches the ground, kNightFloor..1.
float daylight_at(double t);

// The sky at that time, into `out[3]`.
void sky_at(double t, float out[3]);

// How bright the sky's light is on the ground, for drawing: kMoonlight at
// midnight, 1 at noon.
float sunlight_at(double t);

// The light that casts shadows, and the light that does not, for drawing.
//
// The sun rises in +x, stands over the world at noon and sets in -x, leaning a
// little towards +z all the way so that noon's shadows are not straight down
// the side of every block. By night it is the moon, opposite, weak and blue.
// Either one gives no direct light while it sits on the horizon, which is when
// the other takes over.
struct SkyLight {
    float dir[3];       // towards the light, unit length
    float direct[3];    // its colour times its strength, on a face looking straight at it
    float ambient[3];   // the sky's own light, on a face looking up
    bool  moon;         // whether dir is the moon's
};
SkyLight sky_light_at(double t);

// Sixteen bits, which is a minute of a twenty-minute day divided into fifty --
// finer than anything the eye follows, and two bytes on the wire.
uint16_t clock_to_wire(double t);
double   clock_from_wire(uint16_t w);

} // namespace cnp
