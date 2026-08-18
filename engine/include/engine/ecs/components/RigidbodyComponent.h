#pragma once

#include <cstdint>

namespace engine::ecs {

// Association between an entity and its Jolt Physics body (M4, docs/03 section 9).
// Deliberately just a packed uint32, not a JPH::BodyID -- keeping Jolt's headers out of
// the ECS's public API (this file is included transitively by every World.h consumer) is
// worth the one explicit JPH::BodyID(rawId)/GetIndexAndSequenceNumber() round-trip that
// happens inside physics/PhysicsWorld.cpp, the only place that ever needs to know Jolt
// exists. Same "packed index" idea as Entity itself.
struct RigidbodyComponent {
    // Matches JPH::BodyID::cInvalidBodyID -- PhysicsWorld::CreateBody() fills this in.
    static constexpr std::uint32_t kInvalidBodyId = 0xffffffffu;

    std::uint32_t bodyId = kInvalidBodyId;

    // Read once, at PhysicsWorld::CreateBody() time (M4 scope -- no runtime mass changes
    // yet). Meaningless for a static body.
    float mass = 1.0f;
};

} // namespace engine::ecs
