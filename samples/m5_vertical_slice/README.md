# m5_vertical_slice — target milestone

Not started. Goal: move the cube with the keyboard, Space to jump (real physics impulse),
touch another object and a script reacts to the collision — everything together, same
frame, no race conditions (docs/02 sections 2 and 4, docs/03 section 10).

Needs: `physics/CollisionCallbackDispatcher`, `ScriptComponent` extended with
`OnCollisionEnter/Stay/Exit` + `OnTriggerEnter/Exit`, `physics/Raycast`,
`scripts/PlayerScript.h` (input + jump impulse + ground raycast) and `scripts/TargetScript.h`
(reacts to `OnCollisionEnter`). `core/Application` implements the full frame:
Poll Input -> Script phase -> barrier -> Physics phase -> barrier ->
Collision Callback phase -> Post-Physics/Render (CLAUDE.md section 4).
