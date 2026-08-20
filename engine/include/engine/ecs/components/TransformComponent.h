#pragma once

#include "engine/ecs/Entity.h"

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

namespace engine::ecs {

struct TransformComponent {
    glm::vec3 position = glm::vec3(0.0f);
    glm::quat rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f); // identity (w, x, y, z)
    glm::vec3 scale = glm::vec3(1.0f);

    // Hierarchy (post-Editor-E8, docs/07-unity-parity-analysis.md): kInvalidEntity means
    // "no parent, this entity is a root", same sentinel convention used everywhere else
    // in this engine (see Entity.h). When set, position/rotation/scale above are relative
    // to the parent's own space, not world space -- GetMatrix() below only ever returns
    // the *local* matrix; composing the actual world-space matrix means walking the
    // parent chain, which needs a World to resolve Entity -> TransformComponent lookups
    // and so lives on World itself (World::GetWorldMatrix()), not here.
    Entity parent = kInvalidEntity;

    glm::mat4 GetMatrix() const;
};

} // namespace engine::ecs
