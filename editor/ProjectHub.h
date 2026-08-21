#pragma once

#include <string>
#include <vector>

// Editor step E7 -- Project Hub (docs/06-editor-roadmap.md), extended post-E8 with real
// Open/New actions (the user's own explicit request: manage "the project" -- open one
// from a file, create a new one from scratch). Deliberately minimal: this repo doesn't
// have a real "install the engine once, multiple separate projects reference it"
// packaging story yet (no export/install target exists for Pi-Engine at all), so a
// literal port of docs/01 section 6.1's Project Hub description isn't buildable today.
// What *is* buildable and matches Unity Hub's actual day-to-day behavior: a small
// persisted "recently opened scenes" list with a way to jump straight back into one, plus
// opening an arbitrary scene file by path and creating a brand-new empty one -- "project"
// means "scene file" throughout this header, not a separate manifest/directory concept.
//
// No namespace -- matches tools/cooker/AssetImporter.h's precedent for a small,
// tool-local helper header (this is editor-local code, not part of engine_core).

struct RecentProject {
    std::string scenePath;
    std::string lastOpenedUtc; // Informational only (e.g. "2026-08-19 15:42 UTC"), never parsed back.
};

// Reads the persisted recent-projects list (most-recently-opened first), empty if the
// file doesn't exist yet or can't be read/parsed.
std::vector<RecentProject> LoadRecentProjects();

// Adds/moves `scenePath` to the front of the persisted list (deduplicated by exact path
// match, capped at 10 entries) and writes it back immediately -- call once per successful
// scene load, so the list is always current without a separate explicit "save" step.
void RecordRecentProject(const std::string& scenePath);

// Creates a brand-new, empty scene JSON document at `path` -- the Editor's "New Project"/
// "New Scene File" action, a direct extension of this same Project Hub rather than a
// separate system (see this header's own comment for why "project" means "scene file"
// here). Writes through the exact same engine::scene::SaveScene() path the Editor's own
// "Save" button already uses, just given a throwaway empty engine::ecs::World instead of
// the Editor's live one -- a freshly created scene round-trips through LoadScene()
// identically to any other, nothing scene-creation-specific about its JSON shape. Creates
// any missing parent directories. Refuses (returns false, logs to stderr) if `path`
// already exists -- never silently overwrites an existing file -- or if the parent
// directories/file itself can't be created. The caller is expected to follow a successful
// call with RelaunchWithProject(path), same as picking an existing entry from the
// recent-projects list.
bool CreateNewSceneFile(const std::string& path);

// Relaunches the current executable pointed at `scenePath` (fork + execv against
// /proc/self/exe) -- mirrors Unity Hub's real behavior of one Editor process per open
// project, rather than reloading a scene inside the same running process (which nothing
// else in this engine supports either -- no runtime C++ hot-reload, docs/01 section 6.1).
// Linux only; returns false immediately on any other platform or if the relaunch itself
// fails to start. The caller is expected to request its own process quit shortly after a
// successful call (see editor/main.cpp) -- the new process owns the window/GPU context
// from here on.
bool RelaunchWithProject(const std::string& scenePath);
