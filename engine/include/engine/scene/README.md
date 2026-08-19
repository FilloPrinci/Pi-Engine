# scene

- `EntityDesc.h` -- done (M7): plain-data description of one entity (Transform + Mesh by
  GUID + Collider + Rigidbody), JSON-free by design -- see the header's own comment.
- `SceneDocument.h` + `.cpp` -- done (M7, extended Editor step E4): `ParseSceneDocument()`
  (JSON -> `EntityDesc` list, `nlohmann::json` stays an implementation detail of the .cpp)
  and `SpawnEntities()` (creates the real ECS components +, if a `PhysicsWorld` is given,
  the Jolt body). Shared by `Scene.h` and `Prefab.h` so the two never parse the format
  differently. `WriteSceneDocument()`/`ExtractEntityDescs()` (E4, `docs/06-editor-
  roadmap.md`) are the inverse -- live `ecs::World` state back to JSON, used by the
  Editor's Save action. One deliberate gap: `RigidbodyComponent` doesn't retain its
  static/dynamic flag, so `ExtractEntityDescs()` drops (with a stderr warning, not a
  silent guess) the `"rigidbody"` block for any entity that has one.
- `Scene.h` + `.cpp` -- done (M7, docs/01 section 12.2; extended Editor step E4):
  `LoadScene()` parses + spawns immediately, at no position offset. `SaveScene()` writes
  the reverse direction -- every entity currently in a `World` back to a scene JSON file.
- `Prefab.h` + `.cpp` -- done (M7, docs/01 section 13): same JSON schema as a scene,
  parsed once and instantiable any number of times at different positions. **v1 scope**
  (docs/01 section 13.4's own precedent for trimming Prefab's first version): no
  nested-prefab-reference remapping (13.2), no nested Prefabs (13.3), no override
  tracking (13.4) -- each `Instantiate()` is a plain, independent clone offset by a
  position only (not a full transform composition).

No script attachment from a scene/prefab document yet -- `Scene::Load`/`Prefab::
Instantiate` would need an `InputSystem&` just to `Attach()` one, and no sample needs a
scripted scene/prefab entity yet.
