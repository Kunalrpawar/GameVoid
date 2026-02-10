// ============================================================================
// GameVoid Engine — Camera System
// ============================================================================
// Supports both perspective and orthographic projection.  Attach a Camera to
// a GameObject and set it as the active camera in the Scene.
// ============================================================================
#pragma once

#include "core/Component.h"
#include "core/Math.h"

namespace gv {

enum class ProjectionType { Perspective, Orthographic };

class Camera : public Component {
public:
    Camera() = default;
    ~Camera() override = default;

    std::string GetTypeName() const override { return "Camera"; }

    // ── Projection parameters ──────────────────────────────────────────────
    ProjectionType projectionType = ProjectionType::Perspective;

    // Perspective
    f32 fov         = 60.0f;   // degrees
    f32 aspectRatio = 16.0f / 9.0f;
    f32 nearPlane   = 0.1f;
    f32 farPlane    = 1000.0f;

    // Orthographic
    f32 orthoSize   = 10.0f;   // half-height of the ortho frustum

    // ── Matrices ───────────────────────────────────────────────────────────
    /// Build and return the projection matrix based on current settings.
    Mat4 GetProjectionMatrix() const {
        if (projectionType == ProjectionType::Perspective) {
            f32 fovRad = fov * (3.14159265358979f / 180.0f);
            return Mat4::Perspective(fovRad, aspectRatio, nearPlane, farPlane);
        } else {
            f32 hw = orthoSize * aspectRatio;
            f32 hh = orthoSize;
            return Mat4::Ortho(-hw, hw, -hh, hh, nearPlane, farPlane);
        }
    }

    /// Build the view matrix from the owning GameObject's transform.
    /// Requires the Camera to be attached to a GameObject.
    Mat4 GetViewMatrix() const;

    // ── Convenience ────────────────────────────────────────────────────────
    void SetPerspective(f32 fovDeg, f32 aspect, f32 nearP, f32 farP) {
        projectionType = ProjectionType::Perspective;
        fov = fovDeg; aspectRatio = aspect; nearPlane = nearP; farPlane = farP;
    }

    void SetOrthographic(f32 size, f32 aspect, f32 nearP, f32 farP) {
        projectionType = ProjectionType::Orthographic;
        orthoSize = size; aspectRatio = aspect; nearPlane = nearP; farPlane = farP;
    }
};

} // namespace gv
