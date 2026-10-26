#include "core/ecs/health.h"

namespace cnp::ecs {

uint16_t hurt(Health &h, uint16_t amount)
{
    const uint16_t taken = amount < h.hp ? amount : h.hp;
    h.hp = (uint16_t)(h.hp - taken);
    return taken;
}

uint16_t heal(Health &h, uint16_t amount)
{
    if (h.hp >= h.max) return 0;

    const uint16_t room  = (uint16_t)(h.max - h.hp);
    const uint16_t given = amount < room ? amount : room;
    h.hp = (uint16_t)(h.hp + given);
    return given;
}

uint16_t regen_step(Regen &r, const Health &h, double dt)
{
    if (dt <= 0.0) return 0;

    r.since_hurt += dt;

    // A dead body does not heal itself back to life, and a full one has nothing
    // to earn. Both stop the counter rather than letting it run: a counter that
    // runs while nothing can come of it pays out all at once the moment
    // something can.
    if (!h.alive() || h.full()) {
        r.earned = 0.0;
        return 0;
    }

    if (r.since_hurt < kRegenDelay) {
        r.earned = 0.0;
        return 0;
    }

    r.earned += dt;

    // A whole number of points, and the remainder stays on the clock. A tick
    // long enough for two points gives two: a server that stuttered must not
    // owe a player health it silently dropped.
    uint16_t points = 0;
    while (r.earned >= kRegenPeriod) {
        r.earned -= kRegenPeriod;
        ++points;
    }

    const uint16_t room = (uint16_t)(h.max - h.hp);
    return points < room ? points : room;
}

void regen_interrupt(Regen &r)
{
    r.since_hurt = 0.0;
    r.earned     = 0.0;
}

} // namespace cnp::ecs
