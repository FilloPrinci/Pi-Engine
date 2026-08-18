# renderer

- `MeshLoader.h` + `.cpp` -- done (M1): glTF via cgltf, raw vertex/index buffers
  (cache-vertex optimization is Cooker work, post-vertical-slice, docs/01 section 12).
  POSITION + NORMAL only, single primitive -- no materials/textures/skinning yet.
- `ForwardLitPipeline.h` + `.cpp` -- done (M1): unlit variant only; lighting added when
  needed. `ForwardPlusPBRPipeline` is out of scope for M0-M5 (docs/02 section 5).
- `FrustumCuller.h` + `.cpp` -- done (M2): first system to submit real jobs to the Job
  System -- Gribb/Hartmann plane extraction + parallel bounding-sphere test per entity,
  writes `MeshComponent::visible`.

Two concrete pipeline classes, never an uber-shader with branching (CLAUDE.md rule 7).
