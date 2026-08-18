#pragma once

#include "engine/ecs/Entity.h"
#include "engine/physics/Raycast.h"

#include <glm/vec3.hpp>

#include <memory>
#include <vector>

namespace engine::ecs {
class World;
}

namespace engine::jobs {
class JobSystem;
}

namespace engine::script {
class ScriptComponent;
}

// Forward declarations only -- this header deliberately never includes a single Jolt
// header. Jolt is a heavy, single-milestone-specific dependency; hiding every JPH:: type
// behind unique_ptr<IncompleteType> members means renderer/, script/, and every sample
// that doesn't touch physics never needs Jolt on its include path or link line to build
// against this header. Only PhysicsWorld.cpp and physics/JoltJobSystemAdapter.h /
// physics/CollisionCallbackDispatcher.h (which have to name JPH:: base classes) see Jolt
// directly.
namespace JPH {
class TempAllocatorImpl;
class PhysicsSystem;
class BroadPhaseLayerInterfaceTable;
class ObjectLayerPairFilterTable;
class ObjectVsBroadPhaseLayerFilterTable;
} // namespace JPH

namespace engine::physics {

class JoltJobSystemAdapter;
class CollisionCallbackDispatcher;

// Thin wrapper around JPH::PhysicsSystem (docs/03 section 9, docs/01 section 9.1: Jolt
// chosen specifically because its lock-free broadphase/island construction plays well
// with our own Job System instead of fighting it).
class PhysicsWorld {
public:
    // Both declared here, defined in the .cpp: a unique_ptr<IncompleteType> member's
    // constructor and destructor both need the pointee complete at the point they're
    // actually instantiated, which is only true where PhysicsWorld.cpp includes Jolt's
    // headers, not from every other TU that just includes this header (defaulting either
    // one inline here would force that instantiation right here, defeating the whole
    // point of hiding Jolt behind this header).
    PhysicsWorld();
    ~PhysicsWorld();

    PhysicsWorld(const PhysicsWorld&) = delete;
    PhysicsWorld& operator=(const PhysicsWorld&) = delete;

    bool Init(jobs::JobSystem& jobSystem);
    void Shutdown();

    // Creates the Jolt body for `entity` from its TransformComponent + ColliderComponent,
    // adds it to the simulation, and writes the resulting body id into its
    // RigidbodyComponent -- `entity` must already have all three (docs/03 section 9).
    // `isStatic` picks EMotionType::Static (immovable, e.g. the ground) vs Dynamic (falls
    // under gravity, responds to forces); Kinematic is a later extension, not needed for
    // M4/M5's exit criteria. `ColliderComponent::isTrigger` controls whether the body is a
    // sensor (M5): no collision response, but still reports contacts as
    // OnTriggerEnter/Exit instead of OnCollisionEnter/Stay/Exit.
    bool CreateBody(ecs::World& world, ecs::Entity entity, bool isStatic);

    // Fixed-timestep simulation step (docs/01 section 9.4) -- called by PhysicsPhase's
    // accumulator, never directly from a variable-framerate Update. Drains every
    // RigidbodyComponent's queued AddImpulse/SetHorizontalVelocity calls and applies them
    // through the real BodyInterface *before* stepping, matching "applied at the start of
    // the next physics step" (docs/01 section 9.6). Blocks until the entire step
    // (including every worker-thread contribution) has completed, which is exactly what
    // makes it safe to read body transforms immediately afterward -- the "synchronization
    // barrier" from CLAUDE.md section 4 is this call returning, not a separate primitive.
    void Step(ecs::World& world, float fixedDeltaSeconds);

    // Post-physics sync (docs/01 section 9.4): writes every RigidbodyComponent's
    // resulting Jolt transform back into its entity's TransformComponent. Call after
    // Step(), before Script/Render read positions for the next frame.
    void SyncTransforms(ecs::World& world);

    // Collision Callback phase (docs/01 section 9.6, docs/03 section 10): dispatches every
    // OnCollisionEnter/Stay/Exit and OnTriggerEnter/Exit accrued since the last call onto
    // the scripts attached to the entities involved -- single-threaded, safe to read/write
    // anything. Call once per frame, after Step()/SyncTransforms(), before the next
    // frame's Script phase reads any of it.
    void DispatchCollisionCallbacks(const std::vector<script::ScriptComponent*>& scripts);

    // Read-only query against the state resulting from the end of the previous physics
    // step (docs/01 section 9.6) -- safe to call directly from a script's OnUpdate, no
    // queuing needed. Returns false (outHit untouched) if nothing was hit within
    // maxDistance.
    bool Raycast(const glm::vec3& origin, const glm::vec3& direction, float maxDistance,
                 RaycastHit& outHit) const;

    void SetGravity(const glm::vec3& gravity);

private:
    std::unique_ptr<JoltJobSystemAdapter> m_jobSystemAdapter;
    std::unique_ptr<CollisionCallbackDispatcher> m_collisionDispatcher;
    std::unique_ptr<JPH::TempAllocatorImpl> m_tempAllocator;
    std::unique_ptr<JPH::BroadPhaseLayerInterfaceTable> m_broadPhaseLayerInterface;
    std::unique_ptr<JPH::ObjectLayerPairFilterTable> m_objectLayerPairFilter;
    std::unique_ptr<JPH::ObjectVsBroadPhaseLayerFilterTable> m_objectVsBroadPhaseLayerFilter;
    std::unique_ptr<JPH::PhysicsSystem> m_physicsSystem;
};

} // namespace engine::physics
