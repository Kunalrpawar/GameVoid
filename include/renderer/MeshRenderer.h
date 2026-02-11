// ============================================================================
// GameVoid Engine — Mesh Renderer Component
// ============================================================================
// Attaches mesh + material data to a GameObject so the Renderer can draw it.
// ============================================================================
#pragma once

#include "core/Component.h"
#include "core/Types.h"
#include "core/Math.h"
#include <string>

namespace gv {

// Forward declarations (defined in Assets module)
class Mesh;
class Material;

// ── Built-in primitive shapes ──────────────────────────────────────────────
/// Identifies which built-in mesh the renderer should draw.
/// In a full engine the MeshRenderer would reference a Mesh asset instead.
enum class PrimitiveType : int { None, Triangle, Cube, Plane };

/// Component that holds a reference to a Mesh and a Material.
/// The renderer inspects GameObjects for this component when drawing a scene.
class MeshRenderer : public Component {
public:
    MeshRenderer() = default;
    ~MeshRenderer() override = default;

    std::string GetTypeName() const override { return "MeshRenderer"; }

    // ── Built-in primitive (quick setup, no Mesh asset needed) ─────────────
    PrimitiveType primitiveType = PrimitiveType::None;
    Vec4 color { 0.8f, 0.3f, 0.2f, 1.0f };   // flat colour (RGBA)

    // ── Asset-based data (used when primitiveType == None) ─────────────────
    void SetMesh(Shared<Mesh> mesh)         { m_Mesh = std::move(mesh); }
    void SetMaterial(Shared<Material> mat)   { m_Material = std::move(mat); }

    Shared<Mesh>     GetMesh()     const { return m_Mesh; }
    Shared<Material> GetMaterial() const { return m_Material; }

    void OnRender() override {
        // The actual draw call is issued by the Renderer which queries the
        // MeshRenderer for mesh/material data.  This callback is a hook for
        // per-object pre-render logic (e.g. updating shader uniforms).
    }

private:
    Shared<Mesh>     m_Mesh;
    Shared<Material> m_Material;
};

/// Component for 2D sprite rendering (quad with texture).
class SpriteRenderer : public Component {
public:
    SpriteRenderer() = default;
    ~SpriteRenderer() override = default;

    std::string GetTypeName() const override { return "SpriteRenderer"; }

    // Sprite-specific data
    std::string texturePath;        // path to the sprite texture
    Vec2 tiling  { 1, 1 };         // UV tiling
    Vec2 offset  { 0, 0 };         // UV offset
    Vec4 colour  { 1, 1, 1, 1 };   // tint colour (RGBA)

    void OnRender() override {
        // Placeholder for 2D sprite drawing logic.
    }

private:
    // Texture handle would go here.
    u32 m_TextureID = 0;
};

} // namespace gv
