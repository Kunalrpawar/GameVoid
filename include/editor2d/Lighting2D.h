// ============================================================================
// GameVoid Engine — 2D Lighting System
// ============================================================================
// Adds dynamic lighting to 2D scenes with point lights, ambient light,
// and soft shadow / falloff rendering.
//
// Components:
//   - AmbientLight2D : global colour & intensity that fills the whole scene
//   - PointLight2D   : radial light with colour, radius, intensity, flicker
//   - SpotLight2D    : cone-shaped directional light
//   - LightWorld2D   : manager that collects all lights and computes a
//                       per-pixel light map (soft circle approximation)
//
// The viewport renderer queries LightWorld2D::GatherLights() each frame
// and blends the result over the sprite layer as a multiply pass.
// ============================================================================
#pragma once

#include "core/Types.h"
#include "core/Math.h"
#include "core/Component.h"
#include <vector>
#include <string>

namespace gv {

// Forward
class Scene2D;
class GameObject;

// ════════════════════════════════════════════════════════════════════════════
// AmbientLight2D — fills the scene with a base light colour
// ════════════════════════════════════════════════════════════════════════════
class AmbientLight2D : public Component {
public:
    std::string GetTypeName() const override { return "AmbientLight2D"; }

    Vec4 color      { 0.15f, 0.15f, 0.2f, 1.0f };  // dark blue-ish default
    f32  intensity  = 0.3f;                           // 0 = pitch black, 1 = full bright
};

// ════════════════════════════════════════════════════════════════════════════
// PointLight2D — radial light attached to a GameObject
// ════════════════════════════════════════════════════════════════════════════
class PointLight2D : public Component {
public:
    std::string GetTypeName() const override { return "PointLight2D"; }

    // ── Appearance ─────────────────────────────────────────────────────────
    Vec4 color       { 1.0f, 0.9f, 0.7f, 1.0f };    // warm yellow default
    f32  intensity   = 1.0f;                           // brightness multiplier
    f32  radius      = 6.0f;                           // world-unit radius of the light
    f32  falloff     = 2.0f;                           // exponent for distance attenuation
                                                        // 1 = linear, 2 = quadratic (default)

    // ── Flicker (optional) ─────────────────────────────────────────────────
    bool flickerEnabled   = false;
    f32  flickerSpeed     = 8.0f;     // oscillation speed
    f32  flickerAmount    = 0.15f;    // intensity variation (±)
    f32  flickerTimer     = 0.0f;     // internal timer

    // ── Computed each frame ────────────────────────────────────────────────
    f32  effectiveIntensity = 1.0f;

    /// Call every frame to update flicker.
    void UpdateFlicker(f32 dt);
};

// ════════════════════════════════════════════════════════════════════════════
// SpotLight2D — directional cone of light
// ════════════════════════════════════════════════════════════════════════════
class SpotLight2D : public Component {
public:
    std::string GetTypeName() const override { return "SpotLight2D"; }

    Vec4 color         { 1, 1, 1, 1 };
    f32  intensity     = 1.0f;
    f32  range         = 10.0f;        // how far the beam reaches
    f32  innerAngle    = 15.0f;        // degrees — full-bright cone
    f32  outerAngle    = 30.0f;        // degrees — falloff cone
    f32  direction     = 0.0f;         // angle in degrees (0 = right)
    f32  falloff       = 2.0f;
};

// ════════════════════════════════════════════════════════════════════════════
// LightInfo2D — flattened per-light data for rendering
// ════════════════════════════════════════════════════════════════════════════
struct LightInfo2D {
    enum class Kind { Point, Spot };
    Kind kind = Kind::Point;

    Vec2 position   { 0, 0 };
    Vec4 color      { 1, 1, 1, 1 };
    f32  intensity  = 1.0f;
    f32  radius     = 6.0f;
    f32  falloff    = 2.0f;

    // Spot-only
    f32  direction  = 0.0f;
    f32  innerAngle = 15.0f;
    f32  outerAngle = 30.0f;
};

// ════════════════════════════════════════════════════════════════════════════
// LightWorld2D — collects all light components and produces a render list
// ════════════════════════════════════════════════════════════════════════════
class LightWorld2D {
public:
    LightWorld2D() = default;

    // ── Settings ───────────────────────────────────────────────────────────
    bool enabled          = true;      // master switch for 2D lighting

    // ── Interface ──────────────────────────────────────────────────────────

    /// Scan the scene and update internal light list.  Call once per frame.
    void GatherLights(Scene2D* scene, f32 dt);

    /// Get the ambient colour × intensity (pre-multiplied).
    Vec4 GetAmbientColor() const { return m_Ambient; }

    /// Get the list of active lights for the renderer.
    const std::vector<LightInfo2D>& GetLights() const { return m_Lights; }

    /// Compute the light contribution at a given world point.
    /// Returns an additive RGB colour that the sprite renderer can
    /// multiply / blend with.
    Vec4 SampleLightAt(f32 worldX, f32 worldY) const;

private:
    Vec4 m_Ambient { 0.15f, 0.15f, 0.2f, 1.0f };
    std::vector<LightInfo2D> m_Lights;
};

} // namespace gv
