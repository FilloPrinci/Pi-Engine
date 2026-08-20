# debug

- `ImGuiOverlay.h` + `.cpp` -- done (Editor step E1, `docs/06-editor-roadmap.md`): a
  reusable Dear ImGui <-> RHI integration, resolving the `TODO(M2)` that sat in
  `engine/CMakeLists.txt` since M2 unused. Wraps `imgui_impl_vulkan`/`imgui_impl_sdl2`,
  both vendored in `third_party/imgui_backends/` (see that directory's own README for
  why they're vendored instead of pulled from vcpkg's `imgui` port features). Not
  Editor-specific -- any consumer wanting an immediate-mode overlay can use this, whether
  that's an Editor panel or an in-sample debug overlay.
- `Console.h` + `.cpp` -- done (Editor step E5, fixed during E7): captures the engine's
  existing stdout/stderr output into an in-memory buffer an ImGui panel can render, via
  low-level file descriptor redirection (`dup2` onto a pipe) rather than a new logging API
  every `std::printf`/`std::fprintf(stderr, ...)` call site would need to be rewritten to
  use. Output is teed back to the original fds, so redirecting/inheriting the process's own
  stdout/stderr keeps working unchanged. POSIX only (`dup`/`dup2`/`pipe`/`fcntl`) --
  `Init()` returns `false` on any other platform and `Update()` is a no-op there, matching
  this project's Linux-primary focus without hard-failing a Windows build. `Shutdown()`
  does one final flush+drain before restoring the real fds -- found missing while testing
  Editor step E7: any early `return EXIT_FAILURE` in `main()` before the frame loop starts
  (so `Update()` never ran) was silently swallowing whatever had been printed, exactly the
  diagnostic output an early-failure path most needs to actually reach the user.

`ImGuiIO::WantCaptureMouse` is consulted as of post-Editor-E8's viewport picking + gizmo
(`editor/main.cpp`) -- gates *starting* a new pick/gizmo-drag so clicking an ImGui panel
never also acts on the 3D view behind it, but doesn't gate an already-started drag's
continuation/release (stranding it "stuck" if the mouse strays over a panel mid-drag would
be worse). `WantCaptureKeyboard` is consulted too, since the Undo/Redo follow-up
(`editor/UndoStack.h`) added Ctrl+Z/Ctrl+Y shortcuts that need to defer to ImGui's own
in-widget text-edit shortcuts (e.g. undoing a keystroke inside a `DragFloat`'s text-entry
box) rather than also firing the Editor's own undo stack at the same time.

`ImGuiConfigFlags_DockingEnable` is *not* set here -- docking (post-Editor-E8, the
Editor's panel layout) is an Editor-only opt-in, enabled directly in `editor/main.cpp`
after `ImGuiOverlay::Init()` returns, since this class stays generic for non-Editor
consumers (`samples/e1_imgui_overlay`) that have no reason to opt into it. Needs vcpkg's
imgui `docking-experimental` feature (`vcpkg.json`) -- the plain `imgui` feature set
doesn't build `DockBuilder`/`DockSpace` support at all.
