#pragma once

#include "engine/ecs/Entity.h"
#include "engine/scene/EntityDesc.h"

#include <glm/vec3.hpp>

#include <functional>
#include <vector>

namespace engine::ecs {
class World;
}

namespace engine::scene {

// Parses a scene/prefab JSON document's "entities" array into a flat list of EntityDesc
// (docs/01 section 12.2: "Scenes and Prefabs... saved in a readable text format during
// development (JSON/YAML)... git-friendly, diffable and mergeable"). Shared by Scene.h
// (loads+spawns immediately) and Prefab.h (loads once, spawns on demand, possibly more
// than once) so the two never parse the format differently. nlohmann::json stays an
// implementation detail of the .cpp -- this header, like EntityDesc.h, never exposes it.
bool ParseSceneDocument(const char* path, std::vector<EntityDesc>& outEntities);

// Called once per EntityDesc that has both a "collider" and a "rigidbody" block, after its
// ECS components already exist -- expected to call physics::PhysicsWorld::CreateBody(world,
// entity, isStatic). A callback instead of a physics::PhysicsWorld* on purpose: engine/scene/
// doesn't otherwise know physics/ exists at all (no include, no link-time dependency), the
// same reason IDisplayBackend decouples the engine from a specific windowing backend. Pass
// an empty std::function (the default) for a scene/prefab with no physics entities, or
// before a PhysicsWorld exists -- SpawnEntities() still adds the RigidbodyComponent, just
// with no live Jolt body (see its own comment).
using CreatePhysicsBodyFn = std::function<void(ecs::World&, ecs::Entity, bool isStatic)>;

// Creates one real ECS entity per EntityDesc in `world`, offset by `positionOffset`
// (Scene::Load uses (0,0,0); Prefab::Instantiate uses wherever the caller wants this copy
// placed). Returns the created entities in the same order as `entities`.
std::vector<ecs::Entity> SpawnEntities(ecs::World& world, const std::vector<EntityDesc>& entities,
                                       const glm::vec3& positionOffset,
                                       const CreatePhysicsBodyFn& createPhysicsBody = {});

} // namespace engine::scene
