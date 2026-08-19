# Pi-Engine

Open source 3D engine, optimized for **Raspberry Pi 4** (forward-compatible with Pi5),
targeting low-poly retro-style games as the primary use case. C++20, Vulkan 1.2 core.

Full context and rationale: see [`CLAUDE.md`](CLAUDE.md) and the design documents in
[`docs/`](docs/). Current status: **M0 — Hello Vulkan** through **M5 — Vertical Slice**
are all verified end to end on Linux desktop and **physical Pi4 hardware** — the initial
milestone roadmap (docs/02) is complete, and the full **Asset Pipeline** (M6-M7) is done
on top of it. M2 populates a 64-entity ECS scene and culls it in parallel via the Job
System, rendering only what's inside the camera frustum through the real V3D (VideoCore)
Vulkan driver; M3 adds a Unity-like Script System (`ScriptComponent`/`ComponentHandle<T>`/
`REGISTER_SCRIPT`/`EXPOSE`) driving a cube via real WASD keyboard input; M4 adds Jolt
Physics via an adapter that injects Jolt's own jobs into the engine's Job System instead
of a second thread pool; M5 ties everything together into `Application`'s full phase loop
(Poll Input → Script → barrier → Physics → barrier → Collision Callback →
Post-Physics/Render) with a physics-driven player that walks, jumps, and reacts to
touching a trigger volume via script; M6 adds `tools/cooker` (docs/01 section 12.4) — a
standalone offline CLI, never linked into the shipped game; M7 rounds it out to a full
Asset Pipeline in 5 parts — persistent Asset GUIDs (`.meta` sidecars), a JSON Scene/Prefab
format, shader cooking (GLSL → SPIR-V via `shaderc`, no local `glslc` needed), texture
cooking + a dedicated textured pipeline, and Cooker-generated mesh LODs via
`meshoptimizer`. Every sample now loads Cooker output (meshes/shaders/textures) instead of
parsing source assets at runtime (see [`samples/`](samples/)).

**Editor**: in progress, following a staged roadmap in
[`docs/06-editor-roadmap.md`](docs/06-editor-roadmap.md) (the full v1 vision in `docs/01`
section 6 broken into incremental steps E1-E8). **E1 — ImGui-Vulkan integration** is done
(`engine::debug::ImGuiOverlay`, see [`samples/e1_imgui_overlay`](samples/e1_imgui_overlay));
**E2 — app skeleton + Scene View** and **E3 — Inspector panel + entity selection** are
done too: [`editor/`](editor/) is a real separate application linking `engine_core`,
loading a `.scene.json` and rendering it with a keyboard-navigable camera (A/D yaw, W/S
pitch, Up/Down zoom); clicking an entity in the Scene panel selects it and shows its
Transform/Mesh/Collider in an Inspector panel, editable live — dragging or typing a new
value moves/resizes the object in the Scene View immediately, no separate "apply" step.

## Prerequisites

- CMake ≥ 3.24, Ninja (Linux/macOS) or Visual Studio 2022 (Windows).
- [vcpkg](https://github.com/microsoft/vcpkg), with `VCPKG_ROOT` set in your environment —
  dependencies are pinned in [`vcpkg.json`](vcpkg.json) (manifest mode, no manual install
  needed beyond that) *except* the autotools below, which vcpkg itself cannot provide.
- **Linux only**: `autoconf`, `automake`, `libtool`, `autoconf-archive` — SDL2 pulls in
  `libxcrypt` on Linux, and vcpkg builds it from source via autotools instead of CMake.
  ```
  sudo apt install autoconf autoconf-archive automake libtool   # Debian/Ubuntu
  sudo dnf install autoconf autoconf-archive automake libtool   # Fedora
  ```
- [Vulkan SDK](https://vulkan.lunarg.com/) — the usual source of the Vulkan validation
  layers used in debug builds. `shaders/*.vert`/`*.frag` are compiled to SPIR-V by the
  Cooker (`tools/cooker`, via the vcpkg-provided `shaderc` library, see
  `shaders/README.md`), not by a local `glslc`, so the SDK itself is optional if you only
  need to build.
- **Cross-compiling for Pi4/Pi5** from Linux x86_64 (the primary dev workflow, see
  [`docs/02-mvp-roadmap-analysis.md`](docs/02-mvp-roadmap-analysis.md) section 3): an
  `aarch64-linux-gnu` cross toolchain, e.g. on Debian/Ubuntu:
  ```
  sudo apt install g++-aarch64-linux-gnu gcc-aarch64-linux-gnu
  ```

## Build

Presets (`CMakePresets.json`): `pi4`, `pi5`, `linux-pc`, `windows`.

```
cmake --preset linux-pc
cmake --build --preset linux-pc-debug
ctest --preset linux-pc-debug
```

Cross-compiling for Pi4:

```
cmake --preset pi4
cmake --build --preset pi4-debug
```

**Caveat since M6**: `tools/cooker` is build-time tooling and must run on the machine
doing the build, not the cross-compilation target, so it's skipped entirely (and every
sample's cook step with it) while actually cross-compiling for `pi4`/`pi5` — see
`cmake/CookAssets.cmake`. A true cross-compiled build currently expects
`assets_cooked/` (under the build directory) to already exist from a prior *native*
build (`linux-pc`, or `pi4`/`pi5` built directly on the device itself, see below — that
turns `CMAKE_CROSSCOMPILING` off, so the Cooker builds and runs normally there). Proper
host-tool bootstrapping for true cross-compiles is future work.

Building *directly on* a Pi4/Pi5 (native ARM, no cross toolchain) instead of cross-compiling:
the `pi4`/`pi5` presets chainload `cmake/toolchains/aarch64-linux-gnu.cmake`, which hardcodes
the cross-compiler names (`aarch64-linux-gnu-gcc`) — those binaries don't exist under that
name on a native ARM system (there it's just `gcc`). Override the chainload to build natively:
```
cmake --preset pi4 -DVCPKG_CHAINLOAD_TOOLCHAIN_FILE=
cmake --build build/pi4 --config Debug
```

## Repository layout

See `CLAUDE.md` section 6. Every `engine/include/engine/<module>/` directory has its own
`README.md` tracking which milestone/step populates it and with which files — the original
M0-M5 plan is in [`docs/03-technical-analysis-claude-code.md`](docs/03-technical-analysis-claude-code.md);
later work (M6+, Editor) is tracked directly in each module's own README as it lands.

## Roadmap

| # | Milestone | Status |
|---|---|---|
| M0 | Hello Vulkan | Verified on Linux desktop and physical Pi4 hardware |
| M1 | Hello Mesh | Verified on Linux desktop and physical Pi4 hardware |
| M2 | Hello Scene | Verified on Linux desktop and physical Pi4 hardware |
| M3 | Hello Script | Verified on Linux desktop and physical Pi4 hardware |
| M4 | Hello Physics | Verified on Linux desktop and physical Pi4 hardware |
| M5 | Vertical Slice (target) | Verified on Linux desktop and physical Pi4 hardware |
| M6 | Minimal Asset Cooker (meshes) | Verified on Linux desktop and physical Pi4 hardware |
| M7 | Asset Pipeline — GUID, Scene/Prefab, shaders, textures, LOD (5 parts) | Verified on Linux desktop and physical Pi4 hardware |

M0-M5 is the initial roadmap (the vertical slice); M6 onward is post-vertical-slice work
(docs/02 section 6), picked one milestone at a time.

Details: `CLAUDE.md` section 8, [`docs/02-mvp-roadmap-analysis.md`](docs/02-mvp-roadmap-analysis.md).

### Editor (in progress)

| # | Step | Status |
|---|---|---|
| E1 | ImGui-Vulkan integration | Verified on Linux desktop and physical Pi4 hardware |
| E2 | `editor/` app skeleton + Scene View | Verified on Linux desktop and physical Pi4 hardware |
| E3 | Inspector + entity selection | Verified on Linux desktop and physical Pi4 hardware |
| E4 | Scene saving | Not started |
| E5 | Console panel | Not started |
| E6 | Asset Browser (minimal) | Not started |
| E7 | Project Hub | Not started |
| E8 | Build/Play/Debug pipeline | Not started |

Details: [`docs/06-editor-roadmap.md`](docs/06-editor-roadmap.md).
