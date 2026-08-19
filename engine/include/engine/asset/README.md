# asset

- `AssetGuid.h` + `.cpp` — done (M7): stable 128-bit identifier for a source asset
  (docs/01 section 12.3), assigned once by `tools/cooker` and persisted in a sidecar
  `.meta` file next to the source (e.g. `assets/m1_cube.glb.meta`) -- committed to git,
  unlike the cooked output. Embedded in cooked mesh/texture headers (`renderer/CookedMesh.h`,
  `renderer/CookedTexture.h`) so runtime code can round-trip a GUID without needing to know
  the sidecar/manifest mechanics at all. `engine::scene` resolves entities' `MeshComponent
  ::meshGuid` this way (M7 Scene/Prefab step).
- `AssetMeta.h` + `.cpp` — done (Editor step E6, `docs/06-editor-roadmap.md`):
  `TryReadAssetMetaGuid()`, the read-only counterpart to `tools/cooker`'s
  `GetOrCreateAssetGuid()` -- reads an existing `.meta` sidecar's GUID back out without
  ever creating one. Used by the Editor's Asset Browser panel to show a selected source
  asset's GUID; never called by `tools/cooker` itself (it has its own
  create-if-missing logic, `AssetImporter.h`/`.cpp`).
- Manifest / GUID -> cooked-path resolution: still not built as a general lookup table --
  every consumer (samples, the Editor) that resolves a GUID to cooked output today does so
  by trying the one/few cooked files it already knows about and checking the embedded GUID
  matches (see e.g. `samples/m7_scene_and_prefab/main.cpp`'s `resolveMesh`), not a real
  manifest. Worth building once there are enough distinct cooked assets that "just try the
  ones you know about" stops being reasonable.

`tools/cooker` is the only thing that ever calls `GenerateAssetGuid()` or *writes* a
`.meta` file -- `AssetMeta.h`'s `TryReadAssetMetaGuid()` is the only place in `engine_core`
that reads one back, and only for display (the Editor), never to resolve an asset at
runtime (that's the embedded-GUID-in-cooked-output path above).
