// ============================================================================
// GameVoid Engine — Physics Behavior Analyzer
// ============================================================================
// Analyzes and tracks object behavior: velocity, acceleration, forces,
// and computes metrics like kinetic energy, momentum, and impact force.
//
// Used to understand physics interaction and debug object behavior.
// ============================================================================
#pragma once

#include "core/Math.h"
#include "core/Component.h"
#include <string>
#include <vector>

namespace gv {

// ── Physics behavior state ─────────────────────────────────────────────────
enum class PhysicsBehaviorState {
    Idle,           // No movement
    Moving,         // Linear motion
    Rotating,       // Angular motion only
    Falling,        // Falling under gravity
    Colliding,      // In collision
    Constrained,    // Constrained motion
    Sleeping        // Physics asleep (optimization)
};

// ── Individual behavior metric ─────────────────────────────────────────────
struct BehaviorMetric {
    std::string name;      // "velocity", "momentum", "energy", etc.
    f32 value = 0.0f;
    f32 maxValue = 0.0f;   // Peak recorded value
    f32 minValue = 9999.0f; // Minimum recorded value
    bool isActive = false;
};

// ============================================================================
// BehaviorAnalyzer — Tracks and analyzes physics object behavior
// ============================================================================
class BehaviorAnalyzer : public Component {
public:
    std::string GetTypeName() const override { return "BehaviorAnalyzer"; }

    // ── Lifecycle ──────────────────────────────────────────────────────────
    void Update(f32 dt);
    void OnDestroy();

    // ── State query ────────────────────────────────────────────────────────
    PhysicsBehaviorState GetBehaviorState() const { return m_BehaviorState; }
    const std::string& GetBehaviorStateName() const;

    // ── Metrics tracking ───────────────────────────────────────────────────
    f32 GetVelocityMagnitude() const { return m_VelocityMagnitude; }
    f32 GetAccelerationMagnitude() const { return m_AccelerationMagnitude; }
    f32 GetKineticEnergy() const { return m_KineticEnergy; }
    f32 GetMomentumMagnitude() const { return m_MomentumMagnitude; }
    f32 GetAngularVelocityMagnitude() const { return m_AngularVelocityMagnitude; }
    f32 GetRotationalKineticEnergy() const { return m_RotationalKineticEnergy; }
    f32 GetImpactForce() const { return m_ImpactForce; }
    f32 GetNetForceMagnitude() const { return m_NetForceMagnitude; }

    // ── History tracking ──────────────────────────────────────────────────
    const std::vector<f32>& GetVelocityHistory() const { return m_VelocityHistory; }
    const std::vector<f32>& GetEnergyHistory() const { return m_EnergyHistory; }

    // ── Behavior flags ─────────────────────────────────────────────────────
    bool isMoving = false;
    bool isRotating = false;
    bool isFalling = false;
    bool isColliding = false;
    bool isUpside = false;
    bool isOnGround = false;

    // ── Configuration ──────────────────────────────────────────────────────
    i32 historyBufferSize = 120;  // frames to keep in history
    f32 velocityThreshold = 0.01f; // minimum velocity to consider "moving"
    f32 angularThreshold = 0.01f;  // minimum angular velocity to consider "rotating"

    // ── Debugging ──────────────────────────────────────────────────────────
    bool enableDebugLog = false;
    void PrintBehaviorReport() const;
    std::string GetBehaviorSummary() const;

private:
    PhysicsBehaviorState m_BehaviorState = PhysicsBehaviorState::Idle;
    
    // Current metrics
    f32 m_VelocityMagnitude = 0.0f;
    f32 m_AccelerationMagnitude = 0.0f;
    f32 m_KineticEnergy = 0.0f;
    f32 m_MomentumMagnitude = 0.0f;
    f32 m_AngularVelocityMagnitude = 0.0f;
    f32 m_RotationalKineticEnergy = 0.0f;
    f32 m_ImpactForce = 0.0f;
    f32 m_NetForceMagnitude = 0.0f;

    // Previous frame velocity (for acceleration calculation)
    Vec3 m_PreviousVelocity { 0, 0, 0 };
    Vec3 m_PreviousAngularVel { 0, 0, 0 };

    // History buffers (for graphing)
    std::vector<f32> m_VelocityHistory;
    std::vector<f32> m_EnergyHistory;
    std::vector<f32> m_MomentumHistory;

    // Internal metrics map
    std::vector<BehaviorMetric> m_Metrics;

    void UpdateMetrics(f32 dt);
    void UpdateBehaviorState();
    void RecordToHistory();
    void ComputeAcceleration();
};

} // namespace gv
