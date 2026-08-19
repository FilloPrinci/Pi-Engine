# m7_scene_and_prefab

Scene/Prefab (docs/01 sections 12.2, 13), the second of five Asset Pipeline steps done
after M6. `assets/level.scene.json` describes a static ground slab; `assets/cube.prefab.json`
describes one dynamic cube, instantiated three times at different X positions above the
ground -- three independent, physically-simulated falling cubes from a single prefab file.

Every entity's mesh is resolved by `MeshComponent::meshGuid` (M7, `engine/asset/AssetGuid.h`)
through a small in-sample GUID -> GPU-buffers cache, not a hardcoded filename -- see
`main.cpp`'s `resolveMesh` lambda. Physics bodies are created through a
`scene::CreatePhysicsBodyFn` callback wired to the real `PhysicsWorld`, not a direct
dependency from `engine/scene/` on `engine/physics/` (see `engine/scene/SceneDocument.h`'s
own comment for why that split matters).

No scripting -- attaching a script from a scene/prefab document is out of scope for this
step (see `engine/scene/README.md`).
