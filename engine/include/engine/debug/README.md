# debug

- `ImGuiOverlay.h` + `.cpp` -- done (Editor step E1, `docs/06-editor-roadmap.md`): a
  reusable Dear ImGui <-> RHI integration, resolving the `TODO(M2)` that sat in
  `engine/CMakeLists.txt` since M2 unused. Wraps `imgui_impl_vulkan`/`imgui_impl_sdl2`,
  both vendored in `third_party/imgui_backends/` (see that directory's own README for
  why they're vendored instead of pulled from vcpkg's `imgui` port features). Not
  Editor-specific -- any consumer wanting an immediate-mode overlay can use this, whether
  that's an Editor panel or an in-sample debug overlay.

No input-capture handling yet (`ImGuiIO::WantCaptureMouse`/`WantCaptureKeyboard` aren't
consulted anywhere) -- fine while nothing but non-interactive demo content uses this
overlay; needed once a Scene View's camera navigation has to coexist with clickable
ImGui widgets (Editor step E3+).
