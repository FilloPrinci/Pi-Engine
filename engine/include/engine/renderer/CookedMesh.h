#pragma once

#include "engine/asset/AssetGuid.h"
#include "engine/renderer/MeshLoader.h"

#include <cstdint>
#include <vector>

namespace engine::renderer {

// Minimal binary mesh format produced offline by tools/cooker and consumed at runtime by
// LoadCookedMesh() -- the "Cooked Assets" half of docs/01 section 12.1's Source/Cooked
// split ("the shipped game only loads Cooked Assets -- no heavy conversion/decoding
// library... ends up in the final binary"). No cgltf/JSON parsing at runtime for a cooked
// mesh.
//
// One shared vertex buffer plus `lodCount` index buffers (docs/01 section 12.2: "the
// Cooker... automatically generates the LOD levels... by decimating the mesh"). LOD index
// 0 is always the full-detail mesh `cooker mesh` loaded from the source; index 1, 2, ...
// are progressively decimated via meshoptimizer's edge-collapse simplifier
// (tools/cooker/CookMesh.cpp), each still indexing into the *same* vertex array -- LOD
// generation only ever needs to shrink the index buffer, not duplicate/shrink vertex data
// (meshopt_simplify's own contract), which keeps this format simple and each LOD cheap to
// add. Every index buffer is also vertex-cache optimized (meshopt_optimizeVertexCache) --
// the other half of docs/01 section 12.2's "optimizes vertex order for the GPU cache".
//
// Deliberately minimal for this milestone still: no per-hardware-profile variants (no
// Hardware Profile System built yet -- so nothing *selects* a LOD by distance/profile at
// runtime either, that's future work once one exists), no compression. See
// engine/include/engine/renderer/README.md for what's built vs. explicitly deferred.
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
    std::uint32_t lodCount; // Number of index buffers that follow the shared vertex buffer.
};

inline constexpr char kCookedMeshMagic[4] = {'P', 'I', 'C', 'M'};
// 2: guidHigh/guidLow. 3: Vertex grew uv. 4: multiple LOD index buffers replaced the single
// top-level indexCount (M7 LOD-generation step) -- see the struct comment above.
inline constexpr std::uint32_t kCookedMeshVersion = 4;

// Writes `vertices` (the shared vertex buffer) plus one index buffer per entry in `lods`
// (`lods[0]` is LOD0, the highest detail) to `path`, tagged with the source asset's `guid`
// (docs/01 section 12.3). Offline-tooling code (tools/cooker) -- never called at runtime
// by the engine itself, but kept here (not duplicated in the tool) so the writer and
// LoadCookedMesh() can never silently drift apart.
bool WriteCookedMeshLODs(const char* path, const asset::AssetGuid& guid,
                          const std::vector<Vertex>& vertices,
                          const std::vector<std::vector<std::uint32_t>>& lods);

// Convenience wrapper for the common single-LOD case (every mesh before this milestone,
// and any mesh `cooker mesh` couldn't usefully decimate further) -- equivalent to
// `WriteCookedMeshLODs(path, guid, mesh.vertices, {mesh.indices})`.
bool WriteCookedMesh(const char* path, const asset::AssetGuid& guid, const MeshData& mesh);

// Reads a file written by WriteCookedMeshLODs()/WriteCookedMesh() back into `outMesh`,
// loading LOD level `lodIndex` (default 0 -- the highest detail, i.e. exactly what every
// pre-LOD call site already expects, since it's what WriteCookedMesh()'s single-LOD files
// have always stored at index 0). Fails if `lodIndex >= lodCount`. The runtime-facing
// half -- fast, no heap-heavy parsing, safe to call from a sample's startup path the same
// way LoadMesh() is (indeed with a similar signature/error-handling convention). `outGuid`
// is optional (nullptr default): pass a pointer to get the source asset's id back, e.g.
// for diagnostics or GUID-based resolution (engine::scene).
bool LoadCookedMesh(const char* path, MeshData& outMesh, asset::AssetGuid* outGuid = nullptr,
                     std::uint32_t lodIndex = 0);

// Reads just the header to report how many LOD levels `path` has, without loading any
// vertex/index data -- lets a caller (e.g. samples/m7_lod) discover the valid range for
// `lodIndex` above before deciding which one(s) to load.
bool GetCookedMeshLODCount(const char* path, std::uint32_t& outLodCount);

} // namespace engine::renderer
