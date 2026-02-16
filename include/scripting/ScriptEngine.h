// ============================================================================
// GameVoid Engine — Scripting Module (Embedded Interpreter)
// ============================================================================
// Provides a lightweight embedded scripting system so designers can attach
// scripts to GameObjects without recompiling C++.
//
// The engine includes a built-in mini scripting language ("GVScript") with:
//   - Variables (numbers, strings, booleans)
//   - Functions: print(), set_position(), get_position(), spawn(), destroy()
//   - Control flow: if/else, while, for
//   - Math: sin, cos, sqrt, abs, random
//   - Engine bindings: access to scene and game objects
//
// Scripts can be loaded from .gvs files or written inline.
// ============================================================================
#pragma once

#include "core/Component.h"
#include "core/Types.h"
#include <string>
#include <unordered_map>
#include <functional>
#include <vector>

namespace gv {

// Forward declarations
class Scene;
class GameObject;
class ScriptEngine;

// ============================================================================
// Script Value — variant type for script variables
// ============================================================================
struct ScriptValue {
    enum Type { Nil, Number, String, Bool };
    Type type = Nil;
    f64 numberVal = 0;
    std::string stringVal;
    bool boolVal = false;

    ScriptValue() : type(Nil) {}
    ScriptValue(f64 n) : type(Number), numberVal(n) {}
    ScriptValue(const std::string& s) : type(String), stringVal(s) {}
    ScriptValue(bool b) : type(Bool), boolVal(b) {}

    f64 AsNumber() const { return (type == Number) ? numberVal : 0; }
    std::string AsString() const;
    bool AsBool() const;
    bool IsNil() const { return type == Nil; }
};

// ============================================================================
// Script Component
// ============================================================================
/// Attach one of these to a GameObject to run a script each frame.
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

    /// Bind the ScriptEngine so this component can execute scripts.
    void SetEngine(ScriptEngine* engine) { m_Engine = engine; }
    ScriptEngine* GetEngine() const      { return m_Engine; }

    // ── Lifecycle (called by ScriptEngine) ─────────────────────────────────
    void OnStart() override;
    void OnUpdate(f32 dt) override;
    void OnDetach() override;

private:
    std::string m_ScriptPath;
    std::string m_Source;
    bool m_Loaded = false;
    ScriptEngine* m_Engine = nullptr;
};

// ============================================================================
// Script Engine — Embedded Interpreter
// ============================================================================
/// Manages script execution, registers C++ bindings, and dispatches
/// script callbacks.
class ScriptEngine {
public:
    ScriptEngine() = default;
    ~ScriptEngine() = default;

    // ── Lifecycle ──────────────────────────────────────────────────────────
    /// Initialise the scripting engine and register built-in functions.
    bool Init();

    /// Shut down and free resources.
    void Shutdown();

    // ── Script execution ───────────────────────────────────────────────────
    /// Load and execute a script file.
    bool LoadFile(const std::string& path);

    /// Execute an inline script string.
    bool Execute(const std::string& source);

    /// Call a named function with no arguments.
    bool CallFunction(const std::string& funcName);

    /// Call a named function with a float argument (e.g. "on_update").
    bool CallFunction(const std::string& funcName, f32 arg);

    // ── Engine bindings ────────────────────────────────────────────────────
    /// Expose a C++ function to scripts.
    using NativeFunc = std::function<ScriptValue(const std::vector<ScriptValue>&)>;
    void RegisterFunction(const std::string& name, NativeFunc func);

    /// Expose the Scene API so scripts can spawn/query objects.
    void BindSceneAPI(Scene& scene);

    /// Expose the GameObject API (get/set transform, add component, etc.).
    void BindGameObjectAPI();

    /// Expose the Event API (on, emit, off) so scripts can use events.
    void BindEventAPI();

    /// Set the "self" object for script execution context.
    void SetSelfObject(GameObject* obj) { m_SelfObject = obj; }
    GameObject* GetSelfObject() const { return m_SelfObject; }

    // ── Variable access ────────────────────────────────────────────────────
    void SetVariable(const std::string& name, ScriptValue val);
    ScriptValue GetVariable(const std::string& name) const;

    // ── Hot-reload ─────────────────────────────────────────────────────────
    void EnableHotReload(bool enable);

    /// Get the last error message from script execution.
    const std::string& GetLastError() const { return m_LastError; }

    bool IsInitialised() const { return m_Initialised; }

private:
    // ── Internal interpreter ───────────────────────────────────────────────
    struct Token {
        enum Kind {
            Eof, Number, Str, Ident, LParen, RParen, LBrace, RBrace,
            Comma, Semicolon, Assign, Plus, Minus, Star, Slash, Percent,
            Eq, Neq, Lt, Gt, Lte, Gte, And, Or, Not, Dot,
            If, Else, While, For, Func, Return, Var, True_, False_
        };
        Kind kind = Eof;
        std::string text;
        f64 numVal = 0;
        i32 line = 0;
    };

    std::vector<Token> Tokenize(const std::string& source);
    ScriptValue ExecuteTokens(const std::vector<Token>& tokens, size_t& pos);
    ScriptValue ExecuteBlock(const std::vector<Token>& tokens, size_t& pos);
    ScriptValue ExecuteStatement(const std::vector<Token>& tokens, size_t& pos);
    ScriptValue ExecuteExpression(const std::vector<Token>& tokens, size_t& pos);
    ScriptValue ExecuteComparison(const std::vector<Token>& tokens, size_t& pos);
    ScriptValue ExecuteAddSub(const std::vector<Token>& tokens, size_t& pos);
    ScriptValue ExecuteMulDiv(const std::vector<Token>& tokens, size_t& pos);
    ScriptValue ExecuteUnary(const std::vector<Token>& tokens, size_t& pos);
    ScriptValue ExecutePrimary(const std::vector<Token>& tokens, size_t& pos);
    ScriptValue CallNative(const std::string& name, const std::vector<ScriptValue>& args);

    bool m_Initialised = false;
    bool m_HotReload   = false;
    std::string m_LastError;
    std::unordered_map<std::string, ScriptValue> m_Variables;
    std::unordered_map<std::string, NativeFunc> m_NativeFunctions;
    Scene* m_BoundScene = nullptr;
    GameObject* m_SelfObject = nullptr;

    // User-defined script functions
    struct ScriptFunc {
        std::string name;
        std::vector<std::string> params;
        std::vector<Token> body;
    };
    std::unordered_map<std::string, ScriptFunc> m_ScriptFunctions;
};

} // namespace gv
