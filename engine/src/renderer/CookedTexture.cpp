#include "engine/renderer/CookedTexture.h"

#include <cstdio>
#include <cstring>

namespace engine::renderer {

bool WriteCookedTexture(const char* path, const asset::AssetGuid& guid, const TextureData& texture) {
    std::FILE* file = std::fopen(path, "wb");
    if (file == nullptr) {
        std::fprintf(stderr, "WriteCookedTexture: failed to open \"%s\" for writing\n", path);
        return false;
    }

    CookedTextureHeader header{};
    std::memcpy(header.magic, kCookedTextureMagic, sizeof(header.magic));
    header.version = kCookedTextureVersion;
    header.guidHigh = guid.high;
    header.guidLow = guid.low;
    header.width = texture.width;
    header.height = texture.height;

    const std::size_t expectedBytes = static_cast<std::size_t>(header.width) * header.height * 4;
    if (texture.pixels.size() != expectedBytes) {
        std::fprintf(stderr,
                      "WriteCookedTexture: pixel buffer size %zu doesn't match %ux%u RGBA8 (%zu)\n",
                      texture.pixels.size(), header.width, header.height, expectedBytes);
        std::fclose(file);
        return false;
    }

    bool ok = std::fwrite(&header, sizeof(header), 1, file) == 1;
    if (ok && expectedBytes > 0) {
        ok = std::fwrite(texture.pixels.data(), 1, expectedBytes, file) == expectedBytes;
    }

    std::fclose(file);
    if (!ok) {
        std::fprintf(stderr, "WriteCookedTexture: failed while writing \"%s\"\n", path);
    }
    return ok;
}

bool LoadCookedTexture(const char* path, TextureData& outTexture, asset::AssetGuid* outGuid) {
    std::FILE* file = std::fopen(path, "rb");
    if (file == nullptr) {
        std::fprintf(stderr, "LoadCookedTexture: failed to open \"%s\"\n", path);
        return false;
    }

    CookedTextureHeader header{};
    if (std::fread(&header, sizeof(header), 1, file) != 1) {
        std::fprintf(stderr, "LoadCookedTexture: \"%s\" is too small for a header\n", path);
        std::fclose(file);
        return false;
    }
    if (std::memcmp(header.magic, kCookedTextureMagic, sizeof(header.magic)) != 0) {
        std::fprintf(stderr, "LoadCookedTexture: \"%s\" is not a cooked texture (bad magic)\n", path);
        std::fclose(file);
        return false;
    }
    if (header.version != kCookedTextureVersion) {
        std::fprintf(stderr, "LoadCookedTexture: \"%s\" has version %u, expected %u\n", path,
                      header.version, kCookedTextureVersion);
        std::fclose(file);
        return false;
    }

    if (outGuid != nullptr) {
        outGuid->high = header.guidHigh;
        outGuid->low = header.guidLow;
    }

    outTexture.width = header.width;
    outTexture.height = header.height;
    const std::size_t expectedBytes = static_cast<std::size_t>(header.width) * header.height * 4;
    outTexture.pixels.resize(expectedBytes);

    bool ok = true;
    if (expectedBytes > 0) {
        ok = std::fread(outTexture.pixels.data(), 1, expectedBytes, file) == expectedBytes;
    }

    std::fclose(file);
    if (!ok) {
        std::fprintf(stderr, "LoadCookedTexture: failed while reading \"%s\" (truncated file?)\n",
                      path);
    }
    return ok;
}

} // namespace engine::renderer
