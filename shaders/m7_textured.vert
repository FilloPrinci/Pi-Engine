#version 450

// M7 -- Asset Pipeline textures step. Textured-unlit: passes UV through, no lighting math
// (matches m1_unlit's "prove the pipeline first" scope, just texture-sampled instead of
// normal-visualized).

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inUV;

layout(location = 0) out vec2 vUV;

layout(push_constant) uniform PushConstants {
    mat4 mvp;
} pc;

void main() {
    gl_Position = pc.mvp * vec4(inPosition, 1.0);
    vUV = inUV;
}
