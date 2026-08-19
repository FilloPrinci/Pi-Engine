# cooker

Minimal offline Asset Cooker (docs/01 section 12.4, docs/02 section 6's first
post-vertical-slice milestone). Standalone CLI, not linked to `engine_core` -- compiles
`engine/src/renderer/{MeshLoader,CookedMesh}.cpp` directly as its own sources instead, so
its link graph is just cgltf + glm (no Vulkan/SDL2/Jolt).

```
cooker <input.gltf|.glb> <output.mesh>
```

Wired into the main build via `cmake/CookAssets.cmake`: every sample depends on the
`cooked_assets` target, which cooks `assets/m1_cube.glb` into
`<build dir>/assets_cooked/m1_cube.mesh` once (CMake `OUTPUT`/`DEPENDS` tracking gives
incremental re-cooking for free, same principle as the shader compilation each sample's
own `CMakeLists.txt` already does).

M6 scope: meshes only, no vertex-cache optimization (meshoptimizer), no LOD generation, no
per-hardware-profile output, no texture/audio/shader/scene cooking, no Asset GUID -- see
`engine/include/engine/renderer/CookedMesh.h`'s own comment for why each is deferred.

Only built for the host (skipped while cross-compiling, see root `CMakeLists.txt` and this
directory's parent comment in `cmake/CookAssets.cmake`) -- a build-time tool has to run on
the machine doing the build, not the cross-compilation target.
