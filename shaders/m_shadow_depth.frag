#version 450

// Depth-only pass -- no color attachment exists to write to (RHIShadowMap's own render
// pass), so this stage genuinely does nothing but exist (a graphics pipeline still needs
// a fragment shader stage even when only depth is wanted).

void main() {
}
