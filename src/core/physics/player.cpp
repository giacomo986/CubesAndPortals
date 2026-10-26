#include "core/physics/player.h"

#include "core/physics/movement.h"

#include "core/physics/aabb.h"
#include "core/physics/vehicle.h"
#include "core/voxel/raycast.h"

#include <algorithm>
#include <cmath>

namespace cnp {
// The look speed and the pitch limit are in player.h. The movement speeds live
// in core/physics/movement.h, because the server checks claimed positions
// against them and two copies of "how fast is a sprint" is a disagreement
// waiting for its first cheater -- and they arrive here as the world's
// MovementRules, because a world may declare its own.

Vec3 Player::eye() const
{
    // Sliding, or under a block after a slide: low, over the feet.
    if (low) return Vec3{pos.x, pos.y + kSlideEye, pos.z};
    // Lying: at the head end, a hand over what it lies on.
    if (lying)
        return Vec3{lie_at.x + std::sin(lie_head) * 0.72f, lie_at.y + 0.32f,
                    lie_at.z - std::cos(lie_head) * 0.72f};
    return Vec3{pos.x, pos.y + eye_height * (riding ? kChuteEye : crouching ? kCrouchEye : 1.0f), pos.z};
}

Vec3 Player::forward() const
{
    const float cp = std::cos(pitch);
    return Vec3{cp * std::sin(yaw), std::sin(pitch), -cp * std::cos(yaw)};
}

void look_apply_roll(PlayerInput &input, float roll)
{
    if (std::fabs(roll) < 1e-4f) return;

    const float c = std::cos(roll), s = std::sin(roll);
    const float dx = input.look_dx, dy = input.look_dy;

    input.look_dx = dx * c - dy * s;
    input.look_dy = dx * s + dy * c;
}

// A wheel that turns less at speed: from half the top speed to the top speed it
// asks for less and less, down to kGripAtTop of `turn` -- 1.3 rad/s at the
// track's 24 m/s. It was the kart's only grip before the kart had tyres; now
// the tyres decide how much of it the kart takes, and past their grip it
// slides wide. Below 4 m/s the wheel turns in proportion to the speed, and not
// at all at rest.
constexpr float kGripLossFrom = 0.5f;   // of the top speed
constexpr float kGripAtTop    = 0.5f;   // of `turn`

float drive_turn_rate(const MovementRules &rules, float speed)
{
    const float v    = std::fabs(speed);
    float       grip = std::min(1.0f, v / 4.0f);

    const float from = rules.sprint * kGripLossFrom;
    if (v > from && rules.sprint > from)
        grip *= 1.0f - (1.0f - kGripAtTop) * std::min(1.0f, (v - from) / (rules.sprint - from));
    return rules.turn * grip;
}

namespace {

// The collision box this frame: the body's, or narrowed and lowered going
// into a wall portal (Player::cb_half, cb_top).
float box_half(const Player &p, const MovementRules &rules)
{
    const float half = rules.width * 0.5f;
    return p.cb_half > 0.0f ? std::min(p.cb_half, half) : half;
}
float box_height(const Player &p)
{
    const float h = std::max(0.3f, kPlayerHeight - p.cb_top);
    return p.low ? std::min(h, kSlideHeight) : h;
}

// Whether a wall is against the body on the side (dx, dz), within `gap`: the
// box a little off the floor and under the ceiling, so neither is the wall.
bool touches(const World &world, const NodeRegistry &reg, const Player &p, float half, int dx,
             int dz, float gap = 0.12f)
{
    const Vec3 at = p.pos + Vec3{(float)dx * gap, 0.1f, (float)dz * gap};
    return box_hits_world(world, reg, body_box(at, half, p.cb_shift, box_height(p) - 0.2f));
}


// Closes `v` on `want` at `k` per second, the same way at any frame rate.
Vec3 approach(Vec3 v, Vec3 want, float k, float dt)
{
    return want + (v - want) * std::exp(-k * dt);
}

} // namespace

void player_update(Player &player, const World &world, const NodeRegistry &reg,
                   const MovementRules &rules, const PlayerInput &input, float dt,
                   std::vector<DriveEvent> *log)
{
    player.eye_height = rules.eye;   // the camera and every ray a hand casts
    player.crouching  = false;       // until the walk below says so
    player.riding     = false;       // and the slide
    // Walked off where it lay: up (core.player_lie), on this side at once.
    if (player.lying && std::hypot(player.pos.x - player.lie_at.x, player.pos.z - player.lie_at.z) > 0.6f)
        player.lying = false;
    if (dt <= 0.0f) return;
    // A hitch (window drag, a shader compile) must not teleport the player
    // through a wall. The move itself is walked in half-node steps (move_body),
    // so this bounds the WORK a hitch costs rather than the collision. Not a
    // kart's: it steps its chassis at 60 Hz from its own accumulator and
    // bounds what it catches up itself (vehicle.cpp, a quarter of a second);
    // cut to 50 ms here, a kart at 13 fps went at a third of its speed.
    const float frame_dt = dt;
    dt = std::min(dt, 0.05f);

    // The keys' edges: parkour does things on a press, not while a key is held.
    const bool jump_pressed    = input.jump && !player.jump_was;
    const bool descend_pressed = input.descend && !player.descend_was;
    player.jump_was    = input.jump;
    player.descend_was = input.descend;
    player.kick        = std::max(0.0f, player.kick - dt);
    player.flip        = std::max(0.0f, player.flip - dt);
    const bool pk      = rules.parkour && !rules.drives() && !player.flying;
    if (!pk) {
        player.wall_side = 0;
        player.slide     = 0.0f;
        player.low       = false;
        player.roped     = false;
    }

    // Driving, the mouse tilts the view and the wheel turns the body: a body
    // that turned with the mouse would turn faster than any wheel.
    if (!rules.drives()) player.yaw += input.look_dx * kLookSpeed;
    player.pitch -= input.look_dy * kLookSpeed;
    player.pitch  = std::clamp(player.pitch, -kMaxPitch, kMaxPitch);

    // Hanging from a ledge, or pulling up over it, the head turns at most
    // 135 degrees either way from the wall: three quarters of a turn in all,
    // and never looking straight back over the shoulder it cannot see past.
    if (player.hanging || player.mantle > 0.0f) {
        constexpr float kPi = 3.14159265f, kMost = 0.75f * kPi;
        const float wall = std::atan2((float)player.hang_dx, (float)-player.hang_dz);
        float off = std::remainder(player.yaw - wall, 2.0f * kPi);
        off = std::clamp(off, -kMost, kMost);
        player.yaw = wall + off;
    }

    if (rules.drives()) {
        vehicle_update(player, world, reg, rules, input, frame_dt, log);
        return;
    }
    player.vehicle.reset();
    player.body_pitch = player.body_roll = 0.0f;

    if (input.toggle_fly && (player.flying || rules.flies())) {
        player.flying = !player.flying;
        player.velocity = Vec3{};
    }
    // A body that may not fly (a speed of 0, core.player_fly) falls.
    if (player.flying && !rules.flies()) player.flying = false;

    // Horizontal basis: the flattened look direction, so looking up does not
    // slow you down.
    const Vec3 fwd{std::sin(player.yaw), 0.0f, -std::cos(player.yaw)};
    const Vec3 right{std::cos(player.yaw), 0.0f, std::sin(player.yaw)};

    Vec3 wish = fwd * input.move_forward + right * input.move_right;
    if (length_sq(wish) > 1.0f) wish = normalize(wish);

    if (player.flying) {
        const float speed = rules.fly * (input.sprint ? kFlySprintFactor : 1.0f);
        Vec3 want = wish * speed;
        if (input.jump)    want.y += speed;
        if (input.descend) want.y -= speed;
        // Weight in the air: the keys push towards their speed, and what
        // they do not push along keeps going, slowly let go.
        const bool  pushed = length_sq(wish) > 0.0f, lifted = input.jump || input.descend;
        const Vec3  flat   = approach(Vec3{player.velocity.x, 0.0f, player.velocity.z},
                                      Vec3{want.x, 0.0f, want.z}, pushed ? kFlyGrip : kFlyCoast, dt);
        const float up     = want.y + (player.velocity.y - want.y) * std::exp(-(lifted ? kFlyGrip : kFlyCoast) * dt);
        player.velocity    = Vec3{flat.x, up, flat.z};

        const MoveResult r = move_body(world, reg, player.pos, player.velocity * dt,
                                       player.cb_shift, kStepHeight, box_half(player, rules), box_height(player));
        // What a wall or the ground stopped is not kept pushing into it.
        const Vec3 moved = r.pos - player.pos;
        if (std::fabs(moved.x) + 1e-4f < std::fabs(player.velocity.x * dt)) player.velocity.x = moved.x / dt;
        if (std::fabs(moved.y) + 1e-4f < std::fabs(player.velocity.y * dt)) player.velocity.y = moved.y / dt;
        if (std::fabs(moved.z) + 1e-4f < std::fabs(player.velocity.z * dt)) player.velocity.z = moved.z / dt;
        player.pos       = r.pos;
        player.on_ground = r.on_ground;
        return;
    }

    const float half = rules.width * 0.5f;
    if (player.regrab > 0.0f) player.regrab = std::max(0.0f, player.regrab - dt);

    // Pulling up over a ledge: first straight up until the feet clear its
    // edge, then forward onto it -- never through the corner of the block.
    if (player.mantle > 0.0f) {
        player.mantle = std::min(1.0f, player.mantle + dt / (player.stepping ? kStepUpTime : kMantleTime));
        const Vec3 &a = player.mantle_from, &b = player.mantle_to;
        const float up   = std::min(1.0f, player.mantle / 0.6f);
        const float over = std::max(0.0f, (player.mantle - 0.6f) / 0.4f);
        player.pos = Vec3{a.x + (b.x - a.x) * over, a.y + (b.y + 0.02f - a.y) * up,
                          a.z + (b.z - a.z) * over};
        player.velocity = Vec3{};
        if (player.mantle >= 1.0f) {
            player.mantle    = 0.0f;
            player.stepping  = false;
            player.pos       = b;
            player.on_ground = true;
        } else {
            player.on_ground = false;
        }
        return;
    }

    // Hanging from a ledge by the hands.
    if (player.hanging) {
        const Ledge l = find_ledge(world, reg, player.pos, half, player.hang_dx, player.hang_dz,
                                   kHangDrop - 0.3f, kHangDrop + 0.3f);
        player.velocity = Vec3{};
        if (!l.found || input.descend) {
            // The ledge went, or the hands let go: it falls, and does not
            // catch the same ledge again on the way down.
            player.hanging = false;
            player.regrab  = 0.5f;
            return;
        }
        // A jump pressed while hanging -- not the one that got it here, still
        // held -- pulls up and over: to standing on the ledge, a body's width
        // in from its edge, if a body fits there.
        if (!input.jump) player.jump_held = false;
        if (input.jump && !player.jump_held) {
            const float in = half + 0.1f;
            Vec3 to = player.pos;
            to.y = l.edge;
            if (player.hang_dx != 0) to.x = l.face + (float)player.hang_dx * in;
            else                     to.z = l.face + (float)player.hang_dz * in;
            if (!box_hits_world(world, reg, body_box(to, half))) {
                player.hanging     = false;
                player.stepping    = false;
                player.mantle      = 1e-4f;
                player.mantle_from = player.pos;
                player.mantle_to   = to;
            }
            return;
        }
        // Sideways along it, while there is ledge to hold.
        const float side = input.move_right;
        if (std::fabs(side) > 0.1f) {
            // The wall's right, seen facing it: (-dz, dx).
            const Vec3 along{(float)-player.hang_dz * side * kShimmy * dt, 0.0f,
                             (float)player.hang_dx * side * kShimmy * dt};
            const MoveResult r = move_body(world, reg, player.pos, along, player.cb_shift, 0.0f, half);
            const Ledge still  = find_ledge(world, reg, r.pos, half, player.hang_dx, player.hang_dz,
                                            kHangDrop - 0.3f, kHangDrop + 0.3f);
            if (still.found) {
                player.pos   = r.pos;
                player.pos.y = still.edge - kHangDrop;
            }
        }
        player.on_ground = false;
        return;
    }

    // Riding a slide (chute_at): along its middle line, gathering speed up to
    // the belt's, the feet on its bed and pulled back to the middle if off it
    // -- inside the chute, where its stepped boxes had a body walk down stairs
    // on top of it. Predicted here like any walk, so smooth; off its end the
    // body flies on with the speed it had. Jump gets off. Round a turn the
    // view turns with the chute.
    if (!input.jump) {
        Chute c = chute_at(world, reg, player.pos.x, player.pos.y, player.pos.z);
        if (c.on) {
            const Vec3  from  = player.pos;
            const float speed = std::clamp(std::hypot(player.velocity.x, player.velocity.z) + kChuteAccel * dt,
                                           kChuteStart, c.speed);
            const float heading = std::atan2(c.tx, -c.tz);   // Player::forward's yaw
            Vec3 p = player.pos, last{};
            for (float left = speed * dt; left > 0.0f && c.on;) {
                const float step = std::min(left, 0.1f);
                left -= step;
                const float keep = std::exp(-kChutePull * step);
                const Vec3  was  = p;
                p.x = c.mx + (p.x - c.mx) * keep + c.tx * step;
                p.z = c.mz + (p.z - c.mz) * keep + c.tz * step;
                p.y = c.surface;
                c   = chute_at(world, reg, p.x, p.y, p.z);
                if (c.on) p.y = c.surface;
                last = (p - was) * (1.0f / step);
            }
            // Off its end in the middle of the frame: on at the speed it rode
            // and the way it went, not the part of the frame it was on it.
            player.velocity  = c.on ? (p - from) * (1.0f / dt) : last * speed;
            // Round a turn the body turns with it, and the view: as much as
            // the chute turned under it this frame.
            if (c.on) player.yaw += std::remainder(std::atan2(c.tx, -c.tz) - heading, 6.2831853f);
            player.pos       = p;
            player.on_ground = c.on;
            player.riding    = c.on;
            return;
        }
    }

    player.crouching  = (input.descend && !rules.drives()) || player.low;
    // A parkour world is run through, never walked: the sprint without the key.
    const float speed = player.crouching    ? rules.walk * kCrouchPace
                      : input.sprint || pk ? rules.sprint
                                            : rules.walk;

    // Swimming, when any part of the body is in a liquid: the keys say what the
    // horizontal velocity is at half the pace, the body sinks slowly and jump held
    // lifts it. The numbers are core/physics/movement.h's, which the server's
    // validator is under too.
    if (box_in_liquid(world, reg, body_box(player.pos, rules.width * 0.5f))) {
        const Vec3 h = approach(Vec3{player.velocity.x, 0.0f, player.velocity.z},
                                wish * (speed * kSwimPace), kSwimGrip, dt);
        player.velocity.x = h.x;
        player.velocity.z = h.z;
        player.velocity.y += rules.gravity * kSwimGravity * dt;
        if (player.velocity.y < -kSwimSink) player.velocity.y = -kSwimSink;
        if (input.jump) player.velocity.y = kSwimRise;

        const MoveResult r = move_body(world, reg, player.pos, player.velocity * dt,
                                       player.cb_shift, kStepHeight, box_half(player, rules), box_height(player));
        player.pos = r.pos;
        if (r.on_ground || r.hit_ceiling) player.velocity.y = 0.0f;
        player.on_ground = r.on_ground;
        return;
    }

    // On a ladder: no gravity. Jump climbs, and so does walking into the
    // ladder or the wall it is on, as in the classic block game; crouch holds still, and
    // otherwise the body slides down slowly -- and walks on and off it as on
    // the ground. The numbers are core/physics/movement.h's, which the
    // server's validator is under too.
    if (box_on_climbable(world, reg, body_box(player.pos, rules.width * 0.5f))) {
        const Vec3 h = approach(Vec3{player.velocity.x, 0.0f, player.velocity.z},
                                wish * (speed * kClimbPace), kGroundGrip, dt);
        player.velocity.x = h.x;
        player.velocity.z = h.z;
        // As far as this frame's step and a little more -- a blocked step is
        // not taken at all, so at a low frame rate a body stops that far
        // short -- and a little off the floor, so standing on it is not
        // walking into it.
        const float reach   = std::sqrt(h.x * h.x + h.z * h.z) * dt + 0.05f;
        const bool  pushing = length_sq(wish) > 0.01f &&
                             box_hits_world(world, reg, body_box(player.pos + normalize(wish) * reach +
                                                                 Vec3{0.0f, 0.05f, 0.0f}, half));
        player.velocity.y = input.jump ? kClimbUp : player.crouching ? 0.0f : pushing ? kClimbUp : -kClimbSlide;

        const MoveResult r = move_body(world, reg, player.pos, player.velocity * dt,
                                       player.cb_shift, kStepHeight, box_half(player, rules), box_height(player));
        player.pos = r.pos;
        if (r.on_ground || r.hit_ceiling) player.velocity.y = 0.0f;
        player.on_ground = r.on_ground;
        return;
    }

    // Parkour (MovementRules::parkour): a slide, a run along a wall, a kick off
    // one, a rope. A backflip is the jump's, below.
    bool on_wall = false;   // running along a wall this update: no gravity
    bool kicked  = false;   // this press of jump spent
    if (pk) {
        if (player.on_ground) {
            player.wall_side = 0;
            player.used_dx = player.used_dz = 0;
        }
        // Jump on a rope lets go of it, with a little lift.
        if (player.roped && jump_pressed) {
            player.roped      = false;
            player.velocity.y = std::max(player.velocity.y, 0.0f) + 3.0f;
            kicked            = true;
        }
        // Let out with crouch, taken in with sprint.
        if (player.roped) {
            if (input.descend) player.rope_len += kRopeReel * dt;
            if (input.sprint)  player.rope_len = std::max(kRopeShortest, player.rope_len - kRopeReel * dt);
        }

        // Crouch pressed at more than a walk, on the ground: down on the back,
        // feet first, faster than the sprint and slowing, under a block.
        const float hspeed = std::hypot(player.velocity.x, player.velocity.z);
        if (player.on_ground && !player.roped && descend_pressed && !player.low &&
            hspeed > rules.walk + 0.5f) {
            const float k = std::max(hspeed, rules.sprint * kSlideBoost) / hspeed;
            player.velocity.x *= k;
            player.velocity.z *= k;
            player.slide = kSlideTime;
            player.low   = true;
        }
        const bool room = !box_hits_world(world, reg, body_box(player.pos, half, player.cb_shift,
                                                               std::max(0.3f, kPlayerHeight - player.cb_top)));
        if (player.slide > 0.0f) {
            player.slide -= dt;
            if (jump_pressed && room) player.slide = 0.0f;   // up out of it, into the jump
        }
        // Up again once the slide is over, if a body stands there; until then
        // it goes on low, at a crouch.
        if (player.low && player.slide <= 0.0f && room) player.low = false;

        if (!player.on_ground && !player.roped && !kicked) {
            // Jump against a wall in the air: kicked off it, away and up --
            // between two walls, from one to the other and higher each time.
            // Not off a wall on the same side twice running: one wall is not
            // climbed.
            if (jump_pressed) {
                const int sides[5][2] = {{player.wall_dx, player.wall_dz}, {1, 0}, {-1, 0}, {0, 1}, {0, -1}};
                for (int i = player.wall_side != 0 ? 0 : 1; i < 5 && !kicked; ++i) {
                    const int dx = sides[i][0], dz = sides[i][1];
                    // Within kKickReach: in a gap two wide, one wall always is.
                    if (!touches(world, reg, player, half, dx, dz, kKickReach)) continue;
                    const bool same = player.used_dx == dx && player.used_dz == dz;
                    if (same && i != 0) continue;
                    if (dx != 0) player.velocity.x = (float)-dx * kKickOut;
                    else         player.velocity.z = (float)-dz * kKickOut;
                    player.velocity.y = rules.jump;
                    player.kick       = kKickTime;
                    player.wall_side  = 0;
                    player.used_dx    = dx;
                    player.used_dz    = dz;
                    kicked            = true;
                }
            }
            if (player.wall_side != 0 && !kicked) {
                // Along it while forward is held, the wall is there and the
                // run is under kWallRunMost.
                if (input.move_forward < 0.3f || player.wall_run >= kWallRunMost ||
                    !touches(world, reg, player, half, player.wall_dx, player.wall_dz))
                    player.wall_side = 0;
                else
                    on_wall = true;
            } else if (!kicked && input.move_forward > 0.5f && player.flip <= 0.0f &&
                       hspeed >= kWallRunMin && player.velocity.y < 1.0f) {
                // Near the top of the jump, so the run is off the floor.
                // Forward and fast enough, a wall beside the way it goes: on it.
                const bool along_x = std::fabs(player.velocity.x) >= std::fabs(player.velocity.z);
                for (int sgn : {1, -1}) {
                    const int dx = along_x ? 0 : sgn, dz = along_x ? sgn : 0;
                    if (!touches(world, reg, player, half, dx, dz)) continue;
                    if (player.used_dx == dx && player.used_dz == dz) continue;
                    // The body's right: (cos yaw, sin yaw).
                    player.wall_side = (float)dx * std::cos(player.yaw) + (float)dz * std::sin(player.yaw) > 0.0f ? 1 : -1;
                    player.wall_dx   = dx;
                    player.wall_dz   = dz;
                    player.wall_run  = 0.0f;
                    player.used_dx   = dx;
                    player.used_dz   = dz;
                    on_wall          = true;
                    break;
                }
            }
        }
        if (on_wall) {
            // Level along the wall, at least kWallRunMin, pressed against it.
            float &along = player.wall_dx != 0 ? player.velocity.z : player.velocity.x;
            float &into  = player.wall_dx != 0 ? player.velocity.x : player.velocity.z;
            along = (along < 0.0f ? -1.0f : 1.0f) * std::max(std::fabs(along), kWallRunMin);
            into  = (float)(player.wall_dx + player.wall_dz) * 0.5f;
            player.velocity.y = 0.0f;
        }
    }
    const bool sliding = pk && player.slide > 0.0f;

    if (sliding && player.on_ground) {
        // Sliding: the keys have no say, and it slows.
        player.velocity.x *= std::exp(-1.2f * dt);
        player.velocity.z *= std::exp(-1.2f * dt);
    } else if (on_wall) {
        // Running along a wall: as set above.
    } else if (player.on_ground) {
        // Standing on something: the velocity closes on what the keys ask for
        // at the ground's grip (kGroundGrip), which is the body's weight. Low
        // after a slide, under a block, at a crouch.
        const Vec3 h = approach(Vec3{player.velocity.x, 0.0f, player.velocity.z},
                                wish * (player.low ? rules.walk * kCrouchPace : speed), kGroundGrip, dt);
        player.velocity.x = h.x;
        player.velocity.z = h.z;
    } else {
        // In the air, the classic FPS air acceleration (kAirAccelerate): the keys add
        // along their direction until the velocity along it reaches the cap,
        // so nothing pressed keeps what the body has -- a portal's speed too.
        const float wish_len = std::sqrt(wish.x * wish.x + wish.z * wish.z);
        if (wish_len > 1e-4f) {
            const float dx    = wish.x / wish_len;
            const float dz    = wish.z / wish_len;
            const float was   = std::sqrt(player.velocity.x * player.velocity.x +
                                          player.velocity.z * player.velocity.z);
            const float along = player.velocity.x * dx + player.velocity.z * dz;
            const float pace  = speed * std::min(wish_len, 1.0f);
            const float add   = std::min(kAirAccelerate * pace * dt,
                                         std::min(pace, kAirSpeedCap) - along);
            if (add > 0.0f) {
                player.velocity.x += dx * add;
                player.velocity.z += dz * add;

                // Bent, never sped past the sprint: no bunny hop (kAirSpeedCap).
                const float most = std::max(was, rules.sprint);
                const float now  = std::sqrt(player.velocity.x * player.velocity.x +
                                             player.velocity.z * player.velocity.z);
                if (now > most) {
                    player.velocity.x *= most / now;
                    player.velocity.z *= most / now;
                }
            }
        }
    }

    // Swung on a rope by the keys, in the air.
    if (player.roped && !player.on_ground) {
        player.velocity.x += wish.x * kRopePump * dt;
        player.velocity.z += wish.z * kRopePump * dt;
    }

    if (!on_wall) player.velocity.y += rules.gravity * dt;

    // A fall stops gaining speed at the terminal speed, whatever pulls it. The
    // validator allows exactly that much downwards, and until a world could
    // declare its gravity nothing needed saying: a world 64 blocks tall at
    // -25 m/s^2 tops out at 56.6 m/s. A world with twice the gravity does not,
    // and neither does a fall through a floor portal into a ceiling portal
    // above it, which lasts as long as the player lets it.
    if (player.velocity.y < -kTerminalFallSpeed) player.velocity.y = -kTerminalFallSpeed;

    if (input.jump && player.on_ground && !player.low && !kicked) {
        if (pk && jump_pressed && input.move_forward < -0.5f) {
            // Back held: a backflip, higher than a jump and backwards -- onto
            // a block two high behind.
            player.velocity.y = kFlipRise;
            player.velocity.x = -fwd.x * kFlipBack;
            player.velocity.z = -fwd.z * kFlipBack;
            player.flip       = kFlipTime;
        } else {
            player.velocity.y = rules.jump;
        }
    }

    // Standing on a belt (NodeDef::conveyor): carried the way it runs, on top
    // of the keys and not into the velocity -- stepping off leaves it behind.
    Vec3 carry{};
    if (player.on_ground) {
        const Belt b = belt_under(world, reg, player.pos.x, player.pos.y, player.pos.z);
        if (b.on) carry = Vec3{b.dx * b.speed * dt, 0.0f, b.dz * b.speed * dt};
    }

    const Vec3       before = player.pos;
    const MoveResult r = move_body(world, reg, player.pos, player.velocity * dt + carry,
                                   player.cb_shift, kStepHeight, box_half(player, rules), box_height(player));
    player.pos = r.pos;

    // Zeroing vertical speed on contact is what stops gravity accumulating into
    // a value that punches through the floor after a few seconds of standing.
    if (r.on_ground || r.hit_ceiling) player.velocity.y = 0.0f;
    player.on_ground = r.on_ground;

    if (on_wall) {
        // How far along the wall; stopped by something ahead, off it.
        const Vec3  moved = r.pos - before;
        const float went  = std::hypot(moved.x, moved.z);
        player.wall_run += went;
        if (went < 0.5f * std::hypot(player.velocity.x, player.velocity.z) * dt) player.wall_side = 0;
    }

    // On a rope: the hands no further from where it holds than its length --
    // put back onto that sphere through whatever is in the way, and what took
    // them outwards taken out of the velocity. That is the swing.
    if (player.roped) {
        const Vec3  out  = player.hands() - player.rope_at;
        const float dist = length(out);
        if (dist > player.rope_len && dist > 1e-4f) {
            const Vec3 n  = out * (1.0f / dist);
            const Vec3 to = player.rope_at + n * player.rope_len - Vec3{0.0f, kRopeHands, 0.0f};
            const MoveResult c = move_body(world, reg, player.pos, to - player.pos, player.cb_shift, 0.0f,
                                           box_half(player, rules), box_height(player));
            player.pos       = c.pos;
            player.on_ground = player.on_ground || c.on_ground;
            const float vr   = dot(player.velocity, n);
            if (vr > 0.0f) player.velocity = player.velocity - n * vr;
        }
        const float most = std::hypot(player.velocity.x, player.velocity.z);
        if (most > kRopeTop) {
            player.velocity.x *= kRopeTop / most;
            player.velocity.z *= kRopeTop / most;
        }
    }
    // Parkour doing something is not a moment to step up or grab a ledge.
    if (player.low || player.wall_side != 0 || player.flip > 0.0f || player.roped) return;

    // On the ground, walking into a block one high along the axis it faces
    // most and touching it, crouch not held: up onto it, if a body fits there.
    if (player.on_ground && !input.descend && !input.jump && input.move_forward > 0.3f) {
        const Vec3 f = player.forward();
        const int  dx = std::fabs(f.x) >= std::fabs(f.z) ? (f.x > 0 ? 1 : -1) : 0;
        const int  dz = dx == 0 ? (f.z > 0 ? 1 : -1) : 0;
        const Ledge l = find_ledge(world, reg, player.pos, half, dx, dz, kStepHeight + 0.01f, 1.01f);
        const float along = dx != 0 ? player.pos.x : player.pos.z;
        const float gap   = dx + dz > 0 ? l.face - (along + half) : (along - half) - l.face;
        if (l.found && gap < 0.1f) {
            const float in = half + 0.1f;
            Vec3 to = player.pos;
            to.y = l.edge;
            if (dx != 0) to.x = l.face + (float)dx * in;
            else         to.z = l.face + (float)dz * in;
            if (!box_hits_world(world, reg, body_box(to, half))) {
                player.hang_dx     = dx;
                player.hang_dz     = dz;
                player.stepping    = true;
                player.mantle      = 1e-4f;
                player.mantle_from = player.pos;
                player.mantle_to   = to;
                return;
            }
        }
    }

    // In the air, facing a wall whose top the hands can reach -- from a node
    // over the feet (a jump at a wall two high takes it on the way up) to over
    // the head (a jump at a wall three high, at the top of it) -- and pushing
    // towards it or already touching it: the hands take it. Along the axis
    // the body faces most, so a ledge is grabbed looking at it.
    // In a parkour world too: a wall too tall to reach the top of is still
    // kicked up, and a backflip starts on the ground, where hands take nothing.
    if (rules.ledges && !player.on_ground && !input.descend && player.regrab <= 0.0f) {
        const Vec3 f = player.forward();
        const int  dx = std::fabs(f.x) >= std::fabs(f.z) ? (f.x > 0 ? 1 : -1) : 0;
        const int  dz = dx == 0 ? (f.z > 0 ? 1 : -1) : 0;
        const Ledge l = find_ledge(world, reg, player.pos, half, dx, dz, 1.0f, kHangDrop + 0.25f);
        const float along = dx != 0 ? player.pos.x : player.pos.z;
        const float gap   = dx + dz > 0 ? l.face - (along + half) : (along - half) - l.face;
        if (l.found && (input.move_forward > 0.3f || gap < 0.1f)) {
            Vec3 at = player.pos;
            at.y = l.edge - kHangDrop;
            // Against the wall's face.
            if (dx != 0) at.x = l.face - (float)dx * (half + 0.01f);
            else         at.z = l.face - (float)dz * (half + 0.01f);
            if (!box_hits_world(world, reg, body_box(at, half))) {
                player.pos      = at;
                player.velocity = Vec3{};
                player.hanging   = true;
                player.hang_dx   = dx;
                player.hang_dz   = dz;
                player.jump_held = input.jump;
            }
        }
    }
}

bool rope_fire(Player &player, const World &world, const NodeRegistry &reg, Vec3 cam_eye, Vec3 cam_look)
{
    const Vec3 from = player.eye();
    Vec3       look = player.forward();
    // What the crosshair is on, then from the eye to it: in third person the
    // camera sits behind and over the head, and the eye's own look lands well
    // off the crosshair. The look when that point is nearer than a node or
    // behind the eye (the camera's ray met a wall behind the body).
    const float      far = kRopeReach + length(from - cam_eye);
    const RaycastHit on  = raycast(world, reg, cam_eye, cam_look, far);
    const Vec3       to  = cam_eye + cam_look * (on.hit ? on.distance : far) - from;
    const float      l   = length(to);
    if (l >= 1.0f && dot(to, look) / l >= 0.5f) look = to * (1.0f / l);
    const RaycastHit hit  = raycast(world, reg, from, look, kRopeReach);
    if (!hit.hit) return false;
    player.roped    = true;
    player.rope_at  = from + look * hit.distance;
    player.rope_len = std::max(kRopeShortest, length(player.hands() - player.rope_at));
    return true;
}

void StepSmooth::note(const Player &p, bool was_on_ground, float y_before)
{
    const float rise = p.pos.y - y_before;
    if (was_on_ground && p.on_ground && !p.flying && !p.vehicle && rise > 0.01f &&
        rise <= kStepHeight + 0.01f)
        lag = std::max(lag - rise, -1.0f);
    // Sat down in a slide, or up out of one: the eye goes down or up as gently.
    if (p.riding != riding) {
        const float d = p.eye_height * (1.0f - kChuteEye);
        lag    = std::clamp(lag + (p.riding ? d : -d), -1.0f, 1.0f);
        riding = p.riding;
    }
}

void StepSmooth::settle(float dt)
{
    constexpr float k = 22.0f;   // most of it in 0.15 s, all of it in 0.3
    constexpr float h = 1.0f / 240.0f;
    for (float left = std::min(dt, 0.25f); left > 0.0f; left -= h) {
        const float step = std::min(left, h);
        speed += (-k * k * lag - 2.0f * k * speed) * step;
        lag += speed * step;
    }
    if (std::fabs(lag) < 1e-4f && std::fabs(speed) < 1e-3f) lag = speed = 0.0f;
}

} // namespace cnp
