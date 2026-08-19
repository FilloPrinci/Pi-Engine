# scene

- `EntityDesc.h` -- done (M7): plain-data description of one entity (Transform + Mesh by
  GUID + Collider + Rigidbody), JSON-free by design -- see the header's own comment.
- `SceneDocument.h` + `.cpp` -- done (M7, extended Editor steps E4 and E8): `ParseSceneDocument()`
  (JSON -> `EntityDesc` list, `nlohmann::json` stays an implementation detail of the .cpp)
  and `SpawnEntities()` (creates the real ECS components +, if a `CreatePhysicsBodyFn` is
  given, the Jolt body +, if an `AttachScriptFn` is given, each of `EntityDesc::
  scriptNames`). Shared by `Scene.h` and `Prefab.h` so the two never parse the format
  differently. `WriteSceneDocument()`/`ExtractEntityDescs()` (E4, `docs/06-editor-
  roadmap.md`) are the inverse -- live `ecs::World` state back to JSON, used by the
  Editor's Save action. Two deliberate gaps, same shape: `RigidbodyComponent` doesn't
  retain its static/dynamic flag, so `ExtractEntityDescs()` drops (with a stderr warning,
  not a silent guess) the `"rigidbody"` block for any entity that has one; attached
  scripts live entirely outside the ECS (the caller of `AttachScriptFn` owns the
  `ScriptComponent` instances), so `"scripts"` is always silently omitted on save too --
  see `SceneDocument.h`'s own comment for both.
- `Scene.h` + `.cpp` -- done (M7, docs/01 section 12.2; extended Editor steps E4 and E8):
  `LoadScene()` parses + spawns immediately, at no position offset. `SaveScene()` writes
  the reverse direction -- every entity currently in a `World` back to a scene JSON file.
- `Prefab.h` + `.cpp` -- done (M7, docs/01 section 13): same JSON schema as a scene,
  parsed once and instantiable any number of times at different positions. **v1 scope**
  (docs/01 section 13.4's own precedent for trimming Prefab's first version): no
  nested-prefab-reference remapping (13.2), no nested Prefabs (13.3), no override
  tracking (13.4) -- each `Instantiate()` is a plain, independent clone offset by a
  position only (not a full transform composition). `Prefab::Instantiate()` doesn't take
  an `AttachScriptFn` yet -- only `Scene::LoadScene()` does (editor/play_main.cpp is the
  only caller so far).

Script attachment from a scene document (`EntityDesc::scriptNames`, `"scripts": [...]` in
JSON) is done, first used by Editor step E8's Play Mode (`editor/play_main.cpp`,
`editor/scripts/RotateScript.h`) -- see `EntityDesc.h`'s own comment for the one
structural limit that's still true regardless: a scene can only reference a script type
already compiled into whichever executable loads it (no runtime C++ hot-reload, docs/01
section 6.1), so this is "data-driven" in the sense of *which* linked-in script to use,
never truly dynamic/hot-loadable scripting.
