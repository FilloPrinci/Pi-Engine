#pragma once

#include <glm/glm.hpp>

namespace engine::ecs {

// Collision shape (M4, docs/03 section 9). Box/Sphere only for now -- capsule/mesh shapes
// are later extensions (docs/01 section 9.6), not needed for M4's exit criterion (a box
// falling onto a box, docs/02 section 4: "the cube falls under gravity and comes to rest
// on a plane").
struct ColliderComponent {
    enum class ShapeType { Box, Sphere };

    ShapeType shapeType = ShapeType::Box;
    glm::vec3 halfExtents = glm::vec3(0.5f); // Box only
    float radius = 0.5f;                     // Sphere only

    // Stored from the start (matches docs/01 section 9.6's ColliderComponent), but not
    // acted on until OnTriggerEnter/Exit exists (M5) -- PhysicsWorld::CreateBody() doesn't
    // read it yet.
    bool isTrigger = false;
};

} // namespace engine::ecs
