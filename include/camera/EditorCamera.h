// ============================================================================
// GameVoid Engine — Unified Editor Camera Controller
// ============================================================================
// A single camera controller that handles ALL editor viewport navigation:
//
//   ORBIT MODE (default):
//     MMB drag          → orbit around focus point
//     Shift+MMB drag    → pan (move focus + camera together)
//     Scroll            → zoom (dolly in/out)
//     Alt+LMB           → orbit (Maya-style)
//     Alt+MMB           → pan (Maya-style)
//     F                 → focus on selected object
//
//   FLY MODE (hold RMB):
//     RMB + mouse move  → free look (FPS-style)
//     WASD              → move forward/back/left/right
//     Q / E             → move down / up
//     Shift             → sprint (2.5x speed)
//     Scroll (while RMB)→ adjust fly speed
//
//   VIEW SNAPS (Numpad):
//     Numpad 1  → front   Ctrl+Numpad 1 → back
//     Numpad 3  → right   Ctrl+Numpad 3 → left
//     Numpad 7  → top     Ctrl+Numpad 7 → bottom
//     Numpad 5  → toggle perspective / orthographic
//     Numpad 0  → reset to default view
//
// Everything is smoothly interpolated for a polished feel.
// ============================================================================
#pragma once

#include "core/Math.h"
#include "core/Types.h"

namespace gv {

class Transform;   // forward
class Camera;      // forward

// ── Camera navigation mode ─────────────────────────────────────────────────
enum class EditorCamMode {
    Orbit,      // default: orbit/pan/zoom around a focus point
    Fly,        // RMB held: FPS-style fly-through
    Idle        // no input happening
};

// ============================================================================
// EditorCamera — self-contained editor viewport camera
// ============================================================================
class EditorCamera {
public:
    EditorCamera() = default;

    // ── Tuning — Orbit ─────────────────────────────────────────────────────
    f32 orbitSensitivity  = 0.35f;   // degrees per pixel of mouse drag (was 0.30)
    f32 panSensitivity    = 0.025f;  // world-units per pixel — much easier panning (was 0.012)
    f32 zoomSensitivity   = 1.5f;    // multiplier per scroll tick (was 1.2)
    f32 minDist           = 0.3f;
    f32 maxDist           = 800.0f;

    // ── Tuning — Fly ───────────────────────────────────────────────────────
    f32 flySpeed          = 18.0f;   // base speed — faster movement (was 10)
    f32 flySprintMul      = 3.5f;    // sprint multiplier (was 3.0)
    f32 flyMinSpeed       = 1.0f;    // minimum fly speed (was 0.5)
    f32 flyMaxSpeed       = 150.0f;  // max fly speed (was 100)
    f32 flySensitivity    = 0.20f;   // degrees per pixel (was 0.18)

    // ── Tuning — Smoothing ─────────────────────────────────────────────────
    f32 smoothFactor      = 16.0f;   // higher = snappier response (was 12)

    // ── Pitch limits ───────────────────────────────────────────────────────
    f32 pitchMin = -89.0f;
    f32 pitchMax =  89.0f;

    // ──────────────────────────────────────────────────────────────────────
    // Public interface — call each frame from the editor viewport
    // ──────────────────────────────────────────────────────────────────────

    /// Process an orbit (mouse pixel deltas).
    void Orbit(f32 dx, f32 dy);

    /// Process a pan (mouse pixel deltas, in screen space).
    void Pan(f32 dx, f32 dy);

    /// Process a zoom (scroll wheel delta, positive = zoom in).
    void Zoom(f32 scrollDelta);

    /// Begin fly mode (call once when RMB is pressed).
    void BeginFly();

    /// Update fly mode each frame.  fwd/right/up in [-1..1].
    void FlyUpdate(f32 dx, f32 dy, f32 fwd, f32 right, f32 up, f32 dt, bool sprint);

    /// End fly mode (call once when RMB is released).
    void EndFly();

    /// Adjust fly speed from scroll wheel (while flying).
    void FlyAdjustSpeed(f32 scrollDelta);

    /// Snap-focus on a world position (e.g. selected object).
    void FocusOn(const Vec3& target, f32 dist = -1.0f);

    /// Snap to an exact yaw/pitch (e.g. numpad shortcuts).
    void SnapView(f32 yaw, f32 pitch);

    /// Main update — call once per frame with deltaTime.
    /// Smoothly interpolates current → target state.
    void Update(f32 dt);

    /// Write resulting position + rotation into a Transform.
    void ApplyToTransform(Transform& t) const;

    // ── Read state ─────────────────────────────────────────────────────────
    Vec3 GetEyePosition()  const;
    Vec3 GetFocusPoint()   const { return m_FocusCur; }
    Vec3 GetForwardDir()   const;
    Vec3 GetRightDir()     const;
    Vec3 GetUpDir()        const;
    f32  GetYaw()          const { return m_YawCur; }
    f32  GetPitch()        const { return m_PitchCur; }
    f32  GetDistance()      const { return m_DistCur; }
    EditorCamMode GetMode() const { return m_Mode; }
    f32  GetFlySpeed()     const { return flySpeed; }

    // ── Direct state manipulation (e.g. loading a saved view) ──────────────
    void SetOrbitState(const Vec3& focus, f32 yaw, f32 pitch, f32 dist);

private:
    // ── Current (smoothed) state ───────────────────────────────────────────
    Vec3 m_FocusCur  { 0, 0, 0 };
    f32  m_YawCur    = 30.0f;
    f32  m_PitchCur  = -25.0f;
    f32  m_DistCur   = 15.0f;

    // ── Target state (set by input, interpolated toward) ───────────────────
    Vec3 m_FocusTgt  { 0, 0, 0 };
    f32  m_YawTgt    = 30.0f;
    f32  m_PitchTgt  = -25.0f;
    f32  m_DistTgt   = 15.0f;

    // ── Fly mode state ─────────────────────────────────────────────────────
    Vec3 m_FlyPos    { 0, 0, 0 };     // camera position in fly mode
    f32  m_FlyYaw    = 0.0f;
    f32  m_FlyPitch  = 0.0f;

    EditorCamMode m_Mode = EditorCamMode::Idle;

    // ── Helpers ────────────────────────────────────────────────────────────
    static f32  Clamp(f32 v, f32 lo, f32 hi);
    static f32  WrapAngle(f32 deg);
    static f32  LerpAngle(f32 a, f32 b, f32 t);
    static f32  Lerp(f32 a, f32 b, f32 t);
    static Vec3 LerpVec3(const Vec3& a, const Vec3& b, f32 t);
    Vec3  ComputeEyeFromOrbit(const Vec3& focus, f32 yaw, f32 pitch, f32 dist) const;
};

} // namespace gv
