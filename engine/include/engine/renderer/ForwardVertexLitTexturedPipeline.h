#pragma once

#include "engine/rhi/RHIPipeline.h"

#include <glm/glm.hpp>
#include <volk.h>

namespace engine::rhi {
class RHIContext;
}

namespace engine::renderer {

// Texture-supporting sibling of ForwardVertexLitPipeline (same split
// ForwardLitColorPipeline/ForwardLitTexturedColorPipeline already established) -- a
// seventh separate concrete pipeline (CLAUDE.md rule 7). Needs *two* independently-bound
// resources with different lifetimes: the per-frame lighting UBO (set = 0, bound once per
// pass, same as every other lit pipeline) and a per-material albedo texture (set = 1,
// rebound per draw, same as ForwardLitTexturedColorPipeline's own single descriptor set)
// -- the first pipeline in this codebase needing RHIPipeline's second-descriptor-set
// support (RHIPipeline.h's own comment on `secondDescriptorSetLayoutBindings`). Its set = 1
// reuses the *exact* single combined-image-sampler binding shape
// ForwardLitTexturedColorPipeline already declares (binding = 0, COMBINED_IMAGE_SAMPLER,
// fragment stage only), so a VkDescriptorSet allocated against that pipeline's own layout
// is Vulkan-spec-compatible here too ("identically defined" descriptor set layouts,
// Vulkan spec 14.2.2) -- the caller can reuse its *existing* per-texture descriptor set
// cache unchanged, no second cache/pool needed.
//
// Expects `engine::renderer::Vertex` (MeshLoader.h: position + normal + uv). Sampled texel
// multiplies the material's tintColor (albedo = texel * tint, same order
// m_material_textured_color.frag uses), then that albedo multiplies the interpolated
// per-vertex lit color from m_forward_vertex_lit_textured.vert. When no texture is
// assigned, the caller simply doesn't draw the entity through this pipeline at all
// (ForwardVertexLitPipeline covers the no-texture case instead), so there is no in-shader
// "missing texture" branch.
class ForwardVertexLitTexturedPipeline {
public:
    ForwardVertexLitTexturedPipeline() = default;
    ~ForwardVertexLitTexturedPipeline() = default;

    ForwardVertexLitTexturedPipeline(const ForwardVertexLitTexturedPipeline&) = delete;
    ForwardVertexLitTexturedPipeline& operator=(const ForwardVertexLitTexturedPipeline&) = delete;

    bool Init(rhi::RHIContext& context, VkRenderPass renderPass, VkExtent2D viewportExtent,
              const char* vertexShaderPath, const char* fragmentShaderPath);
    void Shutdown();

    void Bind(VkCommandBuffer commandBuffer) const;
    void PushModelAndTint(VkCommandBuffer commandBuffer, const glm::mat4& model,
                          const glm::vec4& tintColor) const;

    // set = 0 -- shared per-frame lighting data, bound once per pass (same convention as
    // ForwardVertexLitPipeline/ForwardLitShadedPipeline; can share the exact same
    // VkDescriptorSet as both of those, see this class's own header comment).
    void BindFrameDescriptorSet(VkCommandBuffer commandBuffer, VkDescriptorSet descriptorSet) const;
    // set = 1 -- per-material albedo texture, rebound per draw (can share the exact same
    // VkDescriptorSet cache as ForwardLitTexturedColorPipeline, see this class's own
    // header comment).
    void BindMaterialDescriptorSet(VkCommandBuffer commandBuffer, VkDescriptorSet descriptorSet) const;

    VkDescriptorSetLayout GetDescriptorSetLayout() const {
        return m_pipeline.GetDescriptorSetLayout();
    }
    VkDescriptorSetLayout GetMaterialDescriptorSetLayout() const {
        return m_pipeline.GetSecondDescriptorSetLayout();
    }

private:
    rhi::RHIPipeline m_pipeline;
};

} // namespace engine::renderer
