#pragma once

// Where a frame went.
//
// DESIGN.md 8 asks M9 for a profiler and says nothing about what one is, so
// `frame_budget` wrote the property down before this file existed: THE PARTS
// HAVE TO ADD UP TO THE WHOLE. Everything named, plus everything not named, is
// the frame -- exactly, as arithmetic, not approximately.
//
// That is the whole design. A profiler that shows four sections adding to 60%
// of a 20 ms frame is worse than none: it points at the largest of the four
// while the 8 ms nobody instrumented is where the frame actually went. So the
// remainder is COMPUTED here and handed back as a number of its own, and a
// frame with nothing instrumented in it comes back 100% unattributed rather
// than as an empty list that reads like a clean bill of health.
//
// Self time and not inclusive time is what the sum is over. A section is
// charged the time it was open MINUS the time its children were open, so a
// parent and its child never claim the same milliseconds twice. Both numbers
// are reported, because "meshing costs 9 ms" and "meshing itself costs 1 ms
// and the upload inside it costs 8" are different answers and a reader chasing
// a budget needs the second one.
//
// ONE THREAD -- the one that calls prof_frame_begin. There is no lock and no
// per-thread table, and that is a decision rather than an omission: everything
// this measures today runs on the frame thread, including meshing. When meshing
// moves to a worker (M9's other half), what this will measure is the frame
// thread's WAIT for it, which is the number a frame budget is actually about;
// what the worker spends is a second question and will need a second answer,
// not a mutex bolted onto this one.

#include <cstddef>
#include <vector>

namespace cnp::diag {

// One named span of a frame.
struct ProfSection {
    // A string literal the caller owns. Nothing here copies it: a profiler that
    // allocated a string per section per frame would be a profiler that shows
    // up in its own numbers.
    const char *name = nullptr;

    // The time this section was open, minus the time its children were. This is
    // the column that sums to the frame.
    double self_ms = 0.0;

    // The time it was open, children included.
    double total_ms = 0.0;

    // How many times it was entered this frame. Entering the same name twice is
    // one row and two calls, not two rows.
    int calls = 0;

    // How deep it sat, 0 for a section opened directly in the frame. Taken from
    // the FIRST time it was entered: a name used at two different depths is a
    // reader's problem to name better, not something this will invent an answer
    // for.
    int depth = 0;
};

// A note on the two columns above. `self_ms` is the one the sum is over and it
// is right under every arrangement, recursion included. `total_ms` is not: a
// section entered inside itself counts the inner span in both levels, which is
// what "inclusive" means and is worth knowing before reading one.

// A finished frame.
struct ProfFrame {
    // Sections in the order they were first entered, which keeps an overlay
    // from reshuffling itself every frame.
    std::vector<ProfSection> sections;

    double frame_ms       = 0.0;   // begin to end
    double attributed_ms  = 0.0;   // the sum of every self_ms above
    double unattributed_ms = 0.0;  // frame_ms - attributed_ms, computed and not measured

    // Ends that had no begin. A frame with any is a frame whose numbers are
    // suspect above the point it happened, so it is reported rather than
    // swallowed -- swallowing one pops somebody else's section and quietly
    // moves their milliseconds into the wrong row.
    int unmatched_ends = 0;

    // Sections still open when the frame ended -- a begin with no end, which is
    // what an early return out of a measured block leaves behind. They are
    // closed at the frame boundary so their time lands in their own row rather
    // than disappearing out of the sum, and counted here so that the frame says
    // it had to do that.
    int unclosed_sections = 0;
};

// Off by default. When off, begin/end do a single load and return: the shipped
// build must not pay for clocks nobody reads.
void prof_enable(bool on);
bool prof_enabled();

void prof_frame_begin();
void prof_frame_end();

void prof_begin(const char *name);
void prof_end();

// The last frame that prof_frame_end closed. Empty, with a frame_ms of zero,
// when the profiler is off -- rather than a frame of zeroes, which would read
// as a measurement.
const ProfFrame &prof_last_frame();

// A scope, because every early return out of a measured block is an unmatched
// begin waiting to happen.
class ProfScope {
public:
    explicit ProfScope(const char *name) { prof_begin(name); }
    ~ProfScope() { prof_end(); }
    ProfScope(const ProfScope &)            = delete;
    ProfScope &operator=(const ProfScope &) = delete;
};

} // namespace cnp::diag
