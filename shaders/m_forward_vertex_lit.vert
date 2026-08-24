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
//
// Lighting phase B -- the static shadow map lookup happens here too (per-vertex, matching
// the rest of this shader's own lighting), not in the fragment shader -- see
// ForwardVertexLitPipeline.h's own comment for why the descriptor binding still declares
// both stages regardless.

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
    mat4 lightViewProj; // lighting phase B -- the baked shadow-casting light's own
                        // view-projection.
    // w = index of the shadow-casting light within `lights[]` below, or -1 if none
    // qualifies (FrameLightingData's own comment, ForwardLitShadedPipeline.h).
    vec4 cameraWorldPosition;
    vec4 ambientAndCount;
    GpuLight lights[4];
} frame;

// Lighting phase B -- same comparison sampler/binding shape as
// m_forward_lit_shaded.frag's own uShadowMap; see that shader's comment for what
// texture() returns for a "shadow" sampler type.
layout(set = 0, binding = 1) uniform sampler2DShadow uShadowMap;

float ComputeShadow(vec3 worldPosition, vec3 N, vec3 L) {
    vec4 lightSpace = frame.lightViewProj * vec4(worldPosition, 1.0);
    vec3 projected = lightSpace.xyz / lightSpace.w;
    vec2 shadowUV = projected.xy * 0.5 + 0.5;
    float bias = max(0.0025 * (1.0 - dot(N, L)), 0.0006);
    return textureLod(uShadowMap, vec3(shadowUV, projected.z - bias), 0.0);
}

void main() {
    vec4 worldPosition = pc.model * vec4(inPosition, 1.0);
    // Uniform-scale assumption, same as m_forward_lit_shaded.vert -- see that shader/
    // ForwardLitShadedPipeline.h's own comment.
    vec3 worldNormal = normalize(mat3(pc.model) * inNormal);
    vec3 viewDir = normalize(frame.cameraWorldPosition.xyz - worldPosition.xyz);
    int shadowLightIndex = int(frame.cameraWorldPosition.w);

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
        // Lighting phase B -- only the one baked light (if any) is shadowed, same
        // reasoning m_forward_lit_shaded.frag's own comment gives.
        float shadow = i == shadowLightIndex ? ComputeShadow(worldPosition.xyz, worldNormal, lightDir) : 1.0;
        vec3 lightColor = light.color.rgb * light.color.a;
        float diffuseTerm = max(dot(worldNormal, lightDir), 0.0);
        vec3 halfVector = normalize(lightDir + viewDir);
        float specularTerm =
            diffuseTerm > 0.0 ? pow(max(dot(worldNormal, halfVector), 0.0), 32.0) * 0.3 : 0.0;
        lit += (diffuseTerm + specularTerm) * lightColor * attenuation * shadow;
    }

    vLitColor = lit;
    gl_Position = frame.viewProj * worldPosition;
}
