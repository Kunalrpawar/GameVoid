// ============================================================================
// GameVoid Engine — Force & Moment Controller
// ============================================================================
// High-level interface for controlling forces, moments (torque), and gravity
// on 2D/3D physics objects through keyboard input.
//
// Supports:
//   - Directional forces (up, down, left, right, forward, backward)
//   - Moment/rotation control
//   - Gravity override
//   - Velocity damping
//   - Impact response
// ============================================================================
#pragma once

#include "core/Component.h"
#include "core/Math.h"
#include <string>

namespace gv {

// ── Force direction types ──────────────────────────────────────────────────
enum class ForceDirection {
    Forward,   // +Z / +X (2D)
    Backward,  // -Z / -X (2D)
    Left,      // -X / -X (2D)
    Right,     // +X / +X (2D)
    Up,        // +Y / +Y (both)
    Down,      // -Y / -Y (both)
    Custom     // User-defined direction
};

// ── Moment/rotation direction ──────────────────────────────────────────────
enum class MomentDirection {
    Pitch,     // Rotation around X (tilt forward/back)
    Yaw,       // Rotation around Y (spin left/right)
    Roll,      // Rotation around Z (barrel roll)
    Custom     // User-defined axis
};

// ============================================================================
// ForceController — Keyboard-driven force and moment application
// ============================================================================
class ForceController : public Component {
public:
    std::string GetTypeName() const override { return "ForceController"; }

    // ── Lifecycle ──────────────────────────────────────────────────────────
    void OnCreate();
    void OnDestroy();
    void Update(f32 dt);

    // ── Force application ──────────────────────────────────────────────────
    /// Apply a force in a specific direction
    void ApplyForceInDirection(ForceDirection dir, f32 magnitude);

    /// Apply a moment (rotational force) in a specific direction
    void ApplyMomentInDirection(MomentDirection dir, f32 magnitude);

    /// Apply custom force vector
    void ApplyForceVector(const Vec3& force);

    /// Apply custom moment vector
    void ApplyMomentVector(const Vec3& moment);

    // ── Gravity control ────────────────────────────────────────────────────
    /// Override world gravity for this object
    void SetGravityScale(f32 scale) { m_GravityScale = scale; }
    f32 GetGravityScale() const { return m_GravityScale; }

    /// Disable gravity entirely
    void DisableGravity(bool disable) { m_DisableGravity = disable; }
    bool IsGravityDisabled() const { return m_DisableGravity; }

    /// Set custom gravity (overrides world gravity)
    void SetCustomGravity(f32 gravity) { m_CustomGravity = gravity; m_UseCustomGravity = true; }
    void UseWorldGravity() { m_UseCustomGravity = false; }

    // ── Damping / Drag ─────────────────────────────────────────────────────
    /// Linear velocity damping (0 = no damping, 1 = instant stop)
    void SetLinearDamping(f32 damping) { m_LinearDamping = damping; }
    f32 GetLinearDamping() const { return m_LinearDamping; }

    /// Angular velocity damping (0 = no damping, 1 = instant stop)
    void SetAngularDamping(f32 damping) { m_AngularDamping = damping; }
    f32 GetAngularDamping() const { return m_AngularDamping; }

    // ── Input binding (keyboard to forces) ─────────────────────────────────
    /// Bind a key to apply a force in a direction
    /// e.g., BindKeyToForce("W", ForceDirection::Up, 100.0f)
    void BindKeyToForce(const std::string& keyName, ForceDirection dir, f32 magnitude);

    /// Bind a key to apply a moment in a direction
    /// e.g., BindKeyToMoment("Q", MomentDirection::Roll, 50.0f)
    void BindKeyToMoment(const std::string& keyName, MomentDirection dir, f32 magnitude);

    // ── Force limits ───────────────────────────────────────────────────────
    /// Maximum velocity (clamped)
    void SetMaxVelocity(f32 maxVel) { m_MaxVelocity = maxVel; }
    f32 GetMaxVelocity() const { return m_MaxVelocity; }

    /// Maximum angular velocity (clamped)
    void SetMaxAngularVelocity(f32 maxAngVel) { m_MaxAngularVelocity = maxAngVel; }
    f32 GetMaxAngularVelocity() const { return m_MaxAngularVelocity; }

    // ── Configuration ──────────────────────────────────────────────────────
    bool immediateMode = false;        // Apply forces instantly instead of over time
    bool applyToLocalSpace = true;     // Apply forces in local or world space
    bool constrainToPlane = false;     // Keep object on a 2D plane
    std::string constrainedPlane = "XY"; // "XY", "XZ", "YZ"

    // ── State query ────────────────────────────────────────────────────────
    Vec3 GetCurrentForce() const { return m_CurrentForce; }
    Vec3 GetCurrentMoment() const { return m_CurrentMoment; }
    f32 GetCurrentSpeed() const;
    f32 GetCurrentAngularSpeed() const;

private:
    // Force/moment accumulation
    Vec3 m_CurrentForce { 0, 0, 0 };
    Vec3 m_CurrentMoment { 0, 0, 0 };

    // Gravity control
    f32 m_GravityScale = 1.0f;
    bool m_DisableGravity = false;
    f32 m_CustomGravity = -9.81f;
    bool m_UseCustomGravity = false;

    // Damping
    f32 m_LinearDamping = 0.1f;
    f32 m_AngularDamping = 0.1f;

    // Limits
    f32 m_MaxVelocity = 100.0f;
    f32 m_MaxAngularVelocity = 360.0f;

    // Input tracking
    std::vector<std::pair<std::string, std::pair<ForceDirection, f32>>> m_KeyForceBindings;
    std::vector<std::pair<std::string, std::pair<MomentDirection, f32>>> m_KeyMomentBindings;

    // Internal helpers
    Vec3 GetForceDirectionVector(ForceDirection dir) const;
    Vec3 GetMomentDirectionVector(MomentDirection dir) const;
    void ProcessInputBindings();
    void ApplyDamping(f32 dt);
    void ClampVelocities();
    void ApplyGravityOverride();
};

} // namespace gv
