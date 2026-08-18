#pragma once

#include "engine/core/Assert.h"
#include "engine/ecs/Entity.h"
#include "engine/physics/CollisionInfo.h"
#include "engine/physics/PhysicsWorld.h"
#include "engine/platform/InputSystem.h"
#include "engine/script/ComponentHandle.h"

namespace engine::ecs {
class World;
}

namespace engine::script {

// Base class for gameplay code (docs/01 section 6.2): a Unity-like OnStart/OnUpdate/
// OnDestroy lifecycle, extended in M5 with the collision/trigger callbacks physics needs
// (docs/01 section 9.6) -- see physics/CollisionCallbackDispatcher.h for how these get
// called safely despite Jolt's contact events arriving on solver worker threads.
class ScriptComponent {
public:
    virtual ~ScriptComponent() = default;

    virtual void OnStart() {}
    virtual void OnUpdate(float deltaSeconds) { (void)deltaSeconds; }
    virtual void OnDestroy() {}

    // M5 (docs/01 section 9.6): dispatched single-threaded, in the Collision Callback
    // phase (after the Physics phase's barrier) -- safe to read/write anything here, same
    // as OnUpdate. Default no-ops so a script only overrides what it actually reacts to.
    virtual void OnCollisionEnter(const physics::CollisionInfo& collision) { (void)collision; }
    virtual void OnCollisionStay(const physics::CollisionInfo& collision) { (void)collision; }
    virtual void OnCollisionExit(ecs::Entity other) { (void)other; }
    virtual void OnTriggerEnter(ecs::Entity other) { (void)other; }
    virtual void OnTriggerExit(ecs::Entity other) { (void)other; }

    ecs::Entity GetEntity() const { return m_entity; }

    // Safe access to another component on the same entity (docs/01 section 6.2) -- never
    // a raw T* (CLAUDE.md rule 4), see ComponentHandle<T>'s own comment for why.
    template <typename T>
    ComponentHandle<T> GetComponent() const {
        ENGINE_ASSERT(m_world != nullptr, "ScriptComponent::GetComponent() before Attach()");
        return ComponentHandle<T>(*m_world, m_entity);
    }

    // Wires this script to the entity/world/input/physics it reads every frame. Not part
    // of the OnStart/OnUpdate/OnDestroy lifecycle a script author overrides -- called once
    // by whatever attaches the script (a sample's main.cpp for now; a ScriptComponent
    // storage inside World arrives once more than one sample needs it). `physicsWorld` is
    // optional (nullptr default) -- M3's sample has no physics at all and never calls
    // GetPhysics(), so it can keep using the 3-argument form unchanged.
    void Attach(ecs::World& world, ecs::Entity entity, const platform::InputSystem& input,
                const physics::PhysicsWorld* physicsWorld = nullptr) {
        m_world = &world;
        m_entity = entity;
        m_input = &input;
        m_physicsWorld = physicsWorld;
    }

protected:
    // Input is read once per frame, before the Script phase (docs/01 section 11.4) -- this
    // is that same snapshot, not re-polled per script.
    const platform::InputSystem& GetInput() const {
        ENGINE_ASSERT(m_input != nullptr, "ScriptComponent::GetInput() before Attach()");
        return *m_input;
    }

    // Read-only queries (Raycast/OverlapSphere) against the previous physics step's result
    // (docs/01 section 9.6) -- safe to call directly from OnUpdate, unlike
    // RigidbodyComponent::AddImpulse/SetHorizontalVelocity, which mutate state and so are
    // queued instead.
    const physics::PhysicsWorld& GetPhysics() const {
        ENGINE_ASSERT(m_physicsWorld != nullptr,
                      "ScriptComponent::GetPhysics() before Attach() with a PhysicsWorld");
        return *m_physicsWorld;
    }

private:
    ecs::World* m_world = nullptr;
    ecs::Entity m_entity;
    const platform::InputSystem* m_input = nullptr;
    const physics::PhysicsWorld* m_physicsWorld = nullptr;
};

} // namespace engine::script
