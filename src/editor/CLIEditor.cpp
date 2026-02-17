// ============================================================================
// GameVoid Engine — CLI Editor Implementation
// ============================================================================
#include "editor/CLIEditor.h"
#include "core/Scene.h"
#include "core/GameObject.h"
#include "core/SceneSerializer.h"
#include "physics/Physics.h"
#include "ai/AIManager.h"
#include "scripting/ScriptEngine.h"
#include "assets/Assets.h"
#include "renderer/MeshRenderer.h"
#include "renderer/Lighting.h"
#include "renderer/Camera.h"
#include <iostream>
#include <sstream>

namespace gv {

// ── Tokeniser ──────────────────────────────────────────────────────────────

std::vector<std::string> CLIEditor::Tokenise(const std::string& input) {
    std::vector<std::string> tokens;
    std::istringstream iss(input);
    std::string token;
    while (iss >> token) tokens.push_back(token);
    return tokens;
}

// ── Initialisation ─────────────────────────────────────────────────────────

void CLIEditor::Init(Scene* scene, PhysicsWorld* physics, AIManager* ai,
                     ScriptEngine* scripting, AssetManager* assets) {
    m_Scene     = scene;
    m_Physics   = physics;
    m_AI        = ai;
    m_Scripting = scripting;
    m_Assets    = assets;

    // Register built-in commands
    using Args = const std::vector<std::string>&;
    RegisterCommand("help",      "Show available commands",
        [this](Args a){ CmdHelp(a); });
    RegisterCommand("spawn",     "spawn <type> <name>  — types: cube, sphere, plane, empty, light, camera",
        [this](Args a){ CmdSpawn(a); });
    RegisterCommand("list",      "List all objects in the scene",
        [this](Args a){ CmdList(a); });
    RegisterCommand("inspect",   "inspect <name>  — show details of an object",
        [this](Args a){ CmdInspect(a); });
    RegisterCommand("setpos",    "setpos <name> <x> <y> <z>",
        [this](Args a){ CmdSetPos(a); });
    RegisterCommand("setrot",    "setrot <name> <pitch> <yaw> <roll> (degrees)",
        [this](Args a){ CmdSetRot(a); });
    RegisterCommand("setscale",  "setscale <name> <x> <y> <z>",
        [this](Args a){ CmdSetScale(a); });
    RegisterCommand("delete",    "delete <name>",
        [this](Args a){ CmdDelete(a); });
    RegisterCommand("physics",   "physics step [dt]  — run one physics step",
        [this](Args a){ CmdPhysicsStep(a); });
    RegisterCommand("ai",        "ai prompt <text>  — send a prompt to Gemini",
        [this](Args a){ CmdAIPrompt(a); });
    RegisterCommand("aigenlevel","aigenlevel <description>  — generate a level from text",
        [this](Args a){ CmdAIGenLevel(a); });
    RegisterCommand("run",       "run <script.lua>  — execute a Lua script",
        [this](Args a){ CmdRunScript(a); });
    RegisterCommand("dump",      "Dump scene hierarchy",
        [this](Args a){ CmdDump(a); });
    RegisterCommand("aigenerate","aigenerate <prompt>  — AI-generate an object from text",
        [this](Args a){ CmdAIGenObject(a); });
    RegisterCommand("addphysics","addphysics <name> [mass]  — add RigidBody+Collider to object",
        [this](Args a){ CmdAddPhysics(a); });
    RegisterCommand("count",     "Count all objects in the scene",
        [this](Args a){ CmdCount(a); });
    RegisterCommand("rename",    "rename <old> <new>  — rename an object",
        [this](Args a){ CmdRename(a); });
    RegisterCommand("save",      "save [file]  — save scene to disk (default: scene.gvs)",
        [this](Args a){ CmdSave(a); });
    RegisterCommand("load",      "load [file]  — load scene from disk (default: scene.gvs)",
        [this](Args a){ CmdLoad(a); });

    GV_LOG_INFO("CLIEditor initialised — type 'help' for commands.");
}

// ── Main loop ──────────────────────────────────────────────────────────────

void CLIEditor::Run() {
    std::string line;
    std::cout << "\n======================================\n"
              << "  GameVoid CLI Editor\n"
              << "  Type 'help' for a list of commands.\n"
              << "  Type 'exit' to quit.\n"
              << "======================================\n\n";

    while (true) {
        std::cout << "gv> ";
        if (!std::getline(std::cin, line)) break;
        if (line == "exit" || line == "quit") break;
        if (line.empty()) continue;
        ExecuteCommand(line);
    }
}

void CLIEditor::ExecuteCommand(const std::string& input) {
    auto tokens = Tokenise(input);
    if (tokens.empty()) return;

    auto it = m_Commands.find(tokens[0]);
    if (it == m_Commands.end()) {
        std::cout << "Unknown command: " << tokens[0] << "  (type 'help')\n";
        return;
    }

    // Remove the command name, pass remaining tokens as args.
    std::vector<std::string> args(tokens.begin() + 1, tokens.end());
    it->second.handler(args);
}

void CLIEditor::RegisterCommand(const std::string& name, const std::string& help,
                                CommandHandler handler) {
    m_Commands[name] = { help, std::move(handler) };
}

// ── Built-in command implementations ───────────────────────────────────────

void CLIEditor::CmdHelp(const std::vector<std::string>& /*args*/) {
    std::cout << "\nAvailable commands:\n";
    for (auto& kv : m_Commands)
        std::cout << "  " << kv.first << "  — " << kv.second.help << "\n";
    std::cout << "  exit        — quit the editor\n\n";
}

void CLIEditor::CmdSpawn(const std::vector<std::string>& args) {
    if (!m_Scene) { std::cout << "No active scene.\n"; return; }
    std::string type = (args.size() >= 1) ? args[0] : "empty";
    std::string name = (args.size() >= 2) ? args[1] : type;

    GameObject* obj = m_Scene->CreateGameObject(name);

    if (type == "cube" || type == "sphere" || type == "plane") {
        auto* mr = obj->AddComponent<MeshRenderer>();
        // In production: mr->SetMesh(Mesh::Create...()); 
        (void)mr;
        std::cout << "Spawned " << type << " '" << name << "'\n";
    } else if (type == "light") {
        obj->AddComponent<PointLight>();
        std::cout << "Spawned point light '" << name << "'\n";
    } else if (type == "camera") {
        auto* cam = obj->AddComponent<Camera>();
        cam->SetPerspective(60.0f, 16.0f / 9.0f, 0.1f, 1000.0f);
        std::cout << "Spawned camera '" << name << "'\n";
    } else {
        std::cout << "Spawned empty object '" << name << "'\n";
    }
}

void CLIEditor::CmdList(const std::vector<std::string>& /*args*/) {
    if (!m_Scene) { std::cout << "No active scene.\n"; return; }
    std::cout << "Objects in scene '" << m_Scene->GetName() << "':\n";
    for (auto& o : m_Scene->GetAllObjects()) {
        std::cout << "  [" << o->GetID() << "] " << o->GetName()
                  << "  " << o->GetTransform().ToString() << "\n";
    }
}

void CLIEditor::CmdInspect(const std::vector<std::string>& args) {
    if (!m_Scene || args.empty()) { std::cout << "Usage: inspect <name>\n"; return; }
    auto* obj = m_Scene->FindByName(args[0]);
    if (!obj) { std::cout << "Object '" << args[0] << "' not found.\n"; return; }

    std::cout << "=== " << obj->GetName() << " (id=" << obj->GetID() << ") ===\n"
              << "  Transform: " << obj->GetTransform().ToString() << "\n"
              << "  Components:\n";
    for (auto& c : obj->GetComponents())
        std::cout << "    - " << c->GetTypeName() << (c->IsEnabled() ? "" : " [disabled]") << "\n";
}

void CLIEditor::CmdSetPos(const std::vector<std::string>& args) {
    if (!m_Scene || args.size() < 4) { std::cout << "Usage: setpos <name> <x> <y> <z>\n"; return; }
    auto* obj = m_Scene->FindByName(args[0]);
    if (!obj) { std::cout << "Not found.\n"; return; }
    obj->GetTransform().SetPosition(std::stof(args[1]), std::stof(args[2]), std::stof(args[3]));
    std::cout << "Position set.\n";
}

void CLIEditor::CmdSetRot(const std::vector<std::string>& args) {
    if (!m_Scene || args.size() < 4) { std::cout << "Usage: setrot <name> <pitch> <yaw> <roll>\n"; return; }
    auto* obj = m_Scene->FindByName(args[0]);
    if (!obj) { std::cout << "Not found.\n"; return; }
    obj->GetTransform().SetEulerDeg(std::stof(args[1]), std::stof(args[2]), std::stof(args[3]));
    std::cout << "Rotation set.\n";
}

void CLIEditor::CmdSetScale(const std::vector<std::string>& args) {
    if (!m_Scene || args.size() < 4) { std::cout << "Usage: setscale <name> <x> <y> <z>\n"; return; }
    auto* obj = m_Scene->FindByName(args[0]);
    if (!obj) { std::cout << "Not found.\n"; return; }
    obj->GetTransform().SetScale(std::stof(args[1]), std::stof(args[2]), std::stof(args[3]));
    std::cout << "Scale set.\n";
}

void CLIEditor::CmdDelete(const std::vector<std::string>& args) {
    if (!m_Scene || args.empty()) { std::cout << "Usage: delete <name>\n"; return; }
    auto* obj = m_Scene->FindByName(args[0]);
    if (!obj) { std::cout << "Not found.\n"; return; }
    m_Scene->DestroyGameObject(obj);
    std::cout << "Deleted '" << args[0] << "'.\n";
}

void CLIEditor::CmdPhysicsStep(const std::vector<std::string>& args) {
    if (!m_Physics) { std::cout << "Physics not available.\n"; return; }
    f32 dt = (args.size() >= 2) ? std::stof(args[1]) : (1.0f / 60.0f);
    m_Physics->Step(dt);
    std::cout << "Physics stepped " << dt << "s.\n";
}

void CLIEditor::CmdAIPrompt(const std::vector<std::string>& args) {
    if (!m_AI) { std::cout << "AI module not available.\n"; return; }
    if (args.empty()) { std::cout << "Usage: ai <prompt text...>\n"; return; }
    std::string prompt;
    for (auto& a : args) prompt += a + " ";
    auto resp = m_AI->SendPrompt(prompt);
    if (resp.success)
        std::cout << "AI response:\n" << resp.text << "\n";
    else
        std::cout << "AI error: " << resp.errorMessage << "\n";
}

void CLIEditor::CmdAIGenLevel(const std::vector<std::string>& args) {
    if (!m_AI || !m_Scene) { std::cout << "AI or scene not available.\n"; return; }
    if (args.empty()) { std::cout << "Usage: aigenlevel <description...>\n"; return; }
    std::string desc;
    for (auto& a : args) desc += a + " ";
    auto blueprints = m_AI->GenerateLevel(desc);
    m_AI->PopulateScene(*m_Scene, blueprints);
    std::cout << "Generated " << blueprints.size() << " objects.\n";
}

void CLIEditor::CmdRunScript(const std::vector<std::string>& args) {
    if (!m_Scripting) { std::cout << "Scripting not available.\n"; return; }
    if (args.empty()) { std::cout << "Usage: run <script.lua>\n"; return; }
    m_Scripting->LoadFile(args[0]);
    std::cout << "Script executed.\n";
}

void CLIEditor::CmdDump(const std::vector<std::string>& /*args*/) {
    if (!m_Scene) { std::cout << "No active scene.\n"; return; }
    m_Scene->DumpHierarchy();
}

// ── New debug commands ─────────────────────────────────────────────────────

void CLIEditor::CmdAIGenObject(const std::vector<std::string>& args) {
    if (!m_AI || !m_Scene) { std::cout << "AI or scene not available.\n"; return; }
    if (args.empty()) { std::cout << "Usage: aigenerate <prompt text...>\n"; return; }
    std::string prompt;
    for (size_t i = 0; i < args.size(); ++i) {
        if (i > 0) prompt += " ";
        prompt += args[i];
    }
    auto* obj = m_AI->GenerateObjectFromPrompt(prompt, *m_Scene);
    if (obj)
        std::cout << "AI generated object '" << obj->GetName() << "'.\n";
    else
        std::cout << "AI failed to generate object.\n";
}

void CLIEditor::CmdAddPhysics(const std::vector<std::string>& args) {
    if (!m_Scene) { std::cout << "No active scene.\n"; return; }
    if (args.empty()) { std::cout << "Usage: addphysics <name> [mass]\n"; return; }
    auto* obj = m_Scene->FindByName(args[0]);
    if (!obj) { std::cout << "Object '" << args[0] << "' not found.\n"; return; }

    f32 mass = (args.size() >= 2) ? std::stof(args[1]) : 1.0f;
    PhysicsWorld::AddPhysicsComponents(obj, RigidBodyType::Dynamic,
                                       ColliderType::Box, mass);
    std::cout << "Added RigidBody (mass=" << mass << ") + Collider to '" << args[0] << "'.\n";
}

void CLIEditor::CmdCount(const std::vector<std::string>& /*args*/) {
    if (!m_Scene) { std::cout << "No active scene.\n"; return; }
    std::cout << "Scene '" << m_Scene->GetName() << "' has "
              << m_Scene->GetAllObjects().size() << " object(s).\n";
}

void CLIEditor::CmdRename(const std::vector<std::string>& args) {
    if (!m_Scene || args.size() < 2) { std::cout << "Usage: rename <old> <new>\n"; return; }
    auto* obj = m_Scene->FindByName(args[0]);
    if (!obj) { std::cout << "Object '" << args[0] << "' not found.\n"; return; }
    obj->SetName(args[1]);
    std::cout << "Renamed to '" << args[1] << "'.\n";
}

void CLIEditor::CmdSave(const std::vector<std::string>& args) {
    if (!m_Scene) { std::cout << "No active scene.\n"; return; }
    std::string path = (args.size() >= 1) ? args[0] : "scene.gvs";
    if (SceneSerializer::SaveScene(*m_Scene, path))
        std::cout << "Scene saved to '" << path << "'.\n";
    else
        std::cout << "Failed to save scene to '" << path << "'.\n";
}

void CLIEditor::CmdLoad(const std::vector<std::string>& args) {
    if (!m_Scene) { std::cout << "No active scene.\n"; return; }
    std::string path = (args.size() >= 1) ? args[0] : "scene.gvs";
    if (SceneSerializer::LoadScene(*m_Scene, path))
        std::cout << "Scene loaded from '" << path << "'.\n";
    else
        std::cout << "Failed to load scene from '" << path << "'.\n";
}

} // namespace gv
