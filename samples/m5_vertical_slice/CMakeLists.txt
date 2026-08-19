# GLSL -> SPIR-V at build time (docs/03 section 5). Requires the Vulkan SDK's glslc on
# PATH or discoverable via VULKAN_SDK (see root README.md).
# Reuses M1's unlit shader (shaders/m1_unlit.*) -- M5 doesn't need a new pipeline variant.
find_program(GLSLC_EXECUTABLE
    NAMES glslc
    HINTS "$ENV{VULKAN_SDK}/bin" "$ENV{VULKAN_SDK}/Bin" "$ENV{VULKAN_SDK}/Bin32"
)
if(NOT GLSLC_EXECUTABLE)
    message(FATAL_ERROR
        "m5_vertical_slice: glslc not found. Install the Vulkan SDK (https://vulkan.lunarg.com/) "
        "and make sure glslc is on PATH, or set the VULKAN_SDK environment variable.")
endif()

set(M5_SHADER_SOURCE_DIR "${CMAKE_SOURCE_DIR}/shaders")
set(M5_SHADER_OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/shaders")
set(M5_SHADER_SOURCES
    "${M5_SHADER_SOURCE_DIR}/m1_unlit.vert"
    "${M5_SHADER_SOURCE_DIR}/m1_unlit.frag"
)

set(M5_SHADER_OUTPUTS "")
foreach(shader_source ${M5_SHADER_SOURCES})
    get_filename_component(shader_name "${shader_source}" NAME)
    set(shader_output "${M5_SHADER_OUTPUT_DIR}/${shader_name}.spv")
    add_custom_command(
        OUTPUT "${shader_output}"
        COMMAND "${CMAKE_COMMAND}" -E make_directory "${M5_SHADER_OUTPUT_DIR}"
        COMMAND "${GLSLC_EXECUTABLE}" "${shader_source}" -o "${shader_output}"
        DEPENDS "${shader_source}"
        COMMENT "Compiling ${shader_name} to SPIR-V"
        VERBATIM
    )
    list(APPEND M5_SHADER_OUTPUTS "${shader_output}")
endforeach()

add_custom_target(m5_vertical_slice_shaders DEPENDS ${M5_SHADER_OUTPUTS})

# engine_core links Jolt::Jolt PRIVATE (engine/CMakeLists.txt) -- this sample calls into
# physics/PhysicsWorld directly (unlike m3_hello_script), so it needs its own find_package/
# link to pull libJolt.a's object code into its own final link (see m4_hello_physics's
# CMakeLists.txt, which needs the exact same thing).
find_package(Jolt CONFIG REQUIRED)

add_executable(m5_vertical_slice main.cpp)
target_link_libraries(m5_vertical_slice PRIVATE engine_core Jolt::Jolt)
pi_engine_set_warnings(m5_vertical_slice)
add_dependencies(m5_vertical_slice m5_vertical_slice_shaders)
# assets_cooked/m1_cube.mesh (docs/01 section 12.4, cmake/CookAssets.cmake) -- see that
# file's comment for what happens to this dependency while cross-compiling.
if(TARGET cooked_assets)
    add_dependencies(m5_vertical_slice cooked_assets)
endif()

target_compile_definitions(m5_vertical_slice PRIVATE
    PI_ENGINE_SHADER_DIR="${M5_SHADER_OUTPUT_DIR}"
    PI_ENGINE_COOKED_ASSET_DIR="${PI_ENGINE_COOKED_ASSET_DIR}"
)
