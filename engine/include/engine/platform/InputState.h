#pragma once

namespace engine::platform {

// Fleshed out in M3 (docs/03, section 8: "InputState.h, InputSystem.h -- keyboard only in
// M3"). For now it only carries what M0's present loop needs: whether the backend has
// observed a request to quit (SDL_QUIT, window close button, Alt+F4, ...).
struct InputState {
    bool quitRequested = false;
};

} // namespace engine::platform
