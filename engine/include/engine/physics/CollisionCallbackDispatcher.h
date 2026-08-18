#pragma once

#include "engine/ecs/Entity.h"
#include "engine/script/ScriptComponent.h"

#include <Jolt/Jolt.h>
#include <Jolt/Physics/Collision/ContactListener.h>

#include <glm/vec3.hpp>

#include <cstdint>
#include <mutex>
#include <unordered_map>
#include <vector>

namespace engine::physics {

// Registered as the JPH::PhysicsSystem's one ContactListener (docs/01 section 9.6). Jolt
// calls OnContactAdded/Persisted/Removed *during* PhysicsWorld::Step(), on whichever
// solver worker thread happened to process that contact -- running arbitrary script code
// there would be unsafe (mid-solve, no synchronization). So these overrides do nothing but
// record the raw event into a mutex-protected buffer (docs/01 section 9.6 calls this
// "per-thread lock-free buffers"; a single mutex is the same simplification
// jobs/JobSystem.h's own worker queues already made over true lock-free structures --
// contact events are comparatively rare compared to that class's per-frame-hot job queue,
// so it costs nothing here). Dispatch() -- the actual Collision Callback phase, called
// once per frame after the Physics phase's barrier -- drains that buffer single-threaded
// and turns Jolt's Added/Persisted/Removed into Enter/Stay/Exit on the right scripts.
class CollisionCallbackDispatcher final : public JPH::ContactListener {
public:
    void OnContactAdded(const JPH::Body& body1, const JPH::Body& body2,
                        const JPH::ContactManifold& manifold, JPH::ContactSettings& settings) override;
    void OnContactPersisted(const JPH::Body& body1, const JPH::Body& body2,
                            const JPH::ContactManifold& manifold, JPH::ContactSettings& settings) override;
    void OnContactRemoved(const JPH::SubShapeIDPair& subShapePair) override;

    void Dispatch(const std::vector<script::ScriptComponent*>& scripts);

private:
    enum class RawEventType { Added, Persisted, Removed };

    struct RawEvent {
        RawEventType type = RawEventType::Added;
        std::uint64_t bodyPairKey = 0; // sorted (lowBodyRawId << 32) | highBodyRawId
        std::uint64_t userData1 = 0;
        std::uint64_t userData2 = 0;
        glm::vec3 contactPoint = glm::vec3(0.0f);
        glm::vec3 contactNormal = glm::vec3(0.0f);
        float impactSpeed = 0.0f;
        bool isSensor = false;
    };

    // What Dispatch() needs to remember about a still-touching pair to correctly turn a
    // later OnContactRemoved (which, per Jolt's own docs, cannot access the bodies at all)
    // into OnCollisionExit vs OnTriggerExit, and to whom.
    struct ActiveContact {
        ecs::Entity entity1;
        ecs::Entity entity2;
        bool isSensor;
    };

    void PushRawEvent(RawEventType type, const JPH::Body& body1, const JPH::Body& body2,
                      const JPH::ContactManifold& manifold, const JPH::ContactSettings& settings);

    std::mutex m_bufferMutex;
    std::vector<RawEvent> m_pendingEvents;

    // Only ever touched from Dispatch() (single-threaded, main thread) -- never from the
    // OnContact* overrides above, so it needs no synchronization of its own.
    std::unordered_map<std::uint64_t, ActiveContact> m_activeContacts;
};

} // namespace engine::physics
