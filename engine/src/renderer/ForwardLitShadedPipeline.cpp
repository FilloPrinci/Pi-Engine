#include "engine/renderer/ForwardLitShadedPipeline.h"

#include "engine/renderer/MeshLoader.h"

#include <cstddef>

namespace engine::renderer {

bool ForwardLitShadedPipeline::Init(rhi::RHIContext& context, VkRenderPass renderPass,
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

    // model (mat4, vertex) + tintColor (vec4, fragment) -- 80 bytes, comfortably under
    // Vulkan's guaranteed 128-byte minimum push-constant size (unlike model+precomputed-
    // MVP+tint, which wouldn't be -- see this class's own header comment for why viewProj
    // lives in the frame UBO instead).
    VkPushConstantRange pushRange{};
    pushRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    pushRange.offset = 0;
    pushRange.size = sizeof(glm::mat4) + sizeof(glm::vec4);
    desc.pushConstantRanges = {pushRange};

    // The per-frame lighting UBO (FrameLightingData) -- bound once per frame by the
    // caller (BindFrameDescriptorSet()), read by both stages (viewProj in the vertex
    // shader, lights/ambient/camera position in the fragment shader).
    VkDescriptorSetLayoutBinding frameDataBinding{};
    frameDataBinding.binding = 0;
    frameDataBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    frameDataBinding.descriptorCount = 1;
    frameDataBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;

    // Lighting phase B -- the static shadow map's comparison sampler, same set as the
    // frame UBO above (this class's own header comment explains why). This pipeline's own
    // shadow lookup happens per-fragment (matching its per-fragment Blinn-Phong), but the
    // binding still declares *both* stages -- ForwardVertexLitPipeline's own comment on
    // its identical binding explains why: every lit pipeline's set-0 bindings must stay
    // identically defined so callers can share one already-allocated VkDescriptorSet
    // across all three (Vulkan spec 14.2.2).
    VkDescriptorSetLayoutBinding shadowMapBinding{};
    shadowMapBinding.binding = 1;
    shadowMapBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    shadowMapBinding.descriptorCount = 1;
    shadowMapBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;

    desc.descriptorSetLayoutBindings = {frameDataBinding, shadowMapBinding};

    return m_pipeline.Init(context, desc);
}

void ForwardLitShadedPipeline::Shutdown() {
    m_pipeline.Shutdown();
}

void ForwardLitShadedPipeline::Bind(VkCommandBuffer commandBuffer) const {
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline.GetHandle());
}

void ForwardLitShadedPipeline::PushModelAndTint(VkCommandBuffer commandBuffer,
                                                 const glm::mat4& model,
                                                 const glm::vec4& tintColor) const {
    vkCmdPushConstants(commandBuffer, m_pipeline.GetLayout(),
                        VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0,
                        sizeof(glm::mat4), &model);
    vkCmdPushConstants(commandBuffer, m_pipeline.GetLayout(),
                        VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                        sizeof(glm::mat4), sizeof(glm::vec4), &tintColor);
}

void ForwardLitShadedPipeline::BindFrameDescriptorSet(VkCommandBuffer commandBuffer,
                                                        VkDescriptorSet descriptorSet) const {
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline.GetLayout(),
                             0, 1, &descriptorSet, 0, nullptr);
}

} // namespace engine::renderer
