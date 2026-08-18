# m4_hello_physics

Not started. Goal: cube falls under gravity and comes to rest on a plane, working
Jolt<->JobSystem adapter (docs/02 section 4, docs/03 section 9).

Needs: `physics/PhysicsWorld`, `physics/JoltJobSystemAdapter` (the trickiest piece of this
milestone), `ecs/components/RigidbodyComponent` + `ColliderComponent`, `physics/PhysicsPhase`
(fixed timestep, barriers between Script and Post-Physics).
