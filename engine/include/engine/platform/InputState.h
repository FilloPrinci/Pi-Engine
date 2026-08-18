#pragma once

namespace engine::platform {

// Placeholder — the real key/mouse/gamepad state lands in M3 (docs/03, section 8:
// "InputState.h, InputSystem.h — solo tastiera in M3"). Kept as an (currently empty)
// struct rather than a forward declaration so IDisplayBackend::PollEvents() has a
// concrete, extensible type to write into from M0 onward.
struct InputState {};

} // namespace engine::platform
