# e1_imgui_overlay

Editor step E1 -- ImGui-Vulkan integration (`docs/06-editor-roadmap.md`). Resolves the
`TODO(M2)` that sat unused in `engine/CMakeLists.txt` since M2: `engine::debug::ImGuiOverlay`
now wraps `imgui_impl_vulkan`/`imgui_impl_sdl2` (both vendored, see
`third_party/README.md`) and draws into the same render pass/subpass as the sample's own
geometry.

Reuses M1's cube mesh and unlit pipeline verbatim -- the only new thing here is the
overlay: a small custom window (mesh stats, FPS) plus ImGui's own demo window, exercising
enough real widgets/plots to prove the whole `Init`/`NewFrame`/`Render`/`Shutdown`
lifecycle actually works, not just that it compiles.

Not the Editor app itself (that starts at step E2, `editor/` at the repo root) -- this is
the foundation step E2+ builds its panels on top of.
