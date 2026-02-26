// ============================================================================
// GameVoid Engine — 2D Scene
// ============================================================================
// Manages a list of 2D GameObjects with sorting layers, rendering order,
// and a simple 2D physics step.
// ============================================================================
#pragma once

#include "core/Types.h"
#include "core/GameObject.h"
#include "editor2d/Editor2DTypes.h"
#include <string>
#include <vector>
#include <algorithm>

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
        // Update sprite animations
        for (auto& o : m_Objects) {
            if (!o->IsActive()) continue;
            auto* spr = o->GetComponent<SpriteComponent>();
            if (spr) spr->UpdateAnimation(dt);
            o->Update(dt);
        }
        // Simple 2D physics step
        StepPhysics(dt);
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

    // ── 2D Physics (very simplified) ───────────────────────────────────────
    Vec2 gravity { 0.0f, -9.81f };

    // ── Meta ───────────────────────────────────────────────────────────────
    const std::string& GetName() const { return m_Name; }
    void SetName(const std::string& n) { m_Name = n; }

private:
    i32 LayerOrder(const std::string& layerName) const {
        for (auto& l : m_SortLayers)
            if (l.name == layerName) return l.order;
        return 0;
    }

    void StepPhysics(f32 dt) {
        for (auto& o : m_Objects) {
            if (!o->IsActive()) continue;
            auto* rb = o->GetComponent<RigidBody2D>();
            if (!rb || rb->bodyType == BodyType2D::Static) continue;

            // Apply gravity
            rb->velocity.x += gravity.x * rb->gravityScale * dt;
            rb->velocity.y += gravity.y * rb->gravityScale * dt;

            // Damping
            rb->velocity.x *= (1.0f - rb->linearDamping * dt);
            rb->velocity.y *= (1.0f - rb->linearDamping * dt);
            rb->angularVel *= (1.0f - rb->angularDamping * dt);

            // Integrate
            auto& t = o->GetTransform();
            t.position.x += rb->velocity.x * dt;
            t.position.y += rb->velocity.y * dt;
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
