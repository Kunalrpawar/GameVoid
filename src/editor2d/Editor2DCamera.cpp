// ============================================================================
// GameVoid Engine — 2D Editor Camera Implementation
// ============================================================================
#include "editor2d/Editor2DCamera.h"
#include <cmath>

namespace gv {

void Editor2DCamera::Pan(f32 dx, f32 dy) {
    // Pan in world units — scale by inverse zoom so panning stays consistent
    f32 scale = panSensitivity / m_ZoomCur;
    m_PosTgt.x -= dx * scale;
    m_PosTgt.y += dy * scale;   // Y is inverted (screen Y down, world Y up)
}

void Editor2DCamera::Zoom(f32 scrollDelta, f32 mouseWorldX, f32 mouseWorldY) {
    f32 oldZoom = m_ZoomTgt;
    f32 factor = 1.0f + scrollDelta * zoomSpeed;
    if (factor < 0.1f) factor = 0.1f;
    m_ZoomTgt *= factor;
    m_ZoomTgt = Clamp(m_ZoomTgt, minZoom, maxZoom);

    // Zoom toward mouse position (keeps point under cursor stable)
    f32 zoomRatio = 1.0f - oldZoom / m_ZoomTgt;
    m_PosTgt.x += (mouseWorldX - m_PosTgt.x) * zoomRatio;
    m_PosTgt.y += (mouseWorldY - m_PosTgt.y) * zoomRatio;
}

void Editor2DCamera::FocusOn(const Vec2& target) {
    m_PosTgt = target;
}

void Editor2DCamera::Reset() {
    m_PosTgt  = { 0.0f, 0.0f };
    m_ZoomTgt = 1.0f;
}

void Editor2DCamera::Update(f32 dt) {
    f32 t = 1.0f - std::exp(-smoothFactor * dt);
    if (t > 1.0f) t = 1.0f;

    m_PosCur.x = Lerp(m_PosCur.x, m_PosTgt.x, t);
    m_PosCur.y = Lerp(m_PosCur.y, m_PosTgt.y, t);
    m_ZoomCur  = Lerp(m_ZoomCur,  m_ZoomTgt,  t);
    m_ZoomCur  = Clamp(m_ZoomCur, minZoom, maxZoom);
}

Vec2 Editor2DCamera::ScreenToWorld(f32 screenX, f32 screenY, f32 viewportW, f32 viewportH) const {
    // Screen coords (0,0) at top-left of viewport
    f32 ndcX = (screenX / viewportW) * 2.0f - 1.0f;
    f32 ndcY = 1.0f - (screenY / viewportH) * 2.0f;  // flip Y

    f32 aspect = viewportW / viewportH;
    f32 halfH = 1.0f / m_ZoomCur;
    f32 halfW = halfH * aspect;

    f32 worldX = m_PosCur.x + ndcX * halfW;
    f32 worldY = m_PosCur.y + ndcY * halfH;

    return { worldX, worldY };
}

Vec2 Editor2DCamera::WorldToScreen(const Vec2& world, f32 viewportW, f32 viewportH) const {
    f32 aspect = viewportW / viewportH;
    f32 halfH = 1.0f / m_ZoomCur;
    f32 halfW = halfH * aspect;

    f32 ndcX = (world.x - m_PosCur.x) / halfW;
    f32 ndcY = (world.y - m_PosCur.y) / halfH;

    f32 screenX = (ndcX + 1.0f) * 0.5f * viewportW;
    f32 screenY = (1.0f - ndcY) * 0.5f * viewportH;

    return { screenX, screenY };
}

Mat4 Editor2DCamera::GetProjectionMatrix(f32 viewportW, f32 viewportH) const {
    f32 aspect = viewportW / viewportH;
    f32 halfH = 1.0f / m_ZoomCur;
    f32 halfW = halfH * aspect;
    return Mat4::Ortho(-halfW, halfW, -halfH, halfH, -100.0f, 100.0f);
}

Mat4 Editor2DCamera::GetViewMatrix() const {
    return Mat4::Translate(Vec3(-m_PosCur.x, -m_PosCur.y, 0.0f));
}

void Editor2DCamera::SetState(const Vec2& pos, f32 zoom) {
    m_PosCur = m_PosTgt = pos;
    m_ZoomCur = m_ZoomTgt = zoom;
}

} // namespace gv
