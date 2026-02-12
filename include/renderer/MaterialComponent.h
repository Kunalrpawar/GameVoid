// ============================================================================
// GameVoid Engine — Material Component
// ============================================================================
// An attachable Component wrapping PBRMaterial so that materials can be
// edited per-object in the Inspector (Godot style).
//
// Usage:
//   auto* mat = obj->AddComponent<MaterialComponent>();
//   mat->albedo = Vec4(1, 0, 0, 1);      // red
//   mat->metallic  = 0.8f;
//   mat->roughness = 0.2f;
//
// The renderer reads MaterialComponent at draw time; if present it overrides
// the flat colour in MeshRenderer.
// ============================================================================
#pragma once

#include "core/Component.h"
#include "core/Types.h"
#include "core/Math.h"
#include <string>

namespace gv {

class MaterialComponent : public Component {
public:
    MaterialComponent() = default;
    ~MaterialComponent() override = default;

    std::string GetTypeName() const override { return "Material"; }

    // ── PBR properties (editable in Inspector) ─────────────────────────────
    Vec4 albedo     { 0.8f, 0.8f, 0.8f, 1.0f };  // base colour + alpha
    f32  metallic   = 0.0f;                        // 0 = dielectric, 1 = metal
    f32  roughness  = 0.5f;                        // 0 = mirror, 1 = rough
    Vec3 emission   { 0, 0, 0 };
    f32  emissionStrength = 0.0f;
    f32  ao         = 1.0f;                        // ambient occlusion

    // ── Texture slots (IDs — 0 = no texture) ──────────────────────────────
    u32 albedoMap    = 0;
    u32 normalMap    = 0;
    u32 roughnessMap = 0;
    u32 metallicMap  = 0;

    // ── Name ───────────────────────────────────────────────────────────────
    const std::string& GetMaterialName() const { return m_MatName; }
    void SetMaterialName(const std::string& n) { m_MatName = n; }

    // ── Live-update hook ───────────────────────────────────────────────────
    /// Called by the Inspector or script code after changing properties.
    /// Pushes the new values into the MeshRenderer colour so the viewport
    /// updates immediately.  (Full PBR pipeline would upload uniforms to GPU.)
    void ApplyToMeshRenderer();

private:
    std::string m_MatName = "Default";
};

} // namespace gv
