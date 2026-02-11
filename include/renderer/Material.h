// ============================================================================
// GameVoid Engine — PBR Material System
// ============================================================================
// Material class with albedo, roughness, metallic, emission.
// Shader-graph placeholder with node-based system.
// ============================================================================
#pragma once

#include "core/Types.h"
#include "core/Math.h"
#include "core/Component.h"
#include <string>
#include <vector>
#include <map>

namespace gv {

// ── PBR Material ───────────────────────────────────────────────────────────
/// Holds PBR material properties for rendering.
class PBRMaterial {
public:
    PBRMaterial(const std::string& name = "Default Material")
        : m_Name(name) {}

    // ── Core PBR properties ────────────────────────────────────────────────
    Vec4 albedo    { 0.8f, 0.8f, 0.8f, 1.0f }; // base colour + alpha
    f32  roughness = 0.5f;                       // 0 = mirror, 1 = rough
    f32  metallic  = 0.0f;                       // 0 = dielectric, 1 = metal
    Vec3 emission  { 0, 0, 0 };                  // emissive colour
    f32  emissionStrength = 0.0f;
    f32  normalStrength   = 1.0f;
    f32  ao = 1.0f;                              // ambient occlusion

    // ── Texture slots (IDs — 0 = no texture) ──────────────────────────────
    u32 albedoMap    = 0;
    u32 normalMap    = 0;
    u32 roughnessMap = 0;
    u32 metallicMap  = 0;
    u32 aoMap        = 0;
    u32 emissionMap  = 0;

    // ── Identity ───────────────────────────────────────────────────────────
    const std::string& GetName() const { return m_Name; }
    void SetName(const std::string& n) { m_Name = n; }
    u32 GetID() const { return m_ID; }
    void SetID(u32 id) { m_ID = id; }

    // ── Serialization placeholder ──────────────────────────────────────────
    std::string Serialize() const;
    bool Deserialize(const std::string& data);

private:
    std::string m_Name;
    u32 m_ID = 0;
};

// ── Shader Graph Node (placeholder) ────────────────────────────────────────
enum class ShaderNodeType {
    Output,          // final material output
    AlbedoColor,     // colour constant
    TextureSample,   // sample a texture
    NormalMap,       // normal map processing
    Multiply,        // A * B
    Add,             // A + B
    Lerp,            // mix(A, B, t)
    Fresnel,         // fresnel effect
    Time,            // animated time value
    UVCoord,         // UV coordinates
};

struct ShaderNodePin {
    std::string name;
    bool isOutput = false;
    i32 connectedNode = -1;
    i32 connectedPin  = -1;
    Vec4 defaultValue { 0, 0, 0, 1 };
};

struct ShaderGraphNode {
    u32 id = 0;
    ShaderNodeType type = ShaderNodeType::AlbedoColor;
    Vec2 position { 0, 0 };               // editor position
    std::vector<ShaderNodePin> inputs;
    std::vector<ShaderNodePin> outputs;
    Vec4 value { 1, 1, 1, 1 };            // node-specific parameter
    std::string label;
};

/// A visual shader graph that produces a PBRMaterial's expressions.
class ShaderGraph {
public:
    ShaderGraph() = default;

    u32 AddNode(ShaderNodeType type, Vec2 pos = {});
    void RemoveNode(u32 id);
    bool Connect(u32 fromNode, i32 fromPin, u32 toNode, i32 toPin);
    void Disconnect(u32 toNode, i32 toPin);

    const std::vector<ShaderGraphNode>& GetNodes() const { return m_Nodes; }
    std::vector<ShaderGraphNode>& GetNodesMut() { return m_Nodes; }

    /// Evaluate the graph into a PBRMaterial (simplified).
    PBRMaterial Evaluate() const;

    /// Get/set graph name.
    const std::string& GetName() const { return m_Name; }
    void SetName(const std::string& n) { m_Name = n; }

private:
    std::vector<ShaderGraphNode> m_Nodes;
    std::string m_Name = "New Shader Graph";
    u32 m_NextID = 1;
};

// ── Material Library ───────────────────────────────────────────────────────
/// Manages all materials loaded in the engine.
class MaterialLibrary {
public:
    PBRMaterial* CreateMaterial(const std::string& name);
    PBRMaterial* GetMaterial(const std::string& name);
    PBRMaterial* GetMaterial(u32 id);
    void RemoveMaterial(const std::string& name);

    const std::map<std::string, PBRMaterial>& GetAll() const { return m_Materials; }

private:
    std::map<std::string, PBRMaterial> m_Materials;
    u32 m_NextID = 1;
};

} // namespace gv
