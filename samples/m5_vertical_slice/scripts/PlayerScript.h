#pragma once

#include "engine/ecs/components/RigidbodyComponent.h"
#include "engine/ecs/components/TransformComponent.h"
#include "engine/physics/Raycast.h"
#include "engine/script/ComponentHandle.h"
#include "engine/script/Expose.h"
#include "engine/script/ScriptComponent.h"
#include "engine/script/ScriptRegistry.h"

#include <cstdio>

// M5's player (docs/01 section 9.6, docs/03 section 10): WASD moves it, Space jumps while
// grounded. Unlike M3's MoveScript, this is a real dynamic rigidbody -- horizontal
// movement goes through RigidbodyComponent::SetHorizontalVelocity (queued, preserves
// whatever Y velocity gravity/the last jump left it with) instead of teleporting
// Transform directly, and the jump is a real AddImpulse. "Grounded" is a short downward
// raycast from the player's center (docs' own PlayerScript example does the same thing).
class PlayerScript : public engine::script::ScriptComponent {
public:
    void OnStart() override {
        transform = GetComponent<engine::ecs::TransformComponent>();
        rigidbody = GetComponent<engine::ecs::RigidbodyComponent>();
    }

    void OnUpdate(float /*deltaSeconds*/) override {
        using engine::platform::Key;

        glm::vec3 moveDirection(0.0f);
        if (GetInput().IsKeyHeld(Key::W) || GetInput().IsKeyHeld(Key::Up)) {
            moveDirection.z -= 1.0f;
        }
        if (GetInput().IsKeyHeld(Key::S) || GetInput().IsKeyHeld(Key::Down)) {
            moveDirection.z += 1.0f;
        }
        if (GetInput().IsKeyHeld(Key::A) || GetInput().IsKeyHeld(Key::Left)) {
            moveDirection.x -= 1.0f;
        }
        if (GetInput().IsKeyHeld(Key::D) || GetInput().IsKeyHeld(Key::Right)) {
            moveDirection.x += 1.0f;
        }
        if (moveDirection != glm::vec3(0.0f)) {
            moveDirection = glm::normalize(moveDirection);
        }
        rigidbody->SetHorizontalVelocity(moveDirection * static_cast<float>(moveSpeed));

        engine::physics::RaycastHit groundHit;
        const bool isGrounded = GetPhysics().Raycast(transform->position, glm::vec3(0.0f, -1.0f, 0.0f),
                                                       static_cast<float>(groundCheckDistance), groundHit);

        if (isGrounded && GetInput().WasPressedThisFrame(Key::Space)) {
            rigidbody->AddImpulse(glm::vec3(0.0f, static_cast<float>(jumpImpulse), 0.0f));
            std::printf("m5_vertical_slice: PlayerScript jumped\n");
        }
    }

    engine::script::ComponentHandle<engine::ecs::TransformComponent> transform;
    engine::script::ComponentHandle<engine::ecs::RigidbodyComponent> rigidbody;

    EXPOSE(moveSpeed, 3.0f);
    EXPOSE(jumpImpulse, 5.0f);
    // Player is a 1x1x1 box (half-extent 0.5); a ray from its center reaches the ground
    // exactly at 0.5 when standing on it -- a little extra margin (0.65) keeps "grounded"
    // true through the small penetration slop the solver leaves at rest (see M4's own
    // measured ~0.48 vs. the theoretical 0.5 resting height).
    EXPOSE(groundCheckDistance, 0.65f);
};

REGISTER_SCRIPT(PlayerScript);
