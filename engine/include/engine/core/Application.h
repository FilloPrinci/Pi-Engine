#pragma once

#include "engine/platform/IDisplayBackend.h"

#include <functional>

namespace engine::core {

// Frame-loop orchestrator (docs/03 section 7). Introduced in M2 with just a Poll Input +
// Update + Render phase; M3 forwards the polled InputState into onUpdate so a Script phase
// can read it there (docs/01 section 11.4: input is read once per frame, before Script),
// M4 adds Physics and its barriers, M5 wires the full sequence from CLAUDE.md section 4:
//   Poll Input -> Script phase -> barrier -> Physics phase -> barrier
//   -> Collision Callback phase -> Post-Physics/Render
// Owns the loop's control flow only, not the RHI/window/ECS objects themselves -- the
// sample still constructs those and passes callbacks in, keeping Application decoupled
// from any specific rendering backend.
class Application {
public:
    struct Callbacks {
        // Called once per frame after input has been polled, before rendering. `input` is
        // this frame's snapshot, straight from IDisplayBackend::PollEvents().
        std::function<void(float deltaSeconds, const platform::InputState& input)> onUpdate;
        std::function<void()> onRender;
    };

    // Blocks until the display backend signals a quit request (window close, Alt+F4).
    void Run(platform::IDisplayBackend& displayBackend, const Callbacks& callbacks);
};

} // namespace engine::core
