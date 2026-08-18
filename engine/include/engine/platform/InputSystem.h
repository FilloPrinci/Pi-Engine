#pragma once

#include "engine/platform/InputState.h"

namespace engine::platform {

// Small per-frame query helper over InputState (docs/01 section 11, docs/03 section 8:
// "InputState.h, InputSystem.h -- keyboard only in M3"). Edge detection
// (WasPressedThisFrame/WasReleasedThisFrame) needs the previous frame's snapshot as well
// as the current one, which is why this is a small stateful object fed once per frame via
// Update() rather than free functions over a single InputState.
//
// Not the full Action Mapping layer described in docs/01 section 11 (Input::GetAction,
// remappable bindings, an Input Manager Editor panel) -- that's config-asset/Editor-backed
// and out of scope until the Editor exists (CLAUDE.md section 8). Scripts read physical
// keys directly for now (ScriptComponent::GetInput().IsKeyHeld(Key::W)).
class InputSystem {
public:
    // Called once per frame by whatever polled the display backend (docs/03 section 8's
    // sample main.cpp for now, ahead of a dedicated phase inside Application itself).
    void Update(const InputState& current);

    bool IsKeyHeld(Key key) const;
    bool WasPressedThisFrame(Key key) const;
    bool WasReleasedThisFrame(Key key) const;

private:
    InputState m_current;
    InputState m_previous;
};

} // namespace engine::platform
