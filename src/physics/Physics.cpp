// ============================================================================
// GameVoid Engine -- Physics Implementation
// ============================================================================
#include "physics/Physics.h"
#include "core/GameObject.h"
#include "core/Transform.h"
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
    // Real ray-AABB intersection against all registered bodies
    Vec3 dir = direction.Normalized();
    f32 closestT = maxDistance + 1.0f;
    bool hit = false;

    for (auto* rb : m_Bodies) {
        if (!rb->GetOwner()) continue;
        Collider* col = rb->GetOwner()->GetComponent<Collider>();
        if (!col || col->type != ColliderType::Box) continue;

        Transform& t = rb->GetOwner()->GetTransform();
        Vec3 half(col->boxHalfExtents.x * t.scale.x,
                  col->boxHalfExtents.y * t.scale.y,
                  col->boxHalfExtents.z * t.scale.z);
        Vec3 bmin = t.position - half;
        Vec3 bmax = t.position + half;

        f32 tHit = 0;
        if (RayAABBIntersect(origin, dir, bmin, bmax, tHit)) {
            if (tHit < closestT && tHit <= maxDistance) {
                closestT = tHit;
                outHit.objectA = rb->GetOwner();
                outHit.objectB = nullptr;
                outHit.contactPoint = origin + dir * tHit;
                outHit.penetrationDepth = 0;
                // Approximate contact normal
                Vec3 cp = outHit.contactPoint - t.position;
                f32 ax = std::fabs(cp.x / half.x);
                f32 ay = std::fabs(cp.y / half.y);
                f32 az = std::fabs(cp.z / half.z);
                if (ax > ay && ax > az)      outHit.contactNormal = Vec3(cp.x > 0 ? 1.0f : -1.0f, 0, 0);
                else if (ay > ax && ay > az) outHit.contactNormal = Vec3(0, cp.y > 0 ? 1.0f : -1.0f, 0);
                else                         outHit.contactNormal = Vec3(0, 0, cp.z > 0 ? 1.0f : -1.0f);
                hit = true;
            }
        }
    }

    // Also test non-physics objects in the scene (objects without RigidBody but with Collider)
    // We only test registered bodies here; for full scene picking, caller handles the rest.
    return hit;
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

        // Floor constraint (Y = 0 plane)
        Collider* col = rb->GetOwner()->GetComponent<Collider>();
        if (col && col->type == ColliderType::Box) {
            f32 halfY = col->boxHalfExtents.y * t.scale.y;
            if (t.position.y - halfY < 0.0f) {
                t.position.y = halfY;
                if (rb->velocity.y < 0.0f) {
                    rb->velocity.y = -rb->velocity.y * rb->restitution;
                    if (rb->velocity.y < 0.1f) rb->velocity.y = 0.0f;
                }
            }
        }

        // Clear forces for next frame
        rb->force  = Vec3::Zero();
        rb->torque = Vec3::Zero();
    }
}

void PhysicsWorld::DetectCollisions() {
    m_Collisions.clear();

    for (size_t i = 0; i < m_Bodies.size(); ++i) {
        for (size_t j = i + 1; j < m_Bodies.size(); ++j) {
            RigidBody* a = m_Bodies[i];
            RigidBody* b = m_Bodies[j];
            if (!a->GetOwner() || !b->GetOwner()) continue;

            // Skip static-static pairs
            if (a->bodyType == RigidBodyType::Static &&
                b->bodyType == RigidBodyType::Static) continue;

            Collider* colA = a->GetOwner()->GetComponent<Collider>();
            Collider* colB = b->GetOwner()->GetComponent<Collider>();
            if (!colA || !colB) continue;

            // Only AABB-AABB (Box) for now
            if (colA->type != ColliderType::Box ||
                colB->type != ColliderType::Box) continue;

            Vec3 posA = a->GetOwner()->GetTransform().position;
            Vec3 posB = b->GetOwner()->GetTransform().position;
            Vec3 sclA = a->GetOwner()->GetTransform().scale;
            Vec3 sclB = b->GetOwner()->GetTransform().scale;

            // Effective half-extents (collider half-size × transform scale)
            Vec3 halfA(colA->boxHalfExtents.x * sclA.x,
                       colA->boxHalfExtents.y * sclA.y,
                       colA->boxHalfExtents.z * sclA.z);
            Vec3 halfB(colB->boxHalfExtents.x * sclB.x,
                       colB->boxHalfExtents.y * sclB.y,
                       colB->boxHalfExtents.z * sclB.z);

            f32 dx = posB.x - posA.x;
            f32 dy = posB.y - posA.y;
            f32 dz = posB.z - posA.z;
            f32 overlapX = (halfA.x + halfB.x) - std::fabs(dx);
            f32 overlapY = (halfA.y + halfB.y) - std::fabs(dy);
            f32 overlapZ = (halfA.z + halfB.z) - std::fabs(dz);

            if (overlapX > 0 && overlapY > 0 && overlapZ > 0) {
                CollisionInfo info;
                info.objectA = a->GetOwner();
                info.objectB = b->GetOwner();

                // Resolve along axis of least penetration
                if (overlapX <= overlapY && overlapX <= overlapZ) {
                    info.contactNormal = Vec3(dx > 0 ? 1.0f : -1.0f, 0, 0);
                    info.penetrationDepth = overlapX;
                } else if (overlapY <= overlapX && overlapY <= overlapZ) {
                    info.contactNormal = Vec3(0, dy > 0 ? 1.0f : -1.0f, 0);
                    info.penetrationDepth = overlapY;
                } else {
                    info.contactNormal = Vec3(0, 0, dz > 0 ? 1.0f : -1.0f);
                    info.penetrationDepth = overlapZ;
                }
                info.contactPoint = Vec3(
                    (posA.x + posB.x) * 0.5f,
                    (posA.y + posB.y) * 0.5f,
                    (posA.z + posB.z) * 0.5f);
                m_Collisions.push_back(info);
            }
        }
    }

    // Debug log — throttled (once per ~60 calls ≈ once per second)
    static i32 colLogThrottle = 0;
    if (!m_Collisions.empty() && (colLogThrottle % 60 == 0)) {
        for (auto& c : m_Collisions) {
            GV_LOG_DEBUG("[Physics] Collision: '" + c.objectA->GetName() +
                         "' <-> '" + c.objectB->GetName() + "'");
        }
    }
    colLogThrottle++;
}

void PhysicsWorld::ResolveCollisions() {
    for (auto& c : m_Collisions) {
        if (!c.objectA || !c.objectB) continue;

        RigidBody* rbA = c.objectA->GetComponent<RigidBody>();
        RigidBody* rbB = c.objectB->GetComponent<RigidBody>();
        if (!rbA || !rbB) continue;

        bool dynA = (rbA->bodyType == RigidBodyType::Dynamic);
        bool dynB = (rbB->bodyType == RigidBodyType::Dynamic);
        if (!dynA && !dynB) continue;

        Transform& tA = c.objectA->GetTransform();
        Transform& tB = c.objectB->GetTransform();
        Vec3 normal = c.contactNormal;
        f32  depth  = c.penetrationDepth;

        // 1. Positional correction — push objects apart
        if (dynA && dynB) {
            tA.position = tA.position - normal * (depth * 0.5f);
            tB.position = tB.position + normal * (depth * 0.5f);
        } else if (dynA) {
            tA.position = tA.position - normal * depth;
        } else {
            tB.position = tB.position + normal * depth;
        }

        // 2. Impulse-based velocity response
        Vec3 relVel = rbB->velocity - rbA->velocity;
        f32  velAlongNormal = relVel.Dot(normal);
        if (velAlongNormal > 0.0f) continue; // already separating

        f32 e = (rbA->restitution < rbB->restitution)
                    ? rbA->restitution : rbB->restitution;
        f32 invMassA = dynA ? (1.0f / rbA->mass) : 0.0f;
        f32 invMassB = dynB ? (1.0f / rbB->mass) : 0.0f;

        f32 j = -(1.0f + e) * velAlongNormal / (invMassA + invMassB);
        Vec3 impulse = normal * j;

        if (dynA) rbA->velocity = rbA->velocity - impulse * invMassA;
        if (dynB) rbB->velocity = rbB->velocity + impulse * invMassB;
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
