// ============================================================================
// GameVoid Engine — Camera Implementation
// ============================================================================
#include "renderer/Camera.h"
#include "core/GameObject.h"

namespace gv {

Mat4 Camera::GetViewMatrix() const {
    if (!m_Owner) {
        // No owning GameObject — return identity view (camera at origin looking -Z).
        return Mat4::LookAt(Vec3::Zero(), Vec3::Forward(), Vec3::Up());
    }

    const Transform& t = m_Owner->GetTransform();
    // Derive the forward vector from the transform's rotation.
    // For a simple implementation we look from position towards position + forward.
    Vec3 eye    = t.position;
    Vec3 target = eye + Vec3::Forward(); // TODO: rotate forward by quaternion
    return Mat4::LookAt(eye, target, Vec3::Up());
}

} // namespace gv
