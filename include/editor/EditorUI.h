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
#include "renderer/Renderer.h"
#include "ai/AIManager.h"
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
class TerrainComponent;
class PBRMaterial;
class MaterialLibrary;
class ShaderGraph;
class ParticleEmitter;
class AnimationClip;
class Animator;
class AnimationLibrary;
class NodeGraph;
struct TerrainBrush;

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
    void DrawAIGenerator();
    void DrawBottomTabs();

    // ── New system panels ──────────────────────────────────────────────────
    void DrawTerrainPanel();
    void DrawMaterialPanel();
    void DrawParticlePanel();
    void DrawAnimationPanel();
    void DrawNodeScriptPanel();

    // ── AI Generator helpers ───────────────────────────────────────────────
    void AIGenerate();          // kick off generation
    void AISpawnBlueprints();   // instantiate parsed blueprints into scene
    void AISpawnBlueprintsFrom(const std::vector<AIManager::ObjectBlueprint>& blueprints);
    void AIUndoLastGeneration();

    // ── FBO helpers ────────────────────────────────────────────────────────
    void CreateViewportFBO(u32 w, u32 h);
    void DestroyViewportFBO();

    // ── Add-object helpers ─────────────────────────────────────────────────
    void AddCube();
    void AddLight();
    void AddTerrain();
    void AddParticleEmitter();
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

    // ── Object picking / drag ──────────────────────────────────────────────
    bool m_Dragging      = false;
    i32  m_DragAxis      = -1;       // 0=X, 1=Y, 2=Z, -1=none
    Vec3 m_DragStart     {};
    Vec3 m_DragObjStart  {};
    Vec3 m_DragRotStart  {};
    Vec3 m_DragScaleStart{};
    bool m_SnapToGrid    = false;
    f32  m_GridSize       = 1.0f;

    // Viewport screen-space info for picking
    f32  m_VpScreenX = 0, m_VpScreenY = 0; // top-left of viewport in screen coords
    f32  m_VpScreenW = 1, m_VpScreenH = 1;

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
    bool m_ShowAIPanel = true;

    // ── AI Generator state ─────────────────────────────────────────────────
    char   m_AIPromptBuf[1024] = {};
    bool   m_AIGenerating  = false;
    f32    m_AIProgress    = 0.0f;     // 0..1 progress bar
    std::string m_AIStatusMsg;
    std::vector<u32> m_AILastSpawnedIDs;   // for undo

    // ── Bottom tab state ───────────────────────────────────────────────────
    i32 m_BottomTab = 0;   // 0=Console, 1=Terrain, 2=Material, 3=Particle, 4=Animation, 5=Script

    // ── Terrain editor state ───────────────────────────────────────────────
    i32  m_TerrainRes       = 64;
    f32  m_TerrainSize      = 40.0f;
    f32  m_TerrainHeight    = 6.0f;
    i32  m_TerrainSeed      = 42;
    i32  m_TerrainOctaves   = 6;
    i32  m_TerrainBrushMode = 0;   // 0=Raise,1=Lower,2=Smooth,3=Flatten,4=Paint
    f32  m_TerrainBrushRadius   = 3.0f;
    f32  m_TerrainBrushStrength = 0.4f;
    i32  m_TerrainPaintLayer    = 0;

    // ── Material editor state ──────────────────────────────────────────────
    i32  m_MatSelectedIdx  = -1;
    char m_MatNameBuf[128] = "New Material";
    f32  m_MatAlbedo[4]    = { 0.8f, 0.8f, 0.8f, 1.0f };
    f32  m_MatRoughness    = 0.5f;
    f32  m_MatMetallic     = 0.0f;
    f32  m_MatEmission[3]  = { 0, 0, 0 };
    f32  m_MatEmissionStr  = 0.0f;
    f32  m_MatAO           = 1.0f;

    // ── Particle editor state ──────────────────────────────────────────────
    i32  m_ParticlePreset = 0;

    // ── Animation editor state ─────────────────────────────────────────────
    char m_AnimClipName[64] = "Clip";
    f32  m_AnimTimeline     = 0.0f;
    bool m_AnimPlaying      = false;
    f32  m_AnimSpeed        = 1.0f;

    // ── Node Scripting editor state ────────────────────────────────────────
    i32  m_NodeAddType = 0;
    Vec2 m_NodeCanvasOffset { 0, 0 };
    f32  m_NodeZoom = 1.0f;

    // ── Subsystem instances ────────────────────────────────────────────────
    MaterialLibrary*  m_MaterialLib = nullptr;
    AnimationLibrary* m_AnimLib     = nullptr;
};

} // namespace gv

#endif // GV_HAS_GLFW
