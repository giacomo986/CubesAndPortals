#include "core/physics/fall.h"

namespace cnp {

uint16_t fall_damage(float distance)
{
    if (distance <= kSafeFall) return 0;

    const float over = distance - kSafeFall;
    if (over >= 65535.0f) return 65535;   // a fall out of the sky is still a number

    return (uint16_t)over;
}

float fall_step(FallWatch &w, float y)
{
    if (!w.started) {
        w.started = true;
        w.top     = y;
        w.last    = y;
        return 0.0f;
    }

    float fell = 0.0f;

    if (y < w.last) {
        // Still on the way down. `top` is where this fall began and stays put:
        // a fall is measured from where it started, not from the last sample.
    } else {
        // Stopped, or on the way up. Either way whatever fall was in progress
        // ends HERE, and how far it went is the difference from the top.
        fell = w.top - y;
        if (fell < 0.0f) fell = 0.0f;   // rising past the old top is not a fall
        w.top = y;
    }

    w.last = y;
    return fell;
}

void fall_reset(FallWatch &w, float y)
{
    w.started = true;
    w.top     = y;
    w.last    = y;
}

} // namespace cnp
