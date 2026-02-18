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
#include "scripting/NativeScript.h"
#include "editor/OrbitCamera.h"
#include "editor/UndoRedo.h"
#include <string>
#include <vector>
#include <deque>
#include <set>

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
class MaterialComponent;
class NativeScriptComponent;
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

    /// Get all currently selected objects (multi-select).
    const std::set<GameObject*>& GetSelectedObjects() const { return m_MultiSelected; }

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
    void DrawCodeScriptPanel();        // inline script code editor
    void DrawBehaviorPanel();          // new: behavior editor panel

    // ── High-priority feature panels ───────────────────────────────────────
    void DrawAssetBrowser();           // file-tree asset browser
    void DrawKeyboardShortcuts();      // configurable hotkey panel
    void DrawViewportOverlayControls(f32 vpX, f32 vpY); // in-viewport overlay toggles
    void DrawGizmoCube(f32 vpX, f32 vpY, f32 vpW, f32 vpH, Camera* cam); // orientation cube
    void DrawCameraPreviewPiP(f32 vpX, f32 vpY, f32 vpW, f32 vpH); // PiP preview
    void DrawGridSnapSettings();       // grid & snap settings popup

    // ── Inspector sub-sections ─────────────────────────────────────────────
    void DrawInspectorMaterial();       // Material component editor in inspector
    void DrawInspectorScripts();        // Script/behavior list in inspector
    void DrawInspectorAddComponent();   // "Add Component" button & dropdown

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

    // ── Multi-select & clipboard helpers ───────────────────────────────────
    void SelectObject(GameObject* obj, bool additive);
    void SelectAll();
    void ClearSelection();
    void DuplicateSelected();
    void CopySelected();
    void PasteClipboard();

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

    // ── Multi-select ───────────────────────────────────────────────────────
    std::set<GameObject*> m_MultiSelected;

    // ── Clipboard for copy/paste ───────────────────────────────────────────
    struct ClipboardEntry {
        std::string name;
        Vec3 position, scale;
        Quaternion rotation;
        // Simplified: stores component type names + mesh/color data
        int primitiveType = 0;
        Vec4 color{0.7f, 0.7f, 0.7f, 1.0f};
        bool hasRigidBody  = false;
        bool hasCollider   = false;
    };
    std::vector<ClipboardEntry> m_Clipboard;

    // ── Undo/Redo ──────────────────────────────────────────────────────────
    UndoStack       m_UndoStack;

    // ── Property undo tracking ─────────────────────────────────────────────
    // Stores the old transform when a drag starts, pushed to undo stack on release
    bool  m_PropertyUndoPending = false;
    Vec3  m_PropertyOldPos   {};
    Vec3  m_PropertyOldScale {};
    Quaternion m_PropertyOldRot {};
    u32   m_PropertyObjID = 0;

    // ── Placement mode (drag-to-place objects) ─────────────────────────────
    enum class PlacementType { None, Cube, Light, Terrain, Particles, Floor };
    PlacementType m_PlacementType = PlacementType::None;
    Vec3 m_PlacementPreviewPos { 0, 0, 0 };  // ghost position under cursor
    bool m_PlacementValid = false;            // true when cursor is over valid ground
    void BeginPlacement(PlacementType type);
    void UpdatePlacement(Camera* cam, f32 mousePosX, f32 mousePosY);
    void FinishPlacement();
    void CancelPlacement();
    Vec3 RaycastGroundPlane(Camera* cam, f32 ndcX, f32 ndcY, f32 planeY = 0.0f);

    // ── Object picking / drag ──────────────────────────────────────────────
    bool m_Dragging      = false;
    i32  m_DragAxis      = -1;       // 0=X, 1=Y, 2=Z, -1=none
    Vec3 m_DragStart     {};
    Vec3 m_DragObjStart  {};
    Vec3 m_DragRotStart  {};
    Vec3 m_DragScaleStart{};
    bool m_SnapToGrid    = false;
    f32  m_GridSize       = 1.0f;
    f32  m_RotationSnap   = 15.0f;   // rotation snap angle in degrees
    f32  m_ScaleSnap      = 0.1f;    // scale snap increment

    // Viewport screen-space info for picking
    f32  m_VpScreenX = 0, m_VpScreenY = 0; // top-left of viewport in screen coords
    f32  m_VpScreenW = 1, m_VpScreenH = 1;

    // Viewport FBO
    u32 m_ViewportFBO    = 0;
    u32 m_ViewportColor  = 0;   // colour texture attachment
    u32 m_ViewportDepth  = 0;   // depth renderbuffer
    u32 m_ViewportW      = 1;
    u32 m_ViewportH      = 1;

    // ── Secondary viewport for split view ──────────────────────────────────
    u32 m_ViewportFBO2   = 0;
    u32 m_ViewportColor2 = 0;
    u32 m_ViewportDepth2 = 0;
    u32 m_ViewportW2     = 1;
    u32 m_ViewportH2     = 1;
    enum class ViewportLayout { Single, SideBySide, Quad };
    ViewportLayout m_ViewportLayout = ViewportLayout::Single;
    // Camera angles for secondary viewports
    enum class ViewAngle { Perspective, Top, Front, Right };
    ViewAngle m_SecondaryViewAngle = ViewAngle::Top;

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
    bool m_ShowEditorSettings = false;   // Editor Settings popup (API key input)
    bool m_ShowKeyboardShortcuts = false; // Keyboard shortcuts window
    bool m_ShowGridSnapSettings  = false; // Grid/snap config popup
    bool m_ShowAssetBrowser      = false; // Asset browser panel

    // ── Viewport overlay toggles ───────────────────────────────────────────
    bool m_ShowWireframe      = false;
    bool m_ShowBoundingBoxes  = false;
    bool m_ShowCollisionShapes = false;
    bool m_ShowNormals        = false;
    bool m_ShowGrid           = true;
    bool m_ShowGizmos         = true;
    bool m_ShowCameraPiP      = false;

    // ── Hierarchy search / filter ──────────────────────────────────────────
    char m_HierarchySearchBuf[128] = {};

    // ── Drag-and-drop reparenting ──────────────────────────────────────────
    GameObject* m_DragDropSource = nullptr;

    // ── Asset browser state ────────────────────────────────────────────────
    std::string m_AssetBrowserRoot;           // project root
    std::string m_AssetBrowserCurrentDir;     // currently browsed folder
    char m_AssetSearchBuf[128] = {};

    // ── AI Generator state ─────────────────────────────────────────────────
    char   m_AIPromptBuf[1024] = {};
    bool   m_AIGenerating  = false;
    f32    m_AIProgress    = 0.0f;     // 0..1 progress bar
    std::string m_AIStatusMsg;
    std::vector<u32> m_AILastSpawnedIDs;   // for undo

    // ── AI Settings state ──────────────────────────────────────────────────
    char   m_AIKeyBuf[256] = {};        // API key input buffer

    // ── Bottom tab state ───────────────────────────────────────────────────
    i32 m_BottomTab = 0;   // 0=Console, 1=Terrain, 2=Material, 3=Particle, 4=Animation, 5=NodeScript, 6=Behavior, 7=CodeScript, 8=AssetBrowser

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

    // ── Orbit camera ────────────────────────────────────────────────────────
    OrbitCameraController m_OrbitCam;  // orbit/pan/zoom controller for viewport
    bool m_OrbitActive     = false;    // true while orbiting (MMB or RMB drag)
    bool m_PanActive       = false;    // true while panning  (Shift+MMB)
    Vec2 m_LastMousePos    {};         // previous mouse pos for delta calc
    // ── Code Script editor state ────────────────────────────────────────
    char m_ScriptCodeBuf[4096] = {};   // inline script source editor
    char m_ScriptPathBuf[256]  = {};   // script file path
    // ── Behavior editor state ──────────────────────────────────────────────
    i32  m_AddComponentIdx = 0;        // "Add Component" dropdown index
    i32  m_AddBehaviorIdx  = 0;        // behavior dropdown index

    // ── Subsystem instances ────────────────────────────────────────────────
    MaterialLibrary*  m_MaterialLib = nullptr;
    AnimationLibrary* m_AnimLib     = nullptr;
};

} // namespace gv

#endif // GV_HAS_GLFW
