#version 450

// Lighting phase A (post-Editor-E8, docs/01 section 8.3) -- minimal Blinn-Phong: a flat
// ambient term plus, per active light, an N-dot-L diffuse term and a fixed-shininess
// specular term. No shadows (phase B, a separate static shadow map, not this pass), no
// PBR metallic/roughness (CLAUDE.md keeps the "PBR profile" out of scope) -- this is the
// "Low-Poly Retro" profile's own lighting model, not a scaled-down PBR one.

layout(location = 0) in vec3 vWorldPosition;
layout(location = 1) in vec3 vWorldNormal;
layout(location = 0) out vec4 outColor;

layout(push_constant) uniform PushConstants {
    mat4 model; // unused here, vertex-stage only.
    vec4 tintColor;
} pc;

struct Light {
    // xyz: world-space direction (Directional, already pointing *from* the light) or
    // world-space position (Point). w: 0 = Directional, 1 = Point (see GpuLight's own
    // comment for why this doubles up rather than a separate int field).
    vec4 positionOrDirection;
    vec4 color; // rgb = color, a = intensity.
    vec4 params; // x = range (Point only), yzw reserved.
};

layout(set = 0, binding = 0) uniform FrameLightingData {
    mat4 viewProj; // unused here, vertex-stage only.
    mat4 lightViewProj; // lighting phase B -- the baked shadow-casting light's own
                        // view-projection, used below to look up the shadow map.
    // w = index of the shadow-casting light within `lights[]` below, or -1 if none
    // qualifies (FrameLightingData's own comment, ForwardLitShadedPipeline.h).
    vec4 cameraWorldPosition;
    vec4 ambientAndCount; // rgb = ambient color, a = active light count.
    Light lights[4];
} frame;

// Lighting phase B -- comparison sampler (RHIShadowMap's own, hardware bilinear PCF, see
// that class's header comment). Same set = 0 as the frame UBO above (this pipeline's own
// header comment on why), a "shadow" GLSL sampler type: texture() takes a compare depth
// as the last component and returns an already-filtered 0..1 result, not a raw color.
layout(set = 0, binding = 1) uniform sampler2DShadow uShadowMap;

// Fixed for phase A -- no per-material shininess/specular-intensity property yet (the
// generic material property system, ShaderPropertySchema.h, could grow a Float property
// for this later if a real shader ever needs to vary it; not added speculatively).
const float kShininess = 32.0;
const float kSpecularIntensity = 0.3;

// Lighting phase B -- 1.0 = fully lit, 0.0 = fully shadowed (RHIShadowMap's own hardware
// PCF blends smoothly between the two at shadow edges). `bias` pushes the compared depth
// slightly *toward* the light to avoid shadow acne (self-shadowing artifacts from the
// shadow map's own finite resolution) -- slope-scaled by N-dot-L so grazing-angle surfaces
// (where acne is worst) get a larger bias than surfaces facing the light head-on.
float ComputeShadow(vec3 worldPosition, vec3 N, vec3 L) {
    vec4 lightSpace = frame.lightViewProj * vec4(worldPosition, 1.0);
    vec3 projected = lightSpace.xyz / lightSpace.w;
    vec2 shadowUV = projected.xy * 0.5 + 0.5;
    float bias = max(0.0025 * (1.0 - dot(N, L)), 0.0006);
    return texture(uShadowMap, vec3(shadowUV, projected.z - bias));
}

void main() {
    vec3 N = normalize(vWorldNormal);
    vec3 V = normalize(frame.cameraWorldPosition.xyz - vWorldPosition);
    int shadowLightIndex = int(frame.cameraWorldPosition.w);

    vec3 result = frame.ambientAndCount.rgb * pc.tintColor.rgb;

    int lightCount = int(frame.ambientAndCount.a);
    for (int i = 0; i < lightCount; ++i) {
        Light light = frame.lights[i];
        vec3 L;
        float attenuation = 1.0;

        if (light.positionOrDirection.w < 0.5) {
            // Directional -- positionOrDirection.xyz already points *from* the light
            // toward the scene, so the direction *to* the light is its negation.
            L = normalize(-light.positionOrDirection.xyz);
        } else {
            // Point -- inverse-square-ish falloff clamped to zero at `range`, squared for
            // a softer, more physically-plausible-looking edge than a linear ramp.
            vec3 toLight = light.positionOrDirection.xyz - vWorldPosition;
            float distance = length(toLight);
            L = toLight / max(distance, 0.0001);
            float range = max(light.params.x, 0.0001);
            float falloff = clamp(1.0 - (distance / range), 0.0, 1.0);
            attenuation = falloff * falloff;
        }

        // Lighting phase B -- only the one baked light (if any) is ever shadowed; every
        // other light stays fully lit, matching this pass's own directional-only scope
        // (a shadow baked from one directional light's viewpoint has no meaningful
        // relationship to a different light's own occlusion).
        float shadow = i == shadowLightIndex ? ComputeShadow(vWorldPosition, N, L) : 1.0;

        vec3 lightColor = light.color.rgb * light.color.a;
        float diffuseTerm = max(dot(N, L), 0.0);
        vec3 halfVector = normalize(L + V);
        float specularTerm =
            diffuseTerm > 0.0 ? pow(max(dot(N, halfVector), 0.0), kShininess) * kSpecularIntensity
                              : 0.0;

        result += (diffuseTerm * pc.tintColor.rgb + specularTerm) * lightColor * attenuation * shadow;
    }

    outColor = vec4(result, pc.tintColor.a);
}
