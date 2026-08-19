#pragma once

#include "engine/asset/AssetGuid.h"
#include "engine/renderer/MeshLoader.h"

#include <cstdint>

namespace engine::renderer {

// Minimal binary mesh format produced offline by tools/cooker and consumed at runtime by
// LoadCookedMesh() -- the "Cooked Assets" half of docs/01 section 12.1's Source/Cooked
// split ("the shipped game only loads Cooked Assets -- no heavy conversion/decoding
// library... ends up in the final binary"). Just a flat dump of exactly the
// Position+Normal+index data LoadMesh() already extracts from a glTF source (M1 scope,
// still the only vertex layout the engine has through M6's ForwardLitPipeline) -- no
// cgltf/JSON parsing at runtime for a cooked mesh.
//
// Deliberately minimal for this milestone (docs/01 section 12.2 describes the fuller
// version): no vertex-cache optimization (meshoptimizer), no LOD generation, no
// per-hardware-profile variants (no Hardware Profile System built yet), no compression.
// See engine/include/engine/renderer/README.md for what's built vs. explicitly deferred.
//
// Little-endian only: both this project's target platforms (x86_64 dev machine, ARM
// Cortex-A72/A76 Pi4/Pi5) are little-endian, so the header/vertex/index data is written
// and read with the platform's native byte order -- not portable to a big-endian target,
// not a concern for this project.
struct CookedMeshHeader {
    char magic[4]; // "PICM" -- Pi-Engine Cooked Mesh
    std::uint32_t version;
    std::uint64_t guidHigh; // engine::asset::AssetGuid -- see that header's own comment
    std::uint64_t guidLow;  // (M7, docs/01 section 12.3): the source mesh's persistent id
    std::uint32_t vertexCount;
    std::uint32_t indexCount;
};

inline constexpr char kCookedMeshMagic[4] = {'P', 'I', 'C', 'M'};
inline constexpr std::uint32_t kCookedMeshVersion = 2; // 2: header grew guidHigh/guidLow (M7)

// Writes `mesh` (tagged with its source asset's `guid`, docs/01 section 12.3) to `path` in
// the format above. Offline-tooling code (tools/cooker) -- never called at runtime by the
// engine itself, but kept here (not duplicated in the tool) so the writer and
// LoadCookedMesh() can never silently drift apart.
bool WriteCookedMesh(const char* path, const asset::AssetGuid& guid, const MeshData& mesh);

// Reads a file written by WriteCookedMesh() back into `outMesh`. The runtime-facing half --
// fast, no heap-heavy parsing, safe to call from a sample's startup path the same way
// LoadMesh() is (indeed with a similar signature/error-handling convention). `outGuid` is
// optional (nullptr default): nothing resolves assets by GUID yet (a scene/prefab system
// would), so no sample through M6 needs it -- pass a pointer to get the source asset's id
// back anyway, e.g. for diagnostics.
bool LoadCookedMesh(const char* path, MeshData& outMesh, asset::AssetGuid* outGuid = nullptr);

} // namespace engine::renderer
