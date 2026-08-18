#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

namespace engine::ecs {

struct TransformComponent {
    glm::vec3 position = glm::vec3(0.0f);
    glm::quat rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f); // identity (w, x, y, z)
    glm::vec3 scale = glm::vec3(1.0f);

    glm::mat4 GetMatrix() const;
};

} // namespace engine::ecs
