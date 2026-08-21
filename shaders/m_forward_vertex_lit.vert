#version 450

// ForwardVertexLitPipeline -- the engine's default lit material (docs/01 section 8.3's
// "Low-Poly Retro" profile explicitly allows either vertex lighting or minimal Blinn-
// Phong; ForwardLitShadedPipeline built the fragment/Blinn-Phong half, this is the
// vertex-lit half). All lighting math happens here, once per vertex, and is interpolated
// across the triangle by the rasterizer (Gouraud shading) -- m_forward_vertex_lit.frag
// just multiplies the interpolated result by the material's tint, no lighting work of its
// own. Same FrameLightingData UBO layout as m_forward_lit_shaded.vert/frag (set = 0,
// binding = 0) -- see ForwardLitShadedPipeline.h's own comment for why this is a UBO
// rather than a push constant, and for the directional-light (-Z forward) convention.

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;

layout(location = 0) out vec3 vLitColor;

layout(push_constant) uniform PushConstants {
    mat4 model;
    vec4 tintColor;
} pc;

struct GpuLight {
    vec4 positionOrDirection;
    vec4 color;
    vec4 params;
};

layout(set = 0, binding = 0) uniform FrameLightingData {
    mat4 viewProj;
    vec4 cameraWorldPosition;
    vec4 ambientAndCount;
    GpuLight lights[4];
} frame;

void main() {
    vec4 worldPosition = pc.model * vec4(inPosition, 1.0);
    // Uniform-scale assumption, same as m_forward_lit_shaded.vert -- see that shader/
    // ForwardLitShadedPipeline.h's own comment.
    vec3 worldNormal = normalize(mat3(pc.model) * inNormal);
    vec3 viewDir = normalize(frame.cameraWorldPosition.xyz - worldPosition.xyz);

    vec3 lit = frame.ambientAndCount.rgb;
    int lightCount = int(frame.ambientAndCount.a);
    for (int i = 0; i < lightCount; ++i) {
        GpuLight light = frame.lights[i];
        vec3 lightDir;
        float attenuation = 1.0;
        if (light.positionOrDirection.w < 0.5) {
            // Directional -- positionOrDirection already points from the light toward the
            // scene (World-space forward), so the vector *to* the light is its negation.
            lightDir = normalize(-light.positionOrDirection.xyz);
        } else {
            vec3 toLight = light.positionOrDirection.xyz - worldPosition.xyz;
            float dist = length(toLight);
            lightDir = toLight / max(dist, 0.0001);
            float range = max(light.params.x, 0.0001);
            float falloff = clamp(1.0 - dist / range, 0.0, 1.0);
            attenuation = falloff * falloff;
        }
        vec3 lightColor = light.color.rgb * light.color.a;
        float diffuseTerm = max(dot(worldNormal, lightDir), 0.0);
        vec3 halfVector = normalize(lightDir + viewDir);
        float specularTerm =
            diffuseTerm > 0.0 ? pow(max(dot(worldNormal, halfVector), 0.0), 32.0) * 0.3 : 0.0;
        lit += (diffuseTerm + specularTerm) * lightColor * attenuation;
    }

    vLitColor = lit;
    gl_Position = frame.viewProj * worldPosition;
}
