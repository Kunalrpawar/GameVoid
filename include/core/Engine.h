// ============================================================================
// GameVoid Engine — Engine Bootstrap
// ============================================================================
// Top-level Engine class that owns and wires all subsystems together.
// ============================================================================
#pragma once

#include "core/Types.h"
#include "core/Scene.h"
#include "renderer/Renderer.h"
#include "physics/Physics.h"
#include "assets/Assets.h"
#include "ai/AIManager.h"
#include "scripting/ScriptEngine.h"
#include "editor/CLIEditor.h"
#ifdef GV_HAS_GLFW
#include "editor/EditorUI.h"
#endif
#include "core/Window.h"
#include "future/Placeholders.h"
#include "animation/Animation.h"
#include <string>

namespace gv {

/// Configuration passed to Engine::Init().
struct EngineConfig {
    std::string windowTitle = "GameVoid Engine";
    u32 windowWidth  = 1280;
    u32 windowHeight = 720;
    bool enableEditor    = true;
    bool enableEditorGUI = false;   // --editor-gui: Dear ImGui graphical editor
    bool enablePhysics   = true;
    bool enableScripting = true;
    bool enableAI        = true;
    std::string geminiAPIKey;       // optional – set via editor or config file
};

/// The root object that boots every subsystem and runs the main loop.
class Engine {
public:
    Engine() = default;
    ~Engine() = default;

    // Non-copyable, non-movable (singleton)
    Engine(const Engine&) = delete;
    Engine& operator=(const Engine&) = delete;
    Engine(Engine&&) = delete;
    Engine& operator=(Engine&&) = delete;

    // ── Lifecycle ──────────────────────────────────────────────────────────
    /// Initialise all subsystems.  Returns false on critical failure.
    bool Init(const EngineConfig& config = {});

    /// Enter the main game loop (blocks until the window closes or the user
    /// exits the CLI editor).
    void Run();

    /// Gracefully shut down all subsystems and release resources.
    void Shutdown();

    // ── Subsystem access ───────────────────────────────────────────────────
    IRenderer*     GetRenderer()     { return m_Renderer.get(); }
    SceneManager&  GetSceneManager() { return m_SceneManager; }
    PhysicsWorld&  GetPhysics()      { return m_Physics; }
    AssetManager&  GetAssets()       { return m_Assets; }
    AIManager&     GetAI()           { return m_AI; }
    ScriptEngine&  GetScripting()    { return m_Scripting; }
    CLIEditor&     GetEditor()       { return m_Editor; }
    Window&        GetWindow()       { return m_Window; }
    InputManager&  GetInput()        { return m_Input; }
    AudioEngine&   GetAudio()        { return m_Audio; }

    /// Get the singleton instance (created on first call).
    static Engine& Instance() {
        static Engine instance;
        return instance;
    }

private:
    // ── Subsystems ─────────────────────────────────────────────────────────
    Unique<IRenderer> m_Renderer;
    SceneManager      m_SceneManager;
    PhysicsWorld      m_Physics;
    AssetManager      m_Assets;
    AIManager         m_AI;
    ScriptEngine      m_Scripting;
    CLIEditor         m_Editor;
#ifdef GV_HAS_GLFW
    EditorUI          m_EditorUI;
#endif
    Window            m_Window;
    InputManager      m_Input;
    AudioEngine       m_Audio;

    EngineConfig      m_Config;
    bool              m_Running = false;
};

} // namespace gv
