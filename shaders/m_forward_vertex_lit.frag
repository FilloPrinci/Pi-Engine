#version 450

// Flat-tint albedo, no texture -- see ForwardVertexLitTexturedPipeline/
// m_forward_vertex_lit_textured.frag for the texture-supporting sibling. All lighting
// already happened in m_forward_vertex_lit.vert; this stage only applies the material's
// own tint on top of the interpolated per-vertex lit color.

layout(location = 0) in vec3 vLitColor;

layout(location = 0) out vec4 outColor;

layout(push_constant) uniform PushConstants {
    mat4 model;
    vec4 tintColor;
} pc;

void main() {
    outColor = vec4(vLitColor * pc.tintColor.rgb, pc.tintColor.a);
}
