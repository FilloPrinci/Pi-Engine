# ecs

- `Entity.h` -- done (M2): lightweight handle (index + generation, invalidates references
  to destroyed entities).
- `ComponentStorage.h` -- done (M2): sparse-set storage backing `World`'s component
  arrays -- one `ComponentStorage<T>` per known component type, dense/contiguous, O(1)
  lookup, swap-and-pop removal (docs/01 section 2.3/4).
- `World.h` + `.cpp` -- done (M2): entity lifetime (create/destroy, generation
  invalidation, index recycling) + Transform/Mesh component storage.
- `components/TransformComponent.h`, `components/MeshComponent.h` -- done (M2).
- `components/RigidbodyComponent.h`, `components/ColliderComponent.h` -- M4.

Never a permanent raw pointer to a component — always `ComponentHandle<T>`
(script/ComponentHandle.h, done M3, CLAUDE.md rule 4). M2's systems (FrustumCuller) still
resolve components fresh via `World::Get*()` every call, since nothing there holds a
component reference across a frame boundary; scripts do, via `World::GetComponent<T>()`
(the generic dispatch `ComponentHandle<T>` re-resolves through on every access -- see
World.h's comment next to it).
