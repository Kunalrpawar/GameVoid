

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

void ForceController::OnCreate() {}
void ForceController::OnDestroy() { m_KeyForceBindings.clear(); m_KeyMomentBindings.clear(); }
void ForceController::Update(f32 dt) { ProcessInputBindings(); ApplyGravityOverride(); ApplyDamping(dt); ClampVelocities(); }

void ForceController::ApplyForceInDirection(ForceDirection dir, f32 magnitude) {
    ApplyForceVector(GetForceDirectionVector(dir) * magnitude);
}
void ForceController::ApplyMomentInDirection(MomentDirection dir, f32 magnitude) {
    ApplyMomentVector(GetMomentDirectionVector(dir) * magnitude);
}
void ForceController::ApplyForceVector(const Vec3& force) {
    auto* obj = GetOwner();
    if (!obj) return;
    auto* rb = obj->GetComponent<RigidBody>();
    if (rb) {
        if (immediateMode) rb->velocity += force / rb->mass;
        else { m_CurrentForce += force; rb->velocity += (force / rb->mass) * (1.0f / 60.0f); }
        return;
    }
    auto* rb2d = obj->GetComponent<RigidBody2D>();
    if (rb2d) {
        if (immediateMode) { rb2d->velocity.x += force.x / rb2d->mass; rb2d->velocity.y += force.y / rb2d->mass; }
        else { m_CurrentForce += force; rb2d->velocity.x += (force.x / rb2d->mass) * (1.0f / 60.0f); rb2d->velocity.y += (force.y / rb2d->mass) * (1.0f / 60.0f); }
        return;
    }
}
void ForceController::ApplyMomentVector(const Vec3& moment) {
    auto* obj = GetOwner();
    if (!obj) return;
    auto* rb = obj->GetComponent<RigidBody>();
    if (rb) { rb->angularVelocity += moment; m_CurrentMoment += moment; return; }
    auto* rb2d = obj->GetComponent<RigidBody2D>();
    if (rb2d) { rb2d->angularVel += moment.z; m_CurrentMoment += moment; return; }
}
void ForceController::BindKeyToForce(const std::string& keyName, ForceDirection dir, f32 magnitude) { m_KeyForceBindings.push_back({keyName, {dir, magnitude}}); }
void ForceController::BindKeyToMoment(const std::string& keyName, MomentDirection dir, f32 magnitude) { m_KeyMomentBindings.push_back({keyName, {dir, magnitude}}); }

Vec3 ForceController::GetForceDirectionVector(ForceDirection dir) const {
    switch (dir) {
    case ForceDirection::Forward:  return applyToLocalSpace ? Vec3(0, 0, 1) : Vec3(0, 0, 1);
    case ForceDirection::Backward: return applyToLocalSpace ? Vec3(0, 0, -1) : Vec3(0, 0, -1);
    case ForceDirection::Left:     return applyToLocalSpace ? Vec3(-1, 0, 0) : Vec3(-1, 0, 0);
    case ForceDirection::Right:    return applyToLocalSpace ? Vec3(1, 0, 0) : Vec3(1, 0, 0);
    case ForceDirection::Up:       return Vec3(0, 1, 0);
    case ForceDirection::Down:     return Vec3(0, -1, 0);
    case ForceDirection::Custom:   return Vec3(0, 0, 0);
    }
    return Vec3(0, 0, 0);
}
Vec3 ForceController::GetMomentDirectionVector(MomentDirection dir) const {
    switch (dir) {
    case MomentDirection::Pitch:  return Vec3(1, 0, 0);
    case MomentDirection::Yaw:    return Vec3(0, 1, 0);
    case MomentDirection::Roll:   return Vec3(0, 0, 1);
    case MomentDirection::Custom: return Vec3(0, 0, 0);
    }
    return Vec3(0, 0, 0);
}

void ForceController::ProcessInputBindings() {
    for (auto& binding : m_KeyForceBindings) {
        if (ImGui::IsKeyDown(ImGuiKey_UpArrow) && binding.first == "UpArrow") ApplyForceInDirection(binding.second.first, binding.second.second);
        if (ImGui::IsKeyDown(ImGuiKey_DownArrow) && binding.first == "DownArrow") ApplyForceInDirection(binding.second.first, binding.second.second);
        if (ImGui::IsKeyDown(ImGuiKey_LeftArrow) && binding.first == "LeftArrow") ApplyForceInDirection(binding.second.first, binding.second.second);
        if (ImGui::IsKeyDown(ImGuiKey_RightArrow) && binding.first == "RightArrow") ApplyForceInDirection(binding.second.first, binding.second.second);
        if (ImGui::IsKeyDown(ImGuiKey_W) && binding.first == "W") ApplyForceInDirection(binding.second.first, binding.second.second);
        if (ImGui::IsKeyDown(ImGuiKey_A) && binding.first == "A") ApplyForceInDirection(binding.second.first, binding.second.second);
        if (ImGui::IsKeyDown(ImGuiKey_S) && binding.first == "S") ApplyForceInDirection(binding.second.first, binding.second.second);
        if (ImGui::IsKeyDown(ImGuiKey_D) && binding.first == "D") ApplyForceInDirection(binding.second.first, binding.second.second);
        if (ImGui::IsKeyDown(ImGuiKey_Space) && binding.first == "Space") ApplyForceInDirection(binding.second.first, binding.second.second);
    }
    for (auto& binding : m_KeyMomentBindings) {
        if (ImGui::IsKeyDown(ImGuiKey_Q) && binding.first == "Q") ApplyMomentInDirection(binding.second.first, binding.second.second);
        if (ImGui::IsKeyDown(ImGuiKey_E) && binding.first == "E") ApplyMomentInDirection(binding.second.first, binding.second.second);
    }
}

void ForceController::ApplyDamping(f32 dt) {
    auto* obj = GetOwner();
    if (!obj) return;
    auto* rb = obj->GetComponent<RigidBody>();
    if (rb) { rb->velocity *= (1.0f - m_LinearDamping * dt); rb->angularVelocity *= (1.0f - m_AngularDamping * dt); return; }
    auto* rb2d = obj->GetComponent<RigidBody2D>();
    if (rb2d) { rb2d->velocity *= (1.0f - m_LinearDamping * dt); rb2d->angularVel *= (1.0f - m_AngularDamping * dt); return; }
}

void ForceController::ClampVelocities() {
    auto* obj = GetOwner();
    if (!obj) return;
    auto* rb = obj->GetComponent<RigidBody>();
    if (rb) {
        f32 speed = rb->velocity.Length();
        if (speed > m_MaxVelocity) rb->velocity = (rb->velocity / speed) * m_MaxVelocity;
        f32 angSpeed = rb->angularVelocity.Length();
        if (angSpeed > m_MaxAngularVelocity) rb->angularVelocity = (rb->angularVelocity / angSpeed) * m_MaxAngularVelocity;
        return;
    }
    auto* rb2d = obj->GetComponent<RigidBody2D>();
    if (rb2d) {
        f32 speed = rb2d->velocity.Length();
        if (speed > m_MaxVelocity) rb2d->velocity = (rb2d->velocity / speed) * m_MaxVelocity;
        if (std::fabs(rb2d->angularVel) > m_MaxAngularVelocity) rb2d->angularVel = (rb2d->angularVel > 0 ? 1.0f : -1.0f) * m_MaxAngularVelocity;
        return;
    }
}

void ForceController::ApplyGravityOverride() {
    if (m_DisableGravity || !m_UseCustomGravity) return;
    auto* obj = GetOwner();
    if (!obj) return;
    auto* rb = obj->GetComponent<RigidBody>();
    if (rb) { rb->velocity.y += m_CustomGravity * (1.0f / 60.0f); return; }
    auto* rb2d = obj->GetComponent<RigidBody2D>();
    if (rb2d) { rb2d->velocity.y += m_CustomGravity * (1.0f / 60.0f); return; }
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

} // namespace gv

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
