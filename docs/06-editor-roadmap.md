# Editor — Staged Roadmap

> The full v1 vision for the Editor is designed in `docs/01-engine-design-rpi.md` section 6
> ("Editor and Script System") — Scene View, Asset Browser, Inspector, Console, Project
> Hub, and the Build/Play/Debug pipeline, all at once. That's too much for a single
> milestone (`CLAUDE.md` section 8 explicitly parked the Editor as "out of this roadmap...
> deserves its own dedicated design phase"). This document breaks that v1 vision into
> incremental steps, each with its own exit criterion, following the same discipline
> already used for M0-M7 and the Asset Pipeline's 5 parts: implement, test locally, test
> on physical Pi4 (capped/uncapped FPS), commit, before moving to the next step.

## Why this order

`docs/01` section 6.1 frames the Editor as "a separate application, client of the Engine
Core (same RHI/Vulkan as the final game)." The riskiest, most foundational assumption in
that sentence — that a second executable can link `engine_core` and render into an ImGui-
paneled window using the exact same RHI/renderer the game samples already use — has never
been exercised: Dear ImGui has been a pinned vcpkg dependency since the start but is still
just a `TODO(M2)` comment in `engine/CMakeLists.txt`, never actually wired to the RHI. So
the plan front-loads that risk (E1-E2) before building any Editor *feature* on top of it,
then layers panels one at a time in roughly the order a developer would actually reach for
them (see something → select/inspect it → edit it → save it → browse assets → watch
logs), leaving the two heaviest/most self-contained pieces (Project Hub, Build/Play/Debug
pipeline) for last since neither blocks the others and both are large enough to deserve
their own step regardless.

## Steps

| # | Step | Exit criterion | Status |
|---|---|---|---|
| E1 | ImGui-Vulkan integration | A reusable `engine::debug::ImGuiOverlay` (Init/NewFrame/Render/Shutdown) wired into the RHI via `imgui_impl_vulkan`/`imgui_impl_sdl2`; a small demo (ImGui's own demo window, or an FPS graph) rendered on top of an existing sample's output, verified on Pi4. Resolves the M2 TODO. Not the Editor app itself yet — a building block any consumer (Editor, or a future in-game debug overlay) can use, matching `CLAUDE.md`'s own dependency-table role for Dear ImGui ("Debug overlay"). | ✅ Verified on Linux desktop and physical Pi4 hardware (`samples/e1_imgui_overlay`) |
| E2 | `editor/` app skeleton + Scene View | New `editor/` directory at repo root (sibling to `engine/`, `samples/`, `tools/` — a first-class app, not a build-time tool like the Cooker), its own `CMakeLists.txt`, links `engine_core`. Loads an existing `.scene.json` via `engine::scene::LoadScene` (read-only, no physics callback yet) and renders it with `ForwardLitPipeline`, camera navigable (orbit or free-look). Proves "Editor = separate application, client of Engine Core, same RHI" end to end. | ✅ Verified on Linux desktop and physical Pi4 hardware (`editor/`, keyboard-navigable orbit camera: A/D yaw, W/S pitch, Up/Down zoom) |
| E3 | Inspector panel + entity selection | ImGui panel listing the loaded scene's entities (`ecs::World`); clicking one selects it and shows its components' current values (Transform position/rotation/scale, Mesh GUID, Collider, Rigidbody flags), read-only first, then editable (`ImGui::DragFloat3` etc. mutating the live ECS component, visible immediately in the Scene View). | ✅ Verified on Linux desktop and physical Pi4 hardware (entity selection + live Transform/Mesh/Collider editing confirmed via simulated click + type on real Pi4 hardware, `wlrctl`/`wtype`) |
| E4 | Scene saving | "Save" writes the current (possibly edited) entity state back to a `.scene.json` using the existing schema (`engine::scene`/`SceneDocument`'s JSON format) — round-trips through the same format `LoadScene` already reads, no new format invented. | Not started |
| E5 | Console panel | Captures engine log output into a scrollable ImGui panel (recent lines, errors highlighted) instead of only stderr in an external terminal. | Not started |
| E6 | Asset Browser (minimal) | Lists `assets/` (source) and the cooked output directory in a panel; selecting a source asset shows its `.meta` GUID. No "New Script" template generation yet (scripting-from-editor is its own larger sub-feature, deferred further). | Not started |
| E7 | Project Hub | Multi-project management (engine installed once, each project references the shared installation) — deferred until there's an actual second project to manage; single-project usage is fine until then. | Not started |
| E8 | Build/Play/Debug pipeline | "Play" triggers an incremental CMake build + incremental Cooker run as one flow, shows compile/cook errors in the Console panel (E5), launches the game with the open scene on success; "Play in Debug" launches under GDB. The largest, most self-contained piece — depends on the rest of the Editor existing to be worth building. | Not started |

## Explicitly excluded from every step above (matches `docs/01` section 6.1)

No integrated code editor, no runtime C++ hot-reload — writing code always happens in an
external IDE (VSCode by default). `compile_commands.json` generation is already unconditional
(`CMAKE_EXPORT_COMPILE_COMMANDS=ON` project-wide, root `CMakeLists.txt`) and not
Editor-specific; per-project `.vscode/tasks.json`/`launch.json` generation is part of E8,
not earlier.

## Updating this document

Flip a row's Status to "✅ Verified on Linux desktop and physical Pi4 hardware" (matching
the M0-M7 table style in `CLAUDE.md`) as each step ships, and add a one-line note if a
step's actual scope shifted during implementation (mirroring how `CLAUDE.md` section 8's
"Preliminary decisions" track deferred scope for the engine milestones). Re-sequencing
later steps (e.g. moving E6 before E5) is fine if it turns out to make more sense once
earlier steps are done — this table is a working plan, not a contract.
