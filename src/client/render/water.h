#pragma once

#include "client/gfx/gfx.h"
#include "core/math/mat4.h"

namespace cnp {

// The water in sight, as one shader's uniforms (CNP_WATER_GLSL in shaders.h).
// Every shader that draws into the player's view keeps one of these -- the
// world's, the sky's, a body's, a model's, an opening's -- so that all of them
// fade by the same water. It is the water of the WorldRenderer whose set_water
// is on, and none while no renderer's is: a portal's camera is another place.
// Defined in world_render.cpp, next to the water it reads.
struct WaterUniforms {
    void locate(gfx_shader shader);

    // Onto the bound shader, for the camera `projection` and `view` -- a plain
    // perspective one, or a portal's with its near plane made oblique. The cells go on
    // texture unit 1, and unit 0 is left active with nothing bound: the caller
    // binds its own texture there afterwards.
    void set(const Mat4 &projection, const Mat4 &view) const;
    void off() const;   // for a draw that is not in the world, like a blit

    int water = -1, lo = -1, hi = -1, colour = -1, cells = -1;
    int eye = -1, near = -1, ray_x = -1, ray_y = -1, ray_z = -1;
};

} // namespace cnp
