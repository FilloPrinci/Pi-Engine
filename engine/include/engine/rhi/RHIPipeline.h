#pragma once

#include <volk.h>

#include <vector>

namespace engine::rhi {

class RHIContext;

// Everything needed to build one VkPipeline + its VkPipelineLayout. Not an abstract
// multi-backend material system -- just enough shared Vulkan boilerplate that concrete
// pipeline classes (ForwardLitPipeline in M1, ForwardPlusPBRPipeline later) don't each
// repeat it. Every rendering pipeline stays its own concrete class configuring this
// struct differently -- never a single uber-pipeline with runtime branching
// (CLAUDE.md rule 7).
struct RHIPipelineDesc {
    const char* vertexShaderPath = nullptr;
    const char* fragmentShaderPath = nullptr;
    std::vector<VkVertexInputBindingDescription> vertexBindings;
    std::vector<VkVertexInputAttributeDescription> vertexAttributes;
    std::vector<VkPushConstantRange> pushConstantRanges;
    VkRenderPass renderPass = VK_NULL_HANDLE;
    VkExtent2D viewportExtent{};
    VkCullModeFlags cullMode = VK_CULL_MODE_NONE;
    bool depthTestEnable = true;
    bool depthWriteEnable = true;
};

class RHIPipeline {
public:
    RHIPipeline() = default;
    ~RHIPipeline();

    RHIPipeline(const RHIPipeline&) = delete;
    RHIPipeline& operator=(const RHIPipeline&) = delete;

    // `context` must already be initialized and must outlive this pipeline.
    bool Init(RHIContext& context, const RHIPipelineDesc& desc);
    void Shutdown();

    VkPipeline GetHandle() const { return m_pipeline; }
    VkPipelineLayout GetLayout() const { return m_layout; }

private:
    RHIContext* m_context = nullptr;
    VkPipeline m_pipeline = VK_NULL_HANDLE;
    VkPipelineLayout m_layout = VK_NULL_HANDLE;
};

} // namespace engine::rhi
