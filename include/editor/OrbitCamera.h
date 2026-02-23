// ============================================================================
// GameVoid Engine — Editor Orbit Camera Controller
// ============================================================================
// Blender/Godot-style orbit camera for the 3D viewport:
//   - Middle mouse drag  → orbit around focus point
//   - Shift + MMB drag   → pan
//   - Scroll wheel       → zoom in / out
//   - Right mouse drag   → orbit (alternative)
//   - Alt + LMB drag     → orbit (Maya-style alt)
// ============================================================================
#pragma once

#include "core/Math.h"
#include "core/Types.h"

namespace gv {

class Transform;  // forward

class OrbitCameraController {
public:
    // ── Tuning ─────────────────────────────────────────────────────────────
    f32 orbitSpeed     = 0.25f;   // degrees per pixel
    f32 panSpeed       = 0.01f;   // units per pixel
    f32 zoomSpeed      = 1.0f;    // units per scroll tick (faster zoom)
    f32 minDistance    = 0.5f;    // closest zoom
    f32 maxDistance    = 500.0f;  // farthest zoom
    f32 pitchMin       = -89.0f;
    f32 pitchMax       =  89.0f;

    // ── Fly-through tuning ─────────────────────────────────────────────────
    f32 flySpeed       = 8.0f;    // base fly speed (units/sec)
    f32 flySprintMul   = 2.5f;    // speed multiplier while Shift held
    f32 flyMinSpeed    = 0.5f;    // min speed (scroll-adjustable)
    f32 flyMaxSpeed    = 80.0f;   // max speed (scroll-adjustable)
    f32 flySensitivity = 0.15f;   // degrees per pixel in fly mode

    // ── State (read / write) ───────────────────────────────────────────────
    Vec3 focusPoint  { 0, 0, 0 };   // point the camera orbits around
    f32  distance    = 18.0f;       // distance from focus (farther back for better overview)
    f32  yaw         = 30.0f;       // degrees (left-right)
    f32  pitch       = -25.0f;      // degrees (up-down), negative = looking down

    // ── Interface ──────────────────────────────────────────────────────────

    /// Orbit by pixel deltas (from mouse drag).
    void Orbit(f32 dx, f32 dy);

    /// Pan by pixel deltas (move focus point in camera-local plane).
    void Pan(f32 dx, f32 dy);

    /// Zoom by scroll delta (positive = zoom in).
    void Zoom(f32 delta);

    /// Focus on a specific world position (e.g. selected object).
    void FocusOn(const Vec3& target, f32 dist = -1.0f);

    /// Apply the orbit state to a Transform (position + rotation).
    void ApplyToTransform(Transform& t) const;

    /// Get the computed eye position.
    Vec3 GetEyePosition() const;

    // ── Fly-through mode ───────────────────────────────────────────────────
    /// Rotate view by mouse pixel deltas during fly-through (RMB held).
    void FlyLook(f32 dx, f32 dy);

    /// Move camera in fly-through mode.  forward/right/up are [-1..1].
    void FlyMove(f32 forward, f32 right, f32 up, f32 dt, bool sprint);

    /// Adjust fly speed with scroll wheel.
    void FlyAdjustSpeed(f32 scrollDelta);

    /// After exiting fly mode, recalculate orbit params from eye position.
    void SyncOrbitFromEye(const Vec3& eye);

    /// Get computed forward direction.
    Vec3 GetForwardDir() const;
};

} // namespace gv
