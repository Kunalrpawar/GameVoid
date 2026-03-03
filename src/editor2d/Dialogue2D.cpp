// ============================================================================
// GameVoid Engine — 2D Dialogue System Implementation
// ============================================================================
#include "editor2d/Dialogue2D.h"
#include <algorithm>

namespace gv {

// ════════════════════════════════════════════════════════════════════════════
// DialogueTree
// ════════════════════════════════════════════════════════════════════════════

DialogueNode& DialogueTree::AddNode(const std::string& id,
                                     const std::string& speaker,
                                     const std::string& text,
                                     const std::string& nextID) {
    DialogueNode node;
    node.id         = id;
    node.speaker    = speaker;
    node.text       = text;
    node.nextNodeID = nextID;

    i32 idx = static_cast<i32>(m_Nodes.size());
    m_Nodes.push_back(std::move(node));
    m_Index[id] = idx;

    // First node added becomes the start
    if (m_StartID.empty()) {
        m_StartID = id;
    }

    return m_Nodes.back();
}

DialogueNode* DialogueTree::GetNode(const std::string& id) {
    auto it = m_Index.find(id);
    if (it == m_Index.end()) return nullptr;
    return &m_Nodes[it->second];
}

const DialogueNode* DialogueTree::GetNode(const std::string& id) const {
    auto it = m_Index.find(id);
    if (it == m_Index.end()) return nullptr;
    return &m_Nodes[it->second];
}

const std::string& DialogueTree::GetStartNodeID() const {
    return m_StartID;
}

std::vector<std::string> DialogueTree::GetAllNodeIDs() const {
    std::vector<std::string> ids;
    ids.reserve(m_Nodes.size());
    for (auto& n : m_Nodes) {
        ids.push_back(n.id);
    }
    return ids;
}

// ════════════════════════════════════════════════════════════════════════════
// DialogueRunner2D — static flags
// ════════════════════════════════════════════════════════════════════════════

std::unordered_map<std::string, bool> DialogueRunner2D::s_Flags;

void DialogueRunner2D::SetFlag(const std::string& flag, bool value) {
    s_Flags[flag] = value;
}

bool DialogueRunner2D::HasFlag(const std::string& flag) {
    auto it = s_Flags.find(flag);
    return (it != s_Flags.end()) && it->second;
}

void DialogueRunner2D::ClearAllFlags() {
    s_Flags.clear();
}

// ════════════════════════════════════════════════════════════════════════════
// DialogueRunner2D — conversation control
// ════════════════════════════════════════════════════════════════════════════

void DialogueRunner2D::StartDialogue(DialogueTree* tree) {
    if (!tree || tree->GetNodeCount() == 0) return;

    m_Tree     = tree;
    m_Active   = true;
    m_Finished = false;

    EnterNode(tree->GetStartNodeID());
}

void DialogueRunner2D::EndDialogue() {
    if (!m_Active) return;

    // Fire onExit for current node
    if (m_Tree) {
        auto* node = m_Tree->GetNode(m_CurrentID);
        if (node && node->onExit) node->onExit();
    }

    m_Active   = false;
    m_Finished = true;
    m_Tree     = nullptr;
    m_CurrentID.clear();
    m_FullText.clear();
    m_VisibleText.clear();
}

void DialogueRunner2D::ChooseOption(i32 index) {
    if (!m_Active || !m_Tree) return;

    auto choices = GetAvailableChoices();
    if (index < 0 || index >= static_cast<i32>(choices.size())) return;

    const std::string& targetID = choices[index]->targetNodeID;

    // Exit current node
    auto* curNode = m_Tree->GetNode(m_CurrentID);
    if (curNode && curNode->onExit) curNode->onExit();

    if (targetID.empty()) {
        // No target → end dialogue
        EndDialogue();
    } else {
        EnterNode(targetID);
    }
}

void DialogueRunner2D::Advance() {
    if (!m_Active || !m_Tree) return;

    // If still revealing text, skip to full
    if (m_Revealing) {
        SkipReveal();
        return;
    }

    auto* node = m_Tree->GetNode(m_CurrentID);
    if (!node) { EndDialogue(); return; }

    // If there are choices, don't auto-advance — player must pick
    auto available = GetAvailableChoices();
    if (!available.empty()) return;

    // Exit current
    if (node->onExit) node->onExit();

    if (node->nextNodeID.empty()) {
        EndDialogue();
    } else {
        EnterNode(node->nextNodeID);
    }
}

void DialogueRunner2D::Update(f32 dt) {
    if (!m_Active) return;

    // Typewriter effect
    if (m_Revealing) {
        m_RevealTimer += dt * textSpeed;
        i32 target = static_cast<i32>(m_RevealTimer);
        if (target > static_cast<i32>(m_FullText.size())) {
            target = static_cast<i32>(m_FullText.size());
        }
        if (target != m_CharsRevealed) {
            m_CharsRevealed = target;
            m_VisibleText = m_FullText.substr(0, static_cast<size_t>(m_CharsRevealed));
        }
        if (m_CharsRevealed >= static_cast<i32>(m_FullText.size())) {
            m_Revealing = false;
        }
    }

    // Auto-advance (for cutscenes)
    if (autoAdvance && !m_Revealing) {
        m_AutoTimer += dt;
        if (m_AutoTimer >= autoAdvanceDelay) {
            Advance();
        }
    }
}

const DialogueNode* DialogueRunner2D::GetCurrentNode() const {
    if (!m_Active || !m_Tree) return nullptr;
    return m_Tree->GetNode(m_CurrentID);
}

void DialogueRunner2D::SkipReveal() {
    m_Revealing     = false;
    m_CharsRevealed = static_cast<i32>(m_FullText.size());
    m_VisibleText   = m_FullText;
}

std::vector<const DialogueChoice*> DialogueRunner2D::GetAvailableChoices() const {
    std::vector<const DialogueChoice*> result;
    if (!m_Active || !m_Tree) return result;

    auto* node = m_Tree->GetNode(m_CurrentID);
    if (!node) return result;

    for (auto& choice : node->choices) {
        // Filter by required flag
        if (!choice.requiredFlag.empty() && !HasFlag(choice.requiredFlag))
            continue;
        result.push_back(&choice);
    }
    return result;
}

// ── Private ────────────────────────────────────────────────────────────────

void DialogueRunner2D::EnterNode(const std::string& nodeID) {
    m_CurrentID = nodeID;

    auto* node = m_Tree->GetNode(nodeID);
    if (!node) {
        EndDialogue();
        return;
    }

    // Set flag if configured
    if (!node->setFlag.empty()) {
        SetFlag(node->setFlag);
    }

    // Fire callback
    if (node->onEnter) node->onEnter();

    // Start typewriter
    m_FullText       = node->text;
    m_VisibleText.clear();
    m_RevealTimer    = 0.0f;
    m_CharsRevealed  = 0;
    m_Revealing      = true;
    m_AutoTimer      = 0.0f;
}

} // namespace gv
