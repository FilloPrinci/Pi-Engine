# third_party

Vendored single-header libraries not distributed via vcpkg (docs/03 section 3.1) — drop
the header (plus its license) directly here, no submodules:

- `cgltf.h` — glTF loader, arrives with M1 (`renderer/MeshLoader`).
- `miniaudio.h` — audio engine, arrives post-vertical-slice (Audio is out of scope for
  M0-M5, docs/02 section 5).

Everything else (volk, VMA, SDL2, GLM, Jolt Physics, Dear ImGui, doctest) comes from
vcpkg — see /vcpkg.json.
