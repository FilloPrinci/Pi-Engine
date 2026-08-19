#pragma once

#include "engine/ecs/components/TransformComponent.h"
#include "engine/script/ComponentHandle.h"
#include "engine/script/Expose.h"
#include "engine/script/ScriptComponent.h"
#include "engine/script/ScriptRegistry.h"

#include <glm/gtc/quaternion.hpp>

// Editor step E8+ -- the Editor's first example of data-driven script attachment
// (engine::scene::EntityDesc::scriptNames, docs/07-unity-parity-analysis.md's "data-driven
// scripting" gap). Deliberately simple and generic -- unlike samples/m3-m5's scripts
// (MoveScript/PlayerScript/TargetScript, each tied to one specific demo scene), this is
// meant to be genuinely reusable by any scene a Pi-Engine project author writes: spins its
// entity around a fixed local axis at a constant rate, no input/physics dependency at all,
// EXPOSE()d speed for the (future) Inspector.
//
// Registered into (and only into) editor_play's own binary -- see play_main.cpp's own
// comment for why "data-driven" here still means "already compiled into whichever
// executable loads the scene", not true hot-loadable scripting.
class RotateScript : public engine::script::ScriptComponent {
public:
    void OnStart() override { transform = GetComponent<engine::ecs::TransformComponent>(); }

    void OnUpdate(float deltaSeconds) override {
        const glm::quat delta =
            glm::angleAxis(glm::radians(static_cast<float>(degreesPerSecond)) * deltaSeconds, axis);
        transform->rotation = glm::normalize(delta * transform->rotation);
    }

    engine::script::ComponentHandle<engine::ecs::TransformComponent> transform;
    EXPOSE(degreesPerSecond, 90.0f);
    glm::vec3 axis = glm::vec3(0.0f, 1.0f, 0.0f); // not EXPOSE()d -- vec3 axis picking is
                                                   // an Inspector UI concern, not needed to
                                                   // prove data-driven attachment works.
};

REGISTER_SCRIPT(RotateScript);
