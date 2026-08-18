#pragma once

#include "engine/rhi/RHIPipeline.h"

#include <glm/glm.hpp>
#include <volk.h>

namespace engine::rhi {
class RHIContext;
}

namespace engine::renderer {

// Concrete forward pipeline, unlit variant only for M1 (docs/03 section 6) -- lighting
// arrives when a milestone actually needs it, doesn't block this one. A separate,
// specialized class from ForwardPlusPBRPipeline (out of scope through M5), never a
// single uber-shader/uber-pipeline with runtime branching (CLAUDE.md rule 7).
//
// Expects `engine::renderer::Vertex` (MeshLoader.h: position + normal) as its vertex
// input and a single push-constant mat4 model-view-projection matrix -- no descriptor
// sets, no textures, matching the "no materials yet" scope of M1's MeshLoader.
class ForwardLitPipeline {
public:
    ForwardLitPipeline() = default;
    ~ForwardLitPipeline() = default;

    ForwardLitPipeline(const ForwardLitPipeline&) = delete;
    ForwardLitPipeline& operator=(const ForwardLitPipeline&) = delete;

    // `context` must already be initialized and must outlive this pipeline. Shader paths
    // are the sample's responsibility (as in M0, see samples/m0_hello_vulkan/CMakeLists.txt)
    // -- engine_core stays decoupled from any one sample's build output layout.
    bool Init(rhi::RHIContext& context, VkRenderPass renderPass, VkExtent2D viewportExtent,
              const char* vertexShaderPath, const char* fragmentShaderPath);
    void Shutdown();

    void Bind(VkCommandBuffer commandBuffer) const;
    void PushModelViewProjection(VkCommandBuffer commandBuffer, const glm::mat4& mvp) const;

private:
    rhi::RHIPipeline m_pipeline;
};

} // namespace engine::renderer
