# rhi

Thin Vulkan wrapper (docs/01 section 4, module 1) — not a full multi-backend abstraction.

- `RHIContext.h` + `.cpp` — M0: instance (volk), physical/logical device, queues, swapchain. Vulkan 1.2 core baseline (docs/01 section 3.2).
- `RHISwapchain.h` + `.cpp` — M0.
- `RHIBuffer.h` + `.cpp` — M1: vertex/index buffers via VMA.
- `RHIPipeline.h` + `.cpp` — M1: graphics pipeline creation wrapper.
