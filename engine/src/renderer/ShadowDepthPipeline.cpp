#include "engine/renderer/ShadowDepthPipeline.h"

#include "engine/renderer/MeshLoader.h"

#include <cstddef>

namespace engine::renderer {

bool ShadowDepthPipeline::Init(rhi::RHIContext& context, VkRenderPass renderPass,
                                VkExtent2D viewportExtent, const char* vertexShaderPath,
                                const char* fragmentShaderPath) {
    rhi::RHIPipelineDesc desc;
    desc.vertexShaderPath = vertexShaderPath;
    desc.fragmentShaderPath = fragmentShaderPath;
    desc.renderPass = renderPass;
    desc.viewportExtent = viewportExtent;
    desc.depthTestEnable = true;
    desc.depthWriteEnable = true;
    desc.cullMode = VK_CULL_MODE_NONE; // Matches every other pipeline's own not-yet-
                                       // verified-winding TODO.

    VkVertexInputBindingDescription binding{};
    binding.binding = 0;
    binding.stride = sizeof(Vertex);
    binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    desc.vertexBindings = {binding};

    // Position only -- this class's own header comment explains why normal/uv are never
    // declared even though the bound buffer still has them interleaved.
    VkVertexInputAttributeDescription positionAttr{};
    positionAttr.location = 0;
    positionAttr.binding = 0;
    positionAttr.format = VK_FORMAT_R32G32B32_SFLOAT;
    positionAttr.offset = offsetof(Vertex, position);
    desc.vertexAttributes = {positionAttr};

    VkPushConstantRange pushRange{};
    pushRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    pushRange.offset = 0;
    pushRange.size = sizeof(glm::mat4);
    desc.pushConstantRanges = {pushRange};

    return m_pipeline.Init(context, desc);
}

void ShadowDepthPipeline::Shutdown() {
    m_pipeline.Shutdown();
}

void ShadowDepthPipeline::Bind(VkCommandBuffer commandBuffer) const {
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline.GetHandle());
}

void ShadowDepthPipeline::PushMvp(VkCommandBuffer commandBuffer, const glm::mat4& mvp) const {
    vkCmdPushConstants(commandBuffer, m_pipeline.GetLayout(), VK_SHADER_STAGE_VERTEX_BIT, 0,
                        sizeof(glm::mat4), &mvp);
}

} // namespace engine::renderer
