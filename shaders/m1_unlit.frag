#version 450

// M1 -- Hello Mesh (docs/03 section 6). Debug-visualizes the (unnormalized-across-the-
// triangle, since it's linearly interpolated) vertex normal as an RGB color -- each cube
// face ends up a distinct flat color, an easy visual check that geometry/normals loaded
// correctly without needing any actual lighting yet.

layout(location = 0) in vec3 vNormal;
layout(location = 0) out vec4 outColor;

void main() {
    outColor = vec4(abs(normalize(vNormal)), 1.0);
}
