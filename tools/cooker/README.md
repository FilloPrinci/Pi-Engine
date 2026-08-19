# cooker

Offline Asset Cooker (docs/01 section 12.4, docs/02 section 6's first post-vertical-slice
milestone). Standalone CLI, not linked to `engine_core` -- compiles
`engine/src/asset/AssetGuid.cpp` and `engine/src/renderer/{MeshLoader,CookedMesh}.cpp`
directly as its own sources instead, plus its own `AssetImporter.cpp`/`CookMesh.cpp`/
`CookShader.cpp`, so its link graph is just cgltf + glm + nlohmann-json + shaderc (no
Vulkan/SDL2/Jolt).

Subcommands:

```
cooker mesh <input.gltf|.glb> <output.mesh>
cooker shader <input.vert|.frag> <output.spv>
```

`mesh` also resolves (and, the first time, creates) the source asset's persistent GUID
(`AssetImporter.h`, docs/01 section 12.3) -- reads/writes a `<input>.meta` sidecar next to
the source, and embeds the GUID in the cooked mesh's header. The `.meta` file is meant to
be committed to git (unlike anything under `assets_cooked/`); the tool prints a reminder
the first time it writes one.

`shader` compiles GLSL to SPIR-V via the `shaderc` library (Vulkan 1.2 target
environment, matching CLAUDE.md section 2's baseline). Output is plain SPIR-V -- unlike
meshes, there's no custom binary wrapper: SPIR-V already *is* the format
`RHIPipeline.cpp`'s `ReadFile()`/`CreateShaderModule()` load, so this milestone only
changes *who produces* the `.spv` (shaderc-in-cooker instead of each sample's own
`find_program(glslc)` + `add_custom_command`), not what gets loaded at runtime. Shaders
don't get an Asset GUID -- they're referenced by pipeline class, not by scene/prefab data,
so there's nothing that needs to resolve one by id yet.

Wired into the main build via `cmake/CookAssets.cmake`: every sample depends on the shared
`cooked_assets` (meshes) and `cooked_shaders` (shaders) targets, which cook
`assets/m1_cube.glb` and `shaders/*.{vert,frag}` into `<build dir>/assets_cooked/` once
(CMake `OUTPUT`/`DEPENDS` tracking gives incremental re-cooking for free).

M6/M7 scope so far: meshes and shaders only -- no vertex-cache optimization
(meshoptimizer), no LOD generation, no per-hardware-profile output, no texture/audio/scene
cooking yet, no GUID -> cooked-path manifest/resolution at runtime beyond what
`engine::scene` already does for meshes (nothing resolves a *shader* by GUID) -- see
`engine/include/engine/renderer/CookedMesh.h`'s and `engine/include/engine/asset/README.md`'s
own comments for why each remaining piece is deferred.

Only built for the host (skipped while cross-compiling, see root `CMakeLists.txt` and this
directory's parent comment in `cmake/CookAssets.cmake`) -- a build-time tool has to run on
the machine doing the build, not the cross-compilation target.
