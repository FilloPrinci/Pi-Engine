# editor

Pi-Engine Editor (`docs/01-engine-design-rpi.md` section 6, staged into incremental steps
in [`docs/06-editor-roadmap.md`](../docs/06-editor-roadmap.md)) — "a separate application,
client of the Engine Core (same RHI/Vulkan as the final game)". Links `engine_core` fully,
unlike `tools/cooker` (deliberately not linked, offline-only).

**Step E2 (current)**: app skeleton + Scene View. `editor [path/to/scene.json]` loads a
scene (defaults to `assets/demo.scene.json` if no path is given) via
`engine::scene::LoadScene` — read-only, no `PhysicsWorld`/physics callback yet — and
renders it through the same `ForwardLitPipeline` every sample uses. Camera is
keyboard-navigable (A/D yaw, W/S pitch, Up/Down zoom — no mouse-look input plumbing exists
yet). `engine::debug::ImGuiOverlay` (Editor step E1) is already wired in with a small info
window; the Inspector panel (Editor step E3) builds its real UI on top of the same
overlay.

Mesh resolution is a placeholder GUID → GPU-buffers cache (same pattern as
`samples/m7_scene_and_prefab`) that only knows about `m1_cube.mesh` — a real GUID →
cooked-path manifest is Asset Browser territory (Editor step E6), not built yet.

See the roadmap doc for what's explicitly deferred to later steps (editing, saving,
Console panel, Asset Browser, Project Hub, the Build/Play/Debug pipeline).
