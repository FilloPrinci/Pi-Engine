#include "engine/renderer/FrustumCuller.h"

#include <glm/gtc/matrix_access.hpp>

#include <algorithm>

namespace engine::renderer {

std::array<glm::vec4, 6> FrustumCuller::ExtractFrustumPlanes(const glm::mat4& viewProjection) {
    const glm::vec4 row0 = glm::row(viewProjection, 0);
    const glm::vec4 row1 = glm::row(viewProjection, 1);
    const glm::vec4 row2 = glm::row(viewProjection, 2);
    const glm::vec4 row3 = glm::row(viewProjection, 3);

    std::array<glm::vec4, 6> planes = {
        row3 + row0, // left
        row3 - row0, // right
        row3 + row1, // bottom
        row3 - row1, // top
        row2,        // near (z_clip >= 0, GLM_FORCE_DEPTH_ZERO_TO_ONE convention)
        row3 - row2, // far
    };

    for (glm::vec4& plane : planes) {
        const float length = glm::length(glm::vec3(plane));
        if (length > 1e-6f) {
            plane /= length;
        }
    }
    return planes;
}

void FrustumCuller::Cull(ecs::World& world, jobs::JobSystem& jobSystem,
                          const std::array<glm::vec4, 6>& frustumPlanes) {
    std::vector<ecs::MeshComponent>& meshes = world.Meshes().Data();
    const std::vector<ecs::Entity>& entities = world.Meshes().Entities();

    jobSystem.ParallelFor(meshes.size(), [&](std::size_t begin, std::size_t end) {
        for (std::size_t i = begin; i < end; ++i) {
            const ecs::TransformComponent* transform = world.GetTransform(entities[i]);
            if (transform == nullptr) {
                meshes[i].visible = false;
                continue;
            }

            const glm::vec3 worldCenter =
                glm::vec3(transform->GetMatrix() * glm::vec4(meshes[i].boundsCenter, 1.0f));
            const float maxScale =
                std::max({transform->scale.x, transform->scale.y, transform->scale.z});
            const float worldRadius = meshes[i].boundsRadius * maxScale;

            bool visible = true;
            for (const glm::vec4& plane : frustumPlanes) {
                const float distance = plane.x * worldCenter.x + plane.y * worldCenter.y +
                                        plane.z * worldCenter.z + plane.w;
                if (distance < -worldRadius) {
                    visible = false;
                    break;
                }
            }
            meshes[i].visible = visible;
        }
    });
}

} // namespace engine::renderer
