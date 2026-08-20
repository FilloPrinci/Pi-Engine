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

// Mouse (post-E8, viewport picking + translate gizmo) -- same held/edge split as the Key
// tests above, mirrored for the one InputState mouse button that exists so far.
TEST_CASE("Mouse position and left-button state reflect the last Update, with press/release "
          "edges") {
    InputSystem input;
    CHECK_FALSE(input.IsMouseLeftHeld());
    CHECK_FALSE(input.WasMouseLeftPressedThisFrame());
    CHECK_FALSE(input.WasMouseLeftReleasedThisFrame());

    InputState state;
    state.mouseX = 42.0f;
    state.mouseY = 17.0f;
    state.mouseLeftHeld = true;
    input.Update(state);
    CHECK(input.GetMouseX() == doctest::Approx(42.0f));
    CHECK(input.GetMouseY() == doctest::Approx(17.0f));
    CHECK(input.IsMouseLeftHeld());
    CHECK(input.WasMouseLeftPressedThisFrame());
    CHECK_FALSE(input.WasMouseLeftReleasedThisFrame());

    // Still held next frame -- no longer a fresh press.
    input.Update(state);
    CHECK_FALSE(input.WasMouseLeftPressedThisFrame());

    state.mouseLeftHeld = false;
    input.Update(state);
    CHECK_FALSE(input.IsMouseLeftHeld());
    CHECK_FALSE(input.WasMouseLeftPressedThisFrame());
    CHECK(input.WasMouseLeftReleasedThisFrame());
}
