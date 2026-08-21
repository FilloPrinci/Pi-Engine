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

    // Overwrites an already-initialized buffer's contents in place -- for a buffer written
    // more than once (lighting phase A's per-frame light/camera UBO, docs/01 section 8.3),
    // unlike every other RHIBuffer use so far (vertex/index buffers, all write-once at load
    // time). `sizeBytes` must not exceed the size this buffer was originally created with
    // (InitWithData() sizes the allocation once; this never reallocates) -- callers that
    // need a varying amount of live data (e.g. "however many lights exist this frame")
    // should size the buffer for its worst case up front (a small fixed budget, e.g. 4
    // lights, docs/01's own indicative number) and always write that many, padding unused
    // slots, rather than resizing per frame. Safe to call every frame: the allocation is
    // always persistently host-mapped (see InitWithData()'s own comment), so this is just a
    // memcpy, no new VkBuffer/allocation, no synchronization the caller doesn't already
    // handle by other means (e.g. one buffer per frame-in-flight, so a write here never
    // touches the same bytes a still-in-flight GPU read is using -- see
    // ForwardLitShadedPipeline's own comment for how the frame-UBO caller ensures this).
    void UpdateData(const void* data, std::size_t sizeBytes);

    void Shutdown();

    VkBuffer GetHandle() const { return m_buffer; }

private:
    RHIContext* m_context = nullptr;
    VkBuffer m_buffer = VK_NULL_HANDLE;
    VmaAllocation m_allocation = VK_NULL_HANDLE;
    void* m_mappedData = nullptr;
};

} // namespace engine::rhi
