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
- `CollisionCallbackDispatcher.h` + `.cpp` — done (M5): the one `JPH::ContactListener` Jolt
  allows. `OnContactAdded/Persisted/Removed` fire on solver worker threads *during*
  `PhysicsWorld::Step()`, so they only buffer raw events behind a mutex (docs/01 section
  9.6's "per-thread lock-free buffers" simplified the same way `jobs/JobSystem.h`'s own
  worker queues are -- contact events are comparatively rare, unlike that class's
  per-frame-hot job queue). `Dispatch()` -- the actual Collision Callback phase, called
  once per frame from `PhysicsWorld::DispatchCollisionCallbacks()` -- drains that buffer
  single-threaded and turns Added/Persisted/Removed into
  `OnCollisionEnter/Stay/Exit`/`OnTriggerEnter/Exit` on the right scripts, keyed by each
  Jolt body's packed `Entity` user data (`EntityUserData.h`).
- `Raycast.h` — done (M5): `RaycastHit`, consumed by `PhysicsWorld::Raycast()` (a member,
  not a `Physics::` free function/singleton -- keeps the project's existing
  explicit-dependency style). `OverlapSphere` is deferred -- not needed for M5's exit
  criterion (only `PlayerScript`'s ground-check raycast is), would need a
  `JPH::CollideShapeCollector` subclass that isn't otherwise justified yet.
- `EntityUserData.h` — done (M5): packs `Entity{index, generation}` into the `uint64` every
  Jolt body already carries (`JPH::Body::GetUserData()`) -- the standard Jolt pattern,
  shared by `PhysicsWorld.cpp` (packs at body creation) and
  `CollisionCallbackDispatcher.cpp` (unpacks in contact callbacks).
- `CollisionInfo.h` — done (M5): passed to `OnCollisionEnter/Stay`. `impactSpeed` is
  relative speed along the contact normal, not a true post-solve impulse (Jolt's
  `EstimateCollisionResponse()` computes that properly but wasn't needed for M5's scope) --
  see the header's own comment.

No script ever touches physics data outside the phases in CLAUDE.md section 4 — the barrier is structural, not a matter of discipline (rule 1).
