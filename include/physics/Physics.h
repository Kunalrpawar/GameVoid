// ============================================================================
// GameVoid Engine — Physics Module
// ============================================================================
// Provides a basic rigid-body physics simulation with collision detection.
// Designed as a Component-based system so physics can be attached to any
// GameObject.  In production you'd swap the internals for Bullet, Box2D,
// or PhysX; this skeleton implements the API surface.
// ============================================================================
#pragma once

#include "core/Component.h"
#include "core/Math.h"
#include "core/Types.h"
#include <vector>
#include <string>

namespace gv {

// ─── Collider shapes ───────────────────────────────────────────────────────
enum class ColliderType { Box, Sphere, Capsule, Mesh };

/// Collider component — defines the shape used for collision detection.
class Collider : public Component {
public:
    ColliderType type = ColliderType::Box;

    // Box extents (half-size on each axis)
    Vec3 boxHalfExtents{ 0.5f, 0.5f, 0.5f };

    // Sphere / capsule radius
    f32 radius = 0.5f;

    // Capsule height (total, including caps)
    f32 capsuleHeight = 2.0f;

    // Flags
    bool isTrigger = false;     // Trigger colliders generate events but no physics response

    std::string GetTypeName() const override { return "Collider"; }
};

// ─── Rigid Body ────────────────────────────────────────────────────────────
enum class RigidBodyType { Static, Dynamic, Kinematic };

/// RigidBody component — gives a GameObject physical behaviour.
class RigidBody : public Component {
public:
    RigidBodyType bodyType = RigidBodyType::Dynamic;

    f32  mass        = 1.0f;
    f32  drag        = 0.0f;
    f32  angularDrag = 0.05f;
    bool useGravity  = true;
    f32  restitution = 0.3f;    // bounciness (0..1)
    f32  friction    = 0.5f;    // surface friction

    // Runtime state (managed by PhysicsWorld)
    Vec3 velocity        { 0, 0, 0 };
    Vec3 angularVelocity { 0, 0, 0 };
    Vec3 force           { 0, 0, 0 };  // accumulated force this frame
    Vec3 torque          { 0, 0, 0 };  // accumulated torque this frame

    // ── API ────────────────────────────────────────────────────────────────
    void AddForce(const Vec3& f)   { force  = force  + f; }
    void AddTorque(const Vec3& t)  { torque = torque + t; }
    void AddImpulse(const Vec3& impulse) {
        if (mass > 0.0f) velocity = velocity + impulse * (1.0f / mass);
    }

    std::string GetTypeName() const override { return "RigidBody"; }

    void OnUpdate(f32 /*dt*/) override {
        // Integration is handled centrally by PhysicsWorld; this callback
        // is intentionally empty so that derived classes can add per-body
        // logic if needed.
    }
};

// ─── Collision Info ────────────────────────────────────────────────────────
/// Data produced by the collision detection pass, consumed by scripts / game logic.
struct CollisionInfo {
    GameObject* objectA = nullptr;
    GameObject* objectB = nullptr;
    Vec3 contactPoint{ 0, 0, 0 };
    Vec3 contactNormal{ 0, 1, 0 };
    f32  penetrationDepth = 0.0f;
};

// ─── Physics World ─────────────────────────────────────────────────────────
/// Central physics simulation.  Iterates over all RigidBody components in a
/// scene and performs integration + collision detection each fixed step.
class PhysicsWorld {
public:
    PhysicsWorld() = default;

    // ── Configuration ──────────────────────────────────────────────────────
    Vec3 gravity        { 0, -9.81f, 0 };
    f32  fixedTimeStep  = 1.0f / 60.0f;   // 60 Hz physics tick
    i32  maxSubSteps    = 8;

    // ── Lifecycle ──────────────────────────────────────────────────────────
    /// Initialise internal structures.
    void Init();

    /// Step the simulation forward by `dt` seconds (may run multiple sub-steps).
    void Step(f32 dt);

    /// Shutdown and release resources.
    void Shutdown();

    // ── Queries ────────────────────────────────────────────────────────────
    /// Cast a ray and return the first hit (placeholder API).
    bool Raycast(const Vec3& origin, const Vec3& direction, f32 maxDistance,
                 CollisionInfo& outHit) const;

    // ── Collision results from last step ───────────────────────────────────
    const std::vector<CollisionInfo>& GetCollisions() const { return m_Collisions; }

    // ── Registration (called by Scene when objects are added) ──────────────
    void RegisterBody(RigidBody* body);
    void UnregisterBody(RigidBody* body);

private:
    void IntegrateBodies(f32 dt);
    void DetectCollisions();
    void ResolveCollisions();

    std::vector<RigidBody*>    m_Bodies;
    std::vector<CollisionInfo> m_Collisions;
    f32                        m_Accumulator = 0.0f;
};

} // namespace gv
