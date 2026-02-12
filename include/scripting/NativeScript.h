// ============================================================================
// GameVoid Engine — Native Script Component (C++ Behaviors)
// ============================================================================
// Godot-style C++ script system. Derive from NativeScriptComponent to write
// custom per-object behaviours entirely in C++.
//
// Example:
//   class Rotator : public NativeScriptComponent {
//       void OnUpdate(f32 dt) override {
//           GetTransform().rotation = Quaternion::FromEuler(
//               Vec3(0, m_Speed * dt, 0)) * GetTransform().rotation;
//       }
//       f32 m_Speed = 90.0f; // degrees per second
//   };
//
// Attach to a GameObject:
//   obj->AddComponent<Rotator>();
//
// The engine ticks OnUpdate(dt) each frame for all enabled components.
// ============================================================================
#pragma once

#include "core/Component.h"
#include "core/GameObject.h"
#include "core/Transform.h"
#include "core/Types.h"
#include <string>
#include <vector>
#include <functional>
#include <unordered_map>

namespace gv {

// Forward
class GameObject;

// ============================================================================
// NativeScriptComponent — base class for C++ behaviors
// ============================================================================
class NativeScriptComponent : public Component {
public:
    NativeScriptComponent() = default;
    ~NativeScriptComponent() override = default;

    std::string GetTypeName() const override { return "NativeScript"; }

    /// Override to return a human-readable behaviour name (shown in Inspector).
    virtual std::string GetScriptName() const { return "NativeScript"; }

    // ── Convenience accessors ──────────────────────────────────────────────
    /// Quick access to the owning object's transform.
    Transform& GetTransform() {
        return GetOwner()->GetTransform();
    }
    const Transform& GetTransform() const {
        return GetOwner()->GetTransform();
    }

    // ── Lifecycle (override in derived) ────────────────────────────────────
    // OnAttach()  — called when added to a GameObject
    // OnStart()   — called on the first frame
    // OnUpdate()  — called every frame
    // OnDetach()  — called when removed
    // IsEnabled() / SetEnabled() — inherited from Component
};

// ============================================================================
// Built-in Behaviors
// ============================================================================

/// Rotates the object around a configurable axis every frame.
class RotatorBehavior : public NativeScriptComponent {
public:
    f32  speed = 90.0f;        // degrees per second
    Vec3 axis  { 0, 1, 0 };   // rotation axis (default: Y-up)

    std::string GetScriptName() const override { return "Rotator"; }

    void OnUpdate(f32 dt) override;
};

/// Bobs the object up and down using a sine wave.
class BobBehavior : public NativeScriptComponent {
public:
    f32 amplitude = 0.5f;
    f32 frequency = 2.0f;

    std::string GetScriptName() const override { return "Bob"; }

    void OnStart() override;
    void OnUpdate(f32 dt) override;

private:
    f32 m_BaseY    = 0.0f;
    f32 m_Elapsed  = 0.0f;
};

/// Follows another object at a fixed offset (basic "follow cam" / companion AI).
class FollowBehavior : public NativeScriptComponent {
public:
    Vec3 offset { 0, 2, 5 };
    f32  smoothSpeed = 5.0f;
    GameObject* target = nullptr;   // set at runtime

    std::string GetScriptName() const override { return "Follow"; }

    void OnUpdate(f32 dt) override;
};

/// Automatically destroys the owning object after a timer expires.
class AutoDestroyBehavior : public NativeScriptComponent {
public:
    f32 lifetime = 5.0f;

    std::string GetScriptName() const override { return "AutoDestroy"; }

    void OnUpdate(f32 dt) override;

private:
    f32 m_Elapsed = 0.0f;
};

// ============================================================================
// Behavior Registry — factory for spawning behaviors by name
// ============================================================================
class BehaviorRegistry {
public:
    using Factory = std::function<NativeScriptComponent*()>;

    static BehaviorRegistry& Instance() {
        static BehaviorRegistry inst;
        return inst;
    }

    /// Register a behavior factory.
    void Register(const std::string& name, Factory factory) {
        m_Factories[name] = std::move(factory);
    }

    /// Get all registered behavior names.
    std::vector<std::string> GetNames() const {
        std::vector<std::string> names;
        names.reserve(m_Factories.size());
        for (auto& kv : m_Factories) names.push_back(kv.first);
        return names;
    }

    /// Create a behavior by name (caller owns the pointer — usually added via AddComponent).
    NativeScriptComponent* Create(const std::string& name) const {
        auto it = m_Factories.find(name);
        if (it != m_Factories.end()) return it->second();
        return nullptr;
    }

    /// Check whether a behavior name exists.
    bool Has(const std::string& name) const {
        return m_Factories.count(name) > 0;
    }

private:
    BehaviorRegistry() = default;
    std::unordered_map<std::string, Factory> m_Factories;
};

/// Call once at engine startup to register built-in behaviors.
void RegisterBuiltinBehaviors();

} // namespace gv
