#pragma once

#include "client/gfx/gfx.h"
#include "core/math/mat4.h"
#include "core/math/vec.h"

#include <vector>

// Effects a mod draws inside a render pass (core.render.fx): light, not
// things. One quad and one program; the kind picks what its fragments are --
// a glow, a flame, a shockwave on the ground, frost, a bolt between two
// points, a blade's arc, sparks, a vortex -- each drawn from noise and the
// effect's age, added onto what is behind it (GFX_BLEND_ADD), tested against
// the world's depth but writing none, so walls hide them and they never hide
// each other.
//
// `t` is how far through its life the effect is, 0 to 1: the shader grows,
// sweeps and fades by it, so a mod only counts its effect down.

namespace cnp {

enum class FxKind : int {
    Glow = 0,
    Flame,
    Ring,     // on the ground, about the point
    Frost,    // on the ground
    Bolt,     // from the point to `to`
    Slash,
    Sparks,
    Vortex,
    Jet,      // a flame out of a nozzle at the point, blown towards `to`
    Count,
};

// The kind by its name in core.render.fx, or -1.
int fx_kind_named(const char *name);

class FxRenderer {
public:
    bool init();
    void shutdown();

    void draw(FxKind kind, Vec3 at, Vec3 to, float size, const float rgba[4], float t, float time,
              const Mat4 &projection, const Mat4 &view);

    // Dark marks on the ground (a kart's tyres sliding): triangles, four
    // floats a corner -- x, y, z and the time it was left -- uploaded only
    // when `changed`; each fades out over `life` seconds and is gone after.
    // Mixed over what is behind them, not added: a mark darkens the road.
    void draw_marks(const std::vector<float> &corners, bool changed, float time, float life,
                    const Mat4 &projection, const Mat4 &view);

private:
    gfx_shader m_shader{};
    gfx_buffer m_vb{};
    gfx_mesh   m_mesh{};
    int m_loc_kind = -1, m_loc_center = -1, m_loc_end = -1, m_loc_size = -1, m_loc_color = -1,
        m_loc_t = -1, m_loc_time = -1;

    gfx_shader m_mark_shader{};
    gfx_buffer m_mark_vb{};
    gfx_mesh   m_mark_mesh{};
    int        m_mark_capacity = 0, m_mark_count = 0;
    int        m_loc_mark_time = -1, m_loc_mark_life = -1;
};

} // namespace cnp
