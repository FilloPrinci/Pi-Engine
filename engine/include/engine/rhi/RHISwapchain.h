#pragma once

#include "engine/core/Types.h"

#include <volk.h>

#include <cstdint>
#include <vector>

namespace engine::rhi {

class RHIContext;

// Thin wrapper around VkSwapchainKHR + its VkImageViews (docs/03 section 5). Recreatable
// in place (window resize, VK_ERROR_OUT_OF_DATE_KHR/VK_SUBOPTIMAL_KHR). Does not own a
// VkRenderPass or VkFramebuffer -- those depend on what the renderer wants to do with the
// swapchain images, and stay outside this class.
class RHISwapchain {
public:
    RHISwapchain() = default;
    ~RHISwapchain();

    RHISwapchain(const RHISwapchain&) = delete;
    RHISwapchain& operator=(const RHISwapchain&) = delete;

    // `context` must already be initialized and must outlive this swapchain.
    bool Init(RHIContext& context, core::Extent2D desiredExtent);
    bool Recreate(core::Extent2D desiredExtent);
    void Shutdown();

    VkSwapchainKHR GetHandle() const { return m_swapchain; }
    VkFormat GetImageFormat() const { return m_imageFormat; }
    VkExtent2D GetExtent() const { return m_extent; }
    const std::vector<VkImageView>& GetImageViews() const { return m_imageViews; }
    std::uint32_t GetImageCount() const { return static_cast<std::uint32_t>(m_images.size()); }

private:
    bool CreateSwapchain(core::Extent2D desiredExtent);
    void DestroySwapchain();

    RHIContext* m_context = nullptr;
    VkSwapchainKHR m_swapchain = VK_NULL_HANDLE;
    VkFormat m_imageFormat = VK_FORMAT_UNDEFINED;
    VkExtent2D m_extent{};
    std::vector<VkImage> m_images;
    std::vector<VkImageView> m_imageViews;
};

} // namespace engine::rhi
