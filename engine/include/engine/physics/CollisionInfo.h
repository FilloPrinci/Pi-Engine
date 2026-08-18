#pragma once

#include "engine/ecs/Entity.h"

#include <glm/vec3.hpp>

namespace engine::physics {

// Passed to ScriptComponent::OnCollisionEnter/Stay (docs/01 section 9.6). Jolt-free (like
// every physics/*.h that doesn't itself derive from a JPH:: type) so script/ScriptComponent.h
// can include it without pulling Jolt's headers into every script.
struct CollisionInfo {
    ecs::Entity otherEntity;
    glm::vec3 contactPoint = glm::vec3(0.0f);
    glm::vec3 contactNormal = glm::vec3(0.0f); // points away from *this* entity's surface

    // Relative speed of the two bodies along the contact normal at the moment contact was
    // detected -- a deliberately simple stand-in for a true post-solve impulse magnitude
    // (Jolt's EstimateCollisionResponse() computes that properly, at the cost of running a
    // several-iteration solver estimate per contact; out of scope for M5's exit criterion).
    // Good enough for what the design doc's own example uses it for (gauging impact
    // strength, e.g. to scale a sound effect's volume), not dimensionally a real impulse.
    float impactSpeed = 0.0f;
};

} // namespace engine::physics
