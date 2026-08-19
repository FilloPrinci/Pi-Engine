#include "CookShader.h"

#include <shaderc/shaderc.hpp>

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace {

bool ReadTextFile(const std::string& path, std::string& outSource) {
    std::ifstream in(path);
    if (!in.is_open()) {
        return false;
    }
    std::ostringstream contents;
    contents << in.rdbuf();
    outSource = contents.str();
    return true;
}

// Same convention glslc itself uses -- shaderc has no equivalent built-in filename
// sniffing (it expects the caller to know the shader stage), so this mirrors what the
// project's per-sample CMakeLists.txt files always assumed implicitly via glslc.
shaderc_shader_kind KindFromExtension(const std::string& path) {
    if (path.size() >= 5 && path.compare(path.size() - 5, 5, ".vert") == 0) {
        return shaderc_glsl_vertex_shader;
    }
    if (path.size() >= 5 && path.compare(path.size() - 5, 5, ".frag") == 0) {
        return shaderc_glsl_fragment_shader;
    }
    return shaderc_glsl_infer_from_source;
}

} // namespace

int CookShader(int argc, char** argv) {
    if (argc != 3) {
        std::fprintf(stderr, "usage: cooker shader <input.vert|.frag> <output.spv>\n");
        return EXIT_FAILURE;
    }

    const std::string inputPath = argv[1];
    const std::string outputPath = argv[2];

    std::string source;
    if (!ReadTextFile(inputPath, source)) {
        std::fprintf(stderr, "cooker: failed to read \"%s\"\n", inputPath.c_str());
        return EXIT_FAILURE;
    }

    shaderc::Compiler compiler;
    shaderc::CompileOptions options;
    // Vulkan 1.2 core baseline (CLAUDE.md section 2) -- matches what every sample's own
    // glslc invocation defaulted to through M6 (glslc's own default target environment).
    options.SetTargetEnvironment(shaderc_target_env_vulkan, shaderc_env_version_vulkan_1_2);

    const shaderc::SpvCompilationResult result = compiler.CompileGlslToSpv(
        source, KindFromExtension(inputPath), inputPath.c_str(), options);
    if (result.GetCompilationStatus() != shaderc_compilation_status_success) {
        std::fprintf(stderr, "cooker: failed to compile \"%s\":\n%s\n", inputPath.c_str(),
                     result.GetErrorMessage().c_str());
        return EXIT_FAILURE;
    }

    const std::vector<std::uint32_t> spirv(result.cbegin(), result.cend());
    std::ofstream out(outputPath, std::ios::binary);
    if (!out.is_open()) {
        std::fprintf(stderr, "cooker: failed to open \"%s\" for writing\n", outputPath.c_str());
        return EXIT_FAILURE;
    }
    out.write(reinterpret_cast<const char*>(spirv.data()),
              static_cast<std::streamsize>(spirv.size() * sizeof(std::uint32_t)));
    if (!out) {
        std::fprintf(stderr, "cooker: failed while writing \"%s\"\n", outputPath.c_str());
        return EXIT_FAILURE;
    }

    std::printf("cooker: \"%s\" -> \"%s\" (%zu SPIR-V words)\n", inputPath.c_str(),
                outputPath.c_str(), spirv.size());
    return EXIT_SUCCESS;
}
