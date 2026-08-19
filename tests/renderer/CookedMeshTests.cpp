#include "engine/renderer/CookedMesh.h"

#include <doctest/doctest.h>

#include <cstdio>

using engine::asset::AssetGuid;
using engine::asset::GenerateAssetGuid;
using engine::renderer::GetCookedMeshLODCount;
using engine::renderer::LoadCookedMesh;
using engine::renderer::MeshData;
using engine::renderer::Vertex;
using engine::renderer::WriteCookedMesh;
using engine::renderer::WriteCookedMeshLODs;

namespace {

// Each TEST_CASE uses its own file name -- doctest runs cases in the same process, and a
// shared name would make cases order-dependent (and confusing to debug) if one is ever
// changed to not clean up after itself.
struct ScopedTempFile {
    explicit ScopedTempFile(const char* path) : m_path(path) {}
    ~ScopedTempFile() { std::remove(m_path); }
    const char* m_path;
};

} // namespace

TEST_CASE("WriteCookedMesh/LoadCookedMesh round-trip preserves vertices, indices, and guid") {
    ScopedTempFile file("cooked_mesh_roundtrip.tmp.mesh");

    MeshData original;
    original.vertices = {
        Vertex{glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f), glm::vec2(0.0f, 0.0f)},
        Vertex{glm::vec3(1.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f), glm::vec2(1.0f, 0.0f)},
        Vertex{glm::vec3(0.0f, 1.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f), glm::vec2(0.0f, 1.0f)},
    };
    original.indices = {0, 1, 2};
    const AssetGuid guid = GenerateAssetGuid();

    REQUIRE(WriteCookedMesh(file.m_path, guid, original));

    MeshData loaded;
    AssetGuid loadedGuid;
    REQUIRE(LoadCookedMesh(file.m_path, loaded, &loadedGuid));

    CHECK(loadedGuid == guid);
    REQUIRE(loaded.vertices.size() == original.vertices.size());
    for (std::size_t i = 0; i < original.vertices.size(); ++i) {
        CHECK(loaded.vertices[i].position == original.vertices[i].position);
        CHECK(loaded.vertices[i].normal == original.vertices[i].normal);
        CHECK(loaded.vertices[i].uv == original.vertices[i].uv);
    }
    CHECK(loaded.indices == original.indices);
}

TEST_CASE("LoadCookedMesh's outGuid parameter is optional") {
    ScopedTempFile file("cooked_mesh_no_guid_out.tmp.mesh");
    REQUIRE(WriteCookedMesh(file.m_path, GenerateAssetGuid(), MeshData{}));

    MeshData loaded;
    CHECK(LoadCookedMesh(file.m_path, loaded)); // no third argument -- must still compile/work
}

TEST_CASE("WriteCookedMesh/LoadCookedMesh round-trip handles an empty mesh") {
    ScopedTempFile file("cooked_mesh_empty.tmp.mesh");

    MeshData empty;
    REQUIRE(WriteCookedMesh(file.m_path, GenerateAssetGuid(), empty));

    MeshData loaded;
    REQUIRE(LoadCookedMesh(file.m_path, loaded));
    CHECK(loaded.vertices.empty());
    CHECK(loaded.indices.empty());
}

TEST_CASE("LoadCookedMesh rejects a file that isn't a cooked mesh") {
    ScopedTempFile file("cooked_mesh_garbage.tmp.mesh");

    std::FILE* garbage = std::fopen(file.m_path, "wb");
    REQUIRE(garbage != nullptr);
    const char bytes[] = "not a cooked mesh";
    std::fwrite(bytes, 1, sizeof(bytes), garbage);
    std::fclose(garbage);

    MeshData loaded;
    CHECK_FALSE(LoadCookedMesh(file.m_path, loaded));
}

TEST_CASE("LoadCookedMesh rejects a missing file") {
    MeshData loaded;
    CHECK_FALSE(LoadCookedMesh("this_file_does_not_exist.mesh", loaded));
}

TEST_CASE("LoadCookedMesh rejects a stale version 1 file (pre-GUID format)") {
    ScopedTempFile file("cooked_mesh_old_version.tmp.mesh");

    // Hand-write a v1 header (magic + version + vertex/index counts, no guid fields) --
    // the exact layout WriteCookedMesh produced before M7 added the guid. It's also
    // smaller than the current header, so this is actually rejected by the "too small for
    // a header" check rather than reaching the version comparison -- still exactly the
    // "don't silently misread an old cooked file" guarantee this test cares about.
    struct OldHeaderV1 {
        char magic[4];
        std::uint32_t version;
        std::uint32_t vertexCount;
        std::uint32_t indexCount;
    };
    OldHeaderV1 oldHeader{{'P', 'I', 'C', 'M'}, 1, 0, 0};

    std::FILE* f = std::fopen(file.m_path, "wb");
    REQUIRE(f != nullptr);
    std::fwrite(&oldHeader, sizeof(oldHeader), 1, f);
    std::fclose(f);

    MeshData loaded;
    CHECK_FALSE(LoadCookedMesh(file.m_path, loaded));
}

TEST_CASE("WriteCookedMeshLODs/LoadCookedMesh round-trip selects the requested LOD index") {
    ScopedTempFile file("cooked_mesh_lods.tmp.mesh");

    const std::vector<Vertex> vertices = {
        Vertex{glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f), glm::vec2(0.0f, 0.0f)},
        Vertex{glm::vec3(1.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f), glm::vec2(1.0f, 0.0f)},
        Vertex{glm::vec3(0.0f, 1.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f), glm::vec2(0.0f, 1.0f)},
        Vertex{glm::vec3(1.0f, 1.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f), glm::vec2(1.0f, 1.0f)},
    };
    // LOD0: two triangles (the full quad). LOD1: a single, coarser triangle -- not a
    // realistic simplifier output, just distinct data to prove the right LOD comes back.
    const std::vector<std::uint32_t> lod0 = {0, 1, 2, 1, 3, 2};
    const std::vector<std::uint32_t> lod1 = {0, 1, 3};
    const AssetGuid guid = GenerateAssetGuid();

    REQUIRE(WriteCookedMeshLODs(file.m_path, guid, vertices, {lod0, lod1}));

    std::uint32_t lodCount = 0;
    REQUIRE(GetCookedMeshLODCount(file.m_path, lodCount));
    CHECK(lodCount == 2);

    MeshData loadedLod0;
    AssetGuid loadedGuid;
    REQUIRE(LoadCookedMesh(file.m_path, loadedLod0, &loadedGuid, 0));
    CHECK(loadedGuid == guid);
    CHECK(loadedLod0.vertices.size() == vertices.size());
    CHECK(loadedLod0.indices == lod0);

    MeshData loadedLod1;
    REQUIRE(LoadCookedMesh(file.m_path, loadedLod1, nullptr, 1));
    CHECK(loadedLod1.vertices.size() == vertices.size()); // Shared vertex buffer, same for every LOD.
    CHECK(loadedLod1.indices == lod1);
}

TEST_CASE("LoadCookedMesh rejects a LOD index out of range") {
    ScopedTempFile file("cooked_mesh_lod_out_of_range.tmp.mesh");
    REQUIRE(WriteCookedMesh(file.m_path, GenerateAssetGuid(), MeshData{}));

    MeshData loaded;
    CHECK_FALSE(LoadCookedMesh(file.m_path, loaded, nullptr, 1)); // WriteCookedMesh only wrote LOD0.
}
