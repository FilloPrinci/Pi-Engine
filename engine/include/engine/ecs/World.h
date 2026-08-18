#pragma once

#include "engine/ecs/ComponentStorage.h"
#include "engine/ecs/Entity.h"
#include "engine/ecs/components/MeshComponent.h"
#include "engine/ecs/components/TransformComponent.h"

#include <vector>

namespace engine::ecs {

// Minimal ECS world (docs/03 section 7): entity lifetime (creation, destruction with
// generation invalidation, index recycling) plus one ComponentStorage<T> per known
// component type. Growing the set of component types means adding another
// ComponentStorage<T> member and its Add/Get/Remove/Has wrappers here -- not a generic
// registry, deliberately, matching M2's actual two component types.
class World {
public:
    Entity CreateEntity();
    void DestroyEntity(Entity entity);
    bool IsAlive(Entity entity) const;

    TransformComponent& AddTransform(Entity entity, const TransformComponent& value = {});
    void RemoveTransform(Entity entity);
    TransformComponent* GetTransform(Entity entity);
    const TransformComponent* GetTransform(Entity entity) const;
    bool HasTransform(Entity entity) const;

    MeshComponent& AddMesh(Entity entity, const MeshComponent& value = {});
    void RemoveMesh(Entity entity);
    MeshComponent* GetMesh(Entity entity);
    const MeshComponent* GetMesh(Entity entity) const;
    bool HasMesh(Entity entity) const;

    ComponentStorage<TransformComponent>& Transforms() { return m_transforms; }
    ComponentStorage<MeshComponent>& Meshes() { return m_meshes; }
    const ComponentStorage<MeshComponent>& Meshes() const { return m_meshes; }

    // Generic dispatch to the typed Get*() accessors above, for script/ComponentHandle.h
    // (M3): a handle just needs "give me my T back given a World and an Entity" without
    // World turning into a type-erased generic registry. Only ever instantiated for
    // TransformComponent/MeshComponent -- see the explicit specializations in World.cpp;
    // adding a new component type means adding one more specialization there, same as
    // adding one more Add/Get/Remove/Has group above.
    template <typename T>
    T* GetComponent(Entity entity);
    template <typename T>
    const T* GetComponent(Entity entity) const;

private:
    struct EntityRecord {
        std::uint32_t generation = 0; // 0 means "slot never used" -- see CreateEntity().
        bool alive = false;
    };

    std::vector<EntityRecord> m_entityRecords;
    std::vector<std::uint32_t> m_freeIndices;

    ComponentStorage<TransformComponent> m_transforms;
    ComponentStorage<MeshComponent> m_meshes;
};

// Explicit specializations (defined in World.cpp) -- declared here so every translation
// unit that includes this header resolves World::GetComponent<TransformComponent>/
// <MeshComponent> to them instead of the (deliberately bodyless) primary template.
template <>
TransformComponent* World::GetComponent<TransformComponent>(Entity entity);
template <>
const TransformComponent* World::GetComponent<TransformComponent>(Entity entity) const;
template <>
MeshComponent* World::GetComponent<MeshComponent>(Entity entity);
template <>
const MeshComponent* World::GetComponent<MeshComponent>(Entity entity) const;

} // namespace engine::ecs
