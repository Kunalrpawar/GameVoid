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
    f32 zoomSpeed      = 0.5f;    // units per scroll tick
    f32 minDistance    = 0.5f;    // closest zoom
    f32 maxDistance    = 200.0f;  // farthest zoom
    f32 pitchMin       = -89.0f;
    f32 pitchMax       =  89.0f;

    // ── State (read / write) ───────────────────────────────────────────────
    Vec3 focusPoint  { 0, 0, 0 };   // point the camera orbits around
    f32  distance    = 8.0f;        // distance from focus
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
};

} // namespace gv
