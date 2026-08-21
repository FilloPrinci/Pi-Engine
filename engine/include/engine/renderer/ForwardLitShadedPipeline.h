#pragma once

#include "engine/rhi/RHIPipeline.h"

#include <glm/glm.hpp>
#include <volk.h>

namespace engine::rhi {
class RHIContext;
}

namespace engine::renderer {

// Lighting phase A (docs/01 section 8.3's "Low-Poly Retro" profile -- vertex/Blinn-Phong
// lighting, an indicative budget of 2-4 simultaneous lights; explicitly not the "PBR
// profile" CLAUDE.md keeps out of scope). A fixed, small light budget rather than a
// dynamically-sized list -- matches docs/01's own number and keeps the frame data a
// simple fixed-layout UBO (no SSBO/bindless indexing needed for this scale).
constexpr int kMaxLights = 4;

// One light's GPU-side data -- deliberately not `engine::ecs::LightComponent` itself
// (that struct stays renderer-agnostic, this one matches std140 layout exactly: every
// member is a full vec4, so the array below packs with no compiler-inserted padding
// surprises). `positionOrDirection.w` doubles as the type tag (0 = Directional, using
// -direction; 1 = Point, using position) rather than a separate int field, saving a slot.
struct GpuLight {
    glm::vec4 positionOrDirection = glm::vec4(0.0f, -1.0f, 0.0f, 0.0f); // default: straight down.
    glm::vec4 color = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f); // rgb = color, a = intensity.
    glm::vec4 params = glm::vec4(0.0f); // x = range (Point only), yzw reserved.
};

// Bound once per frame (not per-draw like every other pipeline's push-constant data so
// far) -- shared by every entity ForwardLitShadedPipeline draws that frame. Lives in a
// UBO (set = 0, binding = 0), not a push constant: it's too large to fit push-constant
// space alongside a per-draw model matrix (Vulkan only guarantees 128 bytes minimum), and
// conceptually it *is* per-frame/scene-wide data, not per-draw -- the vertex shader
// multiplies `viewProj * pc.model` itself rather than the CPU precomputing a full MVP per
// draw the way every unlit pipeline in this project still does.
struct FrameLightingData {
    glm::mat4 viewProj = glm::mat4(1.0f);
    glm::vec4 cameraWorldPosition = glm::vec4(0.0f); // xyz used (Blinn-Phong specular's
                                                      // view direction), w unused.
    glm::vec4 ambientAndCount = glm::vec4(0.05f, 0.05f, 0.05f, 0.0f); // rgb = flat ambient
                                                                      // term, a = active
                                                                      // light count (as a
                                                                      // float, cast to int
                                                                      // in the shader).
    GpuLight lights[kMaxLights];
};

// Concrete forward pipeline, lit variant (post-Editor-E8, docs/07-unity-parity-analysis.md
// "Lighting / PBR" row's first real step) -- a separate class from every other
// ForwardLit*Pipeline, never a single uber-shader/uber-pipeline with runtime branching
// (CLAUDE.md rule 7). Minimal Blinn-Phong (ambient + N-dot-L diffuse + a fixed-shininess
// specular term), no shadows (phase B, not this pass), no PBR (metallic/roughness stays
// out of scope per CLAUDE.md).
//
// Expects `engine::renderer::Vertex` (MeshLoader.h: position + normal) as its vertex
// input, same as ForwardLitPipeline/ForwardLitColorPipeline. Per-draw push constant is a
// model matrix (not a precomputed MVP -- the frame UBO's viewProj covers that) plus a
// tint color, same combined-range shape ForwardLitColorPipeline already uses. Uniform
// (not non-uniform) scale is assumed on lit entities: world-space normals are transformed
// by `mat3(model)` directly, skipping the inverse-transpose correction a non-uniform
// scale would need -- a documented phase-A simplification, not an oversight.
class ForwardLitShadedPipeline {
public:
    ForwardLitShadedPipeline() = default;
    ~ForwardLitShadedPipeline() = default;

    ForwardLitShadedPipeline(const ForwardLitShadedPipeline&) = delete;
    ForwardLitShadedPipeline& operator=(const ForwardLitShadedPipeline&) = delete;

    bool Init(rhi::RHIContext& context, VkRenderPass renderPass, VkExtent2D viewportExtent,
              const char* vertexShaderPath, const char* fragmentShaderPath);
    void Shutdown();

    void Bind(VkCommandBuffer commandBuffer) const;

    // Per-draw: the entity's own model matrix (not MVP -- see this class's own comment)
    // plus its material's tint. One combined push-constant range, same reasoning
    // ForwardLitColorPipeline::PushMvpAndTint() already gives.
    void PushModelAndTint(VkCommandBuffer commandBuffer, const glm::mat4& model,
                          const glm::vec4& tintColor) const;

    // Per-frame (called once, before drawing any entity through this pipeline that
    // frame) -- binds the caller's own FrameLightingData descriptor set (see that
    // struct's own comment for why this is a UBO, not a push constant).
    void BindFrameDescriptorSet(VkCommandBuffer commandBuffer, VkDescriptorSet descriptorSet) const;

    // For the caller to allocate a matching VkDescriptorSet against (same pattern as
    // ForwardLitTexturedPipeline::GetDescriptorSetLayout()).
    VkDescriptorSetLayout GetDescriptorSetLayout() const {
        return m_pipeline.GetDescriptorSetLayout();
    }

private:
    rhi::RHIPipeline m_pipeline;
};

} // namespace engine::renderer
