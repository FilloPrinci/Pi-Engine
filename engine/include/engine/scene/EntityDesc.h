#pragma once

#include "engine/asset/AssetGuid.h"
#include "engine/ecs/components/ColliderComponent.h"

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

namespace engine::scene {

// Plain-data description of one entity, parsed from a scene/prefab JSON document
// (SceneDocument.h) and later turned into real ECS components + (optionally) a Jolt body
// by SpawnEntities(). Deliberately JSON-free (no nlohmann::json type appears here) so
// anything that just wants to inspect/construct entity descriptions -- tests, later a
// Prefab -- never needs the JSON library on its include path, the same reasoning
// physics/PhysicsWorld.h stays Jolt-free.
//
// v1 scope (docs/01 section 12.2, 13.4): Transform + Mesh (by GUID) + Collider +
// Rigidbody only. No script attachment yet (Scene::Load would need an InputSystem/
// PhysicsWorld* just to Attach() one, and no sample needs a scripted scene entity yet --
// see docs/01 section 13.4's own "v1 scope: one-way synchronization" precedent for
// deliberately trimming Prefab's first version).
struct EntityDesc {
    glm::vec3 position = glm::vec3(0.0f);
    glm::quat rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
    glm::vec3 scale = glm::vec3(1.0f);

    bool hasMesh = false;
    asset::AssetGuid meshGuid;
    float meshBoundsRadius = 1.0f;

    bool hasCollider = false;
    ecs::ColliderComponent::ShapeType colliderShape = ecs::ColliderComponent::ShapeType::Box;
    glm::vec3 colliderHalfExtents = glm::vec3(0.5f);
    float colliderRadius = 0.5f;
    bool colliderIsTrigger = false;

    bool hasRigidbody = false;
    bool rigidbodyIsStatic = true;
    float rigidbodyMass = 1.0f;
};

} // namespace engine::scene
