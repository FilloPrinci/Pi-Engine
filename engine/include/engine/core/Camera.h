#pragma once

#include <glm/glm.hpp>

namespace engine::core {

// Minimal orbit camera (docs/03 section 6). No input handling yet (InputSystem arrives
// in M3) -- samples drive `yaw`/`pitch` directly, e.g. incrementing yaw each frame for an
// automatic orbit demo.
class Camera {
public:
    glm::mat4 GetViewMatrix() const;

    // Vulkan's clip space has Y pointing down (opposite of OpenGL, which GLM's
    // glm::perspective assumes) -- this flips it so `GLM_FORCE_DEPTH_ZERO_TO_ONE`
    // (set project-wide, see engine/CMakeLists.txt) is the only other adjustment needed.
    glm::mat4 GetProjectionMatrix(float aspectRatio) const;

    glm::vec3 target = glm::vec3(0.0f, 0.0f, 0.0f);
    float distance = 3.0f;
    float yaw = 0.0f;          // radians, around the world Y axis
    float pitch = 0.4f;        // radians, elevation above the target's horizontal plane
    float fovYRadians = glm::radians(60.0f);
    float nearPlane = 0.1f;
    float farPlane = 100.0f;
};

} // namespace engine::core
