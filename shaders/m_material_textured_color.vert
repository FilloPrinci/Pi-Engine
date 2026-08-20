#version 450

// Material assets, "ForwardLitTexturedColor" shader (post-Editor-E8,
// ShaderPropertySchema.h) -- combines m7_textured.vert's UV pass-through with
// m_material_color.vert's combined mvp+tintColor push-constant block (tintColor is a
// fragment-stage-only concern, see m_material_textured_color.frag).

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal; // unused, kept for the shared Vertex layout.
layout(location = 2) in vec2 inUV;

layout(location = 0) out vec2 vUV;

layout(push_constant) uniform PushConstants {
    mat4 mvp;
    vec4 tintColor; // unused here, fragment-stage only.
} pc;

void main() {
    gl_Position = pc.mvp * vec4(inPosition, 1.0);
    vUV = inUV;
}
