#pragma once

#include "engine/platform/IDisplayBackend.h"

#include <functional>

struct SDL_Window;
union SDL_Event;

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

    // Not part of IDisplayBackend (docs/01 section 5.2 defines that interface exactly;
    // DirectDRMDisplayBackend has no titlebar to update). Handy for samples to show a
    // live FPS counter without pulling in the debug-overlay dependency M2 brings
    // (docs/01 section 4, module 4).
    void SetWindowTitle(const char* title);

    // Not part of IDisplayBackend either -- lets a consumer (engine::debug::ImGuiOverlay,
    // Editor step E1) see every raw SDL_Event PollEvents() already loops over internally,
    // without SDL2DisplayBackend needing to know anything about ImGui or any other
    // specific consumer. Same dependency-injection reasoning as
    // engine::scene::CreatePhysicsBodyFn (docs/01 section 12.2, engine/scene/README.md):
    // keeps platform/ decoupled from debug/ at the symbol/link level. Called once per
    // event, in PollEvents(), before that event's InputState translation.
    using RawEventHandler = std::function<void(const SDL_Event&)>;
    void SetRawEventHandler(RawEventHandler handler) { m_rawEventHandler = std::move(handler); }

    // Also not part of IDisplayBackend -- ImGui_ImplSDL2_InitForVulkan() needs the raw
    // SDL_Window*, which IDisplayBackend deliberately never exposes (DirectDRMDisplayBackend
    // has no SDL_Window at all).
    SDL_Window* GetSDLWindow() const { return m_window; }

    // Also not part of IDisplayBackend -- lets application code (e.g. the Editor's
    // Project Hub, step E7) end the frame loop on its own terms instead of only via a
    // real window-close event. The next PollEvents() call reports quitRequested = true,
    // same as if the user had closed the window.
    void RequestQuit() { m_shouldQuit = true; }

private:
    SDL_Window* m_window = nullptr;
    bool m_shouldQuit = false;
    RawEventHandler m_rawEventHandler;
};

} // namespace engine::platform
