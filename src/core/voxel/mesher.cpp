#include "core/voxel/mesher.h"

#include "core/media/face_tiles.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace cnp {
namespace {

// Face order: +X, -X, +Y, -Y, +Z, -Z. The shader's normal table indexes this.
int face_index(int axis, bool positive)
{
    return axis * 2 + (positive ? 0 : 1);
}

// Where the picture's own axes go on each face.
//
// The mesher walks a slice in two in-plane axes (u, v) chosen by rotating the
// axis index, and for three of the six faces that basis is not the one a
// PICTURE wants. A texture has a top and a left; a slice does not. Emitting the
// slice's own (u, v) as texture coordinates drew the side of every block upside
// down on +Z, turned half round on -Z, and transposed on both X faces -- which
// nothing noticed for as long as every block wore noise or a flat colour.
//
// What a face wants is: the picture's u along SCREEN RIGHT seen from outside
// it, and the picture's v along SCREEN DOWN. Down and not up, because v = 0 is
// the first row in memory, which is the top row of the file.
//
// Screen right, derived once rather than chosen: standing outside a face
// looking at it with world up, right = cross(forward, up).
//
//     +X  right -Z, down -Y        -X  right +Z, down -Y
//     +Y  right +X, down +Z        -Y  right +X, down -Z
//     +Z  right +X, down -Y        -Z  right -X, down -Y
//
// Each entry says which of the quad's two offsets feeds the texture axis, and
// whether it is measured from the far edge.
struct TexAxes {
    uint8_t u_from_v;   // 1 = the quad's v offset feeds texture u, 0 = its u offset
    uint8_t u_flip;
    uint8_t v_from_v;
    uint8_t v_flip;
};

constexpr TexAxes kTexAxes[6] = {
    {1, 1, 0, 1},   // +X
    {1, 0, 0, 1},   // -X
    {1, 0, 0, 0},   // +Y
    {1, 0, 0, 1},   // -Y
    {0, 0, 1, 1},   // +Z
    {0, 1, 1, 1},   // -Z
};

// Ambient occlusion for one corner of a face, from the three blocks touching it
// on the lit side. The classic rule: two solid sides fully occlude regardless
// of the corner, otherwise count what is there.
int vertex_ao(bool side1, bool side2, bool corner)
{
    if (side1 && side2) return 0;
    return 3 - ((int)side1 + (int)side2 + (int)corner);
}

// Which picture face `face` of a cell wears, given which way the cell is
// turned.
//
// The block table holds a block's pictures as it was REGISTERED. A turned cell
// shows, on its face `face`, whichever of the block's own faces the turn moved
// there -- which is face_source, the inverse of the orientation.
//
// What the byte MEANS is the block's business and not this function's: four
// quarter turns for a chest, one of twenty-four for a log, a face for a
// ladder. node_cell_orientation is the one place that knows, and it answers 0
// for a byte the block does not allow, so a cell somebody wrote a wild value
// into draws unturned rather than reading past a table.
uint16_t tile_for(const NodeDef &d, int face, uint8_t facing)
{
    // The ORIENTATION decides, and a byte of 0 is not the same question: a
    // wallmounted cell's 0 means "front points at +X", which is a turn. This
    // read as "facing == 0, nothing to do" first, and the wallmounted half of
    // the gate is what said so.
    const int o = node_cell_orientation(d.turn, facing);
    if (o == 0) return d.tex[face];
    return d.tex[face_source(face, o)];
}

// What a face looks like, for the purpose of deciding whether two of them can
// merge. Everything that varies across the quad has to be in here, or greedy
// meshing will merge faces that differ and the difference will vanish.
//
// The resolved TILE and not the facing byte, which is a strictly weaker
// question and the one that matters: two cells turned differently still wear
// the same picture on their top and their bottom, because those two faces do
// not move when a block turns about the vertical. Splitting on the byte would
// give up quads on every roof of turned blocks for nothing.
//
// The light is per corner now, and a face merges only when its four corners
// agree: a quad stretched over two faces whose light ramps across each of them
// would ramp once across both, and the shadow's edge would move.
struct Face {
    BlockId  block = kAir;
    uint16_t tex   = 0;
    uint8_t sky[4]  = {};
    uint8_t lamp[4] = {};
    uint8_t classic = 0;   // the packed light of the cell in front, for the classic look
    uint8_t ao[4] = {3, 3, 3, 3};
    bool    present = false;

    bool flat() const
    {
        return sky[0] == sky[1] && sky[0] == sky[2] && sky[0] == sky[3] &&
               lamp[0] == lamp[1] && lamp[0] == lamp[2] && lamp[0] == lamp[3];
    }

    bool merges_with(const Face &o) const
    {
        return present && o.present && block == o.block && tex == o.tex && flat() && o.flat() &&
               sky[0] == o.sky[0] && lamp[0] == o.lamp[0] && classic == o.classic &&
               ao[0] == o.ao[0] && ao[1] == o.ao[1] && ao[2] == o.ao[2] && ao[3] == o.ao[3];
    }
};

// How the block light at a cell falls on a face looking out along the face's
// normal: full where the light comes from in front, kGrazing where it runs
// along the face, kAway where it comes from behind it.
//
// Which way light comes from is not stored anywhere -- it is the slope of the
// flood fill itself, uphill being towards the lamp. Across the face's plane that
// slope is a central difference. Along the normal it can only be read one way,
// outwards, because inwards is the block the face is on; and a one-sided step
// of a Manhattan fill reads "the lamp is level with me" as "the lamp is behind
// me" (both lose a level). kNormalBias splits that difference.
inline constexpr float kGrazing    = 0.6f;
inline constexpr float kAway       = 0.25f;
inline constexpr float kNormalBias = 0.5f;

} // namespace

ChunkSnapshot snapshot_chunk(const World &world, ChunkPos pos)
{
    ChunkSnapshot s;
    snapshot_chunk_into(world, pos, s);
    return s;
}

void snapshot_chunk_into(const World &world, ChunkPos pos, ChunkSnapshot &s)
{
    s.pos = pos;
    s.blocks.resize((size_t)ChunkSnapshot::kDim * ChunkSnapshot::kDim * ChunkSnapshot::kDim);
    s.light.resize(s.blocks.size());

    // Emptied and not resized: empty means "nothing here is turned", and a
    // snapshot handed back to be filled again would otherwise keep the last
    // chunk's turns. clear() keeps the capacity, so the second chunk that needs
    // a plane does not allocate one.
    s.facing.clear();

    const int bx = pos.x * kChunkSize;
    const int by = pos.y * kChunkSize;
    const int bz = pos.z * kChunkSize;

    // The interior comes from one chunk; only the skirt touches neighbours.
    const Chunk *centre = world.find_chunk(pos);

    constexpr int kLo = -ChunkSnapshot::kPad, kHi = kChunkSize + ChunkSnapshot::kPad;
    for (int z = kLo; z < kHi; ++z) {
        for (int y = kLo; y < kHi; ++y) {
            for (int x = kLo; x < kHi; ++x) {
                const bool interior = (x >= 0 && y >= 0 && z >= 0 &&
                                       x < kChunkSize && y < kChunkSize && z < kChunkSize);
                BlockId id;
                uint8_t lv;
                if (interior && centre) {
                    id = centre->get(x, y, z);
                    lv = centre->light(x, y, z);
                } else {
                    id = world.get_block(bx + x, by + y, bz + z);
                    lv = world.get_light(bx + x, by + y, bz + z);
                }
                const uint8_t fc = (interior && centre)
                                       ? centre->facing(x, y, z)
                                       : world.get_facing(bx + x, by + y, bz + z);

                const size_t i = (size_t)ChunkSnapshot::index(x, y, z);
                s.blocks[i] = id;
                s.light[i]  = lv;

                // Allocated on the first turned cell and not before. Almost
                // every chunk in a world has none, and 39304 bytes a snapshot
                // is a third again on top of the 118 KB the blocks and the
                // light already cost.
                if (fc != 0) {
                    if (s.facing.empty()) s.facing.assign(s.blocks.size(), 0);
                    s.facing[i] = fc;
                }
            }
        }
    }
}

std::vector<Vertex> mesh_snapshot(const ChunkSnapshot &snap, const NodeRegistry &reg)
{
    std::vector<Vertex> out;
    mesh_snapshot_into(snap, reg, out);
    return out;
}

void mesh_snapshot_into(const ChunkSnapshot &snap, const NodeRegistry &reg,
                        std::vector<Vertex> &out, std::vector<Vertex> *alpha,
                        std::vector<PartCell> *parts, std::vector<LiquidCell> *liquids)
{
    if (parts) parts->clear();
    if (liquids) liquids->clear();

    // Whether a block hides the face behind it is the registry's answer now,
    // not "is the id nonzero". Glass and leaves are the whole point: they are
    // solid to walk on and transparent to look through.
    auto is_solid = [&reg](BlockId id) { return reg.is_opaque(id); };

    // Which blocks produce faces at all, and which faces survive.
    //
    // Two rules, and the second one is why glass does not look like a solid
    // block of fog: a translucent face against MORE OF ITSELF is dropped. A
    // wall of glass three deep would otherwise draw six layers of surface,
    // every one of them blended over the last, and come out nearly opaque.
    // Against a different block -- glass over water -- both faces stay, which
    // is the behaviour a mixed pane needs.
    // A block drawn as BOXES is drawn by somebody else: it is not in either
    // stream, whatever it is made of. Asked by name rather than left to fall out
    // of "not opaque and not translucent" -- which is the bucket air is in, and
    // a chest that stopped being drawn because it looked like air to this
    // function would stop being drawn the day anything else changed that
    // answer.
    //
    // Belt and braces today, and said so rather than left to be discovered: the
    // registry already forces a block with boxes to be non-opaque, and a block
    // that is neither opaque nor translucent draws nothing here anyway. A
    // sabotage of this line alone stays green for that reason. The two would
    // come apart the moment a translucent nodebox existed -- a glass lid -- and
    // then this is the line that keeps it out of the alpha stream.
    // Not a cube: boxes, or a model, or a track piece, or the cells under
    // one. One predicate because the question the mesher asks is "do I draw
    // this cell", and a block drawn from a glTF is no more a cube than a
    // nodebox is.
    auto boxed = [&reg](BlockId id) { return !reg.get(id).is_cube(); };
    // A liquid is not a cube either: it is drawn from the cells collected
    // below, at its own height, and never in these two streams.
    auto draws = [&](BlockId id) {
        return !boxed(id) && !reg.is_liquid(id) && (reg.is_opaque(id) || reg.is_translucent(id));
    };
    auto hidden_by = [&reg](BlockId self, BlockId neighbour) {
        return reg.is_opaque(neighbour) || (reg.is_translucent(self) && neighbour == self);
    };

    out.clear();
    if (alpha) alpha->clear();

    // Light can be in a cell that is not opaque, and in one that glows.
    const auto usable = [&](const int c[3]) {
        const BlockId b = snap.block(c[0], c[1], c[2]);
        return !reg.is_opaque(b) || reg.light_source(b) != 0;
    };
    const auto lamp_at = [&](const int c[3]) { return (int)block_light(snap.light_at(c[0], c[1], c[2])); };

    // The brightness of cell c's block light on a face whose normal runs `dir`
    // along `axis`. See kGrazing.
    const auto facing_light = [&](const int c[3], int axis, int dir) -> float {
        const int l0 = lamp_at(c);
        if (l0 == 0) return 0.0f;
        const float bright = light_curve((float)l0);
        if (reg.light_source(snap.block(c[0], c[1], c[2])) != 0) return bright;

        float g[3];
        for (int a = 0; a < 3; ++a) {
            int p[3] = {c[0], c[1], c[2]}, m[3] = {c[0], c[1], c[2]};
            if (a == axis) {
                p[a] += dir;
                g[a] = (float)((usable(p) ? lamp_at(p) : l0) - l0) + kNormalBias;
                continue;
            }
            ++p[a];
            --m[a];
            const bool  hp = usable(p), hm = usable(m);
            const float lp = (float)(hp ? lamp_at(p) : l0), lm = (float)(hm ? lamp_at(m) : l0);
            g[a] = (lp - lm) * ((hp && hm) ? 0.5f : 1.0f);
        }
        const float len = std::sqrt(g[0] * g[0] + g[1] * g[1] + g[2] * g[2]);
        const float d   = g[axis] / len;   // never 0/0: the normal's share is a whole number and a half
        const float f   = d >= 0.0f ? kGrazing + (1.0f - kGrazing) * d : kGrazing + (kGrazing - kAway) * d;
        return bright * f;
    };

    // The cells somebody else draws, collected in one walk of the chunk. Only
    // when a caller asked for them: a caller with no pass to draw them in gets
    // a hole, which is the honest answer -- see the header.
    if (parts) {
        for (int z = 0; z < kChunkSize; ++z)
            for (int y = 0; y < kChunkSize; ++y)
                for (int x = 0; x < kChunkSize; ++x) {
                    const BlockId id = snap.block(x, y, z);
                    if (id == kAir || !boxed(id)) continue;
                    // The cells under a road: nothing to draw, the road is.
                    if (reg.get(id).turn == NodeTurn::Height) continue;

                    PartCell c;
                    c.x      = (uint8_t)x;
                    c.y      = (uint8_t)y;
                    c.z      = (uint8_t)z;
                    c.block  = id;
                    c.facing = snap.facing_at(x, y, z);

                    // The light of the cell the boxes stand IN, and not of a
                    // neighbour: a box is inside its node, so that is the light
                    // falling on it. A cube's faces each take their neighbour's
                    // light because each face is on a wall of the cell.
                    c.light = snap.light_at(x, y, z);
                    parts->push_back(c);
                }
    }

    // The liquid cells, and the shape of each one's surface.
    //
    // A cell with liquid over it is full to the top and has no surface of its
    // own. Otherwise its surface is as high as the fullest of the liquid cells
    // meeting at each corner -- a cell over which more liquid stands counting
    // as full -- which is what slopes the top from the high side to the low one
    // and puts the same height on both sides of every shared edge.
    if (liquids) {
        const auto height = [&](int x, int y, int z) -> int {
            const BlockId id = snap.block(x, y, z);
            if (!reg.is_liquid(id)) return -1;
            if (reg.is_liquid(snap.block(x, y + 1, z))) return 16;
            return (int)reg.liquid_fill(id) * 2;
        };

        static const int kSide[4][3] = {{1, 0, 0}, {-1, 0, 0}, {0, 0, 1}, {0, 0, -1}};
        static const int kSideFace[4] = {0, 1, 4, 5};

        for (int z = 0; z < kChunkSize; ++z)
            for (int y = 0; y < kChunkSize; ++y)
                for (int x = 0; x < kChunkSize; ++x) {
                    const BlockId id = snap.block(x, y, z);
                    if (!reg.is_liquid(id)) continue;

                    LiquidCell c;
                    c.x     = (uint8_t)x;
                    c.y     = (uint8_t)y;
                    c.z     = (uint8_t)z;
                    c.block = id;

                    static const int kLook[6][3] = {{1, 0, 0}, {-1, 0, 0}, {0, 1, 0},
                                                    {0, -1, 0}, {0, 0, 1}, {0, 0, -1}};
                    for (int f = 0; f < 6; ++f)
                        c.light[f] = snap.light_at(x + kLook[f][0], y + kLook[f][1], z + kLook[f][2]);

                    // The sun and the block light apart, each the mean of what
                    // is lit around the corner; a corner with nothing but
                    // walls around it keeps the face's own light.
                    for (int corner = 0; corner < 4; ++corner) {
                        const int cx = corner & 1, cz = corner >> 1;
                        int       sun = 0, lamp = 0, n = 0;
                        for (int dz = cz - 1; dz <= cz; ++dz)
                            for (int dx = cx - 1; dx <= cx; ++dx) {
                                if (reg.is_opaque(snap.block(x + dx, y + 1, z + dz))) continue;
                                const uint8_t l = snap.light_at(x + dx, y + 1, z + dz);
                                sun += l >> 4;
                                lamp += l & 15;
                                ++n;
                            }
                        c.top_light[corner] =
                            n == 0 ? c.light[2]
                                   : (uint8_t)((((sun + n / 2) / n) << 4) | ((lamp + n / 2) / n));
                    }

                    const bool covered = reg.is_liquid(snap.block(x, y + 1, z));
                    for (int corner = 0; corner < 4; ++corner) {
                        if (covered) {
                            c.h[corner] = 16;
                            continue;
                        }
                        const int cx = corner & 1, cz = corner >> 1;
                        int       best = 0;
                        for (int dz = cz - 1; dz <= cz; ++dz)
                            for (int dx = cx - 1; dx <= cx; ++dx)
                                best = std::max(best, height(x + dx, y, z + dz));
                        c.h[corner] = (uint8_t)best;
                    }

                    // A face against liquid is inside the body of water; a face
                    // against something that hides it is not seen. The top is
                    // drawn under a block that hides it only when the surface
                    // stops short of that block, or there is a gap to see into.
                    const auto open = [&](int nx, int ny, int nz) {
                        const BlockId n = snap.block(nx, ny, nz);
                        return !reg.is_liquid(n) && !reg.is_opaque(n);
                    };
                    for (int side = 0; side < 4; ++side)
                        if (open(x + kSide[side][0], y, z + kSide[side][2]))
                            c.faces |= (uint8_t)(1 << kSideFace[side]);
                    if (!covered) {
                        const bool low = c.h[0] < 16 || c.h[1] < 16 || c.h[2] < 16 || c.h[3] < 16;
                        if (open(x, y + 1, z) || low) c.faces |= 1 << 2;
                    }
                    if (open(x, y - 1, z)) c.faces |= 1 << 3;

                    // Every cell, faces or not: the renderer draws the faces
                    // and also needs the whole body, to know how much of a
                    // line of sight runs through water.
                    liquids->push_back(c);
                }
    }

    // Standard greedy meshing: for each axis and each of the two facings, walk
    // the slices, build a mask of faces, and grow rectangles out of the mask.
    // thread_local so each meshing worker keeps its own, and none of them
    // reallocates after the first chunk.
    static thread_local std::vector<Face> mask;
    mask.assign((size_t)kChunkSize * kChunkSize, Face{});

    for (int axis = 0; axis < 3; ++axis) {
        const int u = (axis + 1) % 3;      // first in-plane axis
        const int v = (axis + 2) % 3;      // second in-plane axis

        for (int positive = 1; positive >= 0; --positive) {
            const int dir  = positive ? 1 : -1;
            const int face = face_index(axis, positive != 0);

            for (int slice = 0; slice < kChunkSize; ++slice) {
                std::fill(mask.begin(), mask.end(), Face{});

                for (int j = 0; j < kChunkSize; ++j) {
                    for (int i = 0; i < kChunkSize; ++i) {
                        int p[3];
                        p[axis] = slice;
                        p[u]    = i;
                        p[v]    = j;

                        const BlockId self = snap.block(p[0], p[1], p[2]);
                        if (!draws(self)) continue;

                        int n[3] = {p[0], p[1], p[2]};
                        n[axis] += dir;
                        if (hidden_by(self, snap.block(n[0], n[1], n[2]))) continue;

                        Face f;
                        f.present = true;
                        f.block   = snap.block(p[0], p[1], p[2]);
                        f.tex     = tile_for(reg.get(f.block), face,
                                             snap.facing_at(p[0], p[1], p[2]));

                        // A block that glows is lit by itself on every face,
                        // whatever the cell in front of it holds.
                        const float self_glow = light_curve((float)reg.light_source(self));
                        f.classic = snap.light_at(n[0], n[1], n[2]);

                        // Ambient occlusion and light, sampled in the plane one
                        // step out along the face normal. The four corners
                        // follow the (u,v) order the quad is emitted in below.
                        for (int corner = 0; corner < 4; ++corner) {
                            const int du = (corner == 1 || corner == 2) ? 1 : -1;
                            const int dv = (corner == 2 || corner == 3) ? 1 : -1;

                            int s1[3] = {n[0], n[1], n[2]};
                            int s2[3] = {n[0], n[1], n[2]};
                            int cn[3] = {n[0], n[1], n[2]};
                            s1[u] += du;
                            s2[v] += dv;
                            cn[u] += du;
                            cn[v] += dv;

                            const bool o1 = is_solid(snap.block(s1[0], s1[1], s1[2]));
                            const bool o2 = is_solid(snap.block(s2[0], s2[1], s2[2]));
                            const bool oc = is_solid(snap.block(cn[0], cn[1], cn[2]));
                            f.ao[corner] = (uint8_t)vertex_ao(o1, o2, oc);

                            // The mean of the cells meeting at the corner that
                            // light can be in. The diagonal one only when the
                            // two beside it do not wall it off, or light leaks
                            // through the corner of two blocks.
                            float sky = 0.0f, lamp = 0.0f;
                            int   cells = 0;
                            const auto take = [&](const int c[3]) {
                                sky += light_curve((float)sun_light(snap.light_at(c[0], c[1], c[2])));
                                lamp += facing_light(c, axis, dir);
                                ++cells;
                            };
                            take(n);
                            if (usable(s1)) take(s1);
                            if (usable(s2)) take(s2);
                            if (usable(cn) && !(o1 && o2)) take(cn);
                            lamp = std::max(lamp / (float)cells, self_glow);
                            f.sky[corner]  = (uint8_t)std::lround(sky / (float)cells * 255.0f);
                            f.lamp[corner] = (uint8_t)std::lround(std::min(lamp, 1.0f) * 255.0f);
                        }

                        mask[(size_t)j * kChunkSize + i] = f;
                    }
                }

                // Grow rectangles.
                for (int j = 0; j < kChunkSize; ++j) {
                    for (int i = 0; i < kChunkSize;) {
                        const Face &start = mask[(size_t)j * kChunkSize + i];
                        if (!start.present) { ++i; continue; }

                        int w = 1;
                        while (i + w < kChunkSize &&
                               start.merges_with(mask[(size_t)j * kChunkSize + i + w]))
                            ++w;

                        int h = 1;
                        bool grew = true;
                        while (j + h < kChunkSize && grew) {
                            for (int k = 0; k < w; ++k) {
                                if (!start.merges_with(mask[(size_t)(j + h) * kChunkSize + i + k])) {
                                    grew = false;
                                    break;
                                }
                            }
                            if (grew) ++h;
                        }

                        // Emit. The quad sits on the face between the block and
                        // its empty neighbour, so a positive facing is offset by
                        // one along the axis.
                        int base[3];
                        base[axis] = slice + (positive ? 1 : 0);
                        base[u]    = i;
                        base[v]    = j;

                        int du[3] = {0, 0, 0};
                        int dv[3] = {0, 0, 0};
                        du[u] = w;
                        dv[v] = h;

                        const int cx[4][3] = {
                            {base[0],                 base[1],                 base[2]},
                            {base[0] + du[0],         base[1] + du[1],         base[2] + du[2]},
                            {base[0] + du[0] + dv[0], base[1] + du[1] + dv[1], base[2] + du[2] + dv[2]},
                            {base[0] + dv[0],         base[1] + dv[1],         base[2] + dv[2]},
                        };
                        // The quad's own offsets, then the picture's axes over
                        // them. `a` runs along the slice's u axis and spans w,
                        // `b` along its v axis and spans h.
                        const int corner_a[4] = {0, w, w, 0};
                        const int corner_b[4] = {0, 0, h, h};

                        const TexAxes &ax = kTexAxes[face];
                        int uv[4][2];
                        for (int c = 0; c < 4; ++c) {
                            const int au   = ax.u_from_v ? corner_b[c] : corner_a[c];
                            const int span_u = ax.u_from_v ? h : w;
                            const int av   = ax.v_from_v ? corner_b[c] : corner_a[c];
                            const int span_v = ax.v_from_v ? h : w;

                            uv[c][0] = ax.u_flip ? span_u - au : au;
                            uv[c][1] = ax.v_flip ? span_v - av : av;
                        }

                        Vertex quad[4];
                        for (int c = 0; c < 4; ++c) {
                            quad[c].x       = (uint8_t)cx[c][0];
                            quad[c].y       = (uint8_t)cx[c][1];
                            quad[c].z       = (uint8_t)cx[c][2];
                            quad[c].face_ao = pack_face_ao(face, start.ao[c], start.tex);
                            quad[c].u       = (uint8_t)uv[c][0];
                            quad[c].v       = (uint8_t)uv[c][1];
                            quad[c].sky     = start.sky[c];
                            quad[c].lamp    = start.lamp[c];
                            quad[c].classic = start.classic;
                            quad[c].tex     = (uint8_t)(start.tex & 0xFF);
                        }

                        // Winding. The corner order above traces the rectangle
                        // counter-clockwise when seen from +axis; a face looking
                        // the other way has to be reversed or back-face culling
                        // eats it from the only side it is visible from.
                        //
                        // Whether that base order is already correct alternates
                        // with the axis, because (u, v) = (axis+1, axis+2) is a
                        // right-handed basis for every axis but the cross
                        // product changes sign with the facing.
                        const bool flip = (positive == 0);

                        // Which stream this rectangle belongs to. Greedy
                        // merging already keys on the block id, so a rectangle
                        // is never part opaque and part not.
                        if (!alpha && reg.is_translucent(start.block)) {
                            for (int jj = 0; jj < h; ++jj)
                                for (int ii = 0; ii < w; ++ii)
                                    mask[(size_t)(j + jj) * kChunkSize + i + ii] = Face{};
                            continue;
                        }
                        std::vector<Vertex> &sink =
                            reg.is_translucent(start.block) ? *alpha : out;

                        auto emit = [&](int a, int b, int c) {
                            sink.push_back(quad[a]);
                            sink.push_back(quad[b]);
                            sink.push_back(quad[c]);
                        };
                        if (!flip) { emit(0, 1, 2); emit(0, 2, 3); }
                        else       { emit(0, 2, 1); emit(0, 3, 2); }

                        // Consume the rectangle.
                        for (int jj = 0; jj < h; ++jj)
                            for (int ii = 0; ii < w; ++ii)
                                mask[(size_t)(j + jj) * kChunkSize + i + ii] = Face{};

                        i += w;
                    }
                }
            }
        }
    }
}

} // namespace cnp

namespace cnp {

ChunkVisibility compute_visibility(const ChunkSnapshot &snapshot, const NodeRegistry &reg)
{
    ChunkVisibility vis;

    constexpr int  kN    = kChunkSize;
    constexpr int  kLast = kChunkSize - 1;
    constexpr size_t kCells = (size_t)kN * kN * kN;

    // One flood fill over the whole chunk, region by region, rather than six
    // fills from six faces: a region is what joins faces, and finding the
    // regions once answers all fifteen pairs at the same time.
    std::vector<uint8_t> seen(kCells, 0);
    std::vector<int>     stack;
    stack.reserve(256);

    auto local = [](int x, int y, int z) { return (x * kN + y) * kN + z; };

    // One region's workings, cleared and refilled instead of allocated per
    // region: a chunk of cave is a chunk with dozens of them, and this runs on
    // every chunk of every world for as long as the world is open.
    Region                 tmp;
    uint64_t               tmp_face[6][kFaceWords];
    std::vector<FaceCells> loose;          // grouped by region here, by face below
    bool                   overflowed = false;

    for (int x = 0; x < kN; ++x)
        for (int y = 0; y < kN; ++y)
            for (int z = 0; z < kN; ++z) {
                const int start = local(x, y, z);
                if (seen[(size_t)start]) continue;
                if (reg.is_opaque(snapshot.block(x, y, z))) {
                    seen[(size_t)start] = 1;
                    continue;
                }

                // Which faces this region reaches. A cell on an edge is on two
                // of them and a corner on three, so this is a set and not a
                // single answer -- assigning each cell to one face leaves a
                // hole along a chunk edge, where nobody looks for it.
                int touched = 0;

                tmp = Region{};
                std::memset(tmp_face, 0, sizeof tmp_face);

                seen[(size_t)start] = 1;
                stack.clear();
                stack.push_back(start);

                while (!stack.empty()) {
                    const int at = stack.back();
                    stack.pop_back();

                    const int cz = at % kN;
                    const int cy = (at / kN) % kN;
                    const int cx = at / (kN * kN);

                    // Where the region is, to a quarter of the chunk's width.
                    {
                        const int bit =
                            ((cx / 4) * kCoarseDim + (cy / 4)) * kCoarseDim + (cz / 4);
                        tmp.coarse[bit >> 6] |= 1ull << (bit & 63);
                    }

                    // And where it meets a face, cell by cell, indexed by the
                    // two axes that are not the face's own so that the chunk on
                    // the other side indexes the same cell.
                    auto mark = [&](int f, int cell) {
                        tmp_face[f][cell >> 6] |= 1ull << (cell & 63);
                    };
                    if (cx == 0)     { touched |= 1 << 0; mark(0, cy * kN + cz); }
                    if (cx == kLast) { touched |= 1 << 1; mark(1, cy * kN + cz); }
                    if (cy == 0)     { touched |= 1 << 2; mark(2, cx * kN + cz); }
                    if (cy == kLast) { touched |= 1 << 3; mark(3, cx * kN + cz); }
                    if (cz == 0)     { touched |= 1 << 4; mark(4, cx * kN + cy); }
                    if (cz == kLast) { touched |= 1 << 5; mark(5, cx * kN + cy); }

                    static const int kStep[6][3] = {{-1, 0, 0}, {1, 0, 0}, {0, -1, 0},
                                                    {0, 1, 0},  {0, 0, -1}, {0, 0, 1}};
                    for (const auto &d : kStep) {
                        const int nx = cx + d[0], ny = cy + d[1], nz = cz + d[2];
                        if (nx < 0 || ny < 0 || nz < 0 || nx >= kN || ny >= kN || nz >= kN)
                            continue;   // the skirt is somebody else's chunk
                        const int next = local(nx, ny, nz);
                        if (seen[(size_t)next]) continue;
                        seen[(size_t)next] = 1;
                        if (reg.is_opaque(snapshot.block(nx, ny, nz))) continue;
                        stack.push_back(next);
                    }
                }

                for (int a = 0; a < 6; ++a)
                    for (int b = a + 1; b < 6; ++b)
                        if ((touched & (1 << a)) && (touched & (1 << b))) vis.join(a, b);

                // A region that reaches no face is a sealed room: nothing
                // crossing this chunk can get into it and it is not a way
                // through, so it is summed into nothing and numbered as
                // nothing.
                if (touched == 0) continue;

                if (vis.regions.size() >= (size_t)kMaxRegions) {
                    overflowed = true;
                    continue;
                }

                const uint8_t id = (uint8_t)vis.regions.size();
                tmp.faces        = (uint8_t)touched;
                vis.regions.push_back(tmp);

                for (int f = 0; f < 6; ++f) {
                    if (!(touched & (1 << f))) continue;
                    FaceCells fc;
                    fc.face   = (uint8_t)f;
                    fc.region = id;
                    std::memcpy(fc.cells, tmp_face[f], sizeof fc.cells);
                    loose.push_back(fc);
                }
            }

    // More ways through this chunk than the walk's mask has bits. The fifteen
    // bits above are still exactly right, so the answer is to keep them and say
    // nothing finer -- the walk then crosses this one chunk the coarse way and
    // goes on carrying regions everywhere else.
    if (overflowed) {
        vis.regions.clear();
        loose.clear();
    }

    vis.face_cells.reserve(loose.size());
    for (int f = 0; f < 6; ++f) {
        vis.face_begin[f] = (uint16_t)vis.face_cells.size();
        for (const FaceCells &fc : loose)
            if (fc.face == f) vis.face_cells.push_back(fc);
    }
    vis.face_begin[6] = (uint16_t)vis.face_cells.size();

    return vis;
}

} // namespace cnp
