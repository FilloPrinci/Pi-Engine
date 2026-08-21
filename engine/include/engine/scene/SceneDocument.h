#pragma once

#include "engine/ecs/Entity.h"
#include "engine/scene/EntityDesc.h"

#include <glm/vec3.hpp>

#include <functional>
#include <string>
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
// made since the entities were spawned. One remaining, known, deliberate gap:
// EntityDesc::scriptNames isn't recoverable at all -- attached scripts live outside the
// ECS entirely (AttachScriptFn's caller owns the ScriptComponent instances, see
// SceneDocument.h), so there's no live state to read back even in principle, not even a
// presence check the way GetRigidbody() != nullptr allows for a Rigidbody. `scripts` is
// always omitted here, silently -- consistent with the Editor's Scene View never
// attaching scripts in the first place (E2-E7's read-only view passes no AttachScriptFn
// either), so this only matters for a scene document that already had a "scripts" block
// before the Editor loaded and re-saved it.
// RigidbodyComponent::isStatic used to be this same shape of gap (read once by
// SpawnEntities(), nowhere in the live ECS state to read back from afterward, so an
// entity with a Rigidbody was silently dropped from the saved file) -- fixed post-
// Editor-E8 by retaining isStatic on the component itself (RigidbodyComponent.h's own
// comment), so a Rigidbody now round-trips fully like everything else.
// TransformComponent::parent *is* live ECS state (added alongside hierarchy support,
// post-Editor-E8) -- fully recoverable here, translated back from an ecs::Entity to an
// EntityDesc::parentIndex via this same function's own output order (the Nth entity in
// `world.Transforms().Entities()` becomes the Nth EntityDesc, so a parent's
// position in that same sequence is exactly the index SpawnEntities() will resolve it back
// from -- no separate id map needs to survive the round trip).
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

// Called once per script name in EntityDesc::scriptNames, after the entity's other
// components already exist -- expected to look the name up in
// script::ScriptRegistry::Create(), call ScriptComponent::Attach(), then OnStart(), and
// keep the resulting std::unique_ptr<ScriptComponent> alive somewhere the caller owns
// (SpawnEntities() itself never stores scripts -- nothing in engine::scene runs an
// OnUpdate loop). A callback instead of a script::ScriptRegistry reference, same reasoning
// as CreatePhysicsBodyFn above: engine/scene/ doesn't otherwise know script/ exists at all.
// Pass an empty std::function (the default) for a scene/prefab with no scripted entities,
// or before there's anywhere to run OnUpdate() from -- SpawnEntities() silently skips
// `scriptNames` in that case (unlike a missing CreatePhysicsBodyFn, this isn't warned
// about: a read-only Scene View, Editor step E2, has no scripts running by design, not by
// oversight).
using AttachScriptFn = std::function<void(ecs::World&, ecs::Entity, const std::string& scriptName)>;

// Creates one real ECS entity per EntityDesc in `world`, offset by `positionOffset`
// (Scene::Load uses (0,0,0); Prefab::Instantiate uses wherever the caller wants this copy
// placed). Returns the created entities in the same order as `entities`. Two passes
// internally: every entity is created first (so every EntityDesc::parentIndex, forward or
// backward, resolves to something that already exists), *then* TransformComponent::parent
// is wired up from parentIndex in a second pass -- an out-of-range or self-referencing
// parentIndex is treated as "no parent" (with a stderr warning) rather than corrupting the
// entity or crashing. `positionOffset` is only ever added to a *root* entity's position
// (parentIndex == -1) -- a non-root entity's position is relative to its parent, not
// world space, so it must reach its final world position purely through the parent chain
// (World::GetWorldMatrix()) instead of also being nudged by positionOffset directly,
// which would double-offset it.
std::vector<ecs::Entity> SpawnEntities(ecs::World& world, const std::vector<EntityDesc>& entities,
                                       const glm::vec3& positionOffset,
                                       const CreatePhysicsBodyFn& createPhysicsBody = {},
                                       const AttachScriptFn& attachScript = {});

} // namespace engine::scene
