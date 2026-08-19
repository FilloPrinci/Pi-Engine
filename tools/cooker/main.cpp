#include "CookMesh.h"
#include "CookShader.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

// Minimal offline Asset Cooker (docs/01 section 12.4): "a CLI tool separate from the
// engine runtime -- shares code where it makes sense (e.g. the math library), but is a
// standalone executable: the heavy conversion libraries never end up in the shipped
// game." This tool links neither engine_core nor any of its runtime dependencies
// (Vulkan/SDL2/Jolt) -- it compiles engine/src/{asset,renderer}/*.cpp directly as its own
// sources (see tools/cooker/CMakeLists.txt), reusing that code without pulling in
// anything the engine's rendering/physics/platform layers need.
//
// Two subcommands so far (M6/M7): `mesh` (glTF/GLB -> engine/renderer/CookedMesh.h's
// binary format, tagged with a persistent Asset GUID, docs/01 section 12.3) and `shader`
// (GLSL -> SPIR-V via shaderc, replacing every sample's own glslc invocation). Textures/
// scenes are still out of scope for this tool -- scenes stay raw JSON even after M7 (see
// engine/scene/README.md), and there's no texture pipeline built yet at all.
int main(int argc, char** argv) {
    if (argc < 2) {
        std::fprintf(stderr, "usage: cooker <mesh|shader> <input> <output>\n");
        return EXIT_FAILURE;
    }

    const char* command = argv[1];
    // Shift argv so each Cook*() sees argv[0]="mesh"/"shader", argv[1]=input,
    // argv[2]=output -- same argc/argv shape main() itself gets, just one level in.
    if (std::strcmp(command, "mesh") == 0) {
        return CookMesh(argc - 1, argv + 1);
    }
    if (std::strcmp(command, "shader") == 0) {
        return CookShader(argc - 1, argv + 1);
    }

    std::fprintf(stderr, "cooker: unknown subcommand \"%s\" (expected \"mesh\" or \"shader\")\n",
                 command);
    return EXIT_FAILURE;
}
