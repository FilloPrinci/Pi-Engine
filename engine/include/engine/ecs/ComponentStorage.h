#pragma once

#include "engine/core/Assert.h"
#include "engine/ecs/Entity.h"

#include <cstdint>
#include <limits>
#include <vector>

namespace engine::ecs {

// Sparse-set component storage: a dense, contiguous array of T (the "array contiguo per
// componente" from docs/01 sections 2.3/4 -- cache-friendly iteration, sized to exactly
// the entities that actually have this component) plus a sparse array mapping
// Entity::index -> dense index for O(1) lookup. Removal is swap-and-pop, so the dense
// array never has holes and stays contiguous after entities are destroyed.
//
// Not a generic type-erased registry (that's more machinery than M2 needs) -- World owns
// one ComponentStorage<T> per known component type explicitly.
template <typename T>
class ComponentStorage {
public:
    T& Add(Entity entity, const T& value) {
        ENGINE_ASSERT(!Has(entity), "component already present for this entity");
        if (entity.index >= m_sparse.size()) {
            m_sparse.resize(entity.index + 1, kInvalidDenseIndex);
        }
        m_sparse[entity.index] = static_cast<std::uint32_t>(m_dense.size());
        m_dense.push_back(value);
        m_denseEntities.push_back(entity);
        return m_dense.back();
    }

    void Remove(Entity entity) {
        if (!Has(entity)) {
            return;
        }
        const std::uint32_t denseIndex = m_sparse[entity.index];
        const std::uint32_t lastIndex = static_cast<std::uint32_t>(m_dense.size() - 1);

        m_dense[denseIndex] = m_dense[lastIndex];
        m_denseEntities[denseIndex] = m_denseEntities[lastIndex];
        m_sparse[m_denseEntities[denseIndex].index] = denseIndex;

        m_dense.pop_back();
        m_denseEntities.pop_back();
        m_sparse[entity.index] = kInvalidDenseIndex;
    }

    bool Has(Entity entity) const {
        return entity.index < m_sparse.size() && m_sparse[entity.index] != kInvalidDenseIndex;
    }

    T* Get(Entity entity) {
        if (!Has(entity)) {
            return nullptr;
        }
        return &m_dense[m_sparse[entity.index]];
    }
    const T* Get(Entity entity) const {
        if (!Has(entity)) {
            return nullptr;
        }
        return &m_dense[m_sparse[entity.index]];
    }

    // Contiguous, index-parallel iteration: Data()[i] belongs to Entities()[i].
    std::vector<T>& Data() { return m_dense; }
    const std::vector<T>& Data() const { return m_dense; }
    const std::vector<Entity>& Entities() const { return m_denseEntities; }

    std::size_t Size() const { return m_dense.size(); }

private:
    static constexpr std::uint32_t kInvalidDenseIndex = std::numeric_limits<std::uint32_t>::max();

    std::vector<std::uint32_t> m_sparse; // indexed by Entity::index
    std::vector<T> m_dense;
    std::vector<Entity> m_denseEntities; // parallel to m_dense
};

} // namespace engine::ecs
