#include "engine/renderer/ForwardLitColorPipeline.h"

#include "engine/renderer/MeshLoader.h"

#include <cstddef>

namespace engine::renderer {

bool ForwardLitColorPipeline::Init(rhi::RHIContext& context, VkRenderPass renderPass,
                                    VkExtent2D viewportExtent, const char* vertexShaderPath,
                                    const char* fragmentShaderPath) {
    rhi::RHIPipelineDesc desc;
    desc.vertexShaderPath = vertexShaderPath;
    desc.fragmentShaderPath = fragmentShaderPath;
    desc.renderPass = renderPass;
    desc.viewportExtent = viewportExtent;
    desc.depthTestEnable = true;
    desc.depthWriteEnable = true;
    // Matches ForwardLitPipeline's own not-yet-verified winding choice (see that class's
    // comment) -- material-tinted entities should cull identically to untinted ones.
    desc.cullMode = VK_CULL_MODE_NONE;

    VkVertexInputBindingDescription binding{};
    binding.binding = 0;
    binding.stride = sizeof(Vertex);
    binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    desc.vertexBindings = {binding};

    // Same `Vertex` layout as every other pipeline in this project (position + normal),
    // even though m_material_color.vert only reads the position attribute -- one shared
    // cooked mesh format, no material-specific vertex layout to keep in sync separately.
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

    // A single combined range (mat4 mvp + vec4 tintColor, 80 bytes) visible to both
    // stages -- m_material_color.vert/frag declare the identical PushConstants block
    // layout, so one vkCmdPushConstants() call in PushMvpAndTint() covers both.
    VkPushConstantRange pushRange{};
    pushRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    pushRange.offset = 0;
    pushRange.size = sizeof(glm::mat4) + sizeof(glm::vec4);
    desc.pushConstantRanges = {pushRange};

    return m_pipeline.Init(context, desc);
}

void ForwardLitColorPipeline::Shutdown() {
    m_pipeline.Shutdown();
}

void ForwardLitColorPipeline::Bind(VkCommandBuffer commandBuffer) const {
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline.GetHandle());
}

void ForwardLitColorPipeline::PushMvpAndTint(VkCommandBuffer commandBuffer, const glm::mat4& mvp,
                                              const glm::vec4& tintColor) const {
    // Matches m_material_color.vert/frag's `PushConstants { mat4 mvp; vec4 tintColor; }`
    // layout byte-for-byte: mvp at offset 0 (64 bytes), tintColor immediately after (16
    // bytes) -- two vkCmdPushConstants() calls into the same range rather than building
    // one combined struct, avoiding any padding/alignment mismatch risk between a
    // hand-written C++ struct and GLSL's std430-like push-constant layout rules.
    vkCmdPushConstants(commandBuffer, m_pipeline.GetLayout(),
                        VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0,
                        sizeof(glm::mat4), &mvp);
    vkCmdPushConstants(commandBuffer, m_pipeline.GetLayout(),
                        VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                        sizeof(glm::mat4), sizeof(glm::vec4), &tintColor);
}

} // namespace engine::renderer
