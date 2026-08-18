#pragma once

#include "engine/ecs/Entity.h"

#include <cstdint>

namespace engine::physics {

// Packs an Entity{index, generation} into the uint64 "user data" every Jolt body carries
// (JPH::BodyCreationSettings::mUserData / JPH::Body::GetUserData()) -- the standard Jolt
// pattern for "which game object does this body belong to" (see ContactListener.h's own
// class comment: "cast Body::GetUserData to a game object"). Shared by PhysicsWorld.cpp
// (packs at body creation) and CollisionCallbackDispatcher.cpp (unpacks in contact
// callbacks) so the two agree on the encoding without either exposing Jolt to the other's
// header.
inline std::uint64_t PackEntityUserData(ecs::Entity entity) {
    return (static_cast<std::uint64_t>(entity.index) << 32) | static_cast<std::uint64_t>(entity.generation);
}

inline ecs::Entity UnpackEntityUserData(std::uint64_t userData) {
    return ecs::Entity{static_cast<std::uint32_t>(userData >> 32),
                        static_cast<std::uint32_t>(userData & 0xffffffffu)};
}

} // namespace engine::physics
