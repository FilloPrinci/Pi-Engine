#pragma once

#include <volk.h>
#include <vk_mem_alloc.h>

#include <cstddef>
#include <cstdint>

namespace engine::rhi {

class RHIContext;

// GPU-resident sampled texture: a device-local, optimal-tiling VkImage + VkImageView +
// VkSampler, uploaded once at load time from CPU-side pixel data via a transient
// host-visible staging buffer and a one-off command buffer that is submitted and waited
// on synchronously. That's fine for a load-time-only path (mirrors RHIBuffer's own
// "not a hot path" reasoning) but unlike RHIBuffer this does *not* skip the staging step:
// optimal-tiling images (the layout the GPU actually wants for sampling) generally can't
// be directly host-mapped the way a linear buffer can, even on Pi4/Pi5's unified memory --
// V3DV doesn't expose a host-visible optimal-tiling path, so a staging copy is the
// portable choice here.
//
// M7 scope (Asset Pipeline textures step): single mip level, RGBA8 uncompressed only --
// mipmap generation and block-compressed formats (ETC2/BC7, docs/01 section 12.2) are
// deferred, same as CookedMesh's own "deliberately minimal" scope.
class RHITexture {
public:
    RHITexture() = default;
    ~RHITexture();

    RHITexture(const RHITexture&) = delete;
    RHITexture& operator=(const RHITexture&) = delete;

    // `context` must already be initialized and must outlive this texture. `pixels` must
    // be tightly packed RGBA8 (4 bytes/pixel, no row padding) of exactly
    // `width * height * 4` bytes -- exactly what LoadCookedTexture() produces.
    bool InitWithData(RHIContext& context, std::uint32_t width, std::uint32_t height,
                       const void* pixels, std::size_t sizeBytes);
    void Shutdown();

    VkImageView GetImageView() const { return m_imageView; }
    VkSampler GetSampler() const { return m_sampler; }

private:
    RHIContext* m_context = nullptr;
    VkImage m_image = VK_NULL_HANDLE;
    VmaAllocation m_allocation = VK_NULL_HANDLE;
    VkImageView m_imageView = VK_NULL_HANDLE;
    VkSampler m_sampler = VK_NULL_HANDLE;
};

} // namespace engine::rhi
