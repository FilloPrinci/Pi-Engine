#version 450

// M0 -- Hello Vulkan (docs/03 section 5). No vertex buffer: the 3 positions/colors are
// hardcoded here, indexed by gl_VertexIndex, consistent with M0's "no external assets"
// scope. RHIBuffer/real mesh vertex input arrives in M1.

layout(location = 0) out vec3 vColor;

const vec2 kPositions[3] = vec2[](
    vec2(0.0, -0.5),
    vec2(0.5, 0.5),
    vec2(-0.5, 0.5)
);

const vec3 kColors[3] = vec3[](
    vec3(1.0, 0.0, 0.0),
    vec3(0.0, 1.0, 0.0),
    vec3(0.0, 0.0, 1.0)
);

void main() {
    gl_Position = vec4(kPositions[gl_VertexIndex], 0.0, 1.0);
    vColor = kColors[gl_VertexIndex];
}
