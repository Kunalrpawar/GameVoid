// ============================================================================
// GameVoid Engine — 2D Scene
// ============================================================================
// Manages a list of 2D GameObjects with sorting layers, rendering order,
// and a full 2D physics step with AABB collision detection + response.
//
// The physics pipeline:
//   1. Apply gravity & integrate velocity → tentative position
//   2. Broad-phase: find all collider pairs
//   3. Narrow-phase: AABB / Circle overlap test
//   4. Resolve penetration (push objects apart)
//   5. Adjust velocity (cancel component into surface, apply bounce)
//   6. Fire collision callbacks (enter/stay/exit, trigger)
//   7. Update platformer controllers (ground detection from collisions)
//   8. Update camera follow
//   9. Update collectible bobs
// ============================================================================
#pragma once

#include "core/Types.h"
#include "core/GameObject.h"
#include "editor2d/Editor2DTypes.h"
#include <string>
#include <vector>
#include <algorithm>
#include <set>
#include <cmath>

namespace gv {

class Scene2D {
public:
    explicit Scene2D(const std::string& name = "Untitled 2D Scene")
        : m_Name(name) {}

    ~Scene2D() = default;

    // ── Object management ──────────────────────────────────────────────────
    GameObject* CreateGameObject(const std::string& name = "Sprite") {
        auto obj = MakeShared<GameObject>(name);
        obj->SetID(m_NextID++);
        m_Objects.push_back(obj);
        return obj.get();
    }

    GameObject* FindByName(const std::string& name) const {
        for (auto& o : m_Objects)
            if (o->GetName() == name) return o.get();
        return nullptr;
    }

    GameObject* FindByID(u32 id) const {
        for (auto& o : m_Objects)
            if (o->GetID() == id) return o.get();
        return nullptr;
    }

    void DestroyGameObject(GameObject* obj) {
        m_PendingDestroy.push_back(obj);
    }

    const std::vector<Shared<GameObject>>& GetAllObjects() const { return m_Objects; }

    // ── Lifecycle ──────────────────────────────────────────────────────────
    void Update(f32 dt) {
        // Cap delta time to avoid spiral of death
        if (dt > 0.05f) dt = 0.05f;

        // Update sprite animations
        for (auto& o : m_Objects) {
            if (!o->IsActive()) continue;
            auto* spr = o->GetComponent<SpriteComponent>();
            if (spr) spr->UpdateAnimation(dt);

            // Update animation state machine → sprite
            auto* asm2d = o->GetComponent<AnimStateMachine2D>();
            if (asm2d && spr) {
                auto* state = asm2d->GetCurrentState();
                if (state) {
                    spr->frameRate = state->frameRate;
                    spr->animLooping = state->loop;
                    spr->animPlaying = true;
                    // Set frame range for current state
                    i32 totalFrames = state->endFrame - state->startFrame + 1;
                    if (totalFrames < 1) totalFrames = 1;
                    spr->frameCount = totalFrames;
                    // Offset current frame into state's range
                    // (the sprite animation system handles 0..frameCount-1)
                }
            }

            // Update collectible bob
            auto* coll = o->GetComponent<Collectible2D>();
            if (coll && !coll->collected) {
                coll->UpdateBob(dt);
            }

            o->Update(dt);
        }

        // Full physics step
        StepPhysics(dt);

        // Update camera follow
        UpdateCameraFollow(dt);

        FlushDestroyQueue();
    }

    // ── Sorting layers ─────────────────────────────────────────────────────
    std::vector<SortLayer>& GetSortLayers() { return m_SortLayers; }
    const std::vector<SortLayer>& GetSortLayers() const { return m_SortLayers; }

    void AddSortLayer(const std::string& name, i32 order) {
        m_SortLayers.push_back({ name, order });
        SortLayers();
    }

    void SortLayers() {
        std::sort(m_SortLayers.begin(), m_SortLayers.end(),
            [](const SortLayer& a, const SortLayer& b) { return a.order < b.order; });
    }

    /// Get draw-ordered list of sprites for rendering.
    std::vector<GameObject*> GetSortedRenderList() const {
        std::vector<GameObject*> list;
        for (auto& o : m_Objects) {
            if (o->IsActive() && o->GetComponent<SpriteComponent>())
                list.push_back(o.get());
        }
        // Sort by layer order then sort order within layer
        std::sort(list.begin(), list.end(), [this](GameObject* a, GameObject* b) {
            auto* sa = a->GetComponent<SpriteComponent>();
            auto* sb = b->GetComponent<SpriteComponent>();
            i32 la = LayerOrder(sa->sortLayer);
            i32 lb = LayerOrder(sb->sortLayer);
            if (la != lb) return la < lb;
            return sa->sortOrder < sb->sortOrder;
        });
        return list;
    }

    // ── 2D Physics settings ────────────────────────────────────────────────
    Vec2 gravity     { 0.0f, -20.0f };     // Mario-style needs strong gravity
    f32  bounciness  = 0.0f;               // global bounce factor (0 = no bounce)
    i32  solverIterations = 4;             // collision resolution iterations

    // ── Meta ───────────────────────────────────────────────────────────────
    const std::string& GetName() const { return m_Name; }
    void SetName(const std::string& n) { m_Name = n; }

    // ── Camera follow output (used by viewport) ────────────────────────────
    Vec2 cameraFollowPos { 0, 0 };
    bool hasCameraFollow = false;

private:
    i32 LayerOrder(const std::string& layerName) const {
        for (auto& l : m_SortLayers)
            if (l.name == layerName) return l.order;
        return 0;
    }

    // ════════════════════════════════════════════════════════════════════════
    // AABB helper
    // ════════════════════════════════════════════════════════════════════════
    struct AABB {
        f32 minX, minY, maxX, maxY;
    };

    static AABB GetColliderAABB(GameObject* obj) {
        auto& t = obj->GetTransform();
        auto* col = obj->GetComponent<Collider2D>();
        AABB aabb;
        f32 cx = t.position.x + (col ? col->offset.x : 0.0f);
        f32 cy = t.position.y + (col ? col->offset.y : 0.0f);

        if (col) {
            switch (col->shape) {
            case ColliderShape2D::Box:
                aabb.minX = cx - col->boxSize.x * t.scale.x;
                aabb.minY = cy - col->boxSize.y * t.scale.y;
                aabb.maxX = cx + col->boxSize.x * t.scale.x;
                aabb.maxY = cy + col->boxSize.y * t.scale.y;
                break;
            case ColliderShape2D::Circle:
                aabb.minX = cx - col->radius * t.scale.x;
                aabb.minY = cy - col->radius * t.scale.y;
                aabb.maxX = cx + col->radius * t.scale.x;
                aabb.maxY = cy + col->radius * t.scale.y;
                break;
            case ColliderShape2D::Capsule:
                aabb.minX = cx - col->radius * t.scale.x;
                aabb.minY = cy - (col->height * 0.5f) * t.scale.y;
                aabb.maxX = cx + col->radius * t.scale.x;
                aabb.maxY = cy + (col->height * 0.5f) * t.scale.y;
                break;
            default: {
                // Use sprite size as fallback
                auto* spr = obj->GetComponent<SpriteComponent>();
                f32 hw = spr ? spr->size.x * t.scale.x * 0.5f : 0.5f * t.scale.x;
                f32 hh = spr ? spr->size.y * t.scale.y * 0.5f : 0.5f * t.scale.y;
                aabb.minX = cx - hw;
                aabb.minY = cy - hh;
                aabb.maxX = cx + hw;
                aabb.maxY = cy + hh;
                break;
            }
            }
        } else {
            // No collider — use sprite bounds
            auto* spr = obj->GetComponent<SpriteComponent>();
            f32 hw = spr ? spr->size.x * t.scale.x * 0.5f : 0.5f * t.scale.x;
            f32 hh = spr ? spr->size.y * t.scale.y * 0.5f : 0.5f * t.scale.y;
            aabb.minX = cx - hw;
            aabb.minY = cy - hh;
            aabb.maxX = cx + hw;
            aabb.maxY = cy + hh;
        }
        return aabb;
    }

    static bool AABBOverlap(const AABB& a, const AABB& b) {
        return a.minX < b.maxX && a.maxX > b.minX &&
               a.minY < b.maxY && a.maxY > b.minY;
    }

    // Returns overlap and normal pointing from A toward B
    static bool AABBResolve(const AABB& a, const AABB& b,
                            f32& overlapOut, Vec2& normalOut) {
        f32 overlapX1 = a.maxX - b.minX;  // A right vs B left
        f32 overlapX2 = b.maxX - a.minX;  // B right vs A left
        f32 overlapY1 = a.maxY - b.minY;  // A top vs B bottom
        f32 overlapY2 = b.maxY - a.minY;  // B top vs A bottom

        f32 minOverlapX = (overlapX1 < overlapX2) ? overlapX1 : overlapX2;
        f32 minOverlapY = (overlapY1 < overlapY2) ? overlapY1 : overlapY2;

        if (minOverlapX < minOverlapY) {
            overlapOut = minOverlapX;
            normalOut = (overlapX1 < overlapX2) ? Vec2(-1, 0) : Vec2(1, 0);
        } else {
            overlapOut = minOverlapY;
            normalOut = (overlapY1 < overlapY2) ? Vec2(0, -1) : Vec2(0, 1);
        }
        return true;
    }

    // ════════════════════════════════════════════════════════════════════════
    // Full physics step
    // ════════════════════════════════════════════════════════════════════════
    void StepPhysics(f32 dt) {
        // 1. Apply gravity and integrate velocity → update positions
        for (auto& o : m_Objects) {
            if (!o->IsActive()) continue;
            auto* rb = o->GetComponent<RigidBody2D>();
            if (!rb || rb->bodyType == BodyType2D::Static) continue;

            // Apply gravity (skip kinematic)
            if (rb->bodyType == BodyType2D::Dynamic) {
                rb->velocity.x += gravity.x * rb->gravityScale * dt;
                rb->velocity.y += gravity.y * rb->gravityScale * dt;
            }

            // Clamp max fall speed to prevent tunneling
            f32 maxFallSpeed = -40.0f;
            if (rb->velocity.y < maxFallSpeed) rb->velocity.y = maxFallSpeed;

            // Damping
            rb->velocity.x *= (1.0f - rb->linearDamping * dt);
            // Don't damp Y — gravity handles it, damping Y makes platforms feel bad
            rb->angularVel *= (1.0f - rb->angularDamping * dt);

            // Integrate position
            auto& t = o->GetTransform();
            t.position.x += rb->velocity.x * dt;
            t.position.y += rb->velocity.y * dt;
        }

        // 2. Reset ground flags for platformer controllers
        for (auto& o : m_Objects) {
            auto* pc = o->GetComponent<PlatformerController2D>();
            if (pc) pc->isGrounded = false;
        }

        // 3. Collision detection & resolution (multiple iterations)
        for (i32 iter = 0; iter < solverIterations; iter++) {
            for (size_t i = 0; i < m_Objects.size(); i++) {
                auto* a = m_Objects[i].get();
                if (!a->IsActive()) continue;
                auto* colA = a->GetComponent<Collider2D>();
                if (!colA) continue;

                for (size_t j = i + 1; j < m_Objects.size(); j++) {
                    auto* b = m_Objects[j].get();
                    if (!b->IsActive()) continue;
                    auto* colB = b->GetComponent<Collider2D>();
                    if (!colB) continue;

                    auto* rbA = a->GetComponent<RigidBody2D>();
                    auto* rbB = b->GetComponent<RigidBody2D>();

                    // Both static → skip
                    bool aStatic = (!rbA || rbA->bodyType == BodyType2D::Static);
                    bool bStatic = (!rbB || rbB->bodyType == BodyType2D::Static);
                    if (aStatic && bStatic) continue;

                    AABB aabbA = GetColliderAABB(a);
                    AABB aabbB = GetColliderAABB(b);

                    if (!AABBOverlap(aabbA, aabbB)) continue;

                    // Compute overlap and normal
                    f32 overlap = 0;
                    Vec2 normal { 0, 0 };
                    AABBResolve(aabbA, aabbB, overlap, normal);

                    if (overlap <= 0) continue;

                    // Build collision info
                    Collision2DInfo infoA;
                    infoA.self = a;
                    infoA.other = b;
                    infoA.normal = normal;
                    infoA.overlap = overlap;
                    infoA.isTrigger = colA->isTrigger || colB->isTrigger;

                    Collision2DInfo infoB;
                    infoB.self = b;
                    infoB.other = a;
                    infoB.normal = { -normal.x, -normal.y };
                    infoB.overlap = overlap;
                    infoB.isTrigger = infoA.isTrigger;

                    // ── Trigger: no physics response, just callbacks ────────
                    if (infoA.isTrigger) {
                        auto* listA = a->GetComponent<CollisionListener2D>();
                        auto* listB = b->GetComponent<CollisionListener2D>();
                        if (listA && listA->onTriggerEnter) listA->onTriggerEnter(infoA);
                        if (listB && listB->onTriggerEnter) listB->onTriggerEnter(infoB);

                        // Handle collectibles
                        HandleCollectible(a, b);
                        HandleCollectible(b, a);

                        // Handle hazards
                        HandleHazard(a, b);
                        HandleHazard(b, a);
                        continue;
                    }

                    // ── Physics response ────────────────────────────────────

                    // Separate objects
                    if (aStatic) {
                        // Only move B
                        b->GetTransform().position.x -= normal.x * overlap;
                        b->GetTransform().position.y -= normal.y * overlap;
                    } else if (bStatic) {
                        // Only move A
                        a->GetTransform().position.x += normal.x * overlap;
                        a->GetTransform().position.y += normal.y * overlap;
                    } else {
                        // Both dynamic: split the separation
                        f32 half = overlap * 0.5f;
                        a->GetTransform().position.x += normal.x * half;
                        a->GetTransform().position.y += normal.y * half;
                        b->GetTransform().position.x -= normal.x * half;
                        b->GetTransform().position.y -= normal.y * half;
                    }

                    // Cancel velocity into the collision surface
                    if (rbA && !aStatic) {
                        f32 velDotN = rbA->velocity.x * (-normal.x) + rbA->velocity.y * (-normal.y);
                        if (velDotN > 0) {
                            rbA->velocity.x += (-normal.x) * velDotN * (1.0f + bounciness);
                            rbA->velocity.y += (-normal.y) * velDotN * (1.0f + bounciness);
                        }
                    }
                    if (rbB && !bStatic) {
                        f32 velDotN = rbB->velocity.x * normal.x + rbB->velocity.y * normal.y;
                        if (velDotN > 0) {
                            rbB->velocity.x += normal.x * velDotN * (1.0f + bounciness);
                            rbB->velocity.y += normal.y * velDotN * (1.0f + bounciness);
                        }
                    }

                    // ── Ground detection for platformer controllers ─────────
                    // Normal pointing UP from the surface means we landed on top
                    if (normal.y > 0.5f) {
                        // A is standing on B
                        auto* pcA = a->GetComponent<PlatformerController2D>();
                        if (pcA && rbA) {
                            pcA->isGrounded = true;
                            pcA->OnLanded();
                        }
                    }
                    if (normal.y < -0.5f) {
                        // B is standing on A
                        auto* pcB = b->GetComponent<PlatformerController2D>();
                        if (pcB && rbB) {
                            pcB->isGrounded = true;
                            pcB->OnLanded();
                        }
                    }

                    // Head bump: if normal.y < -0.5 and A was going up, cancel Y
                    if (normal.y < -0.5f && rbA && rbA->velocity.y > 0) {
                        rbA->velocity.y = 0;
                    }
                    if (normal.y > 0.5f && rbB && rbB->velocity.y > 0) {
                        rbB->velocity.y = 0;
                    }

                    // Wall detection for platformer
                    if (std::fabs(normal.x) > 0.5f) {
                        auto* pcA = a->GetComponent<PlatformerController2D>();
                        if (pcA && pcA->enableWallSlide && !pcA->isGrounded) {
                            pcA->isWallSliding = true;
                        }
                        auto* pcB = b->GetComponent<PlatformerController2D>();
                        if (pcB && pcB->enableWallSlide && !pcB->isGrounded) {
                            pcB->isWallSliding = true;
                        }
                    }

                    // Handle stomp-able enemies (Mario-style)
                    HandleStomp(a, b, normal);

                    // Fire callbacks
                    auto* listA = a->GetComponent<CollisionListener2D>();
                    auto* listB = b->GetComponent<CollisionListener2D>();
                    if (listA && listA->onCollisionEnter) listA->onCollisionEnter(infoA);
                    if (listB && listB->onCollisionEnter) listB->onCollisionEnter(infoB);

                    // Handle collectibles (non-trigger collision)
                    HandleCollectible(a, b);
                    HandleCollectible(b, a);

                    // Handle hazards (non-trigger)
                    HandleHazard(a, b);
                    HandleHazard(b, a);
                }
            }
        }

        // 4. Update platformer controllers
        for (auto& o : m_Objects) {
            if (!o->IsActive()) continue;
            auto* pc = o->GetComponent<PlatformerController2D>();
            auto* rb = o->GetComponent<RigidBody2D>();
            if (pc && rb) {
                pc->UpdateController(dt, rb);

                // Auto-flip sprite based on facing
                auto* spr = o->GetComponent<SpriteComponent>();
                if (spr) {
                    spr->flipX = !pc->isFacingRight;
                }
            }
        }
    }

    // ── Collectible pickup handler ─────────────────────────────────────────
    void HandleCollectible(GameObject* collector, GameObject* item) {
        auto* pc = collector->GetComponent<PlatformerController2D>();
        auto* coll = item->GetComponent<Collectible2D>();
        if (!pc || !coll || coll->collected) return;

        coll->collected = true;

        // Find game state and update score
        for (auto& o : m_Objects) {
            auto* gs = o->GetComponent<GameState2D>();
            if (gs) {
                gs->AddScore(coll->scoreValue);
                if (coll->type == Collectible2D::Type::Coin) gs->AddCoin();
                break;
            }
        }

        if (coll->destroyOnPickup) {
            DestroyGameObject(item);
        }
    }

    // ── Hazard damage handler ──────────────────────────────────────────────
    void HandleHazard(GameObject* victim, GameObject* hazardObj) {
        auto* pc = victim->GetComponent<PlatformerController2D>();
        auto* haz = hazardObj->GetComponent<Hazard2D>();
        if (!pc || !haz || pc->isDead) return;

        // Apply knockback
        auto* rb = victim->GetComponent<RigidBody2D>();
        if (rb) {
            f32 dir = (victim->GetTransform().position.x > hazardObj->GetTransform().position.x)
                      ? 1.0f : -1.0f;
            rb->velocity.x = haz->knockbackX * dir;
            rb->velocity.y = haz->knockbackY;
        }

        // Reduce lives via game state
        for (auto& o : m_Objects) {
            auto* gs = o->GetComponent<GameState2D>();
            if (gs) {
                gs->Die();
                if (gs->gameOver) pc->isDead = true;
                break;
            }
        }

        if (haz->destroyOnHit) {
            DestroyGameObject(hazardObj);
        }
    }

    // ── Mario stomp handler ────────────────────────────────────────────────
    void HandleStomp(GameObject* a, GameObject* b, const Vec2& normal) {
        // If A is falling onto B and B has a stompable hazard, stomp it
        auto* rbA = a->GetComponent<RigidBody2D>();
        auto* hazB = b->GetComponent<Hazard2D>();
        auto* pcA = a->GetComponent<PlatformerController2D>();
        if (pcA && rbA && hazB && hazB->canBeStomp && normal.y > 0.5f) {
            // A stomped B
            rbA->velocity.y = hazB->stompBounce;  // bounce up
            DestroyGameObject(b);
            // Award points
            for (auto& o : m_Objects) {
                auto* gs = o->GetComponent<GameState2D>();
                if (gs) { gs->AddScore(200); break; }
            }
            return;
        }

        // Reverse check
        auto* rbB = b->GetComponent<RigidBody2D>();
        auto* hazA = a->GetComponent<Hazard2D>();
        auto* pcB = b->GetComponent<PlatformerController2D>();
        if (pcB && rbB && hazA && hazA->canBeStomp && normal.y < -0.5f) {
            rbB->velocity.y = hazA->stompBounce;
            DestroyGameObject(a);
            for (auto& o : m_Objects) {
                auto* gs = o->GetComponent<GameState2D>();
                if (gs) { gs->AddScore(200); break; }
            }
        }
    }

    // ── Camera follow update ───────────────────────────────────────────────
    void UpdateCameraFollow(f32 dt) {
        hasCameraFollow = false;
        for (auto& o : m_Objects) {
            if (!o->IsActive()) continue;
            auto* cam = o->GetComponent<Camera2DFollow>();
            if (!cam || cam->targetObjectID == 0) continue;

            auto* target = FindByID(cam->targetObjectID);
            if (!target) continue;

            hasCameraFollow = true;
            Vec2 targetPos(target->GetTransform().position.x + cam->offset.x,
                           target->GetTransform().position.y + cam->offset.y);

            // Look-ahead based on velocity
            auto* rb = target->GetComponent<RigidBody2D>();
            if (rb) {
                f32 lookX = (rb->velocity.x > 0.5f) ? cam->lookAheadDist :
                            (rb->velocity.x < -0.5f) ? -cam->lookAheadDist : 0.0f;
                cam->lookAheadPos.x += (lookX - cam->lookAheadPos.x) * cam->lookAheadSpeed * dt;
                targetPos.x += cam->lookAheadPos.x;
            }

            // Dead zone
            f32 diffX = targetPos.x - cam->currentPos.x;
            f32 diffY = targetPos.y - cam->currentPos.y;
            if (std::fabs(diffX) < cam->deadZoneX) targetPos.x = cam->currentPos.x;
            if (std::fabs(diffY) < cam->deadZoneY) targetPos.y = cam->currentPos.y;

            // Smooth
            f32 t = 1.0f - std::exp(-cam->smoothSpeed * dt);
            cam->currentPos.x += (targetPos.x - cam->currentPos.x) * t;
            cam->currentPos.y += (targetPos.y - cam->currentPos.y) * t;

            // Clamp to bounds
            if (cam->useBounds) {
                if (cam->currentPos.x < cam->boundsMin.x) cam->currentPos.x = cam->boundsMin.x;
                if (cam->currentPos.x > cam->boundsMax.x) cam->currentPos.x = cam->boundsMax.x;
                if (cam->currentPos.y < cam->boundsMin.y) cam->currentPos.y = cam->boundsMin.y;
                if (cam->currentPos.y > cam->boundsMax.y) cam->currentPos.y = cam->boundsMax.y;
            }

            cameraFollowPos = cam->currentPos;
            break; // only one camera follow
        }
    }

    void FlushDestroyQueue() {
        for (auto* obj : m_PendingDestroy) {
            m_Objects.erase(
                std::remove_if(m_Objects.begin(), m_Objects.end(),
                    [obj](const Shared<GameObject>& o) { return o.get() == obj; }),
                m_Objects.end());
        }
        m_PendingDestroy.clear();
    }

    std::string m_Name;
    std::vector<Shared<GameObject>> m_Objects;
    std::vector<GameObject*> m_PendingDestroy;
    std::vector<SortLayer> m_SortLayers = { { "Background", -10 }, { "Default", 0 }, { "Foreground", 10 }, { "UI", 100 } };
    u32 m_NextID = 1;
};

} // namespace gv
