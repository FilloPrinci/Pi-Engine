#include "engine/renderer/CookedTexture.h"

#include <doctest/doctest.h>

#include <cstdio>

using engine::asset::AssetGuid;
using engine::asset::GenerateAssetGuid;
using engine::renderer::LoadCookedTexture;
using engine::renderer::TextureData;
using engine::renderer::WriteCookedTexture;

namespace {

// Same reasoning as CookedMeshTests.cpp's identical helper -- each TEST_CASE gets its own
// file name so cases stay order-independent.
struct ScopedTempFile {
    explicit ScopedTempFile(const char* path) : m_path(path) {}
    ~ScopedTempFile() { std::remove(m_path); }
    const char* m_path;
};

} // namespace

TEST_CASE("WriteCookedTexture/LoadCookedTexture round-trip preserves pixels and guid") {
    ScopedTempFile file("cooked_texture_roundtrip.tmp.tex");

    TextureData original;
    original.width = 2;
    original.height = 2;
    original.pixels = {
        255, 0, 0, 255,   0, 255, 0, 255, //
        0, 0, 255, 255,   255, 255, 255, 255,
    };
    const AssetGuid guid = GenerateAssetGuid();

    REQUIRE(WriteCookedTexture(file.m_path, guid, original));

    TextureData loaded;
    AssetGuid loadedGuid;
    REQUIRE(LoadCookedTexture(file.m_path, loaded, &loadedGuid));

    CHECK(loadedGuid == guid);
    CHECK(loaded.width == original.width);
    CHECK(loaded.height == original.height);
    CHECK(loaded.pixels == original.pixels);
}

TEST_CASE("LoadCookedTexture's outGuid parameter is optional") {
    ScopedTempFile file("cooked_texture_no_guid_out.tmp.tex");
    TextureData empty;
    empty.width = 0;
    empty.height = 0;
    REQUIRE(WriteCookedTexture(file.m_path, GenerateAssetGuid(), empty));

    TextureData loaded;
    CHECK(LoadCookedTexture(file.m_path, loaded)); // no third argument -- must still compile/work
}

TEST_CASE("WriteCookedTexture/LoadCookedTexture round-trip handles a zero-sized texture") {
    ScopedTempFile file("cooked_texture_empty.tmp.tex");

    TextureData empty;
    REQUIRE(WriteCookedTexture(file.m_path, GenerateAssetGuid(), empty));

    TextureData loaded;
    REQUIRE(LoadCookedTexture(file.m_path, loaded));
    CHECK(loaded.width == 0);
    CHECK(loaded.height == 0);
    CHECK(loaded.pixels.empty());
}

TEST_CASE("WriteCookedTexture rejects a pixel buffer that doesn't match width*height*4") {
    ScopedTempFile file("cooked_texture_mismatched.tmp.tex");

    TextureData mismatched;
    mismatched.width = 4;
    mismatched.height = 4;
    mismatched.pixels = {1, 2, 3, 4}; // Way short of 4*4*4 = 64 bytes.

    CHECK_FALSE(WriteCookedTexture(file.m_path, GenerateAssetGuid(), mismatched));
}

TEST_CASE("LoadCookedTexture rejects a file that isn't a cooked texture") {
    ScopedTempFile file("cooked_texture_garbage.tmp.tex");

    std::FILE* garbage = std::fopen(file.m_path, "wb");
    REQUIRE(garbage != nullptr);
    const char bytes[] = "not a cooked texture";
    std::fwrite(bytes, 1, sizeof(bytes), garbage);
    std::fclose(garbage);

    TextureData loaded;
    CHECK_FALSE(LoadCookedTexture(file.m_path, loaded));
}

TEST_CASE("LoadCookedTexture rejects a missing file") {
    TextureData loaded;
    CHECK_FALSE(LoadCookedTexture("this_file_does_not_exist.tex", loaded));
}
