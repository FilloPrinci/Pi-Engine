#include "engine/rhi/RHISwapchain.h"

#include "engine/rhi/RHIContext.h"

#include <algorithm>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>

namespace engine::rhi {

namespace {

VkSurfaceFormatKHR ChooseSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& available) {
    for (const VkSurfaceFormatKHR& format : available) {
        if (format.format == VK_FORMAT_B8G8R8A8_UNORM &&
            format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            return format;
        }
    }
    return available.front();
}

VkPresentModeKHR ChoosePresentMode(const std::vector<VkPresentModeKHR>& available) {
    // FIFO is the only mode guaranteed by the spec -- safe baseline for Pi4 (docs/01
    // section 2.3: bandwidth is scarce, MAILBOX/IMMEDIATE would burn extra GPU cycles for
    // no benefit on a display-locked low-poly game). Never the shipped default -- but
    // useful as a one-off profiling toggle to see uncapped throughput/headroom on real
    // hardware, so it is exposed as an env var rather than hardcoded per-sample:
    //   PI_ENGINE_PRESENT_MODE=immediate|mailbox|fifo
    if (const char* override = std::getenv("PI_ENGINE_PRESENT_MODE")) {
        VkPresentModeKHR requested = VK_PRESENT_MODE_FIFO_KHR;
        bool recognized = true;
        if (std::strcmp(override, "immediate") == 0) {
            requested = VK_PRESENT_MODE_IMMEDIATE_KHR;
        } else if (std::strcmp(override, "mailbox") == 0) {
            requested = VK_PRESENT_MODE_MAILBOX_KHR;
        } else if (std::strcmp(override, "fifo") != 0) {
            recognized = false;
        }

        if (!recognized) {
            std::fprintf(stderr,
                         "RHISwapchain: unrecognized PI_ENGINE_PRESENT_MODE=\"%s\" (expected "
                         "fifo|immediate|mailbox), ignoring\n",
                         override);
        } else if (std::find(available.begin(), available.end(), requested) != available.end()) {
            return requested;
        } else {
            std::fprintf(stderr,
                         "RHISwapchain: PI_ENGINE_PRESENT_MODE=\"%s\" not supported by this "
                         "device/surface, falling back to FIFO\n",
                         override);
        }
    }

    for (VkPresentModeKHR mode : available) {
        if (mode == VK_PRESENT_MODE_FIFO_KHR) {
            return mode;
        }
    }
    return available.front();
}

VkExtent2D ChooseExtent(const VkSurfaceCapabilitiesKHR& capabilities, core::Extent2D desired) {
    if (capabilities.currentExtent.width != std::numeric_limits<std::uint32_t>::max()) {
        return capabilities.currentExtent;
    }

    VkExtent2D extent{desired.width, desired.height};
    extent.width =
        std::clamp(extent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
    extent.height = std::clamp(extent.height, capabilities.minImageExtent.height,
                                capabilities.maxImageExtent.height);
    return extent;
}

} // namespace

RHISwapchain::~RHISwapchain() {
    Shutdown();
}

bool RHISwapchain::Init(RHIContext& context, core::Extent2D desiredExtent) {
    m_context = &context;
    return CreateSwapchain(desiredExtent);
}

bool RHISwapchain::Recreate(core::Extent2D desiredExtent) {
    vkDeviceWaitIdle(m_context->GetDevice());
    DestroySwapchain();
    return CreateSwapchain(desiredExtent);
}

bool RHISwapchain::CreateSwapchain(core::Extent2D desiredExtent) {
    VkPhysicalDevice physicalDevice = m_context->GetPhysicalDevice();
    VkSurfaceKHR surface = m_context->GetSurface();

    VkSurfaceCapabilitiesKHR capabilities{};
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface, &capabilities);

    std::uint32_t formatCount = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatCount, nullptr);
    std::vector<VkSurfaceFormatKHR> formats(formatCount);
    vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatCount, formats.data());

    std::uint32_t presentModeCount = 0;
    vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &presentModeCount, nullptr);
    std::vector<VkPresentModeKHR> presentModes(presentModeCount);
    vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &presentModeCount,
                                               presentModes.data());

    const VkSurfaceFormatKHR surfaceFormat = ChooseSurfaceFormat(formats);
    const VkPresentModeKHR presentMode = ChoosePresentMode(presentModes);
    const VkExtent2D extent = ChooseExtent(capabilities, desiredExtent);

    std::uint32_t imageCount = capabilities.minImageCount + 1;
    if (capabilities.maxImageCount > 0 && imageCount > capabilities.maxImageCount) {
        imageCount = capabilities.maxImageCount;
    }

    VkSwapchainCreateInfoKHR createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    createInfo.surface = surface;
    createInfo.minImageCount = imageCount;
    createInfo.imageFormat = surfaceFormat.format;
    createInfo.imageColorSpace = surfaceFormat.colorSpace;
    createInfo.imageExtent = extent;
    createInfo.imageArrayLayers = 1;
    createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    const std::uint32_t queueFamilyIndices[] = {m_context->GetGraphicsQueueFamily(),
                                                 m_context->GetPresentQueueFamily()};
    if (queueFamilyIndices[0] != queueFamilyIndices[1]) {
        createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        createInfo.queueFamilyIndexCount = 2;
        createInfo.pQueueFamilyIndices = queueFamilyIndices;
    } else {
        createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }

    createInfo.preTransform = capabilities.currentTransform;
    createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    createInfo.presentMode = presentMode;
    createInfo.clipped = VK_TRUE;
    createInfo.oldSwapchain = VK_NULL_HANDLE;

    if (vkCreateSwapchainKHR(m_context->GetDevice(), &createInfo, nullptr, &m_swapchain) !=
        VK_SUCCESS) {
        std::fprintf(stderr, "RHISwapchain: vkCreateSwapchainKHR failed\n");
        return false;
    }

    m_imageFormat = surfaceFormat.format;
    m_extent = extent;

    std::uint32_t actualImageCount = 0;
    vkGetSwapchainImagesKHR(m_context->GetDevice(), m_swapchain, &actualImageCount, nullptr);
    m_images.resize(actualImageCount);
    vkGetSwapchainImagesKHR(m_context->GetDevice(), m_swapchain, &actualImageCount, m_images.data());

    m_imageViews.resize(m_images.size());
    for (std::size_t i = 0; i < m_images.size(); ++i) {
        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = m_images[i];
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = m_imageFormat;
        viewInfo.components = {VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY,
                                VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY};
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        viewInfo.subresourceRange.baseMipLevel = 0;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount = 1;

        if (vkCreateImageView(m_context->GetDevice(), &viewInfo, nullptr, &m_imageViews[i]) !=
            VK_SUCCESS) {
            std::fprintf(stderr, "RHISwapchain: vkCreateImageView failed for image %zu\n", i);
            return false;
        }
    }

    return true;
}

void RHISwapchain::DestroySwapchain() {
    if (m_context == nullptr) {
        return;
    }
    for (VkImageView view : m_imageViews) {
        vkDestroyImageView(m_context->GetDevice(), view, nullptr);
    }
    m_imageViews.clear();
    m_images.clear();

    if (m_swapchain != VK_NULL_HANDLE) {
        vkDestroySwapchainKHR(m_context->GetDevice(), m_swapchain, nullptr);
        m_swapchain = VK_NULL_HANDLE;
    }
}

void RHISwapchain::Shutdown() {
    DestroySwapchain();
    m_context = nullptr;
}

} // namespace engine::rhi
