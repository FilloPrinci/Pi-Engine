#include "engine/core/EngineVersion.h"

#include <doctest/doctest.h>

#include <cstring>

TEST_CASE("GetEngineVersionString returns a non-empty version string") {
    const char* version = engine::core::GetEngineVersionString();
    REQUIRE(version != nullptr);
    CHECK(std::strlen(version) > 0);
}
