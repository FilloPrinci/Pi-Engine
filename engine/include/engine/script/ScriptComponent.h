#pragma once

#include "engine/core/Assert.h"
#include "engine/ecs/Entity.h"
#include "engine/platform/InputSystem.h"
#include "engine/script/ComponentHandle.h"

namespace engine::ecs {
class World;
}

namespace engine::script {

// Base class for gameplay code (docs/01 section 6.2): a Unity-like OnStart/OnUpdate/
// OnDestroy lifecycle. OnCollisionEnter/Stay/Exit and OnTriggerEnter/Exit are added in M5
// once physics exists (docs/03 section 10) -- M3 only needs OnUpdate.
class ScriptComponent {
public:
    virtual ~ScriptComponent() = default;

    virtual void OnStart() {}
    virtual void OnUpdate(float deltaSeconds) { (void)deltaSeconds; }
    virtual void OnDestroy() {}

    ecs::Entity GetEntity() const { return m_entity; }

    // Safe access to another component on the same entity (docs/01 section 6.2) -- never
    // a raw T* (CLAUDE.md rule 4), see ComponentHandle<T>'s own comment for why.
    template <typename T>
    ComponentHandle<T> GetComponent() const {
        ENGINE_ASSERT(m_world != nullptr, "ScriptComponent::GetComponent() before Attach()");
        return ComponentHandle<T>(*m_world, m_entity);
    }

    // Wires this script to the entity/world/input it reads every frame. Not part of the
    // OnStart/OnUpdate/OnDestroy lifecycle a script author overrides -- called once by
    // whatever attaches the script (samples/m3_hello_script/main.cpp for now; a
    // ScriptComponent storage inside World arrives once more than one sample needs it).
    void Attach(ecs::World& world, ecs::Entity entity, const platform::InputSystem& input) {
        m_world = &world;
        m_entity = entity;
        m_input = &input;
    }

protected:
    // Input is read once per frame, before the Script phase (docs/01 section 11.4) -- this
    // is that same snapshot, not re-polled per script.
    const platform::InputSystem& GetInput() const {
        ENGINE_ASSERT(m_input != nullptr, "ScriptComponent::GetInput() before Attach()");
        return *m_input;
    }

private:
    ecs::World* m_world = nullptr;
    ecs::Entity m_entity;
    const platform::InputSystem* m_input = nullptr;
};

} // namespace engine::script
