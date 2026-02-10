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
#include "physics/Physics.h"

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
    ImGui::SetNextWindowPos(ImVec2(startX, panelY));
    ImGui::SetNextWindowSize(ImVec2(sideW, panelH));
    DrawHierarchy();

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

    // Console — bottom
    ImGui::SetNextWindowPos(ImVec2(startX, panelY + panelH));
    ImGui::SetNextWindowSize(ImVec2(winW, consoleH));
    DrawConsole();

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
            if (ImGui::MenuItem("Delete Selected")) { DeleteSelected(); }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("View")) {
            ImGui::MenuItem("ImGui Demo", nullptr, &m_ShowDemo);
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("AI")) {
            if (ImGui::MenuItem("Generate Level"))  { PushLog("[AI] Level generation (placeholder)."); }
            if (ImGui::MenuItem("AI Prompt"))       { PushLog("[AI] Prompt (placeholder)."); }
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

            ImGui::Separator();
        }
    }

    ImGui::End();
}

// ── Console Panel ──────────────────────────────────────────────────────────

void EditorUI::DrawConsole() {
    ImGui::Begin("Console");

    if (ImGui::Button("Clear")) s_Logs.clear();
    ImGui::SameLine();
    ImGui::TextDisabled("(%d lines)", static_cast<int>(s_Logs.size()));
    ImGui::Separator();

    ImGui::BeginChild("ConsoleScrollRegion", ImVec2(0, 0), false,
                       ImGuiWindowFlags_HorizontalScrollbar);
    for (auto& line : s_Logs) {
        // Colour-code by level
        if (line.find("[ERROR]") != std::string::npos ||
            line.find("[FATAL]") != std::string::npos) {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1, 0.3f, 0.3f, 1));
            ImGui::TextUnformatted(line.c_str());
            ImGui::PopStyleColor();
        } else if (line.find("[WARN]") != std::string::npos) {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1, 0.8f, 0.2f, 1));
            ImGui::TextUnformatted(line.c_str());
            ImGui::PopStyleColor();
        } else if (line.find("[DEBUG]") != std::string::npos) {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.7f, 1.0f, 1));
            ImGui::TextUnformatted(line.c_str());
            ImGui::PopStyleColor();
        } else {
            ImGui::TextUnformatted(line.c_str());
        }
    }
    // Auto-scroll to bottom
    if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 10.0f)
        ImGui::SetScrollHereY(1.0f);

    ImGui::EndChild();
    ImGui::End();
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
