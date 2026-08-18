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
    RemoveRigidbody(entity); // Note: does NOT destroy the corresponding Jolt body -- no
                             // milestone through M4 destroys a physics entity at runtime,
                             // PhysicsWorld gains that responsibility once one does.
    RemoveCollider(entity);

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

RigidbodyComponent& World::AddRigidbody(Entity entity, const RigidbodyComponent& value) {
    return m_rigidbodies.Add(entity, value);
}
void World::RemoveRigidbody(Entity entity) { m_rigidbodies.Remove(entity); }
RigidbodyComponent* World::GetRigidbody(Entity entity) { return m_rigidbodies.Get(entity); }
const RigidbodyComponent* World::GetRigidbody(Entity entity) const { return m_rigidbodies.Get(entity); }
bool World::HasRigidbody(Entity entity) const { return m_rigidbodies.Has(entity); }

ColliderComponent& World::AddCollider(Entity entity, const ColliderComponent& value) {
    return m_colliders.Add(entity, value);
}
void World::RemoveCollider(Entity entity) { m_colliders.Remove(entity); }
ColliderComponent* World::GetCollider(Entity entity) { return m_colliders.Get(entity); }
const ColliderComponent* World::GetCollider(Entity entity) const { return m_colliders.Get(entity); }
bool World::HasCollider(Entity entity) const { return m_colliders.Has(entity); }

template <>
TransformComponent* World::GetComponent<TransformComponent>(Entity entity) {
    return GetTransform(entity);
}
template <>
const TransformComponent* World::GetComponent<TransformComponent>(Entity entity) const {
    return GetTransform(entity);
}
template <>
MeshComponent* World::GetComponent<MeshComponent>(Entity entity) {
    return GetMesh(entity);
}
template <>
const MeshComponent* World::GetComponent<MeshComponent>(Entity entity) const {
    return GetMesh(entity);
}
template <>
RigidbodyComponent* World::GetComponent<RigidbodyComponent>(Entity entity) {
    return GetRigidbody(entity);
}
template <>
const RigidbodyComponent* World::GetComponent<RigidbodyComponent>(Entity entity) const {
    return GetRigidbody(entity);
}
template <>
ColliderComponent* World::GetComponent<ColliderComponent>(Entity entity) {
    return GetCollider(entity);
}
template <>
const ColliderComponent* World::GetComponent<ColliderComponent>(Entity entity) const {
    return GetCollider(entity);
}

} // namespace engine::ecs
