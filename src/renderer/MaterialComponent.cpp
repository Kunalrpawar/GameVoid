// ============================================================================
// GameVoid Engine — Material Component Implementation
// ============================================================================
#include "renderer/MaterialComponent.h"
#include "core/GameObject.h"
#include "renderer/MeshRenderer.h"

namespace gv {

void MaterialComponent::ApplyToMeshRenderer() {
    if (!GetOwner()) return;
    auto* mr = GetOwner()->GetComponent<MeshRenderer>();
    if (mr) {
        // Push albedo colour into MeshRenderer for immediate viewport
        // feedback.  The PBR pipeline in the Renderer reads the
        // MaterialComponent directly for metallic/roughness/emission/ao
        // and texture maps — those values are uploaded as shader uniforms
        // during RenderScene.  This sync gives a visual preview even when
        // the full PBR path isn't active (e.g. in wireframe or flat mode).
        mr->color = albedo;
    }
}

} // namespace gv
