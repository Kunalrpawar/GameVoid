// ============================================================================
// GameVoid Engine — 2D Dialogue System
// ============================================================================
// A branching dialogue tree for NPCs, cutscenes, and story events.
//
// Components:
//   - DialogueNode     : a single line of dialogue with optional choices
//   - DialogueTree     : a collection of nodes forming a conversation graph
//   - DialogueRunner2D : component that attaches to an NPC and drives the
//                         conversation state machine
//
// How it works:
//   1. Build a DialogueTree (in code or loaded from data) with nodes.
//      Each node has speaker name, text, and 0+ choices that branch to
//      other nodes.
//   2. Attach a DialogueRunner2D to an NPC object.
//   3. When the player interacts, call StartDialogue().
//   4. Call GetCurrentNode() to render the current text in your UI.
//   5. Call ChooseOption(index) to advance to the next node.
//   6. When the current node has no choices and no next node, the
//      dialogue ends (IsFinished() returns true).
//
// Supports:
//   - Linear dialogue (just next→next→next)
//   - Branching choices (RPG-style dialogue trees)
//   - Conditions / flags (simple string-based flag map)
//   - OnEnter / OnExit callbacks per node (trigger events, give items, etc.)
// ============================================================================
#pragma once

#include "core/Types.h"
#include "core/Component.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <functional>

namespace gv {

// ════════════════════════════════════════════════════════════════════════════
// DialogueChoice — a single choice the player can pick
// ════════════════════════════════════════════════════════════════════════════
struct DialogueChoice {
    std::string text;               // displayed text, e.g. "Tell me more"
    std::string targetNodeID;       // which node to jump to
    std::string requiredFlag;       // if non-empty, only show if this flag is set
};

// ════════════════════════════════════════════════════════════════════════════
// DialogueNode — one "page" of conversation
// ════════════════════════════════════════════════════════════════════════════
struct DialogueNode {
    std::string id;                 // unique ID within the tree, e.g. "greet_01"
    std::string speaker;            // name shown above text, e.g. "Old Man"
    std::string text;               // the actual dialogue line
    std::string nextNodeID;         // for linear flow (if no choices), "" = end

    std::vector<DialogueChoice> choices;

    // Callbacks
    std::function<void()> onEnter;  // fired when this node becomes active
    std::function<void()> onExit;   // fired when leaving this node

    // Optional: flag to set when this node is visited
    std::string setFlag;            // e.g. "talked_to_old_man"
};

// ════════════════════════════════════════════════════════════════════════════
// DialogueTree — the full conversation graph
// ════════════════════════════════════════════════════════════════════════════
class DialogueTree {
public:
    DialogueTree() = default;
    explicit DialogueTree(const std::string& treeName) : name(treeName) {}

    std::string name = "Untitled";

    /// Add a node to the tree.  Returns a reference so you can chain.
    DialogueNode& AddNode(const std::string& id, const std::string& speaker,
                          const std::string& text, const std::string& nextID = "");

    /// Find a node by ID (returns nullptr if not found).
    DialogueNode* GetNode(const std::string& id);
    const DialogueNode* GetNode(const std::string& id) const;

    /// Get the starting node (first one added).
    const std::string& GetStartNodeID() const;

    /// Get all node IDs (for editor listing).
    std::vector<std::string> GetAllNodeIDs() const;

    /// How many nodes in total.
    i32 GetNodeCount() const { return static_cast<i32>(m_Nodes.size()); }

private:
    std::vector<DialogueNode> m_Nodes;
    std::unordered_map<std::string, i32> m_Index;  // id → index into m_Nodes
    std::string m_StartID;
};

// ════════════════════════════════════════════════════════════════════════════
// DialogueRunner2D — state machine that drives a conversation
// ════════════════════════════════════════════════════════════════════════════
class DialogueRunner2D : public Component {
public:
    std::string GetTypeName() const override { return "DialogueRunner2D"; }

    // ── Settings ───────────────────────────────────────────────────────────
    f32  interactRadius   = 2.0f;    // world-unit distance to trigger dialogue
    f32  textSpeed        = 30.0f;   // characters per second for typewriter effect
    bool autoAdvance      = false;   // auto-advance after a delay (for cutscenes)
    f32  autoAdvanceDelay = 2.0f;    // seconds to wait before auto-advancing

    // ── Runtime ────────────────────────────────────────────────────────────

    /// Begin a conversation.  Pass a tree (owned externally).
    void StartDialogue(DialogueTree* tree);

    /// End the conversation early.
    void EndDialogue();

    /// Choose a branching option (0-based index into current node's choices).
    void ChooseOption(i32 index);

    /// Advance to the next node (linear flow — uses nextNodeID).
    void Advance();

    /// Update typewriter effect & auto-advance.  Call every frame while active.
    void Update(f32 dt);

    // ── Queries ────────────────────────────────────────────────────────────
    bool IsActive()   const { return m_Active; }
    bool IsFinished() const { return m_Finished; }

    /// Get the current node (or nullptr if not active).
    const DialogueNode* GetCurrentNode() const;

    /// Get the visible portion of text (typewriter effect).
    const std::string& GetVisibleText() const { return m_VisibleText; }

    /// Is the typewriter still revealing text?
    bool IsRevealing() const { return m_Revealing; }

    /// Skip the typewriter and show full text immediately.
    void SkipReveal();

    /// Get the available choices (filtered by flags).
    std::vector<const DialogueChoice*> GetAvailableChoices() const;

    // ── Flags (shared across all runners) ──────────────────────────────────
    /// Global flag map — set flags to gate choices.
    static void SetFlag(const std::string& flag, bool value = true);
    static bool HasFlag(const std::string& flag);
    static void ClearAllFlags();

private:
    DialogueTree*  m_Tree       = nullptr;
    std::string    m_CurrentID;
    bool           m_Active     = false;
    bool           m_Finished   = false;

    // Typewriter
    std::string    m_FullText;
    std::string    m_VisibleText;
    f32            m_RevealTimer = 0.0f;
    i32            m_CharsRevealed = 0;
    bool           m_Revealing  = false;

    // Auto-advance
    f32            m_AutoTimer  = 0.0f;

    // Global flags
    static std::unordered_map<std::string, bool> s_Flags;

    void EnterNode(const std::string& nodeID);
};

} // namespace gv
