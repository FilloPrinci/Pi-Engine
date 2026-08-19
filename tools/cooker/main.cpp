#include "AssetImporter.h"
#include "engine/renderer/CookedMesh.h"
#include "engine/renderer/MeshLoader.h"

#include <cstdio>
#include <cstdlib>

// Minimal offline Asset Cooker (docs/01 section 12.4): "a CLI tool separate from the
// engine runtime -- shares code where it makes sense (e.g. the math library), but is a
// standalone executable: the heavy conversion libraries never end up in the shipped
// game." This tool links neither engine_core nor any of its runtime dependencies
// (Vulkan/SDL2/Jolt) -- it compiles engine/src/{asset,renderer}/*.cpp directly as its own
// sources (see tools/cooker/CMakeLists.txt), reusing that code without pulling in
// anything the engine's rendering/physics/platform layers need.
//
// M6/M7 scope: meshes only (glTF/GLB -> the minimal binary format in
// engine/renderer/CookedMesh.h), tagged with a persistent Asset GUID (docs/01 section
// 12.3, engine/asset/AssetGuid.h). Textures/shaders/scenes are all still out of scope
// (docs/01 section 12.2 describes the fuller pipeline; see tools/cooker/README.md and
// CLAUDE.md for what's actually built vs. deferred).
int main(int argc, char** argv) {
    if (argc != 3) {
        std::fprintf(stderr, "usage: cooker <input.gltf|.glb> <output.mesh>\n");
        return EXIT_FAILURE;
    }

    const char* inputPath = argv[1];
    const char* outputPath = argv[2];

    const engine::asset::AssetGuid guid = GetOrCreateAssetGuid(inputPath);

    engine::renderer::MeshData mesh;
    if (!engine::renderer::LoadMesh(inputPath, mesh)) {
        std::fprintf(stderr, "cooker: failed to load \"%s\"\n", inputPath);
        return EXIT_FAILURE;
    }

    if (!engine::renderer::WriteCookedMesh(outputPath, guid, mesh)) {
        std::fprintf(stderr, "cooker: failed to write \"%s\"\n", outputPath);
        return EXIT_FAILURE;
    }

    std::printf("cooker: \"%s\" -> \"%s\" (guid %s, %zu vertices, %zu indices)\n", inputPath,
                outputPath, engine::asset::ToString(guid).c_str(), mesh.vertices.size(),
                mesh.indices.size());
    return EXIT_SUCCESS;
}
