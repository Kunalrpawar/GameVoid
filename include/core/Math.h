// ============================================================================
// GameVoid Engine — Lightweight Math Library
// ============================================================================
// Minimal math types (Vec2, Vec3, Vec4, Mat4, Quaternion) used throughout the
// engine.  In production you would replace these with GLM or a SIMD-optimised
// library; this skeleton keeps things dependency-free for clarity.
// ============================================================================
#pragma once

#include "core/Types.h"
#include <cmath>

namespace gv {

// ─── Vec2 ──────────────────────────────────────────────────────────────────
struct Vec2 {
    f32 x = 0.0f, y = 0.0f;

    Vec2() = default;
    Vec2(f32 x, f32 y) : x(x), y(y) {}

    Vec2 operator+(const Vec2& o) const { return { x + o.x, y + o.y }; }
    Vec2 operator-(const Vec2& o) const { return { x - o.x, y - o.y }; }
    Vec2 operator*(f32 s) const         { return { x * s, y * s }; }
    f32  Length() const                  { return std::sqrt(x * x + y * y); }
    Vec2 Normalized() const {
        f32 l = Length();
        return (l > 0.0f) ? Vec2{ x / l, y / l } : Vec2{};
    }
};

// ─── Vec3 ──────────────────────────────────────────────────────────────────
struct Vec3 {
    f32 x = 0.0f, y = 0.0f, z = 0.0f;

    Vec3() = default;
    Vec3(f32 x, f32 y, f32 z) : x(x), y(y), z(z) {}

    Vec3 operator+(const Vec3& o) const { return { x + o.x, y + o.y, z + o.z }; }
    Vec3 operator-(const Vec3& o) const { return { x - o.x, y - o.y, z - o.z }; }
    Vec3 operator*(f32 s) const         { return { x * s, y * s, z * s }; }
    Vec3 operator-() const              { return { -x, -y, -z }; }

    f32  Dot(const Vec3& o) const   { return x * o.x + y * o.y + z * o.z; }
    Vec3 Cross(const Vec3& o) const {
        return { y * o.z - z * o.y, z * o.x - x * o.z, x * o.y - y * o.x };
    }
    f32  Length() const       { return std::sqrt(Dot(*this)); }
    Vec3 Normalized() const {
        f32 l = Length();
        return (l > 0.0f) ? Vec3{ x / l, y / l, z / l } : Vec3{};
    }

    static Vec3 Zero()    { return { 0, 0, 0 }; }
    static Vec3 One()     { return { 1, 1, 1 }; }
    static Vec3 Up()      { return { 0, 1, 0 }; }
    static Vec3 Forward() { return { 0, 0,-1 }; }
    static Vec3 Right()   { return { 1, 0, 0 }; }
};

// ─── Vec4 ──────────────────────────────────────────────────────────────────
struct Vec4 {
    f32 x = 0, y = 0, z = 0, w = 0;

    Vec4() = default;
    Vec4(f32 x, f32 y, f32 z, f32 w) : x(x), y(y), z(z), w(w) {}
    Vec4(const Vec3& v, f32 w) : x(v.x), y(v.y), z(v.z), w(w) {}
};

// ─── Mat4 (column-major, OpenGL convention) ────────────────────────────────
struct Mat4 {
    f32 m[16] = {};                     // column-major order

    Mat4() = default;

    /// Returns the 4×4 identity matrix.
    static Mat4 Identity() {
        Mat4 r;
        r.m[0] = r.m[5] = r.m[10] = r.m[15] = 1.0f;
        return r;
    }

    /// Builds a translation matrix.
    static Mat4 Translate(const Vec3& t) {
        Mat4 r = Identity();
        r.m[12] = t.x; r.m[13] = t.y; r.m[14] = t.z;
        return r;
    }

    /// Builds a uniform scale matrix.
    static Mat4 Scale(const Vec3& s) {
        Mat4 r = Identity();
        r.m[0] = s.x; r.m[5] = s.y; r.m[10] = s.z;
        return r;
    }

    /// Builds a rotation matrix around an arbitrary axis (angle in radians).
    static Mat4 Rotate(f32 angle, const Vec3& axis) {
        Mat4 r = Identity();
        Vec3 a = axis.Normalized();
        f32 c = std::cos(angle), s = std::sin(angle), t = 1.0f - c;
        r.m[0]  = t * a.x * a.x + c;
        r.m[1]  = t * a.x * a.y + s * a.z;
        r.m[2]  = t * a.x * a.z - s * a.y;
        r.m[4]  = t * a.x * a.y - s * a.z;
        r.m[5]  = t * a.y * a.y + c;
        r.m[6]  = t * a.y * a.z + s * a.x;
        r.m[8]  = t * a.x * a.z + s * a.y;
        r.m[9]  = t * a.y * a.z - s * a.x;
        r.m[10] = t * a.z * a.z + c;
        return r;
    }

    /// Perspective projection (field of view in radians).
    static Mat4 Perspective(f32 fov, f32 aspect, f32 near, f32 far) {
        Mat4 r;
        f32 tanHalf = std::tan(fov / 2.0f);
        r.m[0]  = 1.0f / (aspect * tanHalf);
        r.m[5]  = 1.0f / tanHalf;
        r.m[10] = -(far + near) / (far - near);
        r.m[11] = -1.0f;
        r.m[14] = -(2.0f * far * near) / (far - near);
        return r;
    }

    /// Orthographic projection.
    static Mat4 Ortho(f32 left, f32 right, f32 bottom, f32 top, f32 near, f32 far) {
        Mat4 r = Identity();
        r.m[0]  =  2.0f / (right - left);
        r.m[5]  =  2.0f / (top - bottom);
        r.m[10] = -2.0f / (far - near);
        r.m[12] = -(right + left) / (right - left);
        r.m[13] = -(top + bottom) / (top - bottom);
        r.m[14] = -(far + near)   / (far - near);
        return r;
    }

    /// Look-at view matrix.
    static Mat4 LookAt(const Vec3& eye, const Vec3& target, const Vec3& up) {
        Vec3 f = (target - eye).Normalized();
        Vec3 r = f.Cross(up).Normalized();
        Vec3 u = r.Cross(f);
        Mat4 m = Identity();
        m.m[0] = r.x;  m.m[4] = r.y;  m.m[8]  = r.z;
        m.m[1] = u.x;  m.m[5] = u.y;  m.m[9]  = u.z;
        m.m[2] = -f.x; m.m[6] = -f.y; m.m[10] = -f.z;
        m.m[12] = -r.Dot(eye);
        m.m[13] = -u.Dot(eye);
        m.m[14] =  f.Dot(eye);
        return m;
    }

    /// Naive matrix multiply (column-major).
    Mat4 operator*(const Mat4& b) const {
        Mat4 r;
        for (int col = 0; col < 4; ++col)
            for (int row = 0; row < 4; ++row) {
                f32 sum = 0;
                for (int k = 0; k < 4; ++k)
                    sum += m[k * 4 + row] * b.m[col * 4 + k];
                r.m[col * 4 + row] = sum;
            }
        return r;
    }
};

// ─── Quaternion (unit quaternion for rotations) ────────────────────────────
struct Quaternion {
    f32 x = 0, y = 0, z = 0, w = 1;

    Quaternion() = default;
    Quaternion(f32 x, f32 y, f32 z, f32 w) : x(x), y(y), z(z), w(w) {}

    /// Create from axis-angle (angle in radians).
    static Quaternion FromAxisAngle(const Vec3& axis, f32 angle) {
        f32 half = angle * 0.5f;
        f32 s = std::sin(half);
        Vec3 a = axis.Normalized();
        return { a.x * s, a.y * s, a.z * s, std::cos(half) };
    }

    /// Create from Euler angles (radians, YXZ order).
    static Quaternion FromEuler(const Vec3& euler) {
        Quaternion qx = FromAxisAngle(Vec3::Right(),   euler.x);
        Quaternion qy = FromAxisAngle(Vec3::Up(),      euler.y);
        Quaternion qz = FromAxisAngle(Vec3(0, 0, 1),   euler.z);
        return qy * qx * qz;
    }

    Quaternion operator*(const Quaternion& q) const {
        return {
            w * q.x + x * q.w + y * q.z - z * q.y,
            w * q.y - x * q.z + y * q.w + z * q.x,
            w * q.z + x * q.y - y * q.x + z * q.w,
            w * q.w - x * q.x - y * q.y - z * q.z
        };
    }

    /// Convert to a 4×4 rotation matrix.
    Mat4 ToMat4() const {
        Mat4 r = Mat4::Identity();
        f32 xx = x * x, yy = y * y, zz = z * z;
        f32 xy = x * y, xz = x * z, yz = y * z;
        f32 wx = w * x, wy = w * y, wz = w * z;
        r.m[0]  = 1 - 2 * (yy + zz);
        r.m[1]  =     2 * (xy + wz);
        r.m[2]  =     2 * (xz - wy);
        r.m[4]  =     2 * (xy - wz);
        r.m[5]  = 1 - 2 * (xx + zz);
        r.m[6]  =     2 * (yz + wx);
        r.m[8]  =     2 * (xz + wy);
        r.m[9]  =     2 * (yz - wx);
        r.m[10] = 1 - 2 * (xx + yy);
        return r;
    }

    Quaternion Normalized() const {
        f32 l = std::sqrt(x*x + y*y + z*z + w*w);
        return (l > 0) ? Quaternion{ x/l, y/l, z/l, w/l } : Quaternion{};
    }
};

} // namespace gv
