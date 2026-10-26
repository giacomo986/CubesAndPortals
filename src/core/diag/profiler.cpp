#include "core/diag/profiler.h"

#include <chrono>
#include <cstring>
#include <utility>

namespace cnp::diag {

namespace {

using Clock = std::chrono::steady_clock;

double ms_since(Clock::time_point t)
{
    return std::chrono::duration<double, std::milli>(Clock::now() - t).count();
}

// A section that has been entered and not yet left.
struct Open {
    const char       *name = nullptr;
    Clock::time_point start{};

    // How much of this section's span belongs to sections opened inside it.
    // Subtracting this at close is the whole of "the parent is not charged for
    // the child", and it is what makes the self times sum to the frame instead
    // of to some multiple of it.
    double child_ms = 0.0;

    int depth = 0;
};

// Two frames and a swap, so a steady state allocates nothing: the finished
// frame keeps the vector it built, and the next one builds into the vector the
// frame before that had. A profiler that allocated per frame would appear in
// its own numbers under whichever section happened to be open.
ProfFrame g_last;
ProfFrame g_building;

std::vector<Open> g_stack;

Clock::time_point g_frame_start{};
bool              g_enabled  = false;
bool              g_in_frame = false;

ProfSection &row_for(const char *name, int depth)
{
    for (ProfSection &s : g_building.sections)
        if (s.name == name || std::strcmp(s.name, name) == 0) return s;

    g_building.sections.push_back(ProfSection{name, 0.0, 0.0, 0, depth});
    return g_building.sections.back();
}

} // namespace

void prof_enable(bool on) { g_enabled = on; }
bool prof_enabled() { return g_enabled; }

void prof_frame_begin()
{
    g_stack.clear();
    g_building.sections.clear();
    g_building.frame_ms          = 0.0;
    g_building.attributed_ms     = 0.0;
    g_building.unattributed_ms   = 0.0;
    g_building.unmatched_ends    = 0;
    g_building.unclosed_sections = 0;

    g_in_frame = g_enabled;
    if (!g_in_frame) return;

    g_frame_start = Clock::now();
}

void prof_frame_end()
{
    if (!g_in_frame) {
        // Not "a frame of zeroes": a reader has to be able to tell a frame that
        // cost nothing from one nobody measured.
        g_last.sections.clear();
        g_last.frame_ms          = 0.0;
        g_last.attributed_ms     = 0.0;
        g_last.unattributed_ms   = 0.0;
        g_last.unmatched_ends    = 0;
        g_last.unclosed_sections = 0;
        return;
    }

    // Sections still open when the frame ended. Closed here rather than
    // dropped, so their time lands in their own row instead of vanishing out of
    // the sum, and counted, because a frame that needed this is a frame with a
    // begin somebody forgot to match.
    g_building.unclosed_sections = (int)g_stack.size();
    while (!g_stack.empty()) prof_end();

    // After the forced closures and not before, or the frame would be shorter
    // than the sections it contains and the remainder would come out negative.
    g_building.frame_ms = ms_since(g_frame_start);

    g_building.attributed_ms = 0.0;
    for (const ProfSection &s : g_building.sections) g_building.attributed_ms += s.self_ms;

    // Computed, never measured, and never clamped. A negative remainder would
    // mean the named parts outlasted the frame that contains them, which is a
    // bug in here worth seeing rather than one worth hiding behind a max().
    g_building.unattributed_ms = g_building.frame_ms - g_building.attributed_ms;

    std::swap(g_last, g_building);
    g_in_frame = false;
}

void prof_begin(const char *name)
{
    if (!g_in_frame) return;
    g_stack.push_back(Open{name, Clock::now(), 0.0, (int)g_stack.size()});
}

void prof_end()
{
    if (!g_in_frame) return;

    if (g_stack.empty()) {
        // Swallowing this is what makes the damage: with nothing to pop, the
        // next legitimate end closes somebody else's section and moves their
        // milliseconds into the wrong row. Counted instead.
        ++g_building.unmatched_ends;
        return;
    }

    const Open open = g_stack.back();
    g_stack.pop_back();

    const double total = ms_since(open.start);

    ProfSection &s = row_for(open.name, open.depth);
    s.self_ms += total - open.child_ms;
    s.total_ms += total;
    s.calls += 1;

    if (!g_stack.empty()) g_stack.back().child_ms += total;
}

const ProfFrame &prof_last_frame() { return g_last; }

} // namespace cnp::diag
