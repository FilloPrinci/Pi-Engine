#pragma once

#include "engine/renderer/MeshLoader.h"

#include <cstdint>

namespace engine::renderer {

// Minimal binary mesh format produced offline by tools/cooker and consumed at runtime by
// LoadCookedMesh() -- the "Cooked Assets" half of docs/01 section 12.1's Source/Cooked
// split ("the shipped game only loads Cooked Assets -- no heavy conversion/decoding
// library... ends up in the final binary"). Just a flat dump of exactly the
// Position+Normal+index data LoadMesh() already extracts from a glTF source (M1 scope,
// still the only vertex layout the engine has through M5's ForwardLitPipeline) -- no
// cgltf/JSON parsing at runtime for a cooked mesh.
//
// Deliberately minimal for this milestone (docs/01 section 12.2 describes the fuller
// version): no vertex-cache optimization (meshoptimizer), no LOD generation (out of scope
// per CLAUDE.md section 6), no per-hardware-profile variants (no Hardware Profile System
// built yet), no compression, no Asset GUID (nothing references assets by GUID yet --
// scenes/materials/Prefabs would, none of those exist either). See
// engine/include/engine/renderer/README.md for what's built vs. explicitly deferred.
//
// Little-endian only: both this project's target platforms (x86_64 dev machine, ARM
// Cortex-A72/A76 Pi4/Pi5) are little-endian, so the header/vertex/index data is written
// and read with the platform's native byte order -- not portable to a big-endian target,
// not a concern for this project.
struct CookedMeshHeader {
    char magic[4]; // "PICM" -- Pi-Engine Cooked Mesh
    std::uint32_t version;
    std::uint32_t vertexCount;
    std::uint32_t indexCount;
};

inline constexpr char kCookedMeshMagic[4] = {'P', 'I', 'C', 'M'};
inline constexpr std::uint32_t kCookedMeshVersion = 1;

// Writes `mesh` to `path` in the format above. Offline-tooling code (tools/cooker) --
// never called at runtime by the engine itself, but kept here (not duplicated in the
// tool) so the writer and LoadCookedMesh() can never silently drift apart.
bool WriteCookedMesh(const char* path, const MeshData& mesh);

// Reads a file written by WriteCookedMesh() back into `outMesh`. The runtime-facing half --
// fast, no heap-heavy parsing, safe to call from a sample's startup path the same way
// LoadMesh() is (indeed with the exact same signature/error-handling convention).
bool LoadCookedMesh(const char* path, MeshData& outMesh);

} // namespace engine::renderer
