// ============================================================================
// GameVoid Engine — Material System Implementation
// ============================================================================
#include "renderer/Material.h"
#include <sstream>

namespace gv {

// ============================================================================
// PBRMaterial Serialization
// ============================================================================
std::string PBRMaterial::Serialize() const {
    std::ostringstream os;
    os << "material:" << m_Name << "\n";
    os << "albedo:" << albedo.x << "," << albedo.y << "," << albedo.z << "," << albedo.w << "\n";
    os << "roughness:" << roughness << "\n";
    os << "metallic:" << metallic << "\n";
    os << "emission:" << emission.x << "," << emission.y << "," << emission.z << "\n";
    os << "emissionStr:" << emissionStrength << "\n";
    os << "ao:" << ao << "\n";
    return os.str();
}

bool PBRMaterial::Deserialize(const std::string& data) {
    std::istringstream is(data);
    std::string line;
    while (std::getline(is, line)) {
        if (line.find("material:") == 0) {
            m_Name = line.substr(9);
        } else if (line.find("albedo:") == 0) {
            std::string v = line.substr(7);
            if (std::sscanf(v.c_str(), "%f,%f,%f,%f", &albedo.x, &albedo.y, &albedo.z, &albedo.w) != 4)
                return false;
        } else if (line.find("roughness:") == 0) {
            roughness = std::stof(line.substr(10));
        } else if (line.find("metallic:") == 0) {
            metallic = std::stof(line.substr(9));
        } else if (line.find("emission:") == 0) {
            std::string v = line.substr(9);
            if (std::sscanf(v.c_str(), "%f,%f,%f", &emission.x, &emission.y, &emission.z) != 3)
                return false;
        } else if (line.find("emissionStr:") == 0) {
            emissionStrength = std::stof(line.substr(12));
        } else if (line.find("ao:") == 0) {
            ao = std::stof(line.substr(3));
        }
    }
    return true;
}

// ============================================================================
// ShaderGraph
// ============================================================================
u32 ShaderGraph::AddNode(ShaderNodeType type, Vec2 pos) {
    ShaderGraphNode node;
    node.id = m_NextID++;
    node.type = type;
    node.position = pos;

    switch (type) {
    case ShaderNodeType::Output:
        node.label = "Material Output";
        node.inputs.push_back({"Albedo", false, -1, -1, {0.8f, 0.8f, 0.8f, 1}});
        node.inputs.push_back({"Roughness", false, -1, -1, {0.5f, 0, 0, 0}});
        node.inputs.push_back({"Metallic", false, -1, -1, {0, 0, 0, 0}});
        node.inputs.push_back({"Emission", false, -1, -1, {0, 0, 0, 0}});
        node.inputs.push_back({"Normal", false, -1, -1, {0.5f, 0.5f, 1, 0}});
        break;
    case ShaderNodeType::AlbedoColor:
        node.label = "Color";
        node.outputs.push_back({"Color", true});
        node.value = {0.8f, 0.8f, 0.8f, 1};
        break;
    case ShaderNodeType::TextureSample:
        node.label = "Texture";
        node.inputs.push_back({"UV", false});
        node.outputs.push_back({"Color", true});
        node.outputs.push_back({"Alpha", true});
        break;
    case ShaderNodeType::Multiply:
        node.label = "Multiply";
        node.inputs.push_back({"A", false, -1, -1, {1, 1, 1, 1}});
        node.inputs.push_back({"B", false, -1, -1, {1, 1, 1, 1}});
        node.outputs.push_back({"Result", true});
        break;
    case ShaderNodeType::Add:
        node.label = "Add";
        node.inputs.push_back({"A", false});
        node.inputs.push_back({"B", false});
        node.outputs.push_back({"Result", true});
        break;
    case ShaderNodeType::Lerp:
        node.label = "Lerp";
        node.inputs.push_back({"A", false});
        node.inputs.push_back({"B", false});
        node.inputs.push_back({"Factor", false, -1, -1, {0.5f, 0, 0, 0}});
        node.outputs.push_back({"Result", true});
        break;
    case ShaderNodeType::Fresnel:
        node.label = "Fresnel";
        node.inputs.push_back({"Power", false, -1, -1, {5, 0, 0, 0}});
        node.outputs.push_back({"Factor", true});
        break;
    case ShaderNodeType::Time:
        node.label = "Time";
        node.outputs.push_back({"Time", true});
        node.outputs.push_back({"Sin", true});
        node.outputs.push_back({"Cos", true});
        break;
    case ShaderNodeType::UVCoord:
        node.label = "UV";
        node.outputs.push_back({"UV", true});
        break;
    default:
        node.label = "Node";
        break;
    }

    u32 id = node.id;
    m_Nodes.push_back(node);
    return id;
}

void ShaderGraph::RemoveNode(u32 id) {
    for (size_t i = 0; i < m_Nodes.size(); ++i) {
        if (m_Nodes[i].id == id) {
            m_Nodes.erase(m_Nodes.begin() + static_cast<long>(i));
            break;
        }
    }
    // Clean up connections referencing this node
    for (auto& n : m_Nodes) {
        for (auto& pin : n.inputs) {
            if (pin.connectedNode == static_cast<i32>(id)) {
                pin.connectedNode = -1;
                pin.connectedPin = -1;
            }
        }
    }
}

bool ShaderGraph::Connect(u32 fromNode, i32 fromPin, u32 toNode, i32 toPin) {
    ShaderGraphNode* to = nullptr;
    for (auto& n : m_Nodes) { if (n.id == toNode) { to = &n; break; } }
    if (!to || toPin < 0 || toPin >= static_cast<i32>(to->inputs.size())) return false;

    to->inputs[toPin].connectedNode = static_cast<i32>(fromNode);
    to->inputs[toPin].connectedPin = fromPin;
    return true;
}

void ShaderGraph::Disconnect(u32 toNode, i32 toPin) {
    for (auto& n : m_Nodes) {
        if (n.id == toNode && toPin >= 0 && toPin < static_cast<i32>(n.inputs.size())) {
            n.inputs[toPin].connectedNode = -1;
            n.inputs[toPin].connectedPin = -1;
            break;
        }
    }
}

PBRMaterial ShaderGraph::Evaluate() const {
    PBRMaterial mat;
    // Find output node
    for (auto& n : m_Nodes) {
        if (n.type == ShaderNodeType::Output) {
            // Albedo
            if (n.inputs.size() > 0 && n.inputs[0].connectedNode >= 0) {
                for (auto& src : m_Nodes) {
                    if (src.id == static_cast<u32>(n.inputs[0].connectedNode)) {
                        mat.albedo = src.value;
                        break;
                    }
                }
            } else if (n.inputs.size() > 0) {
                mat.albedo = n.inputs[0].defaultValue;
            }
            // Roughness
            if (n.inputs.size() > 1) {
                mat.roughness = n.inputs[1].defaultValue.x;
                if (n.inputs[1].connectedNode >= 0) {
                    for (auto& src : m_Nodes) {
                        if (src.id == static_cast<u32>(n.inputs[1].connectedNode)) {
                            mat.roughness = src.value.x;
                            break;
                        }
                    }
                }
            }
            // Metallic
            if (n.inputs.size() > 2) {
                mat.metallic = n.inputs[2].defaultValue.x;
                if (n.inputs[2].connectedNode >= 0) {
                    for (auto& src : m_Nodes) {
                        if (src.id == static_cast<u32>(n.inputs[2].connectedNode)) {
                            mat.metallic = src.value.x;
                            break;
                        }
                    }
                }
            }
            break;
        }
    }
    return mat;
}

// ============================================================================
// MaterialLibrary
// ============================================================================
PBRMaterial* MaterialLibrary::CreateMaterial(const std::string& name) {
    PBRMaterial mat(name);
    mat.SetID(m_NextID++);
    m_Materials[name] = mat;
    return &m_Materials[name];
}

PBRMaterial* MaterialLibrary::GetMaterial(const std::string& name) {
    auto it = m_Materials.find(name);
    return (it != m_Materials.end()) ? &it->second : nullptr;
}

PBRMaterial* MaterialLibrary::GetMaterial(u32 id) {
    for (auto& kv : m_Materials) {
        if (kv.second.GetID() == id) return &kv.second;
    }
    return nullptr;
}

void MaterialLibrary::RemoveMaterial(const std::string& name) {
    m_Materials.erase(name);
}

} // namespace gv
