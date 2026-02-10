// ============================================================================
// GameVoid Engine — FPS Camera Controller
// ============================================================================
// A Component that gives the owning GameObject first-person-shooter style
// controls:  WASD to move, mouse to look around.
//
// Usage:
//   auto* camObj = scene->CreateGameObject("MainCamera");
//   camObj->AddComponent<Camera>();
//   camObj->AddComponent<FPSCameraController>();
//
// The Engine game loop calls UpdateFPSCamera(window, dt) every frame.
// ============================================================================
#pragma once

#include "core/Component.h"
#include "core/Math.h"
#include "core/Types.h"

namespace gv {

// Forward-declare — we don't want to include Window.h in every header
class Window;

/// Attach this to the same GameObject as a Camera to get FPS controls.
class FPSCameraController : public Component {
public:
    // ── Tuning knobs (tweak to taste) ──────────────────────────────────────
    f32 moveSpeed   = 5.0f;     // units per second
    f32 sprintMulti = 2.5f;     // speed multiplier while holding Shift
    f32 sensitivity = 0.12f;    // degrees per pixel of mouse movement

    // ── Pitch limits (clamp to prevent flipping) ───────────────────────────
    f32 pitchMin = -89.0f;      // degrees
    f32 pitchMax =  89.0f;

    FPSCameraController() = default;
    ~FPSCameraController() override = default;

    std::string GetTypeName() const override { return "FPSCameraController"; }

    /// Call once per frame, BEFORE computing the view matrix.
    /// Reads keyboard/mouse from the Window and updates the owning
    /// GameObject's Transform (position + rotation).
    void UpdateFromInput(Window& window, f32 dt);

    // ── Read-only state ────────────────────────────────────────────────────
    f32 GetYaw()   const { return m_Yaw; }
    f32 GetPitch() const { return m_Pitch; }

private:
    // Current Euler angles in degrees
    f32 m_Yaw   =  0.0f;   // around Y axis (left-right)
    f32 m_Pitch =  0.0f;   // around X axis (up-down)
};

} // namespace gv
