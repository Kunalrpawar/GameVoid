// ============================================================================
// GameVoid Engine — Viewport Input Manager
// ============================================================================
// Translates raw mouse / keyboard state from ImGui into high-level editor
// viewport actions (orbit, pan, zoom, fly, pick, gizmo drag, etc.).
//
// This decouples input handling from rendering so the camera controller
// and the EditorUI don't have tangled logic.
// ============================================================================
#pragma once

#include "core/Types.h"
#include "core/Math.h"

namespace gv {

// ── What the viewport should do this frame ─────────────────────────────────
enum class ViewportAction {
    None,
    Orbit,          // user is orbiting (MMB drag or Alt+LMB)
    Pan,            // user is panning (Shift+MMB or Alt+MMB)
    Zoom,           // scroll wheel zoom
    FlyBegin,       // RMB just pressed — enter fly mode
    FlyUpdate,      // RMB held — continuing fly mode
    FlyEnd,         // RMB just released — exit fly mode
    FlySpeedScroll, // scroll while flying — adjust speed
    Pick,           // LMB click — object pick / gizmo click
    GizmoDrag,      // LMB drag on gizmo axis
    FocusSelected,  // F key pressed
    // View snap shortcuts
    SnapFront,
    SnapRight,
    SnapTop,
    SnapBack,
    SnapLeft,
    SnapBottom,
    ToggleOrtho,
    ResetView,
};

// ── Per-frame input state collected from ImGui ─────────────────────────────
struct ViewportInputState {
    // Mouse
    Vec2 mousePos      {};
    Vec2 mouseDelta    {};
    f32  scrollDelta   = 0.0f;

    // Mouse buttons (current frame)
    bool lmbDown       = false;
    bool lmbClicked    = false;
    bool lmbReleased   = false;
    bool mmbDown       = false;
    bool mmbClicked    = false;
    bool rmbDown       = false;
    bool rmbClicked    = false;
    bool rmbReleased   = false;

    // Modifier keys
    bool shiftHeld     = false;
    bool ctrlHeld      = false;
    bool altHeld       = false;

    // Keyboard (for fly movement + shortcuts)
    bool keyW = false, keyA = false, keyS = false, keyD = false;
    bool keyQ = false, keyE = false;
    bool keyF = false;
    bool keyDelete = false;

    // Numpad
    bool numpad0 = false, numpad1 = false, numpad3 = false;
    bool numpad5 = false, numpad7 = false;

    // Whether the mouse is hovering over the viewport
    bool viewportHovered = false;
};

// ============================================================================
// ViewportInputManager — stateful input processor
// ============================================================================
class ViewportInputManager {
public:
    ViewportInputManager() = default;

    /// Feed raw input state each frame.  Call before anything else.
    void Update(const ViewportInputState& input);

    // ── Query current action ───────────────────────────────────────────────
    ViewportAction   GetAction()      const { return m_Action; }
    Vec2             GetMouseDelta()  const { return m_Delta; }
    f32              GetScrollDelta() const { return m_Scroll; }

    /// Fly movement axes: fwd, right, up  in [-1..1]
    f32 GetFlyForward() const { return m_FlyFwd; }
    f32 GetFlyRight()   const { return m_FlyRight; }
    f32 GetFlyUp()      const { return m_FlyUp; }
    bool IsSprinting()  const { return m_Sprint; }

    bool IsFlying()     const { return m_Flying; }
    bool IsOrbiting()   const { return m_Orbiting; }
    bool IsPanning()    const { return m_Panning; }

private:
    // Internal tracking state
    ViewportAction m_Action = ViewportAction::None;
    Vec2 m_Delta {};
    f32  m_Scroll = 0.0f;

    f32  m_FlyFwd   = 0, m_FlyRight = 0, m_FlyUp = 0;
    bool m_Sprint   = false;

    bool m_Flying   = false;
    bool m_Orbiting = false;
    bool m_Panning  = false;
    Vec2 m_LastMouse {};
};

} // namespace gv
