#include "engine/platform/InputSystem.h"

#include <doctest/doctest.h>

using engine::platform::InputState;
using engine::platform::InputSystem;
using engine::platform::Key;

TEST_CASE("InputSystem reflects the held state from the last Update") {
    InputSystem input;
    CHECK_FALSE(input.IsKeyHeld(Key::W));

    InputState state;
    state.keysHeld[static_cast<std::size_t>(Key::W)] = true;
    input.Update(state);

    CHECK(input.IsKeyHeld(Key::W));
    CHECK_FALSE(input.IsKeyHeld(Key::A));
}

TEST_CASE("WasPressedThisFrame is true only on the frame a key transitions to held") {
    InputSystem input;
    InputState state;

    state.keysHeld[static_cast<std::size_t>(Key::Space)] = true;
    input.Update(state);
    CHECK(input.WasPressedThisFrame(Key::Space));
    CHECK_FALSE(input.WasReleasedThisFrame(Key::Space));

    // Still held on the next frame -- no longer a fresh press.
    input.Update(state);
    CHECK_FALSE(input.WasPressedThisFrame(Key::Space));

    state.keysHeld[static_cast<std::size_t>(Key::Space)] = false;
    input.Update(state);
    CHECK_FALSE(input.WasPressedThisFrame(Key::Space));
    CHECK(input.WasReleasedThisFrame(Key::Space));
}
