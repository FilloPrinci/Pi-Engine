#version 450

// Material assets, v1 (flat tint color, post-Editor-E8, docs/07-unity-parity-analysis.md)
// -- unlike m1_unlit.vert/frag (M1's debug normal-color visualization, left untouched),
// this pipeline doesn't care about normals at all, it just transforms position. The tint
// itself is entirely a fragment-stage concern (see m_material_color.frag).

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal; // unused here, but ForwardLitColorPipeline still
                                        // expects renderer::Vertex's full layout so it can
                                        // draw the exact same cooked meshes every other
                                        // pipeline does, no separate vertex format needed.

layout(push_constant) uniform PushConstants {
    mat4 mvp;
    vec4 tintColor;
} pc;

void main() {
    gl_Position = pc.mvp * vec4(inPosition, 1.0);
}
