#version 450

// Lighting phase A (post-Editor-E8, docs/01 section 8.3's "Low-Poly Retro" profile) --
// see ForwardLitShadedPipeline.h's own comment for the full design. Unlike every other
// pipeline in this project, this one does NOT receive a precomputed MVP: the frame UBO's
// viewProj is multiplied here against a per-draw model matrix instead, since viewProj is
// shared scene-wide data (one value per frame) and model is genuinely per-draw.

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;

layout(location = 0) out vec3 vWorldPosition;
layout(location = 1) out vec3 vWorldNormal;

layout(push_constant) uniform PushConstants {
    mat4 model;
    vec4 tintColor; // unused here, fragment-stage only.
} pc;

struct Light {
    vec4 positionOrDirection;
    vec4 color;
    vec4 params;
};

layout(set = 0, binding = 0) uniform FrameLightingData {
    mat4 viewProj;
    vec4 cameraWorldPosition;
    vec4 ambientAndCount;
    Light lights[4];
} frame;

void main() {
    vec4 worldPosition = pc.model * vec4(inPosition, 1.0);
    vWorldPosition = worldPosition.xyz;
    // Uniform-scale assumption (ForwardLitShadedPipeline.h's own comment) -- a full
    // inverse-transpose normal matrix isn't computed here on purpose.
    vWorldNormal = mat3(pc.model) * inNormal;
    gl_Position = frame.viewProj * worldPosition;
}
