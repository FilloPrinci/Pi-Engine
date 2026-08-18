# rhi

Thin Vulkan wrapper (docs/01 section 4, module 1) -- not a full multi-backend abstraction.

- `RHIContext.h` + `.cpp` -- done (M0): instance (volk), physical/logical device, queues,
  VMA allocator. Vulkan 1.2 core baseline (docs/01 section 3.2).
- `RHISwapchain.h` + `.cpp` -- done (M0): swapchain + image views, recreatable in place.
- `RHIBuffer.h` + `.cpp` -- done (M1): vertex/index buffers via VMA, direct mapped upload
  (no staging buffer -- Pi4/Pi5 have no dedicated VRAM, see the header's comment).
- `RHIPipeline.h` + `.cpp` -- done (M1): graphics pipeline creation wrapper, shared by
  concrete pipeline classes (`renderer/ForwardLitPipeline`).
