// ============================================================================
// GameVoid Engine — 2D Lighting System Implementation
// ============================================================================
#include "editor2d/Lighting2D.h"
#include "editor2d/Scene2D.h"
#include "core/GameObject.h"
#include <cmath>

namespace gv {

// ════════════════════════════════════════════════════════════════════════════
// PointLight2D — flicker
// ════════════════════════════════════════════════════════════════════════════

void PointLight2D::UpdateFlicker(f32 dt) {
    if (flickerEnabled) {
        flickerTimer += dt * flickerSpeed;
        // Use a combo of sin waves for organic-looking flicker
        f32 noise = std::sin(flickerTimer) * 0.6f
                  + std::sin(flickerTimer * 2.37f) * 0.3f
                  + std::sin(flickerTimer * 5.13f) * 0.1f;
        effectiveIntensity = intensity + noise * flickerAmount;
        if (effectiveIntensity < 0.0f) effectiveIntensity = 0.0f;
    } else {
        effectiveIntensity = intensity;
    }
}

// ════════════════════════════════════════════════════════════════════════════
// LightWorld2D
// ════════════════════════════════════════════════════════════════════════════

void LightWorld2D::GatherLights(Scene2D* scene, f32 dt) {
    m_Lights.clear();
    m_Ambient = { 0.15f, 0.15f, 0.2f, 1.0f };  // default if no ambient light

    if (!scene || !enabled) return;

    for (auto& obj : scene->GetAllObjects()) {
        if (!obj->IsActive()) continue;

        // ── Ambient ────────────────────────────────────────────────────────
        auto* amb = obj->GetComponent<AmbientLight2D>();
        if (amb && amb->IsEnabled()) {
            m_Ambient = {
                amb->color.x * amb->intensity,
                amb->color.y * amb->intensity,
                amb->color.z * amb->intensity,
                1.0f
            };
        }

        // ── Point lights ───────────────────────────────────────────────────
        auto* pl = obj->GetComponent<PointLight2D>();
        if (pl && pl->IsEnabled()) {
            pl->UpdateFlicker(dt);

            LightInfo2D info;
            info.kind      = LightInfo2D::Kind::Point;
            info.position  = { obj->GetTransform().position.x,
                               obj->GetTransform().position.y };
            info.color     = pl->color;
            info.intensity = pl->effectiveIntensity;
            info.radius    = pl->radius;
            info.falloff   = pl->falloff;
            m_Lights.push_back(info);
        }

        // ── Spot lights ────────────────────────────────────────────────────
        auto* sl = obj->GetComponent<SpotLight2D>();
        if (sl && sl->IsEnabled()) {
            LightInfo2D info;
            info.kind       = LightInfo2D::Kind::Spot;
            info.position   = { obj->GetTransform().position.x,
                                obj->GetTransform().position.y };
            info.color      = sl->color;
            info.intensity  = sl->intensity;
            info.radius     = sl->range;
            info.falloff    = sl->falloff;
            info.direction  = sl->direction;
            info.innerAngle = sl->innerAngle;
            info.outerAngle = sl->outerAngle;
            m_Lights.push_back(info);
        }
    }
}

Vec4 LightWorld2D::SampleLightAt(f32 worldX, f32 worldY) const {
    // Start with ambient
    f32 r = m_Ambient.x;
    f32 g = m_Ambient.y;
    f32 b = m_Ambient.z;

    for (auto& light : m_Lights) {
        f32 dx = worldX - light.position.x;
        f32 dy = worldY - light.position.y;
        f32 dist = std::sqrt(dx * dx + dy * dy);

        if (dist >= light.radius) continue;

        if (light.kind == LightInfo2D::Kind::Point) {
            // Smooth attenuation: 1 - (dist / radius)^falloff
            f32 t = dist / light.radius;
            f32 atten = 1.0f - std::pow(t, light.falloff);
            if (atten < 0.0f) atten = 0.0f;

            r += light.color.x * light.intensity * atten;
            g += light.color.y * light.intensity * atten;
            b += light.color.z * light.intensity * atten;
        }
        else if (light.kind == LightInfo2D::Kind::Spot) {
            // Direction from light to sample point
            f32 angleToPoint = std::atan2(dy, dx) * (180.0f / 3.14159265f);
            f32 angleDiff = angleToPoint - light.direction;
            // Normalize to -180..180
            while (angleDiff > 180.0f)  angleDiff -= 360.0f;
            while (angleDiff < -180.0f) angleDiff += 360.0f;
            f32 absAngle = std::fabs(angleDiff);

            if (absAngle > light.outerAngle) continue;

            // Distance attenuation
            f32 t = dist / light.radius;
            f32 distAtten = 1.0f - std::pow(t, light.falloff);
            if (distAtten < 0.0f) distAtten = 0.0f;

            // Angular attenuation
            f32 angAtten = 1.0f;
            if (absAngle > light.innerAngle) {
                angAtten = 1.0f - (absAngle - light.innerAngle) /
                           (light.outerAngle - light.innerAngle);
                if (angAtten < 0.0f) angAtten = 0.0f;
            }

            f32 atten = distAtten * angAtten;
            r += light.color.x * light.intensity * atten;
            g += light.color.y * light.intensity * atten;
            b += light.color.z * light.intensity * atten;
        }
    }

    // Clamp to 0..1
    if (r > 1.0f) r = 1.0f;
    if (g > 1.0f) g = 1.0f;
    if (b > 1.0f) b = 1.0f;

    return { r, g, b, 1.0f };
}

} // namespace gv
