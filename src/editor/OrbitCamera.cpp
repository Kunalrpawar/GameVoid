// ============================================================================
// GameVoid Engine — Editor Orbit Camera Implementation
// ============================================================================
#include "editor/OrbitCamera.h"
#include "core/Transform.h"
#include <cmath>
#include <algorithm>

namespace gv {

static constexpr f32 kDeg2Rad = 3.14159265358979f / 180.0f;

void OrbitCameraController::Orbit(f32 dx, f32 dy) {
    yaw   -= dx * orbitSpeed;
    pitch -= dy * orbitSpeed;

    // Clamp pitch to avoid flipping
    if (pitch > pitchMax) pitch = pitchMax;
    if (pitch < pitchMin) pitch = pitchMin;

    // Keep yaw in [0, 360)
    if (yaw > 360.0f) yaw -= 360.0f;
    if (yaw < 0.0f)   yaw += 360.0f;
}

void OrbitCameraController::Pan(f32 dx, f32 dy) {
    // Compute camera right and up vectors from current yaw/pitch
    f32 yawRad   = yaw   * kDeg2Rad;
    f32 pitchRad = pitch * kDeg2Rad;

    // Camera's right vector (perpendicular to forward on XZ plane)
    Vec3 right(std::cos(yawRad), 0.0f, -std::sin(yawRad));   // was sin, -cos but depends on convention

    // Camera's up vector (perpendicular to forward and right)
    Vec3 forward(
        std::sin(yawRad) * std::cos(pitchRad),
        std::sin(pitchRad),
        std::cos(yawRad) * std::cos(pitchRad)
    );
    Vec3 up = Vec3(
        right.y * forward.z - right.z * forward.y,
        right.z * forward.x - right.x * forward.z,
        right.x * forward.y - right.y * forward.x
    );

    // Scale pan by distance so it feels consistent at any zoom level
    f32 scaledPan = panSpeed * distance * 0.1f;
    focusPoint = focusPoint - right * (dx * scaledPan);
    focusPoint = focusPoint + up    * (dy * scaledPan);
}

void OrbitCameraController::Zoom(f32 delta) {
    // Scale zoom with distance for smooth feel at any zoom level
    f32 scaledZoom = zoomSpeed * (distance * 0.15f);
    if (scaledZoom < 0.2f) scaledZoom = 0.2f;
    distance -= delta * scaledZoom;
    if (distance < minDistance) distance = minDistance;
    if (distance > maxDistance) distance = maxDistance;
}

void OrbitCameraController::FocusOn(const Vec3& target, f32 dist) {
    focusPoint = target;
    if (dist > 0.0f) distance = dist;
}

Vec3 OrbitCameraController::GetEyePosition() const {
    f32 yawRad   = yaw   * kDeg2Rad;
    f32 pitchRad = pitch * kDeg2Rad;

    // Spherical coordinates → Cartesian offset from focus point
    Vec3 offset(
        distance * std::cos(pitchRad) * std::sin(yawRad),
        distance * std::sin(-pitchRad),   // negative because pitch-down = look down
        distance * std::cos(pitchRad) * std::cos(yawRad)
    );

    return focusPoint + offset;
}

void OrbitCameraController::ApplyToTransform(Transform& t) const {
    Vec3 eye = GetEyePosition();
    t.position = eye;

    // Compute rotation quaternion that looks from eye towards focusPoint
    Vec3 dir = (focusPoint - eye);
    f32 len = dir.Length();
    if (len < 0.0001f) return;
    dir = dir * (1.0f / len);  // normalize

    // Yaw = rotation around Y, Pitch = rotation around X
    // We use Euler angles directly since we already have them
    f32 yawRad   = yaw   * kDeg2Rad;
    f32 pitchRad = pitch * kDeg2Rad;

    // Build rotation: first yaw around world Y (+ 180° because default forward is -Z),
    // then pitch around local X
    Quaternion qYaw   = Quaternion::FromAxisAngle(Vec3::Up(),    (yaw + 180.0f) * kDeg2Rad);
    Quaternion qPitch = Quaternion::FromAxisAngle(Vec3::Right(), -pitch * kDeg2Rad);
    t.rotation = qYaw * qPitch;
}

} // namespace gv
