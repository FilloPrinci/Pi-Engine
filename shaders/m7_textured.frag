#version 450

// M7 -- Asset Pipeline textures step. Samples the bound combined-image-sampler directly --
// no lighting math yet (see m7_textured.vert's comment).

layout(location = 0) in vec2 vUV;
layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 0) uniform sampler2D uTexture;

void main() {
    outColor = texture(uTexture, vUV);
}
