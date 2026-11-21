#pragma once

#include "client/gfx/gfx.h"

// Being hurt, on the screen: the edge of the view goes red for a moment,
// deepest on the side the blow came from -- a creature behind you reddens
// the bottom edge, one on your left the left -- and all round, fainter, for a
// hurt that came from no side (a fall, a burn). Drawn over the world and
// under the HUD by one full-screen quad.

namespace cnp {

class HurtRenderer {
public:
    static constexpr int kMarks = 8;

    // One recent hurt: where it came from on the screen (x right, y up, a unit
    // vector; ignored when `from_side` is false) and how strong it still is, 0..1.
    struct Mark {
        float x = 0.0f, y = 0.0f;
        float strength  = 0.0f;
        bool  from_side = false;
    };

    bool init();
    void shutdown();
    void draw(const Mark *marks, int count, int width, int height);

private:
    gfx_shader m_shader{};
    gfx_buffer m_vb{};
    gfx_mesh   m_mesh{};
    int        m_loc_aspect = -1, m_loc_count = -1, m_loc_marks = -1;
};

} // namespace cnp
