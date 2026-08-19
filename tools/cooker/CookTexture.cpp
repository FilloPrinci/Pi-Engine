#include "CookTexture.h"

#include "AssetImporter.h"
#include "engine/renderer/CookedTexture.h"

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include <cstdio>
#include <cstdlib>

int CookTexture(int argc, char** argv) {
    if (argc != 3) {
        std::fprintf(stderr, "usage: cooker texture <input.png> <output.tex>\n");
        return EXIT_FAILURE;
    }

    const char* inputPath = argv[1];
    const char* outputPath = argv[2];

    const engine::asset::AssetGuid guid = GetOrCreateAssetGuid(inputPath);

    int width = 0;
    int height = 0;
    int sourceChannels = 0;
    // Force 4 channels regardless of the source (grayscale/RGB/RGBA all end up RGBA8) --
    // CookedTexture.h's format is always tightly packed RGBA8, no per-texture channel
    // count to track at runtime.
    unsigned char* pixels = stbi_load(inputPath, &width, &height, &sourceChannels, 4);
    if (pixels == nullptr) {
        std::fprintf(stderr, "cooker: failed to load \"%s\" (%s)\n", inputPath,
                     stbi_failure_reason());
        return EXIT_FAILURE;
    }

    engine::renderer::TextureData texture;
    texture.width = static_cast<std::uint32_t>(width);
    texture.height = static_cast<std::uint32_t>(height);
    const std::size_t byteCount = static_cast<std::size_t>(width) * height * 4;
    texture.pixels.assign(pixels, pixels + byteCount);
    stbi_image_free(pixels);

    if (!engine::renderer::WriteCookedTexture(outputPath, guid, texture)) {
        std::fprintf(stderr, "cooker: failed to write \"%s\"\n", outputPath);
        return EXIT_FAILURE;
    }

    std::printf("cooker: \"%s\" -> \"%s\" (guid %s, %dx%d, %d source channels)\n", inputPath,
                outputPath, engine::asset::ToString(guid).c_str(), width, height, sourceChannels);
    return EXIT_SUCCESS;
}
