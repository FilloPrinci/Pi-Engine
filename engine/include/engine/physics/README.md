# physics

- `PhysicsWorld.h` + `.cpp` — done (M4): wrapper around `JPH::PhysicsSystem`. Deliberately
  Jolt-free in the header (forward declarations + `unique_ptr<IncompleteType>` members) --
  only this .cpp and `JoltJobSystemAdapter.h` (which has to name a JPH base class) ever
  include a Jolt header.
- `JoltJobSystemAdapter.h` + `.cpp` — done (M4): implements `JPH::JobSystemWithBarrier`'s
  remaining virtuals (`GetMaxConcurrency`/`CreateJob`/`FreeJob`/`QueueJob(s)`), injecting
  Jolt's jobs into our own `JobSystem` (docs/01 section 9.3) via the new
  `JobSystem::Submit()` instead of a second thread pool.
- `PhysicsPhase.h` + `.cpp` — done (M4): fixed timestep (accumulator pattern, capped at 4
  steps/frame against the "spiral of death"). The actual synchronization barrier is
  `PhysicsWorld::Step()` returning (it blocks until every worker's contribution to that
  step has completed) -- this class only decides how many times to call it.
- `CollisionCallbackDispatcher.h` + `.cpp` — M5: per-thread lock-free buffers during the solver, single-thread dispatch of collision callbacks afterwards (docs/01 section 9.6).
- `Raycast.h` — M5: `Physics::Raycast` / `Physics::OverlapSphere`.

No script ever touches physics data outside the phases in CLAUDE.md section 4 — the barrier is structural, not a matter of discipline (rule 1).
