# cooker

Minimal offline Asset Cooker (docs/01 section 12.4, docs/02 section 6's first
post-vertical-slice milestone). Standalone CLI, not linked to `engine_core` -- compiles
`engine/src/asset/AssetGuid.cpp` and `engine/src/renderer/{MeshLoader,CookedMesh}.cpp`
directly as its own sources instead, plus its own `AssetImporter.cpp`, so its link graph
is just cgltf + glm + nlohmann-json (no Vulkan/SDL2/Jolt).

```
cooker <input.gltf|.glb> <output.mesh>
```

Also resolves (and, the first time, creates) the source asset's persistent GUID
(`AssetImporter.h`, docs/01 section 12.3) -- reads/writes a `<input>.meta` sidecar next to
the source, and embeds the GUID in the cooked mesh's header. The `.meta` file is meant to
be committed to git (unlike anything under `assets_cooked/`); the tool prints a reminder
the first time it writes one.

Wired into the main build via `cmake/CookAssets.cmake`: every sample depends on the
`cooked_assets` target, which cooks `assets/m1_cube.glb` into
`<build dir>/assets_cooked/m1_cube.mesh` once (CMake `OUTPUT`/`DEPENDS` tracking gives
incremental re-cooking for free, same principle as the shader compilation each sample's
own `CMakeLists.txt` already does).

M6/M7 scope: meshes only, no vertex-cache optimization (meshoptimizer), no LOD generation,
no per-hardware-profile output, no texture/audio/shader/scene cooking yet, no
GUID -> cooked-path manifest/resolution at runtime (nothing references an asset *by* GUID
yet -- a scene/prefab system would) -- see
`engine/include/engine/renderer/CookedMesh.h`'s and `engine/include/engine/asset/README.md`'s
own comments for why each is deferred.

Only built for the host (skipped while cross-compiling, see root `CMakeLists.txt` and this
directory's parent comment in `cmake/CookAssets.cmake`) -- a build-time tool has to run on
the machine doing the build, not the cross-compilation target.
