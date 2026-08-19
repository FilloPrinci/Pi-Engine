#include "engine/renderer/CookedMesh.h"

#include <cstdio>
#include <cstring>

namespace engine::renderer {

static_assert(sizeof(Vertex) == 2 * sizeof(glm::vec3) + sizeof(glm::vec2),
              "CookedMesh format assumes a tightly packed "
              "Vertex{vec3 position; vec3 normal; vec2 uv;} with no padding between them -- "
              "true for every compiler/target this project builds for, but "
              "WriteCookedMeshLODs/LoadCookedMesh would need a real per-field serialization "
              "if that ever changed.");

bool WriteCookedMeshLODs(const char* path, const asset::AssetGuid& guid,
                          const std::vector<Vertex>& vertices,
                          const std::vector<std::vector<std::uint32_t>>& lods) {
    std::FILE* file = std::fopen(path, "wb");
    if (file == nullptr) {
        std::fprintf(stderr, "WriteCookedMeshLODs: failed to open \"%s\" for writing\n", path);
        return false;
    }

    CookedMeshHeader header{};
    std::memcpy(header.magic, kCookedMeshMagic, sizeof(header.magic));
    header.version = kCookedMeshVersion;
    header.guidHigh = guid.high;
    header.guidLow = guid.low;
    header.vertexCount = static_cast<std::uint32_t>(vertices.size());
    header.lodCount = static_cast<std::uint32_t>(lods.size());

    bool ok = std::fwrite(&header, sizeof(header), 1, file) == 1;
    if (ok && header.vertexCount > 0) {
        ok = std::fwrite(vertices.data(), sizeof(Vertex), header.vertexCount, file) ==
             header.vertexCount;
    }
    for (const std::vector<std::uint32_t>& lod : lods) {
        if (!ok) {
            break;
        }
        const auto indexCount = static_cast<std::uint32_t>(lod.size());
        ok = std::fwrite(&indexCount, sizeof(indexCount), 1, file) == 1;
        if (ok && indexCount > 0) {
            ok = std::fwrite(lod.data(), sizeof(std::uint32_t), indexCount, file) == indexCount;
        }
    }

    std::fclose(file);
    if (!ok) {
        std::fprintf(stderr, "WriteCookedMeshLODs: failed while writing \"%s\"\n", path);
    }
    return ok;
}

bool WriteCookedMesh(const char* path, const asset::AssetGuid& guid, const MeshData& mesh) {
    return WriteCookedMeshLODs(path, guid, mesh.vertices, {mesh.indices});
}

namespace {

// Shared header-read + validation, used by both LoadCookedMesh() and
// GetCookedMeshLODCount() -- keeps the magic/version check in exactly one place.
bool ReadValidHeader(std::FILE* file, const char* path, CookedMeshHeader& outHeader) {
    if (std::fread(&outHeader, sizeof(outHeader), 1, file) != 1) {
        std::fprintf(stderr, "LoadCookedMesh: \"%s\" is too small for a header\n", path);
        return false;
    }
    if (std::memcmp(outHeader.magic, kCookedMeshMagic, sizeof(outHeader.magic)) != 0) {
        std::fprintf(stderr, "LoadCookedMesh: \"%s\" is not a cooked mesh (bad magic)\n", path);
        return false;
    }
    if (outHeader.version != kCookedMeshVersion) {
        std::fprintf(stderr, "LoadCookedMesh: \"%s\" has version %u, expected %u\n", path,
                      outHeader.version, kCookedMeshVersion);
        return false;
    }
    return true;
}

} // namespace

bool LoadCookedMesh(const char* path, MeshData& outMesh, asset::AssetGuid* outGuid,
                     std::uint32_t lodIndex) {
    std::FILE* file = std::fopen(path, "rb");
    if (file == nullptr) {
        std::fprintf(stderr, "LoadCookedMesh: failed to open \"%s\"\n", path);
        return false;
    }

    CookedMeshHeader header{};
    if (!ReadValidHeader(file, path, header)) {
        std::fclose(file);
        return false;
    }
    if (lodIndex >= header.lodCount) {
        std::fprintf(stderr, "LoadCookedMesh: \"%s\" has %u LOD(s), requested index %u\n", path,
                      header.lodCount, lodIndex);
        std::fclose(file);
        return false;
    }

    if (outGuid != nullptr) {
        outGuid->high = header.guidHigh;
        outGuid->low = header.guidLow;
    }

    outMesh.vertices.resize(header.vertexCount);
    bool ok = true;
    if (header.vertexCount > 0) {
        ok = std::fread(outMesh.vertices.data(), sizeof(Vertex), header.vertexCount, file) ==
             header.vertexCount;
    }

    outMesh.indices.clear();
    for (std::uint32_t lod = 0; ok && lod < header.lodCount; ++lod) {
        std::uint32_t indexCount = 0;
        ok = std::fread(&indexCount, sizeof(indexCount), 1, file) == 1;
        if (!ok) {
            break;
        }
        if (lod == lodIndex) {
            outMesh.indices.resize(indexCount);
            if (indexCount > 0) {
                ok = std::fread(outMesh.indices.data(), sizeof(std::uint32_t), indexCount, file) ==
                     indexCount;
            }
        } else if (indexCount > 0) {
            ok = std::fseek(file, static_cast<long>(indexCount * sizeof(std::uint32_t)),
                             SEEK_CUR) == 0;
        }
    }

    std::fclose(file);
    if (!ok) {
        std::fprintf(stderr, "LoadCookedMesh: failed while reading \"%s\" (truncated file?)\n", path);
    }
    return ok;
}

bool GetCookedMeshLODCount(const char* path, std::uint32_t& outLodCount) {
    std::FILE* file = std::fopen(path, "rb");
    if (file == nullptr) {
        std::fprintf(stderr, "GetCookedMeshLODCount: failed to open \"%s\"\n", path);
        return false;
    }

    CookedMeshHeader header{};
    const bool ok = ReadValidHeader(file, path, header);
    std::fclose(file);
    if (!ok) {
        return false;
    }
    outLodCount = header.lodCount;
    return true;
}

} // namespace engine::renderer
