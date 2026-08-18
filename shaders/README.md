# shaders

GLSL sources, compiled to SPIR-V at build time by a custom CMake step (not the Cooker yet
-- docs/03 section 5). Requires `glslc` (from the Vulkan SDK) on `PATH` or discoverable via
the `VULKAN_SDK` environment variable; see the root `README.md` prerequisites.

- `m0_triangle.vert` / `m0_triangle.frag` -- M0's first minimal pipeline: 3 hardcoded
  positions/colors indexed by `gl_VertexIndex`, no vertex buffer, no external mesh asset.
- `m1_unlit.vert` / `m1_unlit.frag` -- M1's `ForwardLitPipeline` (unlit variant): MVP push
  constant, visualizes the interpolated vertex normal as color (no lighting math yet).

Only the shader variants relevant to the render pipeline chosen for the project compile
(Low-Poly Retro vs PBR, docs/01 section 8.5) -- not both, once that selection exists.
