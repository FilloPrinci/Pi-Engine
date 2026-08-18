#pragma once

#include <glm/glm.hpp>

#include <cstdint>
#include <vector>

namespace engine::renderer {

struct Vertex {
    glm::vec3 position;
    glm::vec3 normal;
};

struct MeshData {
    std::vector<Vertex> vertices;
    std::vector<std::uint32_t> indices;
};

// Loads the first mesh primitive found in a glTF/GLB file via cgltf (docs/03 section 6).
// M1 scope: POSITION + NORMAL only, single primitive, no materials/textures/skinning --
// that's Asset Pipeline/Cooker territory (docs/01 section 12), out of scope through M5.
bool LoadMesh(const char* path, MeshData& outMesh);

} // namespace engine::renderer
