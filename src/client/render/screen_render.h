#pragma once

#include "client/gfx/gfx.h"
#include "core/math/mat4.h"
#include "core/math/vec.h"

// A screen in the world (core.render.screen): a flat rectangle showing a
// camera's picture (core.render.camera), lit by itself as a screen is, with
// faint scan lines; with no picture, the snow of a set with no signal.
//
// Not the portals' opening shader: that one samples its target by where the
// fragment is on the screen, which is right for a hole and wrong for a picture
// hung on a wall.

namespace cnp {

class ScreenRenderer {
public:
    bool init();
    void shutdown();

    // The picture `tex` (0: snow) on the rectangle about `center` spanned by
    // `right` and `up` (half its width and height each way).
    // `plain`: a photograph -- no scan lines, a little darker, its edge a frame's.
    void draw(gfx_texture tex, Vec3 center, Vec3 right, Vec3 up, float time, bool plain, const Mat4 &projection,
              const Mat4 &view);

private:
    gfx_shader m_shader{};
    gfx_buffer m_vb{};
    gfx_mesh   m_mesh{};
    int m_loc_center = -1, m_loc_right = -1, m_loc_up = -1, m_loc_tex = -1, m_loc_has = -1, m_loc_time = -1,
        m_loc_plain = -1;
};

} // namespace cnp
