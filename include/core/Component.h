// ============================================================================
// GameVoid Engine — Component Base Class
// ============================================================================
// Every attachable behaviour or data chunk (MeshRenderer, RigidBody, Script,
// Light, …) derives from Component.  Components are stored on GameObjects
// and ticked each frame by the engine.
// ============================================================================
#pragma once

#include "core/Types.h"
#include <string>

namespace gv {

// Forward declaration — defined in GameObject.h
class GameObject;

class Component {
public:
    virtual ~Component() = default;

    // ── Lifecycle callbacks (override in derived classes) ───────────────────
    virtual void OnAttach()  {}           // Called once when added to a GameObject
    virtual void OnDetach()  {}           // Called once when removed
    virtual void OnUpdate(f32 /*dt*/) {}  // Called every frame
    virtual void OnRender()  {}           // Called during the render pass
    virtual void OnStart()   {}           // Called on the first frame

    // ── Identification ─────────────────────────────────────────────────────
    /// Each derived class should return a unique type name for serialisation
    /// and editor display.  Override in subclasses.
    virtual std::string GetTypeName() const { return "Component"; }

    // ── Owner ──────────────────────────────────────────────────────────────
    void        SetOwner(GameObject* owner) { m_Owner = owner; }
    GameObject* GetOwner() const            { return m_Owner; }

    // ── Enabled flag ───────────────────────────────────────────────────────
    bool IsEnabled() const         { return m_Enabled; }
    void SetEnabled(bool enabled)  { m_Enabled = enabled; }

protected:
    GameObject* m_Owner   = nullptr;
    bool        m_Enabled = true;
};

} // namespace gv
