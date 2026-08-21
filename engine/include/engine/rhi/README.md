# rhi

Thin Vulkan wrapper (docs/01 section 4, module 1) -- not a full multi-backend abstraction.

- `RHIContext.h` + `.cpp` -- done (M0): instance (volk), physical/logical device, queues,
  VMA allocator. Vulkan 1.2 core baseline (docs/01 section 3.2).
- `RHISwapchain.h` + `.cpp` -- done (M0): swapchain + image views, recreatable in place.
- `RHIBuffer.h` + `.cpp` -- done (M1): vertex/index buffers via VMA, direct mapped upload
  (no staging buffer -- Pi4/Pi5 have no dedicated VRAM, see the header's comment).
  `UpdateData()` (lighting phase A, docs/01 section 8.3) added the first write-more-than-
  once path -- every earlier use was InitWithData() once at load time; a per-frame UBO
  (`renderer::FrameLightingData`) needed a way to overwrite an already-initialized
  buffer's contents in place. Safe to call every frame since the allocation is always
  persistently host-mapped already; synchronization against a still-in-flight GPU read is
  the *caller's* job (one buffer per frame-in-flight, not one shared instance -- see
  `renderer/ForwardLitShadedPipeline.h`'s own comment).
- `RHIPipeline.h` + `.cpp` -- done (M1, extended M7, extended lighting phase A follow-up):
  graphics pipeline creation wrapper, shared by concrete pipeline classes
  (`renderer/ForwardLitPipeline`, `renderer/ForwardLitTexturedPipeline`). M7 (textures
  step) added optional descriptor set layout support
  (`RHIPipelineDesc::descriptorSetLayoutBindings`, set = 0) -- empty by default, so
  pipelines with no bound resources are unaffected. The vertex-lit default-material
  follow-up added a second, independent set (`secondDescriptorSetLayoutBindings`, set = 1)
  for `renderer/ForwardVertexLitTexturedPipeline`, the first pipeline needing two
  independently-bound resources with different lifetimes (a per-frame lighting UBO at
  set 0, a per-material texture at set 1) -- also empty by default, every earlier pipeline
  still only ever creates set 0.
- `RHITexture.h` + `.cpp` -- done (M7, textures step): sampled-texture wrapper (image +
  view + sampler) via VMA, uploaded through a staging buffer (reuses `RHIBuffer`) + a
  synchronous one-off command buffer -- unlike `RHIBuffer`, optimal-tiling images can't be
  directly host-mapped, so this *does* need a staging copy (see the header's own comment).
