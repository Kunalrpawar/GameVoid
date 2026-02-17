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
        Collider* col = rb->GetOwner()->GetComponent<Collider>();

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

        // ── Angular dynamics ───────────────────────────────────────────
        // Apply torque: τ = Iα → α = I⁻¹τ
        if (col && rb->mass > 0.0f) {
            Vec3 invInertia = rb->GetInverseInertiaTensor(col, t.scale);
            // Angular acceleration = I⁻¹ * torque
            Vec3 angAccel(rb->torque.x * invInertia.x,
                          rb->torque.y * invInertia.y,
                          rb->torque.z * invInertia.z);
            rb->angularVelocity = rb->angularVelocity + angAccel * dt;
        }

        // Angular drag
        rb->angularVelocity = rb->angularVelocity * (1.0f / (1.0f + rb->angularDrag * dt));

        // Integrate rotation (quaternion integration)
        // dq/dt = 0.5 * w * q  (where w is angular velocity as a quaternion with w=0)
        f32 avLen = std::sqrt(rb->angularVelocity.x * rb->angularVelocity.x +
                              rb->angularVelocity.y * rb->angularVelocity.y +
                              rb->angularVelocity.z * rb->angularVelocity.z);
        if (avLen > 1e-6f) {
            f32 halfAngle = avLen * dt * 0.5f;
            f32 sinHA = std::sin(halfAngle) / avLen;
            Quaternion deltaQ;
            deltaQ.w = std::cos(halfAngle);
            deltaQ.x = rb->angularVelocity.x * sinHA;
            deltaQ.y = rb->angularVelocity.y * sinHA;
            deltaQ.z = rb->angularVelocity.z * sinHA;

            // Multiply: rotation = deltaQ * rotation
            Quaternion& q = t.rotation;
            Quaternion newQ;
            newQ.w = deltaQ.w * q.w - deltaQ.x * q.x - deltaQ.y * q.y - deltaQ.z * q.z;
            newQ.x = deltaQ.w * q.x + deltaQ.x * q.w + deltaQ.y * q.z - deltaQ.z * q.y;
            newQ.y = deltaQ.w * q.y - deltaQ.x * q.z + deltaQ.y * q.w + deltaQ.z * q.x;
            newQ.z = deltaQ.w * q.z + deltaQ.x * q.y - deltaQ.y * q.x + deltaQ.z * q.w;

            // Normalize to prevent drift
            f32 len = std::sqrt(newQ.w*newQ.w + newQ.x*newQ.x + newQ.y*newQ.y + newQ.z*newQ.z);
            if (len > 1e-8f) {
                f32 invLen = 1.0f / len;
                newQ.w *= invLen; newQ.x *= invLen; newQ.y *= invLen; newQ.z *= invLen;
            }
            q = newQ;
        }

        // Floor constraint (Y = 0 plane)
        if (col && col->type == ColliderType::Box) {
            f32 halfY = col->boxHalfExtents.y * t.scale.y;
            if (t.position.y - halfY < 0.0f) {
                t.position.y = halfY;
                if (rb->velocity.y < 0.0f) {
                    rb->velocity.y = -rb->velocity.y * rb->restitution;
                    if (rb->velocity.y < 0.1f) rb->velocity.y = 0.0f;
                }
                // Friction-based angular damping on ground contact
                rb->angularVelocity = rb->angularVelocity * 0.95f;
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

            Vec3 posA = a->GetOwner()->GetTransform().position;
            Vec3 posB = b->GetOwner()->GetTransform().position;
            Vec3 sclA = a->GetOwner()->GetTransform().scale;
            Vec3 sclB = b->GetOwner()->GetTransform().scale;

            CollisionInfo info;
            info.objectA = a->GetOwner();
            info.objectB = b->GetOwner();
            bool collided = false;

            // ── Box vs Box ─────────────────────────────────────────────
            if (colA->type == ColliderType::Box && colB->type == ColliderType::Box) {
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
                    collided = true;
                }
            }
            // ── Sphere vs Sphere ───────────────────────────────────────
            else if (colA->type == ColliderType::Sphere && colB->type == ColliderType::Sphere) {
                f32 rA = colA->radius * sclA.x; // uniform scale assumption
                f32 rB = colB->radius * sclB.x;
                Vec3 diff = posB - posA;
                f32 dist2 = diff.x * diff.x + diff.y * diff.y + diff.z * diff.z;
                f32 sumR = rA + rB;
                if (dist2 < sumR * sumR && dist2 > 1e-8f) {
                    f32 dist = std::sqrt(dist2);
                    info.contactNormal = diff * (1.0f / dist);
                    info.penetrationDepth = sumR - dist;
                    collided = true;
                }
            }
            // ── Sphere vs Box (or Box vs Sphere) ───────────────────────
            else if ((colA->type == ColliderType::Sphere && colB->type == ColliderType::Box) ||
                     (colA->type == ColliderType::Box && colB->type == ColliderType::Sphere)) {
                // Identify which is sphere and which is box
                bool aIsSphere = (colA->type == ColliderType::Sphere);
                Vec3 sPos = aIsSphere ? posA : posB;
                Vec3 bPos = aIsSphere ? posB : posA;
                Vec3 bScl = aIsSphere ? sclB : sclA;
                Collider* sCol = aIsSphere ? colA : colB;
                Collider* bCol = aIsSphere ? colB : colA;

                f32 sR = sCol->radius * (aIsSphere ? sclA.x : sclB.x);
                Vec3 halfB(bCol->boxHalfExtents.x * bScl.x,
                           bCol->boxHalfExtents.y * bScl.y,
                           bCol->boxHalfExtents.z * bScl.z);
                Vec3 boxMin = bPos - halfB;
                Vec3 boxMax = bPos + halfB;

                auto clamp = [](f32 v, f32 lo, f32 hi) { return v < lo ? lo : (v > hi ? hi : v); };
                Vec3 closest(clamp(sPos.x, boxMin.x, boxMax.x),
                             clamp(sPos.y, boxMin.y, boxMax.y),
                             clamp(sPos.z, boxMin.z, boxMax.z));
                Vec3 diff = sPos - closest;
                f32 dist2 = diff.x * diff.x + diff.y * diff.y + diff.z * diff.z;
                if (dist2 < sR * sR && dist2 > 1e-8f) {
                    f32 dist = std::sqrt(dist2);
                    Vec3 normal = diff * (1.0f / dist);
                    if (!aIsSphere) normal = normal * -1.0f; // ensure A→B direction
                    info.contactNormal = normal;
                    info.penetrationDepth = sR - dist;
                    collided = true;
                }
            }
            // ── Capsule vs Sphere ──────────────────────────────────────
            else if ((colA->type == ColliderType::Capsule && colB->type == ColliderType::Sphere) ||
                     (colA->type == ColliderType::Sphere && colB->type == ColliderType::Capsule)) {
                bool aIsCapsule = (colA->type == ColliderType::Capsule);
                Collider* capCol = aIsCapsule ? colA : colB;
                Collider* sphCol = aIsCapsule ? colB : colA;
                Vec3 capPos = aIsCapsule ? posA : posB;
                Vec3 sphPos = aIsCapsule ? posB : posA;
                f32 capScl = aIsCapsule ? sclA.x : sclB.x;
                f32 sphScl = aIsCapsule ? sclB.x : sclA.x;

                f32 capR = capCol->radius * capScl;
                f32 halfH = (capCol->capsuleHeight * capScl * 0.5f) - capR;
                if (halfH < 0) halfH = 0;
                Vec3 capTop = capPos + Vec3(0, halfH, 0);
                Vec3 capBot = capPos - Vec3(0, halfH, 0);
                f32 sR = sphCol->radius * sphScl;

                // Closest point on capsule line segment to sphere center
                Vec3 seg = capTop - capBot;
                f32 segLen2 = seg.x * seg.x + seg.y * seg.y + seg.z * seg.z;
                f32 t = 0.5f;
                if (segLen2 > 1e-8f) {
                    Vec3 d = sphPos - capBot;
                    t = (d.x * seg.x + d.y * seg.y + d.z * seg.z) / segLen2;
                    if (t < 0) t = 0; if (t > 1) t = 1;
                }
                Vec3 closest = capBot + seg * t;
                Vec3 diff = sphPos - closest;
                f32 dist2 = diff.x * diff.x + diff.y * diff.y + diff.z * diff.z;
                f32 sumR = capR + sR;
                if (dist2 < sumR * sumR && dist2 > 1e-8f) {
                    f32 dist = std::sqrt(dist2);
                    info.contactNormal = diff * (1.0f / dist);
                    if (!aIsCapsule) info.contactNormal = info.contactNormal * -1.0f;
                    info.penetrationDepth = sumR - dist;
                    collided = true;
                }
            }
            // ── Capsule vs Box ─────────────────────────────────────────
            else if ((colA->type == ColliderType::Capsule && colB->type == ColliderType::Box) ||
                     (colA->type == ColliderType::Box && colB->type == ColliderType::Capsule)) {
                // Approximate: treat capsule as a sphere at closest point on its axis
                bool aIsCapsule = (colA->type == ColliderType::Capsule);
                Collider* capCol = aIsCapsule ? colA : colB;
                Collider* boxCol = aIsCapsule ? colB : colA;
                Vec3 capPos = aIsCapsule ? posA : posB;
                Vec3 boxPos = aIsCapsule ? posB : posA;
                Vec3 boxScl = aIsCapsule ? sclB : sclA;
                f32 capScl = aIsCapsule ? sclA.x : sclB.x;

                f32 capR = capCol->radius * capScl;
                f32 halfH = (capCol->capsuleHeight * capScl * 0.5f) - capR;
                if (halfH < 0) halfH = 0;

                Vec3 halfB(boxCol->boxHalfExtents.x * boxScl.x,
                           boxCol->boxHalfExtents.y * boxScl.y,
                           boxCol->boxHalfExtents.z * boxScl.z);
                Vec3 boxMin = boxPos - halfB;
                Vec3 boxMax = boxPos + halfB;

                // Test sphere at top and bottom of capsule, take nearest
                auto clampF = [](f32 v, f32 lo, f32 hi) { return v < lo ? lo : (v > hi ? hi : v); };
                auto testSphereBox = [&](Vec3 sc) -> bool {
                    Vec3 cl(clampF(sc.x, boxMin.x, boxMax.x),
                            clampF(sc.y, boxMin.y, boxMax.y),
                            clampF(sc.z, boxMin.z, boxMax.z));
                    Vec3 d = sc - cl;
                    f32 d2 = d.x * d.x + d.y * d.y + d.z * d.z;
                    if (d2 < capR * capR && d2 > 1e-8f) {
                        f32 dist = std::sqrt(d2);
                        info.contactNormal = d * (1.0f / dist);
                        if (!aIsCapsule) info.contactNormal = info.contactNormal * -1.0f;
                        info.penetrationDepth = capR - dist;
                        return true;
                    }
                    return false;
                };
                Vec3 capTop = capPos + Vec3(0, halfH, 0);
                Vec3 capBot = capPos - Vec3(0, halfH, 0);
                collided = testSphereBox(capPos) || testSphereBox(capTop) || testSphereBox(capBot);
            }
            // ── Capsule vs Capsule ─────────────────────────────────────
            else if (colA->type == ColliderType::Capsule && colB->type == ColliderType::Capsule) {
                f32 rA = colA->radius * sclA.x;
                f32 hA = (colA->capsuleHeight * sclA.x * 0.5f) - rA;
                if (hA < 0) hA = 0;
                f32 rB = colB->radius * sclB.x;
                f32 hB = (colB->capsuleHeight * sclB.x * 0.5f) - rB;
                if (hB < 0) hB = 0;

                Vec3 a1 = posA - Vec3(0, hA, 0), a2 = posA + Vec3(0, hA, 0);
                Vec3 b1 = posB - Vec3(0, hB, 0), b2 = posB + Vec3(0, hB, 0);

                // Closest points between two line segments (simplified)
                Vec3 dA = a2 - a1, dB = b2 - b1;
                Vec3 r0 = a1 - b1;
                f32 aa = dA.x*dA.x + dA.y*dA.y + dA.z*dA.z;
                f32 ee = dB.x*dB.x + dB.y*dB.y + dB.z*dB.z;
                f32 bb = dA.x*dB.x + dA.y*dB.y + dA.z*dB.z;
                f32 cc = dA.x*r0.x + dA.y*r0.y + dA.z*r0.z;
                f32 ff = dB.x*r0.x + dB.y*r0.y + dB.z*r0.z;
                f32 denom = aa * ee - bb * bb;
                f32 sN = 0, tN = 0;
                if (std::fabs(denom) > 1e-8f) {
                    sN = (bb * ff - ee * cc) / denom;
                    tN = (aa * ff - bb * cc) / denom;
                }
                if (sN < 0) sN = 0; if (sN > 1) sN = 1;
                if (tN < 0) tN = 0; if (tN > 1) tN = 1;
                Vec3 cpA = a1 + dA * sN;
                Vec3 cpB = b1 + dB * tN;
                Vec3 diff = cpB - cpA;
                f32 dist2 = diff.x*diff.x + diff.y*diff.y + diff.z*diff.z;
                f32 sumR = rA + rB;
                if (dist2 < sumR * sumR && dist2 > 1e-8f) {
                    f32 dist = std::sqrt(dist2);
                    info.contactNormal = diff * (1.0f / dist);
                    info.penetrationDepth = sumR - dist;
                    collided = true;
                }
            }

            if (collided) {
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

        Collider* colA = c.objectA->GetComponent<Collider>();
        Collider* colB = c.objectB->GetComponent<Collider>();

        // 1. Positional correction — push objects apart
        if (dynA && dynB) {
            tA.position = tA.position - normal * (depth * 0.5f);
            tB.position = tB.position + normal * (depth * 0.5f);
        } else if (dynA) {
            tA.position = tA.position - normal * depth;
        } else {
            tB.position = tB.position + normal * depth;
        }

        // 2. Compute contact point relative to each body's center of mass
        Vec3 rA = c.contactPoint - tA.position;
        Vec3 rB = c.contactPoint - tB.position;

        // 3. Compute relative velocity at contact point (includes angular contribution)
        // v_contact = v_linear + angular_velocity × r
        Vec3 vA = rbA->velocity + rbA->angularVelocity.Cross(rA);
        Vec3 vB = rbB->velocity + rbB->angularVelocity.Cross(rB);
        Vec3 relVel = vB - vA;

        f32 velAlongNormal = relVel.Dot(normal);
        if (velAlongNormal > 0.0f) continue; // already separating

        f32 e = (rbA->restitution < rbB->restitution)
                    ? rbA->restitution : rbB->restitution;
        f32 invMassA = dynA ? (1.0f / rbA->mass) : 0.0f;
        f32 invMassB = dynB ? (1.0f / rbB->mass) : 0.0f;

        // Compute inverse inertia contributions
        Vec3 invIA = (dynA && colA) ? rbA->GetInverseInertiaTensor(colA, tA.scale) : Vec3(0, 0, 0);
        Vec3 invIB = (dynB && colB) ? rbB->GetInverseInertiaTensor(colB, tB.scale) : Vec3(0, 0, 0);

        // Cross products for angular contribution to impulse denominator
        Vec3 rAxN = rA.Cross(normal);
        Vec3 rBxN = rB.Cross(normal);

        // Angular contribution: (I⁻¹(r×n)) × r · n
        Vec3 angTermA(rAxN.x * invIA.x, rAxN.y * invIA.y, rAxN.z * invIA.z);
        Vec3 angTermB(rBxN.x * invIB.x, rBxN.y * invIB.y, rBxN.z * invIB.z);
        f32 angDenomA = angTermA.Cross(rA).Dot(normal);
        f32 angDenomB = angTermB.Cross(rB).Dot(normal);

        f32 j = -(1.0f + e) * velAlongNormal / (invMassA + invMassB + angDenomA + angDenomB);
        Vec3 impulse = normal * j;

        // Apply linear impulse
        if (dynA) rbA->velocity = rbA->velocity - impulse * invMassA;
        if (dynB) rbB->velocity = rbB->velocity + impulse * invMassB;

        // Apply angular impulse: Δω = I⁻¹ * (r × impulse)
        if (dynA) {
            Vec3 angImpA = rA.Cross(impulse * -1.0f);
            rbA->angularVelocity = rbA->angularVelocity + Vec3(
                angImpA.x * invIA.x, angImpA.y * invIA.y, angImpA.z * invIA.z);
        }
        if (dynB) {
            Vec3 angImpB = rB.Cross(impulse);
            rbB->angularVelocity = rbB->angularVelocity + Vec3(
                angImpB.x * invIB.x, angImpB.y * invIB.y, angImpB.z * invIB.z);
        }
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

// ============================================================================
// Inertia Tensor Computation
// ============================================================================
// Diagonal inertia tensors for simple shapes (uniform density assumed).

Vec3 RigidBody::ComputeInertiaTensor(const Collider* collider, const Vec3& scale) const {
    if (!collider || mass <= 0.0f) return Vec3(1, 1, 1);

    f32 m = mass;

    switch (collider->type) {
        case ColliderType::Box: {
            // Box: I = (m/12) * (h² + d², w² + d², w² + h²)
            f32 w = 2.0f * collider->boxHalfExtents.x * scale.x;
            f32 h = 2.0f * collider->boxHalfExtents.y * scale.y;
            f32 d = 2.0f * collider->boxHalfExtents.z * scale.z;
            f32 factor = m / 12.0f;
            return Vec3(factor * (h*h + d*d),
                        factor * (w*w + d*d),
                        factor * (w*w + h*h));
        }
        case ColliderType::Sphere: {
            // Solid sphere: I = (2/5) * m * r²
            f32 r = collider->radius * scale.x;
            f32 I = 0.4f * m * r * r;
            return Vec3(I, I, I);
        }
        case ColliderType::Capsule: {
            // Approximate as cylinder + hemisphere end caps
            f32 r = collider->radius * scale.x;
            f32 totalH = collider->capsuleHeight * scale.x;
            f32 cylH = totalH - 2.0f * r;
            if (cylH < 0) cylH = 0;
            // Cylinder I_xx = I_zz = m*(3r² + h²)/12, I_yy = m*r²/2
            f32 Ixx = m * (3.0f * r * r + cylH * cylH) / 12.0f;
            f32 Iyy = m * r * r * 0.5f;
            return Vec3(Ixx, Iyy, Ixx);
        }
        default:
            return Vec3(m, m, m); // fallback: unit inertia
    }
}

Vec3 RigidBody::GetInverseInertiaTensor(const Collider* collider, const Vec3& scale) const {
    Vec3 I = ComputeInertiaTensor(collider, scale);
    return Vec3(
        (I.x > 1e-8f) ? (1.0f / I.x) : 0.0f,
        (I.y > 1e-8f) ? (1.0f / I.y) : 0.0f,
        (I.z > 1e-8f) ? (1.0f / I.z) : 0.0f
    );
}

} // namespace gv
