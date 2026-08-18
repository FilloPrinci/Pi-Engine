# script

- `ScriptComponent.h` — done (M3, extended M5): base class (`OnStart`, `OnUpdate`,
  `OnDestroy`) plus `GetComponent<T>()`/`GetEntity()`/`GetInput()`. M5 adds
  `OnCollisionEnter/Stay/Exit`/`OnTriggerEnter/Exit` (default no-ops, dispatched by
  `physics/CollisionCallbackDispatcher`) and `GetPhysics()` (read-only queries against
  `physics/PhysicsWorld`, e.g. `Raycast` -- optional, `Attach()`'s `PhysicsWorld*` param
  defaults to `nullptr` so M3's physics-free sample keeps compiling unchanged).
- `ComponentHandle.h` — done (M3): safe handle across ECS rearrangements, re-resolves
  through `World::GetComponent<T>()` on every access instead of caching a raw `T*`.
- `ScriptRegistry.h` + `.cpp` — done (M3): `REGISTER_SCRIPT` factory macro, name -> script
  instance, exercised by `samples/m3_hello_script` (`ScriptRegistry::Create("MoveScript")`).
- `Expose.h` — done (M3): `EXPOSE(field, defaultValue)` macro, `float`/`int`/`bool`/
  `glm::vec3` fields only for now (asset-reference types like `PrefabRef` arrive with the
  Asset Pipeline, post-vertical-slice). Two-argument form, not docs/01 section 6.2's
  one-argument illustration -- see the header's comment for why (offsetof against a
  still-incomplete class needs either UB or the class name repeated; asking for the
  default value once, up front, sidesteps both and gives `ExposedField<T>` its type via
  simple deduction).
