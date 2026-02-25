// ============================================================================
// GameVoid Engine — Transform Component
// ============================================================================
// Encapsulates an object's position, rotation (quaternion), and scale.
// Provides methods to compute the local-to-world model matrix and to
// manipulate the transform in a user-friendly way (Euler helpers, etc.).
// ============================================================================
#pragma once

#include "core/Math.h"

namespace gv {

class Transform {
public:
    Vec3       position{ 0, 0, 0 };
    Quaternion rotation{};                  // identity by default
    Vec3       scale{ 1, 1, 1 };

    Transform() = default;
    Transform(const Vec3& pos, const Quaternion& rot, const Vec3& scl)
        : position(pos), rotation(rot), scale(scl) {}

    // ── Convenience setters ────────────────────────────────────────────────
    void SetPosition(f32 x, f32 y, f32 z) { position = { x, y, z }; }
    void SetScale(f32 x, f32 y, f32 z)    { scale    = { x, y, z }; }
    void SetScale(f32 uniform)             { scale    = { uniform, uniform, uniform }; }

    /// Set rotation from Euler angles (degrees for convenience).
    void SetEulerDeg(f32 pitch, f32 yaw, f32 roll) {
        constexpr f32 deg2rad = 3.14159265358979f / 180.0f;
        rotation = Quaternion::FromEuler({ pitch * deg2rad, yaw * deg2rad, roll * deg2rad });
    }

    // ── Matrix generation ──────────────────────────────────────────────────
    /// Builds the local TRS (Translation × Rotation × Scale) model matrix.
    Mat4 GetLocalMatrix() const {
        return Mat4::Translate(position) * rotation.ToMat4() * Mat4::Scale(scale);
    }

    /// Builds the TRS model matrix. If a parent transform is set,
    /// returns parentWorldMatrix * localMatrix (recursive hierarchy).
    Mat4 GetModelMatrix() const {
        Mat4 local = GetLocalMatrix();
        if (m_Parent) return m_Parent->GetModelMatrix() * local;
        return local;
    }

    // ── Hierarchy ──────────────────────────────────────────────────────────
    void SetParentTransform(Transform* parent) { m_Parent = parent; }
    Transform* GetParentTransform() const      { return m_Parent; }

    /// Get position in world space (extracts translation from the world matrix).
    Vec3 GetWorldPosition() const {
        Mat4 world = GetModelMatrix();
        return Vec3(world.m[12], world.m[13], world.m[14]);
    }

    /// Set position in world space (converts to local space if parented).
    void SetWorldPosition(const Vec3& worldPos) {
        if (!m_Parent) {
            position = worldPos;
        } else {
            // Properly invert parent world transform to get local position
            Mat4 parentWorld = m_Parent->GetModelMatrix();
            Mat4 invParent = parentWorld.Inverse();
            Vec3 local = invParent.TransformPoint(worldPos);
            position = local;
        }
    }

    // ── Movement helpers ───────────────────────────────────────────────────
    void Translate(const Vec3& delta) { position = position + delta; }

    // ── Debug ──────────────────────────────────────────────────────────────
    std::string ToString() const {
        std::ostringstream ss;
        ss << "Pos(" << position.x << ", " << position.y << ", " << position.z << ") "
           << "Scl(" << scale.x    << ", " << scale.y    << ", " << scale.z    << ")";
        return ss.str();
    }

private:
    Transform* m_Parent = nullptr;   // parent transform for hierarchy
};

} // namespace gv
