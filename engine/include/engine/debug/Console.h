#pragma once

#include <cstddef>
#include <deque>
#include <string>

namespace engine::debug {

// Captures the engine's existing stdout/stderr output (Editor step E5,
// docs/06-editor-roadmap.md) into an in-memory buffer an ImGui panel can render, instead
// of only being visible in whatever terminal launched the process. Every
// std::printf/std::fprintf(stderr, ...) call site across the engine already exists and
// stays exactly as it is -- this captures the *existing* output stream via low-level file
// descriptor redirection (dup2 onto a pipe), not a new logging API every call site would
// need to be rewritten to use.
//
// Output is teed, not replaced: everything captured is also written back to the original
// stdout/stderr file descriptors, so redirecting the process's own output to a file/
// terminal (`editor > log.txt 2>&1`, or just running it interactively) keeps working
// exactly as before -- the Console panel is an addition, not a replacement.
//
// POSIX only (dup/dup2/pipe/fcntl) -- matches this project's Linux-primary development
// focus (CLAUDE.md section 2, "Primary development platform: Linux x86_64 desktop,
// verification on physical Pi4"). On any other platform Init() returns false and Update()
// is a no-op; callers are expected to handle that gracefully (an empty/disabled Console
// panel), not treat it as fatal -- same "degrade, don't hard-fail" precedent as
// SpawnEntities() adding a Rigidbody with no live Jolt body when no PhysicsWorld is given.
class Console {
public:
    struct Line {
        std::string text;
        bool isError = false; // true if captured from stderr
    };

    Console() = default;
    ~Console();

    Console(const Console&) = delete;
    Console& operator=(const Console&) = delete;

    // Redirects stdout/stderr through internal pipes. Call as the very first thing in
    // main(), before any printf/fprintf -- forcing line-buffered stdio (this class does
    // that) is only well-defined before the stream has done any other I/O.
    bool Init();
    void Shutdown();

    // Drains whatever's been written to stdout/stderr since the last call, splitting into
    // lines and appending to the internal buffer (oldest lines dropped past kMaxLines) --
    // call once per frame, before drawing a Console panel that reads GetLines().
    void Update();

    const std::deque<Line>& GetLines() const { return m_lines; }
    void Clear() { m_lines.clear(); }

private:
    void DrainStream(int pipeReadFd, int originalFd, std::string& partialLine, bool isError);

    static constexpr std::size_t kMaxLines = 2000;

    int m_stdoutPipeRead = -1;
    int m_stdoutOriginalFd = -1;
    int m_stderrPipeRead = -1;
    int m_stderrOriginalFd = -1;
    std::string m_stdoutPartialLine;
    std::string m_stderrPartialLine;
    std::deque<Line> m_lines;
    bool m_initialized = false;
};

} // namespace engine::debug
