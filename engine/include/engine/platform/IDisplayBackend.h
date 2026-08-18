#pragma once

#include "engine/core/Types.h"
#include "engine/platform/InputState.h"

#include <volk.h>

#include <vector>

namespace engine::platform {

// Decouples the whole engine (RHI, renderer, gameplay) from how the render surface is
// created and input is collected. Two interchangeable implementations, same binary:
// SDL2DisplayBackend (windowed, Wayland/X11 — active from M0) and DirectDRMDisplayBackend
// (KMS/DRM direct, VK_KHR_display — added later, does not block the vertical slice).
// See docs/01, section 5.
class IDisplayBackend {
public:
    virtual ~IDisplayBackend() = default;

    virtual bool Init() = 0;
    virtual VkSurfaceKHR CreateVulkanSurface(VkInstance instance) = 0;
    virtual std::vector<const char*> GetRequiredVulkanExtensions() = 0;
    virtual void PollEvents(InputState& out) = 0;
    virtual core::Extent2D GetDrawableSize() = 0;
    virtual void Shutdown() = 0;
};

} // namespace engine::platform
