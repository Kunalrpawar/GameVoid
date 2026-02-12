// ============================================================================
// GameVoid Engine — Native Script Component Implementation
// ============================================================================
#include "scripting/NativeScript.h"
#include "core/GameObject.h"
#include "core/Scene.h"
#include <cmath>

namespace gv {

// ── Rotator ────────────────────────────────────────────────────────────────

void RotatorBehavior::OnUpdate(f32 dt) {
    if (!GetOwner()) return;
    constexpr f32 deg2rad = 3.14159265358979f / 180.0f;
    f32 angle = speed * dt * deg2rad;

    // Build incremental rotation quaternion around `axis`
    Vec3 a = axis.Normalized();
    f32 halfAngle = angle * 0.5f;
    f32 s = std::sin(halfAngle);
    Quaternion delta;
    delta.x = a.x * s;
    delta.y = a.y * s;
    delta.z = a.z * s;
    delta.w = std::cos(halfAngle);

    // Multiply: current = delta * current
    Quaternion& cur = GetTransform().rotation;
    Quaternion result;
    result.w = delta.w * cur.w - delta.x * cur.x - delta.y * cur.y - delta.z * cur.z;
    result.x = delta.w * cur.x + delta.x * cur.w + delta.y * cur.z - delta.z * cur.y;
    result.y = delta.w * cur.y - delta.x * cur.z + delta.y * cur.w + delta.z * cur.x;
    result.z = delta.w * cur.z + delta.x * cur.y - delta.y * cur.x + delta.z * cur.w;
    cur = result;
}

// ── Bob ────────────────────────────────────────────────────────────────────

void BobBehavior::OnStart() {
    if (GetOwner()) m_BaseY = GetTransform().position.y;
}

void BobBehavior::OnUpdate(f32 dt) {
    if (!GetOwner()) return;
    m_Elapsed += dt;
    GetTransform().position.y = m_BaseY + amplitude * std::sin(frequency * m_Elapsed * 6.28318f);
}

// ── Follow ─────────────────────────────────────────────────────────────────

void FollowBehavior::OnUpdate(f32 dt) {
    if (!GetOwner() || !target) return;
    Vec3 goal = target->GetTransform().position + offset;
    Vec3& pos = GetTransform().position;
    // Simple lerp
    f32 t = 1.0f - std::exp(-smoothSpeed * dt);
    pos.x += (goal.x - pos.x) * t;
    pos.y += (goal.y - pos.y) * t;
    pos.z += (goal.z - pos.z) * t;
}

// ── AutoDestroy ────────────────────────────────────────────────────────────

void AutoDestroyBehavior::OnUpdate(f32 dt) {
    if (!GetOwner()) return;
    m_Elapsed += dt;
    if (m_Elapsed >= lifetime) {
        GetOwner()->SetActive(false);   // deactivate — Scene can clean up later
    }
}

// ── Registry ───────────────────────────────────────────────────────────────

void RegisterBuiltinBehaviors() {
    auto& reg = BehaviorRegistry::Instance();
    reg.Register("Rotator",     []() -> NativeScriptComponent* { return new RotatorBehavior(); });
    reg.Register("Bob",         []() -> NativeScriptComponent* { return new BobBehavior(); });
    reg.Register("Follow",      []() -> NativeScriptComponent* { return new FollowBehavior(); });
    reg.Register("AutoDestroy", []() -> NativeScriptComponent* { return new AutoDestroyBehavior(); });
}

} // namespace gv
