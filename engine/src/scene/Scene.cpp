#include "engine/scene/Scene.h"

namespace engine::scene {

bool LoadScene(const char* path, ecs::World& world, const CreatePhysicsBodyFn& createPhysicsBody) {
    std::vector<EntityDesc> entities;
    if (!ParseSceneDocument(path, entities)) {
        return false;
    }
    SpawnEntities(world, entities, glm::vec3(0.0f), createPhysicsBody);
    return true;
}

bool SaveScene(const char* path, const ecs::World& world) {
    return WriteSceneDocument(path, ExtractEntityDescs(world));
}

} // namespace engine::scene
