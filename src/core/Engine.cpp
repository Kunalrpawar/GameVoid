// ============================================================================
// GameVoid Engine — Engine Bootstrap Implementation
// ============================================================================
#include "core/Engine.h"
#include "renderer/Camera.h"
#include "renderer/Lighting.h"
#include <chrono>

namespace gv {

bool Engine::Init(const EngineConfig& config) {
    m_Config = config;
    GV_LOG_INFO("========================================");
    GV_LOG_INFO("  GameVoid Engine v0.1.0 — Initialising");
    GV_LOG_INFO("========================================");

    // ── Window (only in non-editor / window mode) ────────────────────────────────
    if (!config.enableEditor) {
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
    camObj->GetTransform().SetPosition(0, 2, 10);
    auto* cam = camObj->AddComponent<Camera>();
    cam->SetPerspective(60.0f,
        static_cast<f32>(config.windowWidth) / static_cast<f32>(config.windowHeight),
        0.1f, 1000.0f);
    defaultScene->SetActiveCamera(cam);

    // Create a default directional light
    auto* lightObj = defaultScene->CreateGameObject("DirectionalLight");
    lightObj->GetTransform().SetEulerDeg(-45.0f, -30.0f, 0.0f);
    lightObj->AddComponent<DirectionalLight>();

    // Ambient light
    auto* ambientObj = defaultScene->CreateGameObject("AmbientLight");
    ambientObj->AddComponent<AmbientLight>();

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
    if (config.enableAI && !config.geminiAPIKey.empty()) {
        m_AI.SetAPIKey(config.geminiAPIKey);
        GV_LOG_INFO("AI module configured with Gemini API key.");
    }

    // ── Input / Audio (placeholders) ───────────────────────────────────────
    m_Input.Init();
    m_Audio.Init();

    // ── Editor ─────────────────────────────────────────────────────────────
    if (config.enableEditor) {
        m_Editor.Init(defaultScene, &m_Physics, &m_AI, &m_Scripting, &m_Assets);
    }

    GV_LOG_INFO("Engine initialisation complete.\n");
    m_Running = true;
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

    // If editor mode, run the CLI instead of a real-time loop
    if (m_Config.enableEditor) {
        m_Editor.Run();
        return;
    }

    // ── Real-time game loop (window mode) ──────────────────────────────────
    using Clock = std::chrono::high_resolution_clock;
    auto lastTime  = Clock::now();
    f32  fpsTimer  = 0.0f;
    i32  fpsCount  = 0;

    // Background colour (adjustable with arrow keys)
    f32 bgR = 0.10f, bgG = 0.10f, bgB = 0.14f;

    GV_LOG_INFO("Entering window loop.  Escape=quit, Arrows=change bg colour.");

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

        // ── Physics ────────────────────────────────────────────────────
        if (m_Config.enablePhysics) m_Physics.Step(dt);

        // ── Logic ──────────────────────────────────────────────────────
        scene->Update(dt);

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
