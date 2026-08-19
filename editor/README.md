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

**Step E4 (current)**: Scene saving. A **Save** button (in the info window) calls
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

Mesh resolution is a placeholder GUID → GPU-buffers cache (same pattern as
`samples/m7_scene_and_prefab`) that only knows about `m1_cube.mesh` — a real GUID →
cooked-path manifest is Asset Browser territory (Editor step E6), not built yet.

See the roadmap doc for what's explicitly deferred to later steps (editing, saving,
Console panel, Asset Browser, Project Hub, the Build/Play/Debug pipeline).
