// ============================================================================
// GameVoid Engine — Dear ImGui Editor UI
// ============================================================================
// Godot-style dockable editor built on Dear ImGui + GLFW + OpenGL 3.3.
//
//  ┌─────────── Menu Bar ──────────────────────────────────────────────┐
//  │ File | Edit | View | AI | Build                                   │
//  ├────────────┬──────────────────────────────┬───────────────────────┤
//  │ Hierarchy  │         Viewport             │      Inspector       │
//  │            │  (scene rendered to FBO)      │  Transform sliders   │
//  │            │                               │  Components          │
//  ├────────────┴──────────────────────────────┴───────────────────────┤
//  │ Console (logs, errors)                                            │
//  └───────────────────────────────────────────────────────────────────┘
//
// Public API:  Init() → BeginFrame() → Render() → EndFrame() → Shutdown()
// The Engine calls these in its game loop. All imgui state is encapsulated here.
// ============================================================================
#pragma once

#ifdef GV_HAS_GLFW

#include "core/Types.h"
#include "core/Math.h"
#include <string>
#include <vector>
#include <deque>

namespace gv {

// Forward declarations
class Scene;
class Camera;
class Window;
class PhysicsWorld;
class AIManager;
class ScriptEngine;
class AssetManager;
class GameObject;
class OpenGLRenderer;

/// Gizmo operation mode.
enum class GizmoMode { Translate, Rotate, Scale };

/// Godot-style dockable editor overlay driven by Dear ImGui.
class EditorUI {
public:
    EditorUI() = default;
    ~EditorUI() = default;

    // ── Lifecycle ──────────────────────────────────────────────────────────
    bool Init(Window* window, OpenGLRenderer* renderer, Scene* scene,
              PhysicsWorld* physics, AIManager* ai, ScriptEngine* script,
              AssetManager* assets);
    void Shutdown();

    /// Call at the start of the editor frame (after window input, before draw).
    void BeginFrame();

    /// Build all UI panels — call between BeginFrame and EndFrame.
    void Render(f32 dt);

    /// End the ImGui frame and push draw data to the screen.
    void EndFrame();

    // ── Log sink ───────────────────────────────────────────────────────────
    /// Push a log message into the console panel.
    static void PushLog(const std::string& msg);

    // ── State ──────────────────────────────────────────────────────────────
    bool WantsCaptureKeyboard() const;
    bool WantsCaptureMouse() const;

    /// Currently selected object (for inspector & gizmos).
    GameObject* GetSelectedObject() const { return m_Selected; }

    // ── Viewport FBO ───────────────────────────────────────────────────────
    /// Returns the FBO id for 3D scene rendering.
    u32 GetViewportFBO() const { return m_ViewportFBO; }
    u32 GetViewportWidth()  const { return m_ViewportW; }
    u32 GetViewportHeight() const { return m_ViewportH; }

    /// Resize the viewport FBO (called when the ImGui viewport panel resizes).
    void ResizeViewport(u32 w, u32 h);

    bool IsPlaying() const { return m_Playing; }

private:
    // ── Panel draw methods ─────────────────────────────────────────────────
    void DrawMenuBar();
    void DrawToolbar();
    void DrawHierarchy();
    void DrawInspector();
    void DrawConsole();
    void DrawViewport(f32 dt);

    // ── FBO helpers ────────────────────────────────────────────────────────
    void CreateViewportFBO(u32 w, u32 h);
    void DestroyViewportFBO();

    // ── Add-object helpers ─────────────────────────────────────────────────
    void AddCube();
    void AddLight();
    void DeleteSelected();
    void SaveScene(const std::string& path);
    void LoadScene(const std::string& path);

    // ── State ──────────────────────────────────────────────────────────────
    Window*         m_Window   = nullptr;
    OpenGLRenderer* m_Renderer = nullptr;
    Scene*          m_Scene    = nullptr;
    PhysicsWorld*   m_Physics  = nullptr;
    AIManager*      m_AI       = nullptr;
    ScriptEngine*   m_Script   = nullptr;
    AssetManager*   m_Assets   = nullptr;

    GameObject*     m_Selected = nullptr;
    GizmoMode       m_GizmoMode = GizmoMode::Translate;
    bool            m_Playing  = false;

    // Viewport FBO
    u32 m_ViewportFBO    = 0;
    u32 m_ViewportColor  = 0;   // colour texture attachment
    u32 m_ViewportDepth  = 0;   // depth renderbuffer
    u32 m_ViewportW      = 1;
    u32 m_ViewportH      = 1;

    // Console log ring-buffer
    static std::deque<std::string> s_Logs;
    static const size_t kMaxLogs = 512;

    // FPS
    f32 m_FPS = 0.0f;
    f32 m_FPSAccum = 0.0f;
    i32 m_FPSCount = 0;

    bool m_Initialised = false;
    bool m_ShowDemo    = false;
};

} // namespace gv

#endif // GV_HAS_GLFW
