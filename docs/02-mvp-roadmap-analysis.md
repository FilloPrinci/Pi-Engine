# Analysis — MVP Roadmap and Milestones Toward the Vertical Slice

**Phase:** Analysis
**Status:** Draft v1
**Reference document:** `01-engine-design-rpi.md` (all architectural decisions cited here are defined and justified there)

---

## 1. Goal of this phase

Translate the design phase's architectural decisions into a **concrete MVP feature set** and an **incremental, testable milestone roadmap** — the bridge between "we've decided how it should be built" and "what do we build first, in what order".

## 2. Target of the very first playable milestone

Not the final MVP, but the first concrete goal: a **minimal vertical slice** — the entire pipeline (rendering → physics → scripting → input) working together, even on a single cube, instead of many superficial isolated features. It validates the whole architecture decided during design, not just one piece at a time.

Concrete example of what must happen: *move a cube with the keyboard, press Space to jump (a real physics impulse), touch another object and something happens (a collision event handled by a script).*

---

## 3. Preliminary decisions to avoid getting stuck

- **Editor: out of this roadmap.** All the milestones below are **code only** ("library mode", section 6 of the design document) — the Editor is built after the core is stable and tested, not in parallel.
- **Asset Cooker: deferred until after the first vertical slice.** To reach the target milestone quickly, the early milestones load "raw" assets at runtime (uncooked glTF, direct WAV). The offline Cooker (section 12 of the design document) is its own milestone, right after the vertical slice, before scaling up to bigger scenes.
- **Primary development platform: Linux x86_64 desktop**, for fast iteration (compile time, RenderDoc, more mature Vulkan validation layers there), with **verification on physical Pi4 hardware at every milestone**, not only at the end — to catch ARM/TBDR-specific issues early (consistent with the "Design constraints" checklist in the design document).

---

## 4. Milestone roadmap toward the vertical slice

| # | Milestone | What it shows/validates | Systems involved | Exit criterion |
|---|---|---|---|---|
| **M0** | *Hello Vulkan* | Colored triangle on screen | RHI, Platform Layer (`SDL2DisplayBackend` only) | RHI initializes, swapchain works, first Vulkan pipeline compiles and runs, runs on Pi4 |
| **M1** | *Hello Mesh* | Static cube/mesh loaded from glTF, orbiting camera | RHI, Renderer (basic Low-Poly profile) | Minimal working glTF loader, Low-Poly Retro pipeline (simple forward, unlit) active, correct camera math |
| **M2** | *Hello Scene* | A handful of objects in the scene, frustum culling active | ECS, Job System, Renderer | Minimal ECS (Transform + Mesh component) working, first real use of the Job System (parallel culling across multiple objects) |
| **M3** | *Hello Script* | An object moves via keyboard | Script System, Input System | `ScriptComponent`, `ComponentHandle<T>`, `REGISTER_SCRIPT` working; Input System (keyboard, gamepad can follow later) |
| **M4** | *Hello Physics* | The cube falls under gravity and comes to rest on a plane | Physics, Job System | Working Jolt↔Job System adapter, phase barriers respected (design document, section 9), fixed timestep |
| **M5 — Target** | *Vertical Slice* | Move the cube with the keyboard, Space to jump (physics impulse), touch an object and a script reacts (`OnCollisionEnter`/`OnTriggerEnter`) | All core systems together | Script reads input, applies physics impulses, receives collision callbacks — all within the same frame, no race conditions |

Every milestone has a clean exit criterion ("compiles, runs, you can see/hear the thing described"). If a milestone drags on too long or requires pulling in pieces of later ones, that's a signal it needs to be split further, not pushed through by force.

---

## 5. What does NOT go into the vertical slice (and that's fine)

Systems already well designed during the design phase, but deliberately excluded from this first roadmap:

- Audio (miniaudio, dedicated thread)
- Gamepad (full Action Mapping, hot-plug)
- LOD, bloom/post-processing, PBR profile
- Prefab
- Full Asset Pipeline / Cooker
- Editor

They get added **after** the core (rendering + physics + scripting) is proven solid on real hardware. Introducing them earlier would risk hiding problems in the core behind the complexity of peripheral features.

---

## 6. Next steps

1. **Post-vertical-slice milestones** (to be detailed once M5 is reached): minimal Asset Cooker, gamepad, basic audio, LOD.
2. **Technical analysis in preparation for Claude Code** — for each milestone M0-M5: repository structure, exact interfaces of the modules involved, code conventions, external dependencies to integrate (Vulkan SDK, SDL2, Jolt, glTF loader).
3. **Context for Claude Code** — a condensed context document to provide as a reference during assisted development.
4. **Development on Claude Code** — incremental implementation following exactly M0 → M5.
5. **Testing and improvements** — real profiling on physical Pi4 hardware at every milestone, not only at the end of the roadmap.
