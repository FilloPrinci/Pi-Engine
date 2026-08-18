#include "engine/ecs/components/TransformComponent.h"

#include <glm/gtc/matrix_transform.hpp>

namespace engine::ecs {

glm::mat4 TransformComponent::GetMatrix() const {
    const glm::mat4 translation = glm::translate(glm::mat4(1.0f), position);
    const glm::mat4 rotationMat = glm::mat4_cast(rotation);
    const glm::mat4 scaleMat = glm::scale(glm::mat4(1.0f), scale);
    return translation * rotationMat * scaleMat;
}

} // namespace engine::ecs
