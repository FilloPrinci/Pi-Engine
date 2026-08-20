#version 450

// Material assets, v1 (flat tint color, post-Editor-E8, docs/07-unity-parity-analysis.md).
// No lighting math -- this engine's rendering is still unlit-only everywhere (docs/01
// section 1's PBR profile is an explicit, not-yet-started optional secondary target), so
// a "material" here just means "a per-entity color picked from data instead of hardcoded
// in a shader", the same rendering sophistication every other pipeline in this project has
// so far.

layout(location = 0) out vec4 outColor;

layout(push_constant) uniform PushConstants {
    mat4 mvp;
    vec4 tintColor;
} pc;

void main() {
    outColor = pc.tintColor;
}
