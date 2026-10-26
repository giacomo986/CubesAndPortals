#pragma once

#include "core/math/vec.h"
#include "core/portal/portal_math.h"

// How loud a sound is from where you are standing (DESIGN.md 8, M7: "audio con
// attenuazione").
//
// In core, and with no reference to any audio library, because this is
// arithmetic and not playback: the client's snd_* backend takes a gain and a
// pan and does what it is told. The same split as gfx_*, for the same reason --
// what is worth checking here is the numbers, and a check that has to open an
// audio device is a check nobody runs on a build machine.
//
// The part this engine has and a flat-world one does not is at the bottom: a
// sound in the world on the other side of a portal. The rule is the same one
// the virtual camera follows and it is easy to get wrong in the same way --
// what you hear comes THROUGH the opening, so the direction is the opening's
// and the distance is the way round rather than the way through.

namespace cnp {

// How fast a sound falls off, as a shape rather than a taste: at half the
// range, a quarter of the loudness. That is the inverse-square law's shape, and
// this is the constant that makes the curve below produce exactly it.
inline constexpr float kRolloff = 2.0f;

struct Listener {
    Vec3 pos{};
    Vec3 forward{0.0f, 0.0f, -1.0f};   // unit
    Vec3 right{1.0f, 0.0f, 0.0f};      // unit, and perpendicular to forward
};

struct Emitter {
    Vec3  pos{};
    float range = 32.0f;   // nodes; at this distance it is silent, not quiet
    float gain  = 1.0f;    // the sound's own loudness, before distance
};

// What reaches the ear.
struct Heard {
    float gain = 0.0f;   // 0..1. Exactly 0 means do not play it at all.
    float pan  = 0.0f;   // -1 hard left, +1 hard right, 0 ahead or behind
};

// The curve, exposed because it is the thing worth checking on its own: one
// at no distance, exactly zero at the range, and nothing outside [0, 1] in
// between.
float sound_gain(float distance, float range);

// Where in the stereo field, given which way the listener is facing. Zero for a
// source at the ear, and zero for one directly ahead -- and also zero for one
// directly BEHIND, which stereo panning cannot tell apart from ahead. Said here
// rather than discovered.
float sound_pan(const Listener &l, Vec3 source);

Heard hear(const Listener &l, const Emitter &e);

// A sound in the world on the far side of a pair of openings.
//
// The distance is the way ROUND, not the way through: ear to the opening, plus
// the far opening to the source. And the direction is the OPENING's, because
// that is where the sound comes out. Both halves matter and the second is the
// one that bites: a source that happens to share the listener's coordinates is
// in another world, and computing the distance between two points in two
// different worlds gives zero -- a sound at full volume from something that may
// be a hundred nodes past the doorway.
Heard hear_through(const Listener &l, const PortalSurface &here, const PortalSurface &there,
                   const Emitter &e);

// The point of an opening's rectangle closest to `p`. Here because the audio is
// its only caller; when a second one turns up it belongs in portal_math.
Vec3 nearest_on_opening(const PortalSurface &s, Vec3 p);

} // namespace cnp
