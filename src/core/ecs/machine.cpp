#include "core/ecs/machine.h"

#include "core/ecs/components.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace cnp::ecs {

namespace {

// Puts one mounted thing where it rides, from its carrier as it is now.
bool ride_on(flecs::world &w, flecs::entity e, const Mounted &m)
{
    flecs::entity carrier = w.get_alive(m.on);
    if (!carrier.is_valid()) return false;
    const Position *cp = carrier.try_get<Position>();
    const Look     *cl = carrier.try_get<Look>();
    if (cp == nullptr || cl == nullptr) return false;
    const Roll *cr   = carrier.try_get<Roll>();
    const float roll = cr ? cr->roll : 0.0f;

    const Vec3 at = craft_point(Vec3{cp->x, cp->y, cp->z}, cl->yaw, cl->pitch, roll, m.at);
    Position  &p  = e.get_mut<Position>();
    p.x = at.x;
    p.y = at.y;
    p.z = at.z;
    p.valid = true;
    // Turned with the carrier and then by its own aim; the carrier's tip, as
    // much of it as lies along the way it points.
    Look &l  = e.get_mut<Look>();
    l.yaw    = cl->yaw + m.yaw;
    l.pitch  = m.pitch + cl->pitch * std::cos(m.yaw) - roll * std::sin(m.yaw);
    e.set<Roll>({roll * std::cos(m.yaw) + cl->pitch * std::sin(m.yaw)});
    return true;
}

// A machine's hull across the ground: its middle, its right and its front,
// half its width and length; and from what height to what height it stands.
struct Footprint {
    Vec3  mid, right, front;
    float half_w = 0.0f, half_l = 0.0f, bottom = 0.0f, top = 0.0f;
};

Footprint footprint(const Machine &m)
{
    const CraftSpec &s = craft_spec(*m.sim);
    const float      y = m.pose.yaw;
    Footprint        f;
    f.right  = Vec3{std::cos(y), 0.0f, std::sin(y)};
    f.front  = Vec3{std::sin(y), 0.0f, -std::cos(y)};
    f.mid    = m.pose.base;
    f.half_w = s.half_w;
    f.half_l = s.half_l;
    const float mid_y = m.pose.base.y + craft_ride(*m.sim);
    f.bottom = mid_y - s.half_h;
    f.top    = mid_y + s.half_h;
    return f;
}

// How far under its top feet may be and still be on a machine's roof: the
// client stands them on a hull it draws a little behind the server's.
constexpr float kRoof = 0.35f;

float dot_xz(Vec3 a, Vec3 b) { return a.x * b.x + a.z * b.z; }

// How far a's and b's footprints overlap along `axis`, 0 or less when apart.
float overlap_along(const Footprint &a, const Footprint &b, Vec3 axis)
{
    const auto reach = [&](const Footprint &f) {
        return f.half_w * std::fabs(dot_xz(f.right, axis)) + f.half_l * std::fabs(dot_xz(f.front, axis));
    };
    return reach(a) + reach(b) - std::fabs(dot_xz(b.mid - a.mid, axis));
}

// Two machines apart: along the axis they overlap least on, each gets the
// speed that makes them part at kPart, shared by mass as a knock would.
void part(Machine &a, Machine &b)
{
    constexpr float kPart = 1.5f;   // m/s apart once they touch
    const Footprint fa = footprint(a), fb = footprint(b);
    if (fa.top <= fb.bottom || fb.top <= fa.bottom) return;
    float best = 1e9f;
    Vec3  normal;
    for (const Vec3 axis : {fa.right, fa.front, fb.right, fb.front}) {
        const float o = overlap_along(fa, fb, axis);
        if (o <= 0.0f) return;
        if (o < best) best = o, normal = axis;
    }
    if (dot_xz(fb.mid - fa.mid, normal) < 0.0f) normal = normal * -1.0f;
    const float closing = dot_xz(b.pose.velocity - a.pose.velocity, normal);
    if (closing >= kPart) return;
    const float ma = craft_spec(*a.sim).mass, mb = craft_spec(*b.sim).mass;
    const float j  = kPart - closing;
    craft_push(*a.sim, normal * (-j * mb / (ma + mb)));
    craft_push(*b.sim, normal * (j * ma / (ma + mb)));
}

} // namespace

bool in_machine(flecs::world &w, Vec3 feet, float half_width, float height, Vec3 &out, Vec3 &machine_vel)
{
    bool found = false;
    w.each([&](const Machine &m) {
        if (found || !m.sim) return;
        const Footprint f = footprint(m);
        // Feet in its top third of a node are on it, as on_machine has them.
        if (feet.y >= f.top - kRoof || feet.y + height <= f.bottom) return;
        const Vec3  d  = feet - f.mid;
        const float lx = dot_xz(d, f.right), lz = dot_xz(d, f.front);
        const float px = f.half_w + half_width - std::fabs(lx);
        const float pz = f.half_l + half_width - std::fabs(lz);
        if (px <= 0.0f || pz <= 0.0f) return;
        out         = px < pz ? f.right * (lx < 0.0f ? -1.0f : 1.0f) : f.front * (lz < 0.0f ? -1.0f : 1.0f);
        machine_vel = m.pose.velocity;
        found       = true;
    });
    return found;
}

bool on_machine(flecs::world &w, Vec3 feet, float half_width)
{
    bool found = false;
    w.each([&](const Machine &m) {
        if (found || !m.sim) return;
        const Footprint f = footprint(m);
        if (feet.y < f.top - kRoof || feet.y > f.top + 0.8f) return;
        const float lag = 0.5f + 0.33f * std::hypot(m.pose.velocity.x, m.pose.velocity.z);
        const Vec3  d   = feet - f.mid;
        found = std::fabs(dot_xz(d, f.right)) < f.half_w + half_width + lag &&
                std::fabs(dot_xz(d, f.front)) < f.half_l + half_width + lag;
    });
    return found;
}

void step_machines(flecs::world &w, const World &world, const NodeRegistry &reg, float dt)
{
    w.each([&](flecs::entity e, Machine &m, Position &p, Look &l) {
        if (!m.sim) return;
        craft_step(*m.sim, world, reg, m.keys, dt);
        m.pose   = craft_pose(*m.sim);
        m.impact = std::max(m.impact, m.pose.impact);
        p.x      = m.pose.base.x;
        p.y      = m.pose.base.y;
        p.z      = m.pose.base.z;
        l.yaw    = m.pose.yaw;
        l.pitch  = m.pose.pitch;
        e.set<Roll>({m.pose.roll});
    });

    std::vector<Machine *> all;
    w.each([&](Machine &m) {
        if (m.sim) all.push_back(&m);
    });
    for (size_t i = 0; i < all.size(); ++i)
        for (size_t k = i + 1; k < all.size(); ++k) part(*all[i], *all[k]);

    // What is mounted, carriers first: a barrel on a turret on a hull is put
    // after the turret it rides. Collected before it is moved, as mob.cpp's
    // reap_dead does, since setting a Roll may move an entity's table.
    std::vector<std::pair<flecs::entity_t, Mounted>> mounted;
    w.each([&](flecs::entity e, const Mounted &m) { mounted.emplace_back(e.id(), m); });
    std::vector<bool> done(mounted.size(), false);
    for (int pass = 0; pass < 4; ++pass)
        for (size_t i = 0; i < mounted.size(); ++i) {
            if (done[i]) continue;
            bool waits = false;   // its carrier is mounted too, and not put yet
            for (size_t k = 0; k < mounted.size(); ++k)
                if (!done[k] && k != i && mounted[k].first == mounted[i].second.on) waits = true;
            if (waits && pass < 3) continue;
            ride_on(w, w.entity(mounted[i].first), mounted[i].second);
            done[i] = true;
        }
}

} // namespace cnp::ecs
