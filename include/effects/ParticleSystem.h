// ============================================================================
// GameVoid Engine — Particle System
// ============================================================================
// CPU-simulated particle emitters: fire, smoke, sparks, rain, etc.
// Renders particles as instanced quads via a shared VAO.
// ============================================================================
#pragma once

#include "core/Types.h"
#include "core/Math.h"
#include "core/Component.h"
#include <vector>
#include <string>

namespace gv {

// ── Single particle ────────────────────────────────────────────────────────
struct Particle {
    Vec3 position  { 0, 0, 0 };
    Vec3 velocity  { 0, 0, 0 };
    Vec4 color     { 1, 1, 1, 1 };
    Vec4 colorEnd  { 1, 1, 1, 0 };
    f32  size      = 0.1f;
    f32  sizeEnd   = 0.0f;
    f32  lifetime  = 1.0f;
    f32  age       = 0.0f;
    bool alive     = false;
};

// ── Built-in presets ───────────────────────────────────────────────────────
enum class ParticlePreset { Custom, Fire, Smoke, Sparks, Rain, Snow, Explosion };

// ── Emitter shape ──────────────────────────────────────────────────────────
enum class EmitterShape { Point, Sphere, Cone, Box };

// ── Particle Emitter Component ─────────────────────────────────────────────
class ParticleEmitter : public Component {
public:
    ParticleEmitter() = default;
    ~ParticleEmitter() override = default;

    std::string GetTypeName() const override { return "ParticleEmitter"; }

    void OnStart() override;
    void OnUpdate(f32 dt) override;

    // ── Configuration ──────────────────────────────────────────────────────
    /// Apply one of the built-in presets.
    void ApplyPreset(ParticlePreset preset);

    // ── Emission parameters ────────────────────────────────────────────────
    f32 emissionRate    = 50.0f;     // particles per second
    u32 maxParticles    = 500;
    EmitterShape shape  = EmitterShape::Point;
    f32 shapeRadius     = 0.5f;      // for Sphere/Cone shapes

    // ── Particle parameters ────────────────────────────────────────────────
    f32  lifetimeMin    = 0.5f;
    f32  lifetimeMax    = 2.0f;
    f32  speedMin       = 1.0f;
    f32  speedMax       = 3.0f;
    Vec3 direction      { 0, 1, 0 };  // main emission direction
    f32  spread         = 30.0f;       // cone spread angle (degrees)
    Vec3 gravity        { 0, -2.0f, 0 };

    Vec4 colorStart     { 1.0f, 0.8f, 0.2f, 1.0f };
    Vec4 colorEnd       { 1.0f, 0.1f, 0.0f, 0.0f };
    f32  sizeStart      = 0.15f;
    f32  sizeEnd        = 0.02f;

    bool looping        = true;
    bool worldSpace     = true;

    // ── State ──────────────────────────────────────────────────────────────
    const std::vector<Particle>& GetParticles() const { return m_Particles; }
    u32 GetAliveCount() const { return m_AliveCount; }
    bool IsPlaying() const { return m_Playing; }
    void Play()  { m_Playing = true; }
    void Stop()  { m_Playing = false; }
    void Reset();

    // ── GPU rendering data ─────────────────────────────────────────────────
    u32 GetVAO() const { return m_VAO; }
    u32 GetInstanceVBO() const { return m_InstanceVBO; }
    void InitGPU();
    void UploadInstances();
    void CleanupGPU();

private:
    void EmitParticle();
    void SimulateParticle(Particle& p, f32 dt);

    std::vector<Particle> m_Particles;
    u32 m_AliveCount  = 0;
    f32 m_EmitAccum   = 0.0f;
    bool m_Playing    = true;

    // Instance data: [posX, posY, posZ, size, r, g, b, a] per particle
    std::vector<f32> m_InstanceData;
    u32 m_VAO = 0, m_QuadVBO = 0, m_InstanceVBO = 0;
    bool m_GPUReady = false;
};

} // namespace gv
