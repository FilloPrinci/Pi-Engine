# Project Context — Open Source 3D Engine for Raspberry Pi 4/5

> This document condenses `docs/01-engine-design-rpi.md`, `docs/02-mvp-roadmap-analysis.md`, and `docs/03-technical-analysis-claude-code.md`. It is kept as a constant reference during development. For the "why" behind each choice, the three full documents in `docs/` remain the source of truth.

---

## 1. What the project is

An **open source** 3D engine, **accessible** (clear documentation, a reasonable learning curve for indie developers), **optimized for Raspberry Pi 4** with forward compatibility toward Pi5. Primary target: retro-style low-poly 3D games. Optional secondary target: more realistic 3D (PBR profile).

## 2. Technology stack

| | |
|---|---|
| Language | **C++20**, no non-portable compiler extensions |
| Graphics API | **Vulkan 1.2 core** baseline (V3DV driver), feature detection for 1.3 extensions |
| Build | CMake + presets (`pi4`, `pi5`, `windows`, `linux-pc`) |
| Dependencies | vcpkg manifest mode (`vcpkg.json`) |
| Primary development platform | Linux x86_64 desktop, verification on physical Pi4 at every milestone |

## 3. Target hardware — constraints to always respect

- **Pi4** (primary target): Cortex-A72 quad-core no SMT, 1MB shared L2, VideoCore VI TBDR GPU ~4.4 GFLOPS, ~13GB/s memory bandwidth shared CPU/GPU, no dedicated VRAM.
- **Pi5** (forward compatibility): Cortex-A76, VideoCore VII TBDR GPU, ~2-4× more powerful.
- **The GPU is the bottleneck, not the CPU** — move work to the CPU (culling, batching, LOD) whenever possible.
- **TBDR GPU** (tile-based deferred, like mobile) — avoid framebuffer readbacks, minimize render target changes, exploit the native Hidden Surface Removal.
- **Memory bandwidth is the second scarcest resource** — compressed textures mandatory, overdraw to be actively reduced.

**Checklist to apply to every new feature:**
- [ ] Does it reduce or increase draw calls?
- [ ] Does it reduce or increase overdraw?
- [ ] Compatible with TBDR (no unnecessary readbacks)?
- [ ] Respects the active hardware profile's bandwidth budget?
- [ ] Parallelizable on the Job System, or forcibly single-thread?
- [ ] Scales across the Pi4/Pi5/Desktop profiles with no manual retuning?

## 4. Architecture — modules and responsibilities

```
Application/GameLoop (phase-barrier orchestrator)
├── Platform Layer — IDisplayBackend (SDL2DisplayBackend | DirectDRMDisplayBackend)
├── RHI — thin Vulkan wrapper (volk + VMA)
├── ECS — data-oriented, contiguous arrays per component
├── Job System — work-stealing, shared by culling/physics/animation
├── Renderer — culling, batching, LOD, 2 pipelines (ForwardLitPipeline | ForwardPlusPBRPipeline)
├── Script System — ScriptComponent, ComponentHandle<T>, REGISTER_SCRIPT/EXPOSE
├── Physics — Jolt Physics via an adapter into the Job System
├── Audio — miniaudio, dedicated thread OUTSIDE the Job System
└── Resource Manager — explicit memory budgets per hardware profile
```

**Central architectural rule — phase barriers within the frame** (never violate this):
```
Poll Input → Script phase (OnUpdate) → barrier → Physics phase (parallel) → barrier
→ Collision Callback phase (single-thread) → Post-Physics/Render
```
No script reads/writes physics data while the solver is working on it — guaranteed by the barrier, never by programmer discipline.

**`ComponentHandle<T>`**: never a permanent raw pointer to an ECS component — data can move in memory between frames. Always resolve the handle on every access.

## 5. Code conventions

- **Language: ALWAYS English** — code, comments, logs, error messages, Editor text. No exceptions, including the design/analysis documents in `docs/` (originally written in Italian, translated to English on 2026-08-18).
- Classes/types: `PascalCase`. Public methods: `PascalCase` (`OnUpdate`, `GetComponent`). Private members: `m_camelCase`. Files: `snake_case`.
- Namespace: `engine::<module>` (`engine::rhi`, `engine::ecs`, `engine::jobs`, `engine::renderer`, `engine::script`, `engine::physics`, `engine::platform`, `engine::asset`, `engine::scene`, `engine::debug`).
- **No C++ exceptions in hot-path code** (renderer, physics, job system) — return code/bool + out param, `ENGINE_ASSERT` macro. Exceptions allowed only in the Cooker/offline tooling.
- Standard `std::` for the M0-M5 MVP — no premature custom allocators.
- A single `.clang-format` at the root (LLVM base, 4 spaces, column 100).

## 6. Repository structure

```
Pi-Engine/
├── CMakeLists.txt / CMakePresets.json / vcpkg.json
├── .clang-format / .clang-tidy / .gitignore
├── cmake/
│   ├── toolchains/aarch64-linux-gnu.cmake
│   └── CompilerWarnings.cmake
├── engine/
│   ├── include/engine/{core,platform,rhi,ecs,jobs,renderer,script,physics,asset,scene,debug}/
│   └── src/                    # same structure as include/
├── tests/                      # doctest — ECS, math, Job System
├── samples/
│   ├── m0_hello_vulkan/  m1_hello_mesh/  m2_hello_scene/
│   ├── m3_hello_script/  m4_hello_physics/  m5_vertical_slice/
│   ├── m7_scene_and_prefab/    # post-vertical-slice Asset Pipeline step (M7 2/5)
│   ├── m7_textures/            # post-vertical-slice Asset Pipeline step (M7 4/5)
│   ├── m7_lod/                 # post-vertical-slice Asset Pipeline step (M7 5/5)
│   ├── e1_imgui_overlay/       # Editor step E1 (docs/06-editor-roadmap.md)
├── tools/
│   └── cooker/                 # offline Asset Cooker CLI (M6, docs/01 section 12.4)
├── assets/                     # raw source assets (docs/01 section 12.1)
├── shaders/                    # GLSL sources, compiled to SPIR-V by the Cooker
└── docs/                       # design/analysis documents
```

## 7. Pinned dependencies (vcpkg manifest)

| Library | Role | Integration |
|---|---|---|
| volk | Vulkan meta-loader | vcpkg |
| VMA | GPU memory allocator | vcpkg (header-only) |
| SDL2 | Platform Layer + gamepad | vcpkg |
| GLM | Math | vcpkg (header-only) |
| cgltf | glTF loader | vendored (single header) |
| stb_image | PNG decoding (`cooker texture`) | vendored (single header), `tools/cooker` only |
| Jolt Physics | Physics | vcpkg |
| miniaudio | Audio | vendored (single header) |
| Dear ImGui | Debug overlay / Editor UI | vcpkg (core library only); Vulkan/SDL2 backend glue vendored in `third_party/imgui_backends/` (see that directory's README) |
| doctest | Unit testing | vcpkg |
| nlohmann-json | Asset sidecar (`.meta`)/scene+prefab serialization | vcpkg (header-only); `tools/cooker` (sidecar) and `engine_core`'s `scene/` module (scene/prefab JSON, read at runtime) |
| shaderc | GLSL → SPIR-V compilation | vcpkg; `tools/cooker` only (`cooker shader`, replaces each sample's own direct `glslc` invocation) |
| meshoptimizer | Mesh LOD generation + vertex-cache optimization | vcpkg; `tools/cooker` only (`cooker mesh`) |

## 8. Milestone roadmap (current project status)

Primary goal: **minimal vertical slice** — move a cube with the keyboard, jump (physics impulse), touch an object and a script reacts to a collision. The whole pipeline (rendering→physics→scripting→input) together, not isolated features.

| # | Milestone | Exit criterion | Status |
|---|---|---|---|
| M0 | Hello Vulkan — triangle on screen | RHI init, swapchain, pipeline compiles, runs on Pi4 | ✅ Verified on Linux desktop and physical Pi4 hardware |
| M1 | Hello Mesh — cube from glTF, orbit camera | Minimal glTF loader, unlit pipeline active | ✅ Verified on Linux desktop and physical Pi4 hardware |
| M2 | Hello Scene — culling active | Minimal ECS (Transform+Mesh), Job System in real use | ✅ Verified on Linux desktop and physical Pi4 hardware |
| M3 | Hello Script — object moves via keyboard | ScriptComponent/ComponentHandle/REGISTER_SCRIPT working | ✅ Verified on Linux desktop and physical Pi4 hardware |
| M4 | Hello Physics — cube falls and stops | Jolt↔JobSystem adapter, barriers respected, fixed timestep | ✅ Verified on Linux desktop and physical Pi4 hardware |
| M5 | **Vertical Slice** — jump + collision via script | Everything together, same frame, no race conditions | ✅ Verified on Linux desktop and physical Pi4 hardware |
| M6 | Minimal Asset Cooker — meshes cooked offline, no cgltf at runtime | `tools/cooker` CLI + `renderer/CookedMesh`, every sample loads the cooked output | ✅ Verified on Linux desktop and physical Pi4 hardware |
| M7 | Asset Pipeline — Asset GUID, Scene/Prefab, Shader cooking, Textures, LOD generation (5 parts) | `tools/cooker` gained `shader`/`texture` subcommands and LOD generation (`meshoptimizer`); `engine::scene` (JSON, read at runtime); every asset GUID-tagged via `.meta` sidecars | ✅ Verified on Linux desktop and physical Pi4 hardware (all 5 parts) |

M0-M5 is the initial roadmap from docs/02 (the vertical slice); M6 onward is
post-vertical-slice work (docs/02 section 6), picked one at a time rather than planned as
a fixed sequence up front.

**Preliminary decisions to avoid getting stuck:**
- Editor: started — staged roadmap in `docs/06-editor-roadmap.md` (breaks the full v1
  vision from docs/01 section 6 into incremental steps E1-E8, same discipline as M0-M7).
- Asset Cooker: full M7 scope done — meshes (with LOD), shaders, textures, all GUID-tagged.
  Still no per-hardware-profile output (no Hardware Profile System built yet), no
  mipmaps/block texture compression, no audio/scene cooking (scenes stay raw JSON by
  design, see `engine/scene/README.md`). `tools/cooker`'s own README lists exactly what's
  deferred and why.
- `SDL2DisplayBackend` only through M7 so far — `DirectDRMDisplayBackend` comes later, didn't block the vertical slice.

**Explicitly out of scope still** (already designed in docs/01, not started): Audio (deferred at the user's request — not currently testable), gamepad, bloom/post-processing, PBR profile, per-hardware-profile output (Hardware Profile System), Networking.

For the exact list of files/classes to create in each milestone: `docs/03-technical-analysis-claude-code.md` (sections 5-10).

## 9. Rules that must never be violated

1. No script accesses physics data outside the phases defined in section 4 — the barrier is structural, not conventional.
2. No second thread pool independent of the Job System (Jolt is injected via an adapter, it never runs on its own).
3. Audio never shares the Job System — dedicated thread, always, to avoid buffer underruns.
4. Never a permanent raw pointer to an ECS component — always `ComponentHandle<T>`.
5. Never exceptions in engine hot-path code (renderer/physics/job system).
6. Never non-English text in code, comments, logs, errors.
7. Every rendering pipeline is a separate concrete class (`ForwardLitPipeline`/`ForwardPlusPBRPipeline`) — never an uber-shader with branching.
8. `DirectDRMDisplayBackend` is Linux-only, excluded at compile time on Windows builds.

## 10. Further reading

- **Why** behind every architectural choice (hardware, TBDR rendering, physics, audio, input, Editor/Script System, Asset Pipeline, Prefab): `docs/01-engine-design-rpi.md`
- **Full roadmap** and what's excluded from the vertical slice: `docs/02-mvp-roadmap-analysis.md`
- **Exact files/classes** to create for each milestone, dependency rationale: `docs/03-technical-analysis-claude-code.md`
- **Editor staged roadmap** (E1-E8, breaking docs/01 section 6's full v1 vision into incremental steps): `docs/06-editor-roadmap.md`
