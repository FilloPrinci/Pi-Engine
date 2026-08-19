#pragma once

#include "engine/scene/SceneDocument.h"

namespace engine::ecs {
class World;
}

namespace engine::scene {

// A Prefab is "simply an asset... whose content is a scene fragment" (docs/01 section
// 13.1) -- same JSON schema as Scene.h's document, just parsed once and instantiable into
// a World any number of times afterward, each at its own position.
//
// v1 scope (docs/01 section 13.4, deliberately the same trim the design doc itself
// applies to Prefab overrides): each Instantiate() clones the entity list as plain,
// independent entities. No nested-prefab-reference remapping (docs/01 section 13.2's
// "stable local IDs... remapped to new real ECS IDs") -- nothing in this project's
// scripts/prefabs needs to reference a sibling entity yet, so that whole mechanism is cut
// rather than built and left unexercised. No nested Prefabs (13.3) or override tracking
// (13.4) either.
class Prefab {
public:
    // Parses `path` once; does not touch any World. Safe to call again to reload.
    bool Load(const char* path);

    // Spawns one copy of every entity this prefab describes into `world`, each one's
    // authored position offset by `position` (rotation/scale are used exactly as
    // authored -- v1 doesn't compose a full transform hierarchy, just a position offset,
    // see the class comment). Returns the newly created entities, in the same order as
    // the prefab's own entity list. `createPhysicsBody` is optional, same meaning as
    // Scene.h's LoadScene().
    std::vector<ecs::Entity> Instantiate(ecs::World& world, const glm::vec3& position,
                                         const CreatePhysicsBodyFn& createPhysicsBody = {}) const;

    bool IsLoaded() const { return !m_entities.empty(); }

private:
    std::vector<EntityDesc> m_entities;
};

} // namespace engine::scene
