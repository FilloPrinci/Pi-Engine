#pragma once

#include "engine/ecs/components/TransformComponent.h"
#include "engine/script/ComponentHandle.h"
#include "engine/script/Expose.h"
#include "engine/script/ScriptComponent.h"
#include "engine/script/ScriptRegistry.h"

// M3's first script (docs/03 section 8), written "the way a developer would write it":
// moves its entity on the XZ plane via WASD. Demonstrates the whole Script System in one
// small class -- a cached ComponentHandle<T> instead of a raw pointer (docs/01 section
// 6.2, resolved once in OnStart, safe to keep across frames), GetInput() for keyboard
// state, and an EXPOSE()d tunable for the (future) Inspector.
class MoveScript : public engine::script::ScriptComponent {
public:
    void OnStart() override { transform = GetComponent<engine::ecs::TransformComponent>(); }

    void OnUpdate(float deltaSeconds) override {
        using engine::platform::Key;

        glm::vec3 move(0.0f);
        if (GetInput().IsKeyHeld(Key::W) || GetInput().IsKeyHeld(Key::Up)) {
            move.z -= 1.0f;
        }
        if (GetInput().IsKeyHeld(Key::S) || GetInput().IsKeyHeld(Key::Down)) {
            move.z += 1.0f;
        }
        if (GetInput().IsKeyHeld(Key::A) || GetInput().IsKeyHeld(Key::Left)) {
            move.x -= 1.0f;
        }
        if (GetInput().IsKeyHeld(Key::D) || GetInput().IsKeyHeld(Key::Right)) {
            move.x += 1.0f;
        }

        if (move != glm::vec3(0.0f)) {
            move = glm::normalize(move);
        }
        transform->position += move * static_cast<float>(speed) * deltaSeconds;
    }

    engine::script::ComponentHandle<engine::ecs::TransformComponent> transform;
    EXPOSE(speed, 3.0f);
};

REGISTER_SCRIPT(MoveScript);
