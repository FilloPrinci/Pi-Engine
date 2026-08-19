# renderer

- `MeshLoader.h` + `.cpp` -- done (M1): glTF via cgltf, raw vertex/index buffers.
  POSITION + NORMAL only, single primitive -- no materials/textures/skinning yet. From M6
  on, this is offline-only: `tools/cooker` compiles this exact file directly as one of its
  own sources (not linked via `engine_core`, see that tool's `CMakeLists.txt`) to read the
  glTF source; no sample calls `LoadMesh()` at runtime anymore, they all load the cooked
  output instead (cache-vertex optimization via meshoptimizer is still deferred, docs/01
  section 12.2).
- `CookedMesh.h` + `.cpp` -- done (M6, extended M7): the minimal binary format
  `tools/cooker` writes and `LoadCookedMesh()` reads back -- "the shipped game only loads
  Cooked Assets" (docs/01 section 12.1) starting with meshes. Format version 2 (M7) embeds
  the source asset's `asset::AssetGuid` in the header; `LoadCookedMesh()`'s `outGuid`
  parameter is optional since nothing resolves an asset *by* GUID at runtime yet. Still no
  vertex-cache optimization, LOD, per-hardware-profile variants, or compression -- see the
  header's own comment for why each is deferred.
- `ForwardLitPipeline.h` + `.cpp` -- done (M1): unlit variant only; lighting added when
  needed. `ForwardPlusPBRPipeline` is out of scope for M0-M5 (docs/02 section 5).
- `FrustumCuller.h` + `.cpp` -- done (M2): first system to submit real jobs to the Job
  System -- Gribb/Hartmann plane extraction + parallel bounding-sphere test per entity,
  writes `MeshComponent::visible`.

Two concrete pipeline classes, never an uber-shader with branching (CLAUDE.md rule 7).
