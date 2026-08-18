# ecs

- `Entity.h` — M2: lightweight handle (index + generation, invalidates references to destroyed entities).
- `World.h` + `.cpp` — M2: data-oriented component storage, contiguous arrays per type (docs/01 section 2.3/4).
- `components/TransformComponent.h`, `components/MeshComponent.h` — M2.
- `components/RigidbodyComponent.h`, `components/ColliderComponent.h` — M4.

Never a permanent raw pointer to a component — always `ComponentHandle<T>` (script/ComponentHandle.h, CLAUDE.md rule 4).
