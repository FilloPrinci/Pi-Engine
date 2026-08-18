# jobs

- `JobSystem.h` + `.cpp` — M2: work-stealing task graph, worker count = core count - 1 as an initial estimate, to validate on real hardware (docs/01 section 9.3).

Shared by culling (M2), physics (M4, via `physics/JoltJobSystemAdapter.h`), and — later — animation. Never a second, independent thread pool (CLAUDE.md rule 2).
