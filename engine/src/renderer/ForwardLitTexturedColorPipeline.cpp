#include "engine/renderer/ForwardLitTexturedColorPipeline.h"

#include "engine/renderer/MeshLoader.h"

#include <cstddef>

namespace engine::renderer {

bool ForwardLitTexturedColorPipeline::Init(rhi::RHIContext& context, VkRenderPass renderPass,
                                            VkExtent2D viewportExtent,
                                            const char* vertexShaderPath,
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

    // Same combined mvp+tintColor range as ForwardLitColorPipeline (see that class's own
    // comment) -- m_material_textured_color.vert/frag declare the identical
    // PushConstants block layout.
    VkPushConstantRange pushRange{};
    pushRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    pushRange.offset = 0;
    pushRange.size = sizeof(glm::mat4) + sizeof(glm::vec4);
    desc.pushConstantRanges = {pushRange};

    VkDescriptorSetLayoutBinding samplerBinding{};
    samplerBinding.binding = 0;
    samplerBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    samplerBinding.descriptorCount = 1;
    samplerBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    desc.descriptorSetLayoutBindings = {samplerBinding};

    return m_pipeline.Init(context, desc);
}

void ForwardLitTexturedColorPipeline::Shutdown() {
    m_pipeline.Shutdown();
}

void ForwardLitTexturedColorPipeline::Bind(VkCommandBuffer commandBuffer) const {
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline.GetHandle());
}

void ForwardLitTexturedColorPipeline::PushMvpAndTint(VkCommandBuffer commandBuffer,
                                                       const glm::mat4& mvp,
                                                       const glm::vec4& tintColor) const {
    vkCmdPushConstants(commandBuffer, m_pipeline.GetLayout(),
                        VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0,
                        sizeof(glm::mat4), &mvp);
    vkCmdPushConstants(commandBuffer, m_pipeline.GetLayout(),
                        VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                        sizeof(glm::mat4), sizeof(glm::vec4), &tintColor);
}

void ForwardLitTexturedColorPipeline::BindDescriptorSet(VkCommandBuffer commandBuffer,
                                                          VkDescriptorSet descriptorSet) const {
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline.GetLayout(),
                             0, 1, &descriptorSet, 0, nullptr);
}

} // namespace engine::renderer
