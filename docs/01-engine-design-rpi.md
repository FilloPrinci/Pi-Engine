# Design — Open Source 3D Engine for Raspberry Pi 4 (and Pi 5)

**Phase:** Design
**Status:** Draft v8 — covers hardware, technology choices, module architecture, Platform Layer, Editor and Script System, multi-platform portability, rendering pipeline (TBDR, Low-Poly/PBR profiles, bloom), physics (Jolt, parallelization, scripting API), audio (miniaudio, dedicated thread, buses), input/gamepad (Action Mapping, hot-plug), Asset Pipeline (Source/Cooked, GUID, Cooker), Prefab (instantiation, nested, one-way scope). Basis for the following phases (analysis, technical analysis, Claude Code context)

---

## 1. Project goal

An **open source** 3D engine, **accessible** (clear documentation, a reasonable learning curve for indie developers) and **well optimized for Raspberry Pi 4 hardware**, with forward compatibility toward Raspberry Pi 5. Primary target: retro-style low-poly 3D games. Secondary target (optional, scalable): more realistic 3D for more ambitious projects.

---

## 2. Target hardware analysis (verified data, August 2026)

### 2.1 Raspberry Pi 4 (BCM2711) — primary target

| Component | Spec |
|---|---|
| CPU | 4× ARM Cortex-A72, no SMT, 1 MB shared L2 cache, no L3 |
| GPU | Broadcom VideoCore VI, tile-based deferred renderer (TBDR), 500–600 MHz, ~4.4 GFLOPS |
| Graphics APIs | OpenGL ES 3.1 conformant (V3D driver), **Vulkan 1.3 conformant** (V3DV driver, from Mesa 24.3) |
| RAM | LPDDR4-3200, shared CPU/GPU (no dedicated VRAM) |
| Memory bandwidth | up to ~13 GB/s |

### 2.2 Raspberry Pi 5 (BCM2712) — forward-compatibility target

| Component | Spec |
|---|---|
| CPU | 4× ARM Cortex-A76, 2 MB L2 cache, 2 MB L3 cache, ~35% higher IPC than the A72 at equal clock |
| GPU | Broadcom VideoCore VII, TBDR, 800 MHz–1 GHz, ~10–19 GFLOPS depending on clock |
| Graphics APIs | Same V3DV driver, same Vulkan 1.3 conformance; OpenGL ES 3.1 |
| RAM | LPDDR4X-4267, shared CPU/GPU |
| Memory bandwidth | up to ~17 GB/s |
| Extra | PCIe 2.0 (NVMe), useful for faster asset streaming in future projects |

### 2.3 Architectural implications (the most important point in this document)

1. **The GPU is the main bottleneck, not the CPU.** The Pi5's Cortex-A76 alone produces more FP32 FLOPS than its own VideoCore VII GPU. This is the opposite of a desktop PC, where the GPU dominates. Direct consequence: the engine must move as much work as possible onto the CPU (culling, batching, LOD, animation) and treat GPU cycles as the most precious resource.
2. **Both GPUs are TBDR** (like mobile GPUs: Mali, Adreno, PowerVR), not immediate-mode like a desktop GPU. This changes the rules for an efficient renderer: Vulkan render passes and subpasses must be designed to exploit tiling, every unnecessary framebuffer readback must be avoided, and render target changes must be minimized.
3. **Memory bandwidth (13–17 GB/s, shared CPU+GPU) is the second scarcest resource.** Compressed textures are mandatory (ETC2 as the mobile-class baseline), overdraw must be actively reduced, no redundant buffers.
4. **No dedicated VRAM**: every GPU allocation consumes the same system RAM used by game logic. The resource manager must have explicit, configurable memory budgets, not "infinite" assumptions like on desktop.
5. **Quad-core CPU with no SMT**: the job system / task graph must be designed into the engine core from day one, not bolted on later. Culling, skinning, physics, and asset streaming must run in parallel on worker threads.
6. **Pi4→Pi5 power gap (~2-3× on CPU, ~2-4× on GPU)**: the engine needs a *hardware profile* system that detects the chip at runtime and automatically scales polygon budgets, texture resolution, and LOD distance, instead of requiring manual retuning for each target.

---

## 3. Technology choices

### 3.1 Language: **C++20**

Rationale (not a subjective preference):
- The Vulkan ecosystem on Linux/ARM (validation layers, RenderDoc, SDK, documentation, samples) is natively C/C++.
- Open source libraries essential for an engine (glTF loader, physics — Bullet/Jolt, audio — miniaudio, tooling — Dear ImGui) are C/C++ with mature bindings.
- Direct, predictable control over memory layout and cache behavior: critical with 1 MB of shared L2 across 4 cores (Pi4).
- Consistent with the "accessible" goal: most indie/hobbyist engine developers know C++.
- Rust remains valid in the abstract but would introduce friction here in the ARM/embedded-specific tooling without a decisive advantage, given that most of the code needs manual memory management for performance reasons anyway.

### 3.2 Graphics API: **Vulkan (V3DV driver)**

- Baseline target: **Vulkan 1.2 core**, with feature detection for 1.3 extensions where available (ensures compatibility even on installations with an older Mesa).
- No OpenGL ES backend in v1. Can be added in the future as an alternative backend behind an abstraction layer (RHI), if a need for compatibility with older systems emerges.

### 3.2bis Surface creation and input: dual backend (see section 5)

- **SDL2** for the windowed backend (Wayland/X11) — not SDL3, due to current known bugs in the KMSDRM+Vulkan combination.
- **libdrm + libinput** for the direct-to-screen backend (KMS/DRM, no compositor), implemented by hand via the `VK_KHR_display` extension.
- Both behind the common `IDisplayBackend` interface, detailed in section 5.

### 3.3 Build system and dependencies

- CMake (the de facto standard for cross-platform/cross-compiler C++ projects, good support for ARM64 cross-compilation).
- Dependency management: to be evaluated in the technical analysis phase (vcpkg vs Conan vs git submodules) — a point to dig into in the next phase.
- Compilation target: AArch64 (Raspberry Pi OS 64-bit), with the option of cross-compiling from x86_64 for faster development.

---

## 4. Engine software architecture (modules)

```
┌─────────────────────────────────────────────────────┐
│                  Application / Game                   │
├─────────────────────────────────────────────────────┤
│  Scripting layer (optional, to be evaluated later)    │
├───────────────┬───────────────┬───────────────────────┤
│   ECS (scene   │  Gameplay     │   Audio               │
│   & entities)  │  systems      │   engine               │
├───────────────┴───────────────┴───────────────────────┤
│              Job System / Task Graph                   │
│         (parallelization across 4 cores, no SMT)        │
├─────────────────────────────────────────────────────┤
│  Renderer (abstract RHI)  │  Resource Manager           │
│  - Culling (frustum+occl.)│  - Explicit memory budget    │
│  - Batching / instancing  │  - Asset streaming           │
│  - LOD                    │  - Texture compression       │
│  - Hardware profile       │                              │
├─────────────────────────────────────────────────────┤
│              Vulkan backend (V3DV) — concrete RHI       │
├─────────────────────────────────────────────────────┤
│   Platform Layer (IDisplayBackend)                       │
│   - SDL2Backend (window, Wayland/X11)                    │
│   - DirectDRMBackend (direct KMS/DRM, VK_KHR_display)    │
├─────────────────────────────────────────────────────┤
│         Raspberry Pi OS (Linux, DRM/KMS kernel)          │
└─────────────────────────────────────────────────────┘
```

**Main modules:**

0. **Platform Layer** — the `IDisplayBackend` interface decouples the whole engine (RHI, renderer, gameplay) from how the render surface is created and how input is collected. See section 5 for the full detail: two interchangeable implementations (windowed via SDL2, or direct KMS/DRM), same binary, no difference for the code above this layer.
1. **RHI (Render Hardware Interface)** — thin abstraction layer over Vulkan. Not a huge multi-backend abstraction from day one (over-engineering to avoid), but a clean separation that lets us isolate V3DV/TBDR specifics and — in the future — add a GLES backend if needed.
2. **ECS (Entity Component System)** — data-oriented by nature, a good fit for the reduced-cache constraints of the Cortex-A72/A76 (contiguous layout, cache-friendly iteration).
3. **Job System** — task graph with work-stealing across 4 worker threads, used by culling, animation, physics, streaming.
4. **Resource Manager** — asset management (meshes, textures, materials, audio) with explicit memory budgets per hardware profile (Pi4 vs Pi5), asynchronous streaming.
5. **Renderer** — culling (frustum + occlusion), batching/instancing to minimize draw calls, LOD system, TBDR-aware management (optimized render passes/subpasses, minimal overdraw).
6. **Hardware Profile System** — runtime detection (Pi4/VideoCore VI vs Pi5/VideoCore VII) with automatic scaling of: polygon budget, texture resolution, LOD distance/bias, post-processing quality.
7. **Asset Pipeline** — offline tool for asset conversion/optimization (ETC2 texture compression, mesh optimization, LOD generation), separate from the runtime.
8. **Audio Engine** — **miniaudio** (rationale in section 10), dedicated thread outside the Job System to avoid buffer underruns.
9. **Physics** — **Jolt Physics** (rationale in section 9), integrated into the engine's Job System via an adapter instead of an internal thread pool.
10. **Script System** — `ScriptComponent`, `ComponentHandle<T>`, `REGISTER_SCRIPT`/`EXPOSE` macros; lets compiled C++ gameplay hook into entities/components with a Unity-like syntax. Full detail in section 6.
11. **Editor** — separate application, client of the Engine Core (same RHI/Vulkan as the final game): Scene View, Asset Browser, Inspector, Console, Project Hub, Build/Play/Debug pipeline. No integrated code editor — relies on external IDEs (VSCode) by automatically generating `compile_commands.json` and `.vscode/` configuration. Full detail in section 6.

---

## 5. Platform Layer: dual display and execution modes

### 5.1 Context

The Pi runs **Wayland/labwc** as its default desktop. For a game engine, though, the compositor introduces overhead — small, but not negligible when the GPU is already the scarcest resource. The solution is not to choose *one* display backend, but to design the engine to support **two, interchangeable, in the same binary**, so the developer/player can choose without any code compromise.

### 5.2 Common interface

Vulkan does not need to know *how* the surface it draws onto was created — only that the `VkSurfaceKHR` is valid. This makes it possible to isolate the difference behind a single interface:

```cpp
class IDisplayBackend {
public:
    virtual bool Init() = 0;
    virtual VkSurfaceKHR CreateVulkanSurface(VkInstance instance) = 0;
    virtual std::vector<const char*> GetRequiredVulkanExtensions() = 0;
    virtual void PollEvents(InputState& out) = 0;
    virtual Extent2D GetDrawableSize() = 0;
    virtual void Shutdown() = 0;
};
```

**Everything** else in the engine (RHI, renderer, ECS, gameplay) depends only on `IDisplayBackend`, never on the concrete implementations.

### 5.3 The two implementations

| Backend | How it works | When it's used |
|---|---|---|
| **`SDL2DisplayBackend`** | Window via SDL2 on Wayland/X11 (via XWayland). Vulkan surface via `SDL_Vulkan_CreateSurface`. Input translated directly from SDL2 (keyboard, mouse, gamepad). | Default for development and for the build shipped to players. Works identically on Pi and on a desktop PC, useful for iterating without having to test only on the target hardware. |
| **`DirectDRMDisplayBackend`** | No window: takes the DRM master on a free VT, creates the surface via the `VK_KHR_display` extension (`vkCreateDisplayPlaneSurfaceKHR`), enumerates available resolutions via `vkGetDisplayModePropertiesKHR`. Input via **libinput** (the same library used by Wayland/labwc under the hood — mature, already handles gamepad/keyboard/mouse via evdev). | Optional "Performance Mode" — no compositor in the way, maximum available performance. |

Both backends feed the same `InputState` structure: gameplay code doesn't know (and doesn't need to know) which of the two is active.

### 5.4 How the switch between the two happens

Not in-process (tearing down SDL/Wayland and bringing up DRM at runtime in the same process is fragile — race conditions over who owns the DRM master). The robust mechanism is a **process relaunch**:

1. The player enables "Performance Mode" in the menu → the game saves the preference and relaunches itself with a flag (`--display=drm`).
2. The new process starts on a free VT (via `logind`/`systemd-run --scope`, a mechanism already used by the system to let graphical sessions coexist — **no special permission or root required**, the normal Raspberry Pi OS user is already in the `video`/`render`/`input` groups needed) and instantiates `DirectDRMDisplayBackend` instead of `SDL2DisplayBackend`.
3. The desktop stays alive on its original VT, paused, ready to resume when the game exits or the user switches back.
4. On close, the preference reverts to default and the next launch starts windowed again.

Two clean processes in sequence, never two backends active at the same time — no hybrid state to manage.

### 5.5 Execution modes for the shipped game

| Level | How it presents itself | Notes |
|---|---|---|
| **Windowed (default)** | The compiled game runs like any Linux app: launches, appears fullscreen via SDL2 over the normal desktop. Zero configuration, zero special permissions. | Default shipped build. For most low-poly games targeted by this engine, the performance loss from the compositor is negligible. |
| **Performance (opt-in)** | An option in the game's own menu. Internally executes the switch described in 5.4, transparently to the player (no terminal, no password). | Same binary as Level 1 — no separate build to maintain. |

*(Note: the "dedicated standalone SD image" option was deliberately excluded from scope — the engine targets normal Raspberry Pi OS installations, not providing an operating system/distribution of its own.)*

### 5.6 Practical development consequences

- **A single binary**: both backends always compiled in (the dependencies — SDL2, libdrm, libinput — are lightweight, no reason to exclude them at compile time).
- **A single test suite** for gameplay/renderer, because the logic above `IDisplayBackend` is identical between the two backends; the code that needs *backend-specific* testing shrinks down to the thin surface-creation + input layer, isolated and small.
- **Technical note on SDL**: use **SDL2**, not SDL3, for the windowed backend — SDL3 currently has known, recurring bugs specifically in the KMSDRM+Vulkan combination (black screen, undetected displays, corrupted frames on Pi5); for this reason the direct DRM backend must be hand-implemented via `VK_KHR_display` regardless, not delegated to SDL.

---

## 6. Editor and Script System

### 6.1 Editor philosophy: organize, don't write code

The Editor is **not** an IDE (no integrated code editor, no runtime hot-reload — a deliberate choice to stay simple and consistent with "only compiled C++, maximum performance"). The Editor is where you **see, organize, configure, and launch** — writing code always happens in an external IDE (VSCode by default, configurable).

**Editor v1 scope:**
- **Scene View** — 3D scene visualization/navigation, uses the same RHI as the game (no discrepancy between editor and final build).
- **Asset Browser** — asset import, organization, script creation from template.
- **Inspector** — position/configure game objects (entities), set attributes, assign scripts.
- **Console** — logs, build errors.
- **Project Hub** — multi-project management: engine installed once, each project references the shared installation (no engine copy per project).
- **Build/Play/Debug pipeline** — see 6.4.

**Explicitly excluded from v1 scope:** integrated code editor, runtime C++ hot-reload.

### 6.2 Script System: how a script accesses components

A game object (ECS entity) with a script attached accesses the other components of the same entity (e.g. Transform) with a Unity-like syntax:

```cpp
class PlayerScript : public ScriptComponent {
public:
    void OnStart() override {
        transform = GetComponent<TransformComponent>();
    }
    void OnUpdate(float dt) override {
        transform->position += glm::vec3(0, 0, 1) * speed * dt;
    }

    EXPOSE(speed) float speed = 3.0f; // visible and editable in the Inspector
};
```

- **`ScriptComponent`** — base class with `OnStart()`, `OnUpdate(float dt)`, `OnDestroy()`, plus inherited `GetComponent<T>()`/`GetEntity()`.
- **`ComponentHandle<T>`** — not a permanent raw pointer: the data-oriented ECS (sections 2.3, 4) can move component data in memory between frames (contiguous arrays, cache-friendly). `ComponentHandle<T>` is a lightweight handle that resolves safely on every access, so `transform->position` stays valid even after internal rearrangements — the same pattern used by serious ECS implementations (e.g. Unity DOTS). For the developer, the syntax stays identical to a normal pointer.
- **`REGISTER_SCRIPT(ClassName)`** — a macro that registers the script class in a factory at startup, so the Editor knows which scripts exist in the compiled binary and can list/assign them in the Inspector (no complex C++ reflection required).
- **`EXPOSE(field)`** — an "intrusive" macro that expands into code that registers the field (name, offset, type) in a static table, **without changing the field's own declaration** (it stays a normal `float`). A deliberate choice over an external code-generation tool (Unreal Header Tool-style): no `.generated.h` files to regenerate, the IDE always sees the actual code as written — correct autocomplete always, with no intermediate build steps.

### 6.3 Script creation and external IDE integration

- **From template**: in the Asset Browser, "New Script" generates an `.h`/`.cpp` pair from a template with the stubs (`OnStart`, `OnUpdate`, `OnDestroy`) already in place — like "Create > Script" in Unity. The new file is **automatically registered in the build system** (CMake `file(GLOB)` over the project's script folder, or a manifest generated by the Editor) — the developer never touches `CMakeLists.txt` by hand.
- **Opening in an IDE**: double-clicking the script in the Asset Browser launches the configured external IDE (default VSCode) on that specific file.
- **Accurate autocomplete with no manual configuration**: for every project, the Editor automatically generates:
  - `compile_commands.json` (via `CMAKE_EXPORT_COMPILE_COMMANDS=ON`) — needed so clangd (the engine behind VSCode's C/C++ IntelliSense) knows exactly the project's includes, flags, and C++ standard.
  - `.vscode/tasks.json` and `.vscode/launch.json` — build tasks and debug configuration (GDB) already pointed at the correct project.
  - Result: the developer opens VSCode on the Editor-generated project and has working autocomplete and debugging **immediately**, zero manual setup.
- **Assigning to a game object**: in the Inspector, a "Script" component with a slot — drag the script from the Asset Browser, the Editor saves a reference by class name (not path, survives file moves) in the serialized scene.

### 6.4 Build / Play / Debug pipeline

"Play" button on the open scene in the Editor:
1. Triggers an **incremental build** (CMake recompiles only what changed, not a full rebuild) **and an incremental cook of modified assets** (see section 12) — a single unified flow, not two separate steps to manage by hand.
2. Compile or cook errors shown in the Editor's Console (no external terminal to watch).
3. On success, the Editor launches the game with that scene loaded (uses the same Platform Layer/windowed backend described in section 5).
4. **"Play in Debug"**: launches under GDB, or the developer attaches from VSCode via the already-generated `launch.json` — real breakpoints and stepping available with no additional configuration.

No runtime hot-reload is required for this flow: "Play" is compile-and-launch, still made smooth by the incremental build.

---

## 7. Multi-platform portability (Pi4 / Pi5 / PC Windows / PC Linux)

### 7.1 Principle

Optimizations remain primarily designed for Pi4 (the project's primary target, section 2), but the architectural choices made so far (Vulkan instead of proprietary APIs, `IDisplayBackend`, Hardware Profile System) are already the right abstraction to extend to desktop PC with a few targeted adjustments — not a second engine to maintain in parallel.

### 7.2 What is already portable without changes

- **Vulkan** is natively multi-platform: on PC it runs on NVIDIA/AMD/Intel drivers. Same RHI, same rendering code.
- **Shaders in SPIR-V** (Vulkan's intermediate format) — compiled offline, identical on VideoCore, NVIDIA, AMD, Intel.
- **`SDL2DisplayBackend`** (section 5) — SDL2 natively supports Win32, no rewrite needed, only a recompile for the target platform.

### 7.3 What must be handled explicitly

1. **`DirectDRMDisplayBackend` is Linux-only** (KMS/DRM is a Linux kernel API, it doesn't exist on Windows) — excluded at compile time in Windows builds. On desktop PC only `SDL2DisplayBackend` remains: "Performance Mode" (bypassing the compositor) doesn't carry the same value on desktop, where the GPU isn't the bottleneck like on the Pi.
2. **Hand-written ARM NEON code doesn't compile on x86** — any hand-written NEON intrinsics (skinning, culling, vector math) requires an SSE/AVX equivalent behind the same interface, or the use of a math library with integrated multi-target SIMD support (e.g. GLM in SIMD mode). A technical point to structure carefully in the technical analysis phase — if deferred, it risks producing ARM-only code scattered across the codebase.
3. **Hardware Profile System** (section 4) extended with a **"Desktop"** profile (detected when the GPU isn't Broadcom): the same mechanism already existing for Pi4/Pi5, but that removes the polygon/texture/LOD budget limits instead of tightening them — no new concept, just a third profile.
4. **CMake build presets per target**, each selecting the toolchain, the default hardware profile, and which `IDisplayBackend` backends to include in the binary:

```
cmake --preset pi4      # aarch64 toolchain, Pi4 profile, SDL2+DirectDRM backends
cmake --preset pi5      # aarch64 toolchain, Pi5 profile, SDL2+DirectDRM backends
cmake --preset windows  # MSVC/MinGW toolchain, Desktop profile, SDL2 backend only
cmake --preset linux-pc # native toolchain, Desktop profile, SDL2 backend only
```

### 7.4 Real cost to keep in mind

Every added platform is more testing surface (input, packaging, differences between the various vendors' Vulkan drivers) — it's not free just because the architecture allows it. It still remains much cheaper than maintaining a separate renderer or editor per platform, precisely because the RHI and Platform Layer were designed as thin, swappable layers from the start.

---

## 8. Rendering Pipeline: TBDR techniques and per-project profiles

### 8.1 TBDR optimization techniques (apply to both profiles)

Renderer-writing rules specific to the tile-based deferred renderer architecture (VideoCore VI/VII, section 2), different from a renderer designed for an immediate-mode desktop GPU:

1. **Exploit the native hardware Hidden Surface Removal** — TBDR GPUs already eliminate hidden surfaces within each tile on their own *before* running the fragment shader on covered pixels. To actually get this: avoid unnecessary alpha blending (it disables HSR for transparent objects), avoid `discard`/alpha-test in the fragment shader where possible (it breaks early-depth-test).
2. **Render passes/subpasses designed to stay in tile memory** — every render target change or unnecessary `LOAD_OP_LOAD`/store forces an extra round trip to RAM (the scarcest resource, section 2.3). Practical rule: `VK_ATTACHMENT_LOAD_OP_DONT_CARE` for every buffer that doesn't need to be loaded from a previous frame, `TRANSIENT` attachments for depth/intermediate buffers that must never leave the tile.
3. **Simple, reduced-precision shaders** — fewer ALU instructions per pixel, `mediump`/fp16 where precision allows (V3DV supports the fp16 extensions).
4. **Aggressive CPU culling before geometry submission** (section 4) — on TBDR every tile still processes the triangles that touch it, so reducing draw calls and triangles remains the most effective way to save.

### 8.2 Why two separate pipelines instead of an uber-shader

A generic uber-shader that handles both the low-poly style and PBR with branching would waste GPU cycles on features unused in any given pixel — unacceptable on hardware this constrained. Architectural choice: **two concrete, separate, specialized pipelines**, each compiled only with what it needs — different class implementations in the Renderer (`ForwardLitPipeline` vs `ForwardPlusPBRPipeline`), not a single runtime-configurable system.

### 8.3 "Low-Poly Retro" profile (primary target, section 1)

- **Simple forward rendering**, single pass, no G-buffer.
- Lighting: vertex lighting or minimal Blinn-Phong, indicative budget of 2-4 simultaneous dynamic lights (value to validate on real hardware during the testing phase, section 9).
- Shadows: preferably **baked** (precomputed offline in the Asset Pipeline) instead of real-time shadow maps — consistent with the retro style, nearly free at runtime.
- Materials: ready-made "Unlit" and "Simple Lit" templates in the Editor.

**Lightweight bloom (post-process):**

*Threshold + downsample chain + Dual Kawase Blur* technique (used in mobile engines on TBDR GPUs), not the multi-pass Gaussian bloom from desktop tutorials — the latter costs too much bandwidth for our budget (section 2.3):

1. Luminance threshold extraction **fused into the first downsample** (no dedicated full-resolution pass).
2. Progressive downsample chain (e.g. 1/2 → 1/4 → 1/8 → 1/16, typically 3-4 levels) — each level is cheap because it's already small.
3. **Dual Kawase Blur** instead of Gaussian — constant cost regardless of radius (4-5 texture fetches per pass), no wide kernel; the "softness" comes from the downsample/upsample chain itself.
4. Upsample and accumulate toward full resolution with additive blending.
5. Final composite **fused into the existing tonemapping/color grading pass** — no extra dedicated pass, stays in tile memory (rule 8.1.2).

Parameters scaled per hardware profile (Hardware Profile System, section 4):

| Parameter | Pi4 | Pi5 | Desktop |
|---|---|---|---|
| Chain starting resolution | 1/4 | 1/2 | 1/2 or full |
| Downsample/upsample levels | 3 | 4 | 4-5 |
| Precision | fp16 | fp16 | fp16/fp32 |

On even more constrained hardware, the system can disable bloom entirely (`bloom_enabled: false` in the profile) without touching game code.

### 8.4 "PBR" profile (secondary target, optional, section 1)

- **Not fully deferred** — even though TBDR theoretically makes a G-buffer cheap via subpass/input attachment, on Pi4 ALU and bandwidth remain too scarce overall to justify the added complexity.
- **Forward+ (clustered forward)**: single pass like the retro profile, but with per-cluster light culling — more dynamic lights than simple forward without the cost of a full deferred pipeline.
- Metallic-roughness PBR, with **IBL precomputed** offline in the Asset Pipeline (not in real time).
- Shadow mapping on a constrained budget (reduced resolution/cascades compared to a desktop standard).
- Materials: "PBR Standard" template in the Editor.
- Bloom: same Dual Kawase technique as 8.3, slightly more generous default parameters.

### 8.5 Integration in the Project Hub

In the **Project Hub** ("New Project"), besides name/path, you choose the **rendering style** (`render_pipeline: lowpoly_forward | pbr_forwardplus`), written into the project manifest. This choice determines, downstream:

- which concrete pipeline class the Renderer uses;
- which material/shader templates are available in the Editor for that project;
- the default budgets in the Hardware Profile System (a PBR project starts more conservative than a low-poly one on the same hardware, given the higher per-pixel cost);
- which Asset Pipeline tools are enabled (e.g. the IBL bake only makes sense for PBR projects).

The choice isn't permanently blocking (an advanced project can unlock manual control), but the guided default prevents a beginner developer from accidentally ending up on a pipeline more expensive than needed — consistent with the "accessible" goal (section 1).

---

## 9. Physics: engine, parallelization, scripting integration

### 9.1 Physics engine choice: Jolt Physics

**Jolt Physics** (open source, MIT, used by Guerrilla Games for Horizon Forbidden West and Death Stranding 2) instead of Bullet. Rationale specific to this project: <cite index="46-1">Guerrilla switched to Jolt precisely because of the problems their previous physics engine caused when interacting with multithreaded game object updates — Jolt was architected specifically to solve this</cite>, thanks to <cite index="46-1">a lock-free broadphase and a lock-free simulation-island construction algorithm</cite>. This is exactly our situation: we already have our own Job System (section 4) and we don't want a second internal thread pool competing for the same 4 cores.

### 9.2 How the physics pipeline is parallelized

1. **Broadphase** (AABB update, potentially colliding pairs) — embarrassingly parallel, per body.
2. **Island construction** — <cite index="43-1">bodies are split into "islands": each island is a set of dynamic bodies in contact with each other or connected via a constraint.</cite> Different islands are fully independent within the same step — a natural parallelism boundary for the solver.
3. **Constraint solver** — <cite index="39-1">several jobs run in parallel: each one takes the next unprocessed island and runs the iterative constraint solver for that island.</cite> Parallelism happens *between* islands, not within one (the sequential impulse solver is inherently iterative within a single island).
4. **Large islands (critical case)**: <cite index="41-1">islands whose number of constraints and contacts exceeds a threshold are handed to the `LargeIslandSplitter`, so that multiple jobs can process different parts of the same island in parallel</cite> — this prevents a single stack of physics objects from leaving 3 cores idle.
5. **Final integration** (velocity/position) — again embarrassingly parallel, per body.

### 9.3 Integration with the engine's Job System

Jolt exposes `JobSystem` as a replaceable interface. Architectural choice: **an adapter that injects Jolt's jobs into our own existing Job System** (section 4), instead of two parallel schedulers on the same 4 cores — on a Cortex-A72 with no SMT, two independent thread pools would be pure waste from contention/context switching. A single scheduler, one shared queue: culling, animation, and physics compete equally for the same worker threads in the same frame.

### 9.4 Frame phases and synchronization barriers (scripting safety, no explicit locks)

```
Frame:
 ├─ Script phase (OnUpdate) — reads state, applies forces/impulses to bodies
 ├─ ═══ synchronization barrier ═══
 ├─ Physics phase (parallel: broadphase → island → solve → integrate)
 ├─ ═══ synchronization barrier ═══
 ├─ Collision Callback phase (single-thread, see 9.6)
 └─ Post-Physics phase — scripts read the new Transforms, the renderer reads them for drawing
```

No script reads or writes a `RigidBodyComponent` while the solver is working on it — structurally prevented by the barrier, not by programmer discipline. No mutex to explain to the indie developer, no possible race condition by construction — consistent with the "accessible" goal (section 1).

**Fixed timestep** (accumulator pattern): physics runs at a fixed step (e.g. 60 Hz) decoupled from the variable rendering framerate — this also gives a predictable amount of work to distribute among the workers at each step.

### 9.5 Note on the cache (1 MB shared L2, section 2)

Bodies within the same island are often spatially close (they're in contact with each other) — processing them as a cohesive unit of work keeps a small, cache-friendly working set. Worth reflecting in the `RigidBodyComponent` data layout in the ECS too (SoA, section 4): alignment/padding to avoid false sharing between threads writing to adjacent bodies in memory.

### 9.6 Scripting API: collisions, triggers, raycasts

```cpp
class BallScript : public ScriptComponent {
public:
    void OnCollisionEnter(const CollisionInfo& collision) override {
        Entity other = collision.otherEntity;
        glm::vec3 point  = collision.contactPoint;
        glm::vec3 normal = collision.contactNormal;
        float impulse    = collision.impulseMagnitude;
    }
    void OnCollisionStay(const CollisionInfo& collision) override { /* every frame of continued contact */ }
    void OnCollisionExit(Entity other) override { /* end of contact */ }
    void OnTriggerEnter(Entity other) override { /* for colliders with IsTrigger */ }
    void OnTriggerExit(Entity other) override { }
};
```

Unity-style syntax for the developer; the mechanism underneath, necessary because of the parallel solver (9.2):

1. During the physics phase, Jolt notifies contacts via a `ContactListener` — but these callbacks arrive **on the solver's worker threads**, unsafe for running arbitrary script code.
2. The engine intercepts the events and writes them into **per-thread lock-free buffers** (one queue per worker, no contention between threads) — only raw data collection, zero gameplay logic executed at this point.
3. In the dedicated **Collision Callback phase** (single-threaded, after the end-of-physics barrier), the buffers are merged and dispatched as `OnCollisionEnter/Stay/Exit` on the involved scripts — safe to read/write anything, just like in `OnUpdate`.
4. Enter/Stay/Exit don't arrive directly from Jolt this way (which only gives "contact added/persisted/removed" per shape pair) — the engine maintains a per-entity table of active contacts between frames to derive the correct transition.

```cpp
class PlayerScript : public ScriptComponent {
public:
    void OnStart() override { rigidbody = GetComponent<RigidbodyComponent>(); }
    void OnUpdate(float dt) override {
        if (Input::IsKeyPressed(Key::Space))
            rigidbody->AddImpulse(glm::vec3(0, 5, 0));

        RaycastHit hit;
        if (Physics::Raycast(transform->position, glm::vec3(0,-1,0), 1.5f, hit))
            isGrounded = true;
    }
    EXPOSE(rigidbody) ComponentHandle<RigidbodyComponent> rigidbody;
    bool isGrounded = false;
};
```

- **`RigidbodyComponent`** — thin wrapper over Jolt's `BodyInterface` (`AddForce`, `AddImpulse`, `SetVelocity`/`GetVelocity`, `SetKinematic`). Calls from scripts in `OnUpdate` (pre-physics phase) are queued and applied at the start of the next physics step — safe by construction, no lock managed by the developer.
- **`Physics::Raycast` / `Physics::OverlapSphere`** — read-only queries against the state resulting from the end of the previous physics step, safe in `OnUpdate` with no additional synchronization.
- **`ColliderComponent`** — collision shape (box/sphere/capsule/mesh), `IsTrigger` flag, **layer** — maps directly onto Jolt's native layer/mask filter, exposed in the Editor as a per-layer collision matrix (Project Settings → Physics), the same idea as Unity's collision matrix.

---

## 10. Audio: engine, dedicated thread, scripting integration

### 10.1 What the audio system needs to do (basic concepts)

1. **Play sounds** — music, sound effects, voices: start, stop, loop.
2. **Mix** — multiple simultaneous sounds summed into a single signal without distortion.
3. **Spatial (3D) audio** — a sound on the right must "feel" like it's on the right and attenuate with distance, the audio equivalent of the lighting model for rendering.
4. **Buses per category** — groups with independent volume (Master → Music, SFX, UI, Voice), like Unity's Audio Mixer.

### 10.2 Specific constraint: dedicated thread, outside the Job System

Decoding compressed audio costs CPU, and playback has a constraint that rendering doesn't: if the thread generating the signal is even a millisecond late, you hear a click/glitch (buffer underrun) — the ear notices interruptions more than the eye notices a dropped frame. For this reason audio **does not share the Job System** (culling/physics/animation, sections 4 and 9): it deserves a **dedicated thread**, always active, stable priority, that doesn't compete with jobs of variable duration.

### 10.3 System stack (Raspberry Pi OS)

<cite index="47-1">On Raspberry Pi OS Desktop, PipeWire has replaced PulseAudio as the default audio server</cite>, with ALSA compatibility underneath.

### 10.4 Library: **miniaudio**

- Single C header, zero heavy dependencies — consistent with the "minimal dependencies" philosophy already followed for the other systems.
- Natively supports ALSA/PulseAudio on Linux (works under PipeWire via compatibility), WASAPI on Windows, CoreAudio on macOS — same code on every target from section 7, no per-platform audio backend to write.
- Natively decodes WAV, MP3, FLAC, Ogg Vorbis.
- Integrated basic 3D spatialization engine (`ma_engine`/`ma_sound`: distance attenuation, stereo panning) — no dependency on closed proprietary solutions (FMOD/Wwise), incompatible with the open source goal.

### 10.5 Formats and practical choices to stay lightweight on the Pi4

- **Short SFX**: uncompressed WAV — zero runtime decoding, ideal for frequently repeated sounds (footsteps, gunshots), where the CPU cost of repeated decompression would outweigh the extra storage.
- **Music/long sounds**: Ogg Vorbis, **streamed** from disk (not fully loaded into RAM, consistent with the explicit memory budget already set for textures, section 2.3), decoded in chunks on the dedicated audio thread.
- **No HRTF/binaural audio** — too expensive for the Pi4's CPU budget. Distance attenuation + simple stereo panning: the right trade-off, nearly free.

### 10.6 Integration with ECS and scripting

```cpp
AudioSourceComponent {
    AudioClip clip;
    bool loop;
    bool spatial;       // 2D (music/UI) or 3D (positioned in the world)
    float volume;
    AudioBus bus;        // Music | SFX | UI | Voice
};

AudioListenerComponent { }; // usually on camera/player, unique per scene
```

```cpp
class ExplosionScript : public ScriptComponent {
public:
    void OnCollisionEnter(const CollisionInfo& c) override {
        Audio::PlayOneShot(explosionClip, transform->position, /*volume*/ 1.0f);
    }
    EXPOSE(explosionClip) AudioClip explosionClip;
};
```

`Audio::PlayOneShot` for "fire and forget" sounds (no need for a persistent `AudioSourceComponent` for every effect); looping music/ambience uses `AudioSourceComponent` with `Play()`/`Stop()`/`FadeTo()` called from a script.

### 10.7 Audio buses in the Editor

Project Settings → Audio: a fixed, simple hierarchy (not a complex node graph) **Master → Music, SFX, UI, Voice**, each with its own volume — lets players adjust Music/SFX independently in the final game's options, a standard expected experience.

---

## 11. Input System: keyboard, mouse, gamepad

### 11.1 Principle: same logic as the Platform Layer (section 5)

Input follows the same philosophy already established for display: **different physical backends, a single interface the rest of the engine sees**. Keyboard and mouse come from the active video backend (`SDL2DisplayBackend` or `DirectDRMDisplayBackend`/libinput, section 5).

### 11.2 The specific case of gamepads

Every controller has a different physical layout — a mapping database is needed to translate the device's raw bytes into a canonical standard layout (Xbox-style: A/B/X/Y, left/right sticks, triggers). Solution: SDL2's joystick/game controller subsystem **does not require the video subsystem** — so `SDL_INIT_GAMECONTROLLER` is used **always**, regardless of which of the two video backends (section 5) is active. A single gamepad handling code path in both modes, relying on SDL2's community-maintained controller mapping database (Xbox, PlayStation, Switch Pro, generic) — no database to write and maintain internally.

### 11.3 Layered architecture

```
Script (Input::GetAction("Jump").WasPressed())
        ↑
Action Mapping Layer  — decoupled from the physical device
        ↑
Unified InputState  — same format regardless of the backend
        ↑
   ┌────┴─────┐
Keyboard/Mouse   Gamepad (always via SDL_GameController,
(active video     independent of which video backend is active)
backend, sec. 5)
```

### 11.4 Action Mapping

Scripts don't read the physical key directly (otherwise every remapping would require C++ code changes) — they request a **logical action**, mapped to physical keys/buttons in an editable configuration asset (the same principle as Unity's Input System / Godot's InputMap):

```cpp
class PlayerScript : public ScriptComponent {
public:
    void OnUpdate(float dt) override {
        glm::vec2 move = Input::GetAxis2D("Move");           // WASD or left stick, same code
        if (Input::GetAction("Jump").WasPressedThisFrame())  // Space or pad A button
            rigidbody->AddImpulse(glm::vec3(0, 5, 0));
    }
};
```

In the Editor, the **Input Manager** panel (Project Settings → Input): each action is defined and can be bound to multiple physical inputs together as alternatives (e.g. "Jump" → keyboard Space **and** pad A button).

### 11.5 Deadzone and analog response curve

**Radial** deadzone for analog sticks (not per single axis, otherwise diagonal movement comes out wrong), configurable per action/axis, plus a response curve (linear or exponential) — parameters exposed in the Input Manager, not hardcoded.

### 11.6 Hot-plug and local multiplayer

`OnGamepadConnected(int slot)` / `OnGamepadDisconnected(int slot)` events, the same pattern as the engine's other callbacks (e.g. `OnCollisionEnter`, section 9) — a "Player Manager" script assigns connected pads to player slots, handles disconnection without crashing. Relevant for couch co-op, a common use case for the indie low-poly games this engine targets (section 1).

### 11.7 Timing within the frame

Consistent with the phase structure already established for physics (section 9): input is read **once at the start of the frame**, before the Script phase — every script in the same frame sees the exact same input state.

```
Frame:
 ├─ Poll Input (keyboard/mouse from the active video backend + gamepad via SDL_GameController)
 ├─ Script phase (OnUpdate) — reads Input::GetAction/GetAxis2D, consistent state for the whole frame
 ├─ ... (physics, section 9)
```

### 11.8 Rumble/vibration

Supported by SDL2 on compatible pads. Not essential for the MVP — a low-priority feature to confirm during the analysis phase, doesn't impact the architecture above.

---

## 12. Asset Pipeline

### 12.1 Guiding principle: Source Assets vs Cooked Assets

Two separate worlds:

- **Source Assets** — what the developer/artist produces and puts under version control (glTF exported from Blender, PNG, high-quality source WAV).
- **Cooked Assets** — the optimized, compressed, hardware-specific version, generated **offline** by a separate tool (the "Cooker"), never at runtime on the Pi. The shipped game only loads Cooked Assets — no heavy conversion/decoding library (ETC2 encoder, full glTF parser, etc.) ends up in the final binary.

### 12.2 Pipeline per asset type

- **Mesh** — glTF/GLB source. The Cooker optimizes vertex order for the GPU cache (e.g. via meshoptimizer), automatically generates the **LOD** levels required by the Hardware Profile System (section 4) by decimating the mesh, packs it into a native binary format for fast loading (no JSON parsing at runtime).
- **Texture** — uncompressed PNG/TGA source. The Cooker compresses **per hardware profile**: ETC2 for Pi4/Pi5 (section 8), a more suitable format (e.g. BC7) for the Desktop profile (section 7) — two cooked variants of the same source, the Resource Manager loads the right one based on the active profile. Mipmaps generated offline.
- **Audio** — consistent with section 10: short SFX stay WAV (zero runtime decode), music/long sounds transcoded to Ogg Vorbis at a target bitrate during cooking.
- **Shader** — GLSL source, compiled offline to **SPIR-V** (via glslang/shaderc). The Cooker compiles only the variants relevant to the rendering pipeline chosen when the project was created (Low-Poly Retro or PBR, section 8) — a low-poly project doesn't carry around PBR shaders it never uses.
- **Physics collisions** — collision shapes (convex hulls, simplified meshes for Jolt, section 9) generated at cook time from the source mesh, using Jolt's own tools, not recomputed on every launch.
- **IBL / precomputed lighting** (PBR profile only, section 8.4) and **baked shadows** (Low-Poly profile, section 8.3) — offline bake, never real-time.
- **Scenes and Prefabs** (section 13) — saved in a readable text format during development (JSON/YAML), not binary: git-friendly, diffable and mergeable in a team. The Cooker converts them to optimized binary only in the final build.

### 12.3 Asset GUID

The same principle already used for scripts (section 6: reference by class name, not path), extended to all assets: every source asset receives a **stable GUID** on first import, saved in a sidecar file (e.g. `player_mesh.gltf.meta`) next to the source. Scenes, materials, components, and Prefabs reference assets by GUID, not by file path — renaming/moving a folder in the Asset Browser breaks nothing.

### 12.4 Cooker architecture

- **A CLI tool separate from the engine runtime** — shares code where it makes sense (e.g. the math library), but is a standalone executable: the heavy conversion libraries never end up in the shipped game.
- **Incremental cooking**: hash/timestamp per source asset, only what changed gets recompiled — the same principle as incremental C++ builds (section 6.4).
- **Output organized per hardware profile**: separate cooked cache for `pi4/`, `pi5/`, `desktop/` — consistent with section 7.
- **Integration in the "Play" button** (section 6.4): incremental cooking of modified assets is part of the same flow as C++ recompilation — pressing Play, the Editor recompiles code and re-cooks changed assets together, before launching.

---

## 13. Prefab

### 13.1 What it is, technically

A Prefab is simply **an asset with a GUID** (section 12.3) whose content is a scene fragment (an entity and its children) — not a separate concept built from scratch. It reuses the same scene serialization format (text, JSON/YAML, section 12.2): an entity/component tree saved as a `.prefab` file with a stable GUID.

```cpp
class SpawnerScript : public ScriptComponent {
public:
    void OnUpdate(float dt) override {
        if (shouldSpawn) {
            Entity e = Prefab::Instantiate(enemyPrefab, spawnPosition, spawnRotation);
            shouldSpawn = false;
        }
    }
    EXPOSE(enemyPrefab) PrefabRef enemyPrefab;   // dragged from the Asset Browser into the Inspector
    glm::vec3 spawnPosition;
    glm::quat spawnRotation;
};
```

### 13.2 Internal tree references: remapped local IDs

A script inside a Prefab might want to reference a specific child (e.g. the projectile spawn point on a Player entity). Entity IDs are generated at runtime by the ECS — they can't be fixed in the file. Standard solution (Unity, Godot): inside the Prefab, **stable local IDs** are used (assigned at save time, valid only within that file); at instantiation (`Prefab::Instantiate`) the engine **remaps** them to new real ECS IDs, rebuilding internal references consistently — transparent to the script (`GetChild("MuzzlePoint")` works normally).

### 13.3 Nested Prefabs

Comes "for free" from the GUID architecture: since a Prefab is an asset like any other, referenceable by GUID, **a Prefab can contain another Prefab as a child** simply by referencing it — no special case to write, the same mechanism already provided for mesh/texture/script. Useful for composition (e.g. a "Car" Prefab that contains 4 "Wheel" Prefabs as children).

### 13.4 v1 scope: one-way synchronization

Modern Unity's **nested override** system (you modify an instance, the prefab updates for that property, propagating to other instances except where overridden) is complex and needs to be built carefully — **excluded from v1**, consistent with the approach already used for the Editor (section 6) and networking (scope note in section 15). For the first version:

- `Prefab::Instantiate` clones the tree: from that point on the instance is a normal, independent entity.
- In the Editor, explicit actions **"Update Prefab from this instance"** (one-way, overwrites the `.prefab` file with the selected instance's state) and **"Revert instance from Prefab"** (re-reads the file, overwrites the instance) — cover the main practical use case without the complexity of a diff-based override system.
- Prefab instances in the Scene View are visually marked (different icon/color in the hierarchy) to be distinguishable from normal entities.

### 13.5 Integration with the Asset Pipeline

Consistent with 12.2: during development the `.prefab` stays text (git-friendly), the Cooker converts it to optimized binary only in the final build — the same treatment as scenes.

---

## 14. Design constraints to respect (checklist for every future feature)

Every new engine feature, during development, must answer these questions:

- [ ] Does it reduce or increase the number of draw calls?
- [ ] Does it reduce or increase overdraw?
- [ ] Is it compatible with the TBDR architecture (avoids unnecessary framebuffer readbacks)?
- [ ] Does it respect the memory bandwidth budget of the active hardware profile?
- [ ] Can it be parallelized on the job system, or is it forcibly single-thread?
- [ ] Does it scale correctly between the Pi4 and Pi5 profiles without manual retuning?

---

## 15. Next steps (later phases of the project)

**Scope note — Networking:** explicitly excluded from the v1 design. The primary target (single-player / local multiplayer via gamepad, section 11) doesn't require it, and real netcode is a project of its own (like the Editor, section 6) that deserves a dedicated design phase if/when it becomes a priority. The architectural choices already made (data-oriented ECS section 4, `ComponentHandle<T>` section 6, phased/barrier frame sections 9/11) don't close this door for the future.

1. **Analysis** — define in more detail: the renderer feature set for v1 (what's "must have" for a low-poly MVP), supporting library choices (physics, audio, asset loader, ImGui for tooling), asset pipeline format.
2. **Technical analysis in preparation for Claude Code** — detailed technical specs: repository structure, code conventions, main module interfaces, testable incremental milestones.
3. **Context for Claude Code** — a condensed context document (architecture + constraints + conventions) to provide as a reference during assisted development.
4. **Development on Claude Code** — incremental implementation starting from an MVP (rendered triangle → static mesh → scene with culling → minimal ECS → first playable demo).
5. **Testing and improvements** — real profiling on physical Pi4 hardware, GPU/CPU/memory-bandwidth benchmarks, iteration on hardware profile budgets.
