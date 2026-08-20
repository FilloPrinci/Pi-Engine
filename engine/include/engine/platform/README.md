# platform

- `IDisplayBackend.h` -- done (docs/01 section 5.2).
- `SDL2DisplayBackend.h` + `.cpp` -- done (M0), only active backend through the vertical
  slice (docs/03 section 5).
- `InputState.h` -- done (M3, mouse added post-Editor-E8): raw per-frame snapshot
  (backend-agnostic `Key` enum plus `mouseX`/`mouseY`/`mouseLeftHeld`), filled by
  `SDL2DisplayBackend::PollEvents()` via `SDL_GetKeyboardState()`/`SDL_GetMouseState()`.
  `mouseLeftHeld` isn't a pure live-state query like the keyboard fields -- see
  `SDL2DisplayBackend.cpp`'s own comment on why a down event seen during polling latches
  it `true` for that frame even if the button is already back up by the time the final
  state is queried (a full click can complete within one poll). Gamepad is still a later
  extension.
- `InputSystem.h` + `.cpp` -- done (M3, mouse added post-Editor-E8): held/pressed-this-
  frame/released-this-frame queries over two consecutive `InputState` snapshots, now
  including the mouse (`IsMouseLeftHeld`/`WasMouseLeftPressedThisFrame`/
  `WasMouseLeftReleasedThisFrame`/`GetMouseX`/`GetMouseY`) -- first real consumer is the
  Editor's viewport picking + translate gizmo (`editor/main.cpp`), which needs genuine
  press/release edges, not just held state. Not the full Action Mapping layer (docs/01
  section 11) -- that needs the Editor's Input Manager panel, out of scope until the
  Editor exists. Gamepad via `SDL_GameController` is a later extension.
- `DirectDRMDisplayBackend.h` + `.cpp` -- post-vertical-slice, Linux-only (docs/01 section 7.3).
