#include "engine/rhi/RHIBuffer.h"

#include "engine/rhi/RHIContext.h"

#include <cstdio>
#include <cstring>

namespace engine::rhi {

RHIBuffer::~RHIBuffer() {
    Shutdown();
}

bool RHIBuffer::InitWithData(RHIContext& context, VkBufferUsageFlags usage, const void* data,
                              std::size_t sizeBytes) {
    m_context = &context;

    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = static_cast<VkDeviceSize>(sizeBytes);
    bufferInfo.usage = usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocationCreateInfo allocInfo{};
    allocInfo.usage = VMA_MEMORY_USAGE_AUTO;
    allocInfo.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;

    VmaAllocationInfo resultInfo{};
    if (vmaCreateBuffer(m_context->GetAllocator(), &bufferInfo, &allocInfo, &m_buffer, &m_allocation,
                         &resultInfo) != VK_SUCCESS) {
        std::fprintf(stderr, "RHIBuffer: vmaCreateBuffer failed\n");
        return false;
    }

    if (resultInfo.pMappedData == nullptr) {
        std::fprintf(stderr, "RHIBuffer: allocation was not mapped (unexpected on Pi4's unified "
                              "memory -- see RHIBuffer.h)\n");
        return false;
    }

    std::memcpy(resultInfo.pMappedData, data, sizeBytes);
    return true;
}

void RHIBuffer::Shutdown() {
    if (m_context != nullptr && m_buffer != VK_NULL_HANDLE) {
        vmaDestroyBuffer(m_context->GetAllocator(), m_buffer, m_allocation);
        m_buffer = VK_NULL_HANDLE;
        m_allocation = VK_NULL_HANDLE;
    }
    m_context = nullptr;
}

} // namespace engine::rhi
