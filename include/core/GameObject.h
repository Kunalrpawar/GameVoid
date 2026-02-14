// ============================================================================
// GameVoid Engine — GameObject
// ============================================================================
// A GameObject is a named container with a Transform and a collection of
// Components.  It supports basic parent-child hierarchy.
// ============================================================================
#pragma once

#include "core/Types.h"
#include "core/Transform.h"
#include "core/Component.h"
#include <algorithm>

namespace gv {

class GameObject {
public:
    /// Construct a named game object at the origin.
    explicit GameObject(const std::string& name = "GameObject")
        : m_Name(name) {}

    virtual ~GameObject() = default;

    // ── Identification ─────────────────────────────────────────────────────
    const std::string& GetName() const              { return m_Name; }
    void               SetName(const std::string& n){ m_Name = n; }
    u32                GetID() const                 { return m_ID; }
    void               SetID(u32 id)                 { m_ID = id; }

    // ── Transform access ───────────────────────────────────────────────────
    Transform&       GetTransform()       { return m_Transform; }
    const Transform& GetTransform() const { return m_Transform; }

    // ── Component management ───────────────────────────────────────────────
    /// Attach a component (takes ownership).
    template <typename T, typename... Args>
    T* AddComponent(Args&&... args) {
        static_assert(std::is_base_of<Component, T>::value, "T must derive from Component");
        auto comp = MakeUnique<T>(std::forward<Args>(args)...);
        T* raw = comp.get();
        raw->SetOwner(this);
        raw->OnAttach();
        m_Components.push_back(std::move(comp));
        return raw;
    }

    /// Retrieve the first component of the given type (returns nullptr if absent).
    template <typename T>
    T* GetComponent() const {
        for (auto& c : m_Components) {
            T* casted = dynamic_cast<T*>(c.get());
            if (casted) return casted;
        }
        return nullptr;
    }

    /// Remove the first component of the given type.
    template <typename T>
    bool RemoveComponent() {
        for (auto it = m_Components.begin(); it != m_Components.end(); ++it) {
            if (dynamic_cast<T*>(it->get())) {
                (*it)->OnDetach();
                m_Components.erase(it);
                return true;
            }
        }
        return false;
    }

    /// Get all attached components.
    const std::vector<Unique<Component>>& GetComponents() const { return m_Components; }

    // ── Lifecycle (called by Scene) ────────────────────────────────────────
    virtual void Start() {
        for (auto& c : m_Components) c->OnStart();
    }

    virtual void Update(f32 dt) {
        for (auto& c : m_Components)
            if (c->IsEnabled()) c->OnUpdate(dt);
    }

    virtual void Render() {
        for (auto& c : m_Components)
            if (c->IsEnabled()) c->OnRender();
    }

    // ── Hierarchy (basic parent / children) ────────────────────────────────
    void        SetParent(GameObject* p) { m_Parent = p; }
    GameObject* GetParent() const        { return m_Parent; }

    void AddChild(Shared<GameObject> child) {
        child->SetParent(this);
        child->GetTransform().SetParentTransform(&m_Transform);
        m_Children.push_back(std::move(child));
    }

    /// Remove a child by pointer (does not destroy — returns the shared_ptr).
    Shared<GameObject> RemoveChild(GameObject* child) {
        for (auto it = m_Children.begin(); it != m_Children.end(); ++it) {
            if (it->get() == child) {
                Shared<GameObject> removed = std::move(*it);
                m_Children.erase(it);
                removed->SetParent(nullptr);
                removed->GetTransform().SetParentTransform(nullptr);
                return removed;
            }
        }
        return nullptr;
    }

    const std::vector<Shared<GameObject>>& GetChildren() const { return m_Children; }

    // ── Active flag ────────────────────────────────────────────────────────
    bool IsActive() const        { return m_Active; }
    void SetActive(bool active)  { m_Active = active; }

private:
    std::string                     m_Name;
    u32                             m_ID = 0;
    Transform                       m_Transform;
    std::vector<Unique<Component>>  m_Components;
    GameObject*                     m_Parent = nullptr;
    std::vector<Shared<GameObject>> m_Children;
    bool                            m_Active = true;
};

} // namespace gv
