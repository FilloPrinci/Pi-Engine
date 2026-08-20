#pragma once

#include "engine/asset/AssetGuid.h"
#include "engine/ecs/components/ColliderComponent.h"

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include <string>
#include <vector>

namespace engine::scene {

// Plain-data description of one entity, parsed from a scene/prefab JSON document
// (SceneDocument.h) and later turned into real ECS components + (optionally) a Jolt body
// by SpawnEntities(). Deliberately JSON-free (no nlohmann::json type appears here) so
// anything that just wants to inspect/construct entity descriptions -- tests, later a
// Prefab -- never needs the JSON library on its include path, the same reasoning
// physics/PhysicsWorld.h stays Jolt-free.
//
// v1 scope (docs/01 section 12.2, 13.4): Transform + Mesh (by GUID) + Collider +
// Rigidbody + Scripts (by name only). `scriptNames` follows the exact same reasoning
// `CreatePhysicsBodyFn` already established for Rigidbody -- SpawnEntities() takes an
// optional `AttachScriptFn` callback (SceneDocument.h) instead of depending on
// engine::script directly, so engine/scene/ still never needs to include a single
// script/ header. What's still *not* here: EXPOSE()d field overrides per instance (this
// only says *which* script types to attach, each starting from its own compiled-in
// defaults) -- `engine::script::ExposedFieldRegistry` (script/Expose.h) only tracks
// {name, type} metadata for a future Inspector to list, it has no generic per-instance
// get/set-by-name mechanism yet for a scene document to target.
struct EntityDesc {
    glm::vec3 position = glm::vec3(0.0f);
    glm::quat rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
    glm::vec3 scale = glm::vec3(1.0f);

    bool hasMesh = false;
    asset::AssetGuid meshGuid;
    float meshBoundsRadius = 1.0f;

    // Material assets (post-Editor-E8, renderer/MaterialData.h) -- a separate "block" from
    // Mesh above, same shape/reasoning as it (own has-flag + GUID), because an entity can
    // have a mesh with no material assigned (falls back to ForwardLitPipeline's debug
    // normal-color visualization, see MeshComponent::materialGuid's own comment) but not
    // the reverse -- a material with no mesh has nothing to tint, so hasMaterial only ever
    // gets set on an entity that also has hasMesh.
    bool hasMaterial = false;
    asset::AssetGuid materialGuid;

    bool hasCollider = false;
    ecs::ColliderComponent::ShapeType colliderShape = ecs::ColliderComponent::ShapeType::Box;
    glm::vec3 colliderHalfExtents = glm::vec3(0.5f);
    float colliderRadius = 0.5f;
    bool colliderIsTrigger = false;

    bool hasRigidbody = false;
    bool rigidbodyIsStatic = true;
    float rigidbodyMass = 1.0f;

    // Script type names (REGISTER_SCRIPT's own name, e.g. "RotateScript") to attach, in
    // order. Whatever loads this scene must have each named script's class actually
    // compiled into its own binary already -- there is no runtime C++ hot-reload in this
    // engine (docs/01 section 6.1), so "data-driven" here means "which of the scripts
    // already linked into this executable to use", not "load arbitrary new code".
    std::vector<std::string> scriptNames;

    // Hierarchy (post-Editor-E8, docs/07-unity-parity-analysis.md): index of this
    // entity's parent *within the same document's `entities` array* (position/rotation/
    // scale above are then relative to that parent, not world space), or -1 for "no
    // parent, this is a root entity". An array index rather than a GUID/name reference on
    // purpose -- entities have no persistent identity of their own within a scene
    // document (docs/01 section 12.2 never gave them one), so "the Nth entity in this
    // file" is the only stable way to refer to another entity from inside the same file,
    // the same reasoning `samples/m7_scene_and_prefab`'s mesh GUID lookup uses a real
    // identifier while an in-document cross-reference just uses position. Resolved to a
    // real ecs::Entity by SpawnEntities() only after every entity in the document has
    // already been created (see its own comment) -- a forward reference (parenting to an
    // entity later in the array) works exactly like a backward one.
    int parentIndex = -1;
};

} // namespace engine::scene
