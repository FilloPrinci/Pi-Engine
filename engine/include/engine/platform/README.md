# platform

- `IDisplayBackend.h` -- done (docs/01 section 5.2).
- `SDL2DisplayBackend.h` + `.cpp` -- done (M0), only active backend through the vertical
  slice (docs/03 section 5).
- `InputState.h` -- done (M3): raw per-frame keyboard snapshot (backend-agnostic `Key`
  enum), filled by `SDL2DisplayBackend::PollEvents()` via `SDL_GetKeyboardState()`. Mouse
  state is a later extension.
- `InputSystem.h` + `.cpp` -- done (M3): held/pressed-this-frame/released-this-frame
  queries over two consecutive `InputState` snapshots. Not the full Action Mapping layer
  (docs/01 section 11) -- that needs the Editor's Input Manager panel, out of scope until
  the Editor exists. Gamepad via `SDL_GameController` is a later extension.
- `DirectDRMDisplayBackend.h` + `.cpp` -- post-vertical-slice, Linux-only (docs/01 section 7.3).
