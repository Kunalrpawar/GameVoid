// ============================================================================
// GameVoid Engine — Script Engine Implementation (Embedded Interpreter)
// ============================================================================
#include "scripting/ScriptEngine.h"
#include "core/Scene.h"
#include "core/GameObject.h"
#include "core/EventSystem.h"
#include "renderer/MeshRenderer.h"
#include <fstream>
#include <sstream>
#include <cmath>
#include <cstdlib>
#include <cctype>
#include <algorithm>
#include <chrono>

namespace gv {

// ============================================================================
// ScriptValue helpers
// ============================================================================
std::string ScriptValue::AsString() const {
    switch (type) {
        case Number: { std::ostringstream ss; ss << numberVal; return ss.str(); }
        case String: return stringVal;
        case Bool:   return boolVal ? "true" : "false";
        case Nil:    return "nil";
    }
    return "nil";
}

bool ScriptValue::AsBool() const {
    switch (type) {
        case Number: return numberVal != 0;
        case String: return !stringVal.empty();
        case Bool:   return boolVal;
        case Nil:    return false;
    }
    return false;
}

// ============================================================================
// ScriptComponent
// ============================================================================
void ScriptComponent::OnStart() {
    if (!m_Loaded && m_Engine && m_Engine->IsInitialised()) {
        GV_LOG_INFO("ScriptComponent::OnStart — " +
                    (m_ScriptPath.empty() ? "(inline source)" : m_ScriptPath));

        // Set self-object context so script bindings know which object "self" refers to
        m_Engine->SetSelfObject(GetOwner());

        // Load from file or execute inline source
        if (!m_ScriptPath.empty()) {
            m_Engine->LoadFile(m_ScriptPath);
        } else if (!m_Source.empty()) {
            m_Engine->Execute(m_Source);
        }

        // Call on_start() if the script defines it
        m_Engine->CallFunction("on_start");

        m_Loaded = true;
    }
}

void ScriptComponent::OnUpdate(f32 dt) {
    if (!m_Engine || !m_Engine->IsInitialised()) return;

    // Ensure script is loaded (late-attach)
    if (!m_Loaded) { OnStart(); }

    // Set self-object context for this frame
    m_Engine->SetSelfObject(GetOwner());

    // Call the script's on_update(dt) function
    m_Engine->CallFunction("on_update", dt);
}

void ScriptComponent::OnDetach() { m_Loaded = false; }

// ============================================================================
// ScriptEngine — Init / Shutdown
// ============================================================================
bool ScriptEngine::Init() {
    m_Initialised = true;
    m_Variables.clear();
    m_NativeFunctions.clear();
    m_ScriptFunctions.clear();

    // ── Built-in functions ──────────────────────────────────────────────
    RegisterFunction("print", [](const std::vector<ScriptValue>& args) -> ScriptValue {
        std::string out;
        for (size_t i = 0; i < args.size(); ++i) { if (i) out += " "; out += args[i].AsString(); }
        GV_LOG_INFO("[Script] " + out);
        return {};
    });

    RegisterFunction("sin", [](const std::vector<ScriptValue>& a) -> ScriptValue {
        return a.empty() ? ScriptValue(0.0) : ScriptValue(std::sin(a[0].AsNumber()));
    });
    RegisterFunction("cos", [](const std::vector<ScriptValue>& a) -> ScriptValue {
        return a.empty() ? ScriptValue(0.0) : ScriptValue(std::cos(a[0].AsNumber()));
    });
    RegisterFunction("sqrt", [](const std::vector<ScriptValue>& a) -> ScriptValue {
        return a.empty() ? ScriptValue(0.0) : ScriptValue(std::sqrt(a[0].AsNumber()));
    });
    RegisterFunction("abs", [](const std::vector<ScriptValue>& a) -> ScriptValue {
        return a.empty() ? ScriptValue(0.0) : ScriptValue(std::fabs(a[0].AsNumber()));
    });
    RegisterFunction("floor", [](const std::vector<ScriptValue>& a) -> ScriptValue {
        return a.empty() ? ScriptValue(0.0) : ScriptValue(std::floor(a[0].AsNumber()));
    });
    RegisterFunction("ceil", [](const std::vector<ScriptValue>& a) -> ScriptValue {
        return a.empty() ? ScriptValue(0.0) : ScriptValue(std::ceil(a[0].AsNumber()));
    });
    RegisterFunction("random", [](const std::vector<ScriptValue>& a) -> ScriptValue {
        if (a.size() >= 2) {
            f64 lo = a[0].AsNumber(), hi = a[1].AsNumber();
            return ScriptValue(lo + static_cast<f64>(std::rand()) / RAND_MAX * (hi - lo));
        }
        return ScriptValue(static_cast<f64>(std::rand()) / RAND_MAX);
    });
    RegisterFunction("tostring", [](const std::vector<ScriptValue>& a) -> ScriptValue {
        return a.empty() ? ScriptValue(std::string("nil")) : ScriptValue(a[0].AsString());
    });
    RegisterFunction("tonumber", [](const std::vector<ScriptValue>& a) -> ScriptValue {
        if (a.empty()) return ScriptValue(0.0);
        try { return ScriptValue(std::stod(a[0].AsString())); } catch (...) { return ScriptValue(0.0); }
    });

    GV_LOG_INFO("ScriptEngine initialised (embedded interpreter).");
    return true;
}

void ScriptEngine::Shutdown() {
    m_Variables.clear();
    m_NativeFunctions.clear();
    m_ScriptFunctions.clear();
    m_BoundScene  = nullptr;
    m_SelfObject  = nullptr;
    m_Initialised = false;
    GV_LOG_INFO("ScriptEngine shut down.");
}

// ============================================================================
// Load / Execute
// ============================================================================
bool ScriptEngine::LoadFile(const std::string& path) {
    if (!m_Initialised) return false;
    std::ifstream file(path);
    if (!file.is_open()) {
        m_LastError = "Failed to open script file: " + path;
        GV_LOG_ERROR("ScriptEngine — " + m_LastError);
        return false;
    }
    std::stringstream ss;
    ss << file.rdbuf();
    GV_LOG_INFO("ScriptEngine — loading file: " + path);
    return Execute(ss.str());
}

bool ScriptEngine::Execute(const std::string& source) {
    if (!m_Initialised) return false;
    try {
        auto tokens = Tokenize(source);
        size_t pos = 0;
        while (pos < tokens.size() && tokens[pos].kind != Token::Eof)
            ExecuteStatement(tokens, pos);
        return true;
    } catch (const std::exception& e) {
        m_LastError = std::string("Script error: ") + e.what();
        GV_LOG_ERROR("ScriptEngine — " + m_LastError);
        return false;
    }
}

// ============================================================================
// CallFunction
// ============================================================================
bool ScriptEngine::CallFunction(const std::string& funcName) {
    // Script functions
    auto it = m_ScriptFunctions.find(funcName);
    if (it != m_ScriptFunctions.end()) {
        size_t pos = 0;
        try {
            while (pos < it->second.body.size() && it->second.body[pos].kind != Token::Eof)
                ExecuteStatement(it->second.body, pos);
            return true;
        } catch (const std::exception& e) {
            m_LastError = "Error in " + funcName + ": " + e.what();
            GV_LOG_ERROR("ScriptEngine — " + m_LastError);
            return false;
        }
    }
    // Native functions
    auto nit = m_NativeFunctions.find(funcName);
    if (nit != m_NativeFunctions.end()) { nit->second({}); return true; }
    return false;
}

bool ScriptEngine::CallFunction(const std::string& funcName, f32 arg) {
    auto it = m_ScriptFunctions.find(funcName);
    if (it != m_ScriptFunctions.end()) {
        auto& func = it->second;
        if (!func.params.empty())
            m_Variables[func.params[0]] = ScriptValue(static_cast<f64>(arg));
        m_Variables["dt"] = ScriptValue(static_cast<f64>(arg));
        size_t pos = 0;
        try {
            while (pos < func.body.size() && func.body[pos].kind != Token::Eof)
                ExecuteStatement(func.body, pos);
            return true;
        } catch (const std::exception& e) {
            m_LastError = "Error in " + funcName + ": " + e.what();
            GV_LOG_ERROR("ScriptEngine — " + m_LastError);
            return false;
        }
    }
    auto nit = m_NativeFunctions.find(funcName);
    if (nit != m_NativeFunctions.end()) { nit->second({ ScriptValue(static_cast<f64>(arg)) }); return true; }
    return false;
}

// ============================================================================
// Register / Bind helpers
// ============================================================================
void ScriptEngine::RegisterFunction(const std::string& name, NativeFunc func) {
    m_NativeFunctions[name] = std::move(func);
}

void ScriptEngine::SetVariable(const std::string& name, ScriptValue val) {
    m_Variables[name] = std::move(val);
}

ScriptValue ScriptEngine::GetVariable(const std::string& name) const {
    auto it = m_Variables.find(name);
    return (it != m_Variables.end()) ? it->second : ScriptValue();
}

void ScriptEngine::BindSceneAPI(Scene& scene) {
    m_BoundScene = &scene;

    RegisterFunction("spawn", [this](const std::vector<ScriptValue>& args) -> ScriptValue {
        if (!m_BoundScene || args.empty()) return {};
        std::string nm = args[0].AsString();
        auto* obj = m_BoundScene->CreateGameObject(nm);
        if (obj) {
            auto* mr = obj->AddComponent<MeshRenderer>();
            mr->primitiveType = PrimitiveType::Cube;
            mr->color = Vec4(0.5f, 0.8f, 0.3f, 1.0f);
            GV_LOG_INFO("[Script] Spawned object: " + nm);
            return ScriptValue(static_cast<f64>(obj->GetID()));
        }
        return {};
    });

    RegisterFunction("find", [this](const std::vector<ScriptValue>& args) -> ScriptValue {
        if (!m_BoundScene || args.empty()) return {};
        auto* obj = m_BoundScene->FindByName(args[0].AsString());
        return obj ? ScriptValue(static_cast<f64>(obj->GetID())) : ScriptValue();
    });

    RegisterFunction("destroy", [this](const std::vector<ScriptValue>& args) -> ScriptValue {
        if (!m_BoundScene || args.empty()) return {};
        u32 id = static_cast<u32>(args[0].AsNumber());
        auto* obj = m_BoundScene->FindByID(id);
        if (obj) { m_BoundScene->DestroyGameObject(obj); GV_LOG_INFO("[Script] Destroyed id=" + std::to_string(id)); }
        return {};
    });

    RegisterFunction("set_position", [this](const std::vector<ScriptValue>& args) -> ScriptValue {
        if (args.size() < 3) return {};
        GameObject* obj = m_SelfObject;
        if (args.size() >= 4 && m_BoundScene) {
            obj = m_BoundScene->FindByID(static_cast<u32>(args[0].AsNumber()));
            if (obj) obj->GetTransform().SetPosition((f32)args[1].AsNumber(), (f32)args[2].AsNumber(), (f32)args[3].AsNumber());
        } else if (obj) {
            obj->GetTransform().SetPosition((f32)args[0].AsNumber(), (f32)args[1].AsNumber(), (f32)args[2].AsNumber());
        }
        return {};
    });

    RegisterFunction("get_position_x", [this](const std::vector<ScriptValue>& args) -> ScriptValue {
        GameObject* obj = m_SelfObject;
        if (!args.empty() && m_BoundScene) obj = m_BoundScene->FindByID((u32)args[0].AsNumber());
        return obj ? ScriptValue((f64)obj->GetTransform().position.x) : ScriptValue(0.0);
    });
    RegisterFunction("get_position_y", [this](const std::vector<ScriptValue>& args) -> ScriptValue {
        GameObject* obj = m_SelfObject;
        if (!args.empty() && m_BoundScene) obj = m_BoundScene->FindByID((u32)args[0].AsNumber());
        return obj ? ScriptValue((f64)obj->GetTransform().position.y) : ScriptValue(0.0);
    });
    RegisterFunction("get_position_z", [this](const std::vector<ScriptValue>& args) -> ScriptValue {
        GameObject* obj = m_SelfObject;
        if (!args.empty() && m_BoundScene) obj = m_BoundScene->FindByID((u32)args[0].AsNumber());
        return obj ? ScriptValue((f64)obj->GetTransform().position.z) : ScriptValue(0.0);
    });

    RegisterFunction("set_scale", [this](const std::vector<ScriptValue>& args) -> ScriptValue {
        if (args.empty()) return {};
        GameObject* obj = m_SelfObject;
        if (args.size() >= 2 && m_BoundScene) {
            obj = m_BoundScene->FindByID((u32)args[0].AsNumber());
            if (obj) obj->GetTransform().SetScale((f32)args[1].AsNumber());
        } else if (obj) obj->GetTransform().SetScale((f32)args[0].AsNumber());
        return {};
    });

    RegisterFunction("set_rotation", [this](const std::vector<ScriptValue>& args) -> ScriptValue {
        if (args.size() < 3) return {};
        GameObject* obj = m_SelfObject;
        if (obj) obj->GetTransform().SetEulerDeg((f32)args[0].AsNumber(), (f32)args[1].AsNumber(), (f32)args[2].AsNumber());
        return {};
    });

    RegisterFunction("get_object_count", [this](const std::vector<ScriptValue>&) -> ScriptValue {
        return m_BoundScene ? ScriptValue((f64)m_BoundScene->GetAllObjects().size()) : ScriptValue(0.0);
    });

    GV_LOG_DEBUG("ScriptEngine — Scene API bound.");
}

void ScriptEngine::BindGameObjectAPI() {
    GV_LOG_DEBUG("ScriptEngine — GameObject API bound.");
}

void ScriptEngine::BindEventAPI() {
    // emit(signal_name)  or  emit(signal_name, data_string)
    RegisterFunction("emit", [this](const std::vector<ScriptValue>& args) -> ScriptValue {
        if (args.empty()) return {};
        std::string sig = args[0].AsString();
        std::string data = args.size() >= 2 ? args[1].AsString() : "";
        EventBus::Instance().EmitSignal(sig, data, m_SelfObject);
        return {};
    });

    // on_collision(callback_func_name)  — sugar: calls callback_func_name when self collides
    // The callback function will be invoked with no args; the script can use
    // get_position_x/y/z to inspect the collision objects.
    RegisterFunction("on_collision", [this](const std::vector<ScriptValue>& args) -> ScriptValue {
        if (args.empty()) return {};
        std::string funcName = args[0].AsString();
        GameObject* self = m_SelfObject;
        ScriptEngine* engine = this;
        EventBus::Instance().Subscribe(EventType::CollisionEnter,
            [engine, funcName, self](const Event& e) {
                engine->SetSelfObject(e.objectA == self ? e.objectA : e.objectB);
                engine->CallFunction(funcName);
            }, self);
        return {};
    });

    // get_delta_time() — returns the dt variable set during on_update
    RegisterFunction("get_delta_time", [this](const std::vector<ScriptValue>&) -> ScriptValue {
        return GetVariable("dt");
    });

    // get_time() — returns elapsed time from clock
    RegisterFunction("get_time", [](const std::vector<ScriptValue>&) -> ScriptValue {
        static auto start = std::chrono::high_resolution_clock::now();
        auto now = std::chrono::high_resolution_clock::now();
        f64 elapsed = std::chrono::duration<f64>(now - start).count();
        return ScriptValue(elapsed);
    });

    GV_LOG_DEBUG("ScriptEngine — Event API bound.");
}

void ScriptEngine::EnableHotReload(bool enable) {
    m_HotReload = enable;
    GV_LOG_INFO(std::string("ScriptEngine — hot reload ") + (enable ? "enabled" : "disabled"));
}

// ============================================================================
// Tokenizer
// ============================================================================
std::vector<ScriptEngine::Token> ScriptEngine::Tokenize(const std::string& source) {
    std::vector<Token> tokens;
    size_t i = 0;
    i32 line = 1;

    while (i < source.size()) {
        char c = source[i];

        // Whitespace
        if (c == ' ' || c == '\t' || c == '\r') { ++i; continue; }
        if (c == '\n') { ++line; ++i; continue; }

        // Comments
        if (c == '/' && i + 1 < source.size()) {
            if (source[i + 1] == '/') { while (i < source.size() && source[i] != '\n') ++i; continue; }
            if (source[i + 1] == '*') {
                i += 2;
                while (i + 1 < source.size() && !(source[i] == '*' && source[i + 1] == '/')) { if (source[i] == '\n') ++line; ++i; }
                i += 2; continue;
            }
        }

        // Numbers
        if (std::isdigit(c) || (c == '.' && i + 1 < source.size() && std::isdigit(source[i + 1]))) {
            std::string num;
            while (i < source.size() && (std::isdigit(source[i]) || source[i] == '.')) num += source[i++];
            tokens.push_back({ Token::Number, num, std::stod(num), line });
            continue;
        }

        // Strings
        if (c == '"' || c == '\'') {
            char q = c; ++i;
            std::string str;
            while (i < source.size() && source[i] != q) {
                if (source[i] == '\\' && i + 1 < source.size()) {
                    ++i;
                    switch (source[i]) { case 'n': str += '\n'; break; case 't': str += '\t'; break; case '\\': str += '\\'; break; default: str += source[i]; }
                } else str += source[i];
                ++i;
            }
            if (i < source.size()) ++i;
            tokens.push_back({ Token::Str, str, 0, line });
            continue;
        }

        // Identifiers / keywords
        if (std::isalpha(c) || c == '_') {
            std::string id;
            while (i < source.size() && (std::isalnum(source[i]) || source[i] == '_')) id += source[i++];
            Token t; t.text = id; t.line = line; t.numVal = 0;
            if      (id == "if")                        t.kind = Token::If;
            else if (id == "else")                      t.kind = Token::Else;
            else if (id == "while")                     t.kind = Token::While;
            else if (id == "for")                       t.kind = Token::For;
            else if (id == "func" || id == "function")  t.kind = Token::Func;
            else if (id == "return")                    t.kind = Token::Return;
            else if (id == "var" || id == "local")      t.kind = Token::Var;
            else if (id == "true")                      t.kind = Token::True_;
            else if (id == "false")                     t.kind = Token::False_;
            else if (id == "and")                       t.kind = Token::And;
            else if (id == "or")                        t.kind = Token::Or;
            else if (id == "not")                       t.kind = Token::Not;
            else                                        t.kind = Token::Ident;
            tokens.push_back(t);
            continue;
        }

        // Two-char operators
        if (i + 1 < source.size()) {
            std::string tw = source.substr(i, 2);
            if (tw == "==") { tokens.push_back({ Token::Eq,  "==", 0, line }); i += 2; continue; }
            if (tw == "!=") { tokens.push_back({ Token::Neq, "!=", 0, line }); i += 2; continue; }
            if (tw == "<=") { tokens.push_back({ Token::Lte, "<=", 0, line }); i += 2; continue; }
            if (tw == ">=") { tokens.push_back({ Token::Gte, ">=", 0, line }); i += 2; continue; }
            if (tw == "&&") { tokens.push_back({ Token::And, "&&", 0, line }); i += 2; continue; }
            if (tw == "||") { tokens.push_back({ Token::Or,  "||", 0, line }); i += 2; continue; }
        }

        // Single-char tokens
        Token t; t.line = line; t.numVal = 0;
        switch (c) {
            case '(': t.kind = Token::LParen;    t.text = "("; break;
            case ')': t.kind = Token::RParen;    t.text = ")"; break;
            case '{': t.kind = Token::LBrace;    t.text = "{"; break;
            case '}': t.kind = Token::RBrace;    t.text = "}"; break;
            case ',': t.kind = Token::Comma;     t.text = ","; break;
            case ';': t.kind = Token::Semicolon; t.text = ";"; break;
            case '=': t.kind = Token::Assign;    t.text = "="; break;
            case '+': t.kind = Token::Plus;      t.text = "+"; break;
            case '-': t.kind = Token::Minus;     t.text = "-"; break;
            case '*': t.kind = Token::Star;      t.text = "*"; break;
            case '/': t.kind = Token::Slash;     t.text = "/"; break;
            case '%': t.kind = Token::Percent;   t.text = "%"; break;
            case '<': t.kind = Token::Lt;        t.text = "<"; break;
            case '>': t.kind = Token::Gt;        t.text = ">"; break;
            case '!': t.kind = Token::Not;       t.text = "!"; break;
            case '.': t.kind = Token::Dot;       t.text = "."; break;
            default:  ++i; continue;
        }
        tokens.push_back(t);
        ++i;
    }

    tokens.push_back({ Token::Eof, "", 0, line });
    return tokens;
}

// ============================================================================
// Interpreter — recursive-descent
// ============================================================================
ScriptValue ScriptEngine::ExecuteStatement(const std::vector<Token>& tokens, size_t& pos) {
    if (pos >= tokens.size() || tokens[pos].kind == Token::Eof) return {};

    // Skip semicolons
    while (pos < tokens.size() && tokens[pos].kind == Token::Semicolon) ++pos;
    if (pos >= tokens.size() || tokens[pos].kind == Token::Eof) return {};

    const Token& tok = tokens[pos];

    // ── var x = expr ────────────────────────────────────────────────────
    if (tok.kind == Token::Var) {
        ++pos;
        if (pos >= tokens.size() || tokens[pos].kind != Token::Ident)
            throw std::runtime_error("Expected variable name after 'var'");
        std::string name = tokens[pos].text; ++pos;
        ScriptValue val;
        if (pos < tokens.size() && tokens[pos].kind == Token::Assign) { ++pos; val = ExecuteExpression(tokens, pos); }
        m_Variables[name] = val;
        if (pos < tokens.size() && tokens[pos].kind == Token::Semicolon) ++pos;
        return val;
    }

    // ── func name(params) { body } ──────────────────────────────────────
    if (tok.kind == Token::Func) {
        ++pos;
        if (pos >= tokens.size()) throw std::runtime_error("Expected function name");
        std::string name = tokens[pos].text; ++pos;
        // params
        std::vector<std::string> params;
        if (pos < tokens.size() && tokens[pos].kind == Token::LParen) {
            ++pos;
            while (pos < tokens.size() && tokens[pos].kind != Token::RParen) {
                if (tokens[pos].kind == Token::Ident) params.push_back(tokens[pos].text);
                ++pos;
                if (pos < tokens.size() && tokens[pos].kind == Token::Comma) ++pos;
            }
            if (pos < tokens.size()) ++pos; // skip )
        }
        // collect body tokens between { ... }
        std::vector<Token> body;
        if (pos < tokens.size() && tokens[pos].kind == Token::LBrace) {
            ++pos; int depth = 1;
            while (pos < tokens.size() && depth > 0) {
                if (tokens[pos].kind == Token::LBrace) depth++;
                if (tokens[pos].kind == Token::RBrace) { depth--; if (depth == 0) break; }
                body.push_back(tokens[pos]); ++pos;
            }
            if (pos < tokens.size()) ++pos; // skip }
        }
        body.push_back({ Token::Eof, "", 0, 0 });
        m_ScriptFunctions[name] = { name, params, body };
        return {};
    }

    // ── if (cond) { ... } else { ... } ──────────────────────────────────
    if (tok.kind == Token::If) {
        ++pos;
        if (pos < tokens.size() && tokens[pos].kind == Token::LParen) ++pos;
        ScriptValue cond = ExecuteExpression(tokens, pos);
        if (pos < tokens.size() && tokens[pos].kind == Token::RParen) ++pos;

        auto skipBlock = [&]() {
            if (pos < tokens.size() && tokens[pos].kind == Token::LBrace) {
                ++pos; int d = 1;
                while (pos < tokens.size() && d > 0) { if (tokens[pos].kind == Token::LBrace) d++; if (tokens[pos].kind == Token::RBrace) d--; ++pos; }
            }
        };

        if (cond.AsBool()) {
            ScriptValue r = ExecuteBlock(tokens, pos);
            if (pos < tokens.size() && tokens[pos].kind == Token::Else) { ++pos; skipBlock(); }
            return r;
        } else {
            skipBlock();
            if (pos < tokens.size() && tokens[pos].kind == Token::Else) { ++pos; return ExecuteBlock(tokens, pos); }
            return {};
        }
    }

    // ── while (cond) { body } ───────────────────────────────────────────
    if (tok.kind == Token::While) {
        ++pos;
        size_t condStart = pos;
        for (int guard = 10000; guard > 0; --guard) {
            pos = condStart;
            if (pos < tokens.size() && tokens[pos].kind == Token::LParen) ++pos;
            ScriptValue cond = ExecuteExpression(tokens, pos);
            if (pos < tokens.size() && tokens[pos].kind == Token::RParen) ++pos;
            if (!cond.AsBool()) {
                // skip block
                if (pos < tokens.size() && tokens[pos].kind == Token::LBrace) {
                    ++pos; int d = 1;
                    while (pos < tokens.size() && d > 0) { if (tokens[pos].kind == Token::LBrace) d++; if (tokens[pos].kind == Token::RBrace) d--; ++pos; }
                }
                break;
            }
            ExecuteBlock(tokens, pos);
        }
        return {};
    }

    // ── for (init; cond; step) { body } ─────────────────────────────────
    if (tok.kind == Token::For) {
        ++pos;
        if (pos < tokens.size() && tokens[pos].kind == Token::LParen) ++pos;
        ExecuteStatement(tokens, pos); // init
        size_t condStart = pos;

        for (int guard = 10000; guard > 0; --guard) {
            pos = condStart;
            ScriptValue cond = ExecuteExpression(tokens, pos);
            if (pos < tokens.size() && tokens[pos].kind == Token::Semicolon) ++pos;
            size_t stepStart = pos;
            // find closing paren
            size_t bodyStart = stepStart;
            while (bodyStart < tokens.size() && tokens[bodyStart].kind != Token::RParen) ++bodyStart;
            if (bodyStart < tokens.size()) ++bodyStart;

            if (!cond.AsBool()) {
                pos = bodyStart;
                if (pos < tokens.size() && tokens[pos].kind == Token::LBrace) {
                    ++pos; int d = 1;
                    while (pos < tokens.size() && d > 0) { if (tokens[pos].kind == Token::LBrace) d++; if (tokens[pos].kind == Token::RBrace) d--; ++pos; }
                }
                break;
            }
            pos = bodyStart;
            ExecuteBlock(tokens, pos);
            size_t stp = stepStart;
            ExecuteExpression(tokens, stp);
        }
        return {};
    }

    // ── return expr ─────────────────────────────────────────────────────
    if (tok.kind == Token::Return) {
        ++pos;
        ScriptValue val;
        if (pos < tokens.size() && tokens[pos].kind != Token::Semicolon &&
            tokens[pos].kind != Token::RBrace && tokens[pos].kind != Token::Eof)
            val = ExecuteExpression(tokens, pos);
        if (pos < tokens.size() && tokens[pos].kind == Token::Semicolon) ++pos;
        return val;
    }

    // ── Expression statement ────────────────────────────────────────────
    ScriptValue val = ExecuteExpression(tokens, pos);
    if (pos < tokens.size() && tokens[pos].kind == Token::Semicolon) ++pos;
    return val;
}

ScriptValue ScriptEngine::ExecuteBlock(const std::vector<Token>& tokens, size_t& pos) {
    ScriptValue last;
    if (pos < tokens.size() && tokens[pos].kind == Token::LBrace) {
        ++pos;
        while (pos < tokens.size() && tokens[pos].kind != Token::RBrace && tokens[pos].kind != Token::Eof)
            last = ExecuteStatement(tokens, pos);
        if (pos < tokens.size() && tokens[pos].kind == Token::RBrace) ++pos;
    } else {
        last = ExecuteStatement(tokens, pos);
    }
    return last;
}

// ── Expression hierarchy ────────────────────────────────────────────────
ScriptValue ScriptEngine::ExecuteExpression(const std::vector<Token>& tokens, size_t& pos) {
    return ExecuteComparison(tokens, pos);
}

ScriptValue ScriptEngine::ExecuteComparison(const std::vector<Token>& tokens, size_t& pos) {
    ScriptValue left = ExecuteAddSub(tokens, pos);
    while (pos < tokens.size()) {
        auto k = tokens[pos].kind;
        if (k == Token::Eq || k == Token::Neq || k == Token::Lt || k == Token::Gt ||
            k == Token::Lte || k == Token::Gte || k == Token::And || k == Token::Or) {
            ++pos;
            ScriptValue right = ExecuteAddSub(tokens, pos);
            switch (k) {
                case Token::Eq:  left = ScriptValue(left.AsNumber() == right.AsNumber()); break;
                case Token::Neq: left = ScriptValue(left.AsNumber() != right.AsNumber()); break;
                case Token::Lt:  left = ScriptValue(left.AsNumber() <  right.AsNumber()); break;
                case Token::Gt:  left = ScriptValue(left.AsNumber() >  right.AsNumber()); break;
                case Token::Lte: left = ScriptValue(left.AsNumber() <= right.AsNumber()); break;
                case Token::Gte: left = ScriptValue(left.AsNumber() >= right.AsNumber()); break;
                case Token::And: left = ScriptValue(left.AsBool() && right.AsBool()); break;
                case Token::Or:  left = ScriptValue(left.AsBool() || right.AsBool()); break;
                default: break;
            }
        } else break;
    }
    return left;
}

ScriptValue ScriptEngine::ExecuteAddSub(const std::vector<Token>& tokens, size_t& pos) {
    ScriptValue left = ExecuteMulDiv(tokens, pos);
    while (pos < tokens.size()) {
        if (tokens[pos].kind == Token::Plus) {
            ++pos; ScriptValue r = ExecuteMulDiv(tokens, pos);
            left = (left.type == ScriptValue::String || r.type == ScriptValue::String)
                       ? ScriptValue(left.AsString() + r.AsString())
                       : ScriptValue(left.AsNumber() + r.AsNumber());
        } else if (tokens[pos].kind == Token::Minus) {
            ++pos; ScriptValue r = ExecuteMulDiv(tokens, pos);
            left = ScriptValue(left.AsNumber() - r.AsNumber());
        } else break;
    }
    return left;
}

ScriptValue ScriptEngine::ExecuteMulDiv(const std::vector<Token>& tokens, size_t& pos) {
    ScriptValue left = ExecuteUnary(tokens, pos);
    while (pos < tokens.size()) {
        if (tokens[pos].kind == Token::Star) {
            ++pos; left = ScriptValue(left.AsNumber() * ExecuteUnary(tokens, pos).AsNumber());
        } else if (tokens[pos].kind == Token::Slash) {
            ++pos; f64 d = ExecuteUnary(tokens, pos).AsNumber();
            left = ScriptValue(d != 0 ? left.AsNumber() / d : 0.0);
        } else if (tokens[pos].kind == Token::Percent) {
            ++pos; f64 d = ExecuteUnary(tokens, pos).AsNumber();
            left = ScriptValue(d != 0 ? std::fmod(left.AsNumber(), d) : 0.0);
        } else break;
    }
    return left;
}

ScriptValue ScriptEngine::ExecuteUnary(const std::vector<Token>& tokens, size_t& pos) {
    if (pos < tokens.size() && tokens[pos].kind == Token::Minus) { ++pos; return ScriptValue(-ExecutePrimary(tokens, pos).AsNumber()); }
    if (pos < tokens.size() && tokens[pos].kind == Token::Not)   { ++pos; return ScriptValue(!ExecutePrimary(tokens, pos).AsBool());   }
    return ExecutePrimary(tokens, pos);
}

ScriptValue ScriptEngine::ExecutePrimary(const std::vector<Token>& tokens, size_t& pos) {
    if (pos >= tokens.size() || tokens[pos].kind == Token::Eof) return {};
    const Token& tok = tokens[pos];

    if (tok.kind == Token::Number) { ++pos; return ScriptValue(tok.numVal); }
    if (tok.kind == Token::Str)    { ++pos; return ScriptValue(tok.text); }
    if (tok.kind == Token::True_)  { ++pos; return ScriptValue(true); }
    if (tok.kind == Token::False_) { ++pos; return ScriptValue(false); }

    // Parenthesised
    if (tok.kind == Token::LParen) {
        ++pos; ScriptValue v = ExecuteExpression(tokens, pos);
        if (pos < tokens.size() && tokens[pos].kind == Token::RParen) ++pos;
        return v;
    }

    // Identifier — variable / assignment / function call
    if (tok.kind == Token::Ident) {
        std::string name = tok.text; ++pos;

        // assignment
        if (pos < tokens.size() && tokens[pos].kind == Token::Assign) {
            ++pos; ScriptValue v = ExecuteExpression(tokens, pos);
            m_Variables[name] = v;
            return v;
        }

        // function call
        if (pos < tokens.size() && tokens[pos].kind == Token::LParen) {
            ++pos;
            std::vector<ScriptValue> args;
            while (pos < tokens.size() && tokens[pos].kind != Token::RParen) {
                args.push_back(ExecuteExpression(tokens, pos));
                if (pos < tokens.size() && tokens[pos].kind == Token::Comma) ++pos;
            }
            if (pos < tokens.size()) ++pos; // skip )

            // native
            auto nit = m_NativeFunctions.find(name);
            if (nit != m_NativeFunctions.end()) return nit->second(args);

            // script
            auto sit = m_ScriptFunctions.find(name);
            if (sit != m_ScriptFunctions.end()) {
                auto& fn = sit->second;
                for (size_t j = 0; j < fn.params.size() && j < args.size(); ++j)
                    m_Variables[fn.params[j]] = args[j];
                size_t bp = 0; ScriptValue res;
                while (bp < fn.body.size() && fn.body[bp].kind != Token::Eof)
                    res = ExecuteStatement(fn.body, bp);
                return res;
            }
            m_LastError = "Unknown function: " + name;
            GV_LOG_WARN("[Script] " + m_LastError);
            return {};
        }

        // variable
        auto it = m_Variables.find(name);
        return (it != m_Variables.end()) ? it->second : ScriptValue();
    }

    ++pos;
    return {};
}

ScriptValue ScriptEngine::CallNative(const std::string& name, const std::vector<ScriptValue>& args) {
    auto it = m_NativeFunctions.find(name);
    return (it != m_NativeFunctions.end()) ? it->second(args) : ScriptValue();
}

} // namespace gv
