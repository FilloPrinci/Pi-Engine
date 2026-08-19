# shaders

GLSL sources, compiled to SPIR-V at build time by the Cooker (`tools/cooker`'s `shader`
subcommand, via the `shaderc` library -- docs/03 section 5, docs/01 section 12.4). Wired
into the main build via `cmake/CookAssets.cmake`'s shared `cooked_shaders` target, output
to `<build dir>/assets_cooked/shaders/*.spv`. No local `glslc`/Vulkan SDK install is
needed anymore -- `shaderc` is a vcpkg dependency of the `cooker` tool itself.

- `m0_triangle.vert` / `m0_triangle.frag` -- M0's first minimal pipeline: 3 hardcoded
  positions/colors indexed by `gl_VertexIndex`, no vertex buffer, no external mesh asset.
- `m1_unlit.vert` / `m1_unlit.frag` -- M1's `ForwardLitPipeline` (unlit variant): MVP push
  constant, visualizes the interpolated vertex normal as color (no lighting math yet).

Only the shader variants relevant to the render pipeline chosen for the project compile
(Low-Poly Retro vs PBR, docs/01 section 8.5) -- not both, once that selection exists.
