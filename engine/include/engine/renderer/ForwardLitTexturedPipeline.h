#pragma once

#include "engine/rhi/RHIPipeline.h"

#include <glm/glm.hpp>
#include <volk.h>

namespace engine::rhi {
class RHIContext;
}

namespace engine::renderer {

// Concrete forward pipeline, textured-unlit variant (M7, Asset Pipeline textures step) --
// a separate class from ForwardLitPipeline, never a single uber-shader/uber-pipeline with
// runtime branching (CLAUDE.md rule 7). Same push-constant MVP as ForwardLitPipeline, plus
// one descriptor set (set = 0, binding = 0: combined image sampler, fragment stage only)
// bound per-draw by the caller via BindDescriptorSet() -- this class only owns the
// pipeline/layout, not the descriptor pool or the VkDescriptorSet itself (no material
// system exists yet to own that; see samples/m7_textures/main.cpp).
//
// Expects `engine::renderer::Vertex` (MeshLoader.h: position + normal + uv) as its vertex
// input.
class ForwardLitTexturedPipeline {
public:
    ForwardLitTexturedPipeline() = default;
    ~ForwardLitTexturedPipeline() = default;

    ForwardLitTexturedPipeline(const ForwardLitTexturedPipeline&) = delete;
    ForwardLitTexturedPipeline& operator=(const ForwardLitTexturedPipeline&) = delete;

    bool Init(rhi::RHIContext& context, VkRenderPass renderPass, VkExtent2D viewportExtent,
              const char* vertexShaderPath, const char* fragmentShaderPath);
    void Shutdown();

    void Bind(VkCommandBuffer commandBuffer) const;
    void PushModelViewProjection(VkCommandBuffer commandBuffer, const glm::mat4& mvp) const;
    void BindDescriptorSet(VkCommandBuffer commandBuffer, VkDescriptorSet descriptorSet) const;

    // For the caller to allocate a matching VkDescriptorSet against (see
    // samples/m7_textures/main.cpp).
    VkDescriptorSetLayout GetDescriptorSetLayout() const {
        return m_pipeline.GetDescriptorSetLayout();
    }

private:
    rhi::RHIPipeline m_pipeline;
};

} // namespace engine::renderer
