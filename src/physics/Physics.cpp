// ============================================================================
// GameVoid Engine -- Physics Implementation
// ============================================================================
#include "physics/Physics.h"
#include "core/GameObject.h"
#include <algorithm>
#include <cmath>

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
    // Broad phase: check every pair of registered bodies that have Colliders
    // on their owning GameObjects.  N^2 for the skeleton; swap with spatial
    // partitioning (octree / grid) in production.
    for (size_t i = 0; i < m_Bodies.size(); ++i) {
        for (size_t j = i + 1; j < m_Bodies.size(); ++j) {
            RigidBody* a = m_Bodies[i];
            RigidBody* b = m_Bodies[j];
            if (!a->GetOwner() || !b->GetOwner()) continue;

            Collider* colA = a->GetOwner()->GetComponent<Collider>();
            Collider* colB = b->GetOwner()->GetComponent<Collider>();
            if (!colA || !colB) continue;

            Vec3 posA = a->GetOwner()->GetTransform().position;
            Vec3 posB = b->GetOwner()->GetTransform().position;

            bool hit = false;

            // Sphere-Sphere
            if (colA->type == ColliderType::Sphere && colB->type == ColliderType::Sphere) {
                hit = TestSphereSphere(posA, colA->radius, posB, colB->radius);
            }
            // Box-Box (AABB)
            else if (colA->type == ColliderType::Box && colB->type == ColliderType::Box) {
                Vec3 minA = posA - colA->boxHalfExtents;
                Vec3 maxA = posA + colA->boxHalfExtents;
                Vec3 minB = posB - colB->boxHalfExtents;
                Vec3 maxB = posB + colB->boxHalfExtents;
                hit = TestAABB(minA, maxA, minB, maxB);
            }
            // Sphere-Box
            else if (colA->type == ColliderType::Sphere && colB->type == ColliderType::Box) {
                Vec3 minB = posB - colB->boxHalfExtents;
                Vec3 maxB = posB + colB->boxHalfExtents;
                hit = TestSphereAABB(posA, colA->radius, minB, maxB);
            }
            else if (colA->type == ColliderType::Box && colB->type == ColliderType::Sphere) {
                Vec3 minA = posA - colA->boxHalfExtents;
                Vec3 maxA = posA + colA->boxHalfExtents;
                hit = TestSphereAABB(posB, colB->radius, minA, maxA);
            }

            if (hit) {
                CollisionInfo info;
                info.objectA = a->GetOwner();
                info.objectB = b->GetOwner();
                info.contactNormal = (posB - posA).Normalized();
                info.contactPoint  = posA + info.contactNormal * colA->radius;
                m_Collisions.push_back(info);
            }
        }
    }
}

void PhysicsWorld::ResolveCollisions() {
    for (auto& col : m_Collisions) {
        // Impulse-based collision response:
        // 1. Compute relative velocity along the contact normal
        // 2. Compute impulse scalar using restitution (bounciness)
        // 3. Apply equal and opposite impulses to each body
        // 4. Apply positional correction to fix penetration
        // Placeholder -- details omitted in skeleton.
        (void)col;
    }
}

// == Static collision geometry tests =========================================

bool PhysicsWorld::TestAABB(const Vec3& minA, const Vec3& maxA,
                            const Vec3& minB, const Vec3& maxB) {
    return (minA.x <= maxB.x && maxA.x >= minB.x) &&
           (minA.y <= maxB.y && maxA.y >= minB.y) &&
           (minA.z <= maxB.z && maxA.z >= minB.z);
}

bool PhysicsWorld::TestSphereSphere(const Vec3& posA, f32 radiusA,
                                    const Vec3& posB, f32 radiusB) {
    Vec3 diff = posB - posA;
    f32 distSq = diff.Dot(diff);
    f32 radSum = radiusA + radiusB;
    return distSq <= (radSum * radSum);
}

bool PhysicsWorld::TestSphereAABB(const Vec3& spherePos, f32 radius,
                                  const Vec3& boxMin, const Vec3& boxMax) {
    // Find the closest point on the AABB to the sphere centre
    auto clamp = [](f32 v, f32 lo, f32 hi) -> f32 {
        return (v < lo) ? lo : (v > hi) ? hi : v;
    };
    Vec3 closest{
        clamp(spherePos.x, boxMin.x, boxMax.x),
        clamp(spherePos.y, boxMin.y, boxMax.y),
        clamp(spherePos.z, boxMin.z, boxMax.z)
    };
    Vec3 diff = spherePos - closest;
    return diff.Dot(diff) <= (radius * radius);
}

void PhysicsWorld::AddPhysicsComponents(GameObject* obj, RigidBodyType bodyType,
                                        ColliderType colliderType, f32 mass) {
    if (!obj) return;
    auto* rb = obj->AddComponent<RigidBody>();
    rb->bodyType = bodyType;
    rb->mass     = mass;
    rb->useGravity = (bodyType == RigidBodyType::Dynamic);

    auto* col = obj->AddComponent<Collider>();
    col->type = colliderType;
    GV_LOG_INFO("Physics components added to '" + obj->GetName() + "'");
}

} // namespace gv
