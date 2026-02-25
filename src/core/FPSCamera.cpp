// ============================================================================
// GameVoid Engine — FPS Camera Controller Implementation
// ============================================================================
#include "core/FPSCamera.h"
#include "core/GameObject.h"
#include "core/Window.h"
#include <cmath>
#include <algorithm>

namespace gv {

void FPSCameraController::UpdateFromInput(Window& window, f32 dt) {
    if (!m_Owner) return;

    // Skip mouse-look when cursor is not captured (e.g. user released it with Tab)
    if (!window.IsCursorCaptured()) return;

    Transform& transform = m_Owner->GetTransform();

    // ── Mouse look ─────────────────────────────────────────────────────────
    // GetMouseDelta() returns how many pixels the cursor moved this frame.
    Vec2 mouseDelta = window.GetMouseDelta();

    // Convert pixel movement into yaw/pitch (degrees)
    m_Yaw   -= mouseDelta.x * sensitivity;   // negative so "move right" = look right
    m_Pitch -= mouseDelta.y * sensitivity;    // negative so "move down"  = look down

    // Clamp pitch so the camera can't flip upside-down
    if (m_Pitch > pitchMax) m_Pitch = pitchMax;
    if (m_Pitch < pitchMin) m_Pitch = pitchMin;

    // Apply rotation to the transform (order: yaw around world Y, then pitch around local X)
    constexpr f32 deg2rad = 3.14159265358979f / 180.0f;
    Quaternion qYaw   = Quaternion::FromAxisAngle(Vec3::Up(),    m_Yaw   * deg2rad);
    Quaternion qPitch = Quaternion::FromAxisAngle(Vec3::Right(), m_Pitch * deg2rad);
    transform.rotation = qYaw * qPitch;

    // ── Keyboard movement (WASD) ───────────────────────────────────────────
    // Derive direction vectors from the camera's current rotation.
    // forward = where the camera is looking  (rotated -Z)
    // right   = perpendicular to forward on the horizontal plane
    Vec3 forward = transform.rotation.RotateVec3(Vec3::Forward());   // (0,0,-1) rotated
    Vec3 right   = transform.rotation.RotateVec3(Vec3::Right());     // (1,0,0)  rotated

    // Determine speed (hold Shift to sprint)
    f32 speed = moveSpeed;
    if (window.IsKeyDown(GVKey::LeftShift)) speed *= sprintMulti;

    // Build a movement vector from WASD input
    Vec3 move(0, 0, 0);
    if (window.IsKeyDown(GVKey::W)) move = move + forward;   // forward
    if (window.IsKeyDown(GVKey::S)) move = move - forward;   // backward
    if (window.IsKeyDown(GVKey::D)) move = move + right;     // strafe right
    if (window.IsKeyDown(GVKey::A)) move = move - right;     // strafe left

    // Normalise so diagonal movement isn't faster
    if (move.Length() > 0.001f)
        move = move.Normalized();

    // Apply movement scaled by speed and delta time
    transform.Translate(move * (speed * dt));
}

} // namespace gv
