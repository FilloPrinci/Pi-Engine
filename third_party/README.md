# third_party

Vendored single-header libraries not distributed via vcpkg (docs/03 section 3.1) — drop
the header (plus its license) directly here, no submodules:

- `cgltf.h` — done (M1): glTF loader (`renderer/MeshLoader`), v1.15, MIT license (embedded
  at the end of the file).
- `stb_image.h` — done (M7, Asset Pipeline textures step): PNG decoding for
  `tools/cooker`'s `cooker texture` subcommand, v2.30, dual MIT/public-domain license
  (embedded at the end of the file). Cooker-only (offline tooling) -- never compiled into
  `engine_core`, same reasoning as cgltf being cooker-only after M6.
- `miniaudio.h` — audio engine, arrives post-vertical-slice (Audio is out of scope for
  M0-M5, docs/02 section 5).

Everything else (volk, VMA, SDL2, GLM, Jolt Physics, Dear ImGui, doctest, shaderc) comes
from vcpkg — see /vcpkg.json.
