#include "core/portal/portal_tuning.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>

namespace cnp {
namespace {

PortalTuning g_tuning;

// Trims spaces from both ends. Written out rather than pulled from a string
// utility header, because this file is the only thing in core/ that parses text
// and one helper is cheaper than a dependency.
std::string trim(const std::string &s)
{
    size_t a = 0, b = s.size();
    while (a < b && (s[a] == ' ' || s[a] == '\t' || s[a] == '\r')) ++a;
    while (b > a && (s[b - 1] == ' ' || s[b - 1] == '\t' || s[b - 1] == '\r')) --b;
    return s.substr(a, b - a);
}

bool parse_number(const std::string &text, float *out)
{
    try {
        size_t used = 0;
        const float v = std::stof(text, &used);
        if (used != text.size()) return false;
        *out = v;
        return true;
    } catch (...) {
        return false;
    }
}

} // namespace

const PortalTuning &portal_tuning()
{
    return g_tuning;
}

bool portal_tuning_set(const PortalTuning &t, std::string &err)
{
    // The ranges are what the geometry can express, not taste. A plane outside
    // the block is a portal whose image is not in its own doorway; a trigger
    // outside it fires where there is no portal to fire in; a tunnel that
    // starts past the plane is inside out.
    //
    // `exit`, `view` and `clip` are the three that reach past the block, and
    // they have to:
    // the exit that leaves a crossing seamless is trigger + view, which is
    // outside the exit block for any viewpoint offset at all. Past 1.0 the
    // player lands in the open in front of the destination, which is a place,
    // not an error.
    auto range = [&](const char *name, float v, float lo, float hi) {
        if (v >= lo && v <= hi) return true;
        std::ostringstream o;
        o << name << " is " << v << ", outside [" << lo << ", " << hi << "]";
        err = o.str();
        return false;
    };

    if (!range("plane", t.plane, 0.0f, 1.0f)) return false;
    if (!range("tunnel_start", t.tunnel_start, 0.0f, 1.0f)) return false;
    if (!range("trigger", t.trigger, 0.0f, 1.0f)) return false;
    if (!range("exit", t.exit, 0.0f, 4.0f)) return false;
    if (!range("view", t.view, 0.0f, 4.0f)) return false;
    if (!range("clip", t.clip, 0.0f, 4.0f)) return false;
    if (!range("sides", t.sides, 0.0f, 1.0f)) return false;
    if (!range("one_way", t.one_way, 0.0f, 1.0f)) return false;

    if (t.tunnel_start > t.plane) {
        err = "tunnel_start is past plane: the doorway would start behind its own image";
        return false;
    }
    if (t.trigger > t.plane) {
        err = "trigger is past plane: the crossing would fire behind the image it crosses";
        return false;
    }

    g_tuning = t;
    return true;
}

bool portal_tuning_load(const char *path, bool *found, std::string &err)
{
    if (found) *found = false;

    std::ifstream in(path);
    if (!in) return true;   // absent, not broken
    if (found) *found = true;

    PortalTuning t;
    std::string  line;
    int          lineno = 0;

    while (std::getline(in, line)) {
        ++lineno;
        const size_t hash = line.find('#');
        if (hash != std::string::npos) line = line.substr(0, hash);
        line = trim(line);
        if (line.empty()) continue;

        const size_t eq = line.find('=');
        if (eq == std::string::npos) {
            std::ostringstream o;
            o << path << ":" << lineno << ": expected key = value";
            err = o.str();
            return false;
        }

        const std::string key = trim(line.substr(0, eq));
        const std::string val = trim(line.substr(eq + 1));

        float number = 0.0f;
        if (!parse_number(val, &number)) {
            std::ostringstream o;
            o << path << ":" << lineno << ": '" << val << "' is not a number";
            err = o.str();
            return false;
        }

        if      (key == "plane")        t.plane        = number;
        else if (key == "tunnel_start") t.tunnel_start = number;
        else if (key == "trigger")      t.trigger      = number;
        else if (key == "exit")         t.exit         = number;
        else if (key == "view")         t.view         = number;
        else if (key == "clip")         t.clip         = number;
        else if (key == "sides")        t.sides        = number;
        else if (key == "one_way")      t.one_way      = number;
        else {
            std::ostringstream o;
            o << path << ":" << lineno << ": no setting named '" << key << "'";
            err = o.str();
            return false;
        }
    }

    if (!portal_tuning_set(t, err)) {
        err = std::string(path) + ": " + err;
        return false;
    }
    return true;
}

bool portal_tuning_load_default(std::string &err)
{
    const char *env = std::getenv("CNP_PORTAL_CONFIG");
    const char *candidates[3] = {env, "config/portal.cfg", "../config/portal.cfg"};

    for (const char *path : candidates) {
        if (!path || !*path) continue;
        bool found = false;
        if (!portal_tuning_load(path, &found, err)) return false;
        if (found) {
            const PortalTuning &t = portal_tuning();
            std::printf("portal shape from %s: plane %.2f, tunnel_start %.2f, trigger %.2f, "
                        "exit %.2f, view %.2f, clip %.2f, sides %s, %s\n", path, (double)t.plane,
                        (double)t.tunnel_start, (double)t.trigger, (double)t.exit, (double)t.view,
                        (double)t.clip, t.sides_are_window() ? "window" : "wall",
                        t.is_one_way() ? "one way" : "both ways");

            // The near plane one doorway behind the glued plane is what makes
            // the tunnel the player walks down and the destination's own block
            // the same volume rather than two. Any other value is a deliberate
            // experiment with a cost on each side -- see `clip` in the file --
            // and it is said out loud rather than left to be discovered from an
            // edge that shows sky or a wall that z-fights.
            const float doorway = t.plane - t.tunnel_start;
            if (std::fabs(t.clip - doorway) > 1e-4f)
                std::printf("portal shape: NOTE clip is %.2f, not the %.2f this doorway asks "
                            "for. Short of it the destination's ground is cut away with its "
                            "wall; past it, the wall is left standing in front of the "
                            "opening.\n", (double)t.clip, (double)doorway);
            return true;
        }
    }

    // Nothing found. The built-in defaults are the same numbers the file ships
    // with, so this is a working engine rather than a broken one -- but it is
    // said out loud, because "I changed the config and nothing happened" is
    // otherwise a long afternoon.
    std::printf("portal shape: no config/portal.cfg found, using the built-in defaults\n");
    return true;
}

} // namespace cnp
