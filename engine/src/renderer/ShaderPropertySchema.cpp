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
