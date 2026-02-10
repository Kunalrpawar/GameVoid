// ============================================================================
// GameVoid Engine — Scripting Module (Lua)
// ============================================================================
// Embeds a Lua interpreter so designers can attach scripts to GameObjects
// without recompiling C++.  The skeleton defines the ScriptComponent and
// the ScriptEngine that manages the Lua state.
//
// In production you would link against sol2 + Lua 5.4.
// A Python variant could be swapped in via the same interface.
// ============================================================================
#pragma once

#include "core/Component.h"
#include "core/Types.h"
#include <string>
#include <unordered_map>
#include <functional>

namespace gv {

// Forward declarations
class Scene;
class GameObject;

// ============================================================================
// Script Component
// ============================================================================
/// Attach one of these to a GameObject to run a Lua (or Python) script each
/// frame.  The script file is loaded by ScriptEngine and receives callbacks.
class ScriptComponent : public Component {
public:
    ScriptComponent() = default;
    explicit ScriptComponent(const std::string& scriptPath)
        : m_ScriptPath(scriptPath) {}

    ~ScriptComponent() override = default;

    std::string GetTypeName() const override { return "ScriptComponent"; }

    // ── Configuration ──────────────────────────────────────────────────────
    void SetScriptPath(const std::string& path) { m_ScriptPath = path; }
    const std::string& GetScriptPath() const    { return m_ScriptPath; }

    /// Inline source code (alternative to a file path).
    void SetSource(const std::string& source) { m_Source = source; }
    const std::string& GetSource() const      { return m_Source; }

    // ── Lifecycle (called by ScriptEngine) ─────────────────────────────────
    void OnStart() override;            // load & execute "on_start()"
    void OnUpdate(f32 dt) override;     // execute "on_update(dt)"
    void OnDetach() override;           // clean up Lua references

private:
    std::string m_ScriptPath;           // path to .lua / .py file
    std::string m_Source;               // inline source (takes priority if non-empty)
    bool m_Loaded = false;
};

// ============================================================================
// Script Engine
// ============================================================================
/// Manages the global Lua state, registers C++ bindings, and dispatches
/// script callbacks.
class ScriptEngine {
public:
    ScriptEngine() = default;
    ~ScriptEngine() = default;

    // ── Lifecycle ──────────────────────────────────────────────────────────
    /// Create the Lua state and register engine bindings.
    bool Init();

    /// Destroy the Lua state and free resources.
    void Shutdown();

    // ── Script execution ───────────────────────────────────────────────────
    /// Load a Lua file (registers its functions in the state).
    bool LoadFile(const std::string& path);

    /// Execute an inline Lua string.
    bool Execute(const std::string& source);

    /// Call a named global function with no arguments.
    bool CallFunction(const std::string& funcName);

    /// Call a named global function with a float argument (e.g. "on_update").
    bool CallFunction(const std::string& funcName, f32 arg);

    // ── Engine bindings (registered once in Init) ──────────────────────────
    /// Expose a C++ function to Lua under the given name.
    void RegisterFunction(const std::string& luaName, std::function<void()> func);

    /// Expose the Scene API so scripts can spawn/query objects.
    void BindSceneAPI(Scene& scene);

    /// Expose the GameObject API (get/set transform, add component, etc.).
    void BindGameObjectAPI();

    // ── Hot-reload (placeholder) ───────────────────────────────────────────
    /// Watch script files for changes and reload when they are modified.
    void EnableHotReload(bool enable);

private:
    // In a real build: lua_State* m_LuaState = nullptr;
    bool m_Initialised = false;
    bool m_HotReload   = false;
    std::unordered_map<std::string, std::function<void()>> m_RegisteredFunctions;
};

} // namespace gv
