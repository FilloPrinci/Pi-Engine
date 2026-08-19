#include "engine/renderer/CookedMesh.h"

#include <doctest/doctest.h>

#include <cstdio>

using engine::renderer::LoadCookedMesh;
using engine::renderer::MeshData;
using engine::renderer::Vertex;
using engine::renderer::WriteCookedMesh;

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

TEST_CASE("WriteCookedMesh/LoadCookedMesh round-trip preserves vertices and indices") {
    ScopedTempFile file("cooked_mesh_roundtrip.tmp.mesh");

    MeshData original;
    original.vertices = {
        Vertex{glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f)},
        Vertex{glm::vec3(1.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f)},
        Vertex{glm::vec3(0.0f, 1.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f)},
    };
    original.indices = {0, 1, 2};

    REQUIRE(WriteCookedMesh(file.m_path, original));

    MeshData loaded;
    REQUIRE(LoadCookedMesh(file.m_path, loaded));

    REQUIRE(loaded.vertices.size() == original.vertices.size());
    for (std::size_t i = 0; i < original.vertices.size(); ++i) {
        CHECK(loaded.vertices[i].position == original.vertices[i].position);
        CHECK(loaded.vertices[i].normal == original.vertices[i].normal);
    }
    CHECK(loaded.indices == original.indices);
}

TEST_CASE("WriteCookedMesh/LoadCookedMesh round-trip handles an empty mesh") {
    ScopedTempFile file("cooked_mesh_empty.tmp.mesh");

    MeshData empty;
    REQUIRE(WriteCookedMesh(file.m_path, empty));

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
