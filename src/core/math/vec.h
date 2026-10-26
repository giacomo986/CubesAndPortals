#pragma once

#include <cmath>

namespace cnp {

struct Vec2 {
    float x = 0.0f, y = 0.0f;
};

struct Vec3 {
    float x = 0.0f, y = 0.0f, z = 0.0f;
};

struct Vec4 {
    float x = 0.0f, y = 0.0f, z = 0.0f, w = 0.0f;
};

// A plane stored as its implicit equation: dot(n, p) + d == 0, with n == (x,y,z)
// and d == w. Kept as a Vec4 so it can go straight into a shader uniform and be
// dotted with vec4(worldPos, 1.0).
using Plane = Vec4;

inline Vec3 operator+(Vec3 a, Vec3 b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
inline Vec3 operator-(Vec3 a, Vec3 b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
inline Vec3 operator-(Vec3 a) { return {-a.x, -a.y, -a.z}; }
inline Vec3 operator*(Vec3 a, float s) { return {a.x * s, a.y * s, a.z * s}; }
inline Vec3 operator*(float s, Vec3 a) { return a * s; }
inline Vec3 &operator+=(Vec3 &a, Vec3 b) { a = a + b; return a; }
inline Vec3 &operator-=(Vec3 &a, Vec3 b) { a = a - b; return a; }
inline Vec3 &operator*=(Vec3 &a, float s) { a = a * s; return a; }

inline float dot(Vec3 a, Vec3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
inline float dot(Vec4 a, Vec4 b) { return a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w; }

inline Vec3 cross(Vec3 a, Vec3 b)
{
    return {a.y * b.z - a.z * b.y,
            a.z * b.x - a.x * b.z,
            a.x * b.y - a.y * b.x};
}

inline float length_sq(Vec3 a) { return dot(a, a); }
inline float length(Vec3 a) { return std::sqrt(dot(a, a)); }

inline Vec3 normalize(Vec3 a)
{
    const float len2 = dot(a, a);
    if (len2 <= 1e-20f) return {0.0f, 0.0f, 0.0f};
    return a * (1.0f / std::sqrt(len2));
}

// Signed distance from a point to a plane. Positive on the side the normal
// points to.
inline float plane_distance(Plane p, Vec3 v)
{
    return p.x * v.x + p.y * v.y + p.z * v.z + p.w;
}

// Scales a plane so its normal is unit length. Distances measured against a
// non-normalized plane are off by a constant factor, which silently breaks
// every "is this closer than epsilon" guard downstream.
inline Plane plane_normalize(Plane p)
{
    const float len = std::sqrt(p.x * p.x + p.y * p.y + p.z * p.z);
    if (len <= 1e-20f) return p;
    const float inv = 1.0f / len;
    return {p.x * inv, p.y * inv, p.z * inv, p.w * inv};
}

// Builds the plane through `point` with the given (already normalized) normal.
inline Plane plane_from_point_normal(Vec3 point, Vec3 n)
{
    return {n.x, n.y, n.z, -dot(n, point)};
}

} // namespace cnp
