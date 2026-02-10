// ============================================================================
// GameVoid Engine — CLI Editor / Debugger
// ============================================================================
// A terminal-based editor that lets you:
//   • Spawn objects (cube, sphere, empty, light, camera)
//   • Modify transforms (position, rotation, scale)
//   • List / inspect scene hierarchy
//   • Run simple tests (physics step, render frame)
//   • Interact with the AI module (send prompts, generate levels)
// Intended for rapid prototyping before a full GUI editor is built.
// ============================================================================
#pragma once

#include "core/Types.h"
#include <string>
#include <functional>
#include <unordered_map>

namespace gv {

// Forward declarations
class Scene;
class PhysicsWorld;
class AIManager;
class ScriptEngine;
class AssetManager;

/// Interactive command-line editing interface.
class CLIEditor {
public:
    CLIEditor() = default;

    // ── Initialisation ─────────────────────────────────────────────────────
    /// Wire up references to the live engine systems.
    void Init(Scene* scene, PhysicsWorld* physics, AIManager* ai,
              ScriptEngine* scripting, AssetManager* assets);

    // ── Main loop ──────────────────────────────────────────────────────────
    /// Enter the interactive command loop (blocks until user types "exit").
    void Run();

    /// Process a single command string (non-interactive, useful for scripted tests).
    void ExecuteCommand(const std::string& input);

    // ── Command registration ───────────────────────────────────────────────
    using CommandHandler = std::function<void(const std::vector<std::string>& args)>;

    /// Register a custom command that can be invoked from the CLI.
    void RegisterCommand(const std::string& name, const std::string& help,
                         CommandHandler handler);

private:
    // ── Built-in commands ──────────────────────────────────────────────────
    void CmdHelp(const std::vector<std::string>& args);
    void CmdSpawn(const std::vector<std::string>& args);
    void CmdList(const std::vector<std::string>& args);
    void CmdInspect(const std::vector<std::string>& args);
    void CmdSetPos(const std::vector<std::string>& args);
    void CmdSetRot(const std::vector<std::string>& args);
    void CmdSetScale(const std::vector<std::string>& args);
    void CmdDelete(const std::vector<std::string>& args);
    void CmdPhysicsStep(const std::vector<std::string>& args);
    void CmdAIPrompt(const std::vector<std::string>& args);
    void CmdAIGenLevel(const std::vector<std::string>& args);
    void CmdRunScript(const std::vector<std::string>& args);
    void CmdDump(const std::vector<std::string>& args);

    /// Tokenise a raw input line into command + arguments.
    static std::vector<std::string> Tokenise(const std::string& input);

    // ── State ──────────────────────────────────────────────────────────────
    struct CommandEntry {
        std::string   help;
        CommandHandler handler;
    };

    std::unordered_map<std::string, CommandEntry> m_Commands;

    Scene*        m_Scene     = nullptr;
    PhysicsWorld* m_Physics   = nullptr;
    AIManager*    m_AI        = nullptr;
    ScriptEngine* m_Scripting = nullptr;
    AssetManager* m_Assets    = nullptr;
};

} // namespace gv
