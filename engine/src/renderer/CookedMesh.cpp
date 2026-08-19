#include "engine/renderer/CookedMesh.h"

#include <cstdio>
#include <cstring>

namespace engine::renderer {

static_assert(sizeof(Vertex) == 2 * sizeof(glm::vec3),
              "CookedMesh format assumes a tightly packed Vertex{vec3 position; vec3 normal;} "
              "with no padding between them -- true for every compiler/target this project "
              "builds for, but WriteCookedMesh/LoadCookedMesh would need a real per-field "
              "serialization if that ever changed.");

bool WriteCookedMesh(const char* path, const MeshData& mesh) {
    std::FILE* file = std::fopen(path, "wb");
    if (file == nullptr) {
        std::fprintf(stderr, "WriteCookedMesh: failed to open \"%s\" for writing\n", path);
        return false;
    }

    CookedMeshHeader header{};
    std::memcpy(header.magic, kCookedMeshMagic, sizeof(header.magic));
    header.version = kCookedMeshVersion;
    header.vertexCount = static_cast<std::uint32_t>(mesh.vertices.size());
    header.indexCount = static_cast<std::uint32_t>(mesh.indices.size());

    bool ok = std::fwrite(&header, sizeof(header), 1, file) == 1;
    if (ok && header.vertexCount > 0) {
        ok = std::fwrite(mesh.vertices.data(), sizeof(Vertex), header.vertexCount, file) ==
             header.vertexCount;
    }
    if (ok && header.indexCount > 0) {
        ok = std::fwrite(mesh.indices.data(), sizeof(std::uint32_t), header.indexCount, file) ==
             header.indexCount;
    }

    std::fclose(file);
    if (!ok) {
        std::fprintf(stderr, "WriteCookedMesh: failed while writing \"%s\"\n", path);
    }
    return ok;
}

bool LoadCookedMesh(const char* path, MeshData& outMesh) {
    std::FILE* file = std::fopen(path, "rb");
    if (file == nullptr) {
        std::fprintf(stderr, "LoadCookedMesh: failed to open \"%s\"\n", path);
        return false;
    }

    CookedMeshHeader header{};
    if (std::fread(&header, sizeof(header), 1, file) != 1) {
        std::fprintf(stderr, "LoadCookedMesh: \"%s\" is too small for a header\n", path);
        std::fclose(file);
        return false;
    }
    if (std::memcmp(header.magic, kCookedMeshMagic, sizeof(header.magic)) != 0) {
        std::fprintf(stderr, "LoadCookedMesh: \"%s\" is not a cooked mesh (bad magic)\n", path);
        std::fclose(file);
        return false;
    }
    if (header.version != kCookedMeshVersion) {
        std::fprintf(stderr, "LoadCookedMesh: \"%s\" has version %u, expected %u\n", path,
                      header.version, kCookedMeshVersion);
        std::fclose(file);
        return false;
    }

    outMesh.vertices.resize(header.vertexCount);
    outMesh.indices.resize(header.indexCount);

    bool ok = true;
    if (header.vertexCount > 0) {
        ok = std::fread(outMesh.vertices.data(), sizeof(Vertex), header.vertexCount, file) ==
             header.vertexCount;
    }
    if (ok && header.indexCount > 0) {
        ok = std::fread(outMesh.indices.data(), sizeof(std::uint32_t), header.indexCount, file) ==
             header.indexCount;
    }

    std::fclose(file);
    if (!ok) {
        std::fprintf(stderr, "LoadCookedMesh: failed while reading \"%s\" (truncated file?)\n", path);
    }
    return ok;
}

} // namespace engine::renderer
