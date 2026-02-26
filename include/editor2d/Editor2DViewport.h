// ============================================================================
// GameVoid Engine — 2D Viewport Panel
// ============================================================================
// Handles 2D editor viewport rendering:
//   - Orthographic camera with pan/zoom
//   - 2D grid
//   - Sprite rendering (sorted by layer)
//   - 2D gizmos (translate/rotate/scale handles)
//   - Object picking & drag
//   - Placement preview for new sprites
//   - Tilemap editing tools
//
// Called by EditorUI when the editor is in 2D mode.
// ============================================================================
#pragma once

#ifdef GV_HAS_GLFW

#include "core/Types.h"
#include "core/Math.h"
#include "editor2d/Editor2DTypes.h"
#include "editor2d/Editor2DCamera.h"
#include "editor2d/Scene2D.h"
#include <string>
#include <deque>

namespace gv {

// Forward declarations
class OpenGLRenderer;
class Window;
class GameObject;

class Editor2DViewport {
public:
    Editor2DViewport() = default;
    ~Editor2DViewport() = default;

    // ── Lifecycle ──────────────────────────────────────────────────────────
    bool Init(Window* window, OpenGLRenderer* renderer);
    void Shutdown();

    // ── Frame ──────────────────────────────────────────────────────────────

    /// Render the full 2D viewport panel (call from EditorUI::Render).
    /// Returns the selected object (or nullptr).
    void RenderViewport(f32 dt, f32 vpX, f32 vpY, f32 vpW, f32 vpH);

    // ── Scene ──────────────────────────────────────────────────────────────
    Scene2D& GetScene()             { return m_Scene; }
    const Scene2D& GetScene() const { return m_Scene; }

    // ── Selection ──────────────────────────────────────────────────────────
    GameObject* GetSelected() const         { return m_Selected; }
    void SetSelected(GameObject* obj)       { m_Selected = obj; }

    // ── Camera ─────────────────────────────────────────────────────────────
    Editor2DCamera& GetCamera()             { return m_Camera; }

    // ── Gizmo ──────────────────────────────────────────────────────────────
    Gizmo2DMode GetGizmoMode() const        { return m_GizmoMode; }
    void SetGizmoMode(Gizmo2DMode mode)     { m_GizmoMode = mode; }

    // ── Grid ───────────────────────────────────────────────────────────────
    bool showGrid     = true;
    f32  gridSize     = 1.0f;
    Vec4 gridColor    { 0.3f, 0.3f, 0.35f, 0.4f };
    Vec4 gridAxisX    { 0.8f, 0.2f, 0.2f, 0.6f };
    Vec4 gridAxisY    { 0.2f, 0.8f, 0.2f, 0.6f };

    // ── Snap ───────────────────────────────────────────────────────────────
    bool snapEnabled  = false;
    f32  snapSize     = 1.0f;

    // ── Tilemap editing mode ───────────────────────────────────────────────
    bool  tilemapMode   = false;
    i32   selectedTile  = 0;

    // ── Viewport FBO ───────────────────────────────────────────────────────
    u32 GetFBO() const      { return m_FBO; }
    u32 GetFBOTex() const   { return m_FBOColor; }
    u32 GetFBOW() const     { return m_FBOW; }
    u32 GetFBOH() const     { return m_FBOH; }

    void ResizeFBO(u32 w, u32 h);

    // ── Log ────────────────────────────────────────────────────────────────
    static void PushLog(const std::string& msg);

private:
    // ── Rendering helpers ──────────────────────────────────────────────────
    void RenderGrid2D(f32 vpW, f32 vpH);
    void RenderSprites();
    void RenderGizmo2D(f32 vpW, f32 vpH);
    void RenderColliders2D(f32 vpW, f32 vpH);
    void RenderTilemapGrid(f32 vpW, f32 vpH);

    // ── Input ──────────────────────────────────────────────────────────────
    void HandleInput(f32 dt, f32 vpX, f32 vpY, f32 vpW, f32 vpH);
    void HandlePicking(f32 vpX, f32 vpY, f32 vpW, f32 vpH);
    void HandleDrag(f32 vpX, f32 vpY, f32 vpW, f32 vpH);
    void HandleTilemapPaint(f32 vpX, f32 vpY, f32 vpW, f32 vpH);

    // ── FBO ────────────────────────────────────────────────────────────────
    void CreateFBO(u32 w, u32 h);
    void DestroyFBO();

    // ── State ──────────────────────────────────────────────────────────────
    Window*         m_Window   = nullptr;
    OpenGLRenderer* m_Renderer = nullptr;
    Scene2D         m_Scene;
    Editor2DCamera  m_Camera;
    GameObject*     m_Selected = nullptr;
    Gizmo2DMode     m_GizmoMode = Gizmo2DMode::Translate;

    // Drag state
    bool m_Dragging      = false;
    Vec2 m_DragStart     {};
    Vec2 m_DragObjStart  {};
    f32  m_DragRotStart  = 0.0f;
    Vec2 m_DragScaleStart{};

    // FBO
    u32  m_FBO      = 0;
    u32  m_FBOColor = 0;
    u32  m_FBODepth = 0;
    u32  m_FBOW     = 1;
    u32  m_FBOH     = 1;

    bool m_Initialised = false;

    // ── Viewport screen position (for input mapping) ───────────────────────
    f32 m_VpX = 0, m_VpY = 0, m_VpW = 1, m_VpH = 1;
};

} // namespace gv

#endif // GV_HAS_GLFW
