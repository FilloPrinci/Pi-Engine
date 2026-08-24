#pragma once

#include "engine/rhi/RHIPipeline.h"

#include <glm/glm.hpp>
#include <volk.h>

namespace engine::rhi {
class RHIContext;
}

namespace engine::renderer {

// Lighting phase B (docs/01 section 8.3's "preferably baked" static shadow map) --
// renders scene geometry into an `rhi::RHIShadowMap`'s depth attachment from the light's
// own view-projection instead of the camera's. A separate concrete pipeline (CLAUDE.md
// rule 7) from every lit/unlit forward pipeline: depth-only (no color attachment at all,
// matching `RHIShadowMap`'s own depth-only render pass), no descriptor sets, a single
// `mat4 mvp` push constant already combining the light's view-projection with the
// entity's own model matrix (computed on the CPU per draw -- there's no per-frame UBO to
// pull `viewProj` from here the way the lit pipelines' own frame UBO does, since the
// light's view-projection is fixed for as long as the bake stays valid, not something
// worth a whole UBO for a handful of draws).
//
// Expects `engine::renderer::Vertex` (MeshLoader.h) as its vertex input but only ever
// reads the `position` attribute -- normal/uv are irrelevant to a depth-only pass, so
// they're simply never declared, even though the same interleaved vertex buffer every
// other pipeline uses is bound here too (the vertex *binding*'s stride still matches the
// full `Vertex` struct; only the *attribute* list is shorter).
class ShadowDepthPipeline {
public:
    ShadowDepthPipeline() = default;
    ~ShadowDepthPipeline() = default;

    ShadowDepthPipeline(const ShadowDepthPipeline&) = delete;
    ShadowDepthPipeline& operator=(const ShadowDepthPipeline&) = delete;

    bool Init(rhi::RHIContext& context, VkRenderPass renderPass, VkExtent2D viewportExtent,
              const char* vertexShaderPath, const char* fragmentShaderPath);
    void Shutdown();

    void Bind(VkCommandBuffer commandBuffer) const;
    void PushMvp(VkCommandBuffer commandBuffer, const glm::mat4& mvp) const;

private:
    rhi::RHIPipeline m_pipeline;
};

} // namespace engine::renderer
