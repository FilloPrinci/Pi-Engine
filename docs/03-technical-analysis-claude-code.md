# Technical Analysis — Structure, Conventions, and Interfaces for Claude Code

**Phase:** Technical analysis (preparation for Claude Code)
**Status:** Draft v1
**Reference documents:** `01-engine-design-rpi.md` (architectural decisions), `02-mvp-roadmap-analysis.md` (milestones M0-M5)

---

## 1. Goal of this phase

Make every milestone M0-M5 **executable with no ambiguity**: final repository structure, code conventions, external dependencies with rationale and integration method, and — for each milestone — the exact files/classes/interfaces to create. This document is the direct input for the "Context for Claude Code" (next phase).

---

## 2. Repository structure

```
engine-repo/
├── CMakeLists.txt                  # top-level, aggregates the sub-projects
├── CMakePresets.json                # pi4 / pi5 / windows / linux-pc presets (design doc, section 7.3)
├── vcpkg.json                       # dependency manifest (section 3 of this document)
├── .clang-format                    # single formatting style
├── .clang-tidy                      # static lint rules
├── cmake/
│   ├── toolchains/
│   │   └── aarch64-linux-gnu.cmake  # cross-compilation from x86_64 to Pi4/Pi5
│   └── CompilerWarnings.cmake       # single warning level for all targets
├── engine/                          # Engine Core — library, target `engine_core`
│   ├── include/engine/               # public headers, one subdirectory per module
│   │   ├── core/                     # logging, assert, math wrapper, Application/GameLoop
│   │   ├── platform/                  # IDisplayBackend, SDL2Backend, DirectDRMBackend, Input
│   │   ├── rhi/                       # thin Vulkan wrapper
│   │   ├── ecs/                       # Entity, World, ComponentHandle
│   │   ├── jobs/                      # JobSystem
│   │   ├── renderer/                  # pipeline, culling, mesh loading
│   │   ├── script/                    # ScriptComponent, REGISTER_SCRIPT, EXPOSE
│   │   └── physics/                   # PhysicsWorld, Jolt adapter
│   └── src/                          # implementation, same structure as include/
├── tests/                            # unit tests (ECS, math, Job System) — doctest
├── samples/                          # one executable per milestone
│   ├── m0_hello_vulkan/
│   ├── m1_hello_mesh/
│   ├── m2_hello_scene/
│   ├── m3_hello_script/
│   ├── m4_hello_physics/
│   └── m5_vertical_slice/
├── assets/                           # raw source assets used by the samples (M0-M5, no Cooker yet)
├── shaders/                          # GLSL sources, compiled to SPIR-V at build time for now
└── docs/                             # this document + 01/02, kept in the repo
```

**Note on the samples**: each milestone is its own executable under `samples/`, not a separate Git branch — this way M0 stays compilable and verifiable even after building M5, useful to avoid "losing" exit criteria already reached (regressions caught immediately).

---

## 3. Dependency management: vcpkg in manifest mode

Decision (the design document left this open, section 3.3): **vcpkg in manifest mode** (`vcpkg.json` at the root, integrated via `CMAKE_TOOLCHAIN_FILE`).

Rationale:
- The project's two most delicate dependencies — **SDL2** and **Jolt Physics** — are both mature vcpkg packages with verified support for cross-compilation to `arm64-linux`, checked before locking in this choice.
- Manifest mode pins exact versions in the repository (`vcpkg.json` + `vcpkg-configuration.json`) — reproducible builds across different developers and across the platforms in section 7 of the design document, with no manual git-submodule management per library.
- Integrates directly with the CMake presets already decided (`cmake --preset pi4` etc.) via dedicated vcpkg triplets (`arm64-linux`, `x64-linux`, `x64-windows`).

**Explicit fallback**: if ARM64 cross-compilation problems with a specific library via vcpkg come up during development (historically this has happened with a few SDL-related packages, not SDL2 core), that single dependency switches to a Git submodule compiled directly in the CMake tree — not a failure of the whole strategy, just a one-off exception to handle case by case.

### 3.1 Pinned external dependencies

| Library | Role | Integration |
|---|---|---|
| **volk** | Vulkan meta-loader — loads function pointers without linking directly against `vulkan-1`, enables 1.2/1.3 feature detection (design doc, section 3.2) | vcpkg |
| **Vulkan Memory Allocator (VMA)** | GPU memory allocator — sub-allocation, explicit budget tracking (design doc, section 2.3, point 4) | vcpkg (header-only) |
| **SDL2** | Platform Layer, windowed backend + gamepad (design doc, sections 5, 11) | vcpkg |
| **GLM** | Math (vectors, matrices, quaternions) — same API used in the design document's scripting examples | vcpkg (header-only) |
| **cgltf** | glTF loader — single-header C, no heavy JSON dependency (unlike tinygltf), consistent with the "minimal dependencies" philosophy already followed for miniaudio | vendored (single header, no vcpkg needed) |
| **Jolt Physics** | Physics (design doc, section 9) | vcpkg |
| **miniaudio** | Audio (design doc, section 10) — not needed before the post-vertical-slice audio milestone, but pinned from the start | vendored (single header) |
| **Dear ImGui** | Debug overlay (FPS, ECS state, log) — already useful from M2 to inspect frustum culling and the Job System, a lightweight precursor to the Editor (section 6) | vcpkg |
| **doctest** | Unit testing (ECS, math, Job System) | vcpkg |

---

## 4. Code conventions

- **Standard**: C++20, no non-portable compiler extensions (consistent with the multi-platform target, section 7).
- **Naming**:
  - Classes/types: `PascalCase` (`JobSystem`, `IDisplayBackend`, `ComponentHandle<T>`).
  - Public methods: `PascalCase` (`OnUpdate`, `GetComponent`, `AddImpulse`) — consistent with all the scripting examples already shown in the design document.
  - Private members: `m_camelCase` (`m_position`, `m_workerThreads`).
  - Files: `snake_case` (`job_system.h` / `job_system.cpp`) — avoids case-sensitivity issues between Linux and Windows.
  - Namespace: `engine::<module>` (`engine::rhi`, `engine::ecs`, `engine::jobs`, `engine::renderer`, `engine::script`, `engine::physics`, `engine::platform`).
- **Formatting**: a single `.clang-format` at the repo root (LLVM base style, 4-space indentation, column 100) — applied automatically, no style discussion left to individual commits.
- **Error handling**: **no C++ exceptions in the Engine Core's hot-path code** (renderer, physics, job system) — consistent with the goal of predictable performance on constrained hardware (design doc, section 2.3). Return codes / `bool` + output parameters are used, plus an `ENGINE_ASSERT` macro for programmer errors (debug build: abort with stack trace; release build: no-op or log). Exceptions remain allowed in the Cooker/offline tooling, where performance isn't critical.
- **Containers**: standard `std::` for the MVP (M0-M5) — custom allocators and memory pools are an optimization to introduce **after** the vertical slice runs, not before (avoids prematurely optimizing code that might still change shape).
- **Language**: **English everywhere in code and everything user/developer-facing** — code, comments, log messages, error text (`ENGINE_ASSERT`, Job System/RHI/Cooker logs), and in the future all of the Editor's UI text (menus, tooltips, Console). No exceptions, consistent with the goal of an open source project accessible to an international audience, not just an Italian one (design doc, section 1).

---

## 5. Milestone M0 — *Hello Vulkan*

**Goal (from doc 02, section 2):** colored triangle on screen, RHI initializes, swapchain works, runs on Pi4.

**Files to create:**
- `engine/include/engine/platform/IDisplayBackend.h` — interface already defined in the design doc, section 5.2.
- `engine/include/engine/platform/SDL2DisplayBackend.h` + `.cpp` — the only active implementation in M0 (`DirectDRMDisplayBackend` comes later, doesn't block the vertical slice).
- `engine/include/engine/rhi/RHIContext.h` + `.cpp` — Vulkan instance (via volk), physical device selection, logical device, queues, swapchain. Vulkan 1.2 core baseline (design doc, section 3.2).
- `engine/include/engine/rhi/RHISwapchain.h` + `.cpp`.
- `samples/m0_hello_vulkan/main.cpp` — creates the window, initializes the RHI, a minimal graphics pipeline (inline vertex/fragment shader, no external assets), clear color + triangle, present loop.
- `shaders/m0_triangle.vert` / `.frag` — GLSL sources compiled to SPIR-V at build time (custom CMake target, not the Cooker yet).

**Exit criterion:** unchanged from doc 02.

---

## 6. Milestone M1 — *Hello Mesh*

**Goal:** static cube/mesh from glTF, orbiting camera, Low-Poly Retro unlit pipeline active.

**Files to create:**
- `engine/include/engine/rhi/RHIBuffer.h` + `.cpp` — GPU buffer wrapper (vertex/index) via VMA.
- `engine/include/engine/rhi/RHIPipeline.h` + `.cpp` — Vulkan graphics pipeline creation wrapper (not yet the full multi-backend abstract RHI, just the thin layer decided in the design doc, section 4).
- `engine/include/engine/renderer/MeshLoader.h` + `.cpp` — loads glTF via cgltf, produces raw vertex/index buffers (no cache-vertex optimization: that's Cooker work, not runtime work, consistent with design doc section 12).
- `engine/include/engine/renderer/ForwardLitPipeline.h` + `.cpp` — *unlit* variant only in M1 (lighting arrives when needed, doesn't block this milestone).
- `engine/include/engine/core/Camera.h` — camera math (view/projection), orbit camera for the sample.
- `samples/m1_hello_mesh/main.cpp`.
- `assets/m1_cube.glb` — test source asset.

---

## 7. Milestone M2 — *Hello Scene*

**Goal:** a handful of objects, frustum culling active, first real use of the Job System.

**Files to create:**
- `engine/include/engine/ecs/Entity.h` — lightweight handle (index + generation, to invalidate references to destroyed entities).
- `engine/include/engine/ecs/World.h` + `.cpp` — data-oriented component storage (contiguous arrays per type, consistent with design doc, section 2.3/4).
- `engine/include/engine/ecs/components/TransformComponent.h`, `MeshComponent.h`.
- `engine/include/engine/jobs/JobSystem.h` + `.cpp` — task graph with work-stealing, worker count = `core count - 1` (design doc, section 9.3, initial estimate, to validate on real hardware).
- `engine/include/engine/renderer/FrustumCuller.h` + `.cpp` — first system that submits real jobs to the Job System (parallel culling over all entities with a `MeshComponent`).
- `engine/include/engine/core/Application.h` + `.cpp` — **new, explicit module**: the frame-loop orchestrator (not separately named in the design document, but implicit in the phase/barrier structure already described for physics/input). From here on, every milestone adds a phase to its loop.
- `samples/m2_hello_scene/main.cpp`.

---

## 8. Milestone M3 — *Hello Script*

**Goal:** an object moves via keyboard, minimal Script System.

**Files to create:**
- `engine/include/engine/script/ScriptComponent.h` — base class (`OnStart`, `OnUpdate`, `OnDestroy`), per design doc section 6.2.
- `engine/include/engine/script/ComponentHandle.h` — safe handle across ECS rearrangements (design doc, section 6.2).
- `engine/include/engine/script/ScriptRegistry.h` + `.cpp` — factory + `REGISTER_SCRIPT` macro (design doc, section 6.2).
- `engine/include/engine/script/Expose.h` — `EXPOSE` macro (design doc, section 6.2), only for `float`/`int`/`bool`/`glm::vec3` fields in this milestone (asset-reference types, e.g. `PrefabRef`, arrive with the Asset Pipeline).
- `engine/include/engine/platform/InputState.h`, `InputSystem.h` — keyboard only in M3 (gamepad via `SDL_GameController` is a later extension, doesn't block).
- `samples/m3_hello_script/scripts/MoveScript.h` — the first script, written "the way a developer would write it".
- `samples/m3_hello_script/main.cpp` — adds the Script phase to `Application`'s loop (poll input → `OnUpdate` of all active scripts).

---

## 9. Milestone M4 — *Hello Physics*

**Goal:** the cube falls under gravity and comes to rest on a plane, working Jolt↔Job System adapter.

**Files to create:**
- `engine/include/engine/physics/PhysicsWorld.h` + `.cpp` — wrapper around `JPH::PhysicsSystem`.
- `engine/include/engine/physics/JoltJobSystemAdapter.h` + `.cpp` — implements Jolt's `JPH::JobSystem` interface, injecting its jobs into our own `JobSystem` (design doc, section 9.3) — technically the trickiest piece of this milestone.
- `engine/include/engine/ecs/components/RigidbodyComponent.h`, `ColliderComponent.h`.
- `engine/include/engine/physics/PhysicsPhase.h` + `.cpp` — fixed timestep (accumulator pattern, design doc, section 9.4), barrier orchestration: hooked in as a new phase in `Application` **between** Script and Post-Physics.
- `samples/m4_hello_physics/main.cpp`.

---

## 10. Milestone M5 — *Vertical Slice* (target)

**Goal:** everything together — move the cube, jump, touch an object and a script reacts.

**Files to create:**
- `engine/include/engine/physics/CollisionCallbackDispatcher.h` + `.cpp` — per-thread lock-free buffer during the solver, then single-thread dispatch of `OnCollisionEnter/Stay/Exit` (design doc, section 9.6) — implements the Collision Callback phase in `Application`.
- Extension of `ScriptComponent.h` with `OnCollisionEnter/Stay/Exit`, `OnTriggerEnter/Exit`.
- `engine/include/engine/physics/Raycast.h` — `Physics::Raycast`/`Physics::OverlapSphere` (design doc, section 9.6).
- `samples/m5_vertical_slice/scripts/PlayerScript.h` (input + jump impulse + ground raycast) and `TargetScript.h` (reacts to `OnCollisionEnter`).
- `samples/m5_vertical_slice/main.cpp` — the complete frame loop: `Poll Input → Script phase → barrier → Physics phase → barrier → Collision Callback phase → Post-Physics/Render`, exactly as described in the design doc, section 9.4.

**At this point** `Application`/`GameLoop` implements the entire phase scheme discussed throughout the design phase — it's the module that "closes the loop" between all the others.

---

## 11. Testing

- **Unit tests** (`tests/`, doctest): math (camera, transforms), `World`/ECS (entity creation/destruction, `ComponentHandle` validity after rearrangements), `JobSystem` (correct execution/waiting of a simple task graph) — testable on x86_64 desktop, no Pi hardware required.
- **Real hardware verification**: at every milestone, not only at the end of the roadmap (consistent with doc 02, section 3) — not automated unit tests at this stage, but a manual validation pass (compiles and runs on physical Pi4, exit criterion respected).

---

## 12. Next steps

1. **Context for Claude Code** — a condensed document summarizing architecture (from doc 01), roadmap (from doc 02), and this technical document, in a format meant to be provided as a constant reference during assisted development.
2. **Development on Claude Code** — milestone-by-milestone implementation, M0 → M5, each with its exit criterion verified before moving to the next.
