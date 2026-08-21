# ecs

- `Entity.h` -- done (M2): lightweight handle (index + generation, invalidates references
  to destroyed entities).
- `ComponentStorage.h` -- done (M2): sparse-set storage backing `World`'s component
  arrays -- one `ComponentStorage<T>` per known component type, dense/contiguous, O(1)
  lookup, swap-and-pop removal (docs/01 section 2.3/4).
- `World.h` + `.cpp` -- done (M2): entity lifetime (create/destroy, generation
  invalidation, index recycling) + Transform/Mesh component storage. `GetWorldMatrix()`/
  `IsDescendantOf()` added post-Editor-E8 (docs/07-unity-parity-analysis.md's Hierarchy
  item) -- see `components/TransformComponent.h`'s own comment for the local-vs-world
  split these exist to bridge.
- `components/TransformComponent.h`, `components/MeshComponent.h` -- done (M2).
  `MeshComponent` gains `meshGuid` in M7 (docs/01 section 12.3, `engine/asset/AssetGuid.h`)
  -- which cooked mesh a `scene/`-spawned entity renders, defaulted to
  `asset::kInvalidAssetGuid` so every M0-M6 sample (which never sets it) is unaffected.
  `TransformComponent` gains `parent` post-Editor-E8 (an `Entity`, `kInvalidEntity` = root)
  -- `GetMatrix()` stays purely *local* on purpose (composing a *world* matrix needs to
  walk the parent chain, which needs a `World` to resolve `Entity -> TransformComponent`
  lookups, so that lives on `World::GetWorldMatrix()` instead). Every renderer that draws
  an entity (Editor Scene View, Play Mode) has to call `GetWorldMatrix()`, not
  `transform->GetMatrix()`, or a parented entity renders at its raw local offset instead
  of composed through its parent -- `FrustumCuller.cpp` still uses the local-only path,
  documented there as a known gap since nothing that culls creates a hierarchy yet.
- `components/RigidbodyComponent.h`, `components/ColliderComponent.h` -- done (M4).
  `RigidbodyComponent` stores a packed `uint32` body id, not a `JPH::BodyID`, so this file
  (included transitively by every `World.h` consumer) never pulls Jolt's headers in --
  `physics/PhysicsWorld.cpp` is the only place that converts between the two. M5 adds
  `AddImpulse()`/`SetHorizontalVelocity()` -- still plain data, no Jolt type involved: a
  script's call just sets a `pending*`/`hasPending*` pair that `PhysicsWorld::Step()`
  drains and applies through the real `BodyInterface` at the start of the next physics
  step, then clears (docs/01 section 9.6: "applied at the start of the next physics step").
  `isStatic` (post-Editor-E8, "make everything the Editor shows manageable" phase 3) --
  read once at `PhysicsWorld::CreateBody()` time (M4 scope, no runtime static/dynamic
  toggling), but now *retained* on the live component too, closing a gap
  `ExtractEntityDescs()` used to have (a Rigidbody's static/dynamic flag was previously
  read-once-and-discarded, so saving a scene with one silently dropped it entirely -- see
  `SceneDocument.h`'s own comment).
- `components/LightComponent.h` -- done (lighting phase A, docs/01 section 8.3's "Low-Poly
  Retro" profile). Directional or Point, color/intensity/range, plus `isStatic`/
  `castsShadow` -- hints only in this phase (every light is re-evaluated every frame
  regardless), reserved for a future static shadow map (phase B, not started) that bakes
  once instead of every frame for lights/geometry flagged as never moving. See
  `renderer/ForwardLitShadedPipeline.h`'s own comment for how a `LightComponent` becomes
  GPU-side data each frame.

Never a permanent raw pointer to a component — always `ComponentHandle<T>`
(script/ComponentHandle.h, done M3, CLAUDE.md rule 4). M2's systems (FrustumCuller) still
resolve components fresh via `World::Get*()` every call, since nothing there holds a
component reference across a frame boundary; scripts do, via `World::GetComponent<T>()`
(the generic dispatch `ComponentHandle<T>` re-resolves through on every access -- see
World.h's comment next to it).
