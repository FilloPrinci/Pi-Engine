# rhi

Thin Vulkan wrapper (docs/01 section 4, module 1) -- not a full multi-backend abstraction.

- `RHIContext.h` + `.cpp` -- done (M0): instance (volk), physical/logical device, queues,
  VMA allocator. Vulkan 1.2 core baseline (docs/01 section 3.2).
- `RHISwapchain.h` + `.cpp` -- done (M0): swapchain + image views, recreatable in place.
- `RHIBuffer.h` + `.cpp` -- done (M1): vertex/index buffers via VMA, direct mapped upload
  (no staging buffer -- Pi4/Pi5 have no dedicated VRAM, see the header's comment).
- `RHIPipeline.h` + `.cpp` -- done (M1, extended M7): graphics pipeline creation wrapper,
  shared by concrete pipeline classes (`renderer/ForwardLitPipeline`,
  `renderer/ForwardLitTexturedPipeline`). M7 (textures step) added optional descriptor
  set layout support (`RHIPipelineDesc::descriptorSetLayoutBindings`) -- empty by default,
  so pipelines with no bound resources are unaffected.
- `RHITexture.h` + `.cpp` -- done (M7, textures step): sampled-texture wrapper (image +
  view + sampler) via VMA, uploaded through a staging buffer (reuses `RHIBuffer`) + a
  synchronous one-off command buffer -- unlike `RHIBuffer`, optimal-tiling images can't be
  directly host-mapped, so this *does* need a staging copy (see the header's own comment).
