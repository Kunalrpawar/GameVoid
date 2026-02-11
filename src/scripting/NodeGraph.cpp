// ============================================================================
// GameVoid Engine — Visual Node Scripting Implementation
// ============================================================================
#include "scripting/NodeGraph.h"
#include "core/Scene.h"
#include "core/GameObject.h"
#include "core/Transform.h"
#include <cmath>
#include <cstdlib>
#include <sstream>

namespace gv {

// ── Pin initialisation per node type ───────────────────────────────────────
void NodeGraph::InitNodePins(VisualNode& node) {
    node.inputs.clear();
    node.outputs.clear();

    auto MakePin = [&](const std::string& name, PinType type, bool out) {
        NodePin p;
        p.name = name;
        p.type = type;
        p.isOutput = out;
        p.id = m_NextPinID++;
        if (out) node.outputs.push_back(p);
        else     node.inputs.push_back(p);
    };

    switch (node.type) {
    // Events
    case VisualNodeType::OnStart:
        node.label = "On Start"; node.color = {0.6f, 0.15f, 0.15f, 1};
        MakePin("Flow", PinType::Flow, true);
        break;
    case VisualNodeType::OnUpdate:
        node.label = "On Update"; node.color = {0.6f, 0.15f, 0.15f, 1};
        MakePin("Flow", PinType::Flow, true);
        MakePin("DeltaTime", PinType::Float, true);
        break;
    case VisualNodeType::OnKeyPress:
        node.label = "On Key Press"; node.color = {0.6f, 0.15f, 0.15f, 1};
        MakePin("Key", PinType::String, false);
        MakePin("Flow", PinType::Flow, true);
        break;
    case VisualNodeType::OnCollision:
        node.label = "On Collision"; node.color = {0.6f, 0.15f, 0.15f, 1};
        MakePin("Flow", PinType::Flow, true);
        MakePin("Other", PinType::Object, true);
        break;

    // Flow
    case VisualNodeType::Branch:
        node.label = "Branch"; node.color = {0.4f, 0.4f, 0.6f, 1};
        MakePin("Flow", PinType::Flow, false);
        MakePin("Condition", PinType::Bool, false);
        MakePin("True", PinType::Flow, true);
        MakePin("False", PinType::Flow, true);
        break;
    case VisualNodeType::ForLoop:
        node.label = "For Loop"; node.color = {0.4f, 0.4f, 0.6f, 1};
        MakePin("Flow", PinType::Flow, false);
        MakePin("Count", PinType::Int, false);
        MakePin("Body", PinType::Flow, true);
        MakePin("Done", PinType::Flow, true);
        MakePin("Index", PinType::Int, true);
        break;
    case VisualNodeType::Sequence:
        node.label = "Sequence"; node.color = {0.4f, 0.4f, 0.6f, 1};
        MakePin("Flow", PinType::Flow, false);
        MakePin("Out 0", PinType::Flow, true);
        MakePin("Out 1", PinType::Flow, true);
        break;
    case VisualNodeType::Delay:
        node.label = "Delay"; node.color = {0.4f, 0.4f, 0.6f, 1};
        MakePin("Flow", PinType::Flow, false);
        MakePin("Seconds", PinType::Float, false);
        MakePin("Done", PinType::Flow, true);
        break;

    // Actions
    case VisualNodeType::Print:
        node.label = "Print"; node.color = {0.2f, 0.5f, 0.2f, 1};
        MakePin("Flow", PinType::Flow, false);
        MakePin("Message", PinType::String, false);
        MakePin("Flow", PinType::Flow, true);
        break;
    case VisualNodeType::SetPosition:
        node.label = "Set Position"; node.color = {0.2f, 0.5f, 0.2f, 1};
        MakePin("Flow", PinType::Flow, false);
        MakePin("Position", PinType::Vec3, false);
        MakePin("Flow", PinType::Flow, true);
        break;
    case VisualNodeType::SetRotation:
        node.label = "Set Rotation"; node.color = {0.2f, 0.5f, 0.2f, 1};
        MakePin("Flow", PinType::Flow, false);
        MakePin("Euler", PinType::Vec3, false);
        MakePin("Flow", PinType::Flow, true);
        break;
    case VisualNodeType::SetScale:
        node.label = "Set Scale"; node.color = {0.2f, 0.5f, 0.2f, 1};
        MakePin("Flow", PinType::Flow, false);
        MakePin("Scale", PinType::Vec3, false);
        MakePin("Flow", PinType::Flow, true);
        break;
    case VisualNodeType::ApplyForce:
        node.label = "Apply Force"; node.color = {0.2f, 0.5f, 0.2f, 1};
        MakePin("Flow", PinType::Flow, false);
        MakePin("Force", PinType::Vec3, false);
        MakePin("Flow", PinType::Flow, true);
        break;
    case VisualNodeType::SpawnObject:
        node.label = "Spawn Object"; node.color = {0.2f, 0.5f, 0.2f, 1};
        MakePin("Flow", PinType::Flow, false);
        MakePin("Name", PinType::String, false);
        MakePin("Position", PinType::Vec3, false);
        MakePin("Flow", PinType::Flow, true);
        break;
    case VisualNodeType::DestroyObject:
        node.label = "Destroy Object"; node.color = {0.2f, 0.5f, 0.2f, 1};
        MakePin("Flow", PinType::Flow, false);
        MakePin("Flow", PinType::Flow, true);
        break;
    case VisualNodeType::PlaySound:
        node.label = "Play Sound"; node.color = {0.2f, 0.5f, 0.2f, 1};
        MakePin("Flow", PinType::Flow, false);
        MakePin("SoundName", PinType::String, false);
        MakePin("Flow", PinType::Flow, true);
        break;
    case VisualNodeType::PlayAnimation:
        node.label = "Play Animation"; node.color = {0.2f, 0.5f, 0.2f, 1};
        MakePin("Flow", PinType::Flow, false);
        MakePin("ClipName", PinType::String, false);
        MakePin("Flow", PinType::Flow, true);
        break;

    // Math
    case VisualNodeType::Add:
        node.label = "Add"; node.color = {0.3f, 0.5f, 0.5f, 1};
        MakePin("A", PinType::Float, false);
        MakePin("B", PinType::Float, false);
        MakePin("Result", PinType::Float, true);
        break;
    case VisualNodeType::Subtract:
        node.label = "Subtract"; node.color = {0.3f, 0.5f, 0.5f, 1};
        MakePin("A", PinType::Float, false);
        MakePin("B", PinType::Float, false);
        MakePin("Result", PinType::Float, true);
        break;
    case VisualNodeType::Multiply:
        node.label = "Multiply"; node.color = {0.3f, 0.5f, 0.5f, 1};
        MakePin("A", PinType::Float, false);
        MakePin("B", PinType::Float, false);
        MakePin("Result", PinType::Float, true);
        break;
    case VisualNodeType::Divide:
        node.label = "Divide"; node.color = {0.3f, 0.5f, 0.5f, 1};
        MakePin("A", PinType::Float, false);
        MakePin("B", PinType::Float, false);
        MakePin("Result", PinType::Float, true);
        break;
    case VisualNodeType::Clamp:
        node.label = "Clamp"; node.color = {0.3f, 0.5f, 0.5f, 1};
        MakePin("Value", PinType::Float, false);
        MakePin("Min", PinType::Float, false);
        MakePin("Max", PinType::Float, false);
        MakePin("Result", PinType::Float, true);
        break;
    case VisualNodeType::Random:
        node.label = "Random"; node.color = {0.3f, 0.5f, 0.5f, 1};
        MakePin("Min", PinType::Float, false);
        MakePin("Max", PinType::Float, false);
        MakePin("Result", PinType::Float, true);
        break;

    // Getters
    case VisualNodeType::GetPosition:
        node.label = "Get Position"; node.color = {0.5f, 0.35f, 0.2f, 1};
        MakePin("Position", PinType::Vec3, true);
        break;
    case VisualNodeType::GetRotation:
        node.label = "Get Rotation"; node.color = {0.5f, 0.35f, 0.2f, 1};
        MakePin("Euler", PinType::Vec3, true);
        break;
    case VisualNodeType::GetDeltaTime:
        node.label = "Get Delta Time"; node.color = {0.5f, 0.35f, 0.2f, 1};
        MakePin("DT", PinType::Float, true);
        break;
    case VisualNodeType::GetKeyDown:
        node.label = "Get Key Down"; node.color = {0.5f, 0.35f, 0.2f, 1};
        MakePin("Key", PinType::String, false);
        MakePin("IsDown", PinType::Bool, true);
        break;

    // Comparison
    case VisualNodeType::Equal:
        node.label = "Equal"; node.color = {0.5f, 0.3f, 0.5f, 1};
        MakePin("A", PinType::Float, false);
        MakePin("B", PinType::Float, false);
        MakePin("Result", PinType::Bool, true);
        break;
    case VisualNodeType::Greater:
        node.label = "Greater"; node.color = {0.5f, 0.3f, 0.5f, 1};
        MakePin("A", PinType::Float, false);
        MakePin("B", PinType::Float, false);
        MakePin("Result", PinType::Bool, true);
        break;
    case VisualNodeType::Less:
        node.label = "Less"; node.color = {0.5f, 0.3f, 0.5f, 1};
        MakePin("A", PinType::Float, false);
        MakePin("B", PinType::Float, false);
        MakePin("Result", PinType::Bool, true);
        break;
    case VisualNodeType::And:
        node.label = "And"; node.color = {0.5f, 0.3f, 0.5f, 1};
        MakePin("A", PinType::Bool, false);
        MakePin("B", PinType::Bool, false);
        MakePin("Result", PinType::Bool, true);
        break;
    case VisualNodeType::Or:
        node.label = "Or"; node.color = {0.5f, 0.3f, 0.5f, 1};
        MakePin("A", PinType::Bool, false);
        MakePin("B", PinType::Bool, false);
        MakePin("Result", PinType::Bool, true);
        break;
    case VisualNodeType::Not:
        node.label = "Not"; node.color = {0.5f, 0.3f, 0.5f, 1};
        MakePin("Input", PinType::Bool, false);
        MakePin("Result", PinType::Bool, true);
        break;

    // Variables
    case VisualNodeType::SetVariable:
        node.label = "Set Variable"; node.color = {0.2f, 0.3f, 0.6f, 1};
        MakePin("Flow", PinType::Flow, false);
        MakePin("Value", PinType::Float, false);
        MakePin("Flow", PinType::Flow, true);
        break;
    case VisualNodeType::GetVariable:
        node.label = "Get Variable"; node.color = {0.2f, 0.3f, 0.6f, 1};
        MakePin("Value", PinType::Float, true);
        break;
    }
}

// ── AddNode ────────────────────────────────────────────────────────────────
u32 NodeGraph::AddNode(VisualNodeType type, Vec2 pos) {
    VisualNode node;
    node.id = m_NextNodeID++;
    node.type = type;
    node.position = pos;
    InitNodePins(node);
    u32 id = node.id;
    m_Nodes.push_back(node);
    return id;
}

void NodeGraph::RemoveNode(u32 id) {
    for (size_t i = 0; i < m_Nodes.size(); ++i) {
        if (m_Nodes[i].id == id) {
            m_Nodes.erase(m_Nodes.begin() + static_cast<long>(i));
            break;
        }
    }
    // Remove connections involving this node
    for (size_t i = m_Connections.size(); i > 0; --i) {
        auto& c = m_Connections[i - 1];
        if (c.fromNodeID == id || c.toNodeID == id) {
            m_Connections.erase(m_Connections.begin() + static_cast<long>(i - 1));
        }
    }
}

VisualNode* NodeGraph::GetNode(u32 id) {
    for (auto& n : m_Nodes) { if (n.id == id) return &n; }
    return nullptr;
}

const VisualNode* NodeGraph::GetNode(u32 id) const {
    for (auto& n : m_Nodes) { if (n.id == id) return &n; }
    return nullptr;
}

// ── Connect / Disconnect ───────────────────────────────────────────────────
bool NodeGraph::Connect(u32 fromNode, u32 fromPin, u32 toNode, u32 toPin) {
    // Validate nodes exist
    auto* fn = GetNode(fromNode);
    auto* tn = GetNode(toNode);
    if (!fn || !tn) return false;

    // Remove existing connection on toPin
    Disconnect(toNode, toPin);

    NodeConnection conn;
    conn.fromNodeID = fromNode;
    conn.fromPinID  = fromPin;
    conn.toNodeID   = toNode;
    conn.toPinID    = toPin;
    m_Connections.push_back(conn);

    // Also set connectedNodeID on the pin
    for (auto& pin : tn->inputs) {
        if (pin.id == toPin) {
            pin.connectedNodeID = static_cast<i32>(fromNode);
            pin.connectedPinID  = static_cast<i32>(fromPin);
            break;
        }
    }
    return true;
}

void NodeGraph::Disconnect(u32 toNode, u32 toPinID) {
    auto* tn = GetNode(toNode);
    if (tn) {
        for (auto& pin : tn->inputs) {
            if (pin.id == toPinID) {
                pin.connectedNodeID = -1;
                pin.connectedPinID  = -1;
                break;
            }
        }
    }
    for (size_t i = m_Connections.size(); i > 0; --i) {
        auto& c = m_Connections[i - 1];
        if (c.toNodeID == toNode && c.toPinID == toPinID) {
            m_Connections.erase(m_Connections.begin() + static_cast<long>(i - 1));
        }
    }
}

// ── Execution ──────────────────────────────────────────────────────────────
void NodeGraph::ExecuteOnStart(Scene& scene, GameObject* self) {
    for (auto& n : m_Nodes) {
        if (n.type == VisualNodeType::OnStart && !n.outputs.empty()) {
            // Follow the flow output
            for (auto& c : m_Connections) {
                if (c.fromNodeID == n.id && c.fromPinID == n.outputs[0].id) {
                    ExecuteNode(c.toNodeID, scene, self, 0);
                }
            }
        }
    }
}

void NodeGraph::ExecuteOnUpdate(Scene& scene, GameObject* self, f32 dt) {
    for (auto& n : m_Nodes) {
        if (n.type == VisualNodeType::OnUpdate && !n.outputs.empty()) {
            for (auto& c : m_Connections) {
                if (c.fromNodeID == n.id && c.fromPinID == n.outputs[0].id) {
                    ExecuteNode(c.toNodeID, scene, self, dt);
                }
            }
        }
    }
}

void NodeGraph::ExecuteNode(u32 nodeID, Scene& scene, GameObject* self, f32 dt) {
    auto* node = GetNode(nodeID);
    if (!node) return;

    switch (node->type) {
    case VisualNodeType::Print:
        // In a real engine, route to console. For now, just log.
        break;

    case VisualNodeType::SetPosition:
        if (self) {
            Vec3 pos = EvaluateVec3(nodeID, 1, scene, self, dt);
            self->GetTransform()->SetPosition(pos);
        }
        // Follow flow output
        for (auto& c : m_Connections) {
            if (c.fromNodeID == nodeID) {
                for (auto& out : node->outputs) {
                    if (out.type == PinType::Flow && c.fromPinID == out.id) {
                        ExecuteNode(c.toNodeID, scene, self, dt);
                    }
                }
            }
        }
        break;

    case VisualNodeType::SetRotation:
        if (self) {
            Vec3 euler = EvaluateVec3(nodeID, 1, scene, self, dt);
            self->GetTransform()->SetEulerDeg(euler);
        }
        for (auto& c : m_Connections) {
            if (c.fromNodeID == nodeID) {
                for (auto& out : node->outputs) {
                    if (out.type == PinType::Flow && c.fromPinID == out.id)
                        ExecuteNode(c.toNodeID, scene, self, dt);
                }
            }
        }
        break;

    case VisualNodeType::SetScale:
        if (self) {
            Vec3 sc = EvaluateVec3(nodeID, 1, scene, self, dt);
            self->GetTransform()->SetScale(sc);
        }
        for (auto& c : m_Connections) {
            if (c.fromNodeID == nodeID) {
                for (auto& out : node->outputs) {
                    if (out.type == PinType::Flow && c.fromPinID == out.id)
                        ExecuteNode(c.toNodeID, scene, self, dt);
                }
            }
        }
        break;

    case VisualNodeType::Branch: {
        bool cond = EvaluateBool(nodeID, 1, scene, self, dt);
        // outputs[0] = True, outputs[1] = False
        u32 targetPin = cond ? node->outputs[0].id : node->outputs[1].id;
        for (auto& c : m_Connections) {
            if (c.fromNodeID == nodeID && c.fromPinID == targetPin) {
                ExecuteNode(c.toNodeID, scene, self, dt);
            }
        }
        break;
    }

    case VisualNodeType::Sequence:
        for (auto& out : node->outputs) {
            for (auto& c : m_Connections) {
                if (c.fromNodeID == nodeID && c.fromPinID == out.id) {
                    ExecuteNode(c.toNodeID, scene, self, dt);
                }
            }
        }
        break;

    case VisualNodeType::ForLoop: {
        i32 count = static_cast<i32>(EvaluateFloat(nodeID, 1, scene, self, dt));
        for (i32 i = 0; i < count; ++i) {
            // Body
            for (auto& c : m_Connections) {
                if (c.fromNodeID == nodeID && node->outputs.size() > 0 &&
                    c.fromPinID == node->outputs[0].id) {
                    ExecuteNode(c.toNodeID, scene, self, dt);
                }
            }
        }
        // Done
        for (auto& c : m_Connections) {
            if (c.fromNodeID == nodeID && node->outputs.size() > 1 &&
                c.fromPinID == node->outputs[1].id) {
                ExecuteNode(c.toNodeID, scene, self, dt);
            }
        }
        break;
    }

    case VisualNodeType::SetVariable: {
        f32 val = EvaluateFloat(nodeID, 1, scene, self, dt);
        SetVariable(node->variableName, val);
        for (auto& c : m_Connections) {
            if (c.fromNodeID == nodeID) {
                for (auto& out : node->outputs) {
                    if (out.type == PinType::Flow && c.fromPinID == out.id)
                        ExecuteNode(c.toNodeID, scene, self, dt);
                }
            }
        }
        break;
    }

    default:
        // Follow any flow outputs by default
        for (auto& c : m_Connections) {
            if (c.fromNodeID == nodeID) {
                for (auto& out : node->outputs) {
                    if (out.type == PinType::Flow && c.fromPinID == out.id)
                        ExecuteNode(c.toNodeID, scene, self, dt);
                }
            }
        }
        break;
    }
}

f32 NodeGraph::EvaluateFloat(u32 nodeID, i32 pinIndex, Scene& scene, GameObject* self, f32 dt) {
    auto* node = GetNode(nodeID);
    if (!node || pinIndex < 0 || pinIndex >= static_cast<i32>(node->inputs.size()))
        return 0;

    auto& pin = node->inputs[pinIndex];
    if (pin.connectedNodeID >= 0) {
        auto* src = GetNode(static_cast<u32>(pin.connectedNodeID));
        if (src) {
            switch (src->type) {
            case VisualNodeType::Add:
                return EvaluateFloat(src->id, 0, scene, self, dt) +
                       EvaluateFloat(src->id, 1, scene, self, dt);
            case VisualNodeType::Subtract:
                return EvaluateFloat(src->id, 0, scene, self, dt) -
                       EvaluateFloat(src->id, 1, scene, self, dt);
            case VisualNodeType::Multiply:
                return EvaluateFloat(src->id, 0, scene, self, dt) *
                       EvaluateFloat(src->id, 1, scene, self, dt);
            case VisualNodeType::Divide: {
                f32 b = EvaluateFloat(src->id, 1, scene, self, dt);
                return (std::abs(b) > 0.0001f) ?
                    EvaluateFloat(src->id, 0, scene, self, dt) / b : 0;
            }
            case VisualNodeType::Random: {
                f32 lo = EvaluateFloat(src->id, 0, scene, self, dt);
                f32 hi = EvaluateFloat(src->id, 1, scene, self, dt);
                return lo + static_cast<f32>(std::rand()) / static_cast<f32>(RAND_MAX) * (hi - lo);
            }
            case VisualNodeType::GetDeltaTime:
                return dt;
            case VisualNodeType::GetVariable:
                return GetVariable(src->variableName);
            default:
                return pin.floatVal;
            }
        }
    }
    return pin.floatVal;
}

Vec3 NodeGraph::EvaluateVec3(u32 nodeID, i32 pinIndex, Scene& scene, GameObject* self, f32 dt) {
    auto* node = GetNode(nodeID);
    if (!node || pinIndex < 0 || pinIndex >= static_cast<i32>(node->inputs.size()))
        return {};

    auto& pin = node->inputs[pinIndex];
    if (pin.connectedNodeID >= 0) {
        auto* src = GetNode(static_cast<u32>(pin.connectedNodeID));
        if (src) {
            switch (src->type) {
            case VisualNodeType::GetPosition:
                return self ? self->GetTransform()->position : Vec3{};
            default:
                return pin.vec3Val;
            }
        }
    }
    return pin.vec3Val;
}

bool NodeGraph::EvaluateBool(u32 nodeID, i32 pinIndex, Scene& scene, GameObject* self, f32 dt) {
    auto* node = GetNode(nodeID);
    if (!node || pinIndex < 0 || pinIndex >= static_cast<i32>(node->inputs.size()))
        return false;

    auto& pin = node->inputs[pinIndex];
    if (pin.connectedNodeID >= 0) {
        auto* src = GetNode(static_cast<u32>(pin.connectedNodeID));
        if (src) {
            switch (src->type) {
            case VisualNodeType::Equal:
                return std::abs(EvaluateFloat(src->id, 0, scene, self, dt) -
                               EvaluateFloat(src->id, 1, scene, self, dt)) < 0.001f;
            case VisualNodeType::Greater:
                return EvaluateFloat(src->id, 0, scene, self, dt) >
                       EvaluateFloat(src->id, 1, scene, self, dt);
            case VisualNodeType::Less:
                return EvaluateFloat(src->id, 0, scene, self, dt) <
                       EvaluateFloat(src->id, 1, scene, self, dt);
            case VisualNodeType::And:
                return EvaluateBool(src->id, 0, scene, self, dt) &&
                       EvaluateBool(src->id, 1, scene, self, dt);
            case VisualNodeType::Or:
                return EvaluateBool(src->id, 0, scene, self, dt) ||
                       EvaluateBool(src->id, 1, scene, self, dt);
            case VisualNodeType::Not:
                return !EvaluateBool(src->id, 0, scene, self, dt);
            default:
                return pin.boolVal;
            }
        }
    }
    return pin.boolVal;
}

// ── Variables ──────────────────────────────────────────────────────────────
void NodeGraph::SetVariable(const std::string& name, f32 val) {
    m_Variables[name] = val;
}

f32 NodeGraph::GetVariable(const std::string& name) const {
    auto it = m_Variables.find(name);
    return (it != m_Variables.end()) ? it->second : 0;
}

// ── Serialization ──────────────────────────────────────────────────────────
std::string NodeGraph::Serialize() const {
    std::ostringstream os;
    os << "graph:" << m_Name << "\n";
    os << "nodes:" << m_Nodes.size() << "\n";
    for (auto& n : m_Nodes) {
        os << "node:" << n.id << "," << static_cast<int>(n.type) << ","
           << n.position.x << "," << n.position.y << "\n";
    }
    os << "connections:" << m_Connections.size() << "\n";
    for (auto& c : m_Connections) {
        os << "conn:" << c.fromNodeID << "," << c.fromPinID << ","
           << c.toNodeID << "," << c.toPinID << "\n";
    }
    return os.str();
}

bool NodeGraph::Deserialize(const std::string& /*data*/) {
    // Placeholder for full deserialization
    return true;
}

// ── NodeGraphComponent ─────────────────────────────────────────────────────
void NodeGraphComponent::OnStart() {
    if (!m_Started && Owner) {
        // Get scene from owner — placeholder, would need scene reference
        m_Started = true;
    }
}

void NodeGraphComponent::OnUpdate(f32 /*dt*/) {
    // Execution requires Scene reference, handled externally
}

} // namespace gv
