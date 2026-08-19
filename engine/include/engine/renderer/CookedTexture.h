#pragma once

#include "engine/asset/AssetGuid.h"

#include <cstdint>
#include <vector>

namespace engine::renderer {

// Decoded, CPU-side texture data: always tightly packed RGBA8 (4 bytes/pixel, no row
// padding, no color-space tag) -- exactly what tools/cooker's `cooker texture` subcommand
// decodes a source PNG into via stb_image (forced to 4 channels) and what
// LoadCookedTexture() hands back at runtime for RHITexture::InitWithData() to upload.
struct TextureData {
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    std::vector<std::uint8_t> pixels; // RGBA8, width * height * 4 bytes.
};

// Minimal binary texture format, mirroring CookedMesh.h's mesh equivalent (docs/01
// section 12.1's Source/Cooked split, extended to textures for M7's Asset Pipeline
// textures step). No mipmaps, no block compression (ETC2/BC7, docs/01 section 12.2) --
// deliberately deferred, same reasoning as CookedMesh's own "deliberately minimal"
// comment. Little-endian only (see CookedMesh.h's identical note -- same target
// platforms).
struct CookedTextureHeader {
    char magic[4]; // "PICT" -- Pi-Engine Cooked Texture
    std::uint32_t version;
    std::uint64_t guidHigh; // engine::asset::AssetGuid, docs/01 section 12.3
    std::uint64_t guidLow;
    std::uint32_t width;
    std::uint32_t height;
};

inline constexpr char kCookedTextureMagic[4] = {'P', 'I', 'C', 'T'};
inline constexpr std::uint32_t kCookedTextureVersion = 1;

// Writes `texture` (tagged with its source asset's `guid`) to `path` in the format above.
// Offline-tooling code (tools/cooker) -- kept here, not duplicated in the tool, so the
// writer and LoadCookedTexture() can never silently drift apart (same reasoning as
// CookedMesh.h's WriteCookedMesh).
bool WriteCookedTexture(const char* path, const asset::AssetGuid& guid, const TextureData& texture);

// Reads a file written by WriteCookedTexture() back into `outTexture`. `outGuid` is
// optional (nullptr default) -- same convention as LoadCookedMesh().
bool LoadCookedTexture(const char* path, TextureData& outTexture,
                        asset::AssetGuid* outGuid = nullptr);

} // namespace engine::renderer
