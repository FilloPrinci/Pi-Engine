# asset

- `AssetGuid.h` + `.cpp` — done (M7): stable 128-bit identifier for a source asset
  (docs/01 section 12.3), assigned once by `tools/cooker` and persisted in a sidecar
  `.meta` file next to the source (e.g. `assets/m1_cube.glb.meta`) -- committed to git,
  unlike the cooked output. Embedded in the cooked mesh header
  (`renderer/CookedMesh.h`) so runtime code can round-trip a GUID without needing to know
  the sidecar/manifest mechanics at all.
- Manifest / GUID -> cooked-path resolution: not built yet -- nothing references an asset
  *by* GUID at runtime through M7 (scenes/prefabs will, once they exist).

Only `tools/cooker` ever calls `GenerateAssetGuid()` or touches a `.meta` file -- the
engine runtime only ever reads a GUID back out of already-cooked output.
