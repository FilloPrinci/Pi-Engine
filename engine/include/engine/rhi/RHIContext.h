#pragma once

#include "engine/platform/IDisplayBackend.h"

#include <volk.h>

// vk_mem_alloc.h assumes it can call vk* functions unqualified; with volk providing them
// as loaded function pointers (see RHIContext.cpp), that Just Works as long as volk.h is
// included first, which the include order above already guarantees.
#include <vk_mem_alloc.h>

#include <cstdint>
#include <optional>

namespace engine::rhi {

// Thin Vulkan wrapper (docs/01 section 4, module 1): owns the VkInstance, the selected
// physical device, the logical VkDevice, and its graphics/present queues. Vulkan 1.2 core
// baseline (docs/01 section 3.2) -- feature detection for 1.3 extensions is added only
// where a later milestone actually needs one, not preemptively here.
class RHIContext {
public:
    RHIContext() = default;
    ~RHIContext();

    RHIContext(const RHIContext&) = delete;
    RHIContext& operator=(const RHIContext&) = delete;

    // `backend` must already be initialized (IDisplayBackend::Init() called) and must
    // outlive this context -- it is only used here to query required instance extensions
    // and to create the VkSurfaceKHR this context selects a physical device against.
    bool Init(platform::IDisplayBackend& backend, const char* appName);
    void Shutdown();

    VkInstance GetInstance() const { return m_instance; }
    VkSurfaceKHR GetSurface() const { return m_surface; }
    VkPhysicalDevice GetPhysicalDevice() const { return m_physicalDevice; }
    VkDevice GetDevice() const { return m_device; }
    VkQueue GetGraphicsQueue() const { return m_graphicsQueue; }
    VkQueue GetPresentQueue() const { return m_presentQueue; }
    std::uint32_t GetGraphicsQueueFamily() const { return m_graphicsQueueFamily; }
    std::uint32_t GetPresentQueueFamily() const { return m_presentQueueFamily; }
    VmaAllocator GetAllocator() const { return m_allocator; }

private:
    struct QueueFamilyIndices {
        std::optional<std::uint32_t> graphics;
        std::optional<std::uint32_t> present;

        bool IsComplete() const { return graphics.has_value() && present.has_value(); }
    };

    bool CreateInstance(platform::IDisplayBackend& backend, const char* appName);
    bool CreateDebugMessenger();
    bool SelectPhysicalDevice();
    bool CreateLogicalDevice();
    bool CreateAllocator();
    QueueFamilyIndices FindQueueFamilies(VkPhysicalDevice device) const;
    bool IsDeviceSuitable(VkPhysicalDevice device) const;

    VkInstance m_instance = VK_NULL_HANDLE;
    VkDebugUtilsMessengerEXT m_debugMessenger = VK_NULL_HANDLE;
    VkSurfaceKHR m_surface = VK_NULL_HANDLE;
    VkPhysicalDevice m_physicalDevice = VK_NULL_HANDLE;
    VkDevice m_device = VK_NULL_HANDLE;
    VkQueue m_graphicsQueue = VK_NULL_HANDLE;
    VkQueue m_presentQueue = VK_NULL_HANDLE;
    std::uint32_t m_graphicsQueueFamily = 0;
    std::uint32_t m_presentQueueFamily = 0;
    VmaAllocator m_allocator = VK_NULL_HANDLE;
};

} // namespace engine::rhi
