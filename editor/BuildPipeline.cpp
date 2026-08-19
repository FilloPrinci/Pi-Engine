#include "BuildPipeline.h"

#include <cstdio>
#include <filesystem>
#include <vector>

#if defined(__linux__)
#define PI_EDITOR_BUILD_LINUX 1
#include <sys/wait.h>
#include <unistd.h>
#else
#define PI_EDITOR_BUILD_LINUX 0
#endif

#if PI_EDITOR_BUILD_LINUX

namespace {

// fork() + execvp(args[0], args) + waitpid() -- true only if the child both started and
// exited with status 0. No stdout/stderr redirection here on purpose (see BuildPipeline.h).
bool RunSubprocess(const std::vector<std::string>& args) {
    std::vector<char*> argv;
    argv.reserve(args.size() + 1);
    for (const std::string& arg : args) {
        argv.push_back(const_cast<char*>(arg.c_str()));
    }
    argv.push_back(nullptr);

    const pid_t child = fork();
    if (child < 0) {
        std::fprintf(stderr, "editor: fork() failed while running \"%s\"\n", args[0].c_str());
        return false;
    }
    if (child == 0) {
        execvp(argv[0], argv.data());
        std::fprintf(stderr, "editor: execvp(\"%s\") failed\n", args[0].c_str());
        _exit(127);
    }

    int status = 0;
    if (waitpid(child, &status, 0) < 0) {
        std::fprintf(stderr, "editor: waitpid() failed while running \"%s\"\n", args[0].c_str());
        return false;
    }
    return WIFEXITED(status) && WEXITSTATUS(status) == 0;
}

// readlink("/proc/self/exe") -- same approach as ProjectHub.cpp's RelaunchWithProject(),
// duplicated rather than shared: it's six lines, and BuildPipeline.cpp/ProjectHub.cpp are
// meant to stay independent enough that neither has to change for the other (matches this
// codebase's general tolerance for this kind of small duplication across samples/tools
// rather than introducing a shared helper for a six-line block used in two places).
bool ResolveSelfExecutablePath(std::string& outPath) {
    char buffer[4096];
    const ssize_t length = readlink("/proc/self/exe", buffer, sizeof(buffer) - 1);
    if (length <= 0) {
        std::fprintf(stderr, "editor: readlink(\"/proc/self/exe\") failed\n");
        return false;
    }
    buffer[length] = '\0';
    outPath = buffer;
    return true;
}

// Most recently spawned Play process, if any -- reaped (non-blocking) the next time Play
// is clicked, see BuildPipeline.h's own comment on why this exists.
pid_t g_lastPlayChild = -1;

void ReapFinishedPlayProcesses() {
    if (g_lastPlayChild <= 0) {
        return;
    }
    int status = 0;
    if (waitpid(g_lastPlayChild, &status, WNOHANG) > 0) {
        g_lastPlayChild = -1;
    }
}

} // namespace

bool RunBuildAndCook(const std::string& buildDir) {
    std::printf("editor: Play -- building 'editor_play'...\n");
    if (!RunSubprocess({"cmake", "--build", buildDir, "--target", "editor_play"})) {
        std::fprintf(stderr, "editor: Play -- build failed\n");
        return false;
    }

    const char* cookTargets[] = {"cooked_assets", "cooked_shaders", "cooked_textures"};
    for (const char* target : cookTargets) {
        std::printf("editor: Play -- cooking (%s)...\n", target);
        if (!RunSubprocess({"cmake", "--build", buildDir, "--target", target})) {
            std::fprintf(stderr, "editor: Play -- cooking (%s) failed\n", target);
            return false;
        }
    }

    std::printf("editor: Play -- build + cook succeeded.\n");
    return true;
}

bool LaunchPlayProcess(const std::string& scenePath, bool debug) {
    std::string selfPath;
    if (!ResolveSelfExecutablePath(selfPath)) {
        return false;
    }
    // editor_play (play_main.cpp) is a *sibling* executable, not this same binary with a
    // flag -- see BuildPipeline.h's own comment for why (the ISA-flag-leak trap). CMake
    // places both targets' outputs in the same per-config directory, so the sibling's path
    // is just this executable's own directory with a different filename.
    const std::string playExePath =
        (std::filesystem::path(selfPath).parent_path() / "editor_play").string();

    ReapFinishedPlayProcesses();

    const pid_t child = fork();
    if (child < 0) {
        std::fprintf(stderr, "editor: fork() failed while launching Play\n");
        return false;
    }
    if (child == 0) {
        // Child: exec() fully replaces this process image, same reasoning as
        // ProjectHub.cpp's RelaunchWithProject() -- the difference here is the parent
        // (Editor) keeps running afterwards instead of quitting, see BuildPipeline.h.
        if (debug) {
            execlp("gdb", "gdb", "-q", "-batch", "-ex", "run", "-ex", "bt", "--args",
                   playExePath.c_str(), scenePath.c_str(), static_cast<char*>(nullptr));
            std::fprintf(stderr, "editor: execlp(\"gdb\") failed -- is gdb installed?\n");
        } else {
            execl(playExePath.c_str(), playExePath.c_str(), scenePath.c_str(),
                  static_cast<char*>(nullptr));
            std::fprintf(stderr, "editor: execl(\"%s\") failed\n", playExePath.c_str());
        }
        _exit(127);
    }

    g_lastPlayChild = child;
    return true;
}

#else // !PI_EDITOR_BUILD_LINUX

bool RunBuildAndCook(const std::string&) {
    return false;
}

bool LaunchPlayProcess(const std::string&, bool) {
    return false;
}

#endif
