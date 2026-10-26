#pragma once

// How much of a body is left.
//
// DESIGN.md 8 lists "salute" among M7's pieces, and this is it. A component and
// not a field on a session, for the reason every other component here is one: a
// mob has a body and no session, a player who walks through a portal keeps the
// session and gets a new body, and an observer has a session and no body at all
// -- so "how hurt is that thing" belongs to the thing.
//
// What is NOT here: who hurt it, when, or with what. A damage type is a rule
// about armour and resistances that this engine has none of, and a field that
// nothing reads is a field that will be wrong when something finally does.

#include <cstdint>

namespace cnp::ecs {

// Ten hearts, drawn as twenty points, because a bar that moves by halves is a
// bar a player can read at a glance -- and because every number below (a fall
// of four nodes, a fall of twenty) has to mean something against it.
inline constexpr uint16_t kFullHealth = 20;

// How long a body takes to get one point back, and how long it must go unhurt
// before it starts.
//
// There is no food in this engine, so without regeneration the first fall is a
// wound a player carries until something kills them -- a number that only ever
// goes down is a countdown, not a resource. Four seconds a point is a full bar
// in eighty: long enough that falling twice in a row is dangerous, short enough
// that a bad landing is not the rest of the session.
//
// The wait is what makes it a rule and not a cushion: a body regenerating while
// something is still hurting it never dies.
inline constexpr double kRegenPeriod = 4.0;
inline constexpr double kRegenDelay  = 5.0;

struct Health {
    uint16_t hp  = kFullHealth;
    uint16_t max = kFullHealth;

    bool alive() const { return hp > 0; }
    bool full() const { return hp >= max; }
};

// Takes points off, and never below nothing.
//
// Returns how many were ACTUALLY taken, which is less than asked when the body
// did not have that many left. A caller that wants to know whether this was the
// killing blow asks the body afterwards -- alive() -- rather than reading it out
// of the return value, because "it took less than I asked" and "it died" are
// two different facts that happen to coincide.
uint16_t hurt(Health &h, uint16_t amount);

// What is left of a blow after armour: each point takes 4% off, up to 20 points
// (80%), the rule players of this kind of game already know. Rounded, and never
// below one -- a hit that lands always costs something. A fall is not armoured.
inline uint16_t armoured(uint16_t amount, int armor)
{
    if (amount == 0 || armor <= 0) return amount;
    if (armor > 20) armor = 20;
    const uint32_t left = ((uint32_t)amount * (uint32_t)(25 - armor) + 12u) / 25u;
    return (uint16_t)(left < 1u ? 1u : left);
}

// And back on, never above the maximum. Returns how many were given.
uint16_t heal(Health &h, uint16_t amount);

// The clock a body regenerates on.
//
// Two counters and not one: how long since something last hurt it, and how much
// of the current point has been earned. One counter would have to mean both,
// and the moment it was reset by damage it would also throw away progress the
// body had already paid for.
struct Regen {
    double since_hurt = kRegenDelay;   // an untouched body starts ready
    double earned     = 0.0;
};

// Moves the clock on by `dt` and returns how many points were earned in it.
//
// Takes the health so it can stop counting on a body that is dead or full: a
// counter that runs while nothing can come of it is a counter that pays out all
// at once the moment something can.
uint16_t regen_step(Regen &r, const Health &h, double dt);

// Something hurt it just now, so the wait starts again.
void regen_interrupt(Regen &r);

} // namespace cnp::ecs
