# platform

- `IDisplayBackend.h` -- done (docs/01 section 5.2).
- `SDL2DisplayBackend.h` + `.cpp` -- done (M0), only active backend through the vertical
  slice (docs/03 section 5).
- `InputState.h` -- only `quitRequested` so far; fleshed out with real key/mouse state in
  M3 (docs/03 section 8).
- `InputSystem.h` -- M3, keyboard only; gamepad via SDL_GameController is a later extension.
- `DirectDRMDisplayBackend.h` + `.cpp` -- post-vertical-slice, Linux-only (docs/01 section 7.3).
