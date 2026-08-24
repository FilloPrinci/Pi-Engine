#version 450

// ShadowDepthPipeline -- renders into RHIShadowMap's depth attachment from the light's
// own view-projection. Only cares about clip-space position; no varyings out at all.

layout(location = 0) in vec3 inPosition;

layout(push_constant) uniform PushConstants {
    mat4 mvp; // light.viewProj * model, combined on the CPU (this pipeline's own header
              // comment explains why there's no per-frame UBO to split it out of here).
} pc;

void main() {
    gl_Position = pc.mvp * vec4(inPosition, 1.0);
}
