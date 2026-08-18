#pragma once

#include <cstdio>
#include <cstdlib>

// Programmer-error assertion (CLAUDE.md, sections 5 and 9 rule 5): never a C++ exception
// on the engine hot-path. Debug builds abort with a message; release builds are a no-op so
// this never costs anything in a shipped build. Not a substitute for real error handling on
// recoverable failures (return code / bool + out param) -- only for invariants that should
// be structurally impossible if the engine itself is correct.
#ifndef NDEBUG
#define ENGINE_ASSERT(condition, message)                                                        \
    do {                                                                                          \
        if (!(condition)) {                                                                       \
            std::fprintf(stderr, "ENGINE_ASSERT failed: %s\n  at %s:%d\n  condition: %s\n",       \
                          (message), __FILE__, __LINE__, #condition);                             \
            std::abort();                                                                         \
        }                                                                                          \
    } while (false)
#else
#define ENGINE_ASSERT(condition, message)                                                        \
    do {                                                                                          \
        (void)sizeof((condition));                                                                \
    } while (false)
#endif
