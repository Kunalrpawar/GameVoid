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
    /// Builds the TRS (Translation × Rotation × Scale) model matrix.
    Mat4 GetModelMatrix() const {
        return Mat4::Translate(position) * rotation.ToMat4() * Mat4::Scale(scale);
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
};

} // namespace gv
