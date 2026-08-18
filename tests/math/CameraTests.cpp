#include "engine/core/Camera.h"

#include <doctest/doctest.h>

using engine::core::Camera;

TEST_CASE("GetViewMatrix places the target directly in front of the camera") {
    Camera camera;
    camera.target = glm::vec3(0.0f);
    camera.distance = 5.0f;
    camera.yaw = 0.0f;
    camera.pitch = 0.0f;

    const glm::mat4 view = camera.GetViewMatrix();
    // A right-handed lookAt puts anything the camera looks at on the -Z axis in view
    // space, at exactly `distance` away when looking straight at `target`.
    const glm::vec4 targetInViewSpace = view * glm::vec4(camera.target, 1.0f);

    CHECK(targetInViewSpace.x == doctest::Approx(0.0f).epsilon(0.001));
    CHECK(targetInViewSpace.y == doctest::Approx(0.0f).epsilon(0.001));
    CHECK(targetInViewSpace.z == doctest::Approx(-5.0f).epsilon(0.001));
}

TEST_CASE("GetProjectionMatrix flips Y for Vulkan's clip space convention") {
    Camera camera;
    const glm::mat4 projection = camera.GetProjectionMatrix(16.0f / 9.0f);

    // glm::perspective alone always produces a positive [1][1] (OpenGL convention);
    // Camera::GetProjectionMatrix must flip it for Vulkan (docs/01 section 3.2 baseline).
    CHECK(projection[1][1] < 0.0f);
}

TEST_CASE("GetProjectionMatrix respects the near/far planes (not degenerate)") {
    Camera camera;
    camera.nearPlane = 0.1f;
    camera.farPlane = 100.0f;
    const glm::mat4 projection = camera.GetProjectionMatrix(1.0f);

    // A degenerate (all-zero or non-invertible) projection would indicate nearPlane==
    // farPlane or a similar setup bug -- glm::determinant is a cheap catch-all check.
    CHECK(glm::determinant(projection) != doctest::Approx(0.0f));
}
