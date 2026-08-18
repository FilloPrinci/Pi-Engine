#pragma once

#include "engine/ecs/World.h"
#include "engine/jobs/JobSystem.h"

#include <glm/glm.hpp>

#include <array>

namespace engine::renderer {

// First system to submit real work to the Job System (docs/03 section 7): tests every
// entity with a MeshComponent's world-space bounding sphere against the 6 view-frustum
// planes, in parallel, writing MeshComponent::visible.
class FrustumCuller {
public:
    // Gribb/Hartmann plane extraction from a combined view-projection matrix. Assumes
    // GLM_FORCE_DEPTH_ZERO_TO_ONE (set project-wide, engine/CMakeLists.txt) -- the near
    // plane formula differs for OpenGL's -1..1 clip-space convention.
    // Each plane is (a, b, c, d) with unit normal (a, b, c); a point p is on the inside
    // half-space when a*p.x + b*p.y + c*p.z + d >= 0.
    static std::array<glm::vec4, 6> ExtractFrustumPlanes(const glm::mat4& viewProjection);

    // `world` and `jobSystem` must outlive the call (synchronous: blocks until every
    // entity has been tested, docs/01 section 9.4 barrier pattern -- safe to read
    // MeshComponent::visible immediately after this returns).
    static void Cull(ecs::World& world, jobs::JobSystem& jobSystem,
                      const std::array<glm::vec4, 6>& frustumPlanes);
};

} // namespace engine::renderer
