#pragma once

#include <string>

// Editor step E8 -- Build/Play/Debug pipeline (docs/06-editor-roadmap.md). The Editor-UI
// side of Play: build the `editor_play` target (play_main.cpp, a *separate* executable
// from `editor` -- see that file's own comment for why: physics/PhysicsWorld.h pulls in
// Jolt, and linking Jolt::Jolt directly onto `editor` itself would leak its AVX2/FMA
// INTERFACE_COMPILE_OPTIONS onto every source file of the `editor` target, not just the
// one that needs it -- [[vcpkg-dependency-isa-flag-isolation]]) and re-cook assets (both
// incremental -- CMake's own OUTPUT/DEPENDS tracking, same as every sample already relies
// on, see cmake/CookAssets.cmake), then launch `editor_play` in a fresh process. No
// namespace, matches ProjectHub.h's precedent for editor-local (not engine_core) code.

// Runs `cmake --build <buildDir> --target editor_play`, then `cooked_assets`/
// `cooked_shaders`/`cooked_textures` in that order, stopping at the first failure. Every
// child process is spawned via fork()+execvp() *without* redirecting its stdout/stderr --
// both fds are already redirected into engine::debug::Console's pipe at the OS level
// (Console::Init(), main()'s first statement), so a plain fork() inherits that redirection
// for free and the build/cook output shows up in the Console panel exactly like the
// engine's own log lines, no separate output-capture plumbing needed. Blocks the calling
// thread until done (a no-op incremental build/cook is usually well under a second; a full
// rebuild can take longer) -- a background thread would keep the Editor UI responsive
// during a cold build, but that's more than this first version needs, see
// docs/07-unity-parity-analysis.md. Linux only; returns false immediately on any other
// platform.
bool RunBuildAndCook(const std::string& buildDir);

// Launches `scenePath` in Play Mode -- fork()+execv() of the `editor_play` executable
// (resolved as the sibling of the currently running `editor` binary, i.e.
// dirname(/proc/self/exe) + "/editor_play") with `scenePath` as its one argument
// (non-debug), or fork()+execvp("gdb", ...) wrapping that same command in batch mode
// (debug -- prints a backtrace on crash, otherwise just runs to completion) when `debug`
// is true. Fire-and-forget: unlike ProjectHub's RelaunchWithProject(), the calling
// (Editor) process does NOT quit -- Play launches a second, independent process and the
// Editor keeps running, closer to how Unity's Editor stays open while Play Mode runs
// (docs/07-unity-parity-analysis.md has the fuller comparison). Reaps any already-finished
// previous Play process (non-blocking) before spawning a new one, so repeated Play clicks
// in one Editor session don't accumulate zombie processes. Linux only; returns false
// immediately on any other platform, or if fork() itself fails. Since this is
// fire-and-forget, a failed exec() inside the child (e.g. `gdb` not on PATH, or
// `editor_play` missing) can't be reported through this function's return value -- the
// child prints an error to stderr/Console before exiting instead (same limitation
// RelaunchWithProject() already has for its own execv() call).
bool LaunchPlayProcess(const std::string& scenePath, bool debug);
