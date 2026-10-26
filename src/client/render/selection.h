#pragma once

#include "client/gfx/gfx.h"
#include "core/math/mat4.h"

namespace cnp {

// The outline around the block being pointed at.
//
// Drawn as the back faces of a cube inflated slightly past the block, with
// front faces culled: what survives is a thin shell showing around the edges.
// Cheaper than building line geometry, and it avoids depending on line width,
// which core profile OpenGL only nominally supports.
class SelectionRenderer {
public:
    bool init();
    void shutdown();

    // `block_min` is the minimum corner of the targeted block, in world units.
    void draw(Vec3 block_min, const Mat4 &projection, const Mat4 &view);

    // The same shell around a box of any size, in any colour. Another player is
    // a 0.6 x 1.8 x 0.6 box until there is a model to put there, and drawing
    // one costs a scale matrix rather than a second renderer.
    void draw_box(Vec3 min, Vec3 size, const float colour[4], const Mat4 &projection,
                  const Mat4 &view);

    // Triangles (`count` vertices, xyz each) moved to `at`, filled in one
    // colour and seen through when its alpha is: a track piece in the hand,
    // drawn where it would go before it is put down. Writes no depth, so what
    // is behind it still shows.
    void draw_tris(const float *xyz, int count, Vec3 at, const float colour[4],
                   const Mat4 &projection, const Mat4 &view);

private:
    gfx_shader m_shader{0};
    gfx_buffer m_vb{0};
    gfx_mesh   m_mesh{0};
    int        m_vertex_count = 0;
    int        m_loc_color = -1;

    gfx_buffer m_tri_vb{0};
    gfx_mesh   m_tri_mesh{0};
    int        m_tri_cap = 0;   // vertices m_tri_vb holds
};

} // namespace cnp
