# m5_vertical_slice

Done. Move the cube with the keyboard, Space to jump (real physics impulse), touch
another object and a script reacts — everything together, same frame, no race conditions
(docs/02 sections 2 and 4, docs/03 section 10).

Static ground, a dynamic player (`scripts/PlayerScript.h`: WASD -> queued horizontal
velocity, Space -> queued jump impulse when a downward raycast says it's grounded), and a
static trigger volume (`scripts/TargetScript.h`: shrinks and logs on
`OnTriggerEnter`/`OnTriggerExit`). `core/Application`'s callback finally runs the full
frame from CLAUDE.md section 4: Poll Input -> Script phase -> barrier -> Physics phase ->
barrier -> Collision Callback phase -> Post-Physics/Render.
