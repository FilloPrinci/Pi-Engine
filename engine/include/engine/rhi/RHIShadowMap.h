#pragma once

#include <volk.h>
#include <vk_mem_alloc.h>

#include <cstdint>

namespace engine::rhi {

class RHIContext;

// Render-to-texture depth target (lighting phase B, docs/01 section 8.3's "preferably
// baked" static shadow map guidance) -- this project's first render target that's also
// read back as a shader input. Distinct from every earlier RHI resource: RHIBuffer/
// RHITexture are written once from CPU-side data and never rendered *into*; every
// RHIPipeline so far renders straight to the swapchain and is never sampled afterward.
// Depth-only (no color attachment -- a shadow map only ever needs depth), fixed square
// resolution, comparison-sampled (the owned VkSampler has `compareEnable = VK_TRUE`,
// giving hardware bilinear PCF "for free" -- consuming shaders declare
// `sampler2DShadow`/`samplerCubeShadow` and get a filtered 0..1 shadow term back with no
// manual multi-tap loop of their own).
//
// Baked *once*, synchronously, at load time via a one-off command buffer -- the same
// pattern RHITexture's own upload path already uses (transient command pool, submit,
// `vkQueueWaitIdle()`), not a per-frame render target. That full CPU-side wait is also
// why this class needs no subpass-external *read*-side dependency of its own: nothing
// ever samples the shadow map before the bake's own `vkQueueWaitIdle()` call has already
// returned, so there is no concurrent GPU work to synchronize against.
//
// Directional-only for now (phase B's own scoping decision) -- a single 2D depth target
// viewed from one direction. A point light's shadow would need a *cube map* (6 faces,
// one render pass each, viewed from the light's own position along each cube axis)
// instead of this single directional projection -- deliberately not built speculatively;
// see this class's own README entry for what a `RHIShadowCubeMap` sibling would need.
class RHIShadowMap {
public:
    bool Init(RHIContext& context, std::uint32_t resolution);
    void Shutdown();

    VkRenderPass GetRenderPass() const { return m_renderPass; }
    VkFramebuffer GetFramebuffer() const { return m_framebuffer; }
    VkImageView GetImageView() const { return m_imageView; }
    VkSampler GetSampler() const { return m_sampler; }
    VkExtent2D GetExtent() const { return {m_resolution, m_resolution}; }

private:
    RHIContext* m_context = nullptr;
    VkImage m_image = VK_NULL_HANDLE;
    VmaAllocation m_allocation = VK_NULL_HANDLE;
    VkImageView m_imageView = VK_NULL_HANDLE;
    VkSampler m_sampler = VK_NULL_HANDLE;
    VkRenderPass m_renderPass = VK_NULL_HANDLE;
    VkFramebuffer m_framebuffer = VK_NULL_HANDLE;
    std::uint32_t m_resolution = 0;
};

} // namespace engine::rhi
