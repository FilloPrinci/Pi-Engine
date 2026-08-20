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

**Editor**: the full staged roadmap in [`docs/06-editor-roadmap.md`](docs/06-editor-roadmap.md)
(`docs/01` section 6's v1 vision broken into incremental steps E1-E8) is now complete.
**E1 — ImGui-Vulkan integration** (`engine::debug::ImGuiOverlay`, see
[`samples/e1_imgui_overlay`](samples/e1_imgui_overlay)) through **E7 — Project Hub**:
[`editor/`](editor/) is a real separate application linking `engine_core`, loading a
`.scene.json` and rendering it with a keyboard-navigable camera (A/D yaw, W/S pitch,
Up/Down zoom); clicking an entity in the Scene panel selects it and shows its
Transform/Mesh/Collider in an Inspector panel, editable live — dragging or typing a new
value moves/resizes the object in the Scene View immediately; a "Save" button writes the
edited state back to the same `.scene.json`, round-tripping through the existing scene
format with no new format invented; a Console panel captures the engine's existing
stdout/stderr output live (errors highlighted red), still teed to the launching terminal
too; an Asset Browser lists `assets/` and the Cooker's output side by side, showing a
selected source asset's persistent GUID; a Project Hub panel remembers every scene the
Editor has opened and relaunches into a different one on click (`fork()`+`execv()`, one
process per open project/scene, the same way Unity Hub and the Unity Editor are actually
separate processes). **E8 — Build/Play/Debug pipeline** finishes the roadmap: a "Play"
button runs an incremental build + Cooker pass and launches the scene in a live,
physics-simulated window (`editor_play`, a separate executable so Jolt's compile-flag
requirements never leak onto the rest of the Editor's own source files); "Play in Debug"
runs the same flow under `gdb`. A follow-up analysis,
[`docs/07-unity-parity-analysis.md`](docs/07-unity-parity-analysis.md), maps what it would
take to bring the Editor closer to Unity's own feature set and prioritizes those gaps
against this project's actual goals — all five of that list's original items are now
done: scene JSON gained a `"scripts"` field, and Play Mode runs real gameplay scripts
(`editor/scripts/RotateScript.h`) through a proper Script phase, not just
physics/rendering; the Scene View gained real mouse support — click directly in the 3D
view to select an entity (ray vs. bounding sphere), and a draggable X/Y/Z translate gizmo
on the selection, instead of only the Inspector's numeric fields; entities can now have a
parent — the Scene panel shows the result as a real hierarchy tree, the Inspector can
reparent via a combo box, and both the Scene View and Play Mode render every entity
through its true world-space transform (composed from its whole parent chain), not just
its raw local one; every Inspector edit, reparent, and gizmo drag is now undoable
(Ctrl+Z/Ctrl+Y, or the Undo/Redo buttons), a whole click-drag-release counting as one undo
step rather than one per intermediate value; and entities can now carry a material asset
(`.material.json`) whose properties are genuinely generic — a material is an instance of a
shader, and the Inspector edits whatever properties that shader declares (a color picker
for a tint, an asset picker for a texture reference), not just one hardcoded field — backed
by a fixed, hand-declared property schema per shader and two concrete pipelines so far
(`ForwardLitColorPipeline` for a flat tint, `ForwardLitTexturedColorPipeline` for a
texture times a tint), with no C++ recompile needed to change how an object looks. The
Editor also now uses a real docked panel layout (Hierarchy left, Inspector right,
Assets/Console/Project Hub tabbed along the bottom, the 3D Scene view filling the middle —
the classic Unity arrangement, built via Dear ImGui's docking support), rather than a set
of loosely floating windows.

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

### Editor

| # | Step | Status |
|---|---|---|
| E1 | ImGui-Vulkan integration | Verified on Linux desktop and physical Pi4 hardware |
| E2 | `editor/` app skeleton + Scene View | Verified on Linux desktop and physical Pi4 hardware |
| E3 | Inspector + entity selection | Verified on Linux desktop and physical Pi4 hardware |
| E4 | Scene saving | Verified on Linux desktop and physical Pi4 hardware |
| E5 | Console panel | Verified on Linux desktop and physical Pi4 hardware |
| E6 | Asset Browser (minimal) | Verified on Linux desktop and physical Pi4 hardware |
| E7 | Project Hub | Verified on Linux desktop and physical Pi4 hardware |
| E8 | Build/Play/Debug pipeline | Verified on Linux desktop and physical Pi4 hardware |

Details: [`docs/06-editor-roadmap.md`](docs/06-editor-roadmap.md). What it would take to
close the gap with Unity's own editor, prioritized against this project's actual goals:
[`docs/07-unity-parity-analysis.md`](docs/07-unity-parity-analysis.md).
