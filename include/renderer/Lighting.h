// ============================================================================
// GameVoid Engine — Lighting System
// ============================================================================
// Defines three light types used by the renderer:
//   • AmbientLight  — constant colour added to every fragment
//   • DirectionalLight — infinite-distance light with a direction vector
//   • PointLight    — positional light with attenuation
// Each light type is a Component that can be attached to a GameObject.
// ============================================================================
#pragma once

#include "core/Component.h"
#include "core/Math.h"

namespace gv {

// ─── Ambient Light ─────────────────────────────────────────────────────────
class AmbientLight : public Component {
public:
    Vec3 colour    { 1, 1, 1 };
    f32  intensity = 0.15f;

    std::string GetTypeName() const override { return "AmbientLight"; }
};

// ─── Directional Light ─────────────────────────────────────────────────────
class DirectionalLight : public Component {
public:
    Vec3 direction { 0, -1, 0 };    // default = straight down
    Vec3 colour    { 1, 1, 1 };
    f32  intensity = 1.0f;

    std::string GetTypeName() const override { return "DirectionalLight"; }
};

// ─── Point Light ───────────────────────────────────────────────────────────
class PointLight : public Component {
public:
    Vec3 colour    { 1, 1, 1 };
    f32  intensity = 1.0f;

    // Attenuation (1 / (constant + linear*d + quadratic*d²))
    f32 constant  = 1.0f;
    f32 linear    = 0.09f;
    f32 quadratic = 0.032f;

    f32 range     = 50.0f;          // soft cut-off distance

    std::string GetTypeName() const override { return "PointLight"; }
};

// ─── Spot Light (placeholder for future) ───────────────────────────────────
class SpotLight : public Component {
public:
    Vec3 direction   { 0, -1, 0 };
    Vec3 colour      { 1, 1, 1 };
    f32  intensity   = 1.0f;
    f32  innerCutoff = 12.5f;       // degrees
    f32  outerCutoff = 17.5f;       // degrees

    std::string GetTypeName() const override { return "SpotLight"; }
};

} // namespace gv
