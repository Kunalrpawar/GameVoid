// ============================================================================
// GameVoid Engine — Physics Script Implementation
// ============================================================================

#include "scripting/physics/PhysicsScript.h"
#include "core/GameObject.h"
#include "core/Window.h"
#include "physics/Physics.h"
#include "editor2d/Editor2DTypes.h"
#include <imgui.h>

namespace gv {

bool PhysicsScript::IsKeyPressed(const std::string& keyName) const {
    // Map string key names to ImGui key codes
    if (keyName == "W") return ImGui::IsKeyDown(ImGuiKey_W);
    if (keyName == "A") return ImGui::IsKeyDown(ImGuiKey_A);
    if (keyName == "S") return ImGui::IsKeyDown(ImGuiKey_S);
    if (keyName == "D") return ImGui::IsKeyDown(ImGuiKey_D);
    if (keyName == "Space") return ImGui::IsKeyDown(ImGuiKey_Space);
    if (keyName == "Q") return ImGui::IsKeyDown(ImGuiKey_Q);
    if (keyName == "E") return ImGui::IsKeyDown(ImGuiKey_E);
    if (keyName == "UpArrow") return ImGui::IsKeyDown(ImGuiKey_UpArrow);
    if (keyName == "DownArrow") return ImGui::IsKeyDown(ImGuiKey_DownArrow);
    if (keyName == "LeftArrow") return ImGui::IsKeyDown(ImGuiKey_LeftArrow);
    if (keyName == "RightArrow") return ImGui::IsKeyDown(ImGuiKey_RightArrow);
    if (keyName == "Shift") return ImGui::IsKeyDown(ImGuiKey_LeftShift);
    if (keyName == "Ctrl") return ImGui::IsKeyDown(ImGuiKey_LeftCtrl);
    return false;
}

Vec3 PhysicsScript::GetVelocity() const {
    auto* obj = GetGameObject();
    if (!obj) return Vec3(0, 0, 0);

    // Try 3D rigid body first
    auto* rb = obj->GetComponent<RigidBody>();
    if (rb) return rb->velocity;

    // Try 2D rigid body
    auto* rb2d = obj->GetComponent<RigidBody2D>();
    if (rb2d) return Vec3(rb2d->velocity.x, rb2d->velocity.y, 0);

    return Vec3(0, 0, 0);
}

Vec3 PhysicsScript::GetAngularVelocity() const {
    auto* obj = GetGameObject();
    if (!obj) return Vec3(0, 0, 0);

    // Try 3D rigid body first
    auto* rb = obj->GetComponent<RigidBody>();
    if (rb) return rb->angularVelocity;

    // Try 2D rigid body
    auto* rb2d = obj->GetComponent<RigidBody2D>();
    if (rb2d) return Vec3(0, 0, rb2d->angularVel);

    return Vec3(0, 0, 0);
}

void PhysicsScript::ApplyForcesToRigidBody() {
    auto* obj = GetGameObject();
    if (!obj) return;

    // Apply to 3D rigid body
    auto* rb = obj->GetComponent<RigidBody>();
    if (rb) {
        if (applyAsImpulse) {
            rb->velocity += forceThisFrame * (1.0f / rb->mass);
        } else {
            rb->velocity += forceThisFrame * (1.0f / rb->mass) * (1.0f / 60.0f);  // Approximate dt
        }
        rb->angularVelocity += momentThisFrame;
        forceThisFrame = Vec3(0, 0, 0);
        momentThisFrame = Vec3(0, 0, 0);
        return;
    }

    // Apply to 2D rigid body
    auto* rb2d = obj->GetComponent<RigidBody2D>();
    if (rb2d) {
        if (applyAsImpulse) {
            rb2d->velocity.x += forceThisFrame.x * (1.0f / rb2d->mass);
            rb2d->velocity.y += forceThisFrame.y * (1.0f / rb2d->mass);
        } else {
            rb2d->velocity.x += forceThisFrame.x * (1.0f / rb2d->mass) * (1.0f / 60.0f);
            rb2d->velocity.y += forceThisFrame.y * (1.0f / rb2d->mass) * (1.0f / 60.0f);
        }
        rb2d->angularVel += momentThisFrame.z;
        forceThisFrame = Vec3(0, 0, 0);
        momentThisFrame = Vec3(0, 0, 0);
        return;
    }
}

} // namespace gv
