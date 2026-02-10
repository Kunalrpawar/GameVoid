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

    // ── Renderer ───────────────────────────────────────────────────────────
    m_Renderer = MakeUnique<OpenGLRenderer>();
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

    // ── Real-time game loop ────────────────────────────────────────────────
    using Clock = std::chrono::high_resolution_clock;
    auto lastTime = Clock::now();

    while (m_Running && !m_Renderer->WindowShouldClose()) {
        auto now = Clock::now();
        f32 dt = std::chrono::duration<f32>(now - lastTime).count();
        lastTime = now;

        // Input
        m_Renderer->PollEvents();
        m_Input.Update();

        // Physics
        if (m_Config.enablePhysics) m_Physics.Step(dt);

        // Logic
        scene->Update(dt);

        // Render
        m_Renderer->Clear(0.1f, 0.1f, 0.12f, 1.0f);
        m_Renderer->BeginFrame();
        if (scene->GetActiveCamera())
            m_Renderer->RenderScene(*scene, *scene->GetActiveCamera());
        scene->Render();
        m_Renderer->EndFrame();
    }
}

void Engine::Shutdown() {
    GV_LOG_INFO("Engine shutting down...");

    m_Scripting.Shutdown();
    m_Physics.Shutdown();
    m_Audio.Shutdown();
    m_Assets.Clear();
    if (m_Renderer) m_Renderer->Shutdown();

    m_Running = false;
    GV_LOG_INFO("Engine shut down successfully.");
}

} // namespace gv
