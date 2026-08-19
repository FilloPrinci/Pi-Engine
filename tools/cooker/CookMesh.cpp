#include "CookMesh.h"

#include "AssetImporter.h"
#include "engine/renderer/CookedMesh.h"
#include "engine/renderer/MeshLoader.h"

#include <cstdio>
#include <cstdlib>

int CookMesh(int argc, char** argv) {
    if (argc != 3) {
        std::fprintf(stderr, "usage: cooker mesh <input.gltf|.glb> <output.mesh>\n");
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
