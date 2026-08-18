# m3_hello_script

Done. An object moves via keyboard input, minimal Script System
(docs/02 section 4, docs/03 section 8).

Single cube entity, `MoveScript` (`scripts/MoveScript.h`) instantiated through
`ScriptRegistry::Create("MoveScript")` and moved on the XZ plane via WASD/arrow keys,
static camera so the movement is easy to see on screen.
