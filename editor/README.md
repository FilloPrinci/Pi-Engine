# editor

Pi-Engine Editor (`docs/01-engine-design-rpi.md` section 6, staged into incremental steps
in [`docs/06-editor-roadmap.md`](../docs/06-editor-roadmap.md)) — "a separate application,
client of the Engine Core (same RHI/Vulkan as the final game)". Links `engine_core` fully,
unlike `tools/cooker` (deliberately not linked, offline-only).

**Step E2**: app skeleton + Scene View. `editor [path/to/scene.json]` loads a scene
(defaults to `assets/demo.scene.json` if no path is given) via `engine::scene::LoadScene`
— read-only, no `PhysicsWorld`/physics callback yet — and renders it through the same
`ForwardLitPipeline` every sample uses. Camera is keyboard-navigable (A/D yaw, W/S pitch,
Up/Down zoom — no mouse-look input plumbing exists yet). `engine::debug::ImGuiOverlay`
(Editor step E1) is already wired in with a small info window.

**Step E3**: Inspector panel + entity selection. The **Scene** panel lists every entity
(everything `LoadScene` spawns has a Transform); clicking one selects it. The
**Inspector** panel shows the selected entity's components -- Transform
(position/rotation/scale, rotation shown/edited as Euler degrees), Mesh (GUID, bounds
radius), Collider (shape, half-extents/radius, is-trigger), Rigidbody (body id, mass) --
whichever the entity actually has. Editing a field (`ImGui::DragFloat3`/`Checkbox`)
mutates the live `ComponentStorage` entry directly, no separate "apply" step -- visible in
the Scene View the very next frame.

**Step E4**: Scene saving. A **Save** button (in the info window) calls
`engine::scene::SaveScene()`, overwriting the currently open scene path with every
entity's live component state -- round-trips through the exact same JSON schema
`LoadScene`/`ParseSceneDocument` reads, no new format invented. One deliberate gap: a
`RigidbodyComponent` doesn't retain whether the body was static or dynamic (that flag is
only read once, at spawn time, to decide which `PhysicsWorld::CreateBody()` call to make,
then discarded) -- an entity with a Rigidbody is dropped from the saved file with a
stderr warning rather than guessing and silently writing the wrong value. Not a concern
for the Editor's own current scenes (nothing in it creates a Rigidbody -- E2's Scene View
has no `PhysicsWorld`), only for a scene document that already had one before the Editor
loaded it. No "Save As" yet (Editor step E7's Project Hub is the more natural place to
manage multiple scene files).

**Step E5**: Console panel. Captures the engine's existing stdout/stderr output
(`engine::debug::Console`, built this step) into a scrollable panel -- every
`std::printf`/`std::fprintf(stderr, ...)` call site across the engine already exists and
is captured as-is via low-level file descriptor redirection, not a new logging API
everything would need to be rewritten to use. stderr lines are highlighted red. Output is
teed, not replaced: redirecting the process's own output (`editor > log.txt 2>&1`) or just
running it interactively still works exactly as before. POSIX only (Linux/macOS) -- on
another platform the panel just stays empty rather than failing to build or run.

**Step E6**: Asset Browser (minimal). Lists `assets/` (Cooker source assets,
docs/01 section 12.1) and `assets_cooked/` (this build's Cooker output, including
`shaders/`) side by side, read once at startup via `std::filesystem`. Selecting a source
asset shows its persistent GUID via `engine::asset::TryReadAssetMetaGuid()` (the read-only
counterpart to `tools/cooker`'s `GetOrCreateAssetGuid()` -- the Editor only ever displays
an existing `.meta` sidecar, never creates one) or "no `.meta` sidecar" if none exists.
No "New Script" template generation yet -- scripting-from-editor is its own larger
sub-feature, deferred further.

**Step E7**: Project Hub (minimal -- see `ProjectHub.h`'s own comment for why
this isn't the full "shared engine installation, multiple project directories" system
docs/01 section 6.1 describes; that isn't buildable yet since Pi-Engine has no
install/export target at all). Every successful scene load is recorded to a small local
JSON file (`~/.local/share/pi-engine/recent_projects.json`); the **Project Hub** panel
lists it, most-recently-opened first. Clicking a *different* entry than the one currently
open relaunches the Editor pointed at it -- `fork()` + `execv()` against
`/proc/self/exe`, then the current process requests its own quit -- one process per open
project/scene, the same way Unity Hub and the Unity Editor are actually two separate
processes rather than one process hot-swapping its loaded project. Linux only; on another
platform the relaunch simply doesn't happen (logged, not fatal).

**Step E8**: Build/Play/Debug pipeline. A **Play** button (next to Save) runs
`cmake --build <dir> --target editor_play`, then re-cooks (`cooked_assets`/
`cooked_shaders`/`cooked_textures`) -- both incremental, CMake's own dependency tracking --
and, on success, launches `editor_play` (see `play_main.cpp`'s own comment) pointed at the
currently open scene. **Play in Debug** runs the exact same flow but wraps the launch in
`gdb -batch -ex run -ex bt` instead, so a crash prints a backtrace. Both child processes
(the `cmake`/cook steps and the launched game) inherit this process's stdout/stderr
unredirected, which are already piped into the Console panel (E5) at the OS level -- so
build errors, cook errors, and the running game's own log lines all show up there live,
no separate output-capture plumbing needed. `editor_play` is a genuinely separate
executable from `editor`, not this same binary re-invoked with a flag -- linking
`Jolt::Jolt` (needed for `physics::PhysicsWorld`) directly onto `editor` would leak its
AVX2/FMA compile flags onto every one of `editor`'s own source files, most of which have
nothing to do with physics (see `CMakeLists.txt`'s own comment). Play launches a fresh,
independent process and the Editor keeps running -- unlike Unity, this engine has no
runtime C++ hot-reload, so simulating *inside* the running Editor process was never an
option; Play Mode here means "run the game for real, in its own window", the same
structural choice Project Hub's relaunch already made for switching scenes.

**Post-E8**: data-driven script attachment (`docs/07-unity-parity-analysis.md`'s former
"data-driven scripting: missing" row). Scene JSON gained a `"scripts": [name, ...]` field
(`engine::scene::EntityDesc::scriptNames`); `editor_play` attaches each one via
`ScriptRegistry::Create()` and runs a real Script phase every frame (before Physics, same
order as `samples/m5_vertical_slice`), so Play Mode now runs actual gameplay code, not just
physics/rendering. `editor/scripts/RotateScript.h` is the one script type linked into
`editor_play` so far -- generic and reusable (unlike samples/m3-m5's own demo-specific
scripts), spinning its entity at a constant rate, `EXPOSE()`d for a future Inspector. The
demo scene's middle cube references it (`editor/assets/demo.scene.json`) as a live example.
One structural limit remains, unavoidable without runtime C++ hot-reload (docs/01 section
6.1): a scene can only reference a script type already compiled into `editor_play`'s own
binary -- "data-driven" here means *which* linked-in script to use, never truly
hot-loadable code. A scene referencing an unknown script name just skips it with a stderr
warning rather than failing to load.

**Also post-E8**: viewport object picking + a translate gizmo (`docs/07-unity-parity-
analysis.md`'s former #1 priority item). Clicking directly in the Scene View now selects
the closest entity a ray from the camera actually hits (each entity's Mesh `boundsRadius`
as a sphere, scaled by its largest scale axis -- an approximation, not exact against the
real mesh silhouette, so a very flat/elongated mesh like the ground slab has a noticeably
larger pick volume than its visible footprint) -- the existing Scene-panel list click still
works exactly as before, this is in addition to it, not instead of it. The selected
entity gets a draggable X/Y/Z translate gizmo (red/green/blue lines from ImGui's own
foreground draw list, no new RHI pipeline needed), scaled to stay a roughly constant size
on screen regardless of distance; dragging an axis moves the entity along it, computed by
projecting the mouse's screen-space movement onto that axis's own screen-space direction
(not a full 3D ray-plane intersection -- simpler, and accurate enough at gizmo scale).
Clicking empty space deselects, matching Unity's own Scene View. Needed real mouse input
first -- `InputState`/`InputSystem`/`SDL2DisplayBackend` were keyboard-only through E8,
extended with mouse position + left-button state/edges the same way keyboard already
worked. One real bug found and fixed while testing this over VNC with `wlrctl` (which has
no "hold the button for N ms" primitive, so a synthetic click's down-and-up can both land
inside a single `PollEvents()` poll): a plain `SDL_GetMouseState()` query after the event
loop would report "never held" for such a click, since both events already happened before
the query ran -- fixed by latching the held state to `true` for that frame whenever an
`SDL_MOUSEBUTTONDOWN` event was actually seen during polling, regardless of what the live
state reads afterward, so a real press-then-release pair always reaches `InputSystem`'s
edge detection even for a click faster than one frame.

**Also post-E8**: Hierarchy / parent-child transforms (`docs/07-unity-parity-analysis.md`'s
former #2 priority item). `engine::ecs::TransformComponent` gained a `parent` field
(`kInvalidEntity` = root); `engine::ecs::World::GetWorldMatrix()` composes an entity's real
world-space matrix by walking that chain, and both the Scene View and Play Mode now render
through it instead of a raw local `transform->GetMatrix()`, so a parented entity actually
renders (and picks, and gizmo-drags) at its true world position, not its raw local offset.
The Scene panel shows the result as an indented tree instead of a flat list (root entities
at the top level, children nested underneath, always fully expanded -- see
`renderEntityNode`'s own comment on why per-node collapse/expand isn't worth the added risk
at the scene sizes this project actually has, especially after a real indent-leak bug
showed up testing an `ImGui::TreeNodeEx()`-based version). The Inspector gained a "Parent"
combo box for reparenting, refusing any choice that would create a cycle
(`World::IsDescendantOf()` rejects both the entity itself and any of its own descendants).
Scene JSON gained a `"parent"` field (an index into the same document's `entities` array,
since entities have no persistent id of their own) -- `SpawnEntities()` resolves it in a
second pass once every entity in the batch already exists, so both forward and backward
references work; `positionOffset` (used by `Prefab::Instantiate()`) only ever applies to a
root entity, never a child, since a child's position is already relative to its parent, not
world space. `editor/assets/demo.scene.json` gained a small child cube parented to the
RotateScript'd middle cube as a live example -- verified genuinely composing (not just
existing as a data field) by watching it visibly orbit its spinning parent in Play Mode
across two Pi4 screenshots. One deliberate, documented gap: physics bodies still don't
compose through a parent -- a Rigidbody+Collider entity with a parent is created at its
local transform values as if they were world space, since Jolt has no native "parent"
concept for rigid bodies and a correct fix needs a joint/constraint or a per-frame
kinematic copy, out of scope for this pass.

**Also post-E8**: Undo/Redo (`docs/07-unity-parity-analysis.md`'s former #4 priority item).
`editor/UndoStack.h` is a small, generic `{undo, redo}` closure stack -- deliberately not
per-field-type, so every editable value in the Inspector (Transform's Position/Rotation/
Scale, Mesh's Bounds radius, Collider's Half extents/Radius/Is trigger, Rigidbody's Mass),
Parent reparenting, and the viewport gizmo drag all push through the exact same `Push()`
call. Each closure re-resolves its own `ecs::Entity -> component` pointer fresh when it
actually runs rather than capturing a raw pointer up front (CLAUDE.md rule 4 -- component
storage can move between frames), so an undo/redo still targets the right entity correctly
even if a different one is selected by the time it runs. `TrackFieldEdit()` (`editor/
main.cpp`) is the glue for every ImGui-widget-driven edit: `ImGui::IsItemActivated()`
captures the field's value when a drag/type gesture starts, `IsItemDeactivatedAfterEdit()`
pushes one combined undo step when it ends -- an entire click-drag-release becomes one
undo step, not one per intermediate value the widget reports mid-drag, matching Unity's
own Inspector undo granularity. Reparenting and the gizmo drag push directly instead
(reparenting is an instant, discrete action with nothing to batch; the gizmo drives
`transform->position` from raw mouse state, not an ImGui widget, so there's no
`IsItemActivated()`/`IsItemDeactivatedAfterEdit()` to hook -- `dragStartEntityPosition`,
already captured when the drag begins for the drag math itself, plays the same role).
Ctrl+Z/Ctrl+Y (also Ctrl+Shift+Z) and Undo/Redo buttons in the "Pi-Engine Editor" info
window both work, gated on `!ImGuiIO::WantCaptureKeyboard` so Ctrl+Z while actually typing
in a field's own text-edit box means "undo my typing there" (ImGui's own behavior)
instead of also firing the Editor's undo stack. Verified on Pi4: an Inspector Position
edit (double-click into text-edit mode + type + Enter, the same wtype-based technique
already used for E3 Inspector testing since a real click-drag can't be scripted over VNC)
and a Parent reparent (visibly moved the child into the Scene tree, then back out) both
round-tripped correctly through Undo then Redo.

**Also post-E8**: a real docked panel layout (user-provided reference sketch: Hierarchy
left, Inspector right, Assets/Console/Project Hub tabbed along the bottom, the 3D Scene
view filling the middle -- the classic Unity arrangement). Needed vcpkg's imgui
`docking-experimental` feature (`vcpkg.json`) -- the plain `imgui` feature set doesn't
build `DockBuilder`/`DockSpace` support at all. `ImGuiConfigFlags_DockingEnable` is set
only in `editor/main.cpp`, not inside `engine::debug::ImGuiOverlay` itself, since that
class is shared by non-Editor consumers (`samples/e1_imgui_overlay`) that have no reason
to opt into docking. The default layout is built once, in code, via `imgui_internal.h`'s
`DockBuilder*` API (the standard, if technically "internal", way every ImGui-based tool
sets up a default docked arrangement) -- `Pi-Engine Editor` (info/actions) stacked above
`Hierarchy` on the left, `Inspector` alone on the right, `Console`/`Assets`/`Project Hub`
tabbed together along the bottom, and the central region deliberately left with no window
docked into it at all. That central region uses `ImGuiDockNodeFlags_PassthruCentralNode`
(host window: `ImGuiWindowFlags_NoBackground`) specifically so the 3D scene, still
rendered the same way it always has been (no render-to-texture, no Vulkan changes at
all -- the panels are simply opaque windows drawn on top of the existing full-window 3D
render, exactly as the old floating-window layout already did), shows through
unobstructed, and so the *existing* `WantCaptureMouse`-gated viewport picking/gizmo code
needs zero changes: a click that lands in that empty central area was already correctly
seen as "not over an ImGui window" before docking existed. `DockBuilderGetNode()`/
`IsSplitNode()` mean the layout is only ever *built* the first time this dockspace ID has
no usable data yet (a genuinely fresh launch, or `imgui.ini` deleted) -- once built,
ImGui's own `imgui.ini` persistence takes over exactly like every other panel's position
already did, so a user who manually drags a panel elsewhere keeps that arrangement across
restarts. Renamed two panels to match Unity's own naming (and the reference sketch): the
entity list ("Scene" through E3-E8) is now "Hierarchy", and the Asset Browser ("Asset
Browser" through E6-E8) is now "Assets".

**Also post-E8**: material assets (`docs/07-unity-parity-analysis.md`'s former #5,
originally-last priority item), first shipped scoped to a flat `tintColor` only, then
extended the same session to a genuinely generic property system per explicit follow-up
direction: "a material is an instance of a shader -- whatever properties the shader
declares should be editable, texture included, not just a hardcoded tint."
`engine::renderer::MaterialData` (`.material.json`) is now `{shaderName, properties}` --
`properties` is a `name -> {type, value}` map (`ShaderPropertySchema.h`'s
`ShaderPropertyType`: Color/Float/Texture), not a fixed field, so a material can carry
whatever its target shader declares. `ShaderPropertySchema.h` is the fixed, hand-written
table describing each shader's own properties (this engine's stand-in for real shader
reflection, which it doesn't have) -- each entry maps 1:1 to one of the concrete pipeline
classes (`ForwardLitColorPipeline`: `tintColor`; `ForwardLitTexturedColorPipeline`:
`tintColor` + `albedoTexture`), never a single shader branching at runtime on a material's
data (`CLAUDE.md` rule 7 stays intact -- the genericness lives entirely in this data/
dispatch layer, see that header's own comment). `engine::ecs::MeshComponent` gained a
`materialGuid` field (default `kInvalidAssetGuid`, meaning "no material assigned"); scene
JSON gained a matching `"material": {"guid": "..."}` block, parsed/written by
`SceneDocument.cpp` the same shape as the existing `"mesh"` block. Both `editor/main.cpp`
and `editor/play_main.cpp` render in three passes per frame -- no material -> the original
`ForwardLitPipeline` (M1's debug normal-color visualization, completely unaffected --
**superseded by the lighting phase A follow-up further down**, which changed the
no-material case to a flat purple indicator instead, still without touching
`ForwardLitPipeline` itself);
`"ForwardLitColor"` -> `ForwardLitColorPipeline`; `"ForwardLitTexturedColor"` ->
`ForwardLitTexturedColorPipeline`, which needed real `RHITexture`/descriptor-set plumbing
added to both Editor executables (a GUID -> cooked-`.tex` resolver + a fixed-size
descriptor pool + a GPU texture cache, the same shape as the mesh/material caches already
there) -- previously only `samples/m7_textures/main.cpp` had any of this. The Inspector's
"Material" section is now genuinely dynamic: for the material's own shader, it iterates
that shader's declared properties and renders the matching widget (`ColorEdit4` for
Color, `DragFloat` for Float, a `Combo` populated from every `*.png` under `assets/` for
Texture), writing the edit into the live `MaterialData` immediately (so the Scene View
reflects it the next frame) and persisting to the actual `.material.json` file via
`WriteMaterial()` on `IsItemDeactivatedAfterEdit()` -- a material is a separate asset
file, not part of the scene document, so this *is* its save point, not wired into
`UndoStack` (an accepted gap, same shape as "Save/Play aren't undoable"). Fixed a real,
adjacent placeholder while wiring the texture demo: mesh GUID resolution (`resolveMesh`)
used to be hardcoded to always load `m1_cube.mesh` and just check the requested GUID
matched -- any other mesh silently failed to resolve. Now built from a real GUID ->
cooked-path index (cooked `.mesh` files already embed their own GUID, `CookedMesh.h`'s own
format, no `.meta` scan needed the way materials/textures need). `editor/assets/
demo.scene.json` carries two live examples now: the hierarchy child cube still references
`assets/m_demo_red.material.json` (`"ForwardLitColor"`, unchanged from the first pass),
and a new final entity references `assets/m7_quad.gltf`'s mesh (not the shared
`m1_cube.glb` -- deliberately: that mesh predates `TEXCOORD_0` and every vertex would
sample a texture at UV (0,0), showing one flat color instead of an actual pattern) +
`assets/m_demo_checker_tint.material.json` (`"ForwardLitTexturedColor"`, M7's own checker
texture tinted light blue) -- verified on Pi4: both render correctly side by side in the
Scene View and Play Mode (where the red cube is also visibly orbiting its RotateScript'd
parent, proving materials, hierarchy, scripting, and textures all compose together), at
59-60 FPS capped and 178-211 FPS uncapped. Remaining, deliberate gaps: no Inspector UI to
*assign/unassign* a material to an entity (has to be hand-edited into the scene JSON,
same limitation data-driven script attachment had before it), and `Float` properties are
supported by the type system with no shader actually declaring one yet (not faked just to
demonstrate it -- added when a real shader needs one, same "add when needed" precedent as
everywhere else in this codebase).

**Testing note, not a bug**: while verifying material property editing over VNC, a
material file was found rewritten with default (white tint) values despite no `wlrctl`
call being issued in that round -- traced to a stale/leftover pointer-click event
(likely queued by the compositor or a VNC client from an earlier interaction in the same
long-running session) landing on an Inspector color widget in a freshly-launched process,
not a code defect: reproduced zero times locally with no input, and zero times on Pi4
across two more clean launches with careful, deliberate single clicks. Confirms the
already-documented "materials aren't wired into Undo" gap is a real, if rare, risk for a
stray click during remote testing -- worth knowing about, not a reason to add complexity
that wouldn't help a real user typing at a real keyboard.

**Testing note**: verifying tab clicks (Console/Assets/Project Hub, all docked into the
same tab bar) over VNC with `wlrctl` occasionally needed two clicks instead of one -- the
first sometimes only registered as hover (the tab highlights but the visible content and
the *other* tab's solid "active" styling don't actually change), confirmed by moving the
mouse away afterward and finding the old tab still showing as active. This is the same
general class of "synthetic click can be too fast for a single poll to see as a full
press-then-release" issue already found and fixed in `SDL2DisplayBackend.cpp` for the
Editor's *own* mouse handling -- but tab-switching itself is entirely Dear ImGui's own
vendored internal logic, not code this project owns, so there's nothing to patch here;
a real second click (or a real user's typically-slower click) works every time.

**Also post-E8**: object creation + generic Add/Remove Component (the first item picked up
from a broader, separately-tracked "make everything the Editor shows manageable" ask --
entity creation, component add/remove, and per-component editing were all things the
Editor could *display* but not *change the shape of*). Hierarchy panel gained "Create
Empty" (`world.CreateEntity()` + `AddTransform()`) and "Create Cube" (the same, plus a
`MeshComponent` pointed at whichever cooked mesh is named `m1_cube.mesh`, resolved by
filename through the same GUID index `resolveMesh` already builds, not a hardcoded GUID
string) buttons, and a right-click "Delete" context menu per row (`ImGui::
BeginPopupContextItem()`) -- deletion is deferred to after the whole tree finishes
rendering that frame (`world.DestroyEntity()` mid-iteration would rearrange the very
`ComponentStorage` the tree walk is reading) and orphans any children (reparents them to
root) rather than cascade-deleting them, since `World::DestroyEntity()` itself doesn't
touch other entities' `TransformComponent::parent` (a documented gap, not new). The
Inspector gained matching "Remove Mesh"/"Remove Collider"/"Remove Rigidbody" buttons on
each existing section, and an "Add Component:" row at the bottom offering only the
component types the selected entity doesn't already have (`+ Mesh` seeds a default cube
mesh the same way "Create Cube" does; `+ Collider`/`+ Rigidbody` add with the component's
own compiled-in defaults). Material isn't in that list -- it's a property of
`MeshComponent` (`materialGuid`), not its own ECS component -- so a Mesh with no material
yet gets an "Assign Material" combo instead (populated from every `*.material.json` under
`assets/`, same picker shape as a Texture property's own asset combo), and one that has a
material gets a "Remove Material" button alongside its existing dynamic property editing.
None of this is wired into `UndoStack` -- same accepted-gap reasoning as materials'
own gesture-end persistence and "Save/Play aren't undoable": creating/destroying an
*entity* is structurally different from editing a field on one that still exists (an
`Entity` handle's index can be reused after `DestroyEntity()`, so a naive undo closure
capturing the old handle could silently resurrect data onto a completely different,
later-created entity) and was out of scope for this pass. Verified on Pi4: Create Cube,
Assign/Remove Material, `+ Collider`/`+ Rigidbody`, Remove Mesh/Collider/Rigidbody, and
Delete (via the context menu) each confirmed working correctly via before/after
screenshots showing the expected Inspector/Hierarchy state change (entity/mesh counts,
GUIDs, component sections appearing/disappearing) -- see this section's own testing note
for a data-safety wrinkle hit along the way, not a code defect.

**Testing note, not a bug**: mid-verification, `wlrctl pointer click right` on a Hierarchy
row correctly opened the "Delete" context menu, but the *same test sequence* also showed
an extra entity that hadn't been intentionally created and a different, unrelated entity's
Rotation/Scale fields holding drifted values neither this feature's code nor the user's
actions had set -- the same "stale/leftover synthetic input event firing at an unpredictable
later moment" class of issue already documented for material property editing above, not a
regression in the (pre-existing, unmodified-by-this-feature) Transform DragFloat3 widgets.
Confirmed harmless: nothing had been saved to disk (no `Save` click occurred), and
`editor/assets/demo.scene.json` on the Pi4 device was byte-identical to the committed copy
after killing the process without saving. Not investigated further -- reproducing input
races precisely enough to fix a test-harness quirk (not application code) wasn't worth the
time against the actual feature already being solidly verified through the rest of the
sequence.

**Also post-E8**: Collider shape switching + Rigidbody `isStatic` storage (phase 3 of the
same "make everything the Editor shows manageable" plan phase 1's object creation/Add-
Remove-Component work started). The Collider section's "Shape: Box/Sphere" static text
became a real `Box`/`Sphere` combo, pushed directly to `undoStack` on selection (same
`reparentWithUndo()`-style reasoning as the Transform "Parent" combo -- an instant, discrete
choice has no drag gesture to batch through `TrackFieldEdit()`) -- switching shows/hides
"Half extents" vs. "Radius" immediately, same live-edit-no-apply-step behavior every other
Inspector field already has. `engine::ecs::RigidbodyComponent` gained a real `isStatic`
field (`RigidbodyComponent.h`'s own comment) -- previously read once at
`PhysicsWorld::CreateBody()` time and discarded, so `ExtractEntityDescs()` had to silently
drop a whole `"rigidbody"` block on Save rather than guess (a gap this project's own docs
had named explicitly, more than once). Now it round-trips fully: `SpawnEntities()` stores
it on the live component, `ExtractEntityDescs()` reads it back, and the Rigidbody section
gained an "Is Static" checkbox (`TrackFieldEdit()`-tracked, same as Collider's own "Is
trigger"). `editor/assets/demo.scene.json`'s collider-box entity gained a `"rigidbody":
{"isStatic": false, "mass": 1.0}` block as a live example -- `isStatic: false` deliberately
not `RigidbodyComponent`'s own default (`true`), so watching it actually fall/topple under
gravity in Play Mode is real proof the value made it all the way from JSON into a live
Jolt body, not just that the field exists. Verified on Pi4: Shape combo switch (Box ->
Sphere, `Half extents` swapping for `Radius`) and its Undo both confirmed via screenshot;
"Is Static" checkbox toggle confirmed pushing an Undo entry (button state
enabled/disabled around the click); the demo scene's dynamic Rigidbody entity visibly
tips over and settles under gravity in Play Mode, at 59-60 FPS capped and ~202 FPS
uncapped. Neither of these two field edits needed the "structural operation" Undo
exclusion Add/Remove Component and Create/Delete Entity have -- a shape/isStatic value
change doesn't touch an `Entity` handle's lifetime the way creating or destroying one
does, so both went straight into `undoStack` like every other Inspector field.

**Also post-E8**: lighting phase A (docs/01 section 8.3's "Low-Poly Retro" profile --
vertex/Blinn-Phong lighting, an indicative budget of 2-4 simultaneous lights; deliberately
not the "PBR profile" `CLAUDE.md` keeps out of scope). `engine::ecs::LightComponent`
(Directional or Point, color/intensity/range, plus `isStatic`/`castsShadow` hints reserved
for a not-yet-built static shadow map) is a component like any other -- attach one to any
entity with a Transform. Lighting itself is just another shader choice in the existing
material system (`ShaderPropertySchema.h`'s `"ForwardLitShaded"` entry, backed by a new
`ForwardLitShadedPipeline` -- a fifth separate concrete pipeline, `CLAUDE.md` rule 7 still
honored): assign a material targeting that shader to a Mesh and it renders lit (ambient +
N·L diffuse + a fixed-shininess specular highlight) instead of through any of the other
four pipelines. Needed this project's first per-frame (not per-draw, not write-once)
Uniform Buffer -- `RHIBuffer` gained an `UpdateData()` path (previously every buffer was
written once at load time), and both Editor executables now keep one buffer + descriptor
set *per frame-in-flight* (never a single shared instance, or writing this frame's light
data could race a still-in-flight GPU read from the previous frame) collected fresh every
frame from every `LightComponent` in the scene (up to `kMaxLights`=4) plus the camera's
own view-projection matrix and world position. The Inspector gained a "Light" section
(Type combo -- Directional/Point, pushed to `undoStack` directly like Collider's own Shape
combo -- Color/Intensity/Range/Is Static/Casts Shadow, all `TrackFieldEdit`-tracked) and a
`+ Light` entry in Add Component. `editor/assets/demo.scene.json` gained two light
entities (a warm Directional "sun" and an orange Point light) plus
`assets/m_demo_lit_white.material.json`, a near-white `ForwardLitShaded` material whose
visible shading comes entirely from those two lights, not its own tint -- verified on Pi4:
clearly visible per-face shading gradient and a specular highlight distinguishing it from
every other (flat-tinted or debug-normal-colored) entity in the scene, in both the
Editor's Scene View and Play Mode, at 59-60 FPS capped and ~192 FPS uncapped, with no
visible flicker/corruption at either rate (the double-buffered UBO's own correctness
signal -- a synchronization bug here would likely show up as exactly that). Deliberately
not in this phase: shadows of any kind (a static-only shadow map -- rendered once for
static-flagged lights/geometry, not every frame, per docs/01's own "preferably baked"
shadow guidance -- is phase B, not started) and non-uniform-scale-correct normal
transforms (a documented simplification, `ForwardLitShadedPipeline.h`'s own comment).

**Lighting phase A follow-up, the user's own explicit request**: two changes on top of the
material system, both about what an entity looks like *by default*. First, an entity with
no material assigned no longer renders through `ForwardLitPipeline`'s debug normal-color
visualization in either Editor executable -- it now renders a flat, unmistakable
purple/violet (`kMissingMaterialColor`) through `ForwardLitColorPipeline` instead, this
project's "missing material" indicator (same reasoning as Unity/Source's own magenta/pink
missing-shader colors). `ForwardLitPipeline` itself is untouched -- still M1's own exit
criterion, still used directly by every M0-M7 sample with no material system involved at
all; only the Editor's own dispatch choice changed. Second, the engine's default/base lit
material is now `"ForwardVertexLit"`/`"ForwardVertexLitTextured"` (`ShaderPropertySchema.h`)
-- the sixth and seventh concrete pipeline classes, `ForwardVertexLitPipeline`/
`ForwardVertexLitTexturedPipeline`: the identical lighting formula and `FrameLightingData`
UBO `ForwardLitShadedPipeline` already reads, but evaluated once per *vertex* (Gouraud
shading) instead of once per fragment -- cheaper on Pi4's fill-heavy TBDR GPU, matching
docs/01 section 8.3's own "vertex lighting or minimal Blinn-Phong" wording (the fragment
half already existed; this is the vertex half). The textured variant needed `RHIPipeline`
to support a *second*, independent descriptor set (set = 1, alongside the per-frame
lighting UBO's set = 0) -- the first pipeline in this codebase needing two independently-
bound resources with different lifetimes; both new sets' binding shapes are identical to
already-existing ones (`ForwardLitShadedPipeline`'s set 0, `ForwardLitTexturedColorPipeline`'s
texture set), so both Editor executables reuse their *existing* frame-lighting descriptor
sets and material-texture cache unchanged (Vulkan spec 14.2.2, "identically defined"
descriptor set layouts) -- no new pool/cache/allocation needed. When a material has no
texture, it targets `"ForwardVertexLit"` instead of leaving `albedoTexture` empty on
`"ForwardVertexLitTextured"` -- same "switch shaderName, don't toggle a flag" convention
every texture/no-texture shader pair in this registry follows. `editor/assets/
demo.scene.json`: the ground plane, the RotateScript cube, and the falling physics box are
now *deliberately* left without a material (proving the purple indicator); the second
plain cube references a new `assets/m_demo_vertex_lit.material.json` (green tint); a new
final entity (`assets/m7_quad.gltf`'s mesh, same reason `m_demo_checker_tint` uses it)
references a new `assets/m_demo_vertex_lit_textured.material.json` (the same checker
texture, warm tint) -- all seven concrete pipelines now visible side by side in one scene.

Mesh resolution (`resolveMesh`) is a real GUID → cooked-path index now (fixed alongside
material assets' Texture property type, see that section above) — every `*.mesh` file
under `assets_cooked/` is resolvable, not just `m1_cube.mesh`. Still not a real
general-purpose Asset Manifest (`engine/asset/README.md`) — this Editor's own local index,
rebuilt fresh every launch by scanning the filesystem, the same shape material/texture
resolution already use.

See the roadmap doc for what's explicitly deferred to later steps, and
`docs/07-unity-parity-analysis.md` for what's missing relative to Unity's own editor.
