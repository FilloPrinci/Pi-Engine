#include "engine/renderer/ShaderPropertySchema.h"

namespace engine::renderer {

const std::vector<MaterialShaderInfo>& GetMaterialShaderRegistry() {
    // Hand-written, one entry per concrete material-compatible pipeline class -- see this
    // header's own comment for why this can never be inferred/generated automatically.
    static const std::vector<MaterialShaderInfo> registry = {
        {
            "ForwardLitColor",
            {
                {"tintColor", ShaderPropertyType::Color, glm::vec4(1.0f, 1.0f, 1.0f, 1.0f), 0.0f},
            },
        },
        {
            "ForwardLitTexturedColor",
            {
                {"tintColor", ShaderPropertyType::Color, glm::vec4(1.0f, 1.0f, 1.0f, 1.0f), 0.0f},
                {"albedoTexture", ShaderPropertyType::Texture, glm::vec4(1.0f, 1.0f, 1.0f, 1.0f),
                 0.0f},
            },
        },
        {
            // Lighting phase A (docs/01 section 8.3) -- "tintColor" here plays the same
            // role ForwardLitColor's own tintColor does (the entity's base/albedo color,
            // multiplied by ambient + each light's diffuse/specular contribution in the
            // fragment shader) -- no lighting-specific properties yet (a shininess/
            // specular-intensity Float property is a natural future addition, not added
            // speculatively -- see m_forward_lit_shaded.frag's own comment).
            "ForwardLitShaded",
            {
                {"tintColor", ShaderPropertyType::Color, glm::vec4(1.0f, 1.0f, 1.0f, 1.0f), 0.0f},
            },
        },
        {
            // The engine's default/base lit material (the user's own explicit request) --
            // same lighting formula and same "tintColor" role as ForwardLitShaded, just
            // evaluated per-vertex instead of per-fragment (ForwardVertexLitPipeline.h's
            // own comment). Flat tint only, no texture -- see
            // "ForwardVertexLitTextured" below for the texture-supporting sibling.
            "ForwardVertexLit",
            {
                {"tintColor", ShaderPropertyType::Color, glm::vec4(1.0f, 1.0f, 1.0f, 1.0f), 0.0f},
            },
        },
        {
            // Texture-supporting sibling of "ForwardVertexLit" above -- same split
            // ForwardLitColor/ForwardLitTexturedColor already established. When
            // "albedoTexture" isn't assigned, the material should target
            // "ForwardVertexLit" instead rather than leaving this property empty (same
            // "switch shaderName, don't toggle a flag" convention every other
            // texture/no-texture shader pair in this registry already follows).
            "ForwardVertexLitTextured",
            {
                {"tintColor", ShaderPropertyType::Color, glm::vec4(1.0f, 1.0f, 1.0f, 1.0f), 0.0f},
                {"albedoTexture", ShaderPropertyType::Texture, glm::vec4(1.0f, 1.0f, 1.0f, 1.0f),
                 0.0f},
            },
        },
    };
    return registry;
}

const MaterialShaderInfo* FindMaterialShader(const std::string& name) {
    for (const MaterialShaderInfo& shader : GetMaterialShaderRegistry()) {
        if (shader.name == name) {
            return &shader;
        }
    }
    return nullptr;
}

} // namespace engine::renderer
