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
| Scene View navigation | Partial | Mouse-look free camera, focus-on-selection, gizmos | Keyboard-only orbit camera (A/D/W/S/Up/Down) — unchanged; a translate gizmo now exists (see below) but there's still no mouse-look and no focus-on-selection shortcut |
| Object selection/manipulation | Partial | Click-select in viewport, move/rotate/scale gizmos, multi-select | Click-select directly in the viewport now works (ray vs. each entity's bounding sphere, closest hit wins) alongside the existing Scene-panel list click, and a translate gizmo (drag a colored axis to move) appears on the selection — real progress, but still translate-only (no rotate/scale gizmo), world-space axes only (no local/global toggle), single selection only, and picking is a sphere approximation so a very flat/elongated mesh (e.g. a thin ground slab) has a picking volume noticeably larger than its visible silhouette |
| Hierarchy (parent-child) | Partial | Nested transforms, drag-to-reparent | `TransformComponent` has a `parent` field, composed into a real world matrix by `World::GetWorldMatrix()` (Editor Scene View and Play Mode both render through it); the Scene panel shows entities as an indented tree instead of a flat list; the Inspector has a "Parent" combo box for reparenting (cycle-safe via `World::IsDescendantOf()`) — what's missing next to Unity is drag-and-drop reparenting *in* the Hierarchy panel itself (combo box only for now) and per-node collapse/expand (always fully expanded, an accepted v1 simplification) |
| Inspector — component add/remove | Missing | "Add Component" dropdown, remove via context menu | Inspector only edits components an entity was spawned with; nothing can be added or removed at runtime |
| Undo/Redo | Missing | Ctrl+Z/Y across the whole Editor | Does not exist in any form |
| Scene saving | Have | Ctrl+S, dirty-flag warning on close | Explicit Save button (E4), no dirty-flag/unsaved-changes prompt |
| Asset Browser | Partial | Thumbnails, folders, drag-drop into Scene View/Inspector, search | Flat two-column file list (source/cooked), GUID display only, no thumbnails, no drag-drop, no subfolders (E6) |
| Console panel | Have (minimal) | Filtering by severity, click-to-source, stack traces, collapse duplicates | Scrollback with stderr highlighted red, Clear button — no filtering/collapsing/click-to-source (E5) |
| Project Hub / multi-project | Partial | Installed-once engine, many independent project folders, per-project engine version pin | Recent-scenes list + one-process-per-scene relaunch; no real multi-project packaging since Pi-Engine has no install/export target at all (E7) |
| Play Mode | Partial (deliberate) | Simulates in-place inside the Editor process, editable while playing, exact same window | Launches a genuinely separate process (`editor_play`) with no Editor panels (E8) — **not a gap to close**: there is no C++ hot-reload in this engine, so in-place simulation was never architecturally possible; closing it would mean adding hot-reload, a far larger undertaking than Play Mode itself |
| Data-driven scripting | Partial | Any MonoBehaviour can be added to any GameObject from the Editor, no recompile | Scene JSON now has a `"scripts": [name, ...]` field (`EntityDesc::scriptNames`) that Play Mode (`editor_play`) attaches via `ScriptRegistry::Create()` and runs every frame (Script phase, before Physics) — real progress, but still not attachable from the Inspector (only by hand-editing JSON) and still bounded by the same structural limit as the row below: a scene can only reference a script type already compiled into `editor_play`'s own binary (today: `editor/scripts/RotateScript.h`), never truly hot-loadable code |
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

1. ~~Viewport object picking + a translate gizmo.~~ **Done** (`editor/main.cpp`: ray-vs-
   bounding-sphere picking directly in the 3D view, plus a draggable X/Y/Z translate gizmo
   drawn via ImGui's foreground draw list) — position edits no longer have to go through
   the Inspector's numeric `DragFloat3` alone. Needed real mouse-input plumbing first
   (`InputState`/`InputSystem`/`SDL2DisplayBackend` all gained mouse support, previously
   keyboard-only) and a real bugfix along the way: a synthetic (or very fast real) click
   whose down-and-up both land inside one `PollEvents()` poll was invisible to a
   once-per-frame `SDL_GetMouseState()` query — fixed by latching "held" for that frame
   whenever a down *event* was seen, so InputSystem's press/release edges always see a
   real (if one-frame) press before a release. What's left here (rotate/scale gizmos,
   local- vs. world-space toggle, multi-select) is real Unity functionality this still
   doesn't have, but translate-only was the actual workflow gap, so it's dropped off this
   top-5 list rather than kept at the top.
2. ~~Hierarchy / parent-child transforms.~~ **Done** (`TransformComponent::parent`,
   `World::GetWorldMatrix()`/`IsDescendantOf()`, an indented Scene panel tree, an
   Inspector "Parent" combo) — grouping (a vehicle's wheels, a character's held item) now
   works: a child's position/rotation/scale are relative to its parent, composed into a
   real world matrix everywhere an entity gets rendered. Verified on Pi4 by watching a
   child cube visibly orbit its RotateScript'd parent in Play Mode across two screenshots
   — proof the composition is genuinely live, not just a data field that exists. What's
   still missing next to Unity: drag-and-drop reparenting *in* the Hierarchy panel
   (combo box only), per-node collapse/expand (always fully expanded), and physics bodies
   still don't compose through a parent (a Rigidbody+Collider entity with a parent is
   created at its local transform values as if they were world space -- a known,
   documented gap, out of scope for this pass since Jolt has no native "parent" concept
   for rigid bodies and a correct fix needs a joint/constraint or a per-frame kinematic
   copy, either one a separate, larger piece of work).
3. ~~Data-driven script attachment.~~ **Done** (scene JSON's `"scripts"` field, attached
   and run by Play Mode) — Play (E8) can now run real gameplay for any script type
   `editor_play` was built with, not just preview physics/rendering. What's left here
   (Inspector-driven attach/detach + EXPOSE()d field overrides from the scene document,
   instead of hand-editing JSON) is smaller than what shipped, so it's dropped off this
   top-5 list rather than kept at the top.
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
