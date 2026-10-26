#include "core/math/mat4.h"

#include <cmath>

namespace cnp {

Mat4 mat4_identity()
{
    return Mat4{};
}

Mat4 mat4_multiply(const Mat4 &a, const Mat4 &b)
{
    Mat4 r;
    for (int col = 0; col < 4; ++col) {
        for (int row = 0; row < 4; ++row) {
            float sum = 0.0f;
            for (int k = 0; k < 4; ++k) sum += a.at(row, k) * b.at(k, col);
            r.at(row, col) = sum;
        }
    }
    return r;
}

Mat4 mat4_transpose(const Mat4 &a)
{
    Mat4 r;
    for (int col = 0; col < 4; ++col)
        for (int row = 0; row < 4; ++row) r.at(row, col) = a.at(col, row);
    return r;
}

Mat4 mat4_invert(const Mat4 &a)
{
    // Cofactor expansion. General enough for projection matrices too, which the
    // portal code inverts when it walks a clip plane back into eye space.
    const float *m = a.m;

    const float a00 = m[0],  a01 = m[1],  a02 = m[2],  a03 = m[3];
    const float a10 = m[4],  a11 = m[5],  a12 = m[6],  a13 = m[7];
    const float a20 = m[8],  a21 = m[9],  a22 = m[10], a23 = m[11];
    const float a30 = m[12], a31 = m[13], a32 = m[14], a33 = m[15];

    const float b00 = a00 * a11 - a01 * a10;
    const float b01 = a00 * a12 - a02 * a10;
    const float b02 = a00 * a13 - a03 * a10;
    const float b03 = a01 * a12 - a02 * a11;
    const float b04 = a01 * a13 - a03 * a11;
    const float b05 = a02 * a13 - a03 * a12;
    const float b06 = a20 * a31 - a21 * a30;
    const float b07 = a20 * a32 - a22 * a30;
    const float b08 = a20 * a33 - a23 * a30;
    const float b09 = a21 * a32 - a22 * a31;
    const float b10 = a21 * a33 - a23 * a31;
    const float b11 = a22 * a33 - a23 * a32;

    const float det = b00 * b11 - b01 * b10 + b02 * b09 + b03 * b08 - b04 * b07 + b05 * b06;
    if (std::fabs(det) < 1e-20f) return Mat4{};   // singular: identity beats NaN
    const float inv = 1.0f / det;

    Mat4 r;
    r.m[0]  = ( a11 * b11 - a12 * b10 + a13 * b09) * inv;
    r.m[1]  = (-a01 * b11 + a02 * b10 - a03 * b09) * inv;
    r.m[2]  = ( a31 * b05 - a32 * b04 + a33 * b03) * inv;
    r.m[3]  = (-a21 * b05 + a22 * b04 - a23 * b03) * inv;
    r.m[4]  = (-a10 * b11 + a12 * b08 - a13 * b07) * inv;
    r.m[5]  = ( a00 * b11 - a02 * b08 + a03 * b07) * inv;
    r.m[6]  = (-a30 * b05 + a32 * b02 - a33 * b01) * inv;
    r.m[7]  = ( a20 * b05 - a22 * b02 + a23 * b01) * inv;
    r.m[8]  = ( a10 * b10 - a11 * b08 + a13 * b06) * inv;
    r.m[9]  = (-a00 * b10 + a01 * b08 - a03 * b06) * inv;
    r.m[10] = ( a30 * b04 - a31 * b02 + a33 * b00) * inv;
    r.m[11] = (-a20 * b04 + a21 * b02 - a23 * b00) * inv;
    r.m[12] = (-a10 * b09 + a11 * b07 - a12 * b06) * inv;
    r.m[13] = ( a00 * b09 - a01 * b07 + a02 * b06) * inv;
    r.m[14] = (-a30 * b03 + a31 * b01 - a32 * b00) * inv;
    r.m[15] = ( a20 * b03 - a21 * b01 + a22 * b00) * inv;
    return r;
}

Mat4 mat4_translate(Vec3 t)
{
    Mat4 r;
    r.at(0, 3) = t.x;
    r.at(1, 3) = t.y;
    r.at(2, 3) = t.z;
    return r;
}

Mat4 mat4_scale(Vec3 s)
{
    Mat4 r;
    r.at(0, 0) = s.x;
    r.at(1, 1) = s.y;
    r.at(2, 2) = s.z;
    return r;
}

Mat4 mat4_rotate_x(float radians)
{
    const float c = std::cos(radians), s = std::sin(radians);
    Mat4 r;
    r.at(1, 1) = c;  r.at(1, 2) = -s;
    r.at(2, 1) = s;  r.at(2, 2) = c;
    return r;
}

Mat4 mat4_rotate_z(float radians)
{
    const float c = std::cos(radians), s = std::sin(radians);
    Mat4 r;
    r.at(0, 0) = c;  r.at(0, 1) = -s;
    r.at(1, 0) = s;  r.at(1, 1) = c;
    return r;
}

Mat4 mat4_rotate_y(float radians)
{
    const float c = std::cos(radians), s = std::sin(radians);
    Mat4 r;
    r.at(0, 0) =  c;  r.at(0, 2) = s;
    r.at(2, 0) = -s;  r.at(2, 2) = c;
    return r;
}

Mat4 mat4_perspective(float fov_y_radians, float aspect, float near_plane, float far_plane)
{
    const float top   = near_plane * std::tan(fov_y_radians * 0.5f);
    const float right = top * aspect;
    const float fn    = far_plane - near_plane;

    Mat4 r;
    for (float &v : r.m) v = 0.0f;
    r.at(0, 0) = near_plane / right;
    r.at(1, 1) = near_plane / top;
    r.at(2, 2) = -(far_plane + near_plane) / fn;
    r.at(3, 2) = -1.0f;
    r.at(2, 3) = -(far_plane * near_plane * 2.0f) / fn;
    return r;
}

Mat4 mat4_ortho(float left, float right, float bottom, float top, float near_plane, float far_plane)
{
    Mat4 r;
    for (float &v : r.m) v = 0.0f;
    r.at(0, 0) = 2.0f / (right - left);
    r.at(1, 1) = 2.0f / (top - bottom);
    r.at(2, 2) = -2.0f / (far_plane - near_plane);
    r.at(0, 3) = -(right + left) / (right - left);
    r.at(1, 3) = -(top + bottom) / (top - bottom);
    r.at(2, 3) = -(far_plane + near_plane) / (far_plane - near_plane);
    r.at(3, 3) = 1.0f;
    return r;
}

Mat4 mat4_look_to(Vec3 eye, Vec3 forward, Vec3 up)
{
    // Right-handed: the camera looks down -Z in eye space.
    const Vec3 f = normalize(forward);
    Vec3 s = cross(f, up);
    if (length_sq(s) < 1e-12f) {
        // Looking straight up or down: `up` is degenerate, pick any side vector.
        // Without this the basis collapses and the view matrix becomes NaN --
        // the gimbal-lock case a portal that maps floor to wall hits routinely.
        s = cross(f, Vec3{0.0f, 0.0f, 1.0f});
        if (length_sq(s) < 1e-12f) s = cross(f, Vec3{1.0f, 0.0f, 0.0f});
    }
    s = normalize(s);
    const Vec3 u = cross(s, f);

    Mat4 r;
    r.at(0, 0) = s.x;  r.at(0, 1) = s.y;  r.at(0, 2) = s.z;  r.at(0, 3) = -dot(s, eye);
    r.at(1, 0) = u.x;  r.at(1, 1) = u.y;  r.at(1, 2) = u.z;  r.at(1, 3) = -dot(u, eye);
    r.at(2, 0) = -f.x; r.at(2, 1) = -f.y; r.at(2, 2) = -f.z; r.at(2, 3) =  dot(f, eye);
    r.at(3, 0) = 0.0f; r.at(3, 1) = 0.0f; r.at(3, 2) = 0.0f; r.at(3, 3) =  1.0f;
    return r;
}

Vec4 mat4_transform_vec4(const Mat4 &a, Vec4 v)
{
    return {a.at(0, 0) * v.x + a.at(0, 1) * v.y + a.at(0, 2) * v.z + a.at(0, 3) * v.w,
            a.at(1, 0) * v.x + a.at(1, 1) * v.y + a.at(1, 2) * v.z + a.at(1, 3) * v.w,
            a.at(2, 0) * v.x + a.at(2, 1) * v.y + a.at(2, 2) * v.z + a.at(2, 3) * v.w,
            a.at(3, 0) * v.x + a.at(3, 1) * v.y + a.at(3, 2) * v.z + a.at(3, 3) * v.w};
}

Vec3 mat4_transform_point(const Mat4 &a, Vec3 v)
{
    const Vec4 r = mat4_transform_vec4(a, Vec4{v.x, v.y, v.z, 1.0f});
    if (std::fabs(r.w) > 1e-20f && std::fabs(r.w - 1.0f) > 1e-6f) {
        const float inv = 1.0f / r.w;
        return {r.x * inv, r.y * inv, r.z * inv};
    }
    return {r.x, r.y, r.z};
}

Vec3 mat4_transform_dir(const Mat4 &a, Vec3 v)
{
    const Vec4 r = mat4_transform_vec4(a, Vec4{v.x, v.y, v.z, 0.0f});
    return {r.x, r.y, r.z};
}

} // namespace cnp
