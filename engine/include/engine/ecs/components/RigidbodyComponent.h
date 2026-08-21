#pragma once

#include <cstdint>

#include <glm/vec3.hpp>

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

    // Read once, at PhysicsWorld::CreateBody() time, to decide which of its two body-
    // creation paths to use (M4 scope -- no runtime static/dynamic toggling; Jolt itself
    // supports that, but nothing in this engine has needed it yet). Retained here (post-
    // Editor-E8, "make everything the Editor shows manageable") purely so the value
    // survives round-trips it didn't used to: earlier, this flag was passed straight from
    // scene JSON into SpawnEntities()'s CreatePhysicsBodyFn callback and discarded, so
    // ExtractEntityDescs()/the Editor's own Save button had no live state to read it back
    // from and silently dropped a "rigidbody" block on save (see SceneDocument.cpp's own
    // comment, formerly on this exact gap). Storing it here closes that gap and lets the
    // Editor's Inspector show/edit it directly.
    bool isStatic = true;

    // Thin wrapper over Jolt's BodyInterface (docs/01 section 9.6), but still plain data --
    // never a JPH:: type in this header (see the comment above). Calls from a script's
    // OnUpdate (pre-physics phase) queue a request here; PhysicsWorld::Step() drains and
    // clears these at the start of the next physics step and applies them through the real
    // BodyInterface, then the queue is empty again until a script sets it next frame --
    // "safe by construction, no lock the developer has to manage" (docs/01 section 9.6).
    glm::vec3 pendingImpulse = glm::vec3(0.0f);
    bool hasPendingImpulse = false;

    // Horizontal-only (X/Z) target velocity for a walking/driving character -- PhysicsWorld
    // preserves the body's own current Y velocity (gravity, jumps, falling) when applying
    // this, only overwriting the plane a script actually controls. Y is not settable this
    // way on purpose: a full SetVelocity(vec3) that also stomps Y would fight gravity and
    // AddImpulse-driven jumps every step. GetVelocity() isn't needed yet (no M5 script
    // reads it back) so it's not added prematurely.
    glm::vec3 pendingHorizontalVelocity = glm::vec3(0.0f);
    bool hasPendingHorizontalVelocity = false;

    void AddImpulse(const glm::vec3& impulse) {
        pendingImpulse += impulse;
        hasPendingImpulse = true;
    }

    void SetHorizontalVelocity(const glm::vec3& velocity) {
        pendingHorizontalVelocity = velocity;
        hasPendingHorizontalVelocity = true;
    }
};

} // namespace engine::ecs
