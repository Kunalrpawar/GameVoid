#include "constraints/Constraints.h"
#include "physics/Physics.h"
#include "core/GameObject.h"
#include <cmath>

// ─── HingeConstraint ─────────────────────────────────────────────────────────

void HingeConstraint::Solve(float /*dt*/) {
    // Minimal positional correction (full impulse-based solver is a future pass)
    if (!bodyA || !bodyB) return;
    auto* rbA = bodyA->GetComponent<RigidBody>();
    auto* rbB = bodyB->GetComponent<RigidBody>();
    if (!rbA || !rbB) return;
    // Cancel out-of-axis angular velocity
    if (axisWorld.x != 0 || axisWorld.y != 0 || axisWorld.z != 0) {
        float dot = rbA->angularVelocity.x*axisWorld.x + rbA->angularVelocity.y*axisWorld.y + rbA->angularVelocity.z*axisWorld.z;
        Vec3 onAxis{axisWorld.x*dot, axisWorld.y*dot, axisWorld.z*dot};
        rbA->angularVelocity = onAxis;
        dot = rbB->angularVelocity.x*axisWorld.x + rbB->angularVelocity.y*axisWorld.y + rbB->angularVelocity.z*axisWorld.z;
        onAxis = {axisWorld.x*dot, axisWorld.y*dot, axisWorld.z*dot};
        rbB->angularVelocity = onAxis;
    }
    // Motor
    if (useMotor && rbB) {
        float targetAV = motorTargetRPM * 2.0f * 3.14159265f / 60.0f;
        float dot = rbB->angularVelocity.x*axisWorld.x + rbB->angularVelocity.y*axisWorld.y + rbB->angularVelocity.z*axisWorld.z;
        float diff = targetAV - dot;
        float torqueMag = std::min(std::abs(diff)*100.0f, motorMaxTorque) * (diff >= 0 ? 1.f : -1.f);
        rbB->angularVelocity.x += axisWorld.x * torqueMag * 0.016f;
        rbB->angularVelocity.y += axisWorld.y * torqueMag * 0.016f;
        rbB->angularVelocity.z += axisWorld.z * torqueMag * 0.016f;
    }
}

float HingeConstraint::CurrentAngleDeg() const { return 0.0f; }

// ─── SpringConstraint ─────────────────────────────────────────────────────────

void SpringConstraint::Solve(float dt) {
    if (!bodyA || !bodyB) return;
    auto* rbA = bodyA->GetComponent<RigidBody>();
    auto* rbB = bodyB->GetComponent<RigidBody>();

    Vec3 posA = bodyA->GetTransform().position;
    Vec3 posB = bodyB->GetTransform().position;
    Vec3 delta{posB.x-posA.x, posB.y-posA.y, posB.z-posA.z};
    float dist = std::sqrt(delta.x*delta.x + delta.y*delta.y + delta.z*delta.z);
    if (dist < 1e-6f) return;

    float extension = dist - restLength;
    extension = std::max(extension, -(restLength - minLength));
    extension = std::min(extension, maxLength - restLength);

    float springForce = stiffness * extension;
    float dampForce = 0.0f;
    if (rbA && rbB) {
        Vec3 relVel{rbB->velocity.x-rbA->velocity.x, rbB->velocity.y-rbA->velocity.y, rbB->velocity.z-rbA->velocity.z};
        dampForce = damping * (relVel.x*delta.x + relVel.y*delta.y + relVel.z*delta.z) / dist;
    }
    float totalF = springForce + dampForce;
    Vec3 dir{delta.x/dist, delta.y/dist, delta.z/dist};

    if (rbA) { rbA->velocity.x += dir.x*totalF*dt; rbA->velocity.y += dir.y*totalF*dt; rbA->velocity.z += dir.z*totalF*dt; }
    if (rbB) { rbB->velocity.x -= dir.x*totalF*dt; rbB->velocity.y -= dir.y*totalF*dt; rbB->velocity.z -= dir.z*totalF*dt; }
}

float SpringConstraint::CurrentLength() const {
    if (!bodyA || !bodyB) return 0;
    Vec3 d{bodyB->GetTransform().position.x-bodyA->GetTransform().position.x,
           bodyB->GetTransform().position.y-bodyA->GetTransform().position.y,
           bodyB->GetTransform().position.z-bodyA->GetTransform().position.z};
    return std::sqrt(d.x*d.x+d.y*d.y+d.z*d.z);
}

float SpringConstraint::CurrentExtension() const { return CurrentLength() - restLength; }

// ─── SliderConstraint ─────────────────────────────────────────────────────────

void SliderConstraint::Solve(float /*dt*/) {
    if (!bodyA || !bodyB) return;
    auto* rbB = bodyB->GetComponent<RigidBody>();
    if (!rbB) return;
    // Project velocity onto slide axis, cancel perpendicular component
    Vec3 a = slideAxisWorld;
    float dot = rbB->velocity.x*a.x + rbB->velocity.y*a.y + rbB->velocity.z*a.z;
    Vec3 projected{a.x*dot, a.y*dot, a.z*dot};
    // Motor
    if (useMotor) {
        float diff = motorTargetMPS - dot;
        float force = std::min(std::abs(diff)*100.0f, motorMaxForce) * (diff>=0 ? 1.f:-1.f);
        rbB->velocity.x += a.x*force*0.016f;
        rbB->velocity.y += a.y*force*0.016f;
        rbB->velocity.z += a.z*force*0.016f;
    } else {
        rbB->velocity = projected;  // constrain to axis
    }
}

float SliderConstraint::CurrentOffset() const {
    if (!bodyA || !bodyB) return 0;
    Vec3 d{bodyB->GetTransform().position.x-bodyA->GetTransform().position.x,
           bodyB->GetTransform().position.y-bodyA->GetTransform().position.y,
           bodyB->GetTransform().position.z-bodyA->GetTransform().position.z};
    return d.x*slideAxisWorld.x + d.y*slideAxisWorld.y + d.z*slideAxisWorld.z;
}

// ─── FixedConstraint ─────────────────────────────────────────────────────────

void FixedConstraint::Solve(float /*dt*/) {
    if (isBroken || !bodyA || !bodyB) return;
    auto* rbB = bodyB->GetComponent<RigidBody>();
    if (!rbB) return;
    Vec3 d{bodyB->GetTransform().position.x - bodyA->GetTransform().position.x,
           bodyB->GetTransform().position.y - bodyA->GetTransform().position.y,
           bodyB->GetTransform().position.z - bodyA->GetTransform().position.z};
    float dist = std::sqrt(d.x*d.x+d.y*d.y+d.z*d.z);
    float force = dist * 5000.0f;  // stiff spring approximation
    if (force > breakForce) { isBroken = true; return; }
    rbB->velocity.x -= d.x * force * 0.016f;
    rbB->velocity.y -= d.y * force * 0.016f;
    rbB->velocity.z -= d.z * force * 0.016f;
}

// ─── BallSocketConstraint ─────────────────────────────────────────────────────

void BallSocketConstraint::Solve(float /*dt*/) {
    if (!bodyA || !bodyB) return;
    // TODO: full impulse-based ball-socket implementation
}
