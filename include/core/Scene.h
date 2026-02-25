// ============================================================================
// GameVoid Engine — Scene Manager
// ============================================================================
// A Scene owns a flat list of GameObjects and orchestrates their lifecycle
// (Start → Update → Render).  The engine can hold multiple scenes and switch
// between them.
// ============================================================================
#pragma once

#include "core/Types.h"
#include "core/GameObject.h"
#include "renderer/Camera.h"
#include "physics/Physics.h"
#include <string>
#include <algorithm>

namespace gv {

class Scene {
public:
    explicit Scene(const std::string& name = "Untitled Scene")
        : m_Name(name) {}

    ~Scene() = default;

    // ── Object management ──────────────────────────────────────────────────
    /// Create a new empty GameObject in the scene and return a raw pointer.
    GameObject* CreateGameObject(const std::string& name = "GameObject") {
        auto obj = MakeShared<GameObject>(name);
        obj->SetID(m_NextID++);
        m_Objects.push_back(obj);
        GV_LOG_INFO("Scene '" + m_Name + "' — created object '" + name + "' (id=" + std::to_string(obj->GetID()) + ")");
        return obj.get();
    }

    /// Find an object by name (returns first match or nullptr).
    GameObject* FindByName(const std::string& name) const {
        for (auto& o : m_Objects)
            if (o->GetName() == name) return o.get();
        return nullptr;
    }

    /// Find an object by ID.
    GameObject* FindByID(u32 id) const {
        for (auto& o : m_Objects)
            if (o->GetID() == id) return o.get();
        return nullptr;
    }

    /// Destroy an object by pointer (deferred until end of frame).
    void DestroyGameObject(GameObject* obj) {
        m_PendingDestroy.push_back(obj);
    }

    /// Set the physics world reference for automatic body unregistration.
    void SetPhysicsWorld(PhysicsWorld* pw) { m_Physics = pw; }

    /// Get all objects (read-only).
    const std::vector<Shared<GameObject>>& GetAllObjects() const { return m_Objects; }

    // ── Camera ─────────────────────────────────────────────────────────────
    void    SetActiveCamera(Camera* cam) { m_ActiveCamera = cam; }
    Camera* GetActiveCamera() const      { return m_ActiveCamera; }

    // ── Lifecycle ──────────────────────────────────────────────────────────
    /// Called once before the first frame.
    void Start() {
        for (auto& o : m_Objects) o->Start();
        m_Started = true;
    }

    /// Called every frame.
    void Update(f32 dt) {
        for (auto& o : m_Objects)
            if (o->IsActive()) o->Update(dt);
        FlushDestroyQueue();
    }

    /// Called every frame during the render pass.
    void Render() {
        for (auto& o : m_Objects)
            if (o->IsActive()) o->Render();
    }

    // ── Meta ───────────────────────────────────────────────────────────────
    const std::string& GetName() const { return m_Name; }

    /// Print a summary of all objects to the log (debug helper).
    void DumpHierarchy() const {
        GV_LOG_INFO("=== Scene: " + m_Name + " (" + std::to_string(m_Objects.size()) + " objects) ===");
        for (auto& o : m_Objects) {
            std::string info = "  [" + std::to_string(o->GetID()) + "] " +
                               o->GetName() + "  " + o->GetTransform().ToString();
            GV_LOG_INFO(info);
        }
    }

private:
    void FlushDestroyQueue() {
        for (auto* obj : m_PendingDestroy) {
            // Nullify active camera if it belongs to this object
            if (m_ActiveCamera && m_ActiveCamera->GetOwner() == obj) {
                m_ActiveCamera = nullptr;
            }
            // Unregister physics body before destruction
            if (m_Physics) {
                auto* rb = obj->GetComponent<RigidBody>();
                if (rb) m_Physics->UnregisterBody(rb);
            }
            // Call OnDetach on all components before destruction
            for (auto& comp : obj->GetComponents()) {
                comp->OnDetach();
            }
            m_Objects.erase(
                std::remove_if(m_Objects.begin(), m_Objects.end(),
                    [obj](const Shared<GameObject>& o) { return o.get() == obj; }),
                m_Objects.end());
        }
        m_PendingDestroy.clear();
    }

    std::string                     m_Name;
    std::vector<Shared<GameObject>> m_Objects;
    std::vector<GameObject*>        m_PendingDestroy;
    Camera*                         m_ActiveCamera = nullptr;
    PhysicsWorld*                   m_Physics = nullptr;
    u32                             m_NextID = 1;
    bool                            m_Started = false;
};

// ============================================================================
// Scene Manager — holds all loaded scenes and the current active scene.
// ============================================================================
class SceneManager {
public:
    /// Create and register a new scene; returns a raw pointer.
    Scene* CreateScene(const std::string& name) {
        m_Scenes.push_back(MakeUnique<Scene>(name));
        GV_LOG_INFO("SceneManager — created scene '" + name + "'");
        if (!m_ActiveScene) m_ActiveScene = m_Scenes.back().get();
        return m_Scenes.back().get();
    }

    /// Switch the active scene by name.
    bool SetActiveScene(const std::string& name) {
        for (auto& s : m_Scenes) {
            if (s->GetName() == name) {
                m_ActiveScene = s.get();
                return true;
            }
        }
        GV_LOG_WARN("SceneManager — scene '" + name + "' not found");
        return false;
    }

    Scene* GetActiveScene() const { return m_ActiveScene; }

    const std::vector<Unique<Scene>>& GetAllScenes() const { return m_Scenes; }

    /// Clear all scenes (call during shutdown).
    void Clear() {
        m_ActiveScene = nullptr;
        m_Scenes.clear();
    }

private:
    std::vector<Unique<Scene>> m_Scenes;
    Scene*                     m_ActiveScene = nullptr;
};

} // namespace gv
