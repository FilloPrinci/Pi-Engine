#include "engine/core/Camera.h"

#include <glm/gtc/matrix_transform.hpp>

#include <cmath>

namespace engine::core {

glm::mat4 Camera::GetViewMatrix() const {
    const glm::vec3 eye = target + distance * glm::vec3(std::cos(pitch) * std::sin(yaw),
                                                          std::sin(pitch),
                                                          std::cos(pitch) * std::cos(yaw));
    return glm::lookAt(eye, target, glm::vec3(0.0f, 1.0f, 0.0f));
}

glm::mat4 Camera::GetProjectionMatrix(float aspectRatio) const {
    glm::mat4 projection = glm::perspective(fovYRadians, aspectRatio, nearPlane, farPlane);
    projection[1][1] *= -1.0f;
    return projection;
}

} // namespace engine::core
