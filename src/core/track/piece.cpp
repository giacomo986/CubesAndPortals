#include "core/track/piece.h"

#include "core/voxel/chunk.h"

#include <algorithm>
#include <cmath>
#include <map>

namespace cnp {

namespace {

constexpr float kHalfPi    = 1.57079632679f;
constexpr float kCurbWide  = 0.6f;    // nodes in from the edge where the curb starts rising
constexpr float kCurbTop   = 0.15f;   // nodes, at the very edge

bool chicane(const PieceShape &p) { return p.plan == 3 || p.plan == 4; }
bool walled(const PieceShape &p) { return p.wall != 0 && p.plan == 1; }
// A loop: 6 comes out beside its entry to the left, 7 to the right.
bool looped(const PieceShape &p) { return p.plan == 6 || p.plan == 7; }
// A wide road narrowing to one lane: 8 keeps its right lane, 9 its left.
bool tapered(const PieceShape &p) { return p.plan == 8 || p.plan == 9; }
// A road along its length, one u to one z: a straight and an adapter.
bool straight(const PieceShape &p) { return p.plan == 1 || tapered(p); }

// Nodes over the road's middle a wall ride rolls about: none, the middle
// itself. Rolled about a line over it, the lane went up and across as it
// rolled, a kart driving straight stayed where it was, and the road left it:
// off the low edge half way into the wall. About the middle it stays under the
// kart, which rolls with it; the low edge goes down half a road instead.
constexpr float kWallAxis = 0.0f;

float ease_of(float u) { return 0.5f - 0.5f * std::cos(u * 3.14159265f); }

// The loop's middle line, for a loop of length 1 along it: its heading turns a
// whole circle up and over as the ease goes, so the bend is none where it
// leaves the flat and tightest at the top -- a circle bent a kart from nothing
// to all of it in one node. Where it is at s, forward and up, read off a table
// made once (the line has no closed form): it comes back down 0.304 ahead of
// where it went up, and its top is 0.259 up.
constexpr int kLoopSteps = 1024;
struct LoopLine {
    float z[kLoopSteps + 1], y[kLoopSteps + 1];
    LoopLine()
    {
        z[0] = y[0] = 0.0f;
        double zz = 0.0, yy = 0.0;
        for (int i = 0; i < kLoopSteps; ++i) {
            constexpr int k = 16;   // each step in parts, the middle of each
            for (int j = 0; j < k; ++j) {
                const double s = ((double)i + ((double)j + 0.5) / k) / kLoopSteps;
                const double a = 2.0 * 3.14159265358979 * (0.5 - 0.5 * std::cos(s * 3.14159265358979));
                zz += std::cos(a) / (kLoopSteps * k);
                yy += std::sin(a) / (kLoopSteps * k);
            }
            z[i + 1] = (float)zz;
            y[i + 1] = (float)yy;
        }
    }
};
const LoopLine &loop_line()
{
    static const LoopLine line;
    return line;
}
void loop_at(float s, float &z, float &y)
{
    const LoopLine &l = loop_line();
    const float     f = std::clamp(s, 0.0f, 1.0f) * (float)kLoopSteps;
    const int       i = std::min((int)f, kLoopSteps - 1);
    const float     t = f - (float)i;
    z = l.z[i] + (l.z[i + 1] - l.z[i]) * t;
    y = l.y[i] + (l.y[i + 1] - l.y[i]) * t;
}

// A loop's line, in nodes: as long as the top stays under a kart's ceiling
// over the road (track_game's rise) and the piece's height -- 8.5 up on one
// lane, 11.1 on two. One lane three squares long is the tall loop, twice the
// line and twice as high, 16.9 up. What is left of the footprint's length is
// flat road before and after it, half each.
float loop_line_len(const PieceShape &p)
{
    return p.width >= 2 ? 42.0f : p.size >= 3 ? 64.0f : 32.0f;
}
float loop_flat(const PieceShape &p)
{
    return 0.5f * ((float)(p.size * kPieceUnit) - loop_line_len(p) * loop_line().z[kLoopSteps]);
}
float loop_len(const PieceShape &p) { return 2.0f * loop_flat(p) + loop_line_len(p); }

// Where a loop is at u along it (0 to 1, flat runs and all): how far into the
// line (s), forward, up and the heading's angle above the flat.
void loop_pose(const PieceShape &p, float u, float &s, float &z, float &y, float &a)
{
    const float flat = loop_flat(p), line = loop_line_len(p), d = u * loop_len(p);
    if (d <= flat) {
        s = 0.0f;
        z = d;
        y = a = 0.0f;
        return;
    }
    if (d >= flat + line) {
        s = 1.0f;
        z = d - line + line * loop_line().z[kLoopSteps];
        y = a = 0.0f;
        return;
    }
    s = (d - flat) / line;
    loop_at(s, z, y);
    z = flat + z * line;
    y *= line;
    a = 2.0f * 3.14159265f * ease_of(s);
}

// How far across a loop has gone, from its entry lane to the one beside it:
// none until the line's own way back down has crossed under the way up (at
// 0.17 and 0.83 of it, seen from the side), and a lane's width after.
float loop_shift(const PieceShape &p, float s)
{
    return (float)(kPieceUnit * std::max<int>(p.width, 1)) * ease_of(std::clamp((s - 0.2f) / 0.6f, 0.0f, 1.0f));
}

// Where up-facing ends on a loop's line: a road steeper than 70 degrees has
// nothing under it a column could stand in (as a wall ride's).
constexpr float kLoopUp = 0.2896f;

// How far a wall ride is rolled at u, in radians: positive stands the left edge up.
float roll_at(const PieceShape &p, float u)
{
    const float deg = (float)p.bank0 + (float)(p.bank1 - p.bank0) * ease_of(u);
    return deg * (3.14159265f / 180.0f);
}

// Half the road's width, in nodes: a square's for a road one square wide,
// two for a wide one (PieceShape::width).
float half_of(const PieceShape &p) { return 0.5f * (float)(kPieceUnit * std::max<int>(p.width, 1)); }

// And at u: an adapter's goes from two lanes down to one.
float half_at(const PieceShape &p, float u)
{
    const float h = half_of(p);
    return tapered(p) ? h - 0.5f * h * ease_of(std::clamp(u, 0.0f, 1.0f)) : h;
}

// Of the half width, where the curb starts rising: the same 0.6 nodes from the
// edge on a wide road as on a narrow one.
float curb_from(const PieceShape &p, float u = 0.0f) { return 1.0f - kCurbWide / half_at(p, u); }

// A quarter turn either way: 2 to the right, 5 to the left -- the right one's
// mirror, so a wide road's lanes are the right way round driven either way.
bool turning(const PieceShape &p) { return p.plan == 2 || p.plan == 5; }

float piece_len(const PieceShape &p)   // in units, along the middle of the road
{
    const float l = (float)(p.size * kPieceUnit);
    if (straight(p)) return (float)p.size;
    if (looped(p)) return loop_len(p) / (float)kPieceUnit;
    if (chicane(p)) return std::sqrt(l * l + 64.0f) / (float)kPieceUnit;
    return (l - half_of(p)) * kHalfPi / (float)kPieceUnit;
}

// Unturned, in nodes.
void base_size(const PieceShape &p, float &w, float &l)
{
    w = (float)(straight(p) ? kPieceUnit * std::max<int>(p.width, 1)
                : looped(p)  ? 2 * kPieceUnit * std::max<int>(p.width, 1)
                : chicane(p) ? 2 * kPieceUnit : p.size * kPieceUnit);
    l = (float)(p.size * kPieceUnit);
}

// A chicane's middle line, unturned: from the middle of one square across to
// the middle of the other, along a half cosine -- to the right (-x) for plan 3,
// to the left for plan 4. Where it is at u, and which way it runs.
void chicane_line(const PieceShape &p, float u, float &cx, float &dx)
{
    const float from = p.plan == 3 ? 12.0f : 4.0f, by = p.plan == 3 ? -8.0f : 8.0f;
    cx = from + by * (0.5f - 0.5f * std::cos(u * 3.14159265f));
    dx = by * 0.5f * 3.14159265f * std::sin(u * 3.14159265f);   // per unit of u
}

// A quarter turn about the vertical, as many times as the anchor's byte says,
// of a point in a w by l box, keeping the box's corner at the origin: +z
// becomes +x. Byte 2 is none: a placement turns a front to the player (0 for
// one standing on the -z side), and a piece runs away from them.
int turns(int facing) { return (facing + 2) & 3; }

void turn(int f, float w, float l, float &x, float &z)
{
    for (int i = 0; i < turns(f); ++i) {
        const float nx = z, nz = w - x;
        x = nx;
        z = nz;
        std::swap(w, l);
    }
}

void unturn(int f, float w, float l, float &x, float &z)
{
    if (turns(f) & 1) std::swap(w, l);
    turn((6 - turns(f)) & 3, w, l, x, z);   // the turns that undo these
}

// The road's height at u along it and v across it (-1 left, +1 right).
float height(const PieceShape &p, float u, float v)
{
    const float len = piece_len(p);
    const float c = (float)p.climb, m0 = (float)p.grade0 * len, m1 = (float)p.grade1 * len;
    const float u2 = u * u, u3 = u2 * u;

    // Flat at both ends, a half cosine and not a cubic: it bends the road
    // least for the height it climbs (a cubic bends 6 at its ends where this
    // bends pi^2 / 2), and a crest bent more than gravity holds a kart to is
    // a crest it flies off. Two nodes over two squares holds one at 24 m/s.
    if (looped(p)) {   // no curbs, no bank: a loop is a wall ride's road bent over
        float s, z, y, a;
        loop_pose(p, u, s, z, y, a);
        return kPieceBase + y;
    }

    const float ease = ease_of(u);
    float       y    = kPieceBase;
    if (p.grade0 == 0 && p.grade1 == 0)
        y += c * ease;
    else
        y += (-2.0f * u3 + 3.0f * u2) * c + (u3 - 2.0f * u2 + u) * m0 + (u3 - u2) * m1;

    // Rolled about its own middle (kWallAxis) into a wall: standing, its face
    // looks across the square it stands on, and the kart that rolled with it
    // is on it.
    if (walled(p)) {
        const float a = roll_at(p, u), r = half_of(p) * v;
        return y + kWallAxis - r * std::sin(a) - kWallAxis * std::cos(a);
    }

    // Banked about the middle of the road, which stays where it was: the left
    // edge rises half the bank and the right one sinks half. About the low
    // edge the middle climbed two nodes into every bank and fell two out of
    // it, and a kart at speed flew off the top of that and landed hard.
    const float b = (float)p.bank0 + (float)(p.bank1 - p.bank0) * ease;
    y -= b * v * 0.5f;

    if (p.curbs) {
        const float from = curb_from(p, u);
        y += kCurbTop * std::clamp((std::fabs(v) - from) / (1.0f - from), 0.0f, 1.0f);
    }
    return y;
}

// Where (u, v) is, unturned, on the floor.
void place(const PieceShape &p, float u, float v, float &x, float &z)
{
    const float half = half_of(p);
    if (p.plan == 5) {   // the right turn's mirror: its left is the right turn's right
        PieceShape right = p;
        right.plan       = 2;
        place(right, u, -v, x, z);
        x = (float)(p.size * kPieceUnit) - x;
        return;
    }
    if (walled(p)) {
        const float a = roll_at(p, u);
        x = half - (half * v * std::cos(a) - kWallAxis * std::sin(a));
        z = u * (float)(p.size * kPieceUnit);
        return;
    }
    if (p.plan == 1) {
        x = half - half * v;   // right is -x
        z = u * (float)(p.size * kPieceUnit);
        return;
    }
    if (tapered(p)) {   // the edge it keeps stays where it was
        const float h = half_at(p, u);
        x = p.plan == 8 ? h * (1.0f - v) : 2.0f * half - h * (1.0f + v);
        z = u * (float)(p.size * kPieceUnit);
        return;
    }
    if (looped(p)) {   // entered on its first lane, left on the one beside it
        float s, y, a;
        loop_pose(p, u, s, z, y, a);
        const float in = p.plan == 6 ? half : 3.0f * half;
        x = (p.plan == 6 ? in + loop_shift(p, s) : in - loop_shift(p, s)) - half * v;
        return;
    }
    if (chicane(p)) {
        // Across the line at right angles to it: right is its direction
        // turned a quarter towards -x.
        const float l = (float)(p.size * kPieceUnit);
        float       cx, dx;
        chicane_line(p, u, cx, dx);
        const float n = std::sqrt(dx * dx + l * l);
        x = cx - l / n * half * v;
        z = u * l + dx / n * half * v;
        return;
    }
    const float r = (float)(p.size * kPieceUnit) - half - half * v;   // right is inward
    const float a = u * kHalfPi;
    x = r * std::cos(a);
    z = r * std::sin(a);
}

bool unplace(const PieceShape &p, float x, float z, float &u, float &v)
{
    const float half = half_of(p);
    if (p.plan == 5) {
        PieceShape right = p;
        right.plan       = 2;
        const bool on    = unplace(right, (float)(p.size * kPieceUnit) - x, z, u, v);
        v                = -v;
        return on;
    }
    if (walled(p)) {
        // Over the floor only while the road still faces up more than
        // sideways: a wall has nothing under it to stand in a column.
        u = z / (float)(p.size * kPieceUnit);
        const float a = roll_at(p, std::clamp(u, 0.0f, 1.0f));
        if (std::cos(a) < 0.35f) return false;
        v = ((half - x) + kWallAxis * std::sin(a)) / std::cos(a) / half;
    } else if (p.plan == 1) {
        v = (half - x) / half;
        u = z / (float)(p.size * kPieceUnit);
    } else if (tapered(p)) {
        u = z / (float)(p.size * kPieceUnit);
        const float h = half_at(p, u);
        v = p.plan == 8 ? 1.0f - x / h : (2.0f * half - x) / h - 1.0f;
    } else if (looped(p)) {
        // On its two ends only, where the road still faces up more than
        // sideways (kLoopUp): along each z only grows, so the u over z is
        // found by halves, and the first that is across x is the road.
        const float len = loop_len(p), flat = loop_flat(p), line = loop_line_len(p);
        const float ends[2][2] = {{0.0f, (flat + line * kLoopUp) / len},
                                  {(flat + line * (1.0f - kLoopUp)) / len, 1.0f}};
        for (const auto &e : ends) {
            float lo = e[0], hi = e[1], s, zz, y, a;
            loop_pose(p, lo, s, zz, y, a);
            if (z < zz) continue;
            loop_pose(p, hi, s, zz, y, a);
            if (z > zz) continue;
            for (int i = 0; i < 24; ++i) {
                const float mid = 0.5f * (lo + hi);
                loop_pose(p, mid, s, zz, y, a);
                (zz < z ? lo : hi) = mid;
            }
            u = 0.5f * (lo + hi);
            loop_pose(p, u, s, zz, y, a);
            const float in = p.plan == 6 ? half : 3.0f * half;
            const float cx = p.plan == 6 ? in + loop_shift(p, s) : in - loop_shift(p, s);
            v = (cx - x) / half;
            if (v >= -1.0f && v <= 1.0f) return true;
        }
        return false;
    } else if (chicane(p)) {
        // The point on the line whose right angle passes through (x, z): a
        // few steps of Newton from straight across, which the line never
        // strays far from.
        const float l = (float)(p.size * kPieceUnit);
        u = z / l;
        for (int i = 0; i < 6; ++i) {
            float cx, dx;
            chicane_line(p, std::clamp(u, 0.0f, 1.0f), cx, dx);
            u += ((x - cx) * dx + (z - u * l) * l) / (dx * dx + l * l);
        }
        float cx, dx;
        chicane_line(p, std::clamp(u, 0.0f, 1.0f), cx, dx);
        const float n = std::sqrt(dx * dx + l * l);
        v = (-(x - cx) * l + (z - u * l) * dx) / n / half;
    } else {
        const float r = std::sqrt(x * x + z * z);
        v = ((float)(p.size * kPieceUnit) - half - r) / half;
        u = std::atan2(z, x) / kHalfPi;
    }
    return u >= 0.0f && u <= 1.0f && v >= -1.0f && v <= 1.0f;
}

Vec3 at(const PieceShape &p, int facing, float u, float v, float y)
{
    float w, l, x, z;
    base_size(p, w, l);
    place(p, u, v, x, z);
    turn(facing, w, l, x, z);
    return Vec3{x, y, z};
}

void tri(std::vector<PieceTri> &out, Vec3 a, Vec3 b, Vec3 c, Vec3 outward, uint8_t role,
         uint8_t stripe = 0)
{
    PieceTri t;
    t.role   = role;
    t.stripe = stripe;
    t.v[0] = a;
    if (dot(cross(b - a, c - a), outward) >= 0.0f) {
        t.v[1] = b;
        t.v[2] = c;
    } else {
        t.v[1] = c;
        t.v[2] = b;
    }
    out.push_back(t);
}

void quad(std::vector<PieceTri> &out, Vec3 a, Vec3 b, Vec3 c, Vec3 d, Vec3 outward, uint8_t role,
          uint8_t stripe = 0)
{
    // Corners in order round the edge.
    tri(out, a, b, c, outward, role, stripe);
    tri(out, a, c, d, outward, role, stripe);
}

} // namespace

bool piece_valid(const PieceShape &p)
{
    if (p.plan < 1 || p.plan > 9 || p.size < 1 || p.size > kPieceMaxUnits) return false;
    if (p.width < 1 || p.width > 2) return false;
    // A wide road: straights and turns, and a turn round a middle at least a
    // lane out. Not a chicane or a wall ride -- a wide wall would stand a
    // whole road's width, eight nodes under its middle.
    if (p.width == 2 && (chicane(p) || p.wall != 0)) return false;
    if (turning(p) && p.size < p.width) return false;
    if (chicane(p) && p.size < 2) return false;   // a square across in one is a wall
    // An adapter is a wide road's: its other end is one lane of it.
    if (tapered(p) && (p.width != 2 || p.wall != 0 || p.bank0 != 0 || p.bank1 != 0)) return false;
    // A loop is flat where it starts and ends, and long enough for its line.
    if (looped(p) && (p.wall != 0 || p.climb != 0 || p.grade0 != 0 || p.grade1 != 0 ||
                      p.bank0 != 0 || p.bank1 != 0 || loop_flat(p) < 0.0f))
        return false;
    if (p.wall != 0 && (p.plan != 1 || std::abs(p.bank0) > 90 || std::abs(p.bank1) > 90 ||
                        p.climb != 0 || p.grade0 != 0 || p.grade1 != 0))
        return false;
    for (int i = 0; i <= 16; ++i)
        for (int j = 0; j <= 4; ++j) {
            const float y = height(p, (float)i / 16.0f, -1.0f + 0.5f * (float)j);
            if (y < 0.2f - (float)kPieceBelow || y > (float)kPieceMaxHeight - 0.5f) return false;
        }
    return true;
}

void piece_size(const PieceShape &p, uint8_t facing, int &nx, int &nz)
{
    float w, l;
    base_size(p, w, l);
    if (turns(facing) & 1) std::swap(w, l);
    nx = (int)w;
    nz = (int)l;
}

bool piece_surface(const PieceShape &p, uint8_t facing, float x, float z, float &y)
{
    float w, l, u, v;
    base_size(p, w, l);
    unturn(facing, w, l, x, z);
    if (!unplace(p, x, z, u, v)) return false;
    y = height(p, u, v);
    return true;
}

bool piece_column(const PieceShape &p, uint8_t facing, int cx, int cz, int &top)
{
    // Sampled, and a hair under the lowest sample: the road is smooth and
    // bends less than that between two samples an eighth of a node apart.
    float low   = 1e30f;
    bool  found = false;
    for (int i = 0; i <= 8; ++i)
        for (int j = 0; j <= 8; ++j) {
            float y;
            if (!piece_surface(p, facing, (float)cx + 0.125f * (float)i, (float)cz + 0.125f * (float)j, y))
                continue;
            low   = std::min(low, y);
            found = true;
        }
    if (!found) return false;
    top = (int)std::floor((low - 0.02f) * 32.0f);
    return true;
}

namespace {

// The cells of a column whose road is `top` 32nds up: the lowest and the
// highest, from the origin's level or the road's own cell if that is lower.
void column_cells(int top, int &lo, int &hi)
{
    hi = (int)std::floor((float)(top - 1) / 32.0f);
    lo = std::min(0, hi);
}

} // namespace

void piece_triangles(const PieceShape &p, uint8_t facing, std::vector<PieceTri> &out)
{
    out.clear();
    if (!piece_valid(p)) return;

    const float len_nodes = piece_len(p) * (float)kPieceUnit;
    // Three to the node round a loop: its top bends a kart through a radian
    // in three nodes, and a wheel rolling over the corners felt each one.
    const int   nu        = std::max(8, (int)std::ceil(len_nodes * (looped(p) ? 3.0f : 1.0f)));
    // A road with a back: a wall ride's and a loop's are seen from behind.
    const bool  backed    = walled(p) || looped(p);

    // A wall ride has no curbs: the road curls into the wall that holds it.
    const bool         curbs = p.curbs && !backed;
    // An adapter's curb is as wide at its narrow end: where it starts, as a
    // share of the half width, goes with u (cf below is its wide end's).
    const float        cf    = curb_from(p);
    std::vector<float> vs;
    if (curbs)
        vs = {-1.0f, -cf, -0.5f, 0.0f, 0.5f, cf, 1.0f};
    else if (backed)
        vs = {-1.0f, -0.5f, 0.0f, 0.5f, 1.0f};
    else
        vs = {-1.0f, 0.0f, 1.0f};
    const int nv = (int)vs.size();

    // Which way the road faces at u: up, or for a wall ride rolled with it --
    // what the winding of each triangle is judged by, a wall's face being no
    // more up than down.
    float bw, bl;
    base_size(p, bw, bl);
    const auto facing_at = [&](float u) {
        float hx = 0.0f, hz = 0.0f, hy = 1.0f;   // unturned
        if (walled(p)) {
            const float a = roll_at(p, u);
            hx = -std::sin(a);
            hy = std::cos(a);
        } else if (looped(p)) {   // the heading turned a quarter back
            float s, z, y, a;
            loop_pose(p, u, s, z, y, a);
            hz = -std::sin(a);
            hy = std::cos(a);
        } else {
            return Vec3{0.0f, 1.0f, 0.0f};
        }
        float x0 = 4.0f, z0 = 4.0f, x1 = 4.0f + hx, z1 = 4.0f + hz;
        turn(facing, bw, bl, x0, z0);
        turn(facing, bw, bl, x1, z1);
        return Vec3{x1 - x0, hy, z1 - z0};
    };
    const auto top = [&](int i, int j) {
        const float u = (float)i / (float)nu;
        float       v = vs[(size_t)j];
        // An adapter's curb keeps its width in nodes as the road narrows.
        if (tapered(p) && curbs && std::fabs(v) == cf) v = std::copysign(curb_from(p, u), v);
        return at(p, facing, u, v, height(p, u, v));
    };
    // Down to the origin's floor, or a road's depth under a low edge below it.
    const auto floor_of = [](Vec3 a) { return Vec3{a.x, std::min(0.0f, a.y - kPieceBase), a.z}; };

    for (int i = 0; i < nu; ++i) {
        const Vec3 up = facing_at(((float)i + 0.5f) / (float)nu);
        for (int j = 0; j + 1 < nv; ++j) {
            const bool    curb = curbs && (j == 0 || j == nv - 2);
            const uint8_t role = curb ? kPieceCurb : kPieceRoad;
            quad(out, top(i, j), top(i + 1, j), top(i + 1, j + 1), top(i, j + 1), up, role,
                 (uint8_t)(i & 1));
            // A wall ride's back, drawn and not driven on: seen from behind,
            // a wall is still there.
            if (backed)
                quad(out, top(i, j), top(i + 1, j), top(i + 1, j + 1), top(i, j + 1), up * -1.0f,
                     kPieceSide);
        }
        if (backed) continue;
        // The sides, down to the origin cell's floor: drawn, not driven on.
        for (int j : {0, nv - 1}) {
            const Vec3 a = top(i, j), b = top(i + 1, j);
            const Vec3 in = top(i, nv - 1 - j);
            const Vec3 outward{a.x - in.x, 0.0f, a.z - in.z};
            quad(out, a, b, floor_of(b), floor_of(a), outward, kPieceSide);
        }
    }
    // And the two ends.
    for (int i : {0, nu}) {
        if (backed) break;
        const Vec3 a = top(i, 0), n = top(i == 0 ? 1 : nu - 1, 0);
        const Vec3 outward{a.x - n.x, 0.0f, a.z - n.z};
        for (int j = 0; j + 1 < nv; ++j) {
            const Vec3 b = top(i, j), c = top(i, j + 1);
            quad(out, b, c, floor_of(c), floor_of(b), outward, kPieceSide);
        }
    }
}

void piece_heights(const PieceShape &p, uint8_t facing, float &lo, float &hi)
{
    std::vector<PieceTri> tris;
    piece_triangles(p, facing, tris);
    lo = 0.0f;
    hi = 1.0f;
    for (const PieceTri &t : tris)
        for (const Vec3 &v : t.v) {
            lo = std::min(lo, v.y);
            hi = std::max(hi, v.y);
        }
}

// The middle of a piece's entry square from its origin, turned by `facing`.
static void entry_point(const PieceShape &p, uint8_t facing, float &ex, float &ez)
{
    float w, l;
    base_size(p, w, l);
    // The middle of the entry square, unturned: a straight's first, a turn's
    // square on the +x side of its first row.
    // The entry's RIGHT lane, on a wide road: a right turn's inner one, a left
    // turn's outer one.
    // A loop to the right is entered on the lane left of where it comes out.
    ex = (straight(p) || p.plan == 4 || p.plan == 5 || p.plan == 6 ? 0.5f
                : p.plan == 3                                            ? 1.5f
                : p.plan == 7 ? (float)std::max<int>(p.width, 1) + 0.5f
                              : (float)(p.size - std::max<int>(p.width, 1)) + 0.5f) *
               (float)kPieceUnit;
    ez = 0.5f * (float)kPieceUnit;
    turn(facing, w, l, ex, ez);
}

void piece_origin(const PieceShape &p, uint8_t facing, int x, int z, int &ox, int &oz)
{
    float ex, ez;
    entry_point(p, facing, ex, ez);
    const auto grid = [](int n) { return (int)std::floor((float)n / (float)kPieceUnit) * kPieceUnit; };
    ox = grid(x) - (int)std::floor(ex / (float)kPieceUnit) * kPieceUnit;
    oz = grid(z) - (int)std::floor(ez / (float)kPieceUnit) * kPieceUnit;
}

void piece_entry(const PieceShape &p, uint8_t facing, int ox, int oz, int &x, int &z)
{
    float ex, ez;
    entry_point(p, facing, ex, ez);
    x = ox + (int)std::floor(ex / (float)kPieceUnit) * kPieceUnit;
    z = oz + (int)std::floor(ez / (float)kPieceUnit) * kPieceUnit;
}

void piece_cells(const PieceShape &p, BlockId anchor, uint8_t facing, int ox, int oy, int oz,
                 std::vector<PieceCell> &out)
{
    out.clear();
    out.push_back(PieceCell{ox, oy, oz, anchor, (uint8_t)(facing & 3)});

    int nx, nz;
    piece_size(p, facing, nx, nz);
    for (int cz = 0; cz < nz; ++cz)
        for (int cx = 0; cx < nx; ++cx) {
            int top, lo, hi;
            if (!piece_column(p, facing, cx, cz, top)) continue;
            column_cells(top, lo, hi);
            for (int cy = lo; cy <= hi; ++cy) {
                if (cx == 0 && cy == 0 && cz == 0) continue;   // the anchor
                const int full = std::clamp(top - cy * 32, 1, 32);
                out.push_back(PieceCell{ox + cx, oy + cy, oz + cz, p.fill, (uint8_t)full});
            }
        }

    // A wall ride standing up has nothing under it to stand in a column, and
    // a body on it would be over nothing that is the track. Where it stands
    // straight up its face is on the line between two columns (the road's
    // middle, 4 over), so the cells just behind it are all behind the road
    // and none of them in front of it: full ones, from its foot to its top.
    if (walled(p)) {
        float w, l;
        base_size(p, w, l);
        const int n = p.size * kPieceUnit;
        for (int cz = 0; cz < n; ++cz) {
            const float a = roll_at(p, ((float)cz + 0.5f) / (float)n);
            if (std::fabs(a) < 1.54f) continue;   // within two degrees of standing
            const float bx = a > 0.0f ? 4.5f : 3.5f;   // behind a face turned right, or left
            float x = bx, z = (float)cz + 0.5f;
            turn(facing, w, l, x, z);
            const int tx = (int)std::floor(x), tz = (int)std::floor(z);
            for (int cy = -(kPieceUnit / 2); cy < kPieceUnit / 2; ++cy)
                out.push_back(PieceCell{ox + tx, oy + cy, oz + tz, p.fill, 32});
        }
    }
}

// A piece's own block, or the fill every piece's other cells are (it drops as
// a piece, piece_declare).
static bool is_track(const NodeRegistry &reg, BlockId b)
{
    const NodeDef &d = reg.get(b);
    return d.piece.plan != 0 || (d.drops_as != kAir && reg.get(d.drops_as).piece.plan != 0);
}

bool piece_has_room(const World &world, const NodeRegistry &reg, const std::vector<PieceCell> &cells)
{
    for (const PieceCell &c : cells)
        if (is_track(reg, world.get_block(c.x, c.y, c.z))) return false;
    return true;
}

void piece_headroom(const World &world, const NodeRegistry &reg, const std::vector<PieceCell> &cells,
                    std::vector<PieceCell> &out)
{
    // The top cell of each of its columns, and the cells over it.
    std::map<std::pair<int, int>, int> top;
    for (const PieceCell &c : cells) {
        auto [it, fresh] = top.try_emplace({c.x, c.z}, c.y);
        if (!fresh) it->second = std::max(it->second, c.y);
    }
    for (const auto &[xz, y] : top)
        for (int up = 1; up <= kPieceHeadroom; ++up) {
            const BlockId had = world.get_block(xz.first, y + up, xz.second);
            if (had != kAir && !reg.is_liquid(had) && !is_track(reg, had))
                out.push_back(PieceCell{xz.first, y + up, xz.second, kAir, 0});
        }
}

bool piece_declare(NodeRegistry &reg, BlockId id, BlockId fill, PieceShape p)
{
    if (id == kAir || fill == kAir || id == fill || id >= reg.size() || fill >= reg.size()) return false;
    p.fill = fill;
    if (!piece_valid(p)) return false;

    NodeDef d = reg.get(id);
    d.piece   = p;
    d.turn    = NodeTurn::Upright;
    reg.register_node(d);

    NodeDef f  = reg.get(fill);
    f.turn     = NodeTurn::Height;
    f.piece    = PieceShape{};
    f.drops_as = id;   // no item of its own; breaking one is breaking the piece
    reg.register_node(f);
    return true;
}

bool piece_find(const World &world, const NodeRegistry &reg, int x, int y, int z, int &ox,
                int &oy, int &oz)
{
    const auto grid = [](int n) { return (int)std::floor((float)n / (float)kPieceUnit) * kPieceUnit; };
    for (int gz = 0; gz < kPieceMaxUnits; ++gz)
        for (int gx = 0; gx < kPieceMaxUnits; ++gx)
            for (int cy = -kPieceBelow; cy <= kPieceMaxHeight; ++cy) {
                const int      ax = grid(x) - gx * kPieceUnit, ay = y - cy,
                               az = grid(z) - gz * kPieceUnit;
                const NodeDef &d  = reg.get(world.get_block(ax, ay, az));
                if (d.piece.plan == 0) continue;

                const uint8_t f = world.get_facing(ax, ay, az);
                int           nx, nz;
                piece_size(d.piece, f, nx, nz);
                const int lx = x - ax, lz = z - az;
                if (lx >= nx || lz >= nz) continue;
                const bool is_anchor = lx == 0 && lz == 0 && cy == 0;
                int        top, lo, hi;
                if (!is_anchor && walled(d.piece)) {
                    // Its cells are not all in columns under its road.
                    std::vector<PieceCell> cells;
                    piece_cells(d.piece, kAir, f, ax, ay, az, cells);
                    bool mine = false;
                    for (const PieceCell &c : cells) mine |= c.x == x && c.y == y && c.z == z;
                    if (!mine) continue;
                } else if (!is_anchor) {
                    if (!piece_column(d.piece, f, lx, lz, top)) continue;
                    column_cells(top, lo, hi);
                    if (cy < lo || cy > hi) continue;
                }
                ox = ax;
                oy = ay;
                oz = az;
                return true;
            }
    return false;
}

} // namespace cnp
