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
#include "physics/Physics.h"
#include "ai/AIManager.h"
#include "terrain/Terrain.h"
#include "effects/ParticleSystem.h"
#include "animation/Animation.h"
#include "scripting/NodeGraph.h"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <algorithm>
#include <cstdio>
#include <cmath>

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
            if (ImGui::MenuItem("Add Cube"))        { AddCube(); }
            if (ImGui::MenuItem("Add Light"))       { AddLight(); }
            if (ImGui::MenuItem("Add Terrain"))     { AddTerrain(); }
            if (ImGui::MenuItem("Add Particles"))   { AddParticleEmitter(); }
            ImGui::Separator();
            if (ImGui::MenuItem("Delete Selected")) { DeleteSelected(); }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("View")) {
            ImGui::MenuItem("AI Generator", nullptr, &m_ShowAIPanel);
            ImGui::MenuItem("ImGui Demo", nullptr, &m_ShowDemo);
            ImGui::Separator();
            if (ImGui::MenuItem("Console"))     { m_BottomTab = 0; }
            if (ImGui::MenuItem("Terrain"))     { m_BottomTab = 1; }
            if (ImGui::MenuItem("Material"))    { m_BottomTab = 2; }
            if (ImGui::MenuItem("Particles"))   { m_BottomTab = 3; }
            if (ImGui::MenuItem("Animation"))   { m_BottomTab = 4; }
            if (ImGui::MenuItem("Node Script")) { m_BottomTab = 5; }
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
    if (ImGui::Button("+ Cube"))  AddCube();
    ImGui::SameLine();
    if (ImGui::Button("+ Light")) AddLight();
    ImGui::SameLine();
    if (ImGui::Button("+ Terrain")) AddTerrain();
    ImGui::SameLine();
    if (ImGui::Button("+ Particles")) AddParticleEmitter();
    ImGui::SameLine();
    if (ImGui::Button("Delete"))  DeleteSelected();

    ImGui::End();
}

// ── Hierarchy Panel ────────────────────────────────────────────────────────

void EditorUI::DrawHierarchy() {
    ImGui::Begin("Scene Hierarchy");

    if (!m_Scene) { ImGui::Text("No scene."); ImGui::End(); return; }

    for (auto& obj : m_Scene->GetAllObjects()) {
        if (!obj) continue;
        bool selected = (m_Selected == obj.get());
        std::string label = obj->GetName() + "##" + std::to_string(obj->GetID());
        if (ImGui::Selectable(label.c_str(), selected)) {
            m_Selected = obj.get();
        }
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
        // Convert quaternion to approximate Euler for display
        // Using a simple extraction for display purposes
        float rotDeg[3] = { 0, 0, 0 };
        {
            // Quaternion to Euler (yaw-pitch-roll)
            const Quaternion& q = t.rotation;
            // pitch (X)
            f32 sinp = 2.0f * (q.w * q.x + q.y * q.z);
            f32 cosp = 1.0f - 2.0f * (q.x * q.x + q.y * q.y);
            rotDeg[0] = std::atan2(sinp, cosp) * (180.0f / 3.14159265358979f);
            // yaw (Y)
            f32 siny = 2.0f * (q.w * q.y - q.z * q.x);
            if (std::fabs(siny) >= 1.0f)
                rotDeg[1] = std::copysign(90.0f, siny);
            else
                rotDeg[1] = std::asin(siny) * (180.0f / 3.14159265358979f);
            // roll (Z)
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

    // Components list
    if (ImGui::CollapsingHeader("Components", ImGuiTreeNodeFlags_DefaultOpen)) {
        for (auto& comp : m_Selected->GetComponents()) {
            ImGui::BulletText("%s", comp->GetTypeName().c_str());

            // MeshRenderer specific
            if (comp->GetTypeName() == "MeshRenderer") {
                auto* mr = dynamic_cast<MeshRenderer*>(comp.get());
                if (mr) {
                    const char* types[] = { "None", "Triangle", "Cube" };
                    int idx = static_cast<int>(mr->primitiveType);
                    if (ImGui::Combo("Primitive", &idx, types, 3)) {
                        mr->primitiveType = static_cast<PrimitiveType>(idx);
                    }
                    float col[4] = { mr->color.x, mr->color.y, mr->color.z, mr->color.w };
                    if (ImGui::ColorEdit4("Color", col)) {
                        mr->color = Vec4(col[0], col[1], col[2], col[3]);
                    }
                }
            }

            // RigidBody specific
            if (comp->GetTypeName() == "RigidBody") {
                auto* rb = dynamic_cast<RigidBody*>(comp.get());
                if (rb) {
                    ImGui::DragFloat("Mass", &rb->mass, 0.1f, 0.01f, 1000.0f);
                    ImGui::Checkbox("Use Gravity", &rb->useGravity);
                    ImGui::DragFloat("Restitution", &rb->restitution, 0.01f, 0.0f, 1.0f);
                    ImGui::Text("Velocity: %.2f, %.2f, %.2f",
                        rb->velocity.x, rb->velocity.y, rb->velocity.z);
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

    ImGui::End();
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

    // Render scene into FBO
    glBindFramebuffer(GL_FRAMEBUFFER, m_ViewportFBO);
    glViewport(0, 0, static_cast<GLsizei>(m_ViewportW), static_cast<GLsizei>(m_ViewportH));
    glClearColor(0.10f, 0.10f, 0.14f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);

    Camera* cam = m_Scene ? m_Scene->GetActiveCamera() : nullptr;
    if (cam && m_Renderer) {
        // Update camera aspect ratio to match viewport
        cam->SetPerspective(60.0f,
            static_cast<f32>(m_ViewportW) / static_cast<f32>(m_ViewportH),
            0.1f, 1000.0f);
        m_Renderer->RenderScene(*m_Scene, *cam);
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // Display the FBO colour attachment as an ImGui Image
    ImTextureID texID = static_cast<ImTextureID>(m_ViewportColor);
    ImGui::Image(texID, size, ImVec2(0, 1), ImVec2(1, 0));   // flip Y

    // Viewport click → select object (simple screen-centre pick placeholder)
    if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        PushLog("[Viewport] Click — object picking (placeholder).");
    }

    ImGui::End();
}
// ── AI Generator Panel ───────────────────────────────────────────────────────

void EditorUI::DrawAIGenerator() {
    ImGui::Begin("AI Generator", &m_ShowAIPanel,
        ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoCollapse);

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

        ImGui::EndTabBar();
    }

    ImGui::End();
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
    if (!m_Scene) return;
    static i32 cubeN = 0;
    cubeN++;
    std::string name = "Cube_" + std::to_string(cubeN);
    auto* obj = m_Scene->CreateGameObject(name);
    obj->GetTransform().SetPosition(0, 2.0f, 0);
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
    PushLog("[Editor] Added '" + name + "'.");
}

void EditorUI::AddLight() {
    if (!m_Scene) return;
    static i32 lightN = 0;
    lightN++;
    std::string name = "PointLight_" + std::to_string(lightN);
    auto* obj = m_Scene->CreateGameObject(name);
    obj->GetTransform().SetPosition(0, 3.0f, 0);
    auto* pl = obj->AddComponent<PointLight>();
    pl->colour = Vec3(1, 1, 1);
    pl->intensity = 1.0f;
    m_Selected = obj;
    PushLog("[Editor] Added '" + name + "'.");
}

void EditorUI::DeleteSelected() {
    if (!m_Scene || !m_Selected) return;
    std::string name = m_Selected->GetName();
    m_Scene->DestroyGameObject(m_Selected);
    m_Selected = nullptr;
    PushLog("[Editor] Deleted '" + name + "'.");
}

void EditorUI::SaveScene(const std::string& path) {
    // Minimal serialisation: dump object list
    PushLog("[Editor] Save scene to '" + path + "' (placeholder).");
}

void EditorUI::LoadScene(const std::string& path) {
    PushLog("[Editor] Load scene from '" + path + "' (placeholder).");
}

} // namespace gv

#endif // GV_HAS_GLFW
