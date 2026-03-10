// ============================================================================
// GameVoid Engine — 2D Editor Camera (Pan/Zoom)
// ============================================================================
// Orthographic 2D camera with smooth pan (MMB / Shift+MMB) and zoom (scroll).
// Similar to Godot's 2D viewport camera.
//
//   MMB / Middle drag   → pan
//   Scroll              → zoom in/out
//   Home                → reset to origin
//   F                   → focus on selected object
// ============================================================================
#pragma once

#include "core/Math.h"
#include "core/Types.h"

namespace gv {

class Editor2DCamera {
public:
    Editor2DCamera() = default;

    // ── Tuning ─────────────────────────────────────────────────────────────
    f32 panSensitivity  = 2.5f;    // pixels → world units (higher = faster pan)
    f32 zoomSpeed       = 0.25f;   // zoom factor per scroll tick
    f32 minZoom         = 0.02f;   // minimum zoom (max zoom out)
    f32 maxZoom         = 80.0f;   // maximum zoom (max zoom in)
    f32 smoothFactor    = 20.0f;   // interpolation smoothness (higher = snappier)

    // ── Interface ──────────────────────────────────────────────────────────

    /// Pan by pixel deltas (from mouse drag).
    void Pan(f32 dx, f32 dy);

    /// Zoom by scroll delta (positive = zoom in).
    void Zoom(f32 scrollDelta, f32 mouseWorldX = 0.0f, f32 mouseWorldY = 0.0f);

    /// Focus on a world position.
    void FocusOn(const Vec2& target);

    /// Reset to origin with default zoom.
    void Reset();

    /// Smooth update — call every frame.
    void Update(f32 dt);

    // ── Read state ─────────────────────────────────────────────────────────
    Vec2 GetPosition()  const { return m_PosCur; }
    f32  GetZoom()      const { return m_ZoomCur; }

    /// Convert screen pixel coords (relative to viewport) to world coords.
    Vec2 ScreenToWorld(f32 screenX, f32 screenY, f32 viewportW, f32 viewportH) const;

    /// Convert world coords to screen pixel coords.
    Vec2 WorldToScreen(const Vec2& world, f32 viewportW, f32 viewportH) const;

    /// Build an orthographic projection matrix for the 2D view.
    Mat4 GetProjectionMatrix(f32 viewportW, f32 viewportH) const;

    /// Build the view matrix (translation only, no rotation).
    Mat4 GetViewMatrix() const;

    /// Direct state set (for loading saved views).
    void SetState(const Vec2& pos, f32 zoom);

private:
    // Current (smoothed)
    Vec2 m_PosCur  { 0, 0 };
    f32  m_ZoomCur = 0.15f;   // start zoomed out so objects are visible

    // Target
    Vec2 m_PosTgt  { 0, 0 };
    f32  m_ZoomTgt = 0.15f;   // start zoomed out

    static f32 Lerp(f32 a, f32 b, f32 t) { return a + (b - a) * t; }
    static f32 Clamp(f32 v, f32 lo, f32 hi) { return v < lo ? lo : (v > hi ? hi : v); }
};

} // namespace gv
