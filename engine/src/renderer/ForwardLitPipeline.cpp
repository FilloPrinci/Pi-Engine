#include "engine/renderer/ForwardLitPipeline.h"

#include "engine/renderer/MeshLoader.h"

#include <cstddef>

namespace engine::renderer {

bool ForwardLitPipeline::Init(rhi::RHIContext& context, VkRenderPass renderPass,
                               VkExtent2D viewportExtent, const char* vertexShaderPath,
                               const char* fragmentShaderPath) {
    rhi::RHIPipelineDesc desc;
    desc.vertexShaderPath = vertexShaderPath;
    desc.fragmentShaderPath = fragmentShaderPath;
    desc.renderPass = renderPass;
    desc.viewportExtent = viewportExtent;
    desc.depthTestEnable = true;
    desc.depthWriteEnable = true;
    // Winding direction isn't verified against the Vulkan Y-flip yet (Camera.h flips
    // projection[1][1]) -- NONE avoids silently dropping faces until that's confirmed.
    // TODO(M1+): verify winding, switch to VK_CULL_MODE_BACK_BIT (docs/01 section 8.1).
    desc.cullMode = VK_CULL_MODE_NONE;

    VkVertexInputBindingDescription binding{};
    binding.binding = 0;
    binding.stride = sizeof(Vertex);
    binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    desc.vertexBindings = {binding};

    VkVertexInputAttributeDescription positionAttr{};
    positionAttr.location = 0;
    positionAttr.binding = 0;
    positionAttr.format = VK_FORMAT_R32G32B32_SFLOAT;
    positionAttr.offset = offsetof(Vertex, position);

    VkVertexInputAttributeDescription normalAttr{};
    normalAttr.location = 1;
    normalAttr.binding = 0;
    normalAttr.format = VK_FORMAT_R32G32B32_SFLOAT;
    normalAttr.offset = offsetof(Vertex, normal);

    desc.vertexAttributes = {positionAttr, normalAttr};

    VkPushConstantRange mvpRange{};
    mvpRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    mvpRange.offset = 0;
    mvpRange.size = sizeof(glm::mat4);
    desc.pushConstantRanges = {mvpRange};

    return m_pipeline.Init(context, desc);
}

void ForwardLitPipeline::Shutdown() {
    m_pipeline.Shutdown();
}

void ForwardLitPipeline::Bind(VkCommandBuffer commandBuffer) const {
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline.GetHandle());
}

void ForwardLitPipeline::PushModelViewProjection(VkCommandBuffer commandBuffer,
                                                  const glm::mat4& mvp) const {
    vkCmdPushConstants(commandBuffer, m_pipeline.GetLayout(), VK_SHADER_STAGE_VERTEX_BIT, 0,
                        sizeof(glm::mat4), &mvp);
}

} // namespace engine::renderer
