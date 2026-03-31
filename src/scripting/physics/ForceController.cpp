// ============================================================================
// GameVoid Engine — Force Controller Implementation
// ============================================================================

#include "scripting/physics/ForceController.h"
#include "core/GameObject.h"
#include "physics/Physics.h" // for RigidBody
#include "editor2d/Editor2DTypes.h" // for RigidBody2D
#include <imgui.h>
#include <cmath>

namespace gv {


void ForceController::ApplyGravityOverride() {
    if (m_DisableGravity || !m_UseCustomGravity) return;

    auto* obj = GetOwner();
    if (!obj) return;

    auto* rb = obj->GetComponent<RigidBody>();
    if (rb) {
        rb->velocity.y += m_CustomGravity * (1.0f / 60.0f);
        return;
    }

    auto* rb2d = obj->GetComponent<RigidBody2D>();
    if (rb2d) {
        rb2d->velocity.y += m_CustomGravity * (1.0f / 60.0f);
        return;
    }
}

f32 ForceController::GetCurrentSpeed() const {
    auto* obj = GetOwner();
    if (!obj) return 0.0f;

    auto* rb = obj->GetComponent<RigidBody>();
    if (rb) return rb->velocity.Length();

    auto* rb2d = obj->GetComponent<RigidBody2D>();
    if (rb2d) return rb2d->velocity.Length();

    return 0.0f;
}

f32 ForceController::GetCurrentAngularSpeed() const {
    auto* obj = GetOwner();
    if (!obj) return 0.0f;

    auto* rb = obj->GetComponent<RigidBody>();
    if (rb) return rb->angularVelocity.Length();

    auto* rb2d = obj->GetComponent<RigidBody2D>();
    if (rb2d) return std::fabs(rb2d->angularVel);

    return 0.0f;
}



// ============================================================================
// All ForceController methods below are inside namespace gv
// ============================================================================

namespace gv {

// ...existing method implementations for ForceController...

} // namespace gv

void ForceController::ProcessInputBindings() {
    // Process force bindings
    for (auto& binding : m_KeyForceBindings) {
        if (ImGui::IsKeyDown(ImGuiKey_UpArrow) && binding.first == "UpArrow") {
            ApplyForceInDirection(binding.second.first, binding.second.second);
        }
        if (ImGui::IsKeyDown(ImGuiKey_DownArrow) && binding.first == "DownArrow") {
            ApplyForceInDirection(binding.second.first, binding.second.second);
        }
        if (ImGui::IsKeyDown(ImGuiKey_LeftArrow) && binding.first == "LeftArrow") {
            ApplyForceInDirection(binding.second.first, binding.second.second);
        }
        if (ImGui::IsKeyDown(ImGuiKey_RightArrow) && binding.first == "RightArrow") {
            ApplyForceInDirection(binding.second.first, binding.second.second);
        }
        if (ImGui::IsKeyDown(ImGuiKey_W) && binding.first == "W") {
            ApplyForceInDirection(binding.second.first, binding.second.second);
        }
        if (ImGui::IsKeyDown(ImGuiKey_A) && binding.first == "A") {
            ApplyForceInDirection(binding.second.first, binding.second.second);
        }
        if (ImGui::IsKeyDown(ImGuiKey_S) && binding.first == "S") {
            ApplyForceInDirection(binding.second.first, binding.second.second);
        }
        if (ImGui::IsKeyDown(ImGuiKey_D) && binding.first == "D") {
            ApplyForceInDirection(binding.second.first, binding.second.second);
        }
        if (ImGui::IsKeyDown(ImGuiKey_Space) && binding.first == "Space") {
            ApplyForceInDirection(binding.second.first, binding.second.second);
        }
    }

    // Process moment bindings
    for (auto& binding : m_KeyMomentBindings) {
        if (ImGui::IsKeyDown(ImGuiKey_Q) && binding.first == "Q") {
            ApplyMomentInDirection(binding.second.first, binding.second.second);
        }
        if (ImGui::IsKeyDown(ImGuiKey_E) && binding.first == "E") {
            ApplyMomentInDirection(binding.second.first, binding.second.second);
        }
    }
}

void ForceController::ApplyDamping(f32 dt) {
    auto* obj = GetGameObject();
    if (!obj) return;

    auto* rb = obj->GetComponent<RigidBody>();
    if (rb) {
        rb->velocity *= (1.0f - m_LinearDamping * dt);
        rb->angularVelocity *= (1.0f - m_AngularDamping * dt);
        return;
    }

    auto* rb2d = obj->GetComponent<RigidBody2D>();
    if (rb2d) {
        rb2d->velocity *= (1.0f - m_LinearDamping * dt);
        rb2d->angularVel *= (1.0f - m_AngularDamping * dt);
        return;
    }
}

void ForceController::ClampVelocities() {
    auto* obj = GetGameObject();
    if (!obj) return;

    auto* rb = obj->GetComponent<RigidBody>();
    if (rb) {
        f32 speed = rb->velocity.Length();
        if (speed > m_MaxVelocity) {
            rb->velocity = (rb->velocity / speed) * m_MaxVelocity;
        }
        f32 angSpeed = rb->angularVelocity.Length();
        if (angSpeed > m_MaxAngularVelocity) {
            rb->angularVelocity = (rb->angularVelocity / angSpeed) * m_MaxAngularVelocity;
        }
        return;
    }

    auto* rb2d = obj->GetComponent<RigidBody2D>();
    if (rb2d) {
        f32 speed = rb2d->velocity.Length();
        if (speed > m_MaxVelocity) {
            rb2d->velocity = (rb2d->velocity / speed) * m_MaxVelocity;
        }
        if (std::fabs(rb2d->angularVel) > m_MaxAngularVelocity) {
            rb2d->angularVel = (rb2d->angularVel > 0 ? 1.0f : -1.0f) * m_MaxAngularVelocity;
        }
        return;
    }
}

void ForceController::ApplyGravityOverride() {
    if (m_DisableGravity || !m_UseCustomGravity) return;

    auto* obj = GetGameObject();
    if (!obj) return;

    auto* rb = obj->GetComponent<RigidBody>();
    if (rb) {
        rb->velocity.y += m_CustomGravity * (1.0f / 60.0f);
        return;
    }

    auto* rb2d = obj->GetComponent<RigidBody2D>();
    if (rb2d) {
        rb2d->velocity.y += m_CustomGravity * (1.0f / 60.0f);
        return;
    }
}

f32 ForceController::GetCurrentSpeed() const {
    auto* obj = GetGameObject();
    if (!obj) return 0.0f;

    auto* rb = obj->GetComponent<RigidBody>();
    if (rb) return rb->velocity.Length();

    auto* rb2d = obj->GetComponent<RigidBody2D>();
    if (rb2d) return rb2d->velocity.Length();

    return 0.0f;
}

f32 ForceController::GetCurrentAngularSpeed() const {
    auto* obj = GetGameObject();
    if (!obj) return 0.0f;

    auto* rb = obj->GetComponent<RigidBody>();
    if (rb) return rb->angularVelocity.Length();

    auto* rb2d = obj->GetComponent<RigidBody2D>();
    if (rb2d) return std::fabs(rb2d->angularVel);

    return 0.0f;
}

} // namespace gv
