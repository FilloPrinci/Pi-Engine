#pragma once

#include "engine/rhi/RHIPipeline.h"

#include <glm/glm.hpp>
#include <volk.h>

namespace engine::rhi {
class RHIContext;
}

namespace engine::renderer {

// Concrete forward pipeline, textured + flat-tint variant (post-Editor-E8, material
// assets' "ForwardLitTexturedColor" shader, ShaderPropertySchema.h) -- a separate class
// from ForwardLitPipeline/ForwardLitColorPipeline/ForwardLitTexturedPipeline, never a
// single uber-shader/uber-pipeline with runtime branching (CLAUDE.md rule 7). Combines
// ForwardLitTexturedPipeline's single combined-image-sampler descriptor set (an
// "albedoTexture" property) with ForwardLitColorPipeline's combined mvp+tintColor push
// constant (a "tintColor" property, multiplying the sampled texel) -- the fragment
// shader samples then tints, no lighting math (this engine is still unlit-only
// everywhere).
//
// Expects `engine::renderer::Vertex` (MeshLoader.h: position + normal + uv) as its vertex
// input, same as ForwardLitTexturedPipeline. Owns only the pipeline/descriptor-set
// *layout*; the descriptor pool and the per-material VkDescriptorSet are the caller's
// responsibility (same division of ownership ForwardLitTexturedPipeline already
// established -- see that header's own comment), now driven by MaterialData's
// "albedoTexture" property instead of one hardcoded demo texture.
class ForwardLitTexturedColorPipeline {
public:
    ForwardLitTexturedColorPipeline() = default;
    ~ForwardLitTexturedColorPipeline() = default;

    ForwardLitTexturedColorPipeline(const ForwardLitTexturedColorPipeline&) = delete;
    ForwardLitTexturedColorPipeline& operator=(const ForwardLitTexturedColorPipeline&) = delete;

    bool Init(rhi::RHIContext& context, VkRenderPass renderPass, VkExtent2D viewportExtent,
              const char* vertexShaderPath, const char* fragmentShaderPath);
    void Shutdown();

    void Bind(VkCommandBuffer commandBuffer) const;

    // Same combined-range reasoning as ForwardLitColorPipeline::PushMvpAndTint() -- both
    // values change together every draw, one call instead of two.
    void PushMvpAndTint(VkCommandBuffer commandBuffer, const glm::mat4& mvp,
                        const glm::vec4& tintColor) const;
    void BindDescriptorSet(VkCommandBuffer commandBuffer, VkDescriptorSet descriptorSet) const;

    // For the caller to allocate a matching VkDescriptorSet against (see
    // ForwardLitTexturedPipeline::GetDescriptorSetLayout()'s identical reasoning).
    VkDescriptorSetLayout GetDescriptorSetLayout() const {
        return m_pipeline.GetDescriptorSetLayout();
    }

private:
    rhi::RHIPipeline m_pipeline;
};

} // namespace engine::renderer
