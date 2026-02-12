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
#include <vector>

namespace gv {

// Forward declarations (defined in Assets module)
class Mesh;
class Material;
class IRenderer;

// ── Built-in primitive shapes ──────────────────────────────────────────────
/// Identifies which built-in mesh the renderer should draw.
/// In a full engine the MeshRenderer would reference a Mesh asset instead.
enum class PrimitiveType : int { None, Triangle, Cube, Plane };

// ── LOD Level (per-object) ─────────────────────────────────────────────────
/// Defines a distance threshold at which a lower-detail primitive is used.
struct LODLevel {
    f32           maxDistance    = 50.0f;           // switch beyond this distance
    PrimitiveType primitiveType = PrimitiveType::None; // mesh at this LOD
    f32           scaleFactor   = 1.0f;            // optional size adjustment
};

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

    // ── LOD levels (optional) ──────────────────────────────────────────────
    /// When populated, the renderer picks the appropriate LOD based on
    /// camera distance.  LODs should be sorted by ascending maxDistance.
    std::vector<LODLevel> lodLevels;

    /// Helper: returns the PrimitiveType to draw for the given camera distance.
    /// If no LOD levels are set, returns the base primitiveType.
    PrimitiveType GetLODPrimitive(f32 distance) const {
        for (auto& lod : lodLevels) {
            if (distance <= lod.maxDistance)
                return lod.primitiveType;
        }
        // Beyond all LOD ranges — return the last LOD, or skip rendering
        if (!lodLevels.empty())
            return lodLevels.back().primitiveType;
        return primitiveType;
    }

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
    f32  width   = 64.0f;          // screen-space width
    f32  height  = 64.0f;          // screen-space height

    /// Get the GL texture handle.
    u32 GetTextureID() const { return m_TextureID; }
    void SetTextureID(u32 id) { m_TextureID = id; }

    void OnRender() override {
        // Sprite draw is handled by the Renderer's DrawTexture / DrawRect.
        // No-op here; the renderer queries SpriteRenderer components.
    }

private:
    u32 m_TextureID = 0;
};

} // namespace gv
