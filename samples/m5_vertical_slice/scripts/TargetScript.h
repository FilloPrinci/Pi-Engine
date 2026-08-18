#pragma once

#include "engine/ecs/components/TransformComponent.h"
#include "engine/script/ComponentHandle.h"
#include "engine/script/ScriptComponent.h"
#include "engine/script/ScriptRegistry.h"

#include <cstdio>

// M5's "touch an object and a script reacts" half of the exit criterion (docs/02 section
// 4, docs/01 section 9.6). A static sensor/trigger volume -- ColliderComponent::isTrigger
// set true at spawn time, so it gets OnTriggerEnter/Exit instead of OnCollisionEnter/Stay
// (no physical push-back on whatever touches it). Reacts by shrinking: PhysicsWorld::
// SyncTransforms() only ever writes back position/rotation from Jolt, never scale, so a
// script is free to edit TransformComponent::scale directly with no risk of it being
// overwritten next frame -- unlike position, which a script must go through
// RigidbodyComponent for (see PlayerScript).
class TargetScript : public engine::script::ScriptComponent {
public:
    void OnStart() override { transform = GetComponent<engine::ecs::TransformComponent>(); }

    void OnTriggerEnter(engine::ecs::Entity other) override {
        transform->scale *= 0.5f;
        std::printf("m5_vertical_slice: TargetScript touched by entity %u (gen %u) -- shrinking\n",
                    other.index, other.generation);
    }

    void OnTriggerExit(engine::ecs::Entity other) override {
        std::printf("m5_vertical_slice: entity %u (gen %u) left the target\n", other.index,
                    other.generation);
    }

    engine::script::ComponentHandle<engine::ecs::TransformComponent> transform;
};

REGISTER_SCRIPT(TargetScript);
