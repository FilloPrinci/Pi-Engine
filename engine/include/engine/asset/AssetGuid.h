#pragma once

#include <cstdint>
#include <string>

namespace engine::asset {

// Stable identifier for a source asset (docs/01 section 12.3): "every source asset
// receives a stable GUID on first import... assets referenced by GUID, not by file path --
// renaming/moving a folder breaks nothing." 128 bits (two uint64_t halves), generated with
// std::random_device-seeded randomness -- not a cryptographically strong or RFC 4122
// UUID, doesn't need to be (collision probability is astronomically low at this project's
// asset-count scale, same as e.g. Unity's own asset GUIDs).
struct AssetGuid {
    std::uint64_t high = 0;
    std::uint64_t low = 0;

    bool IsValid() const { return high != 0 || low != 0; }

    friend bool operator==(const AssetGuid& a, const AssetGuid& b) {
        return a.high == b.high && a.low == b.low;
    }
    friend bool operator!=(const AssetGuid& a, const AssetGuid& b) { return !(a == b); }
};

inline constexpr AssetGuid kInvalidAssetGuid{};

// Random 128-bit id (see the struct's own comment for why not a "real" UUID). Called by
// tools/cooker the first time it sees a source asset with no sidecar .meta file yet --
// never at runtime, the engine only ever reads GUIDs back out of already-cooked output.
AssetGuid GenerateAssetGuid();

// 32 lowercase hex digits, no dashes ("a1b2c3d4e5f6a1b2c3d4e5f6a1b2c3d4") -- simple to
// write/parse, no RFC 4122 formatting rules to get right for no benefit here.
std::string ToString(const AssetGuid& guid);
bool TryParseAssetGuid(const std::string& text, AssetGuid& outGuid);

} // namespace engine::asset

namespace std {
template <>
struct hash<engine::asset::AssetGuid> {
    std::size_t operator()(const engine::asset::AssetGuid& guid) const noexcept {
        // Simple, sufficient mix -- the two halves are already independently random, this
        // just needs to spread them across a std::size_t (which may be 32-bit).
        const std::uint64_t combined = guid.high ^ (guid.low + 0x9e3779b97f4a7c15ULL +
                                                     (guid.high << 6) + (guid.high >> 2));
        return static_cast<std::size_t>(combined);
    }
};
} // namespace std
