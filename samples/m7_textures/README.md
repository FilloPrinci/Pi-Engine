# m7_textures

Textures (docs/01 section 12.2), the fourth of five Asset Pipeline steps done after M6.
Exercises the whole texture path end to end: `assets/m7_checker.png` (a small hand-generated
checkerboard) is cooked by `cooker texture` into `assets_cooked/m7_checker.tex`
(`engine::renderer::CookedTexture.h`, plain RGBA8, no mipmaps/compression yet), uploaded at
runtime via `engine::rhi::RHITexture`, and sampled by a dedicated
`engine::renderer::ForwardLitTexturedPipeline` (a separate concrete pipeline class from
`ForwardLitPipeline`, CLAUDE.md rule 7 -- never an uber-shader with branching).

The mesh is `assets/m7_quad.gltf`, a small hand-authored quad with UVs -- `assets/m1_cube.glb`
predates `TEXCOORD_0` support (added this step) and wasn't worth regenerating without a
DCC tool in the loop just to add UVs neither M1-M6 nor `m7_scene_and_prefab` need.

No material system yet: the sample owns its one `VkDescriptorPool`/`VkDescriptorSet`
directly (see `main.cpp`), matching the project's current "raw Vulkan boilerplate at the
sample level, RHI wraps only the cross-cutting parts" style through M7 -- see
`engine/rhi/RHITexture.h`'s own comment for why texture upload needs a staging buffer
(unlike `RHIBuffer`'s direct-mapped path).
