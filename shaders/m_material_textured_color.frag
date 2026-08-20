#version 450

// Material assets, "ForwardLitTexturedColor" shader (post-Editor-E8,
// ShaderPropertySchema.h) -- samples the bound "albedoTexture" property, then multiplies
// by the "tintColor" property. No lighting math (this engine is still unlit-only
// everywhere, see m_material_color.frag's identical comment).

layout(location = 0) in vec2 vUV;
layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 0) uniform sampler2D uAlbedoTexture;

layout(push_constant) uniform PushConstants {
    mat4 mvp; // unused here, vertex-stage only.
    vec4 tintColor;
} pc;

void main() {
    outColor = texture(uAlbedoTexture, vUV) * pc.tintColor;
}
