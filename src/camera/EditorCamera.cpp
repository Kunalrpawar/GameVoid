// ============================================================================
// GameVoid Engine — Unified Editor Camera Implementation
// ============================================================================
#include "camera/EditorCamera.h"
#include "core/Transform.h"
#include <cmath>
#include <algorithm>

namespace gv {

static constexpr f32 kDeg2Rad = 3.14159265358979f / 180.0f;
static constexpr f32 kRad2Deg = 180.0f / 3.14159265358979f;
static constexpr f32 kPI      = 3.14159265358979f;

// ── Helpers ────────────────────────────────────────────────────────────────

f32 EditorCamera::Clamp(f32 v, f32 lo, f32 hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}

f32 EditorCamera::WrapAngle(f32 deg) {
    while (deg > 360.0f) deg -= 360.0f;
    while (deg <   0.0f) deg += 360.0f;
    return deg;
}

f32 EditorCamera::LerpAngle(f32 a, f32 b, f32 t) {
    // Shortest-path interpolation for angles in degrees
    f32 diff = b - a;
    while (diff > 180.0f)  diff -= 360.0f;
    while (diff < -180.0f) diff += 360.0f;
    return a + diff * t;
}

f32 EditorCamera::Lerp(f32 a, f32 b, f32 t) {
    return a + (b - a) * t;
}

Vec3 EditorCamera::LerpVec3(const Vec3& a, const Vec3& b, f32 t) {
    return Vec3(Lerp(a.x, b.x, t), Lerp(a.y, b.y, t), Lerp(a.z, b.z, t));
}

Vec3 EditorCamera::ComputeEyeFromOrbit(const Vec3& focus, f32 yaw, f32 pitch, f32 dist) const {
    f32 yr = yaw   * kDeg2Rad;
    f32 pr = pitch * kDeg2Rad;
    Vec3 offset(
        dist * std::cos(pr) * std::sin(yr),
        dist * std::sin(-pr),
        dist * std::cos(pr) * std::cos(yr)
    );
    return focus + offset;
}

// ── Orbit mode ─────────────────────────────────────────────────────────────

void EditorCamera::Orbit(f32 dx, f32 dy) {
    m_YawTgt   -= dx * orbitSensitivity;
    m_PitchTgt -= dy * orbitSensitivity;
    m_PitchTgt  = Clamp(m_PitchTgt, pitchMin, pitchMax);
    m_YawTgt    = WrapAngle(m_YawTgt);
    m_Mode = EditorCamMode::Orbit;
}

void EditorCamera::Pan(f32 dx, f32 dy) {
    f32 yr = m_YawCur * kDeg2Rad;
    f32 pr = m_PitchCur * kDeg2Rad;

    // Camera-local right (perpendicular on XZ)
    Vec3 right(std::cos(yr), 0.0f, -std::sin(yr));

    // Camera-local up (perpendicular to forward and right)
    Vec3 forward(
        std::sin(yr) * std::cos(pr),
        std::sin(pr),
        std::cos(yr) * std::cos(pr)
    );
    Vec3 up = right.Cross(forward).Normalized();

    // Scale by distance so panning feels uniform at any zoom
    f32 scale = panSensitivity * m_DistCur * 0.25f;
    if (scale < 0.002f) scale = 0.002f;

    m_FocusTgt = m_FocusTgt - right * (dx * scale) + up * (dy * scale);
    m_Mode = EditorCamMode::Orbit;
}

void EditorCamera::Zoom(f32 scrollDelta) {
    // Exponential zoom for consistent feel
    f32 factor = 1.0f - scrollDelta * 0.18f;
    if (factor < 0.05f) factor = 0.05f;
    m_DistTgt *= factor;
    m_DistTgt = Clamp(m_DistTgt, minDist, maxDist);
    m_Mode = EditorCamMode::Orbit;
}

void EditorCamera::FocusOn(const Vec3& target, f32 dist) {
    m_FocusTgt = target;
    if (dist > 0.0f) m_DistTgt = dist;
    m_Mode = EditorCamMode::Orbit;
}

void EditorCamera::SnapView(f32 yaw, f32 pitch) {
    m_YawTgt   = yaw;
    m_PitchTgt = pitch;
    m_Mode = EditorCamMode::Orbit;
}

// ── Fly mode ───────────────────────────────────────────────────────────────

void EditorCamera::BeginFly() {
    m_Mode = EditorCamMode::Fly;
    // Initialise fly position from current smoothed eye
    m_FlyPos   = GetEyePosition();
    m_FlyYaw   = m_YawCur;
    m_FlyPitch = m_PitchCur;
}

void EditorCamera::FlyUpdate(f32 dx, f32 dy, f32 fwd, f32 right, f32 up, f32 dt, bool sprint) {
    // Mouse look
    m_FlyYaw   -= dx * flySensitivity;
    m_FlyPitch -= dy * flySensitivity;
    m_FlyPitch  = Clamp(m_FlyPitch, pitchMin, pitchMax);
    m_FlyYaw    = WrapAngle(m_FlyYaw);

    // Compute local axes
    f32 yr = m_FlyYaw   * kDeg2Rad;
    f32 pr = m_FlyPitch * kDeg2Rad;

    // Forward direction the camera looks at (into the screen)
    Vec3 forwardDir(
        -std::sin(yr) * std::cos(pr),
        -std::sin(pr),
        -std::cos(yr) * std::cos(pr)
    );
    Vec3 rightDir(std::cos(yr), 0.0f, -std::sin(yr));
    Vec3 upDir(0.0f, 1.0f, 0.0f);   // world up for vertical

    f32 speed = flySpeed * (sprint ? flySprintMul : 1.0f) * dt;

    m_FlyPos = m_FlyPos + forwardDir * (fwd   * speed)
                        + rightDir   * (right * speed)
                        + upDir      * (up    * speed);
}

void EditorCamera::EndFly() {
    // Sync orbit state from fly position so switching back is seamless
    m_YawCur   = m_FlyYaw;
    m_PitchCur = m_FlyPitch;
    m_YawTgt   = m_FlyYaw;
    m_PitchTgt = m_FlyPitch;

    // Place focus ahead of where we are
    Vec3 fwdDir = GetForwardDir();
    m_FocusCur = m_FlyPos + fwdDir * m_DistCur;
    m_FocusTgt = m_FocusCur;

    m_Mode = EditorCamMode::Idle;
}

void EditorCamera::FlyAdjustSpeed(f32 scrollDelta) {
    flySpeed *= (1.0f + scrollDelta * 0.18f);
    flySpeed = Clamp(flySpeed, flyMinSpeed, flyMaxSpeed);
}

// ── Main update (call every frame) ─────────────────────────────────────────

void EditorCamera::Update(f32 dt) {
    if (m_Mode == EditorCamMode::Fly) {
        // In fly mode, current = fly state directly (no orbit interp)
        m_YawCur   = m_FlyYaw;
        m_PitchCur = m_FlyPitch;
        return;
    }

    // Smooth interpolation toward target (exponential ease-out)
    f32 t = 1.0f - std::exp(-smoothFactor * dt);
    if (t > 1.0f) t = 1.0f;

    m_FocusCur = LerpVec3(m_FocusCur, m_FocusTgt, t);
    m_YawCur   = LerpAngle(m_YawCur,   m_YawTgt,   t);
    m_PitchCur = Lerp(m_PitchCur, m_PitchTgt, t);
    m_DistCur  = Lerp(m_DistCur,  m_DistTgt,  t);
    m_DistCur  = Clamp(m_DistCur, minDist, maxDist);
}

// ── Apply to transform ─────────────────────────────────────────────────────

void EditorCamera::ApplyToTransform(Transform& t) const {
    Vec3 eye;
    f32  yaw, pitch;

    if (m_Mode == EditorCamMode::Fly) {
        eye   = m_FlyPos;
        yaw   = m_FlyYaw;
        pitch = m_FlyPitch;
    } else {
        eye   = ComputeEyeFromOrbit(m_FocusCur, m_YawCur, m_PitchCur, m_DistCur);
        yaw   = m_YawCur;
        pitch = m_PitchCur;
    }

    t.position = eye;

    // Build rotation quaternion: Yaw around Y (+180° because default forward = -Z),
    // then Pitch around local X
    Quaternion qYaw   = Quaternion::FromAxisAngle(Vec3::Up(),    (yaw + 180.0f) * kDeg2Rad);
    Quaternion qPitch = Quaternion::FromAxisAngle(Vec3::Right(), -pitch * kDeg2Rad);
    t.rotation = qYaw * qPitch;
}

// ── Read state ─────────────────────────────────────────────────────────────

Vec3 EditorCamera::GetEyePosition() const {
    if (m_Mode == EditorCamMode::Fly) return m_FlyPos;
    return ComputeEyeFromOrbit(m_FocusCur, m_YawCur, m_PitchCur, m_DistCur);
}

Vec3 EditorCamera::GetForwardDir() const {
    f32 yr = (m_Mode == EditorCamMode::Fly ? m_FlyYaw   : m_YawCur)   * kDeg2Rad;
    f32 pr = (m_Mode == EditorCamMode::Fly ? m_FlyPitch : m_PitchCur) * kDeg2Rad;
    return Vec3(
        -std::sin(yr) * std::cos(pr),
        -std::sin(pr),
        -std::cos(yr) * std::cos(pr)
    );
}

Vec3 EditorCamera::GetRightDir() const {
    f32 yr = (m_Mode == EditorCamMode::Fly ? m_FlyYaw : m_YawCur) * kDeg2Rad;
    return Vec3(std::cos(yr), 0.0f, -std::sin(yr));
}

Vec3 EditorCamera::GetUpDir() const {
    return GetRightDir().Cross(GetForwardDir()).Normalized();
}

// ── Direct state set ───────────────────────────────────────────────────────

void EditorCamera::SetOrbitState(const Vec3& focus, f32 yaw, f32 pitch, f32 dist) {
    m_FocusCur = m_FocusTgt = focus;
    m_YawCur   = m_YawTgt   = yaw;
    m_PitchCur = m_PitchTgt = pitch;
    m_DistCur  = m_DistTgt  = dist;
}

} // namespace gv
