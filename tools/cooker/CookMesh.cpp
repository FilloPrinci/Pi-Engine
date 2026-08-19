#include "CookMesh.h"

#include "AssetImporter.h"
#include "engine/renderer/CookedMesh.h"
#include "engine/renderer/MeshLoader.h"

#include <meshoptimizer.h>

#include <cstdio>
#include <cstdlib>

using engine::renderer::Vertex;

namespace {

// One LOD ladder rung: `triangleRatio` of LOD0's triangle count is the simplifier's
// *target* (meshopt_simplify may stop earlier if it would exceed `targetError`, docs/01
// section 12.2's "automatically generates the LOD levels... by decimating the mesh").
// `targetError` is relative to the mesh's extents (meshoptimizer's own convention) --
// loosened a little at each further LOD since more aggressive decimation needs more
// slack to reach its target at all.
struct LodRung {
    float triangleRatio;
    float targetError;
};

constexpr LodRung kLodRungs[] = {
    {0.5f, 0.01f},  // LOD1: ~50% triangles
    {0.25f, 0.02f}, // LOD2: ~25% triangles
};

// Simplifies `lod0Indices` down towards `rung.triangleRatio` of its triangle count, then
// runs the result through meshoptimizer's vertex-cache optimizer (docs/01 section 12.2's
// other half: "optimizes vertex order for the GPU cache"). Always simplifies against LOD0
// (not the previous LOD) so each rung's quality is controlled directly against the
// original, not compounded through earlier decimation.
std::vector<std::uint32_t> GenerateLod(const std::vector<Vertex>& vertices,
                                        const std::vector<std::uint32_t>& lod0Indices,
                                        const LodRung& rung) {
    const std::size_t targetIndexCount =
        (static_cast<std::size_t>(static_cast<float>(lod0Indices.size()) * rung.triangleRatio) /
         3) *
        3; // meshopt_simplify wants a whole number of triangles.

    std::vector<std::uint32_t> simplified(lod0Indices.size());
    const std::size_t newIndexCount = meshopt_simplify(
        simplified.data(), lod0Indices.data(), lod0Indices.size(),
        reinterpret_cast<const float*>(vertices.data()), vertices.size(), sizeof(Vertex),
        targetIndexCount, rung.targetError, /*options=*/0, /*result_error=*/nullptr);
    simplified.resize(newIndexCount);

    std::vector<std::uint32_t> optimized(newIndexCount);
    meshopt_optimizeVertexCache(optimized.data(), simplified.data(), newIndexCount,
                                 vertices.size());
    return optimized;
}

} // namespace

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

    // LOD0 is the source mesh as-is, just vertex-cache optimized -- no simplification.
    std::vector<std::uint32_t> lod0(mesh.indices.size());
    meshopt_optimizeVertexCache(lod0.data(), mesh.indices.data(), mesh.indices.size(),
                                 mesh.vertices.size());

    std::vector<std::vector<std::uint32_t>> lods;
    lods.push_back(lod0);
    for (const LodRung& rung : kLodRungs) {
        lods.push_back(GenerateLod(mesh.vertices, lod0, rung));
    }

    if (!engine::renderer::WriteCookedMeshLODs(outputPath, guid, mesh.vertices, lods)) {
        std::fprintf(stderr, "cooker: failed to write \"%s\"\n", outputPath);
        return EXIT_FAILURE;
    }

    std::printf("cooker: \"%s\" -> \"%s\" (guid %s, %zu vertices, %zu LOD(s):", inputPath,
                outputPath, engine::asset::ToString(guid).c_str(), mesh.vertices.size(),
                lods.size());
    for (const std::vector<std::uint32_t>& lod : lods) {
        std::printf(" %zu tris", lod.size() / 3);
    }
    std::printf(")\n");
    return EXIT_SUCCESS;
}
