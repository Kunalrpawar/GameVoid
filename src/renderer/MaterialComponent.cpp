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
        // Push albedo colour into MeshRenderer for immediate viewport feedback.
        // A full PBR pipeline would upload metallic/roughness/emission as
        // shader uniforms instead; this gives a visual preview in the current
        // flat-colour renderer.
        mr->color = albedo;
    }
}

} // namespace gv
