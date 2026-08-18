#pragma once

#include "engine/core/Assert.h"
#include "engine/ecs/Entity.h"
#include "engine/ecs/World.h"

namespace engine::script {

// Never a permanent raw pointer to an ECS component (CLAUDE.md rule 4): World's storage is
// data-oriented (dense, contiguous arrays per component type, docs/01 sections 2.3/4) and
// can move a component's memory between frames -- ComponentStorage<T>::Remove() is
// swap-and-pop, and Add() can reallocate the backing std::vector<T>. A ComponentHandle<T>
// stores a World* + Entity instead of a T*, and re-resolves the pointer through
// World::GetComponent<T>() on every access, so it stays valid (or safely detects it
// isn't) across any number of intervening rearrangements. operator-> keeps the syntax at
// call sites identical to a raw pointer (docs/01 section 6.2), the same "looks like a
// plain pointer, isn't one" framing already used for Entity's generation check.
template <typename T>
class ComponentHandle {
public:
    ComponentHandle() = default;
    ComponentHandle(ecs::World& world, ecs::Entity entity) : m_world(&world), m_entity(entity) {}

    bool IsValid() const { return Resolve() != nullptr; }

    T* operator->() const {
        T* component = Resolve();
        ENGINE_ASSERT(component != nullptr, "ComponentHandle: component no longer exists");
        return component;
    }
    T& operator*() const { return *operator->(); }

    ecs::Entity GetEntity() const { return m_entity; }

private:
    T* Resolve() const {
        if (m_world == nullptr || !m_world->IsAlive(m_entity)) {
            return nullptr;
        }
        return m_world->GetComponent<T>(m_entity);
    }

    ecs::World* m_world = nullptr;
    ecs::Entity m_entity;
};

} // namespace engine::script
