#include "engine/core/Application.h"

#include <chrono>

namespace engine::core {

void Application::Run(platform::IDisplayBackend& displayBackend, const Callbacks& callbacks) {
    platform::InputState input;
    auto lastFrameTime = std::chrono::steady_clock::now();

    while (true) {
        displayBackend.PollEvents(input);
        if (input.quitRequested) {
            break;
        }

        const auto now = std::chrono::steady_clock::now();
        const float deltaSeconds = std::chrono::duration<float>(now - lastFrameTime).count();
        lastFrameTime = now;

        if (callbacks.onUpdate) {
            callbacks.onUpdate(deltaSeconds);
        }
        if (callbacks.onRender) {
            callbacks.onRender();
        }
    }
}

} // namespace engine::core
