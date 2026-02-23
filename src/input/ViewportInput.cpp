// ============================================================================
// GameVoid Engine — Viewport Input Manager Implementation
// ============================================================================
#include "input/ViewportInput.h"
#include <cmath>

namespace gv {

void ViewportInputManager::Update(const ViewportInputState& in) {
    m_Action = ViewportAction::None;
    m_Delta  = Vec2(0, 0);
    m_Scroll = 0.0f;
    m_FlyFwd = m_FlyRight = m_FlyUp = 0.0f;
    m_Sprint = in.shiftHeld;

    // ── Fly mode lifecycle ─────────────────────────────────────────────────
    if (m_Flying) {
        if (in.rmbDown) {
            // Still flying
            m_Delta  = in.mouseDelta;
            m_Scroll = in.scrollDelta;

            // WASD + QE
            if (in.keyW) m_FlyFwd   += 1.0f;
            if (in.keyS) m_FlyFwd   -= 1.0f;
            if (in.keyD) m_FlyRight += 1.0f;
            if (in.keyA) m_FlyRight -= 1.0f;
            if (in.keyE) m_FlyUp    += 1.0f;
            if (in.keyQ) m_FlyUp    -= 1.0f;

            if (in.scrollDelta != 0.0f) {
                m_Action = ViewportAction::FlySpeedScroll;
            } else {
                m_Action = ViewportAction::FlyUpdate;
            }
            return;
        } else {
            // RMB released → end fly
            m_Flying = false;
            m_Action = ViewportAction::FlyEnd;
            return;
        }
    }

    // ── Not currently flying — check for new fly mode ──────────────────────
    if (in.viewportHovered && in.rmbClicked && !in.altHeld && !m_Orbiting && !m_Panning) {
        m_Flying = true;
        m_LastMouse = in.mousePos;
        m_Action = ViewportAction::FlyBegin;
        return;
    }

    // ── Orbit / Pan lifecycle ──────────────────────────────────────────────
    if (m_Orbiting) {
        if (in.mmbDown || (in.altHeld && in.lmbDown)) {
            m_Delta = in.mouseDelta;
            if (in.shiftHeld) {
                m_Action = ViewportAction::Pan;   // shift mid-orbit → pan
            } else {
                m_Action = ViewportAction::Orbit;
            }
            return;
        } else {
            m_Orbiting = false;
        }
    }

    if (m_Panning) {
        if (in.mmbDown || (in.altHeld && in.mmbDown)) {
            m_Delta = in.mouseDelta;
            m_Action = ViewportAction::Pan;
            return;
        } else {
            m_Panning = false;
        }
    }

    // ── Start orbit / pan ──────────────────────────────────────────────────
    if (in.viewportHovered && !m_Flying) {
        if (in.mmbClicked || (in.altHeld && in.lmbClicked)) {
            m_LastMouse = in.mousePos;
            if (in.shiftHeld || (in.altHeld && in.mmbClicked)) {
                m_Panning = true;
                m_Action  = ViewportAction::Pan;
            } else {
                m_Orbiting = true;
                m_Action   = ViewportAction::Orbit;
            }
            m_Delta = Vec2(0, 0);
            return;
        }
    }

    // ── Scroll zoom ────────────────────────────────────────────────────────
    if (in.viewportHovered && in.scrollDelta != 0.0f && !m_Flying) {
        m_Scroll = in.scrollDelta;
        m_Action = ViewportAction::Zoom;
        return;
    }

    // ── Pick (LMB click in viewport, not during fly/orbit/pan) ─────────────
    if (in.viewportHovered && in.lmbClicked && !in.altHeld && !m_Flying && !m_Orbiting && !m_Panning) {
        m_Action = ViewportAction::Pick;
        return;
    }

    // ── Focus selected (F key) ─────────────────────────────────────────────
    if (in.viewportHovered && in.keyF && !m_Flying) {
        m_Action = ViewportAction::FocusSelected;
        return;
    }

    // ── Numpad view shortcuts ──────────────────────────────────────────────
    if (in.viewportHovered && !m_Flying) {
        if (in.numpad1) { m_Action = in.ctrlHeld ? ViewportAction::SnapBack   : ViewportAction::SnapFront; return; }
        if (in.numpad3) { m_Action = in.ctrlHeld ? ViewportAction::SnapLeft   : ViewportAction::SnapRight; return; }
        if (in.numpad7) { m_Action = in.ctrlHeld ? ViewportAction::SnapBottom : ViewportAction::SnapTop;   return; }
        if (in.numpad5) { m_Action = ViewportAction::ToggleOrtho; return; }
        if (in.numpad0) { m_Action = ViewportAction::ResetView;   return; }
    }
}

} // namespace gv
