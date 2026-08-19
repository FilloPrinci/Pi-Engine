# debug

- `ImGuiOverlay.h` + `.cpp` -- done (Editor step E1, `docs/06-editor-roadmap.md`): a
  reusable Dear ImGui <-> RHI integration, resolving the `TODO(M2)` that sat in
  `engine/CMakeLists.txt` since M2 unused. Wraps `imgui_impl_vulkan`/`imgui_impl_sdl2`,
  both vendored in `third_party/imgui_backends/` (see that directory's own README for
  why they're vendored instead of pulled from vcpkg's `imgui` port features). Not
  Editor-specific -- any consumer wanting an immediate-mode overlay can use this, whether
  that's an Editor panel or an in-sample debug overlay.
- `Console.h` + `.cpp` -- done (Editor step E5): captures the engine's existing
  stdout/stderr output into an in-memory buffer an ImGui panel can render, via low-level
  file descriptor redirection (`dup2` onto a pipe) rather than a new logging API every
  `std::printf`/`std::fprintf(stderr, ...)` call site would need to be rewritten to use.
  Output is teed back to the original fds, so redirecting/inheriting the process's own
  stdout/stderr keeps working unchanged. POSIX only (`dup`/`dup2`/`pipe`/`fcntl`) --
  `Init()` returns `false` on any other platform and `Update()` is a no-op there, matching
  this project's Linux-primary focus without hard-failing a Windows build.

No mouse input-capture handling yet (`ImGuiIO::WantCaptureMouse`/`WantCaptureKeyboard`
aren't consulted anywhere) -- not an issue in practice through Editor step E5 since the
Scene View's camera is keyboard-only (A/D/W/S/Up/Down, see `editor/README.md`), so there's
no actual mouse conflict with ImGui panels yet; will matter once a free-look mouse camera
is added.
