# Offline Asset Cooker (docs/01 section 12.4, docs/02 section 6's "post-vertical-slice
# milestone"). Deliberately does NOT link engine_core: it compiles the engine sources it
# actually needs (MeshLoader.cpp for cgltf parsing, CookedMesh.cpp/CookedTexture.cpp for
# the binary writers, AssetGuid.cpp for the persistent asset id) directly as its own
# sources, so the tool's link graph is just cgltf + stb_image + glm + nlohmann-json +
# shaderc + meshoptimizer -- none of engine_core's Vulkan/SDL2/Jolt dependencies, matching
# the design doc's "the heavy conversion libraries never end up in the shipped game" (the
# Cooker isn't shipped either, but this also means it builds and runs without ever needing
# a display/GPU).
find_package(glm CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED) # AssetImporter.cpp's .meta sidecar (M7)
find_package(unofficial-shaderc CONFIG REQUIRED) # CookShader.cpp's GLSL->SPIR-V compile (M7)
find_package(meshoptimizer CONFIG REQUIRED) # CookMesh.cpp's LOD generation (M7)

add_executable(cooker
    AssetImporter.cpp
    CookMesh.cpp
    CookShader.cpp
    CookTexture.cpp
    main.cpp
    "${CMAKE_SOURCE_DIR}/engine/src/asset/AssetGuid.cpp"
    "${CMAKE_SOURCE_DIR}/engine/src/renderer/CookedMesh.cpp"
    "${CMAKE_SOURCE_DIR}/engine/src/renderer/CookedTexture.cpp"
    "${CMAKE_SOURCE_DIR}/engine/src/renderer/MeshLoader.cpp"
)
target_include_directories(cooker PRIVATE
    "${CMAKE_SOURCE_DIR}/engine/include"
    "${CMAKE_SOURCE_DIR}/third_party" # cgltf.h/stb_image.h, vendored (see MeshLoader.cpp/CookTexture.cpp)
)
target_link_libraries(cooker PRIVATE glm::glm nlohmann_json::nlohmann_json unofficial::shaderc::shaderc meshoptimizer::meshoptimizer)
target_compile_features(cooker PRIVATE cxx_std_20)
pi_engine_set_warnings(cooker)

# cgltf.h (vendored third-party, third_party/README.md) triggers this project's strict
# warning flags on its own C-style code -- not ours to fix. Same suppression
# engine/CMakeLists.txt applies to this same file when it's compiled into engine_core;
# set_source_files_properties is directory-scoped, so it has to be repeated here.
if(NOT MSVC)
    set_source_files_properties("${CMAKE_SOURCE_DIR}/engine/src/renderer/MeshLoader.cpp" PROPERTIES
        COMPILE_OPTIONS "-Wno-old-style-cast;-Wno-conversion;-Wno-sign-conversion;-Wno-cast-align;-Wno-double-promotion"
    )
    # stb_image.h (vendored third-party) triggers the same class of warnings on its own
    # C-style code -- CookTexture.cpp is the only source that includes it with
    # STB_IMAGE_IMPLEMENTATION defined, so only that translation unit needs the suppression.
    set_source_files_properties(CookTexture.cpp PROPERTIES
        COMPILE_OPTIONS "-Wno-old-style-cast;-Wno-conversion;-Wno-sign-conversion;-Wno-cast-align;-Wno-double-promotion;-Wno-unused-function"
    )
endif()
