#pragma once

#include "core/math/vec.h"

namespace cnp {

// Column-major 4x4, the layout OpenGL expects from glUniformMatrix4fv with
// transpose=false: m[row + 4*col].
//
// This matches raylib's *index* convention exactly -- raylib's `Matrix.mN` also
// means row + 4*col -- but NOT raylib's memory layout. raylib declares the
// struct as `float m0, m4, m8, m12; float m1, m5, ...`, so the fields sit in
// memory in a different order than their names suggest, and raymath reshuffles
// them in MatrixToFloatV before upload. Converting between the two therefore
// has to go field by field; a memcpy compiles fine and produces a transposed
// mess. See gfx_raylib.cpp, to_raylib().
struct Mat4 {
    float m[16] = {1, 0, 0, 0,
                   0, 1, 0, 0,
                   0, 0, 1, 0,
                   0, 0, 0, 1};

    float &at(int row, int col) { return m[row + 4 * col]; }
    float at(int row, int col) const { return m[row + 4 * col]; }
};

Mat4 mat4_identity();
Mat4 mat4_multiply(const Mat4 &a, const Mat4 &b);   // returns a*b
Mat4 mat4_invert(const Mat4 &a);
Mat4 mat4_transpose(const Mat4 &a);

Mat4 mat4_translate(Vec3 t);
Mat4 mat4_scale(Vec3 s);
Mat4 mat4_rotate_x(float radians);
Mat4 mat4_rotate_y(float radians);
Mat4 mat4_rotate_z(float radians);

// Right-handed, depth mapped to [-1, 1], m11 == -1. Deliberately identical to
// raylib's MatrixPerspective: the oblique near plane in portal_math.cpp derives
// its Q vector from exactly these entries, so the two must not drift apart.
Mat4 mat4_perspective(float fov_y_radians, float aspect, float near_plane, float far_plane);

// Orthographic, the same conventions: right-handed, depth to [-1, 1], near and
// far as distances down -Z.
Mat4 mat4_ortho(float left, float right, float bottom, float top, float near_plane, float far_plane);

// View matrix from an eye point and an already normalized forward direction.
//
// There is no look_at(eye, target) here on purpose. Building the basis from
// `target - eye` loses precision once the two are far apart in float32, which
// is the normal case for a portal camera: the virtual camera can sit thousands
// of nodes away from what it looks at, or in another world entirely. The fork
// this project comes from needed a dedicated setRenderViewMatrix() to work
// around exactly that (DESIGN.md 2.3). Here the direction is simply an input.
Mat4 mat4_look_to(Vec3 eye, Vec3 forward, Vec3 up);

Vec3 mat4_transform_point(const Mat4 &a, Vec3 v);
Vec3 mat4_transform_dir(const Mat4 &a, Vec3 v);
Vec4 mat4_transform_vec4(const Mat4 &a, Vec4 v);

} // namespace cnp
