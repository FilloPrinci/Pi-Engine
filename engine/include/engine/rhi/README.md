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
- `RHIShadowMap.h` + `.cpp` -- done (lighting phase B, docs/01 section 8.3's "preferably
  baked" static shadow map): this project's first *render-to-texture* target -- a
  depth-only image usable both as a render pass's depth attachment and as a later shader's
  sampled input, unlike `RHIBuffer`/`RHITexture` (written once from CPU-side data, never
  rendered into) or every `RHIPipeline` before it (renders straight to the swapchain,
  never sampled afterward). Owns a comparison sampler (`compareEnable = VK_TRUE`) for free
  hardware bilinear PCF, and its own single-attachment (depth only, no color) render
  pass + framebuffer. Baked exactly once, synchronously, at load time (a one-off command
  buffer + `vkQueueWaitIdle()`, same pattern `RHITexture`'s own upload path already uses)
  -- both Editor executables own one `RHIShadowMap` + one
  `renderer::ShadowDepthPipeline` (the depth-only pass that renders scene geometry from
  the light's view instead of the camera's), see `editor/main.cpp`'s own comment for the
  bake itself and how the result is threaded into `renderer::FrameLightingData`
  (`lightViewProj` + which light index it belongs to).

  **Directional-only for now** (the user's own explicit scoping decision) -- a single 2D
  depth target viewed from one direction, orthographic projection. **What a point-light
  cube-map sibling would need**, analyzed ahead of time per the user's own request, not
  yet built:
  - A `VkImage` with `arrayLayers = 6` and `VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT`, a
    `VkImageView` of type `VK_IMAGE_VIEW_TYPE_CUBE` for the sampled read (a
    `samplerCubeShadow` in GLSL, not `sampler2DShadow`) plus **six** separate
    `VK_IMAGE_VIEW_TYPE_2D` views (one per face, `baseArrayLayer` 0..5) for the six
    *render* targets the bake actually draws into -- a single render pass can only target
    one 2D-view framebuffer at a time, so this becomes six bake passes (or one render
    pass with 6 layers via multiview, a real option on Vulkan 1.2 core but more moving
    parts) instead of `RHIShadowMap`'s single pass.
  - Six framebuffers (one per face) sharing the same render pass, or a multiview render
    pass if that route is taken -- either way, six times `RHIShadowMap`'s own single
    framebuffer.
  - Six `glm::lookAt()` view matrices, one down each cube axis (±X/±Y/±Z) from the point
    light's own world position, each paired with a 90°-FOV perspective projection
    (`glm::perspective`, not `glm::ortho` -- a point light's shadow genuinely needs
    perspective, unlike a directional light's parallel rays) covering `near`..`range`
    (`LightComponent::range` already exists and is exactly the right far-plane value).
  - Six bake draws instead of one -- roughly 6x the bake-time GPU cost (still a one-off,
    load-time cost, not a per-frame one, so this stays affordable even on Pi4) and 6x the
    `ShadowDepthPipeline` draw-call count for the same scene geometry.
  - The consuming shaders' own `ComputeShadow()` swaps from "project into one light's 2D
    clip space, sample `sampler2DShadow`" to "sample `samplerCubeShadow` directly by the
    fragment's own world-space-position-minus-light-position direction vector, with the
    compare depth reconstructed from that same vector's length remapped into the
    perspective projection's own depth range" -- a different, slightly more involved
    formula, not a trivial swap of sampler type alone.
  - `FrameLightingData` would need a second `shadowLightIndex`-equivalent slot (today's
    single `cameraWorldPosition.w` only tracks one directional shadow caster) if both a
    directional *and* a point light are ever meant to cast shadows in the same scene --
    out of scope for a first point-light implementation, which could reasonably start by
    just supporting one shadow-casting light total, matching this phase's own directional
    scope.
