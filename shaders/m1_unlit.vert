#version 450

// M1 -- Hello Mesh (docs/03 section 6). Unlit: no lighting math, the fragment shader just
// visualizes the interpolated normal as a color -- cheap and immediately shows whether
// geometry/winding/normals came through MeshLoader correctly.

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;

layout(location = 0) out vec3 vNormal;

layout(push_constant) uniform PushConstants {
    mat4 mvp;
} pc;

void main() {
    gl_Position = pc.mvp * vec4(inPosition, 1.0);
    vNormal = inNormal;
}
