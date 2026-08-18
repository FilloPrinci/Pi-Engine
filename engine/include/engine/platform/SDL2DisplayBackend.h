#pragma once

#include "engine/platform/IDisplayBackend.h"

struct SDL_Window;

namespace engine::platform {

// Windowed backend on Wayland/X11 via SDL2 (docs/01, section 5.3). The only
// IDisplayBackend implementation active through the vertical slice (M0-M5);
// DirectDRMDisplayBackend arrives later and does not block it (docs/01, section 7.3).
class SDL2DisplayBackend final : public IDisplayBackend {
public:
    SDL2DisplayBackend() = default;
    ~SDL2DisplayBackend() override;

    SDL2DisplayBackend(const SDL2DisplayBackend&) = delete;
    SDL2DisplayBackend& operator=(const SDL2DisplayBackend&) = delete;

    bool Init() override;
    VkSurfaceKHR CreateVulkanSurface(VkInstance instance) override;
    std::vector<const char*> GetRequiredVulkanExtensions() override;
    void PollEvents(InputState& out) override;
    core::Extent2D GetDrawableSize() override;
    void Shutdown() override;

private:
    SDL_Window* m_window = nullptr;
    bool m_shouldQuit = false;
};

} // namespace engine::platform
