#include "engine/asset/AssetGuid.h"

#include <doctest/doctest.h>

#include <unordered_set>

using engine::asset::AssetGuid;
using engine::asset::GenerateAssetGuid;
using engine::asset::kInvalidAssetGuid;
using engine::asset::ToString;
using engine::asset::TryParseAssetGuid;

TEST_CASE("A default-constructed AssetGuid is invalid") {
    CHECK_FALSE(AssetGuid{}.IsValid());
    CHECK_FALSE(kInvalidAssetGuid.IsValid());
    CHECK(AssetGuid{} == kInvalidAssetGuid);
}

TEST_CASE("GenerateAssetGuid produces distinct, valid ids") {
    std::unordered_set<AssetGuid> seen;
    for (int i = 0; i < 1000; ++i) {
        const AssetGuid guid = GenerateAssetGuid();
        CHECK(guid.IsValid());
        CHECK(seen.insert(guid).second); // true -> wasn't already in the set
    }
}

TEST_CASE("ToString/TryParseAssetGuid round-trip") {
    const AssetGuid original = GenerateAssetGuid();
    const std::string text = ToString(original);
    CHECK(text.size() == 32);

    AssetGuid parsed;
    REQUIRE(TryParseAssetGuid(text, parsed));
    CHECK(parsed == original);
}

TEST_CASE("TryParseAssetGuid rejects malformed input") {
    AssetGuid guid;
    CHECK_FALSE(TryParseAssetGuid("", guid));
    CHECK_FALSE(TryParseAssetGuid("too-short", guid));
    CHECK_FALSE(TryParseAssetGuid("not-hex-at-all-but-thirty-two-chars!!", guid));
    // 32 chars but with a non-hex character in the middle.
    CHECK_FALSE(TryParseAssetGuid("0123456789abcdefg123456789abcdef", guid));
}
