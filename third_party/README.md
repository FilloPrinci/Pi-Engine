# third_party

Vendored single-header libraries not distributed via vcpkg (docs/03 section 3.1) — drop
the header (plus its license) directly here, no submodules:

- `cgltf.h` — done (M1): glTF loader (`renderer/MeshLoader`), v1.15, MIT license (embedded
  at the end of the file).
- `stb_image.h` — done (M7, Asset Pipeline textures step): PNG decoding for
  `tools/cooker`'s `cooker texture` subcommand, v2.30, dual MIT/public-domain license
  (embedded at the end of the file). Cooker-only (offline tooling) -- never compiled into
  `engine_core`, same reasoning as cgltf being cooker-only after M6.
- `imgui_backends/` — done (Editor step E1): `imgui_impl_sdl2.h/.cpp` and
  `imgui_impl_vulkan.h/.cpp`, v1.92.8 (matching the vcpkg `imgui` package's pinned
  version exactly -- `LICENSE.txt` in that directory, MIT). Not a single header, and not
  vcpkg-installable the way we need: vcpkg's `imgui` port has no `sdl2-binding` feature
  (only `sdl3-binding`, and this project is pinned to SDL2 throughout), and its
  `vulkan-binding` feature builds `imgui_impl_vulkan.cpp` expecting a directly-linked
  `libvulkan` loader (`find_dependency(Vulkan)` in the port's own CMake config) instead of
  going through volk like the rest of this project (`RHIContext.cpp`'s
  `volkInitialize()`/`volkLoadInstance()`/`volkLoadDevice()`). Vendoring these two backend
  files lets `engine/CMakeLists.txt` compile them with `IMGUI_IMPL_VULKAN_USE_VOLK`
  defined instead, so they dispatch through the same volk-loaded function pointers as
  every other Vulkan call in the engine -- no extra link-time dependency on a system
  Vulkan loader library. `imgui.h` and the core library itself (`imgui.cpp`,
  `imgui_widgets.cpp`, ...) still come from vcpkg's plain `imgui` package (no features
  enabled) -- only the two backend glue files needed project-specific configuration.
- `miniaudio.h` — audio engine, arrives post-vertical-slice (Audio is out of scope for
  M0-M5, docs/02 section 5).

Everything else (volk, VMA, SDL2, GLM, Jolt Physics, Dear ImGui's core library, doctest,
shaderc, meshoptimizer) comes from vcpkg — see /vcpkg.json.
