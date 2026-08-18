#pragma once

namespace engine::core {

inline constexpr int kEngineVersionMajor = 0;
inline constexpr int kEngineVersionMinor = 1;
inline constexpr int kEngineVersionPatch = 0;

// Returns "MAJOR.MINOR.PATCH". Defined in version.cpp so engine_core has at least one
// translation unit from day one (CLAUDE.md section 6 repo layout).
const char* GetEngineVersionString();

} // namespace engine::core
