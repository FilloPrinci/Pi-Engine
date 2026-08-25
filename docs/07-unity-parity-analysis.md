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
| Scene View navigation | Partial | Mouse-look free camera, focus-on-selection, gizmos | Keyboard orbit camera (A/D/W/S/Up/Down) plus mouse-look (hold the right mouse button and drag to orbit -- additive, not a replacement for the keyboard controls) — a translate gizmo now exists (see below) but there's still no focus-on-selection shortcut |
| Object selection/manipulation | Partial | Click-select in viewport, move/rotate/scale gizmos, multi-select | Click-select directly in the viewport now works (ray vs. each entity's bounding sphere, closest hit wins) alongside the existing Scene-panel list click, and a translate gizmo (drag a colored axis to move) appears on the selection — real progress, but still translate-only (no rotate/scale gizmo), world-space axes only (no local/global toggle), single selection only, and picking is a sphere approximation so a very flat/elongated mesh (e.g. a thin ground slab) has a picking volume noticeably larger than its visible silhouette |
| Hierarchy (parent-child) | Partial | Nested transforms, drag-to-reparent | `TransformComponent` has a `parent` field, composed into a real world matrix by `World::GetWorldMatrix()` (Editor Scene View and Play Mode both render through it); the Scene panel shows entities as an indented tree instead of a flat list; both the Inspector's "Parent" combo box and now drag-and-drop *in* the Hierarchy panel itself (drag one row onto another to reparent, or onto the empty space below the tree to un-parent to root) reparent through the same cycle-safe `SetParentWithUndo()` helper (`World::IsDescendantOf()`-checked) — what's still missing next to Unity is per-node collapse/expand (always fully expanded, an accepted v1 simplification) |
| Inspector — component add/remove | Have | "Add Component" dropdown, remove via context menu | "Add Component" row (Mesh/Collider/Rigidbody/Light, only offering types the entity doesn't already have) and a "Remove X" button per component section, both fully undoable ("make everything the Editor shows manageable" phases 1 and 5) — Create Empty/Create Cube and the Hierarchy's own "Delete" are undoable too, including a deleted entity's children being correctly re-parented back on Undo |
| Undo/Redo | Have | Ctrl+Z/Y across the whole Editor | A generic `UndoStack` (`editor/UndoStack.h`) backs every Inspector field edit (Transform/Mesh/Collider/Rigidbody/Light), Parent reparenting, the viewport gizmo drag, Add/Remove Component, and Create/Delete Entity (phase 5 — the last of these needed a `std::shared_ptr<Entity>` "cell" shared across a whole undo/redo chain, since the same conceptual object gets a genuinely new `Entity` handle, different generation, every time it's destroyed and recreated) — Ctrl+Z/Ctrl+Y (also Ctrl+Shift+Z) plus Undo/Redo buttons in the Editor's info window; a whole drag-release gesture is one undo step, not one per intermediate value. Not covered yet: Save/Play aren't undoable (neither is meaningful to undo), material property edits/assignment aren't (a separate asset file, not part of the scene document — an accepted gap, same shape as "Save/Play aren't undoable"), and there's no visible undo *history* list (just linear back/forward, matching Unity's own default keyboard behavior, just without its optional history window) |
| Scene saving | Have | Ctrl+S, dirty-flag warning on close | Explicit Save button (E4), no dirty-flag/unsaved-changes prompt |
| Asset Browser | Partial | Thumbnails, folders, drag-drop into Scene View/Inspector, search | Flat two-column file list (source/cooked), GUID display only, no thumbnails, no drag-drop, no subfolders (E6) |
| Console panel | Have (minimal) | Filtering by severity, click-to-source, stack traces, collapse duplicates | Scrollback with stderr highlighted red, Clear button, plus "Info"/"Errors" filter checkboxes (this project's own logging only has that one severity axis -- `std::printf` vs `std::fprintf(stderr, ...)`, no separate Warning level) and a "Collapse" toggle (merges immediately-adjacent identical lines into one row with a "(xN)" count, a pure display-time transform, `Console.h`/`.cpp` themselves untouched) — no click-to-source (deliberately not attempted: no log call site carries structured file:line metadata to jump to without a much larger rewrite of every `printf`/`fprintf` call across the engine) or stack traces (E5) |
| Project Hub / multi-project | Partial | Installed-once engine, many independent project folders, per-project engine version pin | Recent-scenes list + one-process-per-scene relaunch, plus (post-E8, the user's own explicit request) real "Open Scene File" (arbitrary path, not just a previously-opened one) and "New Scene File" (writes a blank scene through the same `SaveScene()` path "Save" uses, then opens it) actions in the Project Hub panel; still no real multi-project packaging since Pi-Engine has no install/export target at all (E7) |
| Play Mode | Partial (deliberate) | Simulates in-place inside the Editor process, editable while playing, exact same window | Launches a genuinely separate process (`editor_play`) with no Editor panels (E8) — **not a gap to close**: there is no C++ hot-reload in this engine, so in-place simulation was never architecturally possible; closing it would mean adding hot-reload, a far larger undertaking than Play Mode itself |
| Data-driven scripting | Partial | Any MonoBehaviour can be added to any GameObject from the Editor, no recompile | Scene JSON now has a `"scripts": [name, ...]` field (`EntityDesc::scriptNames`) that Play Mode (`editor_play`) attaches via `ScriptRegistry::Create()` and runs every frame (Script phase, before Physics) — real progress, but still not attachable from the Inspector (only by hand-editing JSON) and still bounded by the same structural limit as the row below: a scene can only reference a script type already compiled into `editor_play`'s own binary (today: `editor/scripts/RotateScript.h`), never truly hot-loadable code |
| Prefab Editor UI | Missing | Visual Prefab editing, nested prefabs, overrides | `engine::scene::Prefab`/`.prefab.json` exist and instantiate correctly (M7), but only from code (`samples/m7_scene_and_prefab`) — no Editor UI to create/edit one |
| Material/Shader system | Partial | Material assets, shader graphs, per-material parameter editing in Inspector | `renderer/MaterialData.h` is a genuinely generic `{shaderName, properties}` model (`ShaderPropertySchema.h`'s hand-written per-shader property table stands in for real shader reflection), not a fixed tint field -- `Color`/`Float`/`Texture` property types, the Inspector renders the right widget per property and edits persist to the `.material.json` file on gesture-end. Five shaders registered so far, each its own concrete pipeline class (`CLAUDE.md` rule 7 still honored -- never an uber-shader): `ForwardLitColor`/`ForwardLitTexturedColor` (flat/textured tint, unlit), `ForwardLitShaded` (per-fragment Blinn-Phong), `ForwardVertexLit`/`ForwardVertexLitTextured` (the same lighting per-vertex instead, the engine's default material). The Inspector can now *assign* an existing material (a combo over every `*.material.json` under `assets/`) and *create a brand-new one* from scratch (phase 4 — pick a registered shader, a file path, "Create": writes the file with the shader's own declared defaults plus a fresh `.meta` GUID via `engine::asset::GenerateAndWriteAssetMetaGuid()`, then assigns it immediately, no relaunch needed) — no more hand-editing a GUID into the scene JSON to use a material at all. Remaining gap: no shader graph -- still a fixed, small set of hand-written shaders, just with generically-editable per-instance data and in-Editor creation now instead of one hardcoded tint |
| Animation | Missing | Animator/Animation windows, state machines, timeline | No animation system in the engine at all yet |
| Particles | Missing | Particle System component + visual editor | No particle system in the engine at all yet |
| UI system | Missing | uGUI/UI Toolkit + visual Canvas editing | No 2D/UI rendering system in the engine at all yet |
| Lighting / PBR | Partial (PBR half still a non-goal) | Realtime GI, light probes, reflection probes | Lighting phase A (docs/01 section 8.3's "Low-Poly Retro" profile) is done: `LightComponent` (Directional/Point, up to `kMaxLights`=4 simultaneous, docs/01's own indicative budget), `ForwardLitShadedPipeline` (per-fragment Blinn-Phong -- ambient + N·L diffuse + a fixed-shininess specular term), a per-frame lighting UBO (this project's first GPU resource written more than once, double-buffered across frames-in-flight). Phase A follow-up (the user's own explicit request): `ForwardVertexLitPipeline`/`ForwardVertexLitTexturedPipeline` (sixth/seventh concrete pipelines) evaluate the identical formula per-*vertex* instead, and are now the engine's default/base lit material (optionally textured, tint as albedo when untextured); an entity with no material at all now renders a flat purple/violet "missing material" indicator instead of `ForwardLitPipeline`'s old debug normal-color fallback (that pipeline itself untouched -- still what every M0-M7 sample uses directly). The PBR half of this row is still the explicit non-goal `CLAUDE.md` section 1 names -- `ForwardPlusPBRPipeline`/metallic-roughness/IBL remain unimplemented, this is the "Low-Poly Retro" profile's own lighting model, not a scaled-down PBR one. Lighting phase B (directional-only, the user's own scoping decision) added a static shadow map: `rhi::RHIShadowMap` (this project's first render-to-texture RHI resource) + `renderer::ShadowDepthPipeline` (eighth concrete pipeline) bake a depth-only shadow map once, at load time, from the first `LightComponent` found that's `Directional` + `isStatic` + `castsShadow`; all three lit pipelines sample it (comparison sampler, hardware PCF) at their own existing lighting granularity (per-fragment for `ForwardLitShaded`, per-vertex for the two `ForwardVertexLit*` pipelines). A point-light cube-map variant is analyzed but not built (`engine/include/engine/rhi/README.md`'s `RHIShadowMap` entry) |
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
4. ~~Undo/Redo.~~ **Done** (`editor/UndoStack.h`, a generic `{undo, redo}` closure stack)
   — every Inspector field, Parent reparenting, and the gizmo drag now undo/redo as a
   single step per gesture (Ctrl+Z/Ctrl+Y, or the Undo/Redo buttons). What's left
   (a visible history list, rather than just linear back/forward) is polish, not the
   core payoff this was meant to unlock -- an Editor that no longer punishes
   experimentation.
5. ~~Material assets.~~ **Done** — shipped first as a flat-tint-only v1, then extended the
   same session to a genuinely generic property system (`renderer/MaterialData.h`:
   `{shaderName, properties}`, `ShaderPropertySchema.h`'s hand-written per-shader property
   table) per explicit follow-up direction: a material is an instance of a shader, so
   whatever properties the shader declares should be editable, texture included, not just
   a hardcoded tint. Two shaders exist so far, each its own concrete pipeline class
   (`CLAUDE.md` rule 7 still honored): `ForwardLitColorPipeline` (flat tint) and
   `ForwardLitTexturedColorPipeline` (texture * tint, which needed real `RHITexture`/
   descriptor-set plumbing added to both `editor/main.cpp` and `editor/play_main.cpp` --
   previously only `samples/m7_textures` had any). The Inspector's "Material" section is
   dynamic: it renders the right widget per declared property (color picker, drag-float,
   or an asset picker for a texture reference) and persists edits to the `.material.json`
   file on gesture-end. Verified end-to-end on Pi4: a flat-tint red cube and a
   checker-textured, tinted quad render correctly side by side in both the Editor's Scene
   View and Play Mode (the red cube also visibly orbiting its RotateScript'd parent,
   proving hierarchy + scripting + both material shaders compose together), at 59-60 FPS
   capped and 178-211 FPS uncapped. What's left, deliberately deferred: any Inspector UI to
   *assign/unassign* a material to an entity (still hand-edited into the scene JSON), and
   a shader graph (still a fixed, small set of hand-written shaders, not user-composable).

Everything else in the table (Animation, Particles, UI system, NavMesh, Terrain, full
Asset Store-style packages, Editor scripting API) is real Unity functionality this Editor
doesn't have, but none of it blocks the vertical-slice-and-beyond goal the way the five
items above did — they're "eventually, if the project grows toward needing them," not
"next." With all five of this list's original items done, the next round of priorities
(texture-referencing materials, Inspector-driven material/script assignment, drag-and-drop
Hierarchy reparenting) is picked fresh from the table above rather than pursued as a fixed
plan, same discipline as the milestone roadmap itself.
