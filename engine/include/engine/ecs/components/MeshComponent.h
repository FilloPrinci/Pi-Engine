#pragma once

#include "engine/asset/AssetGuid.h"

#include <glm/glm.hpp>

namespace engine::ecs {

// M2 scope (docs/03 section 7): marks an entity as a renderable mesh instance and carries
// just enough data for frustum culling.
struct MeshComponent {
    // Local-space bounding sphere, tested against the view frustum after being
    // transformed by the entity's TransformComponent (renderer/FrustumCuller.h).
    glm::vec3 boundsCenter = glm::vec3(0.0f);
    float boundsRadius = 1.0f;

    // Written by FrustumCuller::Cull() each frame; read by the renderer to decide what to
    // draw. Not synchronized with anything else -- safe because Cull() runs to completion
    // (Job System barrier) before any code reads it (docs/01 section 9.4 barrier pattern).
    bool visible = false;

    // Which cooked mesh this entity renders (M7, docs/01 section 12.3) -- still not a real
    // Resource Manager reference (deferred, docs/01 section 4/12): a scene/prefab-spawned
    // entity's meshGuid is just a lookup key into whatever small per-sample mesh cache
    // resolves GUIDs to already-uploaded GPU buffers (see samples/m7_scene_and_prefab).
    // Defaults to kInvalidAssetGuid -- every M0-M6 sample still points every entity at the
    // one shared cube mesh/GPU buffers it loads once at startup, completely unaffected by
    // this field's existence.
    asset::AssetGuid meshGuid = asset::kInvalidAssetGuid;

    // Which material asset (renderer/MaterialData.h, post-Editor-E8) this entity is tinted
    // with -- kInvalidAssetGuid (the default) means "no material assigned", in which case
    // the renderer falls back to ForwardLitPipeline's original debug normal-color
    // visualization (M1's exit criterion), completely unaffected by this field's existence,
    // exactly like meshGuid's own fallback story above.
    asset::AssetGuid materialGuid = asset::kInvalidAssetGuid;
};

} // namespace engine::ecs
