#include "engine/scene/Prefab.h"

namespace engine::scene {

bool Prefab::Load(const char* path) {
    return ParseSceneDocument(path, m_entities);
}

std::vector<ecs::Entity> Prefab::Instantiate(ecs::World& world, const glm::vec3& position,
                                             const CreatePhysicsBodyFn& createPhysicsBody) const {
    return SpawnEntities(world, m_entities, position, createPhysicsBody);
}

} // namespace engine::scene
