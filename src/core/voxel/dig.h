#pragma once

#include "core/voxel/chunk.h"

#include <cstdint>

// How a dig runs over time, on the side that is HOLDING the button.
//
// The authority lives elsewhere: the server times a dig in
// ServerSessions::plan_edit and the block comes away when its clock says so.
// This is the other half -- the one that decides when to ask, and what to draw
// while waiting -- and it is written once because both clients need it. The
// single-process client is its own authority and this IS its clock; the
// networked client uses it to know when to send the second Edit and how full to
// draw the bar.
//
// The two agree because they divide the same numbers with the same function
// (net::dig_seconds). A pacing rule written twice would be a block that takes
// two seconds on one side and three on the other, and a player watching a bar
// fill up and then waiting.
namespace cnp {

// One dig, as the holder of the button sees it.
//
// The BLOCK is kept as well as the cell, for the reason the server keeps it: a
// dig was for what was there, and a cell that became something else under it is
// a different dig.
struct DigState {
    bool     active = false;
    int32_t  x = 0, y = 0, z = 0;
    BlockId  block = kAir;
    double   since = 0.0;
};

// What one frame of holding the button does.
//
// `pressed` is the frame the button went down, `down` is every frame it is held
// (including that one). `hit` and the cell are this frame's aim. `need` is how
// long this block takes in this hand -- net::dig_seconds, computed by the
// caller, which is what keeps this header from knowing about items.
//
// Returns whether the block comes away NOW. `progress` is filled with 0..1 for
// whoever draws it, and is 0 when nothing is being dug.
//
// A block that takes no time completes on the PRESS frame and not on the frames
// after it, which is what keeps one click to one block: holding the button down
// over a floor of instant blocks must not mine a trench.
inline bool dig_step(DigState &d, bool pressed, bool down, bool hit, int32_t x, int32_t y,
                     int32_t z, BlockId block, double need, double now, float *progress)
{
    if (progress) *progress = 0.0f;

    if (!down || !hit || block == kAir) {
        d.active = false;
        return false;
    }

    // A new dig: the button just went down, or the aim moved, or what was there
    // is not what this dig was for.
    if (!d.active || pressed || d.x != x || d.y != y || d.z != z || d.block != block) {
        // ...but only a PRESS may start one. Sliding the aim onto a new cell
        // with the button already held restarts the clock rather than
        // continuing, and letting it start without a press would mine a trench.
        d.active = true;
        d.x = x; d.y = y; d.z = z;
        d.block = block;
        d.since = now;
    }

    if (need <= 0.0) {
        // Instant, and only on the frame the button went down. Every block in
        // this game was this until hardness existed.
        if (!pressed) return false;
        d.active = false;
        return true;
    }

    const double gone = now - d.since;
    if (progress) *progress = (float)(gone / need > 1.0 ? 1.0 : (gone < 0.0 ? 0.0 : gone / need));
    if (gone < need) return false;

    d.active = false;
    return true;
}

// Whether this frame is a dig at all.
//
// Four ways it is not, and each of them is a way a player stops digging: the
// button comes up, the crosshair leaves the block, a panel takes the mouse, or
// something else has claimed the click. They are ARGUMENTS and not a rule read
// off the platform, because the two loops read their input differently and the
// gate reads neither.
inline bool dig_frame(bool button_down, bool aimed, bool world_has_mouse, bool claimed)
{
    return button_down && aimed && world_has_mouse && !claimed;
}

// A frame where nobody is digging: the state says so and the bar says so.
//
// It is a function and not two lines at each call because the defect it fixes
// was a caller that simply stopped CALLING dig_step -- the progress kept the
// last value it had been given, so a dig broken off half way left a half-full
// bar under the crosshair until the next one started. Seen in play, on a
// released button.
inline void dig_idle(DigState &d, float *progress)
{
    d.active = false;
    if (progress) *progress = 0.0f;
}

} // namespace cnp
