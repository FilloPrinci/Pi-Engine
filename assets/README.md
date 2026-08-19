# assets

Raw source assets (docs/01 section 12.1: "what the developer/artist produces and puts
under version control"). Through M5 these were loaded directly at runtime; from M6 on,
`tools/cooker` converts them into `assets_cooked/` (build-directory output, not checked
in, see `cmake/CookAssets.cmake`) at build time, and the samples load *that* instead --
see `engine/include/engine/renderer/README.md`.

- `m1_cube.glb` — done (M1, docs/03 section 6): test mesh for MeshLoader/cooker. A
  hand-generated unit cube (24 vertices/36 indices, position+normal, no shared vertices
  across faces so each face keeps a flat normal), not exported from a DCC tool -- only a
  minimal known-good triangle list was needed.
- `m1_cube.glb.meta` — done (M7, docs/01 section 12.3): sidecar carrying `m1_cube.glb`'s
  persistent Asset GUID, written once by `tools/cooker` the first time it cooked the
  source and committed alongside it from then on (unlike `assets_cooked/`, a `.meta` file
  *is* checked into git -- it's what makes the id survive the source file being renamed or
  moved). Never edited by hand.
- `m7_quad.gltf` (+ `.meta`) — done (M7, textures step): a hand-authored quad (4 vertices,
  2 triangles) with POSITION+NORMAL+TEXCOORD_0, embedded as a base64 data-URI buffer
  (plain JSON text, no external `.bin`) -- `m1_cube.glb` predates TEXCOORD_0 support and
  wasn't worth regenerating without a DCC tool in the loop. Used only by
  `samples/m7_textures`.
- `m7_checker.png` (+ `.meta`) — done (M7, textures step): a small (64x64) hand-generated
  RGBA checkerboard, test input for `cooker texture` / `samples/m7_textures` -- visually
  confirms UV mapping/sampling orientation at a glance. Not art, just a debug pattern.
- `m7_lod_sphere.gltf` (+ `.meta`) — done (M7, LOD-generation step): a hand-generated
  subdivided icosphere (642 vertices, 1280 triangles, POSITION+NORMAL, embedded base64
  buffer like `m7_quad.gltf`) -- `m1_cube.glb`/`m7_quad.gltf` are both far too simple to
  show a visible difference between `cooker mesh`'s generated LOD levels. Used only by
  `samples/m7_lod`.

Everything here stays git-friendly source (glTF/GLB, PNG/TGA, WAV, `.meta` sidecars) — no
cooked/compressed binaries belong in this repo (`assets_cooked/` is gitignored, generated
by the build).
