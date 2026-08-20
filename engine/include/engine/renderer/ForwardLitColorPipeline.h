#pragma once

#include "engine/rhi/RHIPipeline.h"

#include <glm/glm.hpp>
#include <volk.h>

namespace engine::rhi {
class RHIContext;
}

namespace engine::renderer {

// Concrete forward pipeline, flat-tint-color variant (post-Editor-E8, material assets
// v1 -- see MaterialData.h's own comment for the full "why"). A separate class from
// ForwardLitPipeline (M1's debug normal-color visualization, left untouched) and
// ForwardLitTexturedPipeline (M7 textures) -- never a single uber-shader/uber-pipeline
// with runtime branching (CLAUDE.md rule 7). No lighting, no descriptor sets/textures --
// just position transform + an entirely data-driven output color, the whole point of a
// material asset for this engine's still-unlit-only rendering stage.
//
// Expects `engine::renderer::Vertex` (MeshLoader.h: position + normal + uv) as its vertex
// input, same as every other pipeline in this project, even though the normal/uv
// attributes themselves go unused here -- one shared cooked mesh format, no
// material-specific vertex layout to keep in sync separately.
class ForwardLitColorPipeline {
public:
    ForwardLitColorPipeline() = default;
    ~ForwardLitColorPipeline() = default;

    ForwardLitColorPipeline(const ForwardLitColorPipeline&) = delete;
    ForwardLitColorPipeline& operator=(const ForwardLitColorPipeline&) = delete;

    bool Init(rhi::RHIContext& context, VkRenderPass renderPass, VkExtent2D viewportExtent,
              const char* vertexShaderPath, const char* fragmentShaderPath);
    void Shutdown();

    void Bind(VkCommandBuffer commandBuffer) const;

    // Single combined push constant (mat4 mvp + vec4 tintColor, 80 bytes total, visible to
    // both the vertex and fragment stage -- see m_material_color.vert/frag's own layout)
    // instead of two separate PushX() calls, since both values change together every draw
    // anyway (one call per entity, not two).
    void PushMvpAndTint(VkCommandBuffer commandBuffer, const glm::mat4& mvp,
                        const glm::vec4& tintColor) const;

private:
    rhi::RHIPipeline m_pipeline;
};

} // namespace engine::renderer
