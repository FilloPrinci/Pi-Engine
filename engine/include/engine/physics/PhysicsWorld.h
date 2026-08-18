#pragma once

#include "engine/ecs/Entity.h"

#include <glm/vec3.hpp>

#include <memory>

namespace engine::ecs {
class World;
}

namespace engine::jobs {
class JobSystem;
}

// Forward declarations only -- this header deliberately never includes a single Jolt
// header. Jolt is a heavy, single-milestone-specific dependency; hiding every JPH:: type
// behind unique_ptr<IncompleteType> members means renderer/, script/, and every sample
// that doesn't touch physics never needs Jolt on its include path or link line to build
// against this header. Only PhysicsWorld.cpp and physics/JoltJobSystemAdapter.h (which
// has to name JPH::JobSystemWithBarrier as a base class) see Jolt directly.
namespace JPH {
class TempAllocatorImpl;
class PhysicsSystem;
class BroadPhaseLayerInterfaceTable;
class ObjectLayerPairFilterTable;
class ObjectVsBroadPhaseLayerFilterTable;
} // namespace JPH

namespace engine::physics {

class JoltJobSystemAdapter;

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
    // M4's exit criterion.
    bool CreateBody(ecs::World& world, ecs::Entity entity, bool isStatic);

    // Fixed-timestep simulation step (docs/01 section 9.4) -- called by PhysicsPhase's
    // accumulator, never directly from a variable-framerate Update. Blocks until the
    // entire step (including every worker-thread contribution) has completed, which is
    // exactly what makes it safe to read body transforms immediately afterward -- the
    // "synchronization barrier" from CLAUDE.md section 4 is this call returning, not a
    // separate primitive.
    void Step(float fixedDeltaSeconds);

    // Post-physics sync (docs/01 section 9.4): writes every RigidbodyComponent's
    // resulting Jolt transform back into its entity's TransformComponent. Call after
    // Step(), before Script/Render read positions for the next frame.
    void SyncTransforms(ecs::World& world);

    void SetGravity(const glm::vec3& gravity);

private:
    std::unique_ptr<JoltJobSystemAdapter> m_jobSystemAdapter;
    std::unique_ptr<JPH::TempAllocatorImpl> m_tempAllocator;
    std::unique_ptr<JPH::BroadPhaseLayerInterfaceTable> m_broadPhaseLayerInterface;
    std::unique_ptr<JPH::ObjectLayerPairFilterTable> m_objectLayerPairFilter;
    std::unique_ptr<JPH::ObjectVsBroadPhaseLayerFilterTable> m_objectVsBroadPhaseLayerFilter;
    std::unique_ptr<JPH::PhysicsSystem> m_physicsSystem;
};

} // namespace engine::physics
