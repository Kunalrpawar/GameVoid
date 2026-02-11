// ============================================================================
// GameVoid Engine — Particle System Implementation
// ============================================================================
#include "effects/ParticleSystem.h"
#include <cmath>
#include <cstdlib>
#include <algorithm>

#ifdef GV_HAS_GLFW
#include "core/GLDefs.h"
#endif

namespace gv {

// ── Helpers ────────────────────────────────────────────────────────────────
static f32 Randf() { return static_cast<f32>(std::rand()) / static_cast<f32>(RAND_MAX); }
static f32 Randf(f32 lo, f32 hi) { return lo + Randf() * (hi - lo); }
static f32 LerpF(f32 a, f32 b, f32 t) { return a + t * (b - a); }
static Vec4 LerpV4(Vec4 a, Vec4 b, f32 t) {
    return { LerpF(a.x, b.x, t), LerpF(a.y, b.y, t),
             LerpF(a.z, b.z, t), LerpF(a.w, b.w, t) };
}

// ── Presets ────────────────────────────────────────────────────────────────
void ParticleEmitter::ApplyPreset(ParticlePreset preset) {
    switch (preset) {
    case ParticlePreset::Fire:
        emissionRate = 80; maxParticles = 600;
        lifetimeMin = 0.3f; lifetimeMax = 1.2f;
        speedMin = 1.5f; speedMax = 3.5f;
        direction = { 0, 1, 0 }; spread = 15.0f;
        gravity = { 0, 0.5f, 0 };
        colorStart = { 1, 0.85f, 0.15f, 1 };
        colorEnd   = { 1, 0.1f, 0.0f, 0 };
        sizeStart = 0.18f; sizeEnd = 0.03f;
        break;
    case ParticlePreset::Smoke:
        emissionRate = 30; maxParticles = 400;
        lifetimeMin = 1.5f; lifetimeMax = 4.0f;
        speedMin = 0.3f; speedMax = 1.0f;
        direction = { 0, 1, 0 }; spread = 25.0f;
        gravity = { 0, 0.2f, 0 };
        colorStart = { 0.5f, 0.5f, 0.5f, 0.6f };
        colorEnd   = { 0.3f, 0.3f, 0.3f, 0.0f };
        sizeStart = 0.12f; sizeEnd = 0.35f;
        break;
    case ParticlePreset::Sparks:
        emissionRate = 120; maxParticles = 800;
        lifetimeMin = 0.2f; lifetimeMax = 0.8f;
        speedMin = 4.0f; speedMax = 8.0f;
        direction = { 0, 1, 0 }; spread = 60.0f;
        gravity = { 0, -9.8f, 0 };
        colorStart = { 1, 0.9f, 0.5f, 1 };
        colorEnd   = { 1, 0.4f, 0.1f, 0 };
        sizeStart = 0.04f; sizeEnd = 0.01f;
        break;
    case ParticlePreset::Rain:
        emissionRate = 200; maxParticles = 2000;
        lifetimeMin = 0.8f; lifetimeMax = 1.5f;
        speedMin = 8.0f; speedMax = 12.0f;
        direction = { 0, -1, 0 }; spread = 3.0f;
        gravity = { 0, -2.0f, 0 };
        colorStart = { 0.6f, 0.7f, 0.9f, 0.5f };
        colorEnd   = { 0.4f, 0.5f, 0.8f, 0.0f };
        sizeStart = 0.02f; sizeEnd = 0.01f;
        shape = EmitterShape::Box; shapeRadius = 10.0f;
        break;
    case ParticlePreset::Snow:
        emissionRate = 60; maxParticles = 1000;
        lifetimeMin = 3.0f; lifetimeMax = 6.0f;
        speedMin = 0.3f; speedMax = 0.8f;
        direction = { 0, -1, 0 }; spread = 20.0f;
        gravity = { 0, -0.3f, 0 };
        colorStart = { 1, 1, 1, 0.9f };
        colorEnd   = { 0.9f, 0.95f, 1, 0.0f };
        sizeStart = 0.06f; sizeEnd = 0.03f;
        shape = EmitterShape::Box; shapeRadius = 10.0f;
        break;
    case ParticlePreset::Explosion:
        emissionRate = 500; maxParticles = 500;
        lifetimeMin = 0.3f; lifetimeMax = 1.0f;
        speedMin = 5.0f; speedMax = 15.0f;
        direction = { 0, 0, 0 }; spread = 180.0f;
        gravity = { 0, -5.0f, 0 };
        colorStart = { 1, 0.7f, 0.2f, 1 };
        colorEnd   = { 0.3f, 0.1f, 0.0f, 0 };
        sizeStart = 0.2f; sizeEnd = 0.05f;
        looping = false;
        shape = EmitterShape::Sphere; shapeRadius = 0.2f;
        break;
    default: break;
    }
}

void ParticleEmitter::OnStart() {
    m_Particles.resize(maxParticles);
    m_AliveCount = 0;
    m_EmitAccum = 0;
}

void ParticleEmitter::OnUpdate(f32 dt) {
    if (!m_Playing) return;

    // Ensure pool is sized
    if (m_Particles.size() != maxParticles) {
        m_Particles.resize(maxParticles);
    }

    // Emission
    if (looping || m_EmitAccum < static_cast<f32>(maxParticles)) {
        m_EmitAccum += emissionRate * dt;
        while (m_EmitAccum >= 1.0f) {
            EmitParticle();
            m_EmitAccum -= 1.0f;
        }
    }

    // Simulate
    m_AliveCount = 0;
    for (auto& p : m_Particles) {
        if (!p.alive) continue;
        SimulateParticle(p, dt);
        if (p.alive) m_AliveCount++;
    }
}

void ParticleEmitter::EmitParticle() {
    for (auto& p : m_Particles) {
        if (p.alive) continue;

        p.alive = true;
        p.age = 0;
        p.lifetime = Randf(lifetimeMin, lifetimeMax);
        p.size = sizeStart;
        p.sizeEnd = sizeEnd;
        p.color = colorStart;
        p.colorEnd = colorEnd;

        // Position based on shape
        Vec3 emitPos = { 0, 0, 0 };
        if (Owner) {
            emitPos = Owner->GetTransform()->position;
        }

        switch (shape) {
        case EmitterShape::Point:
            p.position = emitPos;
            break;
        case EmitterShape::Sphere: {
            f32 theta = Randf(0, 6.283185f);
            f32 phi = Randf(-1.57f, 1.57f);
            f32 r = Randf(0, shapeRadius);
            p.position = {
                emitPos.x + r * std::cos(phi) * std::cos(theta),
                emitPos.y + r * std::sin(phi),
                emitPos.z + r * std::cos(phi) * std::sin(theta)
            };
            break;
        }
        case EmitterShape::Box:
            p.position = {
                emitPos.x + Randf(-shapeRadius, shapeRadius),
                emitPos.y + Randf(-shapeRadius * 0.1f, shapeRadius * 0.1f),
                emitPos.z + Randf(-shapeRadius, shapeRadius)
            };
            break;
        case EmitterShape::Cone:
        default:
            p.position = emitPos;
            break;
        }

        // Velocity: direction + spread
        f32 spd = Randf(speedMin, speedMax);
        f32 spreadRad = spread * 3.14159f / 180.0f;
        f32 theta = Randf(0, 6.283185f);
        f32 cosCone = std::cos(spreadRad);
        f32 z2 = Randf(cosCone, 1.0f);
        f32 r2 = std::sqrt(1.0f - z2 * z2);

        Vec3 localDir = { r2 * std::cos(theta), z2, r2 * std::sin(theta) };
        // Rotate localDir so that (0,1,0) aligns with 'direction'
        Vec3 up = { 0, 1, 0 };
        Vec3 dir = direction.Normalized();
        if (std::abs(Vec3::Dot(up, dir)) < 0.999f) {
            Vec3 axis = Vec3::Cross(up, dir).Normalized();
            f32 angle = std::acos(Vec3::Dot(up, dir));
            Quaternion q = Quaternion::FromAxisAngle(axis, angle);
            localDir = q.RotateVec3(localDir);
        } else if (Vec3::Dot(up, dir) < 0) {
            localDir.y = -localDir.y;
        }
        p.velocity = { localDir.x * spd, localDir.y * spd, localDir.z * spd };

        return;
    }
}

void ParticleEmitter::SimulateParticle(Particle& p, f32 dt) {
    p.age += dt;
    if (p.age >= p.lifetime) {
        p.alive = false;
        return;
    }

    // Physics
    p.velocity.x += gravity.x * dt;
    p.velocity.y += gravity.y * dt;
    p.velocity.z += gravity.z * dt;
    p.position.x += p.velocity.x * dt;
    p.position.y += p.velocity.y * dt;
    p.position.z += p.velocity.z * dt;

    // Interpolate colour and size
    f32 t = p.age / p.lifetime;
    p.color = LerpV4(colorStart, colorEnd, t);
    p.size = LerpF(sizeStart, sizeEnd, t);
}

void ParticleEmitter::Reset() {
    for (auto& p : m_Particles) p.alive = false;
    m_AliveCount = 0;
    m_EmitAccum = 0;
}

// ── GPU ────────────────────────────────────────────────────────────────────
void ParticleEmitter::InitGPU() {
#ifdef GV_HAS_GLFW
    if (m_GPUReady) return;

    // Quad: two triangles (billboard, centred at origin, size 1)
    f32 quad[] = {
        -0.5f, -0.5f, 0,  0, 0,
         0.5f, -0.5f, 0,  1, 0,
         0.5f,  0.5f, 0,  1, 1,
        -0.5f, -0.5f, 0,  0, 0,
         0.5f,  0.5f, 0,  1, 1,
        -0.5f,  0.5f, 0,  0, 1,
    };

    glGenVertexArrays(1, &m_VAO);
    glGenBuffers(1, &m_QuadVBO);
    glGenBuffers(1, &m_InstanceVBO);

    glBindVertexArray(m_VAO);

    // Quad geometry
    glBindBuffer(GL_ARRAY_BUFFER, m_QuadVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quad), quad, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(f32), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(f32), (void*)(3 * sizeof(f32)));

    // Instance data: posXYZ, size, RGBA (8 floats per instance)
    glBindBuffer(GL_ARRAY_BUFFER, m_InstanceVBO);
    glBufferData(GL_ARRAY_BUFFER, static_cast<long>(maxParticles * 8 * sizeof(f32)), nullptr, GL_DYNAMIC_DRAW);

    // location 2: position + size (vec4)
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, 8 * sizeof(f32), (void*)0);
    glVertexAttribDivisor(2, 1);
    // location 3: color RGBA (vec4)
    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, 8 * sizeof(f32), (void*)(4 * sizeof(f32)));
    glVertexAttribDivisor(3, 1);

    glBindVertexArray(0);
    m_GPUReady = true;
#endif
}

void ParticleEmitter::UploadInstances() {
#ifdef GV_HAS_GLFW
    if (!m_GPUReady) InitGPU();

    m_InstanceData.clear();
    m_InstanceData.reserve(m_AliveCount * 8);
    for (auto& p : m_Particles) {
        if (!p.alive) continue;
        m_InstanceData.push_back(p.position.x);
        m_InstanceData.push_back(p.position.y);
        m_InstanceData.push_back(p.position.z);
        m_InstanceData.push_back(p.size);
        m_InstanceData.push_back(p.color.x);
        m_InstanceData.push_back(p.color.y);
        m_InstanceData.push_back(p.color.z);
        m_InstanceData.push_back(p.color.w);
    }

    glBindBuffer(GL_ARRAY_BUFFER, m_InstanceVBO);
    glBufferSubData(GL_ARRAY_BUFFER, 0,
                    static_cast<long>(m_InstanceData.size() * sizeof(f32)),
                    m_InstanceData.data());
#endif
}

void ParticleEmitter::CleanupGPU() {
#ifdef GV_HAS_GLFW
    if (m_VAO) { glDeleteVertexArrays(1, &m_VAO); m_VAO = 0; }
    if (m_QuadVBO) { glDeleteBuffers(1, &m_QuadVBO); m_QuadVBO = 0; }
    if (m_InstanceVBO) { glDeleteBuffers(1, &m_InstanceVBO); m_InstanceVBO = 0; }
    m_GPUReady = false;
#endif
}

} // namespace gv
