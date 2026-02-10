// ============================================================================
// GameVoid Engine — Script Engine Implementation
// ============================================================================
#include "scripting/ScriptEngine.h"
#include "core/Scene.h"
#include "core/GameObject.h"

namespace gv {

// ── ScriptComponent ────────────────────────────────────────────────────────

void ScriptComponent::OnStart() {
    if (!m_Loaded) {
        // ScriptEngine would load the file / source here and
        // call the Lua function "on_start()".
        GV_LOG_INFO("ScriptComponent::OnStart — " +
                    (m_ScriptPath.empty() ? "(inline source)" : m_ScriptPath));
        m_Loaded = true;
    }
}

void ScriptComponent::OnUpdate(f32 dt) {
    // In production: call the Lua function "on_update(dt)".
    (void)dt;
}

void ScriptComponent::OnDetach() {
    // Clean up Lua references for this script instance.
    m_Loaded = false;
}

// ── ScriptEngine ───────────────────────────────────────────────────────────

bool ScriptEngine::Init() {
    // In production:
    //   m_LuaState = luaL_newstate();
    //   luaL_openlibs(m_LuaState);
    //   BindGameObjectAPI();
    m_Initialised = true;
    GV_LOG_INFO("ScriptEngine initialised (Lua placeholder).");
    return true;
}

void ScriptEngine::Shutdown() {
    // lua_close(m_LuaState);
    m_Initialised = false;
    GV_LOG_INFO("ScriptEngine shut down.");
}

bool ScriptEngine::LoadFile(const std::string& path) {
    if (!m_Initialised) return false;
    // luaL_dofile(m_LuaState, path.c_str());
    GV_LOG_INFO("ScriptEngine — loaded file: " + path);
    return true;
}

bool ScriptEngine::Execute(const std::string& source) {
    if (!m_Initialised) return false;
    // luaL_dostring(m_LuaState, source.c_str());
    GV_LOG_INFO("ScriptEngine — executed inline source (" +
                std::to_string(source.size()) + " bytes).");
    return true;
}

bool ScriptEngine::CallFunction(const std::string& funcName) {
    // lua_getglobal(m_LuaState, funcName.c_str());
    // lua_pcall(m_LuaState, 0, 0, 0);
    (void)funcName;
    return true;
}

bool ScriptEngine::CallFunction(const std::string& funcName, f32 arg) {
    // lua_getglobal(m_LuaState, funcName.c_str());
    // lua_pushnumber(m_LuaState, arg);
    // lua_pcall(m_LuaState, 1, 0, 0);
    (void)funcName; (void)arg;
    return true;
}

void ScriptEngine::RegisterFunction(const std::string& luaName, std::function<void()> func) {
    m_RegisteredFunctions[luaName] = std::move(func);
    // In production: push a C closure into the Lua state.
}

void ScriptEngine::BindSceneAPI(Scene& scene) {
    // Register functions like:
    //   gv.spawn(name) → scene.CreateGameObject(name)
    //   gv.find(name)  → scene.FindByName(name)
    //   gv.destroy(id) → scene.DestroyGameObject(...)
    (void)scene;
    GV_LOG_DEBUG("ScriptEngine — Scene API bound.");
}

void ScriptEngine::BindGameObjectAPI() {
    // Register functions like:
    //   obj:get_position() → Vec3
    //   obj:set_position(x,y,z)
    //   obj:add_component("RigidBody")
    GV_LOG_DEBUG("ScriptEngine — GameObject API bound.");
}

void ScriptEngine::EnableHotReload(bool enable) {
    m_HotReload = enable;
    GV_LOG_INFO(std::string("ScriptEngine — hot reload ") + (enable ? "enabled" : "disabled"));
}

} // namespace gv
