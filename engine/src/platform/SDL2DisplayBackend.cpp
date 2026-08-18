#include "engine/platform/SDL2DisplayBackend.h"

#include <SDL2/SDL.h>
#include <SDL2/SDL_vulkan.h>

#include <cstdint>
#include <cstdio>

namespace engine::platform {

SDL2DisplayBackend::~SDL2DisplayBackend() {
    Shutdown();
}

bool SDL2DisplayBackend::Init() {
    if (SDL_InitSubSystem(SDL_INIT_VIDEO) != 0) {
        std::fprintf(stderr, "SDL2DisplayBackend: SDL_InitSubSystem(SDL_INIT_VIDEO) failed: %s\n",
                     SDL_GetError());
        return false;
    }

    m_window = SDL_CreateWindow("Pi-Engine", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1280,
                                 720, SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_SHOWN);
    if (m_window == nullptr) {
        std::fprintf(stderr, "SDL2DisplayBackend: SDL_CreateWindow failed: %s\n", SDL_GetError());
        SDL_QuitSubSystem(SDL_INIT_VIDEO);
        return false;
    }

    return true;
}

VkSurfaceKHR SDL2DisplayBackend::CreateVulkanSurface(VkInstance instance) {
    VkSurfaceKHR surface = VK_NULL_HANDLE;
    if (SDL_Vulkan_CreateSurface(m_window, instance, &surface) != SDL_TRUE) {
        std::fprintf(stderr, "SDL2DisplayBackend: SDL_Vulkan_CreateSurface failed: %s\n",
                     SDL_GetError());
        return VK_NULL_HANDLE;
    }
    return surface;
}

std::vector<const char*> SDL2DisplayBackend::GetRequiredVulkanExtensions() {
    unsigned int count = 0;
    if (SDL_Vulkan_GetInstanceExtensions(m_window, &count, nullptr) != SDL_TRUE) {
        std::fprintf(stderr,
                     "SDL2DisplayBackend: SDL_Vulkan_GetInstanceExtensions (count) failed: %s\n",
                     SDL_GetError());
        return {};
    }

    std::vector<const char*> extensions(count);
    if (SDL_Vulkan_GetInstanceExtensions(m_window, &count, extensions.data()) != SDL_TRUE) {
        std::fprintf(stderr,
                     "SDL2DisplayBackend: SDL_Vulkan_GetInstanceExtensions (names) failed: %s\n",
                     SDL_GetError());
        return {};
    }
    return extensions;
}

void SDL2DisplayBackend::PollEvents(InputState& out) {
    SDL_Event event;
    while (SDL_PollEvent(&event) != 0) {
        if (event.type == SDL_QUIT) {
            m_shouldQuit = true;
        } else if (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_CLOSE) {
            m_shouldQuit = true;
        }
        // Keyboard/mouse/gamepad translation arrives in M3 (platform/InputSystem).
    }
    out.quitRequested = m_shouldQuit;
}

core::Extent2D SDL2DisplayBackend::GetDrawableSize() {
    int width = 0;
    int height = 0;
    SDL_Vulkan_GetDrawableSize(m_window, &width, &height);
    return core::Extent2D{static_cast<std::uint32_t>(width), static_cast<std::uint32_t>(height)};
}

void SDL2DisplayBackend::Shutdown() {
    if (m_window != nullptr) {
        SDL_DestroyWindow(m_window);
        m_window = nullptr;
    }
    SDL_QuitSubSystem(SDL_INIT_VIDEO);
}

} // namespace engine::platform
