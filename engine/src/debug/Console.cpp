#include "engine/debug/Console.h"

#include <cstdio>

#if defined(__linux__) || defined(__APPLE__)
#define PI_ENGINE_CONSOLE_POSIX 1
#include <fcntl.h>
#include <unistd.h>
#else
#define PI_ENGINE_CONSOLE_POSIX 0
#endif

namespace engine::debug {

#if PI_ENGINE_CONSOLE_POSIX

namespace {

bool RedirectToPipe(int streamFd, int& outPipeReadFd, int& outOriginalFd) {
    int pipeFds[2];
    if (pipe(pipeFds) != 0) {
        return false;
    }
    outOriginalFd = dup(streamFd);
    if (outOriginalFd < 0) {
        close(pipeFds[0]);
        close(pipeFds[1]);
        return false;
    }
    if (dup2(pipeFds[1], streamFd) < 0) {
        close(pipeFds[0]);
        close(pipeFds[1]);
        close(outOriginalFd);
        outOriginalFd = -1;
        return false;
    }
    close(pipeFds[1]); // streamFd now refers to the same underlying pipe write end.
    fcntl(pipeFds[0], F_SETFL, O_NONBLOCK);
    outPipeReadFd = pipeFds[0];
    return true;
}

} // namespace

Console::~Console() {
    Shutdown();
}

bool Console::Init() {
    if (!RedirectToPipe(STDOUT_FILENO, m_stdoutPipeRead, m_stdoutOriginalFd)) {
        return false;
    }
    if (!RedirectToPipe(STDERR_FILENO, m_stderrPipeRead, m_stderrOriginalFd)) {
        Shutdown();
        return false;
    }
    // stdio defaults to fully-buffered (not line-buffered) once stdout/stderr are no
    // longer connected to a tty, which they aren't anymore after the dup2 above -- without
    // this, output would only reach the pipe (and so the Console panel) in ~4KB bursts
    // instead of promptly per line. Must happen before any other I/O on the stream (the
    // class comment says call Init() first thing in main() for exactly this reason).
    std::setvbuf(stdout, nullptr, _IOLBF, BUFSIZ);
    std::setvbuf(stderr, nullptr, _IOLBF, BUFSIZ);
    m_initialized = true;
    return true;
}

void Console::Shutdown() {
    // Final drain before restoring the real fds -- Update() may never have run (e.g. main()
    // hit an early `return EXIT_FAILURE` before the frame loop, and its onUpdate callback,
    // ever started), in which case whatever was printed is still sitting undrained in the
    // pipe. Without this, that output would be silently lost instead of teed to the
    // original terminal/log file -- exactly the diagnostic output an early-failure path
    // most needs to actually reach the user. Explicit fflush() first: even line-buffered
    // stdio can still be holding a partial (no trailing newline yet) line that was never
    // actually write()'d to the pipe.
    if (m_initialized) {
        std::fflush(stdout);
        std::fflush(stderr);
        DrainStream(m_stdoutPipeRead, m_stdoutOriginalFd, m_stdoutPartialLine, false);
        DrainStream(m_stderrPipeRead, m_stderrOriginalFd, m_stderrPartialLine, true);
    }

    if (m_stdoutOriginalFd >= 0) {
        dup2(m_stdoutOriginalFd, STDOUT_FILENO);
        close(m_stdoutOriginalFd);
        m_stdoutOriginalFd = -1;
    }
    if (m_stderrOriginalFd >= 0) {
        dup2(m_stderrOriginalFd, STDERR_FILENO);
        close(m_stderrOriginalFd);
        m_stderrOriginalFd = -1;
    }
    if (m_stdoutPipeRead >= 0) {
        close(m_stdoutPipeRead);
        m_stdoutPipeRead = -1;
    }
    if (m_stderrPipeRead >= 0) {
        close(m_stderrPipeRead);
        m_stderrPipeRead = -1;
    }
    m_initialized = false;
}

void Console::DrainStream(int pipeReadFd, int originalFd, std::string& partialLine, bool isError) {
    if (pipeReadFd < 0) {
        return;
    }
    char buffer[4096];
    for (;;) {
        const ssize_t bytesRead = read(pipeReadFd, buffer, sizeof(buffer));
        if (bytesRead <= 0) {
            break; // EAGAIN/EWOULDBLOCK (nothing available right now), or an error/EOF.
        }
        const auto count = static_cast<std::size_t>(bytesRead);
        write(originalFd, buffer, count); // Tee back to the real fd -- see class comment.
        partialLine.append(buffer, count);

        std::size_t newlinePos = 0;
        while ((newlinePos = partialLine.find('\n')) != std::string::npos) {
            m_lines.push_back(Line{partialLine.substr(0, newlinePos), isError});
            partialLine.erase(0, newlinePos + 1);
            while (m_lines.size() > kMaxLines) {
                m_lines.pop_front();
            }
        }
    }
}

void Console::Update() {
    if (!m_initialized) {
        return;
    }
    DrainStream(m_stdoutPipeRead, m_stdoutOriginalFd, m_stdoutPartialLine, false);
    DrainStream(m_stderrPipeRead, m_stderrOriginalFd, m_stderrPartialLine, true);
}

#else // !PI_ENGINE_CONSOLE_POSIX

Console::~Console() = default;
bool Console::Init() { return false; }
void Console::Shutdown() {}
void Console::DrainStream(int, int, std::string&, bool) {}
void Console::Update() {}

#endif

} // namespace engine::debug
