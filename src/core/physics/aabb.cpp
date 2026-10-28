#include "core/physics/aabb.h"
#include "core/registry/node_shape.h"

#include <algorithm>
#include <cmath>

namespace cnp {
namespace {

int floor_i(float v) { return (int)std::floor(v); }

} // namespace

bool boxes_overlap(const AABB &a, const AABB &b)
{
    return a.min.x < b.max.x && a.max.x > b.min.x &&
           a.min.y < b.max.y && a.max.y > b.min.y &&
           a.min.z < b.max.z && a.max.z > b.min.z;
}

bool ray_hits_box(const AABB &b, Vec3 origin, Vec3 dir, float max_distance, float &t)
{
    int ignored[3];
    return ray_hits_box(b, origin, dir, max_distance, t, ignored);
}

bool ray_hits_box(const AABB &b, Vec3 origin, Vec3 dir, float max_distance, float &t,
                  int normal[3])
{
    // Slabs: the ray is inside the box only where it is inside all three pairs
    // of planes at once, so the answer is the latest entry against the earliest
    // exit.
    float enter = 0.0f;
    float leave = max_distance;

    // Which axis the latest entry came from, and from which side. -1 while the
    // entry is still the ray's own origin, which is a ray that started inside.
    int enter_axis = -1;
    int enter_sign = 0;

    const float o[3]  = {origin.x, origin.y, origin.z};
    const float d[3]  = {dir.x, dir.y, dir.z};
    const float lo[3] = {b.min.x, b.min.y, b.min.z};
    const float hi[3] = {b.max.x, b.max.y, b.max.z};

    for (int a = 0; a < 3; ++a) {
        // A ray with no component along this axis is divided through anyway,
        // and the infinities land in the right place: outside the slab the two
        // crossings come out with the same sign and `enter > leave` rejects it;
        // inside, they come out -inf and +inf and this axis says nothing. On a
        // plane exactly, one of them is 0/0 -- and every comparison against a
        // NaN below is false, which leaves the axis saying nothing, which is
        // what a ray lying in a face plane should say.
        //
        // There WAS a branch here asking about it directly. It came out because
        // no sabotage of it could make a single check go red: it was three
        // lines saying again what the arithmetic already said. Same reason as
        // M7_FINDINGS 20 and 30.
        const float inv = 1.0f / d[a];
        float       t0  = (lo[a] - o[a]) * inv;
        float       t1  = (hi[a] - o[a]) * inv;
        int         s   = -1;   // came in through the low face, so the normal points down the axis
        if (t0 > t1) { const float swap = t0; t0 = t1; t1 = swap; s = 1; }

        if (t0 > enter) { enter = t0; enter_axis = a; enter_sign = s; }
        if (t1 < leave) leave = t1;
        if (enter > leave) return false;
    }

    normal[0] = normal[1] = normal[2] = 0;
    if (enter_axis >= 0) normal[enter_axis] = enter_sign;

    t = enter;
    return true;
}

bool node_blocks_box(const World &world, const NodeRegistry &reg, int bx, int by, int bz,
                     const AABB &b, const AABB *was)
{
    const BlockId id = world.get_block(bx, by, bz);
    if (!reg.is_solid(id)) return false;

    const NodeDef &def = reg.get(id);
    if (!node_has_shape(def)) return true;   // the whole cell, and the caller is in it

    const uint8_t facing = world.get_facing(bx, by, bz);
    const int     count  = node_shape_count(def);
    for (int i = 0; i < count; ++i) {
        const NodeBox nb = node_shape_box(def, facing, i, bx, by, bz);
        const AABB    part{Vec3{nb.min[0], nb.min[1], nb.min[2]},
                        Vec3{nb.max[0], nb.max[1], nb.max[2]}};
        if (boxes_overlap(b, part) && !(was && def.door != 0 && boxes_overlap(*was, part))) return true;
    }
    return false;
}

bool box_meets_mover(const World::Mover &m, const AABB &b)
{
    if (b.max.y <= m.bottom + kBoxSkin || b.min.y >= m.top - kBoxSkin) return false;
    // Across the ground: the box and the hull apart along the world's x and z
    // or the hull's own right and front, or not at all.
    const float sy = std::sin(m.yaw), cy = std::cos(m.yaw);
    const float hx = (b.max.x - b.min.x) * 0.5f, hz = (b.max.z - b.min.z) * 0.5f;
    const float dx = (b.min.x + b.max.x) * 0.5f - m.x, dz = (b.min.z + b.max.z) * 0.5f - m.z;
    // The hull's reach along x and along z, and the box's along its right (cy,
    // sy) and front (sy, -cy).
    if (std::fabs(dx) >= hx + m.half_w * std::fabs(cy) + m.half_l * std::fabs(sy) - kBoxSkin) return false;
    if (std::fabs(dz) >= hz + m.half_w * std::fabs(sy) + m.half_l * std::fabs(cy) - kBoxSkin) return false;
    if (std::fabs(dx * cy + dz * sy) >= m.half_w + hx * std::fabs(cy) + hz * std::fabs(sy) - kBoxSkin) return false;
    if (std::fabs(dx * sy - dz * cy) >= m.half_l + hx * std::fabs(sy) + hz * std::fabs(cy) - kBoxSkin) return false;
    return true;
}

bool box_hits_world(const World &world, const NodeRegistry &reg, const AABB &b, const AABB *was)
{
    // A hull the body was already in -- it drove into them -- does not hold
    // it: it walks out, as out of a door shut on it.
    for (const World::Mover &m : world.movers)
        if (box_meets_mover(m, b) && (was == nullptr || !box_meets_mover(m, *was))) return true;

    const int x0 = floor_i(b.min.x), x1 = floor_i(b.max.x - kBoxSkin);
    const int y0 = floor_i(b.min.y), y1 = floor_i(b.max.y - kBoxSkin);
    const int z0 = floor_i(b.min.z), z1 = floor_i(b.max.z - kBoxSkin);

    for (int z = z0; z <= z1; ++z)
        for (int y = y0; y <= y1; ++y)
            for (int x = x0; x <= x1; ++x)
                if (node_blocks_box(world, reg, x, y, z, b, was)) return true;

    // And the ring of cells round those, for a block whose boxes leave its own
    // cell (core.node_box allows it): a box reaching into the cell a body is
    // in stops it as its own cell's would. Only a solid block with a box out of
    // bounds is looked at closer; a cube never reaches past its cell.
    for (int z = z0 - 1; z <= z1 + 1; ++z)
        for (int y = y0 - 1; y <= y1 + 1; ++y)
            for (int x = x0 - 1; x <= x1 + 1; ++x) {
                if (x >= x0 && x <= x1 && y >= y0 && y <= y1 && z >= z0 && z <= z1) continue;
                const BlockId id = world.get_block(x, y, z);
                if (!reg.is_solid(id)) continue;
                bool out = false;
                for (const NodeDef::Box &nb : reg.get(id).parts)
                    for (int a = 0; a < 3; ++a) out |= nb.min[a] < 0.0f || nb.max[a] > 1.0f;
                if (out && node_blocks_box(world, reg, x, y, z, b, was)) return true;
            }
    return false;
}

bool box_in_liquid(const World &world, const NodeRegistry &reg, const AABB &b)
{
    // The same cells box_hits_world walks, with the same skin: a body whose
    // feet land exactly on the surface of a lake is standing on the cell above
    // it and not in the water.
    const int x0 = floor_i(b.min.x), x1 = floor_i(b.max.x - kBoxSkin);
    const int y0 = floor_i(b.min.y), y1 = floor_i(b.max.y - kBoxSkin);
    const int z0 = floor_i(b.min.z), z1 = floor_i(b.max.z - kBoxSkin);

    // A cell that is only partly water -- a flow, drawn at its own height --
    // is waded through and not swum in unless its surface is more than half a
    // node over the feet: a stream a quarter deep is walked along. A full cell
    // counts wherever the box touches it, as it always has, so a swimmer at the
    // top of a lake still floats there.
    for (int z = z0; z <= z1; ++z)
        for (int y = y0; y <= y1; ++y)
            for (int x = x0; x <= x1; ++x) {
                const BlockId id = world.get_block(x, y, z);
                if (!reg.is_liquid(id)) continue;
                const uint8_t fill = reg.liquid_fill(id);
                if (fill >= 8 || (float)y + (float)fill / 8.0f > b.min.y + 0.5f) return true;
            }
    return false;
}

bool box_on_climbable(const World &world, const NodeRegistry &reg, const AABB &b)
{
    for (int z = floor_i(b.min.z); z <= floor_i(b.max.z - kBoxSkin); ++z)
        for (int y = floor_i(b.min.y); y <= floor_i(b.max.y - kBoxSkin); ++y)
            for (int x = floor_i(b.min.x); x <= floor_i(b.max.x - kBoxSkin); ++x)
                if (reg.is_climbable(world.get_block(x, y, z))) return true;
    return false;
}

Belt belt_under(const World &world, const NodeRegistry &reg, float x, float y, float z)
{
    // Facing 0..3 heads -z, -x, +z, +x: away from whoever put it down.
    static constexpr int kRun[4][2] = {{0, -1}, {-1, 0}, {0, 1}, {1, 0}};
    const int bx  = floor_i(x);
    const int bz  = floor_i(z);
    const int top = floor_i(y - 0.05f);
    for (int cy = top; cy >= top - 1; --cy) {
        const NodeDef &d = reg.get(world.get_block(bx, cy, bz));
        if (d.conveyor <= 0.0f || d.rail) continue;
        const int   f  = world.get_facing(bx, cy, bz) & 3;
        const int   dx = kRun[f][0], dz = kRun[f][1];
        const float fx = x - (float)bx, fz = z - (float)bz;
        // How far along the run from the edge things come in at, 0 to 1.
        const float t = dx > 0 ? fx : dx < 0 ? 1.0f - fx : dz > 0 ? fz : 1.0f - fz;
        const float s = (float)cy + d.conveyor_height + d.conveyor_rise * t;
        if (std::fabs(y - s) > 0.3f) continue;
        return Belt{true, bx, cy, bz, (float)dx, (float)dz, d.conveyor, s};
    }
    return Belt{};
}

Chute chute_at(const World &world, const NodeRegistry &reg, float x, float y, float z, bool rails)
{
    static constexpr int kRun[4][2] = {{0, -1}, {-1, 0}, {0, 1}, {1, 0}};
    const auto chute = [&](int cx, int cy, int cz) {
        const NodeDef &d = reg.get(world.get_block(cx, cy, cz));
        return d.rail ? rails : d.conveyor >= kChuteSpeed && d.conveyor_rise < 0.0f;
    };
    const auto runs = [&](int cx, int cy, int cz, int dx, int dz) {
        const int f = world.get_facing(cx, cy, cz) & 3;
        return chute(cx, cy, cz) && kRun[f][0] == dx && kRun[f][1] == dz;
    };
    const int bx = floor_i(x), bz = floor_i(z), top = floor_i(y - 0.05f);
    Chute best;
    float best_gap = kChuteReach;
    for (int cy = top + 1; cy >= top - 1; --cy) {
        if (!chute(bx, cy, bz)) continue;
        const NodeDef &d  = reg.get(world.get_block(bx, cy, bz));
        const int      f  = world.get_facing(bx, cy, bz) & 3;
        const float    ox = (float)kRun[f][0], oz = (float)kRun[f][1];
        // The way in: straight on, unless a slide a cell up runs in from a
        // side -- or for a rail, which runs level too, one on the same level.
        float ix = ox, iz = oz;
        const int bkx = bx - kRun[f][0], bkz = bz - kRun[f][1];
        const bool behind = runs(bkx, cy + 1, bkz, kRun[f][0], kRun[f][1]) ||
                            (d.rail && (runs(bkx, cy, bkz, kRun[f][0], kRun[f][1]) ||
                                        runs(bkx, cy - 1, bkz, kRun[f][0], kRun[f][1])));
        if (!behind)
            for (int s : {1, -1}) {
                const int px = s * kRun[f][1], pz = -s * kRun[f][0];   // a side
                if (runs(bx + px, cy + 1, bz + pz, -px, -pz) || (d.rail && runs(bx + px, cy, bz + pz, -px, -pz))) {
                    ix = (float)-px;
                    iz = (float)-pz;
                    break;
                }
            }
        const float cx = (float)bx + 0.5f, cz = (float)bz + 0.5f;
        Chute c;
        float t;
        if (ix == ox && iz == oz) {
            // Straight: from the middle of the side it comes in by.
            t    = (x - (cx - ox * 0.5f)) * ox + (z - (cz - oz * 0.5f)) * oz;
            c.mx = ox != 0.0f ? x : cx;
            c.mz = oz != 0.0f ? z : cz;
            c.tx = ox;
            c.tz = oz;
        } else {
            // A quarter circle of radius a half about the corner: from -out
            // at the way in round to +in at the way out, the way along it
            // turning from in to out.
            const float kx = cx - ix * 0.5f + ox * 0.5f, kz = cz - iz * 0.5f + oz * 0.5f;
            const float rx = x - kx, rz = z - kz;
            const float a  = std::atan2(rx * ix + rz * iz, -(rx * ox + rz * oz));
            t = std::clamp(a / 1.5707963f, 0.0f, 1.0f);
            const float th = t * 1.5707963f, co = std::cos(th), si = std::sin(th);
            c.mx = kx + 0.5f * (-ox * co + ix * si);
            c.mz = kz + 0.5f * (-oz * co + iz * si);
            c.tx = ix * co + ox * si;
            c.tz = iz * co + oz * si;
        }
        c.surface = (float)cy + d.conveyor_height + d.conveyor_rise * t;
        c.speed   = d.conveyor;
        c.rise    = d.conveyor_rise;
        c.rail    = d.rail;
        c.on      = true;
        if (std::fabs(y - c.surface) <= best_gap) {
            best_gap = std::fabs(y - c.surface);
            best     = c;
        }
    }
    return best;
}

Ledge find_ledge(const World &world, const NodeRegistry &reg, Vec3 feet, float half_width,
                 int dx, int dz, float lowest, float highest)
{
    Ledge out;
    // The column the hands reach into: just past the body's side.
    const float reach = half_width + 0.2f;
    const int   cx    = (int)std::floor(feet.x + (float)dx * reach);
    const int   cz    = (int)std::floor(feet.z + (float)dz * reach);
    // And the body's own column, which has to be open over its head for the
    // pull: a ledge under a ceiling is a ledge nobody climbs onto.
    const int bx = (int)std::floor(feet.x), bz = (int)std::floor(feet.z);

    for (int y = (int)std::floor(feet.y + lowest) - 1; y <= (int)std::floor(feet.y + highest); ++y) {
        if (!reg.is_solid(world.get_block(cx, y, cz))) continue;
        // The top of what is there, by its boxes: a slab's edge is half up.
        // Only the boxes the body walks into -- across its width, and starting
        // within the hands' reach -- so a stair is a half step from its front
        // and a node from its back.
        float top = 1.0f;
        const NodeDef &d = reg.get(world.get_block(cx, y, cz));
        if (node_has_shape(d)) {
            const int   a    = dx != 0 ? 0 : 2, s = dx != 0 ? 2 : 0;
            const float fwd  = dx != 0 ? feet.x - (float)cx : feet.z - (float)cz;
            const float side = dx != 0 ? feet.z - (float)cz : feet.x - (float)cx;
            top = -1.0f;
            for (int i = 0; i < node_shape_count(d); ++i) {
                const NodeBox nb = node_shape_box(d, world.get_facing(cx, y, cz), i, 0, 0, 0);
                if (nb.min[s] >= side + half_width || nb.max[s] <= side - half_width) continue;
                if (dx + dz > 0 ? nb.min[a] >= fwd + reach : nb.max[a] <= fwd - reach) continue;
                top = std::max(top, nb.max[1]);
            }
            if (top < 0.0f) continue;
        }
        const float edge = (float)y + top;
        if (edge < feet.y + lowest || edge > feet.y + highest) continue;
        if (reg.is_solid(world.get_block(cx, y + 1, cz)) || reg.is_solid(world.get_block(cx, y + 2, cz)))
            continue;
        if (reg.is_solid(world.get_block(bx, (int)std::floor(edge) + 1, bz))) continue;
        out.found = true;
        out.edge  = edge;
        out.face  = dx > 0 ? (float)cx : dx < 0 ? (float)(cx + 1) : dz > 0 ? (float)cz : (float)(cz + 1);
        return out;
    }
    return out;
}

bool body_at_ledge(const World &world, const NodeRegistry &reg, Vec3 feet, float half_width,
                   int *dx, int *dz)
{
    static const int kDirs[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
    for (const auto &d : kDirs) {
        const Ledge l = find_ledge(world, reg, feet, half_width, d[0], d[1], -0.1f, kHangDrop + 0.3f);
        if (!l.found) continue;
        // Against the wall: within a hand of its face.
        const float along = d[0] != 0 ? feet.x : feet.z;
        const float gap   = (d[0] + d[1] > 0) ? l.face - (along + half_width)
                                              : (along - half_width) - l.face;
        if (gap < 0.3f) {
            if (dx) *dx = d[0];
            if (dz) *dz = d[1];
            return true;
        }
    }
    return false;
}

AABB player_box(Vec3 feet_pos, float bottom_shift)
{
    return body_box(feet_pos, kPlayerHalfWidth, bottom_shift);
}

AABB body_box(Vec3 feet_pos, float half_width, float bottom_shift, float height)
{
    // Clamped here rather than trusted from the caller: a shift at or past the
    // player's height inverts the box, and an inverted box collides with
    // nothing at all -- the player would fall out of the world through a solid
    // floor. The cap leaves a tenth of a node of box.
    const float lift = (bottom_shift < 0.0f)           ? 0.0f
                       : (bottom_shift > height - 0.1f) ? height - 0.1f
                                                        : bottom_shift;
    return AABB{
        Vec3{feet_pos.x - half_width, feet_pos.y + lift, feet_pos.z - half_width},
        Vec3{feet_pos.x + half_width, feet_pos.y + height, feet_pos.z + half_width}};
}

MoveResult move_box(const World &world, const NodeRegistry &reg, Vec3 feet_pos, Vec3 delta,
                    float bottom_shift, float step_height, float half_width, float height)
{
    const auto box_at = [half_width, height](Vec3 feet, float shift) {
        return body_box(feet, half_width, shift, height);
    };

    MoveResult res;
    const AABB was = box_at(feet_pos, bottom_shift);
    Vec3 pos = feet_pos;

    // Y first, then X, then Z. Landing before the horizontal step means a
    // player walking off a ledge and back on does not clip the lip.
    const float step[3] = {delta.y, delta.x, delta.z};
    const int   axis[3] = {1, 0, 2};

    for (int i = 0; i < 3; ++i) {
        if (step[i] == 0.0f) continue;

        Vec3 next = pos;
        if (axis[i] == 0)      next.x += step[i];
        else if (axis[i] == 1) next.y += step[i];
        else                   next.z += step[i];

        if (!box_hits_world(world, reg, box_at(next, bottom_shift), &was)) {
            pos = next;
            continue;
        }

        // Blocked horizontally, and standing on something: try the same step
        // `step_height` higher and set the body back down. The vertical axis
        // comes first in this loop, so `on_ground` is already known.
        if (axis[i] != 1 && res.on_ground && step_height > 0.0f) {
            Vec3 lifted = pos;
            lifted.y += step_height;

            Vec3 across = lifted;
            if (axis[i] == 0) across.x += step[i];
            else              across.z += step[i];

            // Room to rise, and room to stand once across. Both, and in that
            // order: a body that rose into a ceiling and then moved would be
            // standing in it.
            if (!box_hits_world(world, reg, box_at(lifted, bottom_shift), &was) &&
                !box_hits_world(world, reg, box_at(across, bottom_shift), &was)) {
                // Down onto whatever is under it: the deepest drop that is
                // still free, found by halving the range. The body ends resting
                // on the surface rather than floating a step above it, and a
                // step out over a hole drops the whole way and falls from
                // there, which is what walking off a ledge already does.
                //
                // Zero is known free -- it is the position just tested -- so
                // the search only ever has to decide how much further down.
                float lo = 0.0f, hi = step_height;
                for (int tries = 0; tries < 8; ++tries) {
                    const float mid = 0.5f * (lo + hi);
                    Vec3        lower = across;
                    lower.y -= mid;
                    if (box_hits_world(world, reg, box_at(lower, bottom_shift), &was)) hi = mid;
                    else                                                             lo = mid;
                }
                across.y -= lo;

                pos = across;
                continue;
            }
        }

        // Blocked on this axis: keep the other two and record why.
        if (axis[i] == 1) {
            if (step[i] < 0.0f) res.on_ground = true;
            else                res.hit_ceiling = true;
        }
    }

    res.pos = pos;
    return res;
}

MoveResult move_body(const World &world, const NodeRegistry &reg, Vec3 feet_pos, Vec3 delta,
                     float bottom_shift, float step_height, float half_width, float height)
{
    float longest = std::fabs(delta.x);
    if (std::fabs(delta.y) > longest) longest = std::fabs(delta.y);
    if (std::fabs(delta.z) > longest) longest = std::fabs(delta.z);

    int steps = (int)std::ceil(longest / kMaxStep);
    if (steps < 1) steps = 1;
    if (steps > kMaxSteps) steps = kMaxSteps;

    const Vec3 part = delta * (1.0f / (float)steps);

    MoveResult res;
    res.pos = feet_pos;
    for (int i = 0; i < steps; ++i) {
        const MoveResult r =
            move_box(world, reg, res.pos, part, bottom_shift, step_height, half_width, height);
        res.pos         = r.pos;
        res.on_ground   = r.on_ground;
        res.hit_ceiling = res.hit_ceiling || r.hit_ceiling;
    }
    return res;
}

} // namespace cnp
