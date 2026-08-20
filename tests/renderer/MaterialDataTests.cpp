#include "engine/renderer/MaterialData.h"

#include <doctest/doctest.h>

#include <cstdio>

using engine::renderer::LoadMaterial;
using engine::renderer::MaterialData;
using engine::renderer::WriteMaterial;

namespace {

// Same reasoning as CookedTextureTests.cpp's identical helper -- each TEST_CASE gets its
// own file name so cases stay order-independent.
struct ScopedTempFile {
    explicit ScopedTempFile(const char* path) : m_path(path) {}
    ~ScopedTempFile() { std::remove(m_path); }
    const char* m_path;
};

} // namespace

TEST_CASE("WriteMaterial/LoadMaterial round-trip preserves tintColor") {
    ScopedTempFile file("material_roundtrip.tmp.material.json");

    MaterialData original;
    original.tintColor = glm::vec4(0.85f, 0.1f, 0.1f, 1.0f);

    REQUIRE(WriteMaterial(file.m_path, original));

    MaterialData loaded;
    REQUIRE(LoadMaterial(file.m_path, loaded));
    CHECK(loaded.tintColor == original.tintColor);
}

TEST_CASE("MaterialData defaults to opaque white") {
    MaterialData material;
    CHECK(material.tintColor == glm::vec4(1.0f, 1.0f, 1.0f, 1.0f));
}

TEST_CASE("LoadMaterial leaves outMaterial at its default when tintColor is missing") {
    ScopedTempFile file("material_no_tint.tmp.material.json");
    std::FILE* f = std::fopen(file.m_path, "wb");
    REQUIRE(f != nullptr);
    const char bytes[] = "{}";
    std::fwrite(bytes, 1, sizeof(bytes) - 1, f);
    std::fclose(f);

    MaterialData loaded;
    REQUIRE(LoadMaterial(file.m_path, loaded));
    CHECK(loaded.tintColor == glm::vec4(1.0f, 1.0f, 1.0f, 1.0f));
}

TEST_CASE("LoadMaterial rejects malformed JSON") {
    ScopedTempFile file("material_garbage.tmp.material.json");
    std::FILE* f = std::fopen(file.m_path, "wb");
    REQUIRE(f != nullptr);
    const char bytes[] = "not json at all {{{";
    std::fwrite(bytes, 1, sizeof(bytes) - 1, f);
    std::fclose(f);

    MaterialData loaded;
    CHECK_FALSE(LoadMaterial(file.m_path, loaded));
}

TEST_CASE("LoadMaterial rejects a missing file") {
    MaterialData loaded;
    CHECK_FALSE(LoadMaterial("this_file_does_not_exist.material.json", loaded));
}
