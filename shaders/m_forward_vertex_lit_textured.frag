#version 450

// Albedo = sampled texel * tint (same order m_material_textured_color.frag already uses),
// then multiplied by the interpolated per-vertex lit color from
// m_forward_vertex_lit_textured.vert. set = 1 (not set = 0, which is the shared per-frame
// lighting UBO every ForwardVertexLit*/ForwardLitShaded shader binds) -- rebound per draw
// by the caller, same division ForwardLitTexturedColorPipeline already established for its
// own single set.

layout(location = 0) in vec3 vLitColor;
layout(location = 1) in vec2 vUV;

layout(location = 0) out vec4 outColor;

layout(push_constant) uniform PushConstants {
    mat4 model;
    vec4 tintColor;
} pc;

layout(set = 1, binding = 0) uniform sampler2D uAlbedoTexture;

void main() {
    vec4 albedo = texture(uAlbedoTexture, vUV) * pc.tintColor;
    outColor = vec4(albedo.rgb * vLitColor, albedo.a);
}
