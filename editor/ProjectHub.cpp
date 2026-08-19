#include "ProjectHub.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <ctime>
#include <filesystem>
#include <fstream>

#if defined(__linux__)
#define PI_EDITOR_HUB_LINUX 1
#include <unistd.h>
#else
#define PI_EDITOR_HUB_LINUX 0
#endif

namespace {

constexpr std::size_t kMaxRecentProjects = 10;

std::filesystem::path RecentProjectsFilePath() {
    const char* home = std::getenv("HOME");
    if (home == nullptr) {
        return {}; // No HOME -- caller treats an empty path as "persistence unavailable".
    }
    return std::filesystem::path(home) / ".local" / "share" / "pi-engine" / "recent_projects.json";
}

std::string CurrentTimeUtc() {
    const auto now = std::chrono::system_clock::now();
    const std::time_t nowTimeT = std::chrono::system_clock::to_time_t(now);
    std::tm utcTime{};
#if PI_EDITOR_HUB_LINUX
    gmtime_r(&nowTimeT, &utcTime);
#else
    const std::tm* result = std::gmtime(&nowTimeT);
    if (result != nullptr) {
        utcTime = *result;
    }
#endif
    char buffer[32];
    std::strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M UTC", &utcTime);
    return buffer;
}

} // namespace

std::vector<RecentProject> LoadRecentProjects() {
    std::vector<RecentProject> projects;
    const std::filesystem::path path = RecentProjectsFilePath();
    if (path.empty()) {
        return projects;
    }

    std::ifstream in(path);
    if (!in.is_open()) {
        return projects; // Not an error -- just no history yet.
    }

    try {
        nlohmann::json json;
        in >> json;
        if (!json.is_array()) {
            return projects;
        }
        for (const auto& entryJson : json) {
            if (!entryJson.contains("scenePath") || !entryJson["scenePath"].is_string()) {
                continue;
            }
            RecentProject project;
            project.scenePath = entryJson["scenePath"].get<std::string>();
            project.lastOpenedUtc = entryJson.value("lastOpenedUtc", std::string("unknown"));
            projects.push_back(std::move(project));
        }
    } catch (const nlohmann::json::exception& e) {
        std::fprintf(stderr, "LoadRecentProjects: \"%s\" is malformed: %s\n",
                     path.string().c_str(), e.what());
        projects.clear();
    }
    return projects;
}

void RecordRecentProject(const std::string& scenePath) {
    const std::filesystem::path path = RecentProjectsFilePath();
    if (path.empty()) {
        return; // No HOME -- silently skip persistence, same reasoning as LoadRecentProjects.
    }

    std::vector<RecentProject> projects = LoadRecentProjects();
    projects.erase(std::remove_if(projects.begin(), projects.end(),
                                  [&](const RecentProject& p) { return p.scenePath == scenePath; }),
                   projects.end());

    RecentProject entry;
    entry.scenePath = scenePath;
    entry.lastOpenedUtc = CurrentTimeUtc();
    projects.insert(projects.begin(), std::move(entry));
    if (projects.size() > kMaxRecentProjects) {
        projects.resize(kMaxRecentProjects);
    }

    std::error_code errorCode;
    std::filesystem::create_directories(path.parent_path(), errorCode);
    if (errorCode) {
        std::fprintf(stderr, "RecordRecentProject: failed to create \"%s\": %s\n",
                     path.parent_path().string().c_str(), errorCode.message().c_str());
        return;
    }

    nlohmann::json json = nlohmann::json::array();
    for (const RecentProject& project : projects) {
        nlohmann::json entryJson;
        entryJson["scenePath"] = project.scenePath;
        entryJson["lastOpenedUtc"] = project.lastOpenedUtc;
        json.push_back(entryJson);
    }

    std::ofstream out(path);
    if (!out.is_open()) {
        std::fprintf(stderr, "RecordRecentProject: failed to open \"%s\" for writing\n",
                     path.string().c_str());
        return;
    }
    out << json.dump(2);
}

#if PI_EDITOR_HUB_LINUX

bool RelaunchWithProject(const std::string& scenePath) {
    char selfPathBuffer[4096];
    const ssize_t length = readlink("/proc/self/exe", selfPathBuffer, sizeof(selfPathBuffer) - 1);
    if (length <= 0) {
        std::fprintf(stderr, "RelaunchWithProject: readlink(\"/proc/self/exe\") failed\n");
        return false;
    }
    selfPathBuffer[length] = '\0';

    const pid_t child = fork();
    if (child < 0) {
        std::fprintf(stderr, "RelaunchWithProject: fork() failed\n");
        return false;
    }
    if (child == 0) {
        // Child: exec() fully replaces this process image (including its own fresh
        // stdout/stderr, Vulkan/Wayland connections, etc.) -- it does not inherit the
        // parent's in-memory state, only its file descriptors. The parent is expected to
        // quit and close its own window/GPU context shortly after this call returns,
        // which bounds how long both processes could theoretically hold duplicate
        // references to the same underlying Wayland/DRM fds.
        char* args[] = {selfPathBuffer, const_cast<char*>(scenePath.c_str()), nullptr};
        execv(selfPathBuffer, args);
        // Only reached if execv() itself failed.
        std::fprintf(stderr, "RelaunchWithProject: execv(\"%s\") failed\n", selfPathBuffer);
        _exit(127);
    }
    return true; // Parent: child was spawned successfully (its own exec may still fail).
}

#else

bool RelaunchWithProject(const std::string& /*scenePath*/) {
    return false;
}

#endif
