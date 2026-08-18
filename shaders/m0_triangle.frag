#version 450

// M0 -- Hello Vulkan (docs/03 section 5). Trivial passthrough: outputs the color
// interpolated across the triangle by the rasterizer.

layout(location = 0) in vec3 vColor;
layout(location = 0) out vec4 outColor;

void main() {
    outColor = vec4(vColor, 1.0);
}
