# physics

- `PhysicsWorld.h` + `.cpp` — M4: wrapper around `JPH::PhysicsSystem`.
- `JoltJobSystemAdapter.h` + `.cpp` — M4: implements `JPH::JobSystem`, injects Jolt's jobs into our own `JobSystem` (docs/01 section 9.3) instead of a second thread pool.
- `PhysicsPhase.h` + `.cpp` — M4: fixed timestep (accumulator pattern), barrier orchestration between Script and Post-Physics.
- `CollisionCallbackDispatcher.h` + `.cpp` — M5: per-thread lock-free buffers during the solver, single-thread dispatch of collision callbacks afterwards (docs/01 section 9.6).
- `Raycast.h` — M5: `Physics::Raycast` / `Physics::OverlapSphere`.

No script ever touches physics data outside the phases in CLAUDE.md section 4 — the barrier is structural, not a matter of discipline (rule 1).
