# Editor — Unity Parity Analysis

> Requested alongside Editor step E8 ("analyze what it would take to bring the Editor's
> functionality as close as possible to Unity's"). This is an analysis document, not a
> roadmap commitment — unlike `docs/06-editor-roadmap.md` (which tracks a staged plan this
> project is actually executing), the items below are prioritized by relevance to
> Pi-Engine's actual stated goal (`CLAUDE.md` section 1: retro-style low-poly 3D games,
> accessible, Pi4/Pi5-optimized), not by "what would make this Unity." Picking any of these
> up as real work happens the same way M6+/Editor steps did: one at a time, by explicit
> decision, not because this document says so.

## How to read this table

- **Have**: functionally present today, even if minimal.
- **Partial**: something exists but falls meaningfully short of Unity's version.
- **Missing**: nothing exists yet.
- **Non-goal**: `docs/01`/`CLAUDE.md` already rule this out on purpose — listed so the gap
  reads as a deliberate choice, not an oversight.

| Area | Status | Unity has | Pi-Engine has |
|---|---|---|---|
| Scene View navigation | Partial | Mouse-look free camera, focus-on-selection, gizmos | Keyboard-only orbit camera (A/D/W/S/Up/Down), no gizmos, no focus shortcut |
| Object selection/manipulation | Partial | Click-select in viewport, move/rotate/scale gizmos, multi-select | Click-select in a list panel only (no viewport picking), Inspector `DragFloat` fields, single selection only |
| Hierarchy (parent-child) | Missing | Nested transforms, drag-to-reparent | No parent field on `TransformComponent` at all — every entity is a root |
| Inspector — component add/remove | Missing | "Add Component" dropdown, remove via context menu | Inspector only edits components an entity was spawned with; nothing can be added or removed at runtime |
| Undo/Redo | Missing | Ctrl+Z/Y across the whole Editor | Does not exist in any form |
| Scene saving | Have | Ctrl+S, dirty-flag warning on close | Explicit Save button (E4), no dirty-flag/unsaved-changes prompt |
| Asset Browser | Partial | Thumbnails, folders, drag-drop into Scene View/Inspector, search | Flat two-column file list (source/cooked), GUID display only, no thumbnails, no drag-drop, no subfolders (E6) |
| Console panel | Have (minimal) | Filtering by severity, click-to-source, stack traces, collapse duplicates | Scrollback with stderr highlighted red, Clear button — no filtering/collapsing/click-to-source (E5) |
| Project Hub / multi-project | Partial | Installed-once engine, many independent project folders, per-project engine version pin | Recent-scenes list + one-process-per-scene relaunch; no real multi-project packaging since Pi-Engine has no install/export target at all (E7) |
| Play Mode | Partial (deliberate) | Simulates in-place inside the Editor process, editable while playing, exact same window | Launches a genuinely separate process (`editor_play`) with no Editor panels (E8) — **not a gap to close**: there is no C++ hot-reload in this engine, so in-place simulation was never architecturally possible; closing it would mean adding hot-reload, a far larger undertaking than Play Mode itself |
| Data-driven scripting | Missing | Any MonoBehaviour can be added to any GameObject from the Editor, no recompile | `ScriptComponent`s are attached in C++ (`REGISTER_SCRIPT`/`EXPOSE`, `docs/01` section 8) — scene JSON (`EntityDesc`) has no script-attachment field, so Play Mode (E8) can render/simulate physics for a scene but never runs its gameplay scripts |
| Prefab Editor UI | Missing | Visual Prefab editing, nested prefabs, overrides | `engine::scene::Prefab`/`.prefab.json` exist and instantiate correctly (M7), but only from code (`samples/m7_scene_and_prefab`) — no Editor UI to create/edit one |
| Material/Shader system | Missing (structural) | Material assets, shader graphs, per-material parameter editing in Inspector | No material asset concept at all — `ForwardLitPipeline`/`ForwardLitTexturedPipeline` are hardcoded C++ classes (`CLAUDE.md` rule 7: "every rendering pipeline is a separate concrete class, never an uber-shader with branching"); adding a data-driven material would be a Renderer-level change, not an Editor-level one |
| Animation | Missing | Animator/Animation windows, state machines, timeline | No animation system in the engine at all yet |
| Particles | Missing | Particle System component + visual editor | No particle system in the engine at all yet |
| UI system | Missing | uGUI/UI Toolkit + visual Canvas editing | No 2D/UI rendering system in the engine at all yet |
| Lighting / PBR | Missing (partial non-goal) | Realtime GI, light probes, reflection probes | `CLAUDE.md` section 1 lists a PBR profile as an explicit *optional secondary target* — `ForwardPlusPBRPipeline` is named in the architecture (`CLAUDE.md` section 4) but not implemented; low-poly unlit/lit-only is the actual primary target, so this is intentionally deferred rather than forgotten |
| NavMesh / pathfinding | Missing | NavMesh baking + NavMeshAgent | Not part of any milestone or design doc so far |
| Terrain | Missing | Terrain tools, heightmap sculpting | Not part of any milestone or design doc so far — arguably lower priority given the low-poly/retro target, where hand-authored meshes are more the point |
| Profiler | Missing | CPU/GPU/memory frame-by-frame profiler window | No in-Editor profiling UI; `docs/01`'s hardware-budget checklist (`CLAUDE.md` section 3) is currently applied by hand per feature, not measured live |
| Version control integration | Missing | Perforce/PlasticSCM/Git status in Project window | Scene/Prefab JSON is deliberately git-friendly/diffable (`docs/01` section 12.2) but the Editor itself has no VCS awareness |
| Packages / Asset Store | Non-goal | Package Manager, Asset Store marketplace | Explicitly out of scope — Pi-Engine has no package ecosystem or distribution story, and none is planned |
| Editor extensibility (custom `EditorWindow`/`PropertyDrawer`) | Missing | C# Editor scripting API | The Editor is a fixed C++ application; no plugin/extension API exists or is planned near-term |
| Integrated code editor / hot-reload | Non-goal | None (Unity also shells out to an external IDE) | Matches Unity's own choice — VSCode is the intended external IDE (`docs/06` "Explicitly excluded") |

## What actually matters next, for *this* project

Ranked by how much they'd help the stated goal (retro low-poly indie games on Pi4/Pi5),
not by how large the gap looks above:

1. **Viewport object picking + a translate gizmo.** The single biggest everyday workflow
   gap — right now every position edit goes through the Inspector's numeric `DragFloat3`,
   never a click-and-drag in the 3D view itself. This is scene-editing *feel*, which
   matters far more for an indie-accessible engine than most of the "Missing" rows above.
2. **Hierarchy / parent-child transforms.** Almost every real scene needs grouping (a
   vehicle's wheels, a character's held item). This is an ECS-level change first
   (`TransformComponent` needs a parent field and the renderer/physics needs to compose
   world transforms), not just an Editor UI change — bigger than it looks from the table.
3. **Data-driven script attachment.** Without this, Play Mode (E8) can only ever preview
   physics/rendering, never real gameplay — the single biggest thing standing between
   today's Editor and "press Play and actually play the game."
4. **Undo/Redo.** Cheap relative to its payoff once (1)-(2) exist — an Editor without undo
   actively discourages experimentation, which cuts against "accessible" (`CLAUDE.md`
   section 1) more than almost anything else on this list.
5. **Material assets.** Structural and invasive (touches the Renderer, not just the
   Editor), but unlocks per-object color/texture tweaking without a C++ recompile — a
   prerequisite for a non-programmer using this engine at all.

Everything else in the table (Animation, Particles, UI system, NavMesh, Terrain, full
Asset Store-style packages, Editor scripting API) is real Unity functionality this Editor
doesn't have, but none of it blocks the vertical-slice-and-beyond goal the way the five
items above do — they're "eventually, if the project grows toward needing them," not
"next."
