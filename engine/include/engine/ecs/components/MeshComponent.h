#pragma once

#include <glm/glm.hpp>

namespace engine::ecs {

// M2 scope (docs/03 section 7): marks an entity as a renderable mesh instance and carries
// just enough data for frustum culling. No per-entity mesh/material *asset* reference yet
// -- there's no Resource Manager to reference into (deferred, docs/01 section 4/12), so
// M2's sample points every MeshComponent at the one shared cube mesh/GPU buffers it loads
// once at startup. A real per-entity mesh reference is Resource Manager work, later.
struct MeshComponent {
    // Local-space bounding sphere, tested against the view frustum after being
    // transformed by the entity's TransformComponent (renderer/FrustumCuller.h).
    glm::vec3 boundsCenter = glm::vec3(0.0f);
    float boundsRadius = 1.0f;

    // Written by FrustumCuller::Cull() each frame; read by the renderer to decide what to
    // draw. Not synchronized with anything else -- safe because Cull() runs to completion
    // (Job System barrier) before any code reads it (docs/01 section 9.4 barrier pattern).
    bool visible = false;
};

} // namespace engine::ecs
