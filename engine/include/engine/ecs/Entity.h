#pragma once

#include <cstdint>

namespace engine::ecs {

// Lightweight handle: index into World's internal arrays + a generation counter. The
// generation invalidates handles to destroyed (and possibly index-recycled) entities
// (docs/03 section 7) -- comparing a stored Entity against World::IsAlive() catches
// "this entity was destroyed since I last looked at it" instead of silently operating on
// whatever new entity now occupies that index.
struct Entity {
    std::uint32_t index = 0;
    std::uint32_t generation = 0;

    friend bool operator==(const Entity& lhs, const Entity& rhs) {
        return lhs.index == rhs.index && lhs.generation == rhs.generation;
    }
    friend bool operator!=(const Entity& lhs, const Entity& rhs) { return !(lhs == rhs); }
};

inline constexpr Entity kInvalidEntity{};

} // namespace engine::ecs
