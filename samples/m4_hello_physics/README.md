# m4_hello_physics

Done. The cube falls under gravity and comes to rest on a plane, working
Jolt<->JobSystem adapter (docs/02 section 4, docs/03 section 9).

Static ground slab (a scaled instance of the shared cube mesh) + one dynamic cube dropped
5 units above it. `physics/PhysicsWorld` + `physics/JoltJobSystemAdapter` +
`ecs/components/RigidbodyComponent`/`ColliderComponent` + `physics/PhysicsPhase` (fixed
60 Hz timestep). No scripting yet (that's M3, already done, and M5's jump).
