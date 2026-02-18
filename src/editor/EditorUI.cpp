// ============================================================================
// GameVoid Engine — Dear ImGui Editor UI Implementation
// ============================================================================
#ifdef GV_HAS_GLFW

#include "editor/EditorUI.h"

// Dear ImGui core + backends
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

// Engine headers
#include "core/GLDefs.h"
#include "core/Window.h"
#include "core/Scene.h"
#include "core/GameObject.h"
#include "renderer/Renderer.h"
#include "renderer/Camera.h"
#include "renderer/Lighting.h"
#include "renderer/MeshRenderer.h"
#include "renderer/Material.h"
#include "renderer/MaterialComponent.h"
#include "physics/Physics.h"
#include "ai/AIManager.h"
#include "terrain/Terrain.h"
#include "effects/ParticleSystem.h"
#include "animation/Animation.h"
#include "scripting/NodeGraph.h"
#include "scripting/NativeScript.h"
#include "core/SceneSerializer.h"
#include "scripting/ScriptEngine.h"
#include "editor/UndoRedo.h"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <algorithm>
#include <cstdio>
#include <cmath>
#include <cfloat>
#include <fstream>
#include <sstream>
#include <cstring>
#include <set>
#include <filesystem>

namespace gv {

// ── Static log buffer ──────────────────────────────────────────────────────
std::deque<std::string> EditorUI::s_Logs;

void EditorUI::PushLog(const std::string& msg) {
    s_Logs.push_back(msg);
    while (s_Logs.size() > kMaxLogs) s_Logs.pop_front();
}

// ── Lifecycle ──────────────────────────────────────────────────────────────

bool EditorUI::Init(Window* window, OpenGLRenderer* renderer, Scene* scene,
                    PhysicsWorld* physics, AIManager* ai, ScriptEngine* script,
                    AssetManager* assets) {
    m_Window   = window;
    m_Renderer = renderer;
    m_Scene    = scene;
    m_Physics  = physics;
    m_AI       = ai;
    m_Script   = script;
    m_Assets   = assets;

    if (!m_Window || !m_Window->IsInitialised()) {
        GV_LOG_ERROR("EditorUI::Init — window not ready.");
        return false;
    }

    // ── ImGui context ──────────────────────────────────────────────────────
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    // Dark theme with slight blue tint
    ImGui::StyleColorsDark();
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding   = 4.0f;
    style.FrameRounding    = 3.0f;
    style.GrabRounding     = 3.0f;
    style.TabRounding      = 4.0f;
    style.WindowPadding    = ImVec2(8, 8);
    style.Colors[ImGuiCol_WindowBg]      = ImVec4(0.12f, 0.12f, 0.15f, 1.0f);
    style.Colors[ImGuiCol_TitleBg]       = ImVec4(0.08f, 0.08f, 0.10f, 1.0f);
    style.Colors[ImGuiCol_TitleBgActive] = ImVec4(0.15f, 0.15f, 0.22f, 1.0f);
    style.Colors[ImGuiCol_MenuBarBg]     = ImVec4(0.10f, 0.10f, 0.14f, 1.0f);

    // Platform/Renderer backends
    GLFWwindow* glfwWin = m_Window->GetNativeWindow();
    ImGui_ImplGlfw_InitForOpenGL(glfwWin, true);   // true = install callbacks
    ImGui_ImplOpenGL3_Init("#version 330 core");

    // Create scene FBO at initial size
    CreateViewportFBO(m_Window->GetWidth(), m_Window->GetHeight());

    // ── Initialise orbit camera from the scene camera position ─────────────
    Camera* cam = m_Scene ? m_Scene->GetActiveCamera() : nullptr;
    if (cam && cam->GetOwner()) {
        Vec3 camPos = cam->GetOwner()->GetTransform().position;
        // Default: look at origin from the camera's initial position
        m_OrbitCam.focusPoint = Vec3(0, 0, 0);
        Vec3 diff = camPos - m_OrbitCam.focusPoint;
        m_OrbitCam.distance = diff.Length();
        if (m_OrbitCam.distance < 0.1f) m_OrbitCam.distance = 8.0f;
        // Compute yaw/pitch from offset
        m_OrbitCam.yaw   = std::atan2(diff.x, diff.z) * (180.0f / 3.14159265f);
        m_OrbitCam.pitch = -std::asin(diff.y / m_OrbitCam.distance) * (180.0f / 3.14159265f);
        m_OrbitCam.ApplyToTransform(cam->GetOwner()->GetTransform());
    }

    m_Initialised = true;
    GV_LOG_INFO("EditorUI initialised (Dear ImGui " + std::string(IMGUI_VERSION) + ").");
    PushLog("[Editor] Ready.");
    return true;
}

void EditorUI::Shutdown() {
    if (!m_Initialised) return;
    DestroyViewportFBO();
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    m_Initialised = false;
    GV_LOG_INFO("EditorUI shut down.");
}

void EditorUI::BeginFrame() {
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
}

void EditorUI::EndFrame() {
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

bool EditorUI::WantsCaptureKeyboard() const {
    return ImGui::GetIO().WantCaptureKeyboard;
}

bool EditorUI::WantsCaptureMouse() const {
    return ImGui::GetIO().WantCaptureMouse;
}

// ── Main Render ────────────────────────────────────────────────────────────

void EditorUI::Render(f32 dt) {
    // ── Keyboard shortcuts ─────────────────────────────────────────────────
    ImGuiIO& io = ImGui::GetIO();
    if (!io.WantTextInput) {
        // Undo / Redo
        if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Z)) {
            if (m_UndoStack.CanUndo()) { m_UndoStack.Undo(); PushLog("[Edit] Undo"); }
        }
        if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Y)) {
            if (m_UndoStack.CanRedo()) { m_UndoStack.Redo(); PushLog("[Edit] Redo"); }
        }
        // Copy / Paste / Duplicate
        if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_C)) { CopySelected(); }
        if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_V)) { PasteClipboard(); }
        if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_D)) { DuplicateSelected(); }
        // Select All
        if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_A)) { SelectAll(); }
        // Delete selected
        if (ImGui::IsKeyPressed(ImGuiKey_Delete)) { DeleteSelected(); }
        // Gizmo mode shortcuts: W/E/R
        if (ImGui::IsKeyPressed(ImGuiKey_W)) { m_GizmoMode = GizmoMode::Translate; }
        if (ImGui::IsKeyPressed(ImGuiKey_E)) { m_GizmoMode = GizmoMode::Rotate; }
        if (ImGui::IsKeyPressed(ImGuiKey_R)) { m_GizmoMode = GizmoMode::Scale; }
        // Toggle wireframe
        if (ImGui::IsKeyPressed(ImGuiKey_Z) && !io.KeyCtrl) { m_ShowWireframe = !m_ShowWireframe; }
    }

    // FPS counter
    m_FPSAccum += dt;
    m_FPSCount++;
    if (m_FPSAccum >= 0.5f) {
        m_FPS = static_cast<f32>(m_FPSCount) / m_FPSAccum;
        m_FPSAccum = 0.0f;
        m_FPSCount = 0;
    }

    // ── Manual Godot-style panel layout ────────────────────────────────────
    // Layout:  Menu Bar (full width)
    //          Toolbar (full width, thin)
    //          Left: Hierarchy | Centre: Viewport | Right: Inspector
    //          Bottom: Console
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    const f32 winW = viewport->WorkSize.x;
    const f32 winH = viewport->WorkSize.y;
    const f32 startY = viewport->WorkPos.y;
    const f32 startX = viewport->WorkPos.x;

    const f32 menuBarH   = 22.0f;
    const f32 toolbarH   = 36.0f;
    const f32 consoleH   = 180.0f;
    const f32 sideW      = 260.0f;
    const f32 aiPanelH   = m_ShowAIPanel ? 220.0f : 0.0f;
    const f32 topY       = startY;   // menu bar is drawn by MainMenuBar

    // Menu bar (uses ImGui::BeginMainMenuBar which auto-positions)
    DrawMenuBar();

    // Toolbar — full width, just below menu bar
    const f32 tbY = startY + menuBarH;
    ImGui::SetNextWindowPos(ImVec2(startX, tbY));
    ImGui::SetNextWindowSize(ImVec2(winW, toolbarH));
    DrawToolbar();

    // Hierarchy — left panel
    const f32 panelY = tbY + toolbarH;
    const f32 panelH = winH - menuBarH - toolbarH - consoleH;
    const f32 hierH  = panelH - aiPanelH;
    ImGui::SetNextWindowPos(ImVec2(startX, panelY));
    ImGui::SetNextWindowSize(ImVec2(sideW, hierH));
    DrawHierarchy();

    // AI Generator — below hierarchy on the left
    if (m_ShowAIPanel) {
        ImGui::SetNextWindowPos(ImVec2(startX, panelY + hierH));
        ImGui::SetNextWindowSize(ImVec2(sideW, aiPanelH));
        DrawAIGenerator();
    }

    // Inspector — right panel
    ImGui::SetNextWindowPos(ImVec2(startX + winW - sideW, panelY));
    ImGui::SetNextWindowSize(ImVec2(sideW, panelH));
    DrawInspector();

    // Viewport — centre
    const f32 vpX = startX + sideW;
    const f32 vpW = winW - 2.0f * sideW;
    ImGui::SetNextWindowPos(ImVec2(vpX, panelY));
    ImGui::SetNextWindowSize(ImVec2(vpW, panelH));
    DrawViewport(dt);

    // Console — bottom (now tabbed: Console / Terrain / Material / Particle / Animation / Script)
    ImGui::SetNextWindowPos(ImVec2(startX, panelY + panelH));
    ImGui::SetNextWindowSize(ImVec2(winW, consoleH));
    DrawBottomTabs();

    if (m_ShowDemo)
        ImGui::ShowDemoWindow(&m_ShowDemo);
}

// ── Menu Bar ───────────────────────────────────────────────────────────────

void EditorUI::DrawMenuBar() {
    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("New Scene"))       { /* TODO */ PushLog("[File] New Scene"); }
            if (ImGui::MenuItem("Save Scene"))      { SaveScene("scene.gvs"); }
            if (ImGui::MenuItem("Load Scene"))      { LoadScene("scene.gvs"); }
            ImGui::Separator();
            if (ImGui::MenuItem("Exit"))            { m_Window->SetShouldClose(true); }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Edit")) {
            // Undo / Redo
            std::string undoLabel = "Undo";
            if (m_UndoStack.CanUndo()) undoLabel += " (" + m_UndoStack.GetUndoDescription() + ")";
            if (ImGui::MenuItem(undoLabel.c_str(), "Ctrl+Z", false, m_UndoStack.CanUndo())) {
                m_UndoStack.Undo();
                PushLog("[Edit] Undo: " + m_UndoStack.GetRedoDescription());
            }
            std::string redoLabel = "Redo";
            if (m_UndoStack.CanRedo()) redoLabel += " (" + m_UndoStack.GetRedoDescription() + ")";
            if (ImGui::MenuItem(redoLabel.c_str(), "Ctrl+Y", false, m_UndoStack.CanRedo())) {
                m_UndoStack.Redo();
                PushLog("[Edit] Redo");
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Copy", "Ctrl+C"))       { CopySelected(); }
            if (ImGui::MenuItem("Paste", "Ctrl+V"))      { PasteClipboard(); }
            if (ImGui::MenuItem("Duplicate", "Ctrl+D"))  { DuplicateSelected(); }
            if (ImGui::MenuItem("Select All", "Ctrl+A")) { SelectAll(); }
            ImGui::Separator();
            if (ImGui::MenuItem("Add Cube"))        { AddCube(); }
            if (ImGui::MenuItem("Add Light"))       { AddLight(); }
            if (ImGui::MenuItem("Add Terrain"))     { AddTerrain(); }
            if (ImGui::MenuItem("Add Particles"))   { AddParticleEmitter(); }
            ImGui::Separator();
            if (ImGui::MenuItem("Delete Selected", "Del")) { DeleteSelected(); }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("View")) {
            ImGui::MenuItem("AI Generator", nullptr, &m_ShowAIPanel);
            ImGui::MenuItem("Asset Browser", nullptr, &m_ShowAssetBrowser);
            ImGui::MenuItem("ImGui Demo", nullptr, &m_ShowDemo);
            ImGui::Separator();
            if (ImGui::MenuItem("Console"))     { m_BottomTab = 0; }
            if (ImGui::MenuItem("Terrain"))     { m_BottomTab = 1; }
            if (ImGui::MenuItem("Material"))    { m_BottomTab = 2; }
            if (ImGui::MenuItem("Particles"))   { m_BottomTab = 3; }
            if (ImGui::MenuItem("Animation"))   { m_BottomTab = 4; }
            if (ImGui::MenuItem("Node Script")) { m_BottomTab = 5; }
            if (ImGui::MenuItem("Behaviors"))   { m_BottomTab = 6; }
            if (ImGui::MenuItem("Code Script")) { m_BottomTab = 7; }
            ImGui::Separator();

            // Viewport overlay toggles
            if (ImGui::BeginMenu("Viewport Overlays")) {
                ImGui::MenuItem("Wireframe (Z)", nullptr, &m_ShowWireframe);
                ImGui::MenuItem("Bounding Boxes", nullptr, &m_ShowBoundingBoxes);
                ImGui::MenuItem("Collision Shapes", nullptr, &m_ShowCollisionShapes);
                ImGui::MenuItem("Normals", nullptr, &m_ShowNormals);
                ImGui::MenuItem("Grid", nullptr, &m_ShowGrid);
                ImGui::MenuItem("Gizmos", nullptr, &m_ShowGizmos);
                ImGui::MenuItem("Camera Preview (PiP)", nullptr, &m_ShowCameraPiP);
                ImGui::EndMenu();
            }

            // Viewport layout
            if (ImGui::BeginMenu("Viewport Layout")) {
                if (ImGui::MenuItem("Single", nullptr, m_ViewportLayout == ViewportLayout::Single))
                    m_ViewportLayout = ViewportLayout::Single;
                if (ImGui::MenuItem("Side by Side", nullptr, m_ViewportLayout == ViewportLayout::SideBySide))
                    m_ViewportLayout = ViewportLayout::SideBySide;
                if (ImGui::MenuItem("Quad View", nullptr, m_ViewportLayout == ViewportLayout::Quad))
                    m_ViewportLayout = ViewportLayout::Quad;
                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("Post-Processing")) {
                if (m_Renderer) {
                    bool bloom = m_Renderer->IsBloomEnabled();
                    if (ImGui::Checkbox("Bloom", &bloom)) {
                        m_Renderer->SetBloomEnabled(bloom);
                        PushLog(bloom ? "[Render] Bloom ON" : "[Render] Bloom OFF");
                    }
                    bool tonemap = m_Renderer->IsToneMappingEnabled();
                    if (ImGui::Checkbox("Tone Mapping (ACES)", &tonemap)) {
                        m_Renderer->SetToneMappingEnabled(tonemap);
                        PushLog(tonemap ? "[Render] Tone Mapping ON" : "[Render] Tone Mapping OFF");
                    }
                    f32 exposure = m_Renderer->GetExposure();
                    if (ImGui::SliderFloat("Exposure", &exposure, 0.1f, 5.0f, "%.2f")) {
                        m_Renderer->SetExposure(exposure);
                    }
                }
                ImGui::EndMenu();
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Grid & Snap Settings")) { m_ShowGridSnapSettings = true; }
            if (ImGui::MenuItem("Keyboard Shortcuts"))   { m_ShowKeyboardShortcuts = true; }
            if (ImGui::MenuItem("Editor Settings"))      { m_ShowEditorSettings = true; }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("AI")) {
            if (ImGui::MenuItem("Open AI Generator")) { m_ShowAIPanel = true; }
            if (ImGui::MenuItem("Generate Level"))     { AIGenerate(); }
            if (ImGui::MenuItem("Undo AI Generation")) { AIUndoLastGeneration(); }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Build")) {
            if (ImGui::MenuItem("Build & Run"))     { PushLog("[Build] Build & Run (placeholder)."); }
            ImGui::EndMenu();
        }

        // FPS in the menu bar
        char fpsBuf[32];
        std::snprintf(fpsBuf, sizeof(fpsBuf), "  %.0f FPS", m_FPS);
        float w = ImGui::GetContentRegionAvail().x;
        float tw = ImGui::CalcTextSize(fpsBuf).x;
        ImGui::SameLine(w - tw + 8);
        ImGui::TextDisabled("%s", fpsBuf);

        ImGui::EndMainMenuBar();
    }
}

// ── Toolbar ────────────────────────────────────────────────────────────────

void EditorUI::DrawToolbar() {
    ImGui::Begin("Toolbar", nullptr,
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse |
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse);

    bool trSel = (m_GizmoMode == GizmoMode::Translate);
    bool rtSel = (m_GizmoMode == GizmoMode::Rotate);
    bool scSel = (m_GizmoMode == GizmoMode::Scale);

    if (trSel) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.3f, 0.5f, 0.8f, 1));
    if (ImGui::Button("Translate")) m_GizmoMode = GizmoMode::Translate;
    if (trSel) ImGui::PopStyleColor();

    ImGui::SameLine();
    if (rtSel) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.3f, 0.5f, 0.8f, 1));
    if (ImGui::Button("Rotate")) m_GizmoMode = GizmoMode::Rotate;
    if (rtSel) ImGui::PopStyleColor();

    ImGui::SameLine();
    if (scSel) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.3f, 0.5f, 0.8f, 1));
    if (ImGui::Button("Scale")) m_GizmoMode = GizmoMode::Scale;
    if (scSel) ImGui::PopStyleColor();

    ImGui::SameLine(); ImGui::Separator(); ImGui::SameLine();

    if (!m_Playing) {
        if (ImGui::Button("  Play  ")) { m_Playing = true; PushLog("[Editor] Play mode."); }
    } else {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.7f, 0.2f, 0.2f, 1));
        if (ImGui::Button("  Stop  ")) { m_Playing = false; PushLog("[Editor] Stopped."); }
        ImGui::PopStyleColor();
    }

    ImGui::SameLine(); ImGui::Separator(); ImGui::SameLine();

    // Placement buttons — highlight active placement type
    auto placeBtn = [&](const char* label, PlacementType pt) {
        bool active = (m_PlacementType == pt);
        if (active) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.1f, 0.7f, 0.3f, 1.0f));
        if (ImGui::Button(label)) {
            if (active) CancelPlacement(); else BeginPlacement(pt);
        }
        if (active) ImGui::PopStyleColor();
        ImGui::SameLine();
    };
    placeBtn("+ Cube",      PlacementType::Cube);
    placeBtn("+ Light",     PlacementType::Light);
    placeBtn("+ Terrain",   PlacementType::Terrain);
    placeBtn("+ Particles", PlacementType::Particles);
    placeBtn("+ Floor",     PlacementType::Floor);
    ImGui::SameLine(); ImGui::Separator(); ImGui::SameLine();
    ImGui::Checkbox("Snap", &m_SnapToGrid);
    if (m_SnapToGrid) {
        ImGui::SameLine();
        ImGui::SetNextItemWidth(50);
        ImGui::DragFloat("##GridSz", &m_GridSize, 0.1f, 0.1f, 10.0f, "%.1f");
    }
    ImGui::SameLine();
    if (ImGui::Button("Delete"))  DeleteSelected();

    ImGui::End();
}

// ── Hierarchy Panel ────────────────────────────────────────────────────────

void EditorUI::DrawHierarchy() {
    ImGui::Begin("Scene Hierarchy");

    if (!m_Scene) { ImGui::Text("No scene."); ImGui::End(); return; }

    // ── Search / Filter bar ────────────────────────────────────────────────
    ImGui::SetNextItemWidth(-1);
    ImGui::InputTextWithHint("##HierSearch", "Search...", m_HierarchySearchBuf, sizeof(m_HierarchySearchBuf));
    ImGui::Separator();

    std::string filterStr(m_HierarchySearchBuf);
    // Convert to lowercase for case-insensitive matching
    std::string filterLower = filterStr;
    for (auto& c : filterLower) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));

    for (auto& obj : m_Scene->GetAllObjects()) {
        if (!obj) continue;

        // Apply filter
        if (!filterLower.empty()) {
            std::string nameLower = obj->GetName();
            for (auto& c : nameLower) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            if (nameLower.find(filterLower) == std::string::npos) continue;
        }

        bool selected = (m_Selected == obj.get()) || (m_MultiSelected.count(obj.get()) > 0);
        std::string label = obj->GetName() + "##" + std::to_string(obj->GetID());

        // Highlight multi-selected objects
        if (m_MultiSelected.count(obj.get()) > 0 && m_Selected != obj.get())
            ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.2f, 0.4f, 0.6f, 0.6f));

        if (ImGui::Selectable(label.c_str(), selected)) {
            bool ctrl  = ImGui::GetIO().KeyCtrl;
            bool shift = ImGui::GetIO().KeyShift;
            SelectObject(obj.get(), ctrl || shift);
        }

        if (m_MultiSelected.count(obj.get()) > 0 && m_Selected != obj.get())
            ImGui::PopStyleColor();

        // ── Drag-and-drop reparenting ──────────────────────────────────────
        if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID)) {
            m_DragDropSource = obj.get();
            ImGui::SetDragDropPayload("GV_GAMEOBJECT", &m_DragDropSource, sizeof(GameObject*));
            ImGui::Text("Move '%s'", obj->GetName().c_str());
            ImGui::EndDragDropSource();
        }

        if (ImGui::BeginDragDropTarget()) {
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("GV_GAMEOBJECT")) {
                GameObject* dropped = *static_cast<GameObject**>(payload->Data);
                if (dropped && dropped != obj.get()) {
                    // Reparent: remove from old parent, add as child of target
                    if (dropped->GetParent()) {
                        dropped->GetParent()->RemoveChild(dropped);
                    }
                    // Note: simple reparenting at the scene level
                    dropped->SetParent(obj.get());
                    PushLog("[Hierarchy] Reparented '" + dropped->GetName() + "' under '" + obj->GetName() + "'");
                }
            }
            ImGui::EndDragDropTarget();
        }

        // Show children indented
        for (auto& child : obj->GetChildren()) {
            if (!child) continue;
            std::string childLabel = "  > " + child->GetName() + "##" + std::to_string(child->GetID());
            bool childSel = (m_Selected == child.get()) || (m_MultiSelected.count(child.get()) > 0);
            if (ImGui::Selectable(childLabel.c_str(), childSel)) {
                SelectObject(child.get(), ImGui::GetIO().KeyCtrl || ImGui::GetIO().KeyShift);
            }
        }
    }

    // Selection count indicator
    if (m_MultiSelected.size() > 1) {
        ImGui::Separator();
        ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1), "%d objects selected", static_cast<int>(m_MultiSelected.size()));
    }

    ImGui::End();
}

// ── Inspector Panel ────────────────────────────────────────────────────────

void EditorUI::DrawInspector() {
    ImGui::Begin("Inspector");

    if (!m_Selected) {
        ImGui::TextDisabled("Select an object in the Hierarchy.");
        ImGui::End();
        return;
    }

    // Name
    char nameBuf[128];
    std::snprintf(nameBuf, sizeof(nameBuf), "%s", m_Selected->GetName().c_str());
    if (ImGui::InputText("Name", nameBuf, sizeof(nameBuf))) {
        m_Selected->SetName(nameBuf);
    }

    ImGui::Separator();

    // Transform
    Transform& t = m_Selected->GetTransform();
    if (ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen)) {
        float pos[3] = { t.position.x, t.position.y, t.position.z };
        if (ImGui::DragFloat3("Position", pos, 0.05f)) {
            t.position = Vec3(pos[0], pos[1], pos[2]);
        }

        // Show rotation as Euler (approximate)
        float rotDeg[3] = { 0, 0, 0 };
        {
            const Quaternion& q = t.rotation;
            f32 sinp = 2.0f * (q.w * q.x + q.y * q.z);
            f32 cosp = 1.0f - 2.0f * (q.x * q.x + q.y * q.y);
            rotDeg[0] = std::atan2(sinp, cosp) * (180.0f / 3.14159265358979f);
            f32 siny = 2.0f * (q.w * q.y - q.z * q.x);
            if (std::fabs(siny) >= 1.0f)
                rotDeg[1] = std::copysign(90.0f, siny);
            else
                rotDeg[1] = std::asin(siny) * (180.0f / 3.14159265358979f);
            f32 sinr = 2.0f * (q.w * q.z + q.x * q.y);
            f32 cosr = 1.0f - 2.0f * (q.y * q.y + q.z * q.z);
            rotDeg[2] = std::atan2(sinr, cosr) * (180.0f / 3.14159265358979f);
        }
        if (ImGui::DragFloat3("Rotation", rotDeg, 0.5f)) {
            t.SetEulerDeg(rotDeg[0], rotDeg[1], rotDeg[2]);
        }

        float scl[3] = { t.scale.x, t.scale.y, t.scale.z };
        if (ImGui::DragFloat3("Scale", scl, 0.02f, 0.01f, 100.0f)) {
            t.scale = Vec3(scl[0], scl[1], scl[2]);
        }
    }

    // ── Material Component (Godot-style) ───────────────────────────────────
    DrawInspectorMaterial();

    // ── Rigidbody toggle ───────────────────────────────────────────────────
    {
        auto* rb = m_Selected->GetComponent<RigidBody>();
        bool hasRB = (rb != nullptr);
        if (ImGui::CollapsingHeader("Rigidbody", ImGuiTreeNodeFlags_DefaultOpen)) {
            if (ImGui::Checkbox("Has Rigidbody", &hasRB)) {
                if (hasRB && !rb) {
                    rb = m_Selected->AddComponent<RigidBody>();
                    rb->useGravity = true;
                    m_Selected->AddComponent<Collider>()->type = ColliderType::Box;
                    if (m_Physics) m_Physics->RegisterBody(rb);
                    PushLog("[Inspector] Added RigidBody to " + m_Selected->GetName());
                } else if (!hasRB && rb) {
                    m_Selected->RemoveComponent<Collider>();
                    m_Selected->RemoveComponent<RigidBody>();
                    PushLog("[Inspector] Removed RigidBody from " + m_Selected->GetName());
                }
            }
            if (rb) {
                ImGui::DragFloat("Mass", &rb->mass, 0.1f, 0.01f, 1000.0f);
                ImGui::Checkbox("Use Gravity", &rb->useGravity);
                ImGui::DragFloat("Restitution", &rb->restitution, 0.01f, 0.0f, 1.0f);
                ImGui::Text("Velocity: %.2f, %.2f, %.2f",
                    rb->velocity.x, rb->velocity.y, rb->velocity.z);
            }
        }
    }

    // ── Scripts & Behaviors ─────────────────────────────────────────────────
    DrawInspectorScripts();

    // Components list (other components)
    if (ImGui::CollapsingHeader("Components", ImGuiTreeNodeFlags_DefaultOpen)) {
        for (auto& comp : m_Selected->GetComponents()) {
            // Skip types that already have dedicated sections
            if (comp->GetTypeName() == "Material" ||
                comp->GetTypeName() == "RigidBody" ||
                comp->GetTypeName() == "Collider" ||
                comp->GetTypeName() == "NativeScript")
                continue;

            ImGui::BulletText("%s", comp->GetTypeName().c_str());

            // MeshRenderer specific
            if (comp->GetTypeName() == "MeshRenderer") {
                auto* mr = dynamic_cast<MeshRenderer*>(comp.get());
                if (mr) {
                    const char* types[] = { "None", "Triangle", "Cube", "Plane" };
                    int idx = static_cast<int>(mr->primitiveType);
                    if (ImGui::Combo("Primitive", &idx, types, 4)) {
                        mr->primitiveType = static_cast<PrimitiveType>(idx);
                    }
                    float col[4] = { mr->color.x, mr->color.y, mr->color.z, mr->color.w };
                    if (ImGui::ColorEdit4("MR Color", col)) {
                        mr->color = Vec4(col[0], col[1], col[2], col[3]);
                    }
                }
            }

            // Lighting
            if (comp->GetTypeName() == "DirectionalLight") {
                auto* dl = dynamic_cast<DirectionalLight*>(comp.get());
                if (dl) {
                    float dir[3] = { dl->direction.x, dl->direction.y, dl->direction.z };
                    if (ImGui::DragFloat3("Direction", dir, 0.02f)) {
                        dl->direction = Vec3(dir[0], dir[1], dir[2]);
                    }
                    float col[3] = { dl->colour.x, dl->colour.y, dl->colour.z };
                    if (ImGui::ColorEdit3("Light Color", col)) {
                        dl->colour = Vec3(col[0], col[1], col[2]);
                    }
                    ImGui::DragFloat("Intensity", &dl->intensity, 0.01f, 0.0f, 10.0f);
                }
            }
            if (comp->GetTypeName() == "AmbientLight") {
                auto* al = dynamic_cast<AmbientLight*>(comp.get());
                if (al) {
                    float col[3] = { al->colour.x, al->colour.y, al->colour.z };
                    if (ImGui::ColorEdit3("Ambient Color", col)) {
                        al->colour = Vec3(col[0], col[1], col[2]);
                    }
                    ImGui::DragFloat("Intensity", &al->intensity, 0.01f, 0.0f, 5.0f);
                }
            }
            if (comp->GetTypeName() == "PointLight") {
                auto* pl = dynamic_cast<PointLight*>(comp.get());
                if (pl) {
                    float col[3] = { pl->colour.x, pl->colour.y, pl->colour.z };
                    if (ImGui::ColorEdit3("Light Color", col)) {
                        pl->colour = Vec3(col[0], col[1], col[2]);
                    }
                    ImGui::DragFloat("Intensity", &pl->intensity, 0.01f, 0.0f, 10.0f);
                    ImGui::DragFloat("Range", &pl->range, 0.5f, 0.1f, 200.0f);
                }
            }

            // Terrain specific
            if (comp->GetTypeName() == "Terrain") {
                auto* tc = dynamic_cast<TerrainComponent*>(comp.get());
                if (tc) {
                    ImGui::Text("Resolution: %u", tc->GetResolution());
                    ImGui::Text("World Size: %.1f", tc->GetWorldSize());
                    ImGui::Text("Max Height: %.1f", tc->GetMaxHeight());
                    ImGui::Text("Has Mesh: %s", tc->HasMesh() ? "Yes" : "No");
                    if (ImGui::Button("Edit Terrain")) { m_BottomTab = 1; }
                }
            }

            // ParticleEmitter specific
            if (comp->GetTypeName() == "ParticleEmitter") {
                auto* pe = dynamic_cast<ParticleEmitter*>(comp.get());
                if (pe) {
                    ImGui::Text("Alive: %u", pe->GetAliveCount());
                    ImGui::DragFloat("Emission Rate", &pe->emissionRate, 1, 0, 1000);
                    float cs[4] = { pe->colorStart.x, pe->colorStart.y, pe->colorStart.z, pe->colorStart.w };
                    if (ImGui::ColorEdit4("Start Color", cs)) pe->colorStart = {cs[0],cs[1],cs[2],cs[3]};
                    float ce[4] = { pe->colorEnd.x, pe->colorEnd.y, pe->colorEnd.z, pe->colorEnd.w };
                    if (ImGui::ColorEdit4("End Color", ce)) pe->colorEnd = {ce[0],ce[1],ce[2],ce[3]};
                    if (ImGui::Button("Edit Particles")) { m_BottomTab = 3; }
                }
            }

            // Animator specific
            if (comp->GetTypeName() == "Animator") {
                auto* anim = dynamic_cast<Animator*>(comp.get());
                if (anim) {
                    ImGui::Text("Playing: %s", anim->IsPlaying() ? "Yes" : "No");
                    ImGui::Text("Clip: %s", anim->GetCurrentClipName().c_str());
                    ImGui::Text("Time: %.2f", anim->GetTime());
                    if (ImGui::Button("Edit Animation")) { m_BottomTab = 4; }
                }
            }

            // NodeGraph specific
            if (comp->GetTypeName() == "NodeGraph") {
                auto* ng = dynamic_cast<NodeGraphComponent*>(comp.get());
                if (ng) {
                    ImGui::Text("Nodes: %d", static_cast<int>(ng->GetGraph().GetNodes().size()));
                    ImGui::Text("Connections: %d", static_cast<int>(ng->GetGraph().GetConnections().size()));
                    if (ImGui::Button("Edit Script")) { m_BottomTab = 5; }
                }
            }

            ImGui::Separator();
        }
    }

    // ── Add Component Button (Godot-style) ─────────────────────────────────
    DrawInspectorAddComponent();

    ImGui::End();
}

// ── Inspector: Material Section ────────────────────────────────────────────

void EditorUI::DrawInspectorMaterial() {
    auto* mat = m_Selected->GetComponent<MaterialComponent>();
    if (ImGui::CollapsingHeader("Material", ImGuiTreeNodeFlags_DefaultOpen)) {
        if (!mat) {
            ImGui::TextDisabled("No material attached.");
            if (ImGui::Button("+ Add Material Component")) {
                mat = m_Selected->AddComponent<MaterialComponent>();
                // Copy colour from MeshRenderer if present
                auto* mr = m_Selected->GetComponent<MeshRenderer>();
                if (mr) mat->albedo = mr->color;
                PushLog("[Inspector] Added Material to " + m_Selected->GetName());
            }
        }
        if (mat) {
            // Color picker (RGBA)
            float col[4] = { mat->albedo.x, mat->albedo.y, mat->albedo.z, mat->albedo.w };
            if (ImGui::ColorEdit4("Albedo##Mat", col, ImGuiColorEditFlags_AlphaBar)) {
                mat->albedo = Vec4(col[0], col[1], col[2], col[3]);
                mat->ApplyToMeshRenderer();   // live viewport update
            }

            // Metallic slider
            if (ImGui::SliderFloat("Metallic##Mat", &mat->metallic, 0.0f, 1.0f)) {
                // In full PBR pipeline, would update shader uniforms
            }

            // Roughness slider
            if (ImGui::SliderFloat("Roughness##Mat", &mat->roughness, 0.0f, 1.0f)) {
            }

            // Emission
            float em[3] = { mat->emission.x, mat->emission.y, mat->emission.z };
            if (ImGui::ColorEdit3("Emission##Mat", em)) {
                mat->emission = Vec3(em[0], em[1], em[2]);
            }
            ImGui::DragFloat("Emission Str##Mat", &mat->emissionStrength, 0.01f, 0.0f, 10.0f);
            ImGui::SliderFloat("AO##Mat", &mat->ao, 0.0f, 1.0f);

            // Material name
            char mnBuf[128];
            std::snprintf(mnBuf, sizeof(mnBuf), "%s", mat->GetMaterialName().c_str());
            if (ImGui::InputText("Mat Name", mnBuf, sizeof(mnBuf))) {
                mat->SetMaterialName(mnBuf);
            }

            // Preview sphere (inline in inspector)
            ImGui::Separator();
            ImVec2 cur = ImGui::GetCursorScreenPos();
            ImDrawList* draw = ImGui::GetWindowDrawList();
            ImVec2 center(cur.x + 30, cur.y + 30);
            ImU32 baseCol = ImGui::ColorConvertFloat4ToU32(
                ImVec4(mat->albedo.x, mat->albedo.y, mat->albedo.z, 1.0f));
            ImU32 darkCol = ImGui::ColorConvertFloat4ToU32(
                ImVec4(mat->albedo.x * 0.3f, mat->albedo.y * 0.3f, mat->albedo.z * 0.3f, 1.0f));
            draw->AddCircleFilled(center, 28, darkCol, 32);
            draw->AddCircleFilled(ImVec2(center.x - 5, center.y - 5), 20, baseCol, 32);
            f32 specSz = 8.0f * (1.0f - mat->roughness);
            if (specSz > 1)
                draw->AddCircleFilled(ImVec2(center.x - 10, center.y - 10), specSz,
                                      IM_COL32(255, 255, 255, 180), 16);
            ImGui::Dummy(ImVec2(60, 60));
            ImGui::SameLine();
            ImGui::BeginGroup();
            ImGui::TextDisabled("R: %.2f", mat->roughness);
            ImGui::TextDisabled("M: %.2f", mat->metallic);
            ImGui::EndGroup();
        }
    }
}

// ── Inspector: Scripts & Behaviors Section ─────────────────────────────────

void EditorUI::DrawInspectorScripts() {
    if (ImGui::CollapsingHeader("Scripts & Behaviors", ImGuiTreeNodeFlags_DefaultOpen)) {
        bool hasAny = false;

        // List all NativeScript behaviors
        for (auto& comp : m_Selected->GetComponents()) {
            auto* ns = dynamic_cast<NativeScriptComponent*>(comp.get());
            if (!ns) continue;
            hasAny = true;

            ImGui::PushID(ns);
            bool enabled = ns->IsEnabled();
            if (ImGui::Checkbox("##ScriptEnabled", &enabled)) {
                ns->SetEnabled(enabled);
            }
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(0.4f, 0.9f, 0.4f, 1), "%s", ns->GetScriptName().c_str());

            // Type-specific properties
            if (auto* rot = dynamic_cast<RotatorBehavior*>(ns)) {
                ImGui::DragFloat("Speed##Rot", &rot->speed, 1.0f, -1000.0f, 1000.0f);
                float ax[3] = { rot->axis.x, rot->axis.y, rot->axis.z };
                if (ImGui::DragFloat3("Axis##Rot", ax, 0.1f)) {
                    rot->axis = Vec3(ax[0], ax[1], ax[2]);
                }
            }
            if (auto* bob = dynamic_cast<BobBehavior*>(ns)) {
                ImGui::DragFloat("Amplitude##Bob", &bob->amplitude, 0.05f, 0.0f, 10.0f);
                ImGui::DragFloat("Frequency##Bob", &bob->frequency, 0.1f, 0.0f, 20.0f);
            }
            if (auto* ad = dynamic_cast<AutoDestroyBehavior*>(ns)) {
                ImGui::DragFloat("Lifetime##AD", &ad->lifetime, 0.1f, 0.1f, 60.0f);
            }

            ImGui::PopID();
            ImGui::Separator();
        }

        // List ScriptComponents (GVScript)
        for (auto& comp : m_Selected->GetComponents()) {
            if (comp->GetTypeName() == "ScriptComponent") {
                hasAny = true;
                auto* sc = dynamic_cast<ScriptComponent*>(comp.get());
                if (sc) {
                    ImGui::BulletText("Script: %s",
                        sc->GetScriptPath().empty() ? "(inline)" : sc->GetScriptPath().c_str());
                    ImGui::SameLine();
                    if (ImGui::SmallButton("Edit Code")) { m_BottomTab = 7; }
                }
            }
        }

        if (!hasAny) {
            ImGui::TextDisabled("No scripts attached.");
        }

        // Quick-add Script button (prominently visible)
        ImGui::Spacing();
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.15f, 0.45f, 0.65f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.25f, 0.55f, 0.75f, 1.0f));
        if (ImGui::Button("+ Add Code Script", ImVec2(-1, 24))) {
            auto* sc = m_Selected->AddComponent<ScriptComponent>();
            if (m_Script) sc->SetEngine(m_Script);
            PushLog("[Inspector] Added ScriptComponent to " + m_Selected->GetName());
            m_BottomTab = 7;  // switch to the Code Script editor tab
        }
        ImGui::PopStyleColor(2);

        // Behavior dropdown: add new behavior
        ImGui::Separator();
        auto names = BehaviorRegistry::Instance().GetNames();
        if (!names.empty()) {
            if (m_AddBehaviorIdx >= static_cast<i32>(names.size()))
                m_AddBehaviorIdx = 0;

            // Build combo items
            std::string combo;
            for (auto& n : names) { combo += n; combo += '\0'; }
            combo += '\0';
            ImGui::Combo("##BehaviorList", &m_AddBehaviorIdx, combo.c_str());
            ImGui::SameLine();
            if (ImGui::Button("+ Attach")) {
                auto* behavior = BehaviorRegistry::Instance().Create(names[m_AddBehaviorIdx]);
                if (behavior) {
                    // Transfer ownership: AddComponent takes Unique<T>, but we can set owner manually
                    behavior->SetOwner(m_Selected);
                    behavior->OnAttach();
                    // We need to add it via AddComponent but we already have a raw pointer.
                    // Use a small trick: add via the generic component vector.
                    // Actually, the cleanest way is to use AddComponent with the concrete type.
                    // Since we have a factory, we'll attach via the direct approach below.
                    // For now, use AddComponent with the known types:
                    const std::string& bname = names[m_AddBehaviorIdx];
                    delete behavior; // discard factory product
                    if (bname == "Rotator")      m_Selected->AddComponent<RotatorBehavior>();
                    else if (bname == "Bob")      m_Selected->AddComponent<BobBehavior>();
                    else if (bname == "Follow")   m_Selected->AddComponent<FollowBehavior>();
                    else if (bname == "AutoDestroy") m_Selected->AddComponent<AutoDestroyBehavior>();
                    PushLog("[Inspector] Attached behavior: " + bname + " to " + m_Selected->GetName());
                }
            }
        }
    }
}

// ── Inspector: Add Component Button ────────────────────────────────────────

void EditorUI::DrawInspectorAddComponent() {
    ImGui::Separator();
    ImGui::Spacing();

    const char* componentNames[] = { "Rigidbody", "Point Light", "Directional Light",
                                     "Material", "MeshRenderer", "ScriptComponent",
                                     "ParticleEmitter", "Animator", "NodeGraph" };
    const int componentCount = 9;

    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.5f, 0.2f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.7f, 0.3f, 1.0f));

    // Center the button
    float avail = ImGui::GetContentRegionAvail().x;
    float btnW = avail * 0.8f;
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (avail - btnW) * 0.5f);

    if (ImGui::Button("Add Component", ImVec2(btnW, 28))) {
        ImGui::OpenPopup("AddComponentPopup");
    }
    ImGui::PopStyleColor(2);

    if (ImGui::BeginPopup("AddComponentPopup")) {
        ImGui::TextDisabled("Select component to add:");
        ImGui::Separator();

        if (ImGui::MenuItem("Rigidbody")) {
            if (!m_Selected->GetComponent<RigidBody>()) {
                auto* rb = m_Selected->AddComponent<RigidBody>();
                rb->useGravity = true;
                m_Selected->AddComponent<Collider>()->type = ColliderType::Box;
                if (m_Physics) m_Physics->RegisterBody(rb);
                PushLog("[Inspector] Added Rigidbody.");
            }
        }
        if (ImGui::MenuItem("Point Light")) {
            m_Selected->AddComponent<PointLight>();
            PushLog("[Inspector] Added PointLight.");
        }
        if (ImGui::MenuItem("Directional Light")) {
            m_Selected->AddComponent<DirectionalLight>();
            PushLog("[Inspector] Added DirectionalLight.");
        }
        if (ImGui::MenuItem("Material")) {
            if (!m_Selected->GetComponent<MaterialComponent>()) {
                auto* mat = m_Selected->AddComponent<MaterialComponent>();
                auto* mr = m_Selected->GetComponent<MeshRenderer>();
                if (mr) mat->albedo = mr->color;
                PushLog("[Inspector] Added Material.");
            }
        }
        if (ImGui::MenuItem("MeshRenderer")) {
            if (!m_Selected->GetComponent<MeshRenderer>()) {
                auto* mr = m_Selected->AddComponent<MeshRenderer>();
                mr->primitiveType = PrimitiveType::Cube;
                PushLog("[Inspector] Added MeshRenderer.");
            }
        }
        if (ImGui::MenuItem("Code Script")) {
            auto* sc2 = m_Selected->AddComponent<ScriptComponent>();
            if (m_Script) sc2->SetEngine(m_Script);
            PushLog("[Inspector] Added ScriptComponent.");
        }

        ImGui::Separator();
        ImGui::TextDisabled("C++ Behaviors:");
        auto bnames = BehaviorRegistry::Instance().GetNames();
        for (auto& bn : bnames) {
            if (ImGui::MenuItem(bn.c_str())) {
                if (bn == "Rotator")      m_Selected->AddComponent<RotatorBehavior>();
                else if (bn == "Bob")      m_Selected->AddComponent<BobBehavior>();
                else if (bn == "Follow")   m_Selected->AddComponent<FollowBehavior>();
                else if (bn == "AutoDestroy") m_Selected->AddComponent<AutoDestroyBehavior>();
                PushLog("[Inspector] Added behavior: " + bn);
            }
        }

        ImGui::EndPopup();
    }
}

// ── Console Panel (kept as fallback for direct calls) ──────────────────────

void EditorUI::DrawConsole() {
    // Now handled inline by DrawBottomTabs()
}

// ── Viewport Panel ─────────────────────────────────────────────────────────

void EditorUI::DrawViewport(f32 dt) {
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::Begin("Viewport");
    ImGui::PopStyleVar();

    ImVec2 size = ImGui::GetContentRegionAvail();
    u32 w = static_cast<u32>(size.x > 1.0f ? size.x : 1.0f);
    u32 h = static_cast<u32>(size.y > 1.0f ? size.y : 1.0f);

    if (w != m_ViewportW || h != m_ViewportH)
        ResizeViewport(w, h);

    // Save viewport position for picking
    ImVec2 vpPos = ImGui::GetCursorScreenPos();
    m_VpScreenX = vpPos.x;
    m_VpScreenY = vpPos.y;
    m_VpScreenW = size.x;
    m_VpScreenH = size.y;

    // Render scene into FBO
    glBindFramebuffer(GL_FRAMEBUFFER, m_ViewportFBO);
    glViewport(0, 0, static_cast<GLsizei>(m_ViewportW), static_cast<GLsizei>(m_ViewportH));
    glClearColor(0.10f, 0.10f, 0.14f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);

    Camera* cam = m_Scene ? m_Scene->GetActiveCamera() : nullptr;
    if (cam && m_Renderer) {
        cam->SetPerspective(60.0f,
            static_cast<f32>(m_ViewportW) / static_cast<f32>(m_ViewportH),
            0.1f, 1000.0f);

        // Wireframe mode toggle
        if (m_ShowWireframe) glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

        // 1. Skybox (drawn first, behind everything)
        m_Renderer->RenderSkybox(*cam, dt);

        // 2. Grid (toggleable)
        if (m_ShowGrid) m_Renderer->RenderGrid(*cam);

        // 3. Scene objects
        m_Renderer->RenderScene(*m_Scene, *cam);

        // Revert wireframe
        if (m_ShowWireframe) glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

        // 4. Selection highlight (supports multi-select)
        auto drawHighlight = [&](GameObject* obj) {
            if (obj && obj->GetComponent<MeshRenderer>()) {
                auto* mr = obj->GetComponent<MeshRenderer>();
                Mat4 model = obj->GetTransform().GetModelMatrix();
                m_Renderer->RenderHighlight(*cam, model, mr->primitiveType);
            }
        };
        drawHighlight(m_Selected);
        for (auto* obj : m_MultiSelected) {
            if (obj != m_Selected) drawHighlight(obj);
        }

        // 5. Bounding boxes overlay
        if (m_ShowBoundingBoxes && m_Scene) {
            for (auto& obj : m_Scene->GetAllObjects()) {
                if (!obj || !obj->IsActive()) continue;
                auto* mr = obj->GetComponent<MeshRenderer>();
                if (!mr || mr->primitiveType == PrimitiveType::None) continue;
                Transform& t = obj->GetTransform();
                Vec3 half(0.5f * t.scale.x, 0.5f * t.scale.y, 0.5f * t.scale.z);
                m_Renderer->DrawDebugBox(t.position, half, Vec4(0.0f, 1.0f, 0.0f, 0.5f));
            }
        }

        // 6. Collision shapes overlay
        if (m_ShowCollisionShapes && m_Scene) {
            for (auto& obj : m_Scene->GetAllObjects()) {
                if (!obj || !obj->IsActive()) continue;
                auto* col = obj->GetComponent<Collider>();
                if (!col) continue;
                Transform& t = obj->GetTransform();
                if (col->type == ColliderType::Box) {
                    Vec3 half = col->boxHalfExtents * 1.01f; // slightly larger to show outside
                    if (half.x < 0.01f) half = Vec3(0.5f * t.scale.x, 0.5f * t.scale.y, 0.5f * t.scale.z);
                    m_Renderer->DrawDebugBox(t.position, half, Vec4(0.0f, 0.8f, 1.0f, 0.4f));
                } else if (col->type == ColliderType::Sphere) {
                    f32 r = col->sphereRadius > 0.01f ? col->sphereRadius : t.scale.x * 0.5f;
                    m_Renderer->DrawDebugSphere(t.position, r, Vec4(0.0f, 0.8f, 1.0f, 0.4f));
                }
            }
        }

        // 7. Normals visualization
        if (m_ShowNormals && m_Scene) {
            for (auto& obj : m_Scene->GetAllObjects()) {
                if (!obj || !obj->IsActive()) continue;
                auto* mr = obj->GetComponent<MeshRenderer>();
                if (!mr) continue;
                Transform& t = obj->GetTransform();
                Vec3 p = t.position;
                f32 len = 0.3f;
                m_Renderer->DrawDebugLine(p, p + Vec3(0, len, 0), Vec4(0, 0, 1, 1));
            }
        }

        // 8. Gizmo (toggleable)
        if (m_ShowGizmos && m_Selected) {
            Vec3 gizPos = m_Selected->GetTransform().position;
            m_Renderer->RenderGizmo(*cam, gizPos, m_GizmoMode, m_DragAxis);
        }
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // Display the FBO colour attachment as an ImGui Image
    ImTextureID texID = static_cast<ImTextureID>(m_ViewportColor);
    ImGui::Image(texID, size, ImVec2(0, 1), ImVec2(1, 0));   // flip Y

    // ── Viewport overlay controls (top-left icons) ─────────────────────────
    DrawViewportOverlayControls(m_VpScreenX, m_VpScreenY);

    // ── Gizmo orientation cube (top-right corner) ──────────────────────────
    DrawGizmoCube(m_VpScreenX, m_VpScreenY, m_VpScreenW, m_VpScreenH, cam);

    // ── Camera preview PiP (bottom-right corner) ───────────────────────────
    if (m_ShowCameraPiP)
        DrawCameraPreviewPiP(m_VpScreenX, m_VpScreenY, m_VpScreenW, m_VpScreenH);

    // ── Orbit / Pan / Zoom camera controls ─────────────────────────────────
    bool vpHovered = ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);
    ImVec2 mousePos = ImGui::GetIO().MousePos;

    if (cam) {
        bool mmb = ImGui::IsMouseDown(ImGuiMouseButton_Middle);
        bool rmb = ImGui::IsMouseDown(ImGuiMouseButton_Right);
        bool shiftHeld = ImGui::GetIO().KeyShift;
        bool altHeld   = ImGui::GetIO().KeyAlt;

        // Scroll → zoom (only when hovered)
        if (vpHovered) {
            f32 scroll = ImGui::GetIO().MouseWheel;
            if (scroll != 0.0f) {
                m_OrbitCam.Zoom(scroll);
                m_OrbitCam.ApplyToTransform(cam->GetOwner()->GetTransform());
            }
        }

        // ── Start drag: only when viewport is hovered ──────────────────
        if (vpHovered && !m_OrbitActive && !m_PanActive) {
            if (ImGui::IsMouseClicked(ImGuiMouseButton_Middle) ||
                ImGui::IsMouseClicked(ImGuiMouseButton_Right)  ||
                (altHeld && ImGui::IsMouseClicked(ImGuiMouseButton_Left))) {
                m_LastMousePos = Vec2(mousePos.x, mousePos.y);
                if (shiftHeld) {
                    m_PanActive = true;
                } else {
                    m_OrbitActive = true;
                }
            }
        }

        // ── Continue drag: works even when mouse leaves viewport ───────
        if (m_OrbitActive) {
            if (mmb || rmb || (altHeld && ImGui::IsMouseDown(ImGuiMouseButton_Left))) {
                f32 dx = mousePos.x - m_LastMousePos.x;
                f32 dy = mousePos.y - m_LastMousePos.y;
                m_LastMousePos = Vec2(mousePos.x, mousePos.y);
                if (shiftHeld) {
                    m_OrbitCam.Pan(dx, dy);  // shift mid-drag switches to pan
                } else {
                    m_OrbitCam.Orbit(dx, dy);
                }
                m_OrbitCam.ApplyToTransform(cam->GetOwner()->GetTransform());
            } else {
                m_OrbitActive = false;  // button released
            }
        }

        if (m_PanActive) {
            if (mmb || rmb) {
                f32 dx = mousePos.x - m_LastMousePos.x;
                f32 dy = mousePos.y - m_LastMousePos.y;
                m_LastMousePos = Vec2(mousePos.x, mousePos.y);
                m_OrbitCam.Pan(dx, dy);
                m_OrbitCam.ApplyToTransform(cam->GetOwner()->GetTransform());
            } else {
                m_PanActive = false;  // button released
            }
        }
    }

    // ── Focus on selected object (F key) ───────────────────────────────────
    if (vpHovered && ImGui::IsKeyPressed(ImGuiKey_F) && m_Selected && cam) {
        m_OrbitCam.FocusOn(m_Selected->GetTransform().position);
        m_OrbitCam.ApplyToTransform(cam->GetOwner()->GetTransform());
    }

    // ── Placement mode: preview + click-to-place ─────────────────────────
    if (m_PlacementType != PlacementType::None && cam) {
        // Update preview each frame
        UpdatePlacement(cam, mousePos.x, mousePos.y);

        // Draw placement hint in overlay
        if (m_PlacementValid) {
            const char* typeNames[] = { "", "Cube", "Light", "Terrain", "Particles", "Floor" };
            std::string hint = std::string("Placing ") + typeNames[static_cast<int>(m_PlacementType)] +
                " - Click to place, Right-click/Esc to cancel";
            ImVec2 txtSz = ImGui::CalcTextSize(hint.c_str());
            ImGui::GetForegroundDrawList()->AddRectFilled(
                ImVec2(m_VpScreenX + 8, m_VpScreenY + 8),
                ImVec2(m_VpScreenX + txtSz.x + 20, m_VpScreenY + txtSz.y + 16),
                IM_COL32(20, 20, 20, 200), 4.0f);
            ImGui::GetForegroundDrawList()->AddText(
                ImVec2(m_VpScreenX + 14, m_VpScreenY + 12),
                IM_COL32(100, 255, 100, 255), hint.c_str());

            // Draw crosshair at mouse position
            ImGui::GetForegroundDrawList()->AddCircle(
                mousePos, 8.0f, IM_COL32(100, 255, 100, 200), 12, 2.0f);
        }

        // Left-click: commit placement
        if (vpHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && m_PlacementValid
            && !m_OrbitActive && !m_PanActive && !ImGui::GetIO().KeyAlt) {
            FinishPlacement();
        }

        // Right-click or Escape: cancel placement
        if (ImGui::IsMouseClicked(ImGuiMouseButton_Right) || ImGui::IsKeyPressed(ImGuiKey_Escape)) {
            CancelPlacement();
        }
    }

    // ── Object Picking via ray-cast ────────────────────────────────────────
    // Only pick on Left-click when NOT orbiting/panning and NOT placing
    if (vpHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && cam
        && !m_Dragging && !m_OrbitActive && !m_PanActive && !ImGui::GetIO().KeyAlt
        && m_PlacementType == PlacementType::None) {
        // Convert mouse to viewport-local NDC
        f32 mx = (mousePos.x - m_VpScreenX) / m_VpScreenW;
        f32 my = (mousePos.y - m_VpScreenY) / m_VpScreenH;
        f32 ndcX = mx * 2.0f - 1.0f;
        f32 ndcY = 1.0f - my * 2.0f;  // flip Y

        // Unproject: screen → world ray
        Mat4 invVP = (cam->GetProjectionMatrix() * cam->GetViewMatrix()).Inverse();
        Vec3 nearPt = invVP.TransformPoint(Vec3(ndcX, ndcY, -1.0f));
        Vec3 farPt  = invVP.TransformPoint(Vec3(ndcX, ndcY,  1.0f));
        Vec3 rayDir = (farPt - nearPt).Normalized();
        Vec3 rayOrigin = nearPt;

        // Test all scene objects (not just those with RigidBody)
        GameObject* bestHit = nullptr;
        f32 bestT = FLT_MAX;

        if (m_Scene) {
            for (auto& obj : m_Scene->GetAllObjects()) {
                if (!obj || !obj->IsActive()) continue;
                auto* mr = obj->GetComponent<MeshRenderer>();
                if (!mr || mr->primitiveType == PrimitiveType::None) continue;

                Transform& t = obj->GetTransform();
                Vec3 half(0.5f * t.scale.x, 0.5f * t.scale.y, 0.5f * t.scale.z);
                // For planes, half-Y is very thin
                if (mr->primitiveType == PrimitiveType::Plane)
                    half = Vec3(0.5f * t.scale.x, 0.05f, 0.5f * t.scale.z);
                Vec3 bmin = t.position - half;
                Vec3 bmax = t.position + half;

                f32 tHit = 0;
                if (RayAABBIntersect(rayOrigin, rayDir, bmin, bmax, tHit)) {
                    if (tHit < bestT) {
                        bestT = tHit;
                        bestHit = obj.get();
                    }
                }
            }
        }

        if (bestHit) {
            m_Selected = bestHit;
            PushLog("[Viewport] Selected '" + bestHit->GetName() + "'");
        } else {
            m_Selected = nullptr;
        }
    }

    // ── Gizmo Drag Interaction ─────────────────────────────────────────────
    if (vpHovered && m_Selected && cam) {
        // Start drag: check if mouse is near a gizmo axis
        if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !m_Dragging) {
            f32 mx = (mousePos.x - m_VpScreenX) / m_VpScreenW;
            f32 my = (mousePos.y - m_VpScreenY) / m_VpScreenH;
            f32 ndcX = mx * 2.0f - 1.0f;
            f32 ndcY = 1.0f - my * 2.0f;

            Mat4 vp = cam->GetProjectionMatrix() * cam->GetViewMatrix();
            Vec3 gizPos = m_Selected->GetTransform().position;

            // Project gizmo axis tips to screen and check proximity
            auto project = [&](Vec3 worldPt) -> Vec2 {
                Vec3 ndc = vp.TransformPoint(worldPt);
                return { (ndc.x * 0.5f + 0.5f) * m_VpScreenW + m_VpScreenX,
                         (1.0f - (ndc.y * 0.5f + 0.5f)) * m_VpScreenH + m_VpScreenY };
            };

            Vec3 tips[3] = {
                gizPos + Vec3(1.5f, 0, 0),
                gizPos + Vec3(0, 1.5f, 0),
                gizPos + Vec3(0, 0, 1.5f)
            };

            Vec2 center2D = project(gizPos);
            f32 threshold = 30.0f; // pixels

            for (int a = 0; a < 3; ++a) {
                Vec2 tip2D = project(tips[a]);
                // Distance from mouse to line segment (center → tip)
                Vec2 mp(mousePos.x, mousePos.y);
                Vec2 d = tip2D - center2D;
                Vec2 p = mp - center2D;
                f32 lenSq = d.x * d.x + d.y * d.y;
                f32 t = (lenSq > 0.01f) ? (p.x * d.x + p.y * d.y) / lenSq : 0.0f;
                t = (t < 0) ? 0 : (t > 1) ? 1 : t;
                Vec2 closest(center2D.x + d.x * t, center2D.y + d.y * t);
                f32 dist = std::sqrt((mp.x - closest.x)*(mp.x - closest.x) + (mp.y - closest.y)*(mp.y - closest.y));
                if (dist < threshold) {
                    m_Dragging = true;
                    m_DragAxis = a;
                    m_DragStart = Vec3(mousePos.x, mousePos.y, 0);
                    m_DragObjStart = m_Selected->GetTransform().position;
                    // Property undo: capture old values at drag start
                    m_PropertyUndoPending = true;
                    m_PropertyOldPos   = m_Selected->GetTransform().position;
                    m_PropertyOldScale = m_Selected->GetTransform().scale;
                    m_PropertyOldRot   = m_Selected->GetTransform().rotation;
                    m_PropertyObjID    = m_Selected->GetID();
                    // For Euler rotation extract
                    const Quaternion& q = m_Selected->GetTransform().rotation;
                    f32 sinp = 2.0f * (q.w * q.x + q.y * q.z);
                    f32 cosp = 1.0f - 2.0f * (q.x * q.x + q.y * q.y);
                    f32 siny = 2.0f * (q.w * q.y - q.z * q.x);
                    f32 sinr = 2.0f * (q.w * q.z + q.x * q.y);
                    f32 cosr = 1.0f - 2.0f * (q.y * q.y + q.z * q.z);
                    m_DragRotStart = Vec3(
                        std::atan2(sinp, cosp) * (180.0f / 3.14159265f),
                        std::fabs(siny) >= 1.0f ? std::copysign(90.0f, siny) : std::asin(siny) * (180.0f / 3.14159265f),
                        std::atan2(sinr, cosr) * (180.0f / 3.14159265f));
                    m_DragScaleStart = m_Selected->GetTransform().scale;
                    break;
                }
            }
        }

        // Continue drag
        if (m_Dragging && ImGui::IsMouseDown(ImGuiMouseButton_Left) && m_DragAxis >= 0) {
            f32 dx = mousePos.x - m_DragStart.x;
            f32 dy = -(mousePos.y - m_DragStart.y); // flip Y
            f32 delta = (m_DragAxis == 1) ? dy : dx; // Y-axis uses vertical mouse movement

            f32 sensitivity = 0.01f;
            Transform& t = m_Selected->GetTransform();

            if (m_GizmoMode == GizmoMode::Translate) {
                Vec3 pos = m_DragObjStart;
                f32 move = delta * sensitivity * 2.0f;
                if (m_DragAxis == 0) pos.x += move;
                if (m_DragAxis == 1) pos.y += move;
                if (m_DragAxis == 2) pos.z += move;

                if (m_SnapToGrid && m_GridSize > 0.01f) {
                    pos.x = std::round(pos.x / m_GridSize) * m_GridSize;
                    pos.y = std::round(pos.y / m_GridSize) * m_GridSize;
                    pos.z = std::round(pos.z / m_GridSize) * m_GridSize;
                }

                t.position = pos;
            } else if (m_GizmoMode == GizmoMode::Rotate) {
                Vec3 rot = m_DragRotStart;
                f32 angle = delta * sensitivity * 50.0f;
                if (m_DragAxis == 0) rot.x += angle;
                if (m_DragAxis == 1) rot.y += angle;
                if (m_DragAxis == 2) rot.z += angle;
                t.SetEulerDeg(rot.x, rot.y, rot.z);
            } else if (m_GizmoMode == GizmoMode::Scale) {
                Vec3 scl = m_DragScaleStart;
                f32 factor = 1.0f + delta * sensitivity;
                if (factor < 0.05f) factor = 0.05f;
                if (m_DragAxis == 0) scl.x = m_DragScaleStart.x * factor;
                if (m_DragAxis == 1) scl.y = m_DragScaleStart.y * factor;
                if (m_DragAxis == 2) scl.z = m_DragScaleStart.z * factor;
                t.scale = scl;
            }
        }

        // End drag
        if (m_Dragging && ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
            // Push property undo command
            if (m_PropertyUndoPending && m_Selected && m_Selected->GetID() == m_PropertyObjID) {
                Vec3 newPos   = m_Selected->GetTransform().position;
                Vec3 newScale = m_Selected->GetTransform().scale;
                Vec3 oldPos   = m_PropertyOldPos;
                Vec3 oldScale = m_PropertyOldScale;
                std::string objName = m_Selected->GetName();
                Vec3* posPtr   = &m_Selected->GetTransform().position;
                Vec3* scalePtr = &m_Selected->GetTransform().scale;
                m_UndoStack.Execute(std::make_unique<TransformCommand>(
                    posPtr, oldPos, newPos, scalePtr, oldScale, newScale, objName));
                m_PropertyUndoPending = false;
            }
            m_Dragging = false;
            m_DragAxis = -1;
        }
    }

    // Cancel drag if mouse released anywhere
    if (m_Dragging && !ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
        m_Dragging = false;
        m_DragAxis = -1;
    }

    ImGui::End();
}
// ── AI Generator Panel ───────────────────────────────────────────────────────

void EditorUI::DrawAIGenerator() {
    ImGui::Begin("AI Generator", &m_ShowAIPanel,
        ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoCollapse);

    // API key status indicator
    bool hasKey = (m_AI && m_AI->IsReady());
    if (hasKey) {
        ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.3f, 1), "[API Connected]");
    } else {
        ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1), "[No API Key]");
        ImGui::SameLine();
        if (ImGui::SmallButton("Settings")) { m_ShowEditorSettings = true; }
    }

    ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "Describe your scene:");
    ImGui::InputTextMultiline("##AIPrompt", m_AIPromptBuf, sizeof(m_AIPromptBuf),
        ImVec2(-1, 60));

    // Example prompts as clickable chips
    ImGui::TextDisabled("Examples:");
    ImGui::SameLine();
    if (ImGui::SmallButton("cyberpunk city")) {
        std::snprintf(m_AIPromptBuf, sizeof(m_AIPromptBuf), "create cyberpunk city with neon buildings");
    }
    ImGui::SameLine();
    if (ImGui::SmallButton("forest")) {
        std::snprintf(m_AIPromptBuf, sizeof(m_AIPromptBuf), "forest with river and rocks");
    }
    ImGui::SameLine();
    if (ImGui::SmallButton("shooter map")) {
        std::snprintf(m_AIPromptBuf, sizeof(m_AIPromptBuf), "make 3d shooter map with cover and platforms");
    }

    // Generate button
    bool hasPrompt = (m_AIPromptBuf[0] != '\0');
    if (!hasPrompt) ImGui::BeginDisabled();
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.6f, 0.3f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.8f, 0.4f, 1.0f));
    if (ImGui::Button(m_AIGenerating ? "Generating..." : "GENERATE", ImVec2(-1, 28))) {
        if (!m_AIGenerating) AIGenerate();
    }
    ImGui::PopStyleColor(2);
    if (!hasPrompt) ImGui::EndDisabled();

    // Progress bar
    if (m_AIGenerating || m_AIProgress > 0.0f) {
        ImGui::ProgressBar(m_AIProgress, ImVec2(-1, 14));
    }

    // Status message
    if (!m_AIStatusMsg.empty()) {
        bool isError = m_AIStatusMsg.find("Error") != std::string::npos ||
                       m_AIStatusMsg.find("fail") != std::string::npos;
        if (isError)
            ImGui::TextColored(ImVec4(1, 0.3f, 0.3f, 1), "%s", m_AIStatusMsg.c_str());
        else
            ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1), "%s", m_AIStatusMsg.c_str());
    }

    // Undo button
    if (!m_AILastSpawnedIDs.empty()) {
        ImGui::Separator();
        if (ImGui::Button("Undo Last Generation", ImVec2(-1, 22))) {
            AIUndoLastGeneration();
        }
        ImGui::TextDisabled("(%d objects)", static_cast<int>(m_AILastSpawnedIDs.size()));
    }

    ImGui::End();
}

// ── AI Generate ─────────────────────────────────────────────────────────────

void EditorUI::AIGenerate() {
    if (!m_AI || !m_Scene) {
        m_AIStatusMsg = "Error: AI or Scene not available.";
        PushLog("[AI] " + m_AIStatusMsg);
        return;
    }

    std::string prompt(m_AIPromptBuf);
    if (prompt.empty()) return;

    m_AIGenerating = true;
    m_AIProgress   = 0.1f;
    m_AIStatusMsg  = "Sending prompt to Gemini...";
    PushLog("[AI] Prompt: \"" + prompt + "\"");

    // Call AIManager (synchronous for now)
    m_AIProgress = 0.3f;
    auto result = m_AI->GenerateSceneFromPrompt(prompt);
    m_AIProgress = 0.6f;

    if (!result.success) {
        // If the real API isn't connected, generate a fallback procedural scene
        PushLog("[AI] API call failed: " + result.errorMessage);
        PushLog("[AI] Generating procedural fallback scene from keywords...");
        m_AIStatusMsg = "Using procedural fallback (no API key).";

        // Keyword-based procedural generation
        result.objects.clear();
        AIManager::ObjectBlueprint bp;

        // Always add a ground plane
        bp.name = "AI_Ground";
        bp.meshType = "cube";
        bp.position = Vec3(0, -0.05f, 0);
        bp.scale = Vec3(20, 0.1f, 20);
        bp.materialName = "0.3,0.3,0.35,1.0";
        bp.scriptSnippet = "";
        result.objects.push_back(bp);

        // Seed from prompt hash for variety
        u32 seed = 0;
        for (char c : prompt) seed = seed * 31 + static_cast<u32>(c);
        std::srand(seed);

        bool isCyber   = prompt.find("cyber") != std::string::npos || prompt.find("neon") != std::string::npos;
        bool isForest  = prompt.find("forest") != std::string::npos || prompt.find("tree") != std::string::npos;
        bool isShooter = prompt.find("shoot") != std::string::npos || prompt.find("arena") != std::string::npos;
        bool isCity    = prompt.find("city") != std::string::npos || prompt.find("building") != std::string::npos;

        int objCount = 10 + std::rand() % 12;
        for (int i = 0; i < objCount; i++) {
            AIManager::ObjectBlueprint ob;
            f32 x = static_cast<f32>(std::rand() % 200 - 100) / 10.0f;
            f32 z = static_cast<f32>(std::rand() % 200 - 100) / 10.0f;

            if (isCyber || isCity) {
                f32 h = 1.0f + static_cast<f32>(std::rand() % 60) / 10.0f;
                ob.name = "Building_" + std::to_string(i);
                ob.meshType = "cube";
                ob.position = Vec3(x, h * 0.5f, z);
                ob.scale = Vec3(1.0f + static_cast<f32>(std::rand() % 20) / 10.0f, h,
                                1.0f + static_cast<f32>(std::rand() % 20) / 10.0f);
                f32 r = isCyber ? 0.1f + static_cast<f32>(std::rand() % 30) / 100.0f : 0.5f;
                f32 g = isCyber ? 0.0f : 0.5f;
                f32 b = isCyber ? 0.5f + static_cast<f32>(std::rand() % 50) / 100.0f : 0.5f;
                ob.materialName = std::to_string(r) + "," + std::to_string(g) + "," + std::to_string(b) + ",1.0";
            } else if (isForest) {
                f32 h = 1.5f + static_cast<f32>(std::rand() % 30) / 10.0f;
                ob.name = "Tree_" + std::to_string(i);
                ob.meshType = "cube";
                ob.position = Vec3(x, h * 0.5f, z);
                ob.scale = Vec3(0.3f, h, 0.3f);
                ob.materialName = "0.2,0.45,0.15,1.0";
            } else if (isShooter) {
                ob.name = "Cover_" + std::to_string(i);
                ob.meshType = "cube";
                f32 h = 0.5f + static_cast<f32>(std::rand() % 20) / 10.0f;
                ob.position = Vec3(x, h * 0.5f, z);
                ob.scale = Vec3(1.0f + static_cast<f32>(std::rand() % 15) / 10.0f, h,
                                0.5f + static_cast<f32>(std::rand() % 10) / 10.0f);
                ob.materialName = "0.4,0.35,0.3,1.0";
                ob.scriptSnippet = "physics";
            } else {
                ob.name = "Object_" + std::to_string(i);
                ob.meshType = "cube";
                ob.position = Vec3(x, 0.5f + static_cast<f32>(std::rand() % 30) / 10.0f, z);
                ob.scale = Vec3(0.5f + static_cast<f32>(std::rand() % 15) / 10.0f,
                                0.5f + static_cast<f32>(std::rand() % 15) / 10.0f,
                                0.5f + static_cast<f32>(std::rand() % 15) / 10.0f);
                f32 r = static_cast<f32>(std::rand() % 100) / 100.0f;
                f32 g = static_cast<f32>(std::rand() % 100) / 100.0f;
                f32 b = static_cast<f32>(std::rand() % 100) / 100.0f;
                ob.materialName = std::to_string(r) + "," + std::to_string(g) + "," + std::to_string(b) + ",1.0";
            }
            ob.rotation = Vec3(0, static_cast<f32>(std::rand() % 360), 0);
            result.objects.push_back(ob);
        }
        result.success = true;
    }

    m_AIProgress = 0.8f;

    if (result.success && !result.objects.empty()) {
        PushLog("[AI] Received " + std::to_string(result.objects.size()) + " objects. Spawning...");
        // Store blueprints temporarily for spawning
        m_AILastSpawnedIDs.clear();
        AISpawnBlueprintsFrom(result.objects);
        m_AIStatusMsg = "Generated " + std::to_string(result.objects.size()) + " objects!";
        m_AIProgress  = 1.0f;
    } else {
        m_AIStatusMsg = "Error: " + result.errorMessage;
        PushLog("[AI] Generation failed: " + result.errorMessage);
    }

    m_AIGenerating = false;
}

void EditorUI::AISpawnBlueprints() {
    // This is called from AIGenerate; see AISpawnBlueprintsFrom
}

void EditorUI::AISpawnBlueprintsFrom(const std::vector<AIManager::ObjectBlueprint>& blueprints) {
    if (!m_Scene) return;

    for (size_t i = 0; i < blueprints.size(); i++) {
        const auto& bp = blueprints[i];
        auto* obj = m_Scene->CreateGameObject(bp.name);
        obj->GetTransform().SetPosition(bp.position.x, bp.position.y, bp.position.z);
        obj->GetTransform().SetEulerDeg(bp.rotation.x, bp.rotation.y, bp.rotation.z);
        obj->GetTransform().SetScale(bp.scale.x, bp.scale.y, bp.scale.z);

        auto* mr = obj->AddComponent<MeshRenderer>();
        // Map meshType string to PrimitiveType
        if (bp.meshType == "triangle")
            mr->primitiveType = PrimitiveType::Triangle;
        else
            mr->primitiveType = PrimitiveType::Cube;

        // Parse colour from materialName "r,g,b,a"
        f32 r = 0.7f, g = 0.7f, b = 0.7f, a = 1.0f;
        if (!bp.materialName.empty()) {
            int parsed = std::sscanf(bp.materialName.c_str(), "%f,%f,%f,%f", &r, &g, &b, &a);
            if (parsed < 3) { r = 0.7f; g = 0.7f; b = 0.7f; a = 1.0f; }
        }
        mr->color = Vec4(r, g, b, a);

        // Physics if flagged
        if (bp.scriptSnippet == "physics") {
            auto* rb = obj->AddComponent<RigidBody>();
            rb->useGravity = true;
            obj->AddComponent<Collider>()->type = ColliderType::Box;
            if (m_Physics) m_Physics->RegisterBody(rb);
        }

        m_AILastSpawnedIDs.push_back(obj->GetID());
        PushLog("[AI] Spawned '" + bp.name + "' at (" +
            std::to_string(bp.position.x) + ", " +
            std::to_string(bp.position.y) + ", " +
            std::to_string(bp.position.z) + ")");
    }
}

void EditorUI::AIUndoLastGeneration() {
    if (!m_Scene || m_AILastSpawnedIDs.empty()) {
        PushLog("[AI] Nothing to undo.");
        return;
    }

    int count = 0;
    for (u32 id : m_AILastSpawnedIDs) {
        auto* obj = m_Scene->FindByID(id);
        if (obj) {
            m_Scene->DestroyGameObject(obj);
            count++;
        }
    }
    PushLog("[AI] Undid last generation (" + std::to_string(count) + " objects removed).");
    m_AILastSpawnedIDs.clear();
    m_AIStatusMsg = "Undone (" + std::to_string(count) + " removed).";
    m_AIProgress = 0.0f;
    if (m_Selected) {
        // Check if selected was among the deleted
        bool found = false;
        for (auto& o : m_Scene->GetAllObjects())
            if (o.get() == m_Selected) { found = true; break; }
        if (!found) m_Selected = nullptr;
    }
}

// ============================================================================
// Multi-Select & Clipboard Helpers
// ============================================================================

void EditorUI::SelectObject(GameObject* obj, bool additive) {
    if (!obj) return;
    if (additive) {
        // Toggle in/out of multi-selection
        if (m_MultiSelected.count(obj)) {
            m_MultiSelected.erase(obj);
            if (m_Selected == obj) {
                m_Selected = m_MultiSelected.empty() ? nullptr : *m_MultiSelected.begin();
            }
        } else {
            m_MultiSelected.insert(obj);
            m_Selected = obj;
        }
    } else {
        // Single-select (clear multi)
        m_MultiSelected.clear();
        m_MultiSelected.insert(obj);
        m_Selected = obj;
    }
}

void EditorUI::SelectAll() {
    if (!m_Scene) return;
    m_MultiSelected.clear();
    for (auto& obj : m_Scene->GetAllObjects()) {
        if (obj) m_MultiSelected.insert(obj.get());
    }
    if (!m_MultiSelected.empty())
        m_Selected = *m_MultiSelected.begin();
    PushLog("[Edit] Selected all (" + std::to_string(m_MultiSelected.size()) + " objects).");
}

void EditorUI::ClearSelection() {
    m_MultiSelected.clear();
    m_Selected = nullptr;
}

void EditorUI::CopySelected() {
    m_Clipboard.clear();
    auto copyObj = [&](GameObject* obj) {
        if (!obj) return;
        ClipboardEntry entry;
        entry.name     = obj->GetName();
        entry.position = obj->GetTransform().position;
        entry.scale    = obj->GetTransform().scale;
        entry.rotation = obj->GetTransform().rotation;
        auto* mr = obj->GetComponent<MeshRenderer>();
        if (mr) {
            entry.primitiveType = static_cast<int>(mr->primitiveType);
            entry.color = mr->color;
        }
        entry.hasRigidBody = (obj->GetComponent<RigidBody>() != nullptr);
        entry.hasCollider  = (obj->GetComponent<Collider>() != nullptr);
        m_Clipboard.push_back(entry);
    };

    if (m_MultiSelected.size() > 1) {
        for (auto* obj : m_MultiSelected) copyObj(obj);
    } else if (m_Selected) {
        copyObj(m_Selected);
    }
    if (!m_Clipboard.empty())
        PushLog("[Edit] Copied " + std::to_string(m_Clipboard.size()) + " object(s).");
}

void EditorUI::PasteClipboard() {
    if (m_Clipboard.empty() || !m_Scene) {
        PushLog("[Edit] Nothing to paste.");
        return;
    }
    m_MultiSelected.clear();
    for (auto& entry : m_Clipboard) {
        auto* obj = m_Scene->CreateGameObject(entry.name + "_copy");
        obj->GetTransform().position = entry.position + Vec3(1, 0, 1); // offset
        obj->GetTransform().scale    = entry.scale;
        obj->GetTransform().rotation = entry.rotation;
        if (entry.primitiveType > 0) {
            auto* mr = obj->AddComponent<MeshRenderer>();
            mr->primitiveType = static_cast<PrimitiveType>(entry.primitiveType);
            mr->color = entry.color;
        }
        if (entry.hasRigidBody) {
            auto* rb = obj->AddComponent<RigidBody>();
            rb->useGravity = true;
            if (entry.hasCollider)
                obj->AddComponent<Collider>()->type = ColliderType::Box;
            if (m_Physics) m_Physics->RegisterBody(rb);
        }
        m_MultiSelected.insert(obj);
        m_Selected = obj;
    }
    PushLog("[Edit] Pasted " + std::to_string(m_Clipboard.size()) + " object(s).");
}

void EditorUI::DuplicateSelected() {
    if (!m_Scene) return;
    // Build temporary clipboard and paste
    std::vector<ClipboardEntry> savedClip = m_Clipboard;
    CopySelected();
    PasteClipboard();
    m_Clipboard = savedClip; // restore original clipboard
}

// ============================================================================
// Asset Browser Panel
// ============================================================================

void EditorUI::DrawAssetBrowser() {
    if (m_AssetBrowserRoot.empty()) {
        // Default to project root (crude detection)
        m_AssetBrowserRoot = ".";
        m_AssetBrowserCurrentDir = m_AssetBrowserRoot;
    }

    ImGui::Columns(2, "AssetBrowserCols", true);
    ImGui::SetColumnWidth(0, 200);

    // Column 1: Directory tree
    ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1), "Folders");
    ImGui::Separator();

    auto drawDirTree = [&](auto& self, const std::string& path, int depth) -> void {
        if (depth > 5) return;
        try {
            for (auto& entry : std::filesystem::directory_iterator(path)) {
                if (!entry.is_directory()) continue;
                std::string dirName = entry.path().filename().string();
                // Skip hidden/build dirs
                if (dirName[0] == '.' || dirName == "build" || dirName == ".git") continue;

                ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow;
                if (entry.path().string() == m_AssetBrowserCurrentDir)
                    flags |= ImGuiTreeNodeFlags_Selected;

                bool open = ImGui::TreeNodeEx(dirName.c_str(), flags);
                if (ImGui::IsItemClicked()) {
                    m_AssetBrowserCurrentDir = entry.path().string();
                }
                if (open) {
                    self(self, entry.path().string(), depth + 1);
                    ImGui::TreePop();
                }
            }
        } catch (...) {}
    };

    if (ImGui::TreeNodeEx("Project", ImGuiTreeNodeFlags_DefaultOpen)) {
        if (ImGui::IsItemClicked()) m_AssetBrowserCurrentDir = m_AssetBrowserRoot;
        drawDirTree(drawDirTree, m_AssetBrowserRoot, 0);
        ImGui::TreePop();
    }

    ImGui::NextColumn();

    // Column 2: File list
    ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.4f, 1), "Files");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(200);
    ImGui::InputTextWithHint("##AssetSearch", "Filter...", m_AssetSearchBuf, sizeof(m_AssetSearchBuf));
    ImGui::Separator();

    std::string assetFilter(m_AssetSearchBuf);
    for (auto& c : assetFilter) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));

    try {
        for (auto& entry : std::filesystem::directory_iterator(m_AssetBrowserCurrentDir)) {
            std::string fname = entry.path().filename().string();

            // Apply filter
            if (!assetFilter.empty()) {
                std::string fnameLower = fname;
                for (auto& c : fnameLower) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
                if (fnameLower.find(assetFilter) == std::string::npos) continue;
            }

            // Icon based on extension
            const char* icon = entry.is_directory() ? "[D] " : "[F] ";
            std::string ext = entry.path().extension().string();
            if (ext == ".h" || ext == ".hpp" || ext == ".cpp" || ext == ".c")
                icon = "[C] ";
            else if (ext == ".png" || ext == ".jpg" || ext == ".bmp" || ext == ".tga")
                icon = "[T] "; // texture
            else if (ext == ".gvs")
                icon = "[S] "; // script
            else if (ext == ".obj" || ext == ".fbx" || ext == ".glb")
                icon = "[M] "; // mesh
            else if (ext == ".wav" || ext == ".mp3" || ext == ".ogg")
                icon = "[A] "; // audio
            else if (ext == ".ini" || ext == ".json" || ext == ".xml")
                icon = "[*] ";

            std::string label = std::string(icon) + fname;
            if (ImGui::Selectable(label.c_str())) {
                PushLog("[Assets] Selected: " + entry.path().string());
            }

            // Drag-and-drop source for asset files
            if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID)) {
                std::string pathStr = entry.path().string();
                ImGui::SetDragDropPayload("GV_ASSET_PATH", pathStr.c_str(), pathStr.size() + 1);
                ImGui::Text("%s", fname.c_str());
                ImGui::EndDragDropSource();
            }

            // Tooltip with file info
            if (ImGui::IsItemHovered()) {
                ImGui::BeginTooltip();
                ImGui::Text("%s", entry.path().string().c_str());
                if (entry.is_regular_file()) {
                    auto sz = entry.file_size();
                    if (sz > 1024 * 1024)
                        ImGui::Text("Size: %.1f MB", static_cast<float>(sz) / (1024.0f * 1024.0f));
                    else if (sz > 1024)
                        ImGui::Text("Size: %.1f KB", static_cast<float>(sz) / 1024.0f);
                    else
                        ImGui::Text("Size: %llu bytes", static_cast<unsigned long long>(sz));
                }
                ImGui::EndTooltip();
            }
        }
    } catch (...) {
        ImGui::TextDisabled("Cannot read directory.");
    }

    ImGui::Columns(1);
}

// ============================================================================
// Keyboard Shortcuts Panel
// ============================================================================

void EditorUI::DrawKeyboardShortcuts() {
    ImGui::SetNextWindowSize(ImVec2(420, 460), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Keyboard Shortcuts", &m_ShowKeyboardShortcuts)) {
        ImGui::End();
        return;
    }

    ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1), "Editor Shortcuts");
    ImGui::Separator();

    struct ShortcutEntry { const char* keys; const char* action; };
    static const ShortcutEntry shortcuts[] = {
        { "Ctrl+Z",         "Undo" },
        { "Ctrl+Y",         "Redo" },
        { "Ctrl+C",         "Copy selected object(s)" },
        { "Ctrl+V",         "Paste" },
        { "Ctrl+D",         "Duplicate selected" },
        { "Ctrl+A",         "Select all objects" },
        { "Delete",         "Delete selected" },
        { "",               "" },
        { "W",              "Translate gizmo mode" },
        { "E",              "Rotate gizmo mode" },
        { "R",              "Scale gizmo mode" },
        { "F",              "Focus on selected object" },
        { "Z",              "Toggle wireframe" },
        { "Escape",         "Cancel placement" },
        { "",               "" },
        { "Middle Mouse",   "Orbit camera" },
        { "Shift+MMB",      "Pan camera" },
        { "Scroll Wheel",   "Zoom camera" },
        { "Right Mouse",    "Orbit camera (alt)" },
        { "Alt+LMB",        "Orbit camera (Maya-style)" },
        { "",               "" },
        { "Ctrl+Click",     "Add/remove from multi-selection" },
        { "Shift+Click",    "Add to multi-selection" },
        { "Left Click",     "Select / Pick object in viewport" },
    };

    if (ImGui::BeginTable("ShortcutsTable", 2, ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders)) {
        ImGui::TableSetupColumn("Shortcut", ImGuiTableColumnFlags_WidthFixed, 150);
        ImGui::TableSetupColumn("Action");
        ImGui::TableHeadersRow();

        for (auto& s : shortcuts) {
            if (s.keys[0] == '\0') {
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::Separator();
                continue;
            }
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.3f, 1), "%s", s.keys);
            ImGui::TableSetColumnIndex(1);
            ImGui::Text("%s", s.action);
        }
        ImGui::EndTable();
    }

    ImGui::End();
}

// ============================================================================
// Viewport Overlay Controls (top-left of viewport)
// ============================================================================

void EditorUI::DrawViewportOverlayControls(f32 vpX, f32 vpY) {
    ImDrawList* dl = ImGui::GetForegroundDrawList();
    ImVec2 pos(vpX + 8, vpY + 8);
    f32 btnW = 22, btnH = 18, gap = 2;

    auto overlayBtn = [&](const char* label, bool& toggle, ImU32 onColor) {
        ImVec2 tl = pos;
        ImVec2 br(pos.x + btnW, pos.y + btnH);
        ImU32 col = toggle ? onColor : IM_COL32(60, 60, 65, 200);
        dl->AddRectFilled(tl, br, col, 3.0f);
        ImVec2 textSz = ImGui::CalcTextSize(label);
        dl->AddText(ImVec2(tl.x + (btnW - textSz.x) * 0.5f, tl.y + (btnH - textSz.y) * 0.5f),
                    IM_COL32(255, 255, 255, 220), label);
        // Check click
        ImVec2 mp = ImGui::GetIO().MousePos;
        if (mp.x >= tl.x && mp.x <= br.x && mp.y >= tl.y && mp.y <= br.y) {
            if (ImGui::IsMouseClicked(ImGuiMouseButton_Left))
                toggle = !toggle;
            dl->AddRect(tl, br, IM_COL32(255, 255, 255, 100), 3.0f);
        }
        pos.x += btnW + gap;
    };

    overlayBtn("W", m_ShowWireframe,       IM_COL32(80, 140, 200, 220));
    overlayBtn("B", m_ShowBoundingBoxes,   IM_COL32(80, 180, 80, 220));
    overlayBtn("C", m_ShowCollisionShapes, IM_COL32(200, 140, 40, 220));
    overlayBtn("N", m_ShowNormals,         IM_COL32(100, 100, 200, 220));
    overlayBtn("G", m_ShowGrid,            IM_COL32(120, 120, 120, 220));
    overlayBtn("P", m_ShowCameraPiP,       IM_COL32(160, 80, 160, 220));
}

// ============================================================================
// Gizmo Orientation Cube (top-right of viewport)
// ============================================================================

void EditorUI::DrawGizmoCube(f32 vpX, f32 vpY, f32 vpW, f32 vpH, Camera* cam) {
    if (!cam || !cam->GetOwner()) return;

    ImDrawList* dl = ImGui::GetForegroundDrawList();
    f32 cubeSize = 60.0f;
    ImVec2 center(vpX + vpW - cubeSize - 10, vpY + cubeSize + 10);

    // Get camera rotation to rotate cube faces
    Transform& ct = cam->GetOwner()->GetTransform();
    Vec3 camForward = ct.Forward();
    Vec3 camRight   = ct.Right();
    Vec3 camUp      = ct.Up();

    // Draw cube faces as projected directions
    struct FaceInfo { Vec3 dir; const char* label; ImU32 color; ViewAngle angle; };
    FaceInfo faces[] = {
        { Vec3( 1, 0, 0), "+X", IM_COL32(220, 60, 60, 200),  ViewAngle::Right },
        { Vec3(-1, 0, 0), "-X", IM_COL32(120, 30, 30, 200),  ViewAngle::Right },
        { Vec3(0,  1, 0), "+Y", IM_COL32(60, 200, 60, 200),  ViewAngle::Top },
        { Vec3(0, -1, 0), "-Y", IM_COL32(30, 100, 30, 200),  ViewAngle::Top },
        { Vec3(0, 0,  1), "+Z", IM_COL32(60, 60, 220, 200),  ViewAngle::Front },
        { Vec3(0, 0, -1), "-Z", IM_COL32(30, 30, 120, 200),  ViewAngle::Front },
    };

    // Background circle
    dl->AddCircleFilled(center, cubeSize * 0.5f + 4, IM_COL32(30, 30, 35, 180), 24);

    // Sort faces by depth (furthest first)
    struct ProjectedFace { ImVec2 pos; const char* label; ImU32 color; f32 depth; ViewAngle angle; };
    ProjectedFace projected[6];
    for (int i = 0; i < 6; i++) {
        Vec3 d = faces[i].dir;
        // Project onto camera plane
        f32 x = d.x * camRight.x + d.y * camRight.y + d.z * camRight.z;
        f32 y = -(d.x * camUp.x + d.y * camUp.y + d.z * camUp.z);
        f32 z = d.x * camForward.x + d.y * camForward.y + d.z * camForward.z;
        projected[i].pos = ImVec2(center.x + x * cubeSize * 0.3f, center.y + y * cubeSize * 0.3f);
        projected[i].label = faces[i].label;
        projected[i].color = faces[i].color;
        projected[i].depth = z;
        projected[i].angle = faces[i].angle;
    }

    // Sort back-to-front
    for (int i = 0; i < 5; i++)
        for (int j = i + 1; j < 6; j++)
            if (projected[i].depth > projected[j].depth)
                std::swap(projected[i], projected[j]);

    for (int i = 0; i < 6; i++) {
        f32 radius = 10.0f;
        if (projected[i].depth > 0) radius = 12.0f; // front faces larger

        dl->AddCircleFilled(projected[i].pos, radius, projected[i].color, 12);

        ImVec2 textSz = ImGui::CalcTextSize(projected[i].label);
        dl->AddText(ImVec2(projected[i].pos.x - textSz.x * 0.5f,
                           projected[i].pos.y - textSz.y * 0.5f),
                    IM_COL32(255, 255, 255, 220), projected[i].label);

        // Click to snap to that view
        ImVec2 mp = ImGui::GetIO().MousePos;
        f32 dx = mp.x - projected[i].pos.x;
        f32 dy = mp.y - projected[i].pos.y;
        if (dx * dx + dy * dy < radius * radius) {
            dl->AddCircle(projected[i].pos, radius + 2, IM_COL32(255, 255, 255, 200), 12, 2);
            if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                // Snap orbit camera to view angle
                switch (projected[i].angle) {
                    case ViewAngle::Top:
                        m_OrbitCam.yaw = 0; m_OrbitCam.pitch = -89.0f;
                        break;
                    case ViewAngle::Front:
                        m_OrbitCam.yaw = 0; m_OrbitCam.pitch = 0;
                        break;
                    case ViewAngle::Right:
                        m_OrbitCam.yaw = 90; m_OrbitCam.pitch = 0;
                        break;
                    default: break;
                }
                m_OrbitCam.ApplyToTransform(cam->GetOwner()->GetTransform());
                PushLog("[Viewport] Snapped to " + std::string(projected[i].label) + " view.");
            }
        }
    }
}

// ============================================================================
// Camera Preview PiP (bottom-right of viewport)
// ============================================================================

void EditorUI::DrawCameraPreviewPiP(f32 vpX, f32 vpY, f32 vpW, f32 vpH) {
    ImDrawList* dl = ImGui::GetForegroundDrawList();
    f32 pipW = 160, pipH = 100;
    ImVec2 pipPos(vpX + vpW - pipW - 10, vpY + vpH - pipH - 10);

    // Border and background
    dl->AddRectFilled(pipPos, ImVec2(pipPos.x + pipW, pipPos.y + pipH), IM_COL32(20, 20, 25, 220), 4);
    dl->AddRect(pipPos, ImVec2(pipPos.x + pipW, pipPos.y + pipH), IM_COL32(100, 100, 120, 200), 4, 0, 2);

    // Label
    dl->AddText(ImVec2(pipPos.x + 4, pipPos.y + 2), IM_COL32(200, 200, 200, 200), "Game Camera");

    // Show the main viewport texture scaled down (a simplified PiP)
    if (m_ViewportColor) {
        // Use the same FBO texture, just smaller
        ImVec2 uv0(0, 1), uv1(1, 0); // flip Y
        dl->AddImage(static_cast<ImTextureID>(m_ViewportColor),
                     ImVec2(pipPos.x + 2, pipPos.y + 16),
                     ImVec2(pipPos.x + pipW - 2, pipPos.y + pipH - 2),
                     uv0, uv1);
    }
}

// ============================================================================
// Grid & Snap Settings Popup
// ============================================================================

void EditorUI::DrawGridSnapSettings() {
    ImGui::SetNextWindowSize(ImVec2(300, 220), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Grid & Snap Settings", &m_ShowGridSnapSettings)) {
        ImGui::End();
        return;
    }

    ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1), "Grid Settings");
    ImGui::Separator();
    ImGui::Checkbox("Show Grid", &m_ShowGrid);
    ImGui::DragFloat("Grid Spacing", &m_GridSize, 0.1f, 0.1f, 20.0f, "%.1f");

    ImGui::Spacing();
    ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.4f, 1), "Snap Settings");
    ImGui::Separator();
    ImGui::Checkbox("Snap to Grid", &m_SnapToGrid);
    ImGui::DragFloat("Position Snap", &m_GridSize, 0.1f, 0.1f, 10.0f, "%.1f");
    ImGui::DragFloat("Rotation Snap (deg)", &m_RotationSnap, 1.0f, 1.0f, 90.0f, "%.0f");
    ImGui::DragFloat("Scale Snap", &m_ScaleSnap, 0.05f, 0.01f, 2.0f, "%.2f");

    ImGui::Spacing();
    ImGui::TextDisabled("Position snap applies when Snap checkbox is on.");
    ImGui::TextDisabled("Rotation/scale snap apply during gizmo drag.");

    ImGui::End();
}
// ── Bottom Tabbed Panel ────────────────────────────────────────────────────

void EditorUI::DrawBottomTabs() {
    ImGui::Begin("##BottomPanel", nullptr,
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse);

    if (ImGui::BeginTabBar("BottomTabBar")) {
        if (ImGui::BeginTabItem("Console")) {
            m_BottomTab = 0;
            // Inline console content
            if (ImGui::Button("Clear")) s_Logs.clear();
            ImGui::SameLine();
            ImGui::TextDisabled("(%d lines)", static_cast<int>(s_Logs.size()));
            ImGui::Separator();
            ImGui::BeginChild("ConsoleScroll", ImVec2(0, 0), false,
                               ImGuiWindowFlags_HorizontalScrollbar);
            for (auto& line : s_Logs) {
                if (line.find("[ERROR]") != std::string::npos ||
                    line.find("[FATAL]") != std::string::npos) {
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1, 0.3f, 0.3f, 1));
                    ImGui::TextUnformatted(line.c_str());
                    ImGui::PopStyleColor();
                } else if (line.find("[WARN]") != std::string::npos) {
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1, 0.8f, 0.2f, 1));
                    ImGui::TextUnformatted(line.c_str());
                    ImGui::PopStyleColor();
                } else {
                    ImGui::TextUnformatted(line.c_str());
                }
            }
            if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 10.0f)
                ImGui::SetScrollHereY(1.0f);
            ImGui::EndChild();
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Terrain")) {
            m_BottomTab = 1;
            DrawTerrainPanel();
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Material")) {
            m_BottomTab = 2;
            DrawMaterialPanel();
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Particles")) {
            m_BottomTab = 3;
            DrawParticlePanel();
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Animation")) {
            m_BottomTab = 4;
            DrawAnimationPanel();
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Node Script")) {
            m_BottomTab = 5;
            DrawNodeScriptPanel();
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Behaviors")) {
            m_BottomTab = 6;
            DrawBehaviorPanel();
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Code Script")) {
            m_BottomTab = 7;
            DrawCodeScriptPanel();
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Assets")) {
            m_BottomTab = 8;
            DrawAssetBrowser();
            ImGui::EndTabItem();
        }

        ImGui::EndTabBar();
    }

    ImGui::End();

    // ── Keyboard Shortcuts window ──────────────────────────────────────────
    if (m_ShowKeyboardShortcuts) {
        DrawKeyboardShortcuts();
    }

    // ── Grid & Snap Settings popup ─────────────────────────────────────────
    if (m_ShowGridSnapSettings) {
        DrawGridSnapSettings();
    }

    // ── Editor Settings Popup (API key, model selection) ───────────────────
    if (m_ShowEditorSettings) {
        ImGui::OpenPopup("Editor Settings");
        m_ShowEditorSettings = false;
    }
    if (ImGui::BeginPopupModal("Editor Settings", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1), "Gemini AI Configuration");
        ImGui::Separator();

        // Pre-fill from current config
        if (m_AIKeyBuf[0] == '\0' && m_AI && !m_AI->GetConfig().apiKey.empty()) {
            std::snprintf(m_AIKeyBuf, sizeof(m_AIKeyBuf), "%s", m_AI->GetConfig().apiKey.c_str());
        }

        ImGui::InputText("API Key", m_AIKeyBuf, sizeof(m_AIKeyBuf));
        ImGui::TextDisabled("Key is stored in gamevoid_config.ini");

        if (m_AI) {
            // Model selector
            const char* models[] = { "gemini-2.0-flash", "gemini-1.5-flash", "gemini-1.5-pro" };
            static int modelIdx = 0;
            if (m_AI->GetConfig().model == "gemini-2.0-flash") modelIdx = 0;
            else if (m_AI->GetConfig().model == "gemini-1.5-flash") modelIdx = 1;
            else if (m_AI->GetConfig().model == "gemini-1.5-pro") modelIdx = 2;

            if (ImGui::Combo("Model", &modelIdx, models, 3)) {
                m_AI->GetConfigMut().model = models[modelIdx];
            }

            ImGui::DragFloat("Temperature", &m_AI->GetConfigMut().temperature, 0.05f, 0.0f, 2.0f);
        }

        ImGui::Separator();
        if (ImGui::Button("Save & Apply", ImVec2(140, 28))) {
            if (m_AI) {
                m_AI->Init(std::string(m_AIKeyBuf));
                PushLog("[Settings] API key saved. Model: " + m_AI->GetConfig().model);
            }
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(80, 28))) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

// ── Terrain Panel ──────────────────────────────────────────────────────────

void EditorUI::DrawTerrainPanel() {
    ImGui::Columns(2, "TerrainCols", true);
    ImGui::SetColumnWidth(0, 300);

    // Generation settings
    ImGui::TextColored(ImVec4(0.4f, 0.8f, 0.4f, 1), "Generation");
    ImGui::DragInt("Resolution", &m_TerrainRes, 1, 8, 256);
    ImGui::DragFloat("World Size", &m_TerrainSize, 0.5f, 5, 200);
    ImGui::DragFloat("Max Height", &m_TerrainHeight, 0.1f, 0.5f, 50);
    ImGui::DragInt("Seed", &m_TerrainSeed, 1, 0, 99999);
    ImGui::DragInt("Octaves", &m_TerrainOctaves, 1, 1, 12);

    if (ImGui::Button("Generate Terrain", ImVec2(-1, 24))) {
        AddTerrain();
    }

    // Find existing terrain on selected object
    TerrainComponent* terrain = nullptr;
    if (m_Selected) {
        for (auto& comp : m_Selected->GetComponents()) {
            if (comp->GetTypeName() == "Terrain") {
                terrain = dynamic_cast<TerrainComponent*>(comp.get());
                break;
            }
        }
    }

    if (terrain) {
        ImGui::Separator();
        if (ImGui::Button("Regenerate", ImVec2(-1, 22))) {
            terrain->Generate(static_cast<u32>(m_TerrainRes), m_TerrainSize,
                              m_TerrainHeight, static_cast<u32>(m_TerrainSeed), m_TerrainOctaves);
            PushLog("[Terrain] Regenerated terrain.");
        }
    }

    ImGui::NextColumn();

    // Brush tools
    ImGui::TextColored(ImVec4(0.8f, 0.6f, 0.2f, 1), "Brush Tools");
    const char* brushModes[] = { "Raise", "Lower", "Smooth", "Flatten", "Paint" };
    ImGui::Combo("Mode", &m_TerrainBrushMode, brushModes, 5);
    ImGui::DragFloat("Radius", &m_TerrainBrushRadius, 0.1f, 0.5f, 20);
    ImGui::DragFloat("Strength", &m_TerrainBrushStrength, 0.01f, 0.01f, 5.0f);

    if (m_TerrainBrushMode == 4) {
        const char* layers[] = { "Grass", "Rock", "Sand", "Snow" };
        ImGui::Combo("Paint Layer", &m_TerrainPaintLayer, layers, 4);
    }

    // Visual brush indicator
    ImGui::Separator();
    ImGui::TextDisabled("Click viewport to paint (placeholder).");
    if (terrain) {
        ImGui::Text("Vertices: %u", (terrain->GetResolution() + 1) * (terrain->GetResolution() + 1));
        ImGui::Text("Triangles: %u", terrain->GetIndexCount() / 3);
    }

    ImGui::Columns(1);
}

// ── Material Panel ─────────────────────────────────────────────────────────

void EditorUI::DrawMaterialPanel() {
    ImGui::Columns(2, "MatCols", true);
    ImGui::SetColumnWidth(0, 200);

    // Material list
    ImGui::TextColored(ImVec4(0.6f, 0.4f, 0.8f, 1), "Materials");
    if (ImGui::Button("+ New Material", ImVec2(-1, 22))) {
        PushLog("[Material] Created '" + std::string(m_MatNameBuf) + "'.");
    }
    ImGui::Separator();
    // Placeholder material list
    ImGui::TextDisabled("(Default Material)");
    ImGui::TextDisabled("(Use Inspector to edit per-object)");

    ImGui::NextColumn();

    // PBR property editor
    ImGui::TextColored(ImVec4(0.8f, 0.6f, 0.8f, 1), "PBR Properties");
    ImGui::InputText("Name##Mat", m_MatNameBuf, sizeof(m_MatNameBuf));
    ImGui::ColorEdit4("Albedo", m_MatAlbedo);
    ImGui::SliderFloat("Roughness", &m_MatRoughness, 0.0f, 1.0f);
    ImGui::SliderFloat("Metallic", &m_MatMetallic, 0.0f, 1.0f);
    ImGui::ColorEdit3("Emission", m_MatEmission);
    ImGui::DragFloat("Emission Str", &m_MatEmissionStr, 0.01f, 0.0f, 10.0f);
    ImGui::SliderFloat("AO", &m_MatAO, 0.0f, 1.0f);

    ImGui::Separator();

    // Live preview sphere (placeholder visualization)
    ImGui::TextColored(ImVec4(0.5f, 0.8f, 0.5f, 1), "Preview");
    ImVec2 previewSize(80, 80);
    ImVec2 cur = ImGui::GetCursorScreenPos();
    ImDrawList* draw = ImGui::GetWindowDrawList();
    // Draw a fake sphere preview using gradient circle
    ImVec2 center(cur.x + 40, cur.y + 40);
    ImU32 baseCol = ImGui::ColorConvertFloat4ToU32(
        ImVec4(m_MatAlbedo[0], m_MatAlbedo[1], m_MatAlbedo[2], 1.0f));
    ImU32 darkCol = ImGui::ColorConvertFloat4ToU32(
        ImVec4(m_MatAlbedo[0] * 0.3f, m_MatAlbedo[1] * 0.3f, m_MatAlbedo[2] * 0.3f, 1.0f));
    draw->AddCircleFilled(center, 38, darkCol, 32);
    draw->AddCircleFilled(ImVec2(center.x - 8, center.y - 8), 28, baseCol, 32);
    // Specular highlight based on roughness (smaller = shinier)
    f32 specSize = 12.0f * (1.0f - m_MatRoughness);
    if (specSize > 1) {
        draw->AddCircleFilled(ImVec2(center.x - 15, center.y - 15),
                              specSize, IM_COL32(255, 255, 255, 180), 16);
    }
    ImGui::Dummy(previewSize);

    ImGui::SameLine();
    ImGui::BeginGroup();
    ImGui::TextDisabled("R: %.2f", m_MatRoughness);
    ImGui::TextDisabled("M: %.2f", m_MatMetallic);
    ImGui::TextDisabled("AO: %.2f", m_MatAO);
    ImGui::EndGroup();

    ImGui::Separator();

    // Shader Graph section
    ImGui::TextColored(ImVec4(0.4f, 0.7f, 1.0f, 1), "Shader Graph");
    ImGui::TextDisabled("(Visual node graph for advanced materials)");
    const char* nodeTypes[] = { "Color", "Texture", "Multiply", "Add", "Lerp", "Fresnel", "Time", "UV" };
    static int sgNodeSel = 0;
    ImGui::Combo("Add Node##SG", &sgNodeSel, nodeTypes, 8);
    ImGui::SameLine();
    if (ImGui::Button("Add##SG")) {
        PushLog("[Material] Added shader graph node: " + std::string(nodeTypes[sgNodeSel]));
    }

    ImGui::Columns(1);
}

// ── Particle Panel ─────────────────────────────────────────────────────────

void EditorUI::DrawParticlePanel() {
    // Find ParticleEmitter on selected object
    ParticleEmitter* emitter = nullptr;
    if (m_Selected) {
        for (auto& comp : m_Selected->GetComponents()) {
            if (comp->GetTypeName() == "ParticleEmitter") {
                emitter = dynamic_cast<ParticleEmitter*>(comp.get());
                break;
            }
        }
    }

    ImGui::Columns(3, "ParticleCols", true);
    ImGui::SetColumnWidth(0, 200);
    ImGui::SetColumnWidth(1, 250);

    // Column 1: Presets and controls
    ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.2f, 1), "Presets");
    const char* presets[] = { "Custom", "Fire", "Smoke", "Sparks", "Rain", "Snow", "Explosion" };
    if (ImGui::Combo("Preset", &m_ParticlePreset, presets, 7)) {
        if (emitter && m_ParticlePreset > 0) {
            emitter->ApplyPreset(static_cast<ParticlePreset>(m_ParticlePreset));
            PushLog("[Particles] Applied preset: " + std::string(presets[m_ParticlePreset]));
        }
    }
    ImGui::Separator();
    if (emitter) {
        if (ImGui::Button(emitter->IsPlaying() ? "Pause" : "Play", ImVec2(60, 22))) {
            if (emitter->IsPlaying()) emitter->Stop(); else emitter->Play();
        }
        ImGui::SameLine();
        if (ImGui::Button("Reset", ImVec2(60, 22))) {
            emitter->Reset();
        }
        ImGui::Text("Alive: %u / %u", emitter->GetAliveCount(), emitter->maxParticles);
    } else {
        ImGui::TextDisabled("Select a ParticleEmitter object");
        if (ImGui::Button("+ Add Emitter", ImVec2(-1, 22))) {
            AddParticleEmitter();
        }
    }

    ImGui::NextColumn();

    // Column 2: Emission settings
    if (emitter) {
        ImGui::TextColored(ImVec4(1, 0.8f, 0.3f, 1), "Emission");
        ImGui::DragFloat("Rate", &emitter->emissionRate, 1, 0, 2000);
        int mp = static_cast<int>(emitter->maxParticles);
        if (ImGui::DragInt("Max Particles", &mp, 10, 10, 10000)) {
            emitter->maxParticles = static_cast<u32>(mp);
        }
        ImGui::DragFloat("Speed Min", &emitter->speedMin, 0.1f, 0, 50);
        ImGui::DragFloat("Speed Max", &emitter->speedMax, 0.1f, 0, 50);
        ImGui::DragFloat("Lifetime Min", &emitter->lifetimeMin, 0.05f, 0.01f, 30);
        ImGui::DragFloat("Lifetime Max", &emitter->lifetimeMax, 0.05f, 0.01f, 30);
        ImGui::DragFloat("Spread", &emitter->spread, 0.5f, 0, 180);

        float dir[3] = { emitter->direction.x, emitter->direction.y, emitter->direction.z };
        if (ImGui::DragFloat3("Direction", dir, 0.02f)) {
            emitter->direction = { dir[0], dir[1], dir[2] };
        }
        float grav[3] = { emitter->gravity.x, emitter->gravity.y, emitter->gravity.z };
        if (ImGui::DragFloat3("Gravity", grav, 0.1f)) {
            emitter->gravity = { grav[0], grav[1], grav[2] };
        }
    }

    ImGui::NextColumn();

    // Column 3: Appearance
    if (emitter) {
        ImGui::TextColored(ImVec4(0.4f, 0.8f, 1, 1), "Appearance");
        float cs[4] = { emitter->colorStart.x, emitter->colorStart.y, emitter->colorStart.z, emitter->colorStart.w };
        if (ImGui::ColorEdit4("Color Start", cs)) emitter->colorStart = {cs[0],cs[1],cs[2],cs[3]};
        float ce[4] = { emitter->colorEnd.x, emitter->colorEnd.y, emitter->colorEnd.z, emitter->colorEnd.w };
        if (ImGui::ColorEdit4("Color End", ce)) emitter->colorEnd = {ce[0],ce[1],ce[2],ce[3]};
        ImGui::DragFloat("Size Start", &emitter->sizeStart, 0.005f, 0.001f, 5.0f);
        ImGui::DragFloat("Size End", &emitter->sizeEnd, 0.005f, 0.0f, 5.0f);
        ImGui::Checkbox("Looping", &emitter->looping);
        ImGui::Checkbox("World Space", &emitter->worldSpace);

        const char* shapes[] = { "Point", "Sphere", "Cone", "Box" };
        int shp = static_cast<int>(emitter->shape);
        if (ImGui::Combo("Shape", &shp, shapes, 4)) {
            emitter->shape = static_cast<EmitterShape>(shp);
        }
        if (emitter->shape != EmitterShape::Point) {
            ImGui::DragFloat("Shape Radius", &emitter->shapeRadius, 0.1f, 0, 50);
        }
    }

    ImGui::Columns(1);
}

// ── Animation Panel ────────────────────────────────────────────────────────

void EditorUI::DrawAnimationPanel() {
    Animator* animator = nullptr;
    if (m_Selected) {
        for (auto& comp : m_Selected->GetComponents()) {
            if (comp->GetTypeName() == "Animator") {
                animator = dynamic_cast<Animator*>(comp.get());
                break;
            }
        }
    }

    ImGui::Columns(2, "AnimCols", true);
    ImGui::SetColumnWidth(0, 220);

    // Clip management
    ImGui::TextColored(ImVec4(0.3f, 0.7f, 1.0f, 1), "Animation Clips");

    if (animator) {
        for (auto& kv : animator->GetClips()) {
            bool isCurrent = (kv.first == animator->GetCurrentClipName());
            if (isCurrent) ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.3f, 1, 0.3f, 1));
            if (ImGui::Selectable(kv.first.c_str())) {
                animator->Play(kv.first);
                PushLog("[Animation] Playing: " + kv.first);
            }
            if (isCurrent) ImGui::PopStyleColor();
        }

        ImGui::Separator();
        ImGui::InputText("Clip Name", m_AnimClipName, sizeof(m_AnimClipName));
        if (ImGui::Button("+ Add Clip", ImVec2(-1, 22))) {
            AnimationClip clip(m_AnimClipName);
            clip.SetDuration(2.0f);
            // Add default keyframes
            Keyframe k0; k0.time = 0;
            k0.position = m_Selected->GetTransform().position;
            k0.rotation = m_Selected->GetTransform().rotation;
            k0.scale    = m_Selected->GetTransform().scale;
            clip.AddKeyframe(k0);
            Keyframe k1; k1.time = 2.0f;
            k1.position = m_Selected->GetTransform().position;
            k1.position.y += 2.0f;
            k1.rotation = m_Selected->GetTransform().rotation;
            k1.scale    = m_Selected->GetTransform().scale;
            clip.AddKeyframe(k1);
            animator->AddClip(clip);
            PushLog("[Animation] Added clip: " + std::string(m_AnimClipName));
        }
    } else {
        ImGui::TextDisabled("Select an object with Animator");
        if (m_Selected && ImGui::Button("+ Add Animator", ImVec2(-1, 22))) {
            m_Selected->AddComponent<Animator>();
            PushLog("[Animation] Added Animator to " + m_Selected->GetName());
        }
    }

    ImGui::NextColumn();

    // Timeline
    ImGui::TextColored(ImVec4(1, 0.8f, 0.3f, 1), "Timeline");
    if (animator && animator->IsPlaying()) {
        m_AnimTimeline = animator->GetTime();
    }

    if (animator) {
        auto* clip = animator->GetClip(animator->GetCurrentClipName());
        f32 dur = clip ? clip->GetDuration() : 2.0f;

        if (ImGui::SliderFloat("Time", &m_AnimTimeline, 0, dur, "%.2f s")) {
            animator->SetTime(m_AnimTimeline);
        }

        // Playback controls
        if (ImGui::Button(animator->IsPlaying() ? "Pause" : "Play", ImVec2(60, 22))) {
            if (animator->IsPlaying()) animator->Pause(); else animator->Resume();
        }
        ImGui::SameLine();
        if (ImGui::Button("Stop", ImVec2(60, 22))) {
            animator->Stop();
            m_AnimTimeline = 0;
        }
        ImGui::SameLine();
        ImGui::DragFloat("Speed", &m_AnimSpeed, 0.05f, 0.0f, 5.0f);
        animator->SetSpeed(m_AnimSpeed);

        // Visual timeline bar
        if (clip) {
            ImGui::Separator();
            ImVec2 tStart = ImGui::GetCursorScreenPos();
            f32 tW = ImGui::GetContentRegionAvail().x;
            f32 tH = 30;
            ImDrawList* dl = ImGui::GetWindowDrawList();

            // Background
            dl->AddRectFilled(tStart, ImVec2(tStart.x + tW, tStart.y + tH),
                              IM_COL32(40, 40, 50, 255));

            // Keyframe markers
            for (auto& kf : clip->GetKeyframes()) {
                f32 x = tStart.x + (kf.time / dur) * tW;
                dl->AddTriangleFilled(
                    ImVec2(x - 5, tStart.y), ImVec2(x + 5, tStart.y),
                    ImVec2(x, tStart.y + 10), IM_COL32(255, 200, 50, 255));
                dl->AddLine(ImVec2(x, tStart.y + 10), ImVec2(x, tStart.y + tH),
                           IM_COL32(255, 200, 50, 100));
            }

            // Playhead
            f32 px = tStart.x + (m_AnimTimeline / dur) * tW;
            dl->AddLine(ImVec2(px, tStart.y), ImVec2(px, tStart.y + tH),
                       IM_COL32(255, 60, 60, 255), 2.0f);

            ImGui::Dummy(ImVec2(tW, tH));

            // Interp mode
            const char* interpModes[] = { "Linear", "Step", "Cubic Smooth" };
            int iMode = static_cast<int>(clip->GetInterpMode());
            if (ImGui::Combo("Interpolation", &iMode, interpModes, 3)) {
                clip->SetInterpMode(static_cast<InterpMode>(iMode));
            }
            bool loop = clip->IsLooping();
            if (ImGui::Checkbox("Looping", &loop)) { clip->SetLooping(loop); }

            // Add keyframe at current time
            if (ImGui::Button("+ Keyframe Here", ImVec2(-1, 22))) {
                Keyframe kf;
                kf.time = m_AnimTimeline;
                kf.position = m_Selected->GetTransform().position;
                kf.rotation = m_Selected->GetTransform().rotation;
                kf.scale    = m_Selected->GetTransform().scale;
                clip->AddKeyframe(kf);
                PushLog("[Animation] Added keyframe at t=" + std::to_string(m_AnimTimeline));
            }
        }
    }

    ImGui::Columns(1);
}

// ── Behavior Editor Panel ──────────────────────────────────────────────────

// ── Code Script Panel (inline script editor) ──────────────────────────────

void EditorUI::DrawCodeScriptPanel() {
    ImGui::Columns(2, "CodeScriptCols", true);
    ImGui::SetColumnWidth(0, 250);

    // Column 1: Script file management
    ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1), "Script Files");
    ImGui::Separator();

    ImGui::InputText("Path", m_ScriptPathBuf, sizeof(m_ScriptPathBuf));

    if (ImGui::Button("Load File", ImVec2(-1, 24))) {
        std::string path(m_ScriptPathBuf);
        if (!path.empty() && m_Script) {
            std::ifstream file(path);
            if (file.is_open()) {
                std::stringstream ss;
                ss << file.rdbuf();
                std::string src = ss.str();
                size_t len = src.size();
                if (len >= sizeof(m_ScriptCodeBuf)) len = sizeof(m_ScriptCodeBuf) - 1;
                std::memcpy(m_ScriptCodeBuf, src.c_str(), len);
                m_ScriptCodeBuf[len] = '\0';
                PushLog("[Script] Loaded: " + path);
            } else {
                PushLog("[Script] ERROR: Cannot open " + path);
            }
        }
    }

    if (ImGui::Button("Save File", ImVec2(-1, 24))) {
        std::string path(m_ScriptPathBuf);
        if (!path.empty()) {
            std::ofstream file(path);
            if (file.is_open()) {
                file << m_ScriptCodeBuf;
                PushLog("[Script] Saved: " + path);
            } else {
                PushLog("[Script] ERROR: Cannot write " + path);
            }
        }
    }

    ImGui::Separator();
    ImGui::TextColored(ImVec4(1, 0.9f, 0.3f, 1), "Quick Actions");

    if (ImGui::Button("Run Script", ImVec2(-1, 24))) {
        if (m_Script) {
            std::string code(m_ScriptCodeBuf);
            if (m_Script->Execute(code)) {
                PushLog("[Script] Executed successfully.");
            } else {
                PushLog("[Script] ERROR: " + m_Script->GetLastError());
            }
        }
    }

    if (ImGui::Button("Attach to Selected", ImVec2(-1, 24))) {
        if (m_Selected) {
            auto* sc = m_Selected->GetComponent<ScriptComponent>();
            if (!sc) {
                sc = m_Selected->AddComponent<ScriptComponent>();
                if (m_Script) sc->SetEngine(m_Script);
            }
            sc->SetSource(std::string(m_ScriptCodeBuf));
            PushLog("[Script] Attached to " + m_Selected->GetName());
        } else {
            PushLog("[Script] No object selected.");
        }
    }

    ImGui::Separator();
    ImGui::TextDisabled("GVScript Syntax:");
    ImGui::TextWrapped(
        "var x = 10\n"
        "func greet(name) {\n"
        "  print(\"Hello\", name)\n"
        "}\n"
        "if (x > 5) { ... }\n"
        "while (x > 0) { ... }\n"
        "Built-ins: print, sin, cos,\n"
        "sqrt, abs, random, spawn,\n"
        "find, destroy, set_position\n"
    );

    ImGui::NextColumn();

    // Column 2: Code editor
    ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.6f, 1), "Script Editor");
    ImGui::Separator();

    ImVec2 avail = ImGui::GetContentRegionAvail();
    ImGui::InputTextMultiline("##ScriptCode", m_ScriptCodeBuf, sizeof(m_ScriptCodeBuf),
        ImVec2(avail.x, avail.y - 4),
        ImGuiInputTextFlags_AllowTabInput);

    ImGui::Columns(1);
}

void EditorUI::DrawBehaviorPanel() {
    ImGui::Columns(2, "BehaviorCols", true);
    ImGui::SetColumnWidth(0, 250);

    // Column 1: Registered behaviors & object's attached scripts
    ImGui::TextColored(ImVec4(0.4f, 0.9f, 0.4f, 1), "Behavior Registry");
    ImGui::Separator();

    auto names = BehaviorRegistry::Instance().GetNames();
    for (auto& name : names) {
        ImGui::BulletText("%s", name.c_str());
        ImGui::SameLine();
        if (m_Selected) {
            std::string btnLabel = "Attach##" + name;
            if (ImGui::SmallButton(btnLabel.c_str())) {
                if (name == "Rotator")          m_Selected->AddComponent<RotatorBehavior>();
                else if (name == "Bob")         m_Selected->AddComponent<BobBehavior>();
                else if (name == "Follow")      m_Selected->AddComponent<FollowBehavior>();
                else if (name == "AutoDestroy") m_Selected->AddComponent<AutoDestroyBehavior>();
                PushLog("[Behavior] Attached " + name + " to " + m_Selected->GetName());
            }
        }
    }

    if (names.empty()) {
        ImGui::TextDisabled("No behaviors registered.");
    }

    ImGui::NextColumn();

    // Column 2: Attached behaviors on selected object
    ImGui::TextColored(ImVec4(0.8f, 0.6f, 1.0f, 1), "Attached Behaviors");
    ImGui::Separator();

    if (!m_Selected) {
        ImGui::TextDisabled("Select an object to see its behaviors.");
    } else {
        bool foundAny = false;
        for (auto& comp : m_Selected->GetComponents()) {
            auto* ns = dynamic_cast<NativeScriptComponent*>(comp.get());
            if (!ns) continue;
            foundAny = true;

            ImGui::PushID(ns);

            // Enable/disable checkbox
            bool enabled = ns->IsEnabled();
            if (ImGui::Checkbox("##en", &enabled)) {
                ns->SetEnabled(enabled);
            }
            ImGui::SameLine();

            // Behavior name with colour
            ImVec4 col = enabled ? ImVec4(0.4f, 1.0f, 0.4f, 1) : ImVec4(0.5f, 0.5f, 0.5f, 1);
            ImGui::TextColored(col, "%s", ns->GetScriptName().c_str());

            // Inline property editors
            if (auto* rot = dynamic_cast<RotatorBehavior*>(ns)) {
                ImGui::Indent();
                ImGui::DragFloat("Speed", &rot->speed, 1, -720, 720);
                float ax[3] = { rot->axis.x, rot->axis.y, rot->axis.z };
                if (ImGui::DragFloat3("Axis", ax, 0.1f))
                    rot->axis = Vec3(ax[0], ax[1], ax[2]);
                ImGui::Unindent();
            }
            if (auto* bob = dynamic_cast<BobBehavior*>(ns)) {
                ImGui::Indent();
                ImGui::DragFloat("Amplitude", &bob->amplitude, 0.05f, 0, 10);
                ImGui::DragFloat("Frequency", &bob->frequency, 0.1f, 0, 20);
                ImGui::Unindent();
            }
            if (auto* ad = dynamic_cast<AutoDestroyBehavior*>(ns)) {
                ImGui::Indent();
                ImGui::DragFloat("Lifetime", &ad->lifetime, 0.1f, 0.1f, 60);
                ImGui::Unindent();
            }

            ImGui::PopID();
            ImGui::Separator();
        }

        if (!foundAny) {
            ImGui::TextDisabled("No C++ behaviors attached.");
            ImGui::TextDisabled("Use 'Add Component' in Inspector,");
            ImGui::TextDisabled("or click 'Attach' on the left.");
        }
    }

    ImGui::Columns(1);
}

// ── Node Script Panel ──────────────────────────────────────────────────────

void EditorUI::DrawNodeScriptPanel() {
    NodeGraphComponent* ngc = nullptr;
    if (m_Selected) {
        for (auto& comp : m_Selected->GetComponents()) {
            if (comp->GetTypeName() == "NodeGraph") {
                ngc = dynamic_cast<NodeGraphComponent*>(comp.get());
                break;
            }
        }
    }

    ImGui::Columns(2, "NodeCols", true);
    ImGui::SetColumnWidth(0, 200);

    // Node palette
    ImGui::TextColored(ImVec4(0.8f, 0.4f, 1, 1), "Node Palette");

    struct NodeEntry { const char* name; VisualNodeType type; };
    static const NodeEntry palette[] = {
        { "On Start",       VisualNodeType::OnStart },
        { "On Update",      VisualNodeType::OnUpdate },
        { "Branch",         VisualNodeType::Branch },
        { "For Loop",       VisualNodeType::ForLoop },
        { "Print",          VisualNodeType::Print },
        { "Set Position",   VisualNodeType::SetPosition },
        { "Set Rotation",   VisualNodeType::SetRotation },
        { "Set Scale",      VisualNodeType::SetScale },
        { "Add",            VisualNodeType::Add },
        { "Multiply",       VisualNodeType::Multiply },
        { "Get Position",   VisualNodeType::GetPosition },
        { "Get Delta Time", VisualNodeType::GetDeltaTime },
        { "Greater",        VisualNodeType::Greater },
        { "Less",           VisualNodeType::Less },
        { "And",            VisualNodeType::And },
        { "Not",            VisualNodeType::Not },
        { "Set Variable",   VisualNodeType::SetVariable },
        { "Get Variable",   VisualNodeType::GetVariable },
        { "Random",         VisualNodeType::Random },
    };
    static const int paletteCount = sizeof(palette) / sizeof(palette[0]);

    if (ngc) {
        for (int i = 0; i < paletteCount; ++i) {
            if (ImGui::Button(palette[i].name, ImVec2(-1, 20))) {
                Vec2 pos = { 100.0f + static_cast<f32>(ngc->GetGraph().GetNodes().size()) * 20.0f,
                             50.0f };
                ngc->GetGraph().AddNode(palette[i].type, pos);
                PushLog("[Script] Added node: " + std::string(palette[i].name));
            }
        }
    } else {
        ImGui::TextDisabled("Select object with NodeGraph");
        if (m_Selected && ImGui::Button("+ Add NodeGraph", ImVec2(-1, 22))) {
            m_Selected->AddComponent<NodeGraphComponent>();
            PushLog("[Script] Added NodeGraph to " + m_Selected->GetName());
        }
    }

    ImGui::NextColumn();

    // Canvas
    ImGui::TextColored(ImVec4(0.6f, 0.8f, 1.0f, 1), "Graph Canvas");
    if (ngc) {
        NodeGraph& graph = ngc->GetGraph();
        ImVec2 canvasPos = ImGui::GetCursorScreenPos();
        ImVec2 canvasSize = ImGui::GetContentRegionAvail();
        if (canvasSize.y < 50) canvasSize.y = 50;
        ImDrawList* dl = ImGui::GetWindowDrawList();

        // Background
        dl->AddRectFilled(canvasPos,
            ImVec2(canvasPos.x + canvasSize.x, canvasPos.y + canvasSize.y),
            IM_COL32(30, 30, 35, 255));

        // Grid
        for (f32 x = 0; x < canvasSize.x; x += 30) {
            dl->AddLine(ImVec2(canvasPos.x + x, canvasPos.y),
                       ImVec2(canvasPos.x + x, canvasPos.y + canvasSize.y),
                       IM_COL32(50, 50, 55, 255));
        }
        for (f32 y = 0; y < canvasSize.y; y += 30) {
            dl->AddLine(ImVec2(canvasPos.x, canvasPos.y + y),
                       ImVec2(canvasPos.x + canvasSize.x, canvasPos.y + y),
                       IM_COL32(50, 50, 55, 255));
        }

        // Draw connections
        for (auto& conn : graph.GetConnections()) {
            auto* fromN = graph.GetNode(conn.fromNodeID);
            auto* toN   = graph.GetNode(conn.toNodeID);
            if (fromN && toN) {
                ImVec2 p1(canvasPos.x + fromN->position.x + 130,
                          canvasPos.y + fromN->position.y + 15);
                ImVec2 p2(canvasPos.x + toN->position.x,
                          canvasPos.y + toN->position.y + 15);
                ImVec2 cp1(p1.x + 50, p1.y);
                ImVec2 cp2(p2.x - 50, p2.y);
                dl->AddBezierCubic(p1, cp1, cp2, p2, IM_COL32(200, 200, 100, 200), 2);
            }
        }

        // Draw nodes
        for (auto& node : graph.GetNodes()) {
            f32 nx = canvasPos.x + node.position.x;
            f32 ny = canvasPos.y + node.position.y;
            f32 nw = 130, nh = 40;

            ImU32 headerCol = ImGui::ColorConvertFloat4ToU32(
                ImVec4(node.color.x, node.color.y, node.color.z, node.color.w));
            ImU32 bodyCol = IM_COL32(50, 50, 55, 230);

            // Body
            dl->AddRectFilled(ImVec2(nx, ny), ImVec2(nx + nw, ny + nh), bodyCol, 4);
            // Header
            dl->AddRectFilled(ImVec2(nx, ny), ImVec2(nx + nw, ny + 18), headerCol, 4);
            // Title
            dl->AddText(ImVec2(nx + 4, ny + 2), IM_COL32(255, 255, 255, 255),
                       node.label.c_str());

            // Input pins
            for (size_t i = 0; i < node.inputs.size() && i < 2; ++i) {
                f32 py = ny + 22 + static_cast<f32>(i) * 10;
                dl->AddCircleFilled(ImVec2(nx, py), 4, IM_COL32(100, 200, 255, 255));
            }
            // Output pins
            for (size_t i = 0; i < node.outputs.size() && i < 2; ++i) {
                f32 py = ny + 22 + static_cast<f32>(i) * 10;
                dl->AddCircleFilled(ImVec2(nx + nw, py), 4, IM_COL32(100, 255, 100, 255));
            }
        }

        ImGui::Dummy(canvasSize);

        // Node count info
        ImGui::Text("Nodes: %d  Connections: %d",
                    static_cast<int>(graph.GetNodes().size()),
                    static_cast<int>(graph.GetConnections().size()));
    } else {
        ImGui::TextDisabled("No graph selected.");
    }

    ImGui::Columns(1);
}

// ── Add Terrain helper ─────────────────────────────────────────────────────

void EditorUI::AddTerrain() {
    if (!m_Scene) return;
    static i32 terrainN = 0;
    terrainN++;
    std::string name = "Terrain_" + std::to_string(terrainN);
    auto* obj = m_Scene->CreateGameObject(name);
    auto* tc = obj->AddComponent<TerrainComponent>();
    tc->Generate(static_cast<u32>(m_TerrainRes), m_TerrainSize, m_TerrainHeight,
                 static_cast<u32>(m_TerrainSeed), m_TerrainOctaves);
    m_Selected = obj;
    m_BottomTab = 1; // Switch to terrain tab
    PushLog("[Editor] Added terrain '" + name + "' (" +
            std::to_string(m_TerrainRes) + "x" + std::to_string(m_TerrainRes) + ").");
}

// ── Add ParticleEmitter helper ─────────────────────────────────────────────

void EditorUI::AddParticleEmitter() {
    if (!m_Scene) return;
    static i32 particleN = 0;
    particleN++;
    std::string name = "Particles_" + std::to_string(particleN);
    auto* obj = m_Scene->CreateGameObject(name);
    obj->GetTransform().SetPosition(0, 2.0f, 0);
    auto* pe = obj->AddComponent<ParticleEmitter>();
    pe->ApplyPreset(ParticlePreset::Fire);
    pe->OnStart();
    m_Selected = obj;
    m_BottomTab = 3; // Switch to particles tab
    PushLog("[Editor] Added particle emitter '" + name + "'.");
}

// ── FBO Helpers ────────────────────────────────────────────────────────────

void EditorUI::CreateViewportFBO(u32 w, u32 h) {
    if (w < 1) w = 1;
    if (h < 1) h = 1;
    m_ViewportW = w;
    m_ViewportH = h;

    // Colour texture
    glGenTextures(1, &m_ViewportColor);
    glBindTexture(GL_TEXTURE_2D, m_ViewportColor);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8,
                 static_cast<GLsizei>(w), static_cast<GLsizei>(h),
                 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glBindTexture(GL_TEXTURE_2D, 0);

    // Depth renderbuffer
    glGenRenderbuffers(1, &m_ViewportDepth);
    glBindRenderbuffer(GL_RENDERBUFFER, m_ViewportDepth);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24,
                          static_cast<GLsizei>(w), static_cast<GLsizei>(h));
    glBindRenderbuffer(GL_RENDERBUFFER, 0);

    // Framebuffer
    glGenFramebuffers(1, &m_ViewportFBO);
    glBindFramebuffer(GL_FRAMEBUFFER, m_ViewportFBO);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                           GL_TEXTURE_2D, m_ViewportColor, 0);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                              GL_RENDERBUFFER, m_ViewportDepth);

    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE) {
        GV_LOG_ERROR("Viewport FBO incomplete! Status: " + std::to_string(status));
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void EditorUI::DestroyViewportFBO() {
    if (m_ViewportFBO)   { glDeleteFramebuffers(1, &m_ViewportFBO);   m_ViewportFBO = 0; }
    if (m_ViewportColor) { glDeleteTextures(1, &m_ViewportColor);     m_ViewportColor = 0; }
    if (m_ViewportDepth) { glDeleteRenderbuffers(1, &m_ViewportDepth); m_ViewportDepth = 0; }
}

void EditorUI::ResizeViewport(u32 w, u32 h) {
    DestroyViewportFBO();
    CreateViewportFBO(w, h);
}

// ── Add / Delete helpers ───────────────────────────────────────────────────

void EditorUI::AddCube() {
    BeginPlacement(PlacementType::Cube);
}

void EditorUI::AddLight() {
    BeginPlacement(PlacementType::Light);
}

// ── Placement Mode ─────────────────────────────────────────────────────────

void EditorUI::BeginPlacement(PlacementType type) {
    m_PlacementType = type;
    m_PlacementPreviewPos = Vec3(0, 0, 0);
    m_PlacementValid = false;
    const char* names[] = { "", "Cube", "Light", "Terrain", "Particles", "Floor" };
    PushLog("[Editor] Click in viewport to place " + std::string(names[static_cast<int>(type)]) + " (Right-click or Esc to cancel).");
}

void EditorUI::CancelPlacement() {
    if (m_PlacementType != PlacementType::None) {
        PushLog("[Editor] Placement cancelled.");
        m_PlacementType = PlacementType::None;
    }
}

Vec3 EditorUI::RaycastGroundPlane(Camera* cam, f32 ndcX, f32 ndcY, f32 planeY) {
    Mat4 invVP = (cam->GetProjectionMatrix() * cam->GetViewMatrix()).Inverse();
    Vec3 nearPt = invVP.TransformPoint(Vec3(ndcX, ndcY, -1.0f));
    Vec3 farPt  = invVP.TransformPoint(Vec3(ndcX, ndcY,  1.0f));
    Vec3 dir = (farPt - nearPt).Normalized();

    // Intersect with horizontal plane y = planeY
    if (std::fabs(dir.y) < 0.0001f) return Vec3(0, planeY, 0); // parallel
    f32 t = (planeY - nearPt.y) / dir.y;
    if (t < 0) t = 0; // behind camera — clamp
    return nearPt + dir * t;
}

void EditorUI::UpdatePlacement(Camera* cam, f32 mousePosX, f32 mousePosY) {
    if (m_PlacementType == PlacementType::None || !cam) return;

    f32 mx = (mousePosX - m_VpScreenX) / m_VpScreenW;
    f32 my = (mousePosY - m_VpScreenY) / m_VpScreenH;
    if (mx < 0 || mx > 1 || my < 0 || my > 1) { m_PlacementValid = false; return; }

    f32 ndcX = mx * 2.0f - 1.0f;
    f32 ndcY = 1.0f - my * 2.0f;

    Vec3 hit = RaycastGroundPlane(cam, ndcX, ndcY, 0.0f);

    // Snap to grid if enabled
    if (m_SnapToGrid && m_GridSize > 0.01f) {
        hit.x = std::round(hit.x / m_GridSize) * m_GridSize;
        hit.z = std::round(hit.z / m_GridSize) * m_GridSize;
    }

    // Objects sit on top of the ground plane
    if (m_PlacementType == PlacementType::Cube)
        hit.y = 0.5f;  // half-cube height
    else if (m_PlacementType == PlacementType::Light)
        hit.y = 3.0f;  // lights float above
    else if (m_PlacementType == PlacementType::Particles)
        hit.y = 1.0f;
    // Terrain and Floor stay at y=0

    m_PlacementPreviewPos = hit;
    m_PlacementValid = true;
}

void EditorUI::FinishPlacement() {
    if (m_PlacementType == PlacementType::None || !m_Scene) return;

    Vec3 pos = m_PlacementPreviewPos;

    switch (m_PlacementType) {
    case PlacementType::Cube: {
        static i32 cubeN = 0;
        cubeN++;
        std::string name = "Cube_" + std::to_string(cubeN);
        auto* obj = m_Scene->CreateGameObject(name);
        obj->GetTransform().SetPosition(pos.x, pos.y, pos.z);
        auto* mr = obj->AddComponent<MeshRenderer>();
        mr->primitiveType = PrimitiveType::Cube;
        mr->color = Vec4(0.4f + static_cast<f32>(std::rand() % 60) / 100.0f,
                         0.4f + static_cast<f32>(std::rand() % 60) / 100.0f,
                         0.4f + static_cast<f32>(std::rand() % 60) / 100.0f, 1.0f);
        auto* rb = obj->AddComponent<RigidBody>();
        rb->useGravity = true;
        obj->AddComponent<Collider>()->type = ColliderType::Box;
        if (m_Physics) m_Physics->RegisterBody(rb);
        m_Selected = obj;
        PushLog("[Editor] Placed '" + name + "' at (" +
                std::to_string((int)pos.x) + ", " + std::to_string((int)pos.y) + ", " + std::to_string((int)pos.z) + ").");
        break;
    }
    case PlacementType::Light: {
        static i32 lightN = 0;
        lightN++;
        std::string name = "PointLight_" + std::to_string(lightN);
        auto* obj = m_Scene->CreateGameObject(name);
        obj->GetTransform().SetPosition(pos.x, pos.y, pos.z);
        auto* pl = obj->AddComponent<PointLight>();
        pl->colour = Vec3(1, 1, 1);
        pl->intensity = 1.0f;
        m_Selected = obj;
        PushLog("[Editor] Placed '" + name + "'.");
        break;
    }
    case PlacementType::Terrain:
        AddTerrain();  // terrain is special, uses its own logic
        break;
    case PlacementType::Particles:
        AddParticleEmitter();
        if (m_Selected) m_Selected->GetTransform().SetPosition(pos.x, pos.y, pos.z);
        break;
    case PlacementType::Floor: {
        auto* obj = m_Scene->CreateGameObject("FloorPlane");
        obj->GetTransform().SetPosition(pos.x, 0, pos.z);
        obj->GetTransform().SetScale(40.0f, 1.0f, 40.0f);
        auto* mr = obj->AddComponent<MeshRenderer>();
        mr->primitiveType = PrimitiveType::Plane;
        mr->color = Vec4(0.4f, 0.4f, 0.42f, 1.0f);
        auto* rb = obj->AddComponent<RigidBody>();
        rb->bodyType = RigidBodyType::Static;
        rb->useGravity = false;
        auto* col = obj->AddComponent<Collider>();
        col->type = ColliderType::Box;
        col->boxHalfExtents = Vec3(0.5f, 0.01f, 0.5f);
        if (m_Physics) m_Physics->RegisterBody(rb);
        m_Selected = obj;
        PushLog("[Editor] Placed floor plane.");
        break;
    }
    default: break;
    }

    m_PlacementType = PlacementType::None;
}

void EditorUI::DeleteSelected() {
    if (!m_Scene) return;

    // Multi-select delete
    if (m_MultiSelected.size() > 1) {
        int count = 0;
        for (auto* obj : m_MultiSelected) {
            if (obj) { m_Scene->DestroyGameObject(obj); count++; }
        }
        PushLog("[Editor] Deleted " + std::to_string(count) + " objects.");
        m_MultiSelected.clear();
        m_Selected = nullptr;
        return;
    }

    if (!m_Selected) return;
    std::string name = m_Selected->GetName();
    m_Scene->DestroyGameObject(m_Selected);
    m_Selected = nullptr;
    m_MultiSelected.clear();
    PushLog("[Editor] Deleted '" + name + "'.");
}

void EditorUI::SaveScene(const std::string& path) {
    if (!m_Scene) {
        PushLog("[Editor] No scene to save.");
        return;
    }
    if (SceneSerializer::SaveScene(*m_Scene, path)) {
        PushLog("[Editor] Scene saved to '" + path + "'.");
    } else {
        PushLog("[Editor] Failed to save scene to '" + path + "'.");
    }
}

void EditorUI::LoadScene(const std::string& path) {
    if (!m_Scene) {
        PushLog("[Editor] No scene target for loading.");
        return;
    }
    if (SceneSerializer::LoadScene(*m_Scene, path, m_Physics)) {
        PushLog("[Editor] Scene loaded from '" + path + "'.");
        m_Selected = nullptr;
    } else {
        PushLog("[Editor] Failed to load scene from '" + path + "'.");
    }
}

} // namespace gv

#endif // GV_HAS_GLFW
