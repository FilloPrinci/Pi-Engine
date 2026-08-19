#include "engine/renderer/CookedMesh.h"
#include "engine/renderer/MeshLoader.h"

#include <cstdio>
#include <cstdlib>

// Minimal offline Asset Cooker (docs/01 section 12.4): "a CLI tool separate from the
// engine runtime -- shares code where it makes sense (e.g. the math library), but is a
// standalone executable: the heavy conversion libraries never end up in the shipped
// game." This tool links neither engine_core nor any of its runtime dependencies
// (Vulkan/SDL2/Jolt) -- it compiles engine/src/renderer/MeshLoader.cpp and
// CookedMesh.cpp directly as its own sources (see tools/cooker/CMakeLists.txt), reusing
// that code without pulling in anything the engine's rendering/physics/platform layers
// need.
//
// M6 scope: meshes only (glTF/GLB -> the minimal binary format in
// engine/renderer/CookedMesh.h). Textures/audio/shaders/scenes are all still out of scope
// (docs/01 section 12.2 describes the fuller pipeline; see docs/README/CLAUDE.md for what
// this milestone actually covers).
int main(int argc, char** argv) {
    if (argc != 3) {
        std::fprintf(stderr, "usage: cooker <input.gltf|.glb> <output.mesh>\n");
        return EXIT_FAILURE;
    }

    const char* inputPath = argv[1];
    const char* outputPath = argv[2];

    engine::renderer::MeshData mesh;
    if (!engine::renderer::LoadMesh(inputPath, mesh)) {
        std::fprintf(stderr, "cooker: failed to load \"%s\"\n", inputPath);
        return EXIT_FAILURE;
    }

    if (!engine::renderer::WriteCookedMesh(outputPath, mesh)) {
        std::fprintf(stderr, "cooker: failed to write \"%s\"\n", outputPath);
        return EXIT_FAILURE;
    }

    std::printf("cooker: \"%s\" -> \"%s\" (%zu vertices, %zu indices)\n", inputPath, outputPath,
                mesh.vertices.size(), mesh.indices.size());
    return EXIT_SUCCESS;
}
