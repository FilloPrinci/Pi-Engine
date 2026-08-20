#pragma once

#include <array>
#include <cstddef>

namespace engine::platform {

// Physical keys the engine cares about (docs/01 section 11): a small, engine-owned enum
// rather than raw SDL scancodes, so InputState stays backend-agnostic -- SDL2DisplayBackend
// fills it from SDL2 today, DirectDRMDisplayBackend will fill the exact same struct from
// libinput later (docs/01 section 5: "both backends feed the same InputState structure").
// Deliberately just enough for M3's exit criterion (WASD movement) plus Space/Escape for
// what M4/M5 need next -- not a full USB HID keycode table, grows as gameplay needs more.
enum class Key {
    W,
    A,
    S,
    D,
    Up,
    Down,
    Left,
    Right,
    Space,
    Escape,
    Count // sentinel: number of entries above, not a real key
};

// Raw per-frame input snapshot, populated once by IDisplayBackend::PollEvents() at the
// start of the frame and read by everything downstream (docs/01 section 11.4: "input is
// read once at the start of the frame, before the Script phase -- every script in the
// same frame sees the exact same input state"). Keyboard-only through M3-E8; mouse
// arrived post-E8 for the Editor's viewport picking + translate gizmo (the first real
// consumer) -- gamepad is still a later extension (docs/03 section 8).
struct InputState {
    bool quitRequested = false;
    std::array<bool, static_cast<std::size_t>(Key::Count)> keysHeld{};

    // Window-space pixel coordinates, origin top-left (SDL's own convention) -- like
    // keysHeld, this is raw *held* state only, no edge detection here (InputSystem below
    // is where press/release edges get computed, same reasoning for both).
    float mouseX = 0.0f;
    float mouseY = 0.0f;
    bool mouseLeftHeld = false;
};

} // namespace engine::platform
