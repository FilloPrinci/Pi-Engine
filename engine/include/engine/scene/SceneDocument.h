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

// Inverse of ParseSceneDocument() -- writes `entities` as a scene/prefab JSON document at
// `path`, using the exact same field names/shapes ParseEntity() (SceneDocument.cpp) reads,
// so a file this writes loads back through ParseSceneDocument()/LoadScene() unchanged
// (Editor step E4, docs/06-editor-roadmap.md: "round-trips through the same format
// LoadScene already reads, no new format invented"). Overwrites `path` if it already
// exists. Never called at runtime by a shipped game -- only the Editor's "Save" action.
bool WriteSceneDocument(const char* path, const std::vector<EntityDesc>& entities);

// Inverse of SpawnEntities() -- reads every entity in `world` that has a Transform (what
// SpawnEntities() always adds, so this is the same "one EntityDesc per spawned entity"
// set) back into a flat EntityDesc list, picking up whatever live edits an Inspector panel
// made since the entities were spawned. One known, deliberate gap: RigidbodyComponent
// doesn't store whether a body is static or dynamic (SpawnEntities() only ever *reads*
// EntityDesc::rigidbodyIsStatic once, to decide which PhysicsWorld::CreateBody() to call,
// then discards it) -- an entity with a Rigidbody is skipped here (with a stderr warning)
// rather than guessing, since writing a wrong isStatic value would silently corrupt the
// saved file instead of just omitting something. Not a concern for the Editor yet: nothing
// in it creates a Rigidbody in the first place (Editor step E2's Scene View has no
// PhysicsWorld), so this only matters for a scene document that already had one before the
// Editor loaded it.
std::vector<EntityDesc> ExtractEntityDescs(const ecs::World& world);

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
