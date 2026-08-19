#pragma once

// `cooker mesh <input.gltf|.glb> <output.mesh>` (docs/01 section 12.2). `argv[0]` is
// expected to be "mesh" (unused otherwise, kept so error messages/argc math line up the
// same way for every subcommand -- see CookShader.h's identical convention).
int CookMesh(int argc, char** argv);
