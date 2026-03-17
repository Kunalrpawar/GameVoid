// ============================================================================
// GameVoid Engine — Behavior Analyzer Implementation
// ============================================================================

#include "scripting/physics/BehaviorAnalyzer.h"
#include "core/GameObject.h"
#include "physics/Physics.h"
#include "editor2d/Editor2DTypes.h"
#include <cmath>
#include <iostream>

namespace gv {

const std::string& BehaviorAnalyzer::GetBehaviorStateName() const {
    static std::string names[] = {
        "Idle", "Moving", "Rotating", "Falling", "Colliding", "Constrained", "Sleeping"
    };
    return names[static_cast<int>(m_BehaviorState)];
}

void BehaviorAnalyzer::Update(f32 dt) {
    auto* obj = GetGameObject();
    if (!obj) return;

    UpdateMetrics(dt);
    UpdateBehaviorState();
    RecordToHistory();

    if (enableDebugLog) {
        PrintBehaviorReport();
    }
}

void BehaviorAnalyzer::UpdateMetrics(f32 dt) {
    auto* obj = GetGameObject();
    if (!obj) return;

    // Get velocity
    Vec3 velocity(0, 0, 0);
    Vec3 angularVel(0, 0, 0);
    f32 mass = 1.0f;

    auto* rb = obj->GetComponent<RigidBody>();
    if (rb) {
        velocity = rb->velocity;
        angularVel = rb->angularVelocity;
        mass = rb->mass;
    } else {
        auto* rb2d = obj->GetComponent<RigidBody2D>();
        if (rb2d) {
            velocity = Vec3(rb2d->velocity.x, rb2d->velocity.y, 0);
            angularVel = Vec3(0, 0, rb2d->angularVel);
            mass = rb2d->mass;
        }
    }

    // Compute magnitudes
    m_VelocityMagnitude = velocity.Length();
    m_AngularVelocityMagnitude = angularVel.Length();

    // Acceleration (finite difference)
    ComputeAcceleration();

    // Kinetic energy (0.5 * m * v^2)
    m_KineticEnergy = 0.5f * mass * (m_VelocityMagnitude * m_VelocityMagnitude);

    // Momentum (m * v)
    m_MomentumMagnitude = mass * m_VelocityMagnitude;

    // Rotational kinetic energy (approximation: 0.5 * I * w^2, assume I = mass)
    m_RotationalKineticEnergy = 0.5f * mass * (m_AngularVelocityMagnitude * m_AngularVelocityMagnitude);

    // Track behavior
    isMoving = m_VelocityMagnitude > velocityThreshold;
    isRotating = m_AngularVelocityMagnitude > angularThreshold;
    isFalling = velocity.y < -0.1f;  // Rough falling detection
}

void BehaviorAnalyzer::UpdateBehaviorState() {
    // State machine: determine current behavior
    if (isMoving && isRotating) {
        m_BehaviorState = PhysicsBehaviorState::Rotating;
    } else if (isFalling) {
        m_BehaviorState = PhysicsBehaviorState::Falling;
    } else if (isMoving) {
        m_BehaviorState = PhysicsBehaviorState::Moving;
    } else if (isRotating) {
        m_BehaviorState = PhysicsBehaviorState::Rotating;
    } else {
        m_BehaviorState = PhysicsBehaviorState::Idle;
    }
}

void BehaviorAnalyzer::RecordToHistory() {
    m_VelocityHistory.push_back(m_VelocityMagnitude);
    m_EnergyHistory.push_back(m_KineticEnergy);
    m_MomentumHistory.push_back(m_MomentumMagnitude);

    // Trim history to buffer size
    while (m_VelocityHistory.size() > historyBufferSize) {
        m_VelocityHistory.erase(m_VelocityHistory.begin());
        m_EnergyHistory.erase(m_EnergyHistory.begin());
        m_MomentumHistory.erase(m_MomentumHistory.begin());
    }
}

void BehaviorAnalyzer::ComputeAcceleration() {
    auto* obj = GetGameObject();
    if (!obj) return;

    Vec3 currentVel = Vec3(0, 0, 0);
    auto* rb = obj->GetComponent<RigidBody>();
    if (rb) currentVel = rb->velocity;
    else {
        auto* rb2d = obj->GetComponent<RigidBody2D>();
        if (rb2d) currentVel = Vec3(rb2d->velocity.x, rb2d->velocity.y, 0);
    }

    Vec3 accel = (currentVel - m_PreviousVelocity);  // Simplified: no dt
    m_AccelerationMagnitude = accel.Length();
    m_PreviousVelocity = currentVel;
}

void BehaviorAnalyzer::OnDestroy() {
    m_VelocityHistory.clear();
    m_EnergyHistory.clear();
    m_MomentumHistory.clear();
}

void BehaviorAnalyzer::PrintBehaviorReport() const {
    std::cout << "[Behavior] " << GetGameObject()->GetName() << " - "
              << GetBehaviorStateName() << " | "
              << "Vel: " << m_VelocityMagnitude << " m/s | "
              << "Acc: " << m_AccelerationMagnitude << " m/s² | "
              << "E: " << m_KineticEnergy << " J | "
              << "AngVel: " << m_AngularVelocityMagnitude << " rad/s\n";
}

std::string BehaviorAnalyzer::GetBehaviorSummary() const {
    std::string summary = GetGameObject()->GetName() + " [" + GetBehaviorStateName() + "]\n";
    summary += "Velocity: " + std::to_string(m_VelocityMagnitude) + " m/s\n";
    summary += "Kinetic Energy: " + std::to_string(m_KineticEnergy) + " J\n";
    summary += "Angular Velocity: " + std::to_string(m_AngularVelocityMagnitude) + " rad/s\n";
    return summary;
}

} // namespace gv
