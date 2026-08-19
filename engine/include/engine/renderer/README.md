# renderer

- `MeshLoader.h` + `.cpp` -- done (M1, extended M7): glTF via cgltf, raw vertex/index
  buffers. POSITION + NORMAL + optional TEXCOORD_0 (M7 textures step; defaults to (0,0)
  if the source has none), single primitive -- no materials/skinning yet. From M6 on,
  this is offline-only: `tools/cooker` compiles this exact file directly as one of its
  own sources (not linked via `engine_core`, see that tool's `CMakeLists.txt`) to read the
  glTF source; no sample calls `LoadMesh()` at runtime anymore, they all load the cooked
  output instead. `LoadMesh()` itself still only ever produces one "LOD0" mesh --
  vertex-cache optimization and LOD generation both happen downstream, in
  `tools/cooker/CookMesh.cpp`, via meshoptimizer (M7 LOD-generation step).
- `CookedMesh.h` + `.cpp` -- done (M6, extended M7): the minimal binary format
  `tools/cooker` writes and `LoadCookedMesh()` reads back -- "the shipped game only loads
  Cooked Assets" (docs/01 section 12.1) starting with meshes. Format version 2 embeds the
  source asset's `asset::AssetGuid` in the header (`LoadCookedMesh()`'s `outGuid`
  parameter is optional since nothing resolves an asset *by* GUID at runtime yet); version
  3 (textures step) grew `Vertex` by a `uv` field; version 4 (LOD-generation step)
  replaced the single top-level index buffer with `lodCount` index buffers sharing one
  vertex buffer (`WriteCookedMeshLODs()`/`LoadCookedMesh()`'s `lodIndex` parameter,
  default 0 -- the highest detail, so every pre-LOD call site is unaffected;
  `GetCookedMeshLODCount()` reports how many exist). Still no per-hardware-profile
  variants (no Hardware Profile System built yet to pick a LOD by distance/tier at
  runtime -- `samples/m7_lod` only demonstrates manual switching) or compression -- see
  the header's own comment for why each remaining piece is deferred.
- `CookedTexture.h` + `.cpp` -- done (M7, textures step): mirrors `CookedMesh.h` for
  textures -- the binary format `cooker texture` writes and `LoadCookedTexture()` reads
  back. Always tightly packed RGBA8, no mipmaps, no block compression (ETC2/BC7, docs/01
  section 12.2) -- deliberately minimal, same reasoning as `CookedMesh.h`.
- `ForwardLitPipeline.h` + `.cpp` -- done (M1): unlit variant only; lighting added when
  needed. `ForwardPlusPBRPipeline` is out of scope for M0-M5 (docs/02 section 5).
- `ForwardLitTexturedPipeline.h` + `.cpp` -- done (M7, textures step): a separate concrete
  pipeline class from `ForwardLitPipeline` (CLAUDE.md rule 7, never an uber-shader with
  branching) -- same push-constant MVP, plus a single combined-image-sampler descriptor
  set (set = 0, binding = 0, fragment stage). Owns only the pipeline/descriptor-set
  *layout*; the descriptor pool and the `VkDescriptorSet` itself are the caller's
  responsibility (no material system exists yet to own them, see
  `samples/m7_textures/main.cpp`).
- `FrustumCuller.h` + `.cpp` -- done (M2): first system to submit real jobs to the Job
  System -- Gribb/Hartmann plane extraction + parallel bounding-sphere test per entity,
  writes `MeshComponent::visible`.

Two concrete pipeline classes so far, never an uber-shader with branching (CLAUDE.md
rule 7).
