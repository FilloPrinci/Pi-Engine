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

Mesh resolution is a placeholder GUID → GPU-buffers cache (same pattern as
`samples/m7_scene_and_prefab`) that only knows about `m1_cube.mesh` — a real GUID →
cooked-path manifest is Asset Browser territory (Editor step E6), not built yet.

See the roadmap doc for what's explicitly deferred to later steps, and
`docs/07-unity-parity-analysis.md` for what's missing relative to Unity's own editor.
