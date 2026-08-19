# cooker

Offline Asset Cooker (docs/01 section 12.4, docs/02 section 6's first post-vertical-slice
milestone). Standalone CLI, not linked to `engine_core` -- compiles
`engine/src/asset/AssetGuid.cpp` and
`engine/src/renderer/{MeshLoader,CookedMesh,CookedTexture}.cpp` directly as its own
sources instead, plus its own `AssetImporter.cpp`/`CookMesh.cpp`/`CookShader.cpp`/
`CookTexture.cpp`, so its link graph is just cgltf + stb_image + glm + nlohmann-json +
shaderc (no Vulkan/SDL2/Jolt).

Subcommands:

```
cooker mesh <input.gltf|.glb> <output.mesh>
cooker shader <input.vert|.frag> <output.spv>
cooker texture <input.png> <output.tex>
```

`mesh` and `texture` both resolve (and, the first time, create) the source asset's
persistent GUID (`AssetImporter.h`, docs/01 section 12.3) -- reads/writes a `<input>.meta`
sidecar next to the source, and embeds the GUID in the cooked file's header. The `.meta`
file is meant to be committed to git (unlike anything under `assets_cooked/`); the tool
prints a reminder the first time it writes one.

`shader` compiles GLSL to SPIR-V via the `shaderc` library (Vulkan 1.2 target
environment, matching CLAUDE.md section 2's baseline). Output is plain SPIR-V -- unlike
meshes/textures, there's no custom binary wrapper: SPIR-V already *is* the format
`RHIPipeline.cpp`'s `ReadFile()`/`CreateShaderModule()` load, so this milestone only
changes *who produces* the `.spv` (shaderc-in-cooker instead of each sample's own
`find_program(glslc)` + `add_custom_command`), not what gets loaded at runtime. Shaders
don't get an Asset GUID -- they're referenced by pipeline class, not by scene/prefab data,
so there's nothing that needs to resolve one by id yet.

`texture` decodes via `stb_image` (`third_party/stb_image.h`, forced to 4 channels
regardless of the source's actual channel count) and writes
`engine::renderer::CookedTexture.h`'s format -- tightly packed RGBA8, no mipmaps, no block
compression (ETC2/BC7, docs/01 section 12.2) -- same "deliberately minimal" scope as
`cooker mesh`.

Wired into the main build via `cmake/CookAssets.cmake`: every sample depends on the shared
`cooked_assets` (meshes), `cooked_shaders` (shaders), and `cooked_textures` (textures)
targets, which cook every source under `assets/`/`shaders/` into `<build dir>/assets_cooked/`
once (CMake `OUTPUT`/`DEPENDS` tracking gives incremental re-cooking for free).

M6/M7 scope so far: meshes, shaders, and now textures -- no vertex-cache optimization
(meshoptimizer), no LOD generation, no per-hardware-profile output, no mipmaps/block
compression, no audio/scene cooking yet, no GUID -> cooked-path manifest/resolution at
runtime beyond what `engine::scene` already does for meshes -- see
`engine/include/engine/renderer/CookedMesh.h`'s, `CookedTexture.h`'s, and
`engine/include/engine/asset/README.md`'s own comments for why each remaining piece is
deferred.

Only built for the host (skipped while cross-compiling, see root `CMakeLists.txt` and this
directory's parent comment in `cmake/CookAssets.cmake`) -- a build-time tool has to run on
the machine doing the build, not the cross-compilation target.
