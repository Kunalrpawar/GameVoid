// ============================================================================
// GameVoid Engine — Physics Implementation
// ============================================================================
#include "physics/Physics.h"
#include "core/GameObject.h"

namespace gv {

// ── PhysicsWorld ───────────────────────────────────────────────────────────

void PhysicsWorld::Init() {
    GV_LOG_INFO("PhysicsWorld initialised (gravity = " +
                std::to_string(gravity.y) + " m/s²).");
}

void PhysicsWorld::Shutdown() {
    m_Bodies.clear();
    m_Collisions.clear();
    GV_LOG_INFO("PhysicsWorld shut down.");
}

void PhysicsWorld::Step(f32 dt) {
    // Accumulate time and run fixed-step sub-steps.
    m_Accumulator += dt;
    i32 steps = 0;
    while (m_Accumulator >= fixedTimeStep && steps < maxSubSteps) {
        IntegrateBodies(fixedTimeStep);
        DetectCollisions();
        ResolveCollisions();
        m_Accumulator -= fixedTimeStep;
        ++steps;
    }
}

void PhysicsWorld::RegisterBody(RigidBody* body) {
    m_Bodies.push_back(body);
}

void PhysicsWorld::UnregisterBody(RigidBody* body) {
    m_Bodies.erase(std::remove(m_Bodies.begin(), m_Bodies.end(), body), m_Bodies.end());
}

bool PhysicsWorld::Raycast(const Vec3& origin, const Vec3& direction,
                           f32 maxDistance, CollisionInfo& outHit) const {
    // Placeholder — iterate colliders and do ray-box / ray-sphere tests.
    (void)origin; (void)direction; (void)maxDistance; (void)outHit;
    GV_LOG_DEBUG("Raycast (placeholder) — no hit.");
    return false;
}

// ── Private helpers ────────────────────────────────────────────────────────

void PhysicsWorld::IntegrateBodies(f32 dt) {
    for (auto* rb : m_Bodies) {
        if (rb->bodyType != RigidBodyType::Dynamic) continue;
        if (!rb->GetOwner()) continue;

        Transform& t = rb->GetOwner()->GetTransform();

        // Apply gravity
        if (rb->useGravity) {
            rb->velocity = rb->velocity + gravity * dt;
        }

        // Apply accumulated forces  (F = ma → a = F/m)
        if (rb->mass > 0.0f) {
            Vec3 accel = rb->force * (1.0f / rb->mass);
            rb->velocity = rb->velocity + accel * dt;
        }

        // Linear drag
        rb->velocity = rb->velocity * (1.0f / (1.0f + rb->drag * dt));

        // Integrate position
        t.position = t.position + rb->velocity * dt;

        // Clear forces for next frame
        rb->force  = Vec3::Zero();
        rb->torque = Vec3::Zero();
    }
}

void PhysicsWorld::DetectCollisions() {
    m_Collisions.clear();
    // Placeholder — broad-phase (AABB) then narrow-phase per collider pair.
    // For each overlapping pair, push a CollisionInfo into m_Collisions.
}

void PhysicsWorld::ResolveCollisions() {
    for (auto& col : m_Collisions) {
        // Placeholder — apply impulse-based resolution using contact normal,
        // restitution, and friction of the two bodies.
        (void)col;
    }
}

} // namespace gv
