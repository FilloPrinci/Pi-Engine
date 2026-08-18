# script

- `ScriptComponent.h` — M3: base class (`OnStart`, `OnUpdate`, `OnDestroy`); extended in M5 with `OnCollisionEnter/Stay/Exit`, `OnTriggerEnter/Exit`.
- `ComponentHandle.h` — M3: safe handle across ECS rearrangements.
- `ScriptRegistry.h` + `.cpp` — M3: `REGISTER_SCRIPT` factory macro.
- `Expose.h` — M3: `EXPOSE` macro, `float`/`int`/`bool`/`glm::vec3` fields only for now (asset-reference types like `PrefabRef` arrive with the Asset Pipeline, post-vertical-slice).
