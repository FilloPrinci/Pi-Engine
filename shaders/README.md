# shaders

GLSL sources, compiled to SPIR-V at build time (custom CMake step, not the Cooker yet —
docs/03 section 5). `*.spv` output is gitignored.

- `m0_triangle.vert` / `m0_triangle.frag` — arrive with M0, first minimal pipeline
  (inline data, no external mesh asset).

Only the shader variants relevant to the render pipeline chosen for the project compile
(Low-Poly Retro vs PBR, docs/01 section 8.5) — not both, once that selection exists.
