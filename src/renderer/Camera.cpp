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
    Vec3 eye = t.position;

    // Derive the forward direction by rotating the default forward vector
    // (0, 0, -1) by the transform's quaternion rotation.
    Vec3 forward = t.rotation.RotateVec3(Vec3::Forward());
    Vec3 up      = t.rotation.RotateVec3(Vec3::Up());
    Vec3 target  = eye + forward;

    return Mat4::LookAt(eye, target, up);
}

} // namespace gv
