#pragma once

#include <volk.h>
#include <vk_mem_alloc.h>

#include <cstddef>

namespace engine::rhi {

class RHIContext;

// Thin GPU buffer wrapper via VMA (docs/03 section 6). Uploads happen through a
// persistently-mapped, host-visible allocation rather than a staging-buffer copy: Pi4/Pi5
// have no dedicated VRAM (docs/01 section 2.3) -- device-local and host-visible memory
// are frequently the same physical memory there, so a direct mapped write is simple and,
// on this hardware, not meaningfully slower. Revisit with a staging path if profiling
// ever shows otherwise (e.g. once uploads move off the load-time-only path this is today).
class RHIBuffer {
public:
    RHIBuffer() = default;
    ~RHIBuffer();

    RHIBuffer(const RHIBuffer&) = delete;
    RHIBuffer& operator=(const RHIBuffer&) = delete;

    // `context` must already be initialized and must outlive this buffer.
    bool InitWithData(RHIContext& context, VkBufferUsageFlags usage, const void* data,
                       std::size_t sizeBytes);
    void Shutdown();

    VkBuffer GetHandle() const { return m_buffer; }

private:
    RHIContext* m_context = nullptr;
    VkBuffer m_buffer = VK_NULL_HANDLE;
    VmaAllocation m_allocation = VK_NULL_HANDLE;
};

} // namespace engine::rhi
