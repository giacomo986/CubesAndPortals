#pragma once

// What a fall costs, and how a body that is not simulated is seen to take one.
//
// The second half is the interesting one. This server does not simulate
// physics -- it VALIDATES claims (see MovementLimits) -- so there is no
// velocity here to read and no moment where a body is told it has landed. What
// there is, is a stream of positions, and a fall is what they describe: going
// down for a while, and then stopping.
//
// Written here rather than in the server so that the single-process client
// asks the same question. That loop DOES have a velocity and could work it out
// its own way, and then there would be two rules for what a fall is and two
// answers to how far this one was.

#include <cstdint>

namespace cnp {

// How far a body may fall for nothing.
//
// Three nodes. Two is the height of a doorway and of every ledge in a built
// world, and taking damage for stepping off one would make walking around
// hurt; four would make a two-storey drop free, which is most of the falls a
// player takes on purpose.
inline constexpr float kSafeFall = 3.0f;

// Below which there is no world left to fall through.
//
// Bedrock closes the ordinary way out, but an opening put somewhere silly finds
// another, and falling forever is the one failure a player cannot recover from
// without a menu this engine does not have. Sixteen nodes under the bottom of
// the world: far enough that it is not reached by mistake, near enough that the
// fall is over in a second.
inline constexpr float kBelowWorld = -16.0f;

// One point per node past the free three, rounded down.
//
// Down, so that a fall of three and a bit is still free: the boundary is
// crossed by falling FURTHER, not by landing on a slope. A fall of twenty-three
// takes twenty, which is a full bar -- so there is a height that kills, and it
// is a height a player can learn.
uint16_t fall_damage(float distance);

// Watching a body fall, from its positions alone.
struct FallWatch {
    float top     = 0.0f;   // the highest it has been since it last stopped going down
    float last    = 0.0f;   // where it was on the previous sample
    bool  started = false;  // whether the two above mean anything yet
};

// One position sample.
//
// Returns how far the body fell if this sample LANDED it, and zero otherwise --
// so a caller feeds it every position and asks nothing else.
//
// "Landed" is "stopped going down", which is the only landing a stream of
// positions can show. A body that is still descending is in the middle of a
// fall and is not charged for it yet: charging as it goes would bill a player
// for a fall they are about to survive.
float fall_step(FallWatch &w, float y);

// Forgets the fall in progress, as though the body had always been here.
//
// For wherever something MOVES a body rather than the body moving: a respawn,
// the net under the world. A teleport is not a landing, and the drop that
// preceded it is not one this body took.
void fall_reset(FallWatch &w, float y);

} // namespace cnp
