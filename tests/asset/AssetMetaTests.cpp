#include "engine/asset/AssetMeta.h"

#include <doctest/doctest.h>

#include <cstdio>
#include <fstream>

using engine::asset::AssetGuid;
using engine::asset::GenerateAssetGuid;
using engine::asset::ToString;
using engine::asset::TryReadAssetMetaGuid;

namespace {

// Same pattern as CookedMeshTests.cpp/CookedTextureTests.cpp's identical helper -- each
// TEST_CASE gets its own file name so cases stay order-independent.
struct ScopedTempFile {
    explicit ScopedTempFile(const char* path, const char* contents) : m_path(path) {
        std::ofstream out(path);
        out << contents;
    }
    ~ScopedTempFile() { std::remove(m_path); }
    const char* m_path;
};

} // namespace

TEST_CASE("TryReadAssetMetaGuid reads a well-formed .meta sidecar") {
    const AssetGuid expected = GenerateAssetGuid();
    const std::string metaContents = "{\"guid\": \"" + ToString(expected) + "\"}";
    ScopedTempFile file("asset_meta_source.tmp.glb.meta", metaContents.c_str());

    AssetGuid actual;
    REQUIRE(TryReadAssetMetaGuid("asset_meta_source.tmp.glb", actual));
    CHECK(actual == expected);
}

TEST_CASE("TryReadAssetMetaGuid fails when no sidecar exists") {
    AssetGuid guid;
    CHECK_FALSE(TryReadAssetMetaGuid("this_source_has_no_sidecar.glb", guid));
}

TEST_CASE("TryReadAssetMetaGuid fails on malformed JSON") {
    ScopedTempFile file("asset_meta_garbage.tmp.glb.meta", "{ not valid json");
    AssetGuid guid;
    CHECK_FALSE(TryReadAssetMetaGuid("asset_meta_garbage.tmp.glb", guid));
}

TEST_CASE("TryReadAssetMetaGuid fails when \"guid\" is missing or not a valid GUID string") {
    ScopedTempFile noGuidFile("asset_meta_no_guid.tmp.glb.meta", "{\"foo\": \"bar\"}");
    AssetGuid guid;
    CHECK_FALSE(TryReadAssetMetaGuid("asset_meta_no_guid.tmp.glb", guid));

    ScopedTempFile badGuidFile("asset_meta_bad_guid.tmp.glb.meta", "{\"guid\": \"not-hex\"}");
    CHECK_FALSE(TryReadAssetMetaGuid("asset_meta_bad_guid.tmp.glb", guid));
}
