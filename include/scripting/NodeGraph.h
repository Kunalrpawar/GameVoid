// ============================================================================
// GameVoid Engine — Visual Node Scripting System
// ============================================================================
// Blueprint-style visual scripting with draggable logic nodes.
// Nodes represent events, conditions, actions, math operations.
// ============================================================================
#pragma once

#include "core/Types.h"
#include "core/Math.h"
#include "core/Component.h"
#include <string>
#include <vector>
#include <map>
#include <functional>

namespace gv {

// Forward declarations
class Scene;
class GameObject;

// ── Pin Types ──────────────────────────────────────────────────────────────
enum class PinType { Flow, Bool, Int, Float, Vec3, String, Object };

// ── Pin ────────────────────────────────────────────────────────────────────
struct NodePin {
    std::string name;
    PinType     type     = PinType::Flow;
    bool        isOutput = false;
    u32         id       = 0;

    // Connection (single connection per input, multiple per output for flow)
    i32 connectedNodeID = -1;
    i32 connectedPinID  = -1;

    // Default / current value
    f32  floatVal = 0.0f;
    i32  intVal   = 0;
    bool boolVal  = false;
    Vec3 vec3Val  { 0, 0, 0 };
    std::string strVal;
};

// ── Node Types ─────────────────────────────────────────────────────────────
enum class VisualNodeType {
    // Events (entry points)
    OnStart,
    OnUpdate,
    OnKeyPress,
    OnCollision,

    // Flow control
    Branch,         // if-then-else
    ForLoop,
    Sequence,
    Delay,

    // Actions
    Print,
    SetPosition,
    SetRotation,
    SetScale,
    ApplyForce,
    SpawnObject,
    DestroyObject,
    PlaySound,
    PlayAnimation,

    // Math
    Add,
    Subtract,
    Multiply,
    Divide,
    Clamp,
    Random,

    // Getters
    GetPosition,
    GetRotation,
    GetDeltaTime,
    GetKeyDown,

    // Comparison
    Equal,
    Greater,
    Less,
    And,
    Or,
    Not,

    // Variables
    SetVariable,
    GetVariable,
};

// ── Visual Script Node ─────────────────────────────────────────────────────
struct VisualNode {
    u32 id = 0;
    VisualNodeType type = VisualNodeType::OnStart;
    Vec2 position { 0, 0 };   // editor canvas position
    std::string label;
    Vec4 color { 0.3f, 0.3f, 0.3f, 1.0f }; // node header colour

    std::vector<NodePin> inputs;
    std::vector<NodePin> outputs;

    // For variable nodes
    std::string variableName;
};

// ── Connection ─────────────────────────────────────────────────────────────
struct NodeConnection {
    u32 fromNodeID = 0;
    u32 fromPinID  = 0;
    u32 toNodeID   = 0;
    u32 toPinID    = 0;
};

// ── Visual Script Graph ────────────────────────────────────────────────────
/// A graph of interconnected nodes that defines gameplay behaviour.
class NodeGraph {
public:
    NodeGraph(const std::string& name = "New Graph") : m_Name(name) {}

    // ── Node management ────────────────────────────────────────────────────
    u32 AddNode(VisualNodeType type, Vec2 pos = {});
    void RemoveNode(u32 id);
    VisualNode* GetNode(u32 id);
    const VisualNode* GetNode(u32 id) const;

    // ── Connections ────────────────────────────────────────────────────────
    bool Connect(u32 fromNode, u32 fromPin, u32 toNode, u32 toPin);
    void Disconnect(u32 toNode, u32 toPinID);
    const std::vector<NodeConnection>& GetConnections() const { return m_Connections; }

    // ── Execution ──────────────────────────────────────────────────────────
    /// Execute the graph starting from OnStart nodes. Called by ScriptEngine.
    void ExecuteOnStart(Scene& scene, GameObject* self);

    /// Execute the graph starting from OnUpdate nodes each frame.
    void ExecuteOnUpdate(Scene& scene, GameObject* self, f32 dt);

    // ── Accessors ──────────────────────────────────────────────────────────
    const std::string& GetName() const { return m_Name; }
    void SetName(const std::string& n) { m_Name = n; }
    const std::vector<VisualNode>& GetNodes() const { return m_Nodes; }
    std::vector<VisualNode>& GetNodesMut() { return m_Nodes; }

    // ── Variables ──────────────────────────────────────────────────────────
    void SetVariable(const std::string& name, f32 val);
    f32  GetVariable(const std::string& name) const;

    // ── Serialization ──────────────────────────────────────────────────────
    std::string Serialize() const;
    bool Deserialize(const std::string& data);

private:
    void ExecuteNode(u32 nodeID, Scene& scene, GameObject* self, f32 dt);
    f32  EvaluateFloat(u32 nodeID, i32 pinIndex, Scene& scene, GameObject* self, f32 dt);
    Vec3 EvaluateVec3(u32 nodeID, i32 pinIndex, Scene& scene, GameObject* self, f32 dt);
    bool EvaluateBool(u32 nodeID, i32 pinIndex, Scene& scene, GameObject* self, f32 dt);

    void InitNodePins(VisualNode& node);

    std::vector<VisualNode>      m_Nodes;
    std::vector<NodeConnection>  m_Connections;
    std::map<std::string, f32>   m_Variables;
    std::string m_Name;
    u32 m_NextNodeID = 1;
    u32 m_NextPinID  = 1;
};

// ── Node Graph Component ──────────────────────────────────────────────────
/// Attach to a GameObject to give it a visual script.
class NodeGraphComponent : public Component {
public:
    NodeGraphComponent() = default;
    ~NodeGraphComponent() override = default;

    std::string GetTypeName() const override { return "NodeGraph"; }

    void OnStart() override;
    void OnUpdate(f32 dt) override;

    NodeGraph& GetGraph() { return m_Graph; }
    const NodeGraph& GetGraph() const { return m_Graph; }

private:
    NodeGraph m_Graph;
    bool m_Started = false;
};

} // namespace gv
