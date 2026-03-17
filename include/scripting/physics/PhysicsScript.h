// ============================================================================
// GameVoid Engine — Physics Script Base Class
// ============================================================================
// Attach to GameObject to control 3D/2D physics behavior through C++ scripts.
// Handles gravity, forces, moments, and impulses via keyboard input.
//
// Derived classes implement OnPhysicsUpdate() to define custom behavior.
// ============================================================================
#pragma once

#include "core/Component.h"
#include "core/Math.h"
#include "physics/Physics.h"
#include "editor2d/Editor2DTypes.h"
#include <string>

namespace gv {

// ── Physics constraint types ──────────────────────────────────────────────
enum class PhysicsConstraint {
    None,
    FixedDistance,
    Hinge,
    Slider,
    BallSocket
};

// ── Physics script input bindings ──────────────────────────────────────────
struct PhysicsInputBinding {
    std::string keyName;      // "W", "A", "S", "D", "Space", "Q", "E", etc.
    std::string forceAxis;    // "forceX", "forceY", "forceZ"
    f32 magnitude = 1.0f;     // Force magnitude multiplier
    bool isImpulse = false;   // true = apply impulse, false = apply force
};

// ============================================================================
// PhysicsScript — Base class for physics-driven game object behavior
// ============================================================================
class PhysicsScript : public Component {
public:
    std::string GetTypeName() const override { return "PhysicsScript"; }

    // ── Lifecycle ──────────────────────────────────────────────────────────
    virtual void OnCreate() {}
    virtual void OnDestroy() {}
    virtual void OnPhysicsUpdate(f32 dt) = 0;

    // ── Gravity control ────────────────────────────────────────────────────
    bool  disableGravity = false;
    Vec3  gravityScale { 1.0f, 1.0f, 1.0f };
    f32   customGravity = -9.81f;  // Only used if gravityOverride = true
    bool  gravityOverride = false;

    // ── Force application (updated each physics frame) ─────────────────────
    Vec3  forceThisFrame { 0, 0, 0 };      // Forces to apply (impulse/continuous)
    Vec3  momentThisFrame { 0, 0, 0 };     // Torque/moments to apply
    bool  applyAsImpulse = false;          // true = impulse, false = continuous force

    // ── Constraint setup ────────────────────────────────────────────────────
    PhysicsConstraint constraint = PhysicsConstraint::None;
    GameObject* constraintTarget = nullptr;
    f32 constraintDistance = 1.0f;

    // ── Input bindings (for keyboard-driven forces) ────────────────────────
    std::vector<PhysicsInputBinding> inputBindings;

    /// Register a keyboard key to apply a force/impulse
    void BindKeyToForce(const std::string& keyName, const std::string& forceAxis, 
                        f32 magnitude, bool isImpulse = false) {
        PhysicsInputBinding binding;
        binding.keyName = keyName;
        binding.forceAxis = forceAxis;
        binding.magnitude = magnitude;
        binding.isImpulse = isImpulse;
        inputBindings.push_back(binding);
    }

    /// Apply a force to the object (continuous, not instantaneous)
    void ApplyForce(const Vec3& force, bool asImpulse = false) {
        forceThisFrame += force;
        applyAsImpulse = asImpulse;
    }

    /// Apply a moment/torque (rotation force)
    void ApplyMoment(const Vec3& moment) {
        momentThisFrame += moment;
    }

    /// Check if a specific key is pressed/held
    virtual bool IsKeyPressed(const std::string& keyName) const;

    /// Get current velocity of the game object (2D or 3D)
    Vec3 GetVelocity() const;

    /// Get current angular velocity (rotation speed)
    Vec3 GetAngularVelocity() const;

    /// Apply forces to rigid body (called from physics system)
    void ApplyForcesToRigidBody();
};

} // namespace gv
