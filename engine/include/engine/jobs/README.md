# jobs

- `JobSystem.h` + `.cpp` -- done (M2): work-stealing task graph (mutex-protected
  per-worker deques, not lock-free -- see the header's comment on why that's fine for
  now), worker count = core count - 1 (docs/01 section 9.3). `ParallelFor` is the one
  primitive exposed so far, driving `renderer/FrustumCuller`. Validated on physical Pi4:
  3 worker threads (4 cores - 1), unit tests (10000-item ParallelFor coverage +
  correctness) pass on both x86_64 and Pi4's weaker ARM memory model.

Shared by culling (M2), physics (M4, via `physics/JoltJobSystemAdapter.h`), and — later — animation. Never a second, independent thread pool (CLAUDE.md rule 2).
