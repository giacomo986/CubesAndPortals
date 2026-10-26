#include "core/physics/movement.h"

#include "core/physics/aabb.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace cnp {
namespace {

// The player's box, in blocks. Taken from physics/aabb.h rather than restated
// here, and that is not tidiness: this file decides which positions the server
// accepts, aabb.h decides which positions the client's collision produces, and
// two constants that merely happen to agree are two constants that can stop
// agreeing in one edit. When they disagree the server refuses positions the
// player reached legally, which is the worst failure this code has.
// The width is the world's (MovementRules::width) and the height
// MovementLimits::height (kPlayerHeight, or a parkour world's slide), and both
// arrive as parameters.

// How far above the top of a jump a body that cannot fly may be claimed before
// it is refused: float settling, and a claim sampled a little late at the apex.
constexpr float kRiseSlack = 0.3f;

// The box this function is asking about, with its own margins in it -- see the
// two paragraphs below for why they are what they are. It exists because a
// block may be a SHAPE rather than a cell (registry/node_shape.h), and asking
// "is this cell solid" about a slab answers for a block that is not there.
AABB probe_box(float x, float y, float z, float half, float height)
{
    return AABB{Vec3{x - half, y + 0.05f, z - half},
                Vec3{x + half - kBoxSkin, y + height - 0.05f, z + half - kBoxSkin}};
}

// Whether one whole node lies inside one of the exemption boxes. All of it, or
// none of it: half an exempt node would open the wall the other half is part of.
//
// Stated rather than implied: every opening this engine builds today is node
// aligned, so containment and mere overlap give the same answer for all of
// them, and a sabotage that swaps one for the other changes nothing. The rule
// is written this way for the openings that are not -- half a node of
// exemption is not half a node of passable stone.
bool node_exempt(int bx, int by, int bz, const SolidExemption *exempt, size_t count)
{
    for (size_t i = 0; i < count; ++i) {
        const SolidExemption &e = exempt[i];
        if ((float)bx       < e.min_x || (float)(bx + 1) > e.max_x) continue;
        if ((float)by       < e.min_y || (float)(by + 1) > e.max_y) continue;
        if ((float)bz       < e.min_z || (float)(bz + 1) > e.max_z) continue;
        return true;
    }
    return false;
}

} // namespace

bool inside_solid(const World &world, const NodeRegistry &registry, float x, float y, float z)
{
    return inside_solid(world, registry, x, y, z, nullptr, 0);
}

bool inside_solid(const World &world, const NodeRegistry &registry, float x, float y, float z,
                  const SolidExemption *exempt, size_t exempt_count, float half_width,
                  float height)
{
    // The far face is EXCLUSIVE, exactly as box_hits_world has it: a box whose
    // edge lands on a node boundary touches that node without entering it.
    //
    // This used to round the far edge in, and the two rules disagreed by one
    // node for one position in ten -- every position where a coordinate landed
    // on a boundary. The player could walk there, because collision let them,
    // and then anything asking THIS function was told they were inside stone.
    // A portal crossing was refused for it: walk into an opening a fifth of a
    // node off centre and the arrival, a fifth of a node off centre at the
    // other end, has its box edge exactly on the wall beside the hole. The
    // player walks through and comes out the far side of the same wall, having
    // been told nothing. Reported from play as "ogni tanto il teleport
    // fallisce"; the comment above this function had predicted the shape of it.
    const int x0 = (int)std::floor(x - half_width);
    const int x1 = (int)std::floor(x + half_width - kBoxSkin);
    const int z0 = (int)std::floor(z - half_width);
    const int z1 = (int)std::floor(z + half_width - kBoxSkin);

    // From the feet up, not including the very top of the head: standing with
    // your scalp brushing a ceiling is legal, and rounding it in would refuse
    // every player who jumps in a two-high tunnel.
    //
    // The 0.05 is deliberately more forgiving than the skin above, and stays.
    // It is what lets a position that has settled a hair into the floor be
    // accepted -- the collision resolver leaves the box kBoxSkin short of what
    // it lands on, and a client that reports its feet after landing is inside
    // that margin. The horizontal has no such settling and needs no such
    // slack.
    const int y0 = (int)std::floor(y + 0.05f);
    const int y1 = (int)std::floor(y + height - 0.05f);

    const AABB probe = probe_box(x, y, z, half_width, height);

    // A door's panel is not "inside": a door shut on whoever stood in its
    // cell leaves them overlapping it, and the resolver lets them walk out
    // (node_blocks_box's `was`) -- refused here, they would be held there.
    for (int bz = z0; bz <= z1; ++bz)
        for (int by = y0; by <= y1; ++by)
            for (int bx = x0; bx <= x1; ++bx)
                if (node_blocks_box(world, registry, bx, by, bz, probe) &&
                    registry.get(world.get_block(bx, by, bz)).door == 0 &&
                    !node_exempt(bx, by, bz, exempt, exempt_count))
                    return true;

    return false;
}

MovementVerdict check_move(const MovementState &last, float x, float y, float z, double now,
                           const World &world, const NodeRegistry &registry,
                           const MovementLimits &limits)
{
    return check_move(last, x, y, z, now, world, registry, limits, nullptr, 0);
}

MovementVerdict check_move(const MovementState &last, float x, float y, float z, double now,
                           const World &world, const NodeRegistry &registry,
                           const MovementLimits &limits, const SolidExemption *exempt,
                           size_t exempt_count)
{
    MovementVerdict v;

    // Inside a wall is refused whatever the speed was. Checked first because it
    // is the one that does not depend on history: a player who was never placed
    // still cannot be told they are inside stone.
    if (limits.refuse_inside_solid &&
        inside_solid(world, registry, x, y, z, exempt, exempt_count, limits.solid_half,
                     limits.height)) {
        v.accepted = false;
        v.x = last.valid ? last.x : x;
        v.y = last.valid ? last.y : y;
        v.z = last.valid ? last.z : z;
        // Where, in the sentence: a refusal that does not say which position
        // is one nobody can check, and the width is the world's.
        char where[128];
        std::snprintf(where, sizeof where, " (%.2f %.2f %.2f, %.1f wide)", (double)x, (double)y,
                      (double)z, (double)(limits.solid_half * 2.0f));
        v.reason   = std::string("that position is inside a solid block") + where;
        v.budget_h = last.budget_h;   // refused, so nothing was spent
        v.budget_v = last.budget_v;
        return v;
    }

    // The first claim from a player has nothing to be compared against. Taken
    // as given: they have to start somewhere, and the spawn is the server's
    // idea anyway once there is one.
    if (!last.valid) {
        v.accepted = true;
        v.x = x; v.y = y; v.z = z;

        // A player who has just arrived starts with a full budget rather than
        // an empty one. Empty would mean the first claim after the spawn is
        // measured against nothing and refused, which is a correction for
        // existing.
        v.budget_h = limits.max_horizontal * limits.tolerance * (float)limits.max_burst;
        v.budget_v = limits.max_vertical * limits.tolerance * (float)limits.max_burst;
        v.ground_y = y;
        return v;
    }

    // Clamped at both ends. A negative interval means the clock went backwards
    // or two claims arrived in the same tick, and either way no time passed and
    // nothing is added to what this player has in hand.
    double dt = now - last.when;
    if (dt < 0.0) dt = 0.0;
    if (dt > limits.max_interval) dt = limits.max_interval;

    const float dx = x - last.x;
    const float dy = y - last.y;
    const float dz = z - last.z;

    const float horizontal = std::sqrt(dx * dx + dz * dz);
    const float vertical   = std::fabs(dy);

    // What this claim is measured against: a budget filled at the allowed speed
    // over real time and capped, not a rate over the interval between two
    // arrivals. See MovementState::budget_h for the defect that produced.
    const float rate_h = limits.max_horizontal * limits.tolerance;
    const float rate_v = limits.max_vertical * limits.tolerance;
    const float cap_h  = rate_h * (float)limits.max_burst;
    const float cap_v  = rate_v * (float)limits.max_burst;

    float allowance_h = last.budget_h + rate_h * (float)dt;
    float allowance_v = last.budget_v + rate_v * (float)dt;
    if (allowance_h > cap_h) allowance_h = cap_h;
    if (allowance_v > cap_v) allowance_v = cap_v;

    // The grace the server granted when it moved this player itself. It covers
    // the jump AND the speed carried out of it, which are two different
    // failures of the same check -- see MovementLimits::portal_grace. Read from
    // the last accepted state, so it survives however many claims arrive inside
    // the window, and it expires by the clock rather than by being spent.
    //
    // Everything below the speed checks still runs. A graced player who claims
    // a position inside a block is refused like anybody else, and that is the
    // rule that stops a grace being a licence rather than an allowance.
    const bool graced = now <= last.grace_until;

    if (!graced && horizontal > allowance_h) {
        v.accepted = false;
        v.x = last.x; v.y = last.y; v.z = last.z;
        v.budget_h = allowance_h;   // refused, so nothing was spent
        v.budget_v = allowance_v;
        v.reason = "moved " + std::to_string(horizontal) + " blocks sideways in " +
                   std::to_string(dt) + "s, the limit is " + std::to_string(allowance_h);
        return v;
    }

    if (!graced && vertical > allowance_v) {
        v.accepted = false;
        v.x = last.x; v.y = last.y; v.z = last.z;
        v.budget_h = allowance_h;
        v.budget_v = allowance_v;
        v.reason = "moved " + std::to_string(vertical) + " blocks vertically in " +
                   std::to_string(dt) + "s, the limit is " + std::to_string(allowance_v);
        return v;
    }

    // How HIGH, for a body that cannot fly. The vertical budget above is a
    // speed, and a climb at jump speed fits it for as long as a client cares to
    // claim one: in a world that drives, that is a kart over the walls. So the
    // height is measured from where the body last stood -- or from where the
    // server last put it -- and a body on nothing may be at most the top of a
    // jump above that. A body standing on something is where it stands, which
    // is what lets a step, a slab and a landing on a higher block through.
    const float half   = limits.half_width;
    const AABB  below{Vec3{x - half, y - 0.1f, z - half}, Vec3{x + half, y, z + half}};
    // Or in a liquid: a body swimming up out of a lake is held up by the lake,
    // and rises as far as the keys lift it.
    // Or on a ladder, which holds a body wherever it is on it; or by its
    // hands at a ledge, hanging or pulling itself up over it.
    const bool stands = box_hits_world(world, registry, below) ||
                        box_in_liquid(world, registry, body_box(Vec3{x, y, z}, half)) ||
                        box_on_climbable(world, registry, body_box(Vec3{x, y, z}, half)) ||
                        body_at_ledge(world, registry, Vec3{x, y, z}, half);
    const float stood  = last.placed ? last.y : last.ground_y;

    if (!graced && limits.rise >= 0.0f && !stands && y > stood + limits.rise + kRiseSlack) {
        v.accepted = false;
        v.x = last.x; v.y = last.y; v.z = last.z;
        v.budget_h = allowance_h;   // refused, so nothing was spent
        v.budget_v = allowance_v;
        v.ground_y = last.ground_y;
        char buf[160];
        std::snprintf(buf, sizeof buf,
                      "%.2f above where it last stood, standing on nothing, and a body here "
                      "cannot fly: a jump reaches %.2f", (double)(y - stood),
                      (double)limits.rise);
        v.reason = buf;
        return v;
    }
    // A body that may fly is held up wherever it is: when it may no longer
    // (core.player_fly, a flight pack taken off in the air) its fall starts
    // from there, and not from where it took off.
    v.ground_y = (stands || graced || limits.rise < 0.0f) ? y : stood;

    // What the claim went THROUGH (TODO.md OPEN-46). Everything above judges
    // where a claim ENDS, so a claim on the far side of a wall was accepted as
    // long as the distance fitted the budget -- and a world that declares
    // 256 m/s makes that distance 12.8 nodes a claim.
    //
    // Not a straight line, which would refuse honest players: a body rounding
    // the end of a wall between two claims, or stepping up a stair, goes where
    // the straight line between its two positions does not. So the server walks
    // the claim with the client's own resolver, five ways -- the delta as one
    // move, the two L-shaped ways round, across and then down, up and then
    // across -- and accepts it if any of them arrives. "Arrives" is within kMaxStep on every axis, because move_body
    // stops up to one step short of what it runs into; and that is not slack a
    // wall can hide in, because a body on the far side of any box, however thin,
    // is at least six tenths from where a resolver on the near side stops.
    //
    // A claim no longer than kMaxStep on every axis passes that test standing
    // still, so it is not walked -- which is every claim of a player on foot.
    if (!graced && !last.placed && limits.refuse_inside_solid) {
        constexpr float kPathSlack = 0.05f;   // the skin, and a step-up's settling

        const Vec3  from{last.x, last.y, last.z};
        const Vec3  to{x, y, z};
        const Vec3  d       = to - from;
        const float longest = std::max({std::fabs(d.x), std::fabs(d.y), std::fabs(d.z)});

        // A node holding a floor portal up is not a wall to the body sinking
        // through it, and the resolver does not know that. A claim whose sweep
        // comes near one is judged by where it ends, as every claim was before.
        bool near_exemption = false;
        for (size_t i = 0; i < exempt_count && !near_exemption; ++i) {
            const SolidExemption &e = exempt[i];
            near_exemption =
                std::min(from.x, to.x) - 1.0f < e.max_x && std::max(from.x, to.x) + 1.0f > e.min_x &&
                std::min(from.y, to.y) - 1.0f < e.max_y && std::max(from.y, to.y) + 2.0f > e.min_y &&
                std::min(from.z, to.z) - 1.0f < e.max_z && std::max(from.z, to.z) + 1.0f > e.min_z;
        }

        if (longest > kMaxStep && !near_exemption) {
            const auto arrives = [&](Vec3 p) {
                const float slack = kMaxStep + kPathSlack;
                return std::fabs(p.x - to.x) <= slack && std::fabs(p.y - to.y) <= slack &&
                       std::fabs(p.z - to.z) <= slack;
            };

            // The world's body, not the engine's: a kart is walked as wide as
            // the client moved it.
            const auto walk = [&](Vec3 at, Vec3 by) {
                return move_body(world, registry, at, by, 0.0f, kStepHeight, limits.half_width,
                                 limits.height).pos;
            };

            bool reached = arrives(walk(from, d));
            if (!reached) {
                const Vec3 a = walk(from, Vec3{d.x, d.y, 0.0f});
                reached = arrives(walk(a, Vec3{0.0f, 0.0f, to.z - a.z}));
            }
            if (!reached) {
                const Vec3 b = walk(from, Vec3{0.0f, d.y, d.z});
                reached = arrives(walk(b, Vec3{to.x - b.x, 0.0f, 0.0f}));
            }

            // Across and then down, and up and then across. A body walking off
            // a ledge has to be out from over it before it can drop, and one
            // climbing has to rise before it can move over -- and the three
            // walks above all move vertically in the same steps as, or before,
            // the horizontal. four_clients found the first: a walker stepping
            // off a dirt column, a quarter of a node sideways and a whole node
            // down in one claim, was refused by every walk, because each tried
            // to drop while its box was still over the column.
            if (!reached) {
                const Vec3 h = walk(from, Vec3{d.x, 0.0f, d.z});
                reached = arrives(walk(h, Vec3{0.0f, to.y - h.y, 0.0f}));
            }
            if (!reached) {
                const Vec3 u = walk(from, Vec3{0.0f, d.y, 0.0f});
                reached = arrives(walk(u, Vec3{to.x - u.x, 0.0f, to.z - u.z}));
            }

            if (!reached) {
                v.accepted = false;
                v.x = last.x; v.y = last.y; v.z = last.z;
                v.budget_h = allowance_h;   // refused, so nothing was spent
                v.budget_v = allowance_v;
                // Both positions in the sentence, like the speed refusals carry
                // their distance: a refusal about a path that does not say which
                // path is one nobody can check -- four_clients' was read off
                // this line.
                char where[160];
                std::snprintf(where, sizeof where, " (%.2f %.2f %.2f to %.2f %.2f %.2f)",
                              (double)from.x, (double)from.y, (double)from.z, (double)to.x,
                              (double)to.y, (double)to.z);
                v.reason = std::string("went through something solid: no way of walking, "
                                       "sliding or stepping from the last position reaches "
                                       "this one") +
                           where;
                return v;
            }
        }
    }

    v.accepted = true;
    v.x = x; v.y = y; v.z = z;

    // Spent. A graced claim spends nothing, because a server that moved this
    // player itself has not been told anything about how fast they can travel
    // -- charging a portal crossing to the player's budget would leave them
    // unable to walk for a second after arriving.
    v.budget_h = graced ? cap_h : allowance_h - horizontal;
    v.budget_v = graced ? cap_v : allowance_v - vertical;
    if (v.budget_h < 0.0f) v.budget_h = 0.0f;
    if (v.budget_v < 0.0f) v.budget_v = 0.0f;
    return v;
}

std::string movement_rules_problem(const MovementRules &rules)
{
    const struct {
        const char *name;
        float       value;
    } speeds[] = {{"walk", rules.walk},
                  {"sprint", rules.sprint},
                  {"fly", rules.fly},
                  {"jump", rules.jump}};

    char buf[160];
    for (const auto &s : speeds) {
        if (!std::isfinite(s.value)) return std::string(s.name) + " is not a number";
        if (s.value < 0.0f) {
            std::snprintf(buf, sizeof buf, "%s is %.2f m/s, and a speed is not negative", s.name,
                          (double)s.value);
            return buf;
        }
        if (s.value > kMaxDeclaredSpeed) {
            std::snprintf(buf, sizeof buf, "%s is %.2f m/s, and a world may declare at most %.0f",
                          s.name, (double)s.value, (double)kMaxDeclaredSpeed);
            return buf;
        }
    }

    const struct {
        const char *name;
        float       value;
        float       most;
        const char *unit;
    } drive[] = {{"accel", rules.accel, kMaxDeclaredAccel, "m/s^2"},
                 {"brake", rules.brake, kMaxDeclaredAccel, "m/s^2"},
                 {"turn", rules.turn, kMaxDeclaredTurn, "rad/s"},
                 {"drag", rules.drag, kMaxDeclaredDrag, "1/s"}};
    for (const auto &d : drive) {
        if (!std::isfinite(d.value)) return std::string(d.name) + " is not a number";
        if (d.value < 0.0f || d.value > d.most) {
            std::snprintf(buf, sizeof buf, "%s is %.2f %s, and a world may declare 0 to %.2f",
                          d.name, (double)d.value, d.unit, (double)d.most);
            return buf;
        }
    }

    if (!std::isfinite(rules.width)) return "width is not a number";
    if (rules.width < kMinBodyWidth || rules.width > kMaxBodyWidth) {
        std::snprintf(buf, sizeof buf, "width is %.2f nodes, and a world may declare %.1f to %.1f",
                      (double)rules.width, (double)kMinBodyWidth, (double)kMaxBodyWidth);
        return buf;
    }

    // 0 is as long as it is wide; otherwise no shorter than the thinnest body
    // and no longer than two of the widest, which a portal still takes.
    if (!std::isfinite(rules.length)) return "length is not a number";
    if (rules.length != 0.0f &&
        (rules.length < kMinBodyWidth || rules.length > 2.0f * kMaxBodyWidth)) {
        std::snprintf(buf, sizeof buf, "length is %.2f nodes, and a world may declare 0 or %.1f to %.1f",
                      (double)rules.length, (double)kMinBodyWidth, (double)(2.0f * kMaxBodyWidth));
        return buf;
    }

    // Negative is "the top of the jump"; a height a world declares is at most
    // what a body could be streamed ground under.
    if (!std::isfinite(rules.rise)) return "rise is not a number";
    if (rules.rise > kMaxDeclaredRise) {
        std::snprintf(buf, sizeof buf, "rise is %.2f nodes, and a world may declare at most %.0f",
                      (double)rules.rise, (double)kMaxDeclaredRise);
        return buf;
    }

    // Inside the body: a camera over its head would look down on the box it
    // collides with, and one at its feet would see under every step.
    if (!std::isfinite(rules.eye)) return "eye is not a number";
    if (rules.eye < 0.2f || rules.eye > kPlayerHeight) {
        std::snprintf(buf, sizeof buf, "eye is %.2f nodes, and a world may declare 0.2 to %.1f",
                      (double)rules.eye, (double)kPlayerHeight);
        return buf;
    }

    if (!std::isfinite(rules.gravity)) return "gravity is not a number";
    // Upwards is refused and strength is not. A fall is bounded by the terminal
    // speed whatever pulls it, and a rise is bounded by nothing: a body under
    // upward gravity gains speed until it meets a ceiling or leaves the world.
    if (rules.gravity > 0.0f) {
        std::snprintf(buf, sizeof buf, "gravity is %.2f m/s^2, which is upwards",
                      (double)rules.gravity);
        return buf;
    }
    return {};
}

MovementLimits limits_for(const MovementRules &rules)
{
    MovementLimits limits;

    // The fastest a body under these rules moves by its own keys. In the air
    // the keys never take it past the sprint (kAirAccelerate), so the ground
    // speeds and flying at a sprint are all there is.
    //
    // A body that drives does not fly (core/physics/player.cpp, drive_update), so its
    // fastest is its top speed forwards or back -- not the flight at a sprint
    // that would add 28 m/s to every driven body's allowance.
    limits.max_horizontal = rules.drives()
                                ? std::max(rules.walk, rules.sprint * kNitroTop)
                                : std::max({rules.walk, rules.sprint, rules.fly * kFlySprintFactor});

    // What is NOT checked for a body that drives: how fast its speed grows, and
    // how fast it turns. Tried, and taken out: the server sees when a claim
    // ARRIVES, and a budget built on arrival times let a claim of 0 to 24 m/s in
    // one go through -- 0 refused of 140, the same distance as on foot -- while
    // correcting an honest driver 4 times in 140 claims on a link with 120 ms of
    // jitter (synthetic claims, TODO.md OPEN-47). Inertia wants the client's own
    // send time in the claim.

    // Upwards a jump or a flight, downwards a fall -- which is capped at the
    // terminal speed on the client whatever the gravity, so the gravity itself
    // is not a limit here.
    limits.max_vertical = rules.drives()
                              ? std::max(kTerminalFallSpeed, rules.jump)
                              : std::max({kTerminalFallSpeed, rules.jump, rules.fly * kFlySprintFactor});

    // A body that drives is a chassis that turns (core/physics/vehicle.h), and
    // the square the server checks has to fit inside it whichever way it faces:
    // a square as wide as the chassis sticks out past its sides at 45 degrees,
    // and would find a block beside a kart that is not touching it. The square
    // inside a width w at every angle is w / sqrt 2 across.
    limits.half_width = rules.width * 0.5f * (rules.drives() ? 0.70710678f : 1.0f);

    // And what "inside a solid block" is measured with: the same box on foot,
    // a point for a body that drives. See MovementLimits::solid_half.
    limits.solid_half = rules.drives() ? kBoxSkin : limits.half_width;
    limits.arrival_half =
        rules.drives() ? 0.5f * std::max(rules.width, rules.length) : rules.width * 0.5f;

    // The top of a jump, v^2 / 2g, for a body that cannot fly -- and for one
    // that drives, its chassis lifted off its wheels and some headroom over
    // that (kChassisLift). With no gravity a jump has no top, and height is not
    // something this can check.
    // Or what the world declared instead (MovementRules::rise).
    limits.rise = (rules.flies() || rules.gravity >= 0.0f) ? -1.0f
                  : rules.rise >= 0.0f
                      ? rules.rise
                      : rules.jump * rules.jump / (-2.0f * rules.gravity) +
                            (rules.drives() ? kChassisLift + kDriveHeadroom : 0.0f);

    // Parkour on foot: a swing faster than a sprint, kicks up between two
    // walls to any height, and a body slid under a block. ponytail: any height
    // and only the slide's box against walls, for anybody in such a world; a
    // claim checked against the move it says it is making if one is abused.
    if (rules.parkour && !rules.drives()) {
        limits.max_horizontal = std::max(limits.max_horizontal, kRopeTop);
        limits.rise           = -1.0f;
        limits.height         = kSlideHeight;
    }
    return limits;
}

} // namespace cnp
