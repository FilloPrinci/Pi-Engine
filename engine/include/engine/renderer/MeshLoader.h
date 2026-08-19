#pragma once

#include <glm/glm.hpp>

#include <cstdint>
#include <vector>

namespace engine::renderer {

struct Vertex {
    glm::vec3 position;
    glm::vec3 normal;
    glm::vec2 uv; // TEXCOORD_0 (M7 Asset Pipeline textures step); (0,0) if the source has none.
};

struct MeshData {
    std::vector<Vertex> vertices;
    std::vector<std::uint32_t> indices;
};

// Loads the first mesh primitive found in a glTF/GLB file via cgltf (docs/03 section 6).
// POSITION + NORMAL + TEXCOORD_0 (UV optional, defaults to (0,0) if the source has none),
// single primitive, no materials/skinning -- that's Asset Pipeline/Cooker territory
// (docs/01 section 12), still out of scope.
bool LoadMesh(const char* path, MeshData& outMesh);

} // namespace engine::renderer
