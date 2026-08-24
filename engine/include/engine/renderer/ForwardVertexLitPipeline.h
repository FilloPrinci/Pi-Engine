#pragma once

#include "engine/rhi/RHIPipeline.h"

#include <glm/glm.hpp>
#include <volk.h>

namespace engine::rhi {
class RHIContext;
}

namespace engine::renderer {

// Concrete forward pipeline, the engine's *default* lit material (the user's own explicit
// request: "il materiale di base deve essere quello lit con illuminazione dinamica solo
// per vertex"). docs/01 section 8.3's "Low-Poly Retro" profile explicitly allows either
// vertex lighting or minimal Blinn-Phong -- ForwardLitShadedPipeline already built the
// fragment/Blinn-Phong half; this is the vertex-lit half. A sixth separate concrete
// pipeline (CLAUDE.md rule 7) -- never a branch inside ForwardLitShadedPipeline's own
// shader.
//
// Same lighting formula as ForwardLitShadedPipeline (ambient + N-dot-L diffuse + a fixed-
// shininess Blinn-Phong specular term, reading the identical FrameLightingData UBO at
// set = 0), but evaluated once per *vertex* and interpolated across the triangle (Gouraud
// shading) instead of once per fragment -- cheaper on Pi4's fill-heavy TBDR GPU at the
// cost of coarser highlights on large triangles, an intentional trade-off for the "base"
// material every low-poly mesh is meant to use by default, not an oversight.
//
// Flat-tint only, no texture -- see ForwardVertexLitTexturedPipeline for the texture-
// supporting sibling (same split ForwardLitColorPipeline/ForwardLitTexturedColorPipeline
// already established: switching a material between the two means retargeting its
// shaderName, not toggling a flag on one shared shader).
//
// Lighting phase B: set = 0 also carries the static shadow map's comparison sampler
// (binding = 1, alongside the FrameLightingData UBO at binding = 0) -- this pipeline's own
// shadow lookup happens per-*vertex* (matching its per-vertex lighting), unlike
// ForwardLitShadedPipeline's per-fragment one, but the binding itself still declares both
// stages (this class's own .cpp comment explains why that matters).
//
// Expects `engine::renderer::Vertex` (MeshLoader.h: position + normal), same as
// ForwardLitShadedPipeline. Per-draw push constant is a model matrix (not a precomputed
// MVP -- the frame UBO's viewProj covers that, same reasoning as
// ForwardLitShadedPipeline.h's own comment) plus a tint color.
class ForwardVertexLitPipeline {
public:
    ForwardVertexLitPipeline() = default;
    ~ForwardVertexLitPipeline() = default;

    ForwardVertexLitPipeline(const ForwardVertexLitPipeline&) = delete;
    ForwardVertexLitPipeline& operator=(const ForwardVertexLitPipeline&) = delete;

    bool Init(rhi::RHIContext& context, VkRenderPass renderPass, VkExtent2D viewportExtent,
              const char* vertexShaderPath, const char* fragmentShaderPath);
    void Shutdown();

    void Bind(VkCommandBuffer commandBuffer) const;

    // Per-draw: the entity's own model matrix plus its material's tint. Same shape as
    // ForwardLitShadedPipeline::PushModelAndTint().
    void PushModelAndTint(VkCommandBuffer commandBuffer, const glm::mat4& model,
                          const glm::vec4& tintColor) const;

    // Per-frame (called once, before drawing any entity through this pipeline that frame)
    // -- binds the caller's own FrameLightingData descriptor set. The binding shape is
    // identical to ForwardLitShadedPipeline's own set 0, so callers can share the exact
    // same already-allocated VkDescriptorSet across both pipelines (Vulkan spec 14.2.2,
    // "identically defined" descriptor set layouts) -- no separate allocation needed.
    void BindFrameDescriptorSet(VkCommandBuffer commandBuffer, VkDescriptorSet descriptorSet) const;

    VkDescriptorSetLayout GetDescriptorSetLayout() const {
        return m_pipeline.GetDescriptorSetLayout();
    }

private:
    rhi::RHIPipeline m_pipeline;
};

} // namespace engine::renderer
