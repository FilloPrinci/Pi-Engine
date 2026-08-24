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
- `ShaderPropertySchema.h` + `.cpp` -- done (post-Editor-E8, extending `docs/07-unity-
  parity-analysis.md`'s former #5 priority item from a fixed tint to a genuinely generic
  material property system, per explicit follow-up direction: "a material is an instance
  of a shader -- whatever properties the shader declares should be editable, not just a
  hardcoded tint"). A fixed, hand-written table of `{shaderName -> [{propertyName,
  Color|Float|Texture, default}]}` -- the "reflection" substitute this engine uses instead
  of parsing compiled SPIR-V (no SPIRV-Cross/spirv-reflect dependency): each entry maps
  1:1 to one of the concrete pipeline classes below, matching that pipeline's own
  hand-written push-constant/descriptor-set layout. See this header's own comment for why
  a *generic* property model here doesn't reintroduce the uber-shader CLAUDE.md rule 7
  forbids -- the genericness lives entirely in this data/dispatch layer, never inside a
  shader branching at runtime.
- `MaterialData.h` + `.cpp` -- done (post-Editor-E8, alongside `ShaderPropertySchema.h`
  above): `{shaderName, properties}` -- `properties` is a `name -> {type, value}` map
  (`MaterialPropertyValue`, reusing `ShaderPropertyType`), not a hardcoded `tintColor`
  field, so a material can hold whatever properties its target shader declares, texture
  references included. `LoadMaterial()`/`WriteMaterial()` read/write plain
  `.material.json` (`{"shader": "...", "properties": {"tintColor": [r,g,b,a],
  "albedoTexture": {"guid": "..."}}}` -- a property's own JSON shape says its type, no
  redundant type tag needed), no Cooker binary step -- same reasoning Scene/Prefab
  documents stay raw JSON (`engine/scene/README.md`), GUID-tagged via the same `.meta`
  sidecar mechanism every other asset under `assets/` uses. `GetColor()`/`GetFloat()`/
  `GetTexture()` return a caller-supplied fallback (the shader's own declared default) for
  a missing/wrong-typed property rather than asserting.
- `ForwardLitColorPipeline.h` + `.cpp` -- done (post-Editor-E8, alongside `MaterialData.h`
  above): a third separate concrete pipeline class (CLAUDE.md rule 7, still never an
  uber-shader with branching) -- same `Vertex` layout and MVP push constant as
  `ForwardLitPipeline`, plus a `vec4 tintColor` in the same combined push-constant range
  (`PushMvpAndTint()`, one call instead of two, since both change together every draw). No
  lighting math (this engine is still unlit-only everywhere), no descriptor sets. Backs
  `ShaderPropertySchema.h`'s `"ForwardLitColor"` entry -- and, in the Editor executables
  only (`editor/main.cpp`/`editor/play_main.cpp`), also draws entities with *no* material
  assigned at all, using a hardcoded purple/violet tint instead of one read from a
  `MaterialData` (this project's "missing material" indicator, the user's own explicit
  request -- replaces what used to fall back to `ForwardLitPipeline`'s debug normal-color
  visualization there; `ForwardLitPipeline` itself is untouched, still used directly by
  every M0-M7 sample with no material system involved).
- `ForwardLitTexturedColorPipeline.h` + `.cpp` -- done (post-Editor-E8, added when material
  property editing grew to cover the Texture property type): a fourth separate concrete
  pipeline class -- `ForwardLitTexturedPipeline`'s single combined-image-sampler
  descriptor set plus `ForwardLitColorPipeline`'s combined mvp+tintColor push constant;
  the fragment shader samples then tints. Backs `ShaderPropertySchema.h`'s
  `"ForwardLitTexturedColor"` entry. Same ownership split as `ForwardLitTexturedPipeline`
  (owns only the pipeline/descriptor-set *layout*; the descriptor pool and per-material
  `VkDescriptorSet` are the caller's responsibility -- now `editor/main.cpp`'s/
  `editor/play_main.cpp`'s own material-texture GPU cache, not one hardcoded demo texture
  like `samples/m7_textures/main.cpp` still has).
- `ShaderPropertySchema.h`'s registry lookup for lighting is described in
  `ForwardLitShadedPipeline.h`'s own entry below, not repeated here.
- `ForwardLitShadedPipeline.h` + `.cpp` -- done (lighting phase A, docs/01 section 8.3's
  "Low-Poly Retro" profile -- not the "PBR profile" `CLAUDE.md` keeps out of scope): a
  fifth separate concrete pipeline class, minimal Blinn-Phong (ambient + N·L diffuse + a
  fixed-shininess specular term, no shadows yet -- a separate, not-yet-started static
  shadow map is phase B). The first pipeline in this project bound to a UBO instead of
  only push constants/a texture sampler: `FrameLightingData` (viewProj, camera world
  position, ambient color, up to `kMaxLights`=4 `GpuLight` entries) is scene-wide,
  per-frame data, not per-draw, so it doesn't fit push-constant space alongside a model
  matrix the way every other pipeline's MVP does -- the vertex shader multiplies
  `frame.viewProj * pc.model` itself instead of the CPU precomputing a full MVP per draw.
  Bound once per frame (`BindFrameDescriptorSet()`), not per-entity. Backs
  `ShaderPropertySchema.h`'s `"ForwardLitShaded"` entry. Assumes uniform (not
  non-uniform) scale on lit entities -- world-space normals are `mat3(model) * normal`
  directly, skipping the inverse-transpose correction non-uniform scale would need, a
  documented phase-A simplification.
- `ForwardVertexLitPipeline.h` + `.cpp` -- done (lighting phase A follow-up, the user's own
  explicit request: the engine's default/base lit material). A sixth separate concrete
  pipeline class -- identical lighting formula and `FrameLightingData` UBO binding shape as
  `ForwardLitShadedPipeline`, but evaluated once per *vertex* (Gouraud shading) and
  interpolated across the triangle, instead of once per fragment -- cheaper on Pi4's
  fill-heavy TBDR GPU, the intended trade-off for the material every low-poly mesh is meant
  to use by default. Its set-0 binding is identically defined to
  `ForwardLitShadedPipeline`'s own, so callers can bind the exact same already-allocated
  `VkDescriptorSet` into either pipeline's layout (Vulkan spec 14.2.2) -- no second
  allocation needed. Backs `ShaderPropertySchema.h`'s `"ForwardVertexLit"` entry.
- `ForwardVertexLitTexturedPipeline.h` + `.cpp` -- done alongside `ForwardVertexLitPipeline`
  above: a seventh separate concrete pipeline class, texture-supporting sibling of it (same
  split `ForwardLitColorPipeline`/`ForwardLitTexturedColorPipeline` already established).
  The first pipeline in this codebase needing *two* independently-bound descriptor sets --
  `RHIPipeline` grew `RHIPipelineDesc::secondDescriptorSetLayoutBindings` (set = 1) for
  this, alongside the existing single-set support (set = 0, unchanged for every earlier
  pipeline). Set 0 is the shared per-frame lighting UBO (same binding shape as
  `ForwardLitShadedPipeline`/`ForwardVertexLitPipeline`, so it's shareable the same way);
  set 1 reuses `ForwardLitTexturedColorPipeline`'s own single combined-image-sampler
  binding shape, so a material's already-cached texture `VkDescriptorSet` is directly
  reusable here too, no second texture cache needed. Backs `ShaderPropertySchema.h`'s
  `"ForwardVertexLitTextured"` entry -- when no texture is assigned, a material should
  target `"ForwardVertexLit"` instead rather than leaving `albedoTexture` empty (same
  "switch shaderName, don't toggle a flag" convention every texture/no-texture shader pair
  in this registry follows).
- `ShadowDepthPipeline.h` + `.cpp` -- done (lighting phase B, docs/01 section 8.3's
  "preferably baked" static shadow map, directional-only for now): an eighth separate
  concrete pipeline class, depth-only (no color attachment at all, matching
  `rhi::RHIShadowMap`'s own depth-only render pass) -- renders scene geometry from the
  shadow-casting light's own view-projection instead of the camera's, into
  `RHIShadowMap`'s depth target. No descriptor sets, a single `mat4 mvp` push constant
  (the light's view-projection combined with the entity's model matrix on the CPU, since
  there's no per-frame UBO to split it out of the way the three lit pipelines' own
  `viewProj` is). Not a material-backed shader -- `ShaderPropertySchema.h` has no entry
  for it, since a shadow bake pass isn't something a material ever targets, it's driven
  entirely by `editor/main.cpp`'s own bake code iterating every mesh entity once at load
  time. See `rhi/README.md`'s `RHIShadowMap` entry for the full design (why it's baked
  once rather than per-frame, the comparison-sampler/hardware-PCF choice, and an analysis
  of what a future point-light cube-map variant would need).

  All three lit pipelines (`ForwardLitShadedPipeline`/`ForwardVertexLitPipeline`/
  `ForwardVertexLitTexturedPipeline`) gained a second binding in their shared set = 0 (the
  shadow map's own comparison sampler, `sampler2DShadow`, alongside the `FrameLightingData`
  UBO) -- identically defined across all three (same binding index/type/stage flags) so
  the one already-shared frame descriptor set keeps covering all three, no new allocation.
  `ForwardLitShadedPipeline` samples it per-*fragment* (matching its own per-fragment
  lighting); `ForwardVertexLitPipeline`/`ForwardVertexLitTexturedPipeline` sample it
  per-*vertex* instead (`textureLod()`, not `texture()` -- implicit-LOD sampling needs
  fragment-stage derivatives that don't exist in a vertex shader), matching their own
  per-vertex lighting. `FrameLightingData` grew a `lightViewProj` field (the baked light's
  own view-projection) plus repurposed `cameraWorldPosition.w` (previously unused) to
  carry which light index within that frame's own `lights[]` array is the shadow caster,
  or -1 if none qualifies -- every *other* light in the scene stays completely unshadowed,
  matching this phase's directional-only, one-baked-light scope.

Eight concrete pipeline classes so far, never an uber-shader with branching (CLAUDE.md
rule 7).
