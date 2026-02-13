// ============================================================================
// GameVoid Engine — Engine Bootstrap Implementation
// ============================================================================
#include "core/Engine.h"
#include "core/FPSCamera.h"
#include "physics/Physics.h"
#include "renderer/Camera.h"
#include "renderer/Lighting.h"
#include "renderer/MeshRenderer.h"
#include "renderer/MaterialComponent.h"
#include "scripting/NativeScript.h"
#ifdef GV_HAS_GLFW
#include "core/GLDefs.h"
#endif
#include <chrono>
#include <cstdlib>   // rand()

namespace gv {

bool Engine::Init(const EngineConfig& config) {
    m_Config = config;
    GV_LOG_INFO("========================================");
    GV_LOG_INFO("  GameVoid Engine v0.1.0 — Initialising");
    GV_LOG_INFO("========================================");

    // ── Window (only in non-editor / window mode, or GUI editor) ───────────────
    if (!config.enableEditor || config.enableEditorGUI) {
        if (!m_Window.Init(config.windowWidth, config.windowHeight, config.windowTitle)) {
            GV_LOG_FATAL("Failed to create window.");
            return false;
        }
    }

    // ── Renderer ─────────────────────────────────────────────────────────────────────
    m_Renderer = MakeUnique<OpenGLRenderer>();
    // Give the renderer access to the Window (it needs the GL context)
    if (m_Window.IsInitialised()) {
        static_cast<OpenGLRenderer*>(m_Renderer.get())->SetWindow(&m_Window);
    }
    if (!m_Renderer->Init(config.windowWidth, config.windowHeight, config.windowTitle)) {
        GV_LOG_FATAL("Failed to initialise renderer.");
        return false;
    }

    // ── Scene ──────────────────────────────────────────────────────────────
    Scene* defaultScene = m_SceneManager.CreateScene("Default");

    // Create a default camera
    auto* camObj = defaultScene->CreateGameObject("MainCamera");
    camObj->GetTransform().SetPosition(0, 1.5f, 5);   // slightly above, 5 units back
    auto* cam = camObj->AddComponent<Camera>();
    cam->SetPerspective(60.0f,
        static_cast<f32>(config.windowWidth) / static_cast<f32>(config.windowHeight),
        0.1f, 1000.0f);
    defaultScene->SetActiveCamera(cam);
    camObj->AddComponent<FPSCameraController>();   // FPS movement + mouse-look
    GV_LOG_INFO("[StartupScene] Camera at (0, 1.5, 5) with FPSCameraController.");

    // Create a default directional light
    auto* lightObj = defaultScene->CreateGameObject("DirectionalLight");
    auto* dirLight = lightObj->AddComponent<DirectionalLight>();
    dirLight->direction = Vec3(-0.3f, -1.0f, -0.5f);  // from upper-right-front
    dirLight->intensity = 0.9f;

    // Ambient light
    auto* ambientObj = defaultScene->CreateGameObject("AmbientLight");
    ambientObj->AddComponent<AmbientLight>();

    // ── Default visible objects (so the user sees something immediately) ────
    // 1. A coloured cube that falls from height
    auto* cubeObj = defaultScene->CreateGameObject("DefaultCube");
    cubeObj->GetTransform().SetPosition(0.0f, 3.0f, 0.0f);
    auto* cubeMR = cubeObj->AddComponent<MeshRenderer>();
    cubeMR->primitiveType = PrimitiveType::Cube;
    cubeMR->color = Vec4(0.25f, 0.6f, 1.0f, 1.0f);   // sky-blue
    auto* cubeMat = cubeObj->AddComponent<MaterialComponent>();
    cubeMat->albedo = cubeMR->color;
    cubeMat->metallic = 0.2f;
    cubeMat->roughness = 0.5f;
    auto* cubeRB = cubeObj->AddComponent<RigidBody>();
    cubeRB->useGravity = true;
    cubeObj->AddComponent<Collider>()->type = ColliderType::Box;
    m_Physics.RegisterBody(cubeRB);
    GV_LOG_INFO("[StartupScene] Created 'DefaultCube' at (0, 3, 0) with RigidBody.");

    // 2. A flat triangle offset to the left
    auto* triObj = defaultScene->CreateGameObject("DefaultTriangle");
    triObj->GetTransform().SetPosition(-2.5f, 0.0f, 0.0f);
    auto* triMR = triObj->AddComponent<MeshRenderer>();
    triMR->primitiveType = PrimitiveType::Triangle;
    triMR->color = Vec4(1.0f, 0.4f, 0.1f, 1.0f);     // orange
    GV_LOG_INFO("[StartupScene] Created 'DefaultTriangle' at (-2.5, 0, 0) (orange).");

    // 3. A second cube that falls from even higher (near the first to collide)
    auto* cube2Obj = defaultScene->CreateGameObject("RotatedCube");
    cube2Obj->GetTransform().SetPosition(0.3f, 6.0f, 0.0f);
    cube2Obj->GetTransform().SetScale(0.75f);
    auto* cube2MR = cube2Obj->AddComponent<MeshRenderer>();
    cube2MR->primitiveType = PrimitiveType::Cube;
    cube2MR->color = Vec4(0.2f, 0.9f, 0.3f, 1.0f);   // green
    auto* cube2RB = cube2Obj->AddComponent<RigidBody>();
    cube2RB->useGravity = true;
    cube2Obj->AddComponent<Collider>()->type = ColliderType::Box;
    m_Physics.RegisterBody(cube2RB);
    GV_LOG_INFO("[StartupScene] Created 'RotatedCube' at (0.3, 6, 0) with RigidBody.");

    // 4. Ground plane (Plane primitive with Static RigidBody + Collider)
    auto* floorObj = defaultScene->CreateGameObject("Floor");
    floorObj->GetTransform().SetPosition(0.0f, 0.0f, 0.0f);
    floorObj->GetTransform().SetScale(40.0f, 1.0f, 40.0f);
    auto* floorMR = floorObj->AddComponent<MeshRenderer>();
    floorMR->primitiveType = PrimitiveType::Plane;
    floorMR->color = Vec4(0.4f, 0.4f, 0.42f, 1.0f);
    auto* floorRB = floorObj->AddComponent<RigidBody>();
    floorRB->bodyType = RigidBodyType::Static;
    floorRB->useGravity = false;
    auto* floorCol = floorObj->AddComponent<Collider>();
    floorCol->type = ColliderType::Box;
    floorCol->boxHalfExtents = Vec3(0.5f, 0.01f, 0.5f);  // scaled by transform
    m_Physics.RegisterBody(floorRB);
    GV_LOG_INFO("[StartupScene] Created ground plane with physics.");

    // ── Physics ────────────────────────────────────────────────────────────
    if (config.enablePhysics) {
        m_Physics.Init();
    }

    // ── Scripting ──────────────────────────────────────────────────────────
    if (config.enableScripting) {
        m_Scripting.Init();
        m_Scripting.BindSceneAPI(*defaultScene);
    }

    // ── AI ─────────────────────────────────────────────────────────────────
    if (config.enableAI) {
        // Use provided key or try loading from config file
        if (!config.geminiAPIKey.empty()) {
            m_AI.Init(config.geminiAPIKey);
        } else {
            m_AI.Init("");  // will try loading from config file
        }
        GV_LOG_INFO("AI module configured (key " + std::string(m_AI.IsReady() ? "present" : "absent") + ").");
    }

    // ── Register built-in C++ behaviors ────────────────────────────────────
    RegisterBuiltinBehaviors();
    GV_LOG_INFO("Registered " + std::to_string(BehaviorRegistry::Instance().GetNames().size()) + " built-in behaviors.");

    // ── Input / Audio (placeholders) ───────────────────────────────────────
    m_Input.Init();
    m_Audio.Init();

    // ── Editor ─────────────────────────────────────────────────────────────
    if (config.enableEditor && !config.enableEditorGUI) {
        m_Editor.Init(defaultScene, &m_Physics, &m_AI, &m_Scripting, &m_Assets);
    }

#ifdef GV_HAS_GLFW
    // ── GUI Editor (ImGui) ────────────────────────────────────────────────
    if (config.enableEditorGUI) {
        auto* glr = static_cast<OpenGLRenderer*>(m_Renderer.get());
        m_EditorUI.Init(&m_Window, glr, defaultScene,
                        &m_Physics, &m_AI, &m_Scripting, &m_Assets);
    }
#endif

    GV_LOG_INFO("Engine initialisation complete.\n");
    m_Running = true;

    // Capture mouse for FPS-style look (Tab to toggle later)
    // In GUI editor mode, leave cursor free for ImGui interaction
    if (m_Window.IsInitialised() && !config.enableEditorGUI) {
        m_Window.SetCursorCaptured(true);
        GV_LOG_INFO("Cursor captured for FPS camera. Press Tab to toggle.");
    }

    return true;
}

void Engine::Run() {
    if (!m_Running) {
        GV_LOG_ERROR("Engine::Run() called before successful Init().");
        return;
    }

    Scene* scene = m_SceneManager.GetActiveScene();
    if (!scene) {
        GV_LOG_ERROR("No active scene — cannot run.");
        return;
    }

    // Start all objects once
    scene->Start();

    // If CLI editor mode, run the CLI instead of a real-time loop
    if (m_Config.enableEditor && !m_Config.enableEditorGUI) {
        m_Editor.Run();
        return;
    }

#ifdef GV_HAS_GLFW
    // ── GUI Editor loop (ImGui) ────────────────────────────────────────────
    if (m_Config.enableEditorGUI) {
        using Clock = std::chrono::high_resolution_clock;
        auto lastTime = Clock::now();

        GV_LOG_INFO("Entering GUI editor loop.  Use panels to interact.");

        while (m_Running && !m_Window.ShouldClose()) {
            auto now = Clock::now();
            f32 dt = std::chrono::duration<f32>(now - lastTime).count();
            lastTime = now;
            if (dt > 0.1f) dt = 0.1f;   // clamp stalls

            // ── Input ──────────────────────────────────────────────────
            m_Window.BeginFrame();
            m_Window.PollEvents();
            m_Input.Update();

            if (m_Window.IsKeyDown(GVKey::Escape)) { m_Window.SetShouldClose(true); break; }

            // FPS camera only when cursor is captured (disabled in editor — orbit cam handles it)
            // In editor-gui mode the orbit camera in EditorUI handles all camera movement.

            // Physics (when playing)
            if (m_Config.enablePhysics && m_EditorUI.IsPlaying())
                m_Physics.Step(dt);

            scene->Update(dt);

            // ── Audio update ───────────────────────────────────────────
            m_Audio.Update();

            // ── ImGui frame (scene is rendered to FBO inside DrawViewport) ──
            m_EditorUI.BeginFrame();
            m_EditorUI.Render(dt);
            m_EditorUI.EndFrame();

            // ── Present ────────────────────────────────────────────────
            // Restore default framebuffer viewport to full window before swap
            glViewport(0, 0, static_cast<GLsizei>(m_Window.GetWidth()),
                               static_cast<GLsizei>(m_Window.GetHeight()));
            m_Window.SwapBuffers();
        }

        m_EditorUI.Shutdown();
        return;
    }
#endif

    // ── Real-time game loop (window mode) ──────────────────────────────────
    using Clock = std::chrono::high_resolution_clock;
    auto lastTime  = Clock::now();
    f32  fpsTimer  = 0.0f;
    i32  fpsCount  = 0;

    // Background colour (adjustable with arrow keys)
    f32 bgR = 0.10f, bgG = 0.10f, bgB = 0.14f;

    // Cube-spawn counter
    i32 spawnCount = 0;

    GV_LOG_INFO("Entering window loop.  WASD=move  Mouse=look  E=spawn  L=light  Tab=cursor  Esc=quit");

    while (m_Running && !m_Window.ShouldClose()) {
        auto now = Clock::now();
        f32 dt = std::chrono::duration<f32>(now - lastTime).count();
        lastTime = now;

        // ── Input ──────────────────────────────────────────────────────
        m_Window.BeginFrame();
        m_Window.PollEvents();
        m_Input.Update();

        // Escape to close
        if (m_Window.IsKeyDown(GVKey::Escape)) {
            m_Window.SetShouldClose(true);
            break;
        }

        // Tab toggles cursor capture (so you can Alt-Tab / use the OS)
        if (m_Window.IsKeyPressed(GVKey::Tab)) {
            bool next = !m_Window.IsCursorCaptured();
            m_Window.SetCursorCaptured(next);
            GV_LOG_INFO(next ? "Cursor captured." : "Cursor released.");
        }

        // L toggles Phong lighting
        if (m_Window.IsKeyPressed(GVKey::L)) {
#ifdef GV_HAS_GLFW
            auto* glr = static_cast<OpenGLRenderer*>(m_Renderer.get());
            glr->SetLightingEnabled(!glr->IsLightingEnabled());
            GV_LOG_INFO(glr->IsLightingEnabled() ? "Lighting ON." : "Lighting OFF.");
#endif
        }

        // Arrow keys adjust background colour
        const f32 speed = 0.5f * dt;
        if (m_Window.IsKeyDown(GVKey::Up))    bgG += speed;
        if (m_Window.IsKeyDown(GVKey::Down))  bgG -= speed;
        if (m_Window.IsKeyDown(GVKey::Right)) bgR += speed;
        if (m_Window.IsKeyDown(GVKey::Left))  bgB += speed;
        if (m_Window.IsKeyPressed(GVKey::Space)) { bgR = 0.10f; bgG = 0.10f; bgB = 0.14f; }
        if (bgR < 0) bgR = 0; if (bgR > 1) bgR = 1;
        if (bgG < 0) bgG = 0; if (bgG > 1) bgG = 1;
        if (bgB < 0) bgB = 0; if (bgB > 1) bgB = 1;

        // ── FPS Camera update ──────────────────────────────────────────
        Camera* activeCam = scene->GetActiveCamera();
        if (activeCam) {
            auto* fps = activeCam->GetOwner()->GetComponent<FPSCameraController>();
            if (fps) fps->UpdateFromInput(m_Window, dt);
        }

        // ── E-key: spawn a cube at camera position ─────────────────────
        if (m_Window.IsKeyPressed(GVKey::E) && activeCam) {
            spawnCount++;
            Vec3 pos = activeCam->GetOwner()->GetTransform().position;

            std::string name = "SpawnedCube_" + std::to_string(spawnCount);
            auto* obj = scene->CreateGameObject(name);
            obj->GetTransform().SetPosition(pos.x, pos.y, pos.z);
            obj->GetTransform().SetScale(0.5f);        // half-size cubes

            auto* mr = obj->AddComponent<MeshRenderer>();
            mr->primitiveType = PrimitiveType::Cube;
            // Random colour per cube
            mr->color = Vec4(
                0.3f + static_cast<f32>(std::rand() % 70) / 100.0f,
                0.3f + static_cast<f32>(std::rand() % 70) / 100.0f,
                0.3f + static_cast<f32>(std::rand() % 70) / 100.0f,
                1.0f);

            // Physics — spawned cube falls and collides
            auto* spawnRB = obj->AddComponent<RigidBody>();
            spawnRB->useGravity = true;
            obj->AddComponent<Collider>()->type = ColliderType::Box;
            m_Physics.RegisterBody(spawnRB);

            GV_LOG_INFO("[Spawn] " + name + " at ("
                + std::to_string(pos.x) + ", "
                + std::to_string(pos.y) + ", "
                + std::to_string(pos.z) + ")");
        }

        // ── Physics ────────────────────────────────────────────────────
        if (m_Config.enablePhysics) m_Physics.Step(dt);

        // ── Logic ──────────────────────────────────────────────────────
        scene->Update(dt);
        m_Audio.Update();

        // ── Render ─────────────────────────────────────────────────────
        m_Renderer->Clear(bgR, bgG, bgB, 1.0f);
        m_Renderer->BeginFrame();

        // Draw built-in demo triangle (proves GL works)
#ifdef GV_HAS_GLFW
        static_cast<OpenGLRenderer*>(m_Renderer.get())->RenderDemo(dt);
#endif

        if (scene->GetActiveCamera())
            m_Renderer->RenderScene(*scene, *scene->GetActiveCamera());
        scene->Render();
        m_Renderer->EndFrame();

        // ── FPS counter in title bar ───────────────────────────────────
        fpsCount++;
        fpsTimer += dt;
        if (fpsTimer >= 1.0f) {
            std::string title = "GameVoid Engine — " + std::to_string(fpsCount) + " FPS";
            m_Window.SetTitle(title);
            fpsCount = 0;
            fpsTimer = 0.0f;
        }
    }
}

void Engine::Shutdown() {
    GV_LOG_INFO("Engine shutting down...");

    m_Scripting.Shutdown();
    m_Physics.Shutdown();
    m_Audio.Shutdown();
    m_Assets.Clear();
    if (m_Renderer) m_Renderer->Shutdown();
    m_Window.Shutdown();

    m_Running = false;
    GV_LOG_INFO("Engine shut down successfully.");
}

} // namespace gv
