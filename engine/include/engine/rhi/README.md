# rhi

Thin Vulkan wrapper (docs/01 section 4, module 1) -- not a full multi-backend abstraction.

- `RHIContext.h` + `.cpp` -- done (M0): instance (volk), physical/logical device, queues.
  Vulkan 1.2 core baseline (docs/01 section 3.2).
- `RHISwapchain.h` + `.cpp` -- done (M0): swapchain + image views, recreatable in place.
- `RHIBuffer.h` + `.cpp` -- M1: vertex/index buffers via VMA.
- `RHIPipeline.h` + `.cpp` -- M1: graphics pipeline creation wrapper (M0's sample still
  talks to `vkCreateGraphicsPipelines` directly, since it only ever needs one hardcoded
  pipeline -- see `samples/m0_hello_vulkan/main.cpp`).
