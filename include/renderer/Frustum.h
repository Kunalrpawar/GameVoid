// ============================================================================
// GameVoid Engine — Frustum Culling
// ============================================================================
// Extracts 6 frustum planes from a view-projection matrix and tests
// axis-aligned bounding boxes (AABBs) and spheres against them.
// ============================================================================
#pragma once

#include "core/Math.h"
#include "core/Types.h"

namespace gv {

/// A plane in Ax + By + Cz + D = 0 form.
struct Plane {
    f32 a = 0, b = 0, c = 0, d = 0;

    /// Distance from a point to this plane (positive = inside).
    f32 DistanceTo(const Vec3& p) const {
        return a * p.x + b * p.y + c * p.z + d;
    }

    /// Normalize the plane equation.
    void Normalize() {
        f32 len = std::sqrt(a * a + b * b + c * c);
        if (len > 1e-8f) { a /= len; b /= len; c /= len; d /= len; }
    }
};

/// View frustum — 6 planes extracted from the viewProjection matrix.
class Frustum {
public:
    Frustum() = default;

    /// Extract frustum planes from a combined view * projection matrix.
    /// Uses the Gribb-Hartmann method (column-major Mat4).
    void ExtractFromVP(const Mat4& vp) {
        // Left
        m_Planes[0].a = vp.m[3]  + vp.m[0];
        m_Planes[0].b = vp.m[7]  + vp.m[4];
        m_Planes[0].c = vp.m[11] + vp.m[8];
        m_Planes[0].d = vp.m[15] + vp.m[12];
        // Right
        m_Planes[1].a = vp.m[3]  - vp.m[0];
        m_Planes[1].b = vp.m[7]  - vp.m[4];
        m_Planes[1].c = vp.m[11] - vp.m[8];
        m_Planes[1].d = vp.m[15] - vp.m[12];
        // Bottom
        m_Planes[2].a = vp.m[3]  + vp.m[1];
        m_Planes[2].b = vp.m[7]  + vp.m[5];
        m_Planes[2].c = vp.m[11] + vp.m[9];
        m_Planes[2].d = vp.m[15] + vp.m[13];
        // Top
        m_Planes[3].a = vp.m[3]  - vp.m[1];
        m_Planes[3].b = vp.m[7]  - vp.m[5];
        m_Planes[3].c = vp.m[11] - vp.m[9];
        m_Planes[3].d = vp.m[15] - vp.m[13];
        // Near
        m_Planes[4].a = vp.m[3]  + vp.m[2];
        m_Planes[4].b = vp.m[7]  + vp.m[6];
        m_Planes[4].c = vp.m[11] + vp.m[10];
        m_Planes[4].d = vp.m[15] + vp.m[14];
        // Far
        m_Planes[5].a = vp.m[3]  - vp.m[2];
        m_Planes[5].b = vp.m[7]  - vp.m[6];
        m_Planes[5].c = vp.m[11] - vp.m[10];
        m_Planes[5].d = vp.m[15] - vp.m[14];

        for (auto& p : m_Planes) p.Normalize();
    }

    /// Test if a sphere is inside (or intersects) the frustum.
    bool TestSphere(const Vec3& center, f32 radius) const {
        for (const auto& p : m_Planes) {
            if (p.DistanceTo(center) < -radius) return false;
        }
        return true;
    }

    /// Test if an AABB (min/max corners) is inside (or intersects) the frustum.
    bool TestAABB(const Vec3& minCorner, const Vec3& maxCorner) const {
        for (const auto& p : m_Planes) {
            // Find the positive vertex (corner most aligned with plane normal)
            Vec3 pv;
            pv.x = (p.a >= 0) ? maxCorner.x : minCorner.x;
            pv.y = (p.b >= 0) ? maxCorner.y : minCorner.y;
            pv.z = (p.c >= 0) ? maxCorner.z : minCorner.z;
            if (p.DistanceTo(pv) < 0) return false;
        }
        return true;
    }

    /// Convenience: test a bounding sphere from position + uniform scale.
    /// Assumes the object has a unit bounding sphere scaled by `scale`.
    bool TestObject(const Vec3& position, f32 scale) const {
        return TestSphere(position, scale * 1.733f); // sqrt(3) ≈ 1.733 for unit cube diagonal
    }

private:
    Plane m_Planes[6];
};

} // namespace gv
