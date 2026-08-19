#include "engine/renderer/ForwardLitTexturedPipeline.h"

#include "engine/renderer/MeshLoader.h"

#include <cstddef>

namespace engine::renderer {

bool ForwardLitTexturedPipeline::Init(rhi::RHIContext& context, VkRenderPass renderPass,
                                       VkExtent2D viewportExtent, const char* vertexShaderPath,
                                       const char* fragmentShaderPath) {
    rhi::RHIPipelineDesc desc;
    desc.vertexShaderPath = vertexShaderPath;
    desc.fragmentShaderPath = fragmentShaderPath;
    desc.renderPass = renderPass;
    desc.viewportExtent = viewportExtent;
    desc.depthTestEnable = true;
    desc.depthWriteEnable = true;
    // Same as ForwardLitPipeline: winding isn't verified against the Vulkan Y-flip yet, so
    // NONE avoids silently dropping faces (see that class's identical TODO).
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

    VkVertexInputAttributeDescription uvAttr{};
    uvAttr.location = 2;
    uvAttr.binding = 0;
    uvAttr.format = VK_FORMAT_R32G32_SFLOAT;
    uvAttr.offset = offsetof(Vertex, uv);

    desc.vertexAttributes = {positionAttr, normalAttr, uvAttr};

    VkPushConstantRange mvpRange{};
    mvpRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    mvpRange.offset = 0;
    mvpRange.size = sizeof(glm::mat4);
    desc.pushConstantRanges = {mvpRange};

    VkDescriptorSetLayoutBinding samplerBinding{};
    samplerBinding.binding = 0;
    samplerBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    samplerBinding.descriptorCount = 1;
    samplerBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    desc.descriptorSetLayoutBindings = {samplerBinding};

    return m_pipeline.Init(context, desc);
}

void ForwardLitTexturedPipeline::Shutdown() {
    m_pipeline.Shutdown();
}

void ForwardLitTexturedPipeline::Bind(VkCommandBuffer commandBuffer) const {
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline.GetHandle());
}

void ForwardLitTexturedPipeline::PushModelViewProjection(VkCommandBuffer commandBuffer,
                                                           const glm::mat4& mvp) const {
    vkCmdPushConstants(commandBuffer, m_pipeline.GetLayout(), VK_SHADER_STAGE_VERTEX_BIT, 0,
                        sizeof(glm::mat4), &mvp);
}

void ForwardLitTexturedPipeline::BindDescriptorSet(VkCommandBuffer commandBuffer,
                                                     VkDescriptorSet descriptorSet) const {
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline.GetLayout(),
                             0, 1, &descriptorSet, 0, nullptr);
}

} // namespace engine::renderer
