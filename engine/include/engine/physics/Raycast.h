#pragma once

#include "engine/ecs/Entity.h"

#include <glm/vec3.hpp>

namespace engine::physics {

// Result of PhysicsWorld::Raycast() (docs/01 section 9.6: `Physics::Raycast`). A read-only
// query against the state resulting from the end of the previous physics step -- safe to
// call directly from a script's OnUpdate, no queuing needed (unlike RigidbodyComponent::
// AddImpulse/SetVelocity, which mutate state and so *are* queued).
struct RaycastHit {
    ecs::Entity entity;
    glm::vec3 point = glm::vec3(0.0f);
    glm::vec3 normal = glm::vec3(0.0f);
    float distance = 0.0f;
};

} // namespace engine::physics
