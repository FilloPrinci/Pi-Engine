#include "engine/ecs/World.h"

namespace engine::ecs {

Entity World::CreateEntity() {
    if (!m_freeIndices.empty()) {
        const std::uint32_t index = m_freeIndices.back();
        m_freeIndices.pop_back();
        EntityRecord& record = m_entityRecords[index];
        record.alive = true;
        // generation was already >= 1 from this slot's first use; bumping it again is
        // what invalidates any Entity handle still pointing at the old occupant.
        return Entity{index, record.generation};
    }

    const std::uint32_t index = static_cast<std::uint32_t>(m_entityRecords.size());
    m_entityRecords.push_back(EntityRecord{/*generation=*/1, /*alive=*/true});
    return Entity{index, 1};
}

void World::DestroyEntity(Entity entity) {
    if (!IsAlive(entity)) {
        return;
    }
    RemoveTransform(entity);
    RemoveMesh(entity);

    EntityRecord& record = m_entityRecords[entity.index];
    record.alive = false;
    ++record.generation;
    m_freeIndices.push_back(entity.index);
}

bool World::IsAlive(Entity entity) const {
    return entity.index < m_entityRecords.size() && m_entityRecords[entity.index].alive &&
           m_entityRecords[entity.index].generation == entity.generation;
}

TransformComponent& World::AddTransform(Entity entity, const TransformComponent& value) {
    return m_transforms.Add(entity, value);
}
void World::RemoveTransform(Entity entity) { m_transforms.Remove(entity); }
TransformComponent* World::GetTransform(Entity entity) { return m_transforms.Get(entity); }
const TransformComponent* World::GetTransform(Entity entity) const { return m_transforms.Get(entity); }
bool World::HasTransform(Entity entity) const { return m_transforms.Has(entity); }

MeshComponent& World::AddMesh(Entity entity, const MeshComponent& value) {
    return m_meshes.Add(entity, value);
}
void World::RemoveMesh(Entity entity) { m_meshes.Remove(entity); }
MeshComponent* World::GetMesh(Entity entity) { return m_meshes.Get(entity); }
const MeshComponent* World::GetMesh(Entity entity) const { return m_meshes.Get(entity); }
bool World::HasMesh(Entity entity) const { return m_meshes.Has(entity); }

} // namespace engine::ecs
