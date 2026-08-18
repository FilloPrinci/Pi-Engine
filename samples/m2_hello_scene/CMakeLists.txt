# GLSL -> SPIR-V at build time (docs/03 section 5), not the Cooker yet. Requires the
# Vulkan SDK's glslc on PATH or discoverable via VULKAN_SDK (see root README.md).
# Reuses M1's unlit shader (shaders/m1_unlit.*) -- M2 doesn't need a new pipeline variant.
find_program(GLSLC_EXECUTABLE
    NAMES glslc
    HINTS "$ENV{VULKAN_SDK}/bin" "$ENV{VULKAN_SDK}/Bin" "$ENV{VULKAN_SDK}/Bin32"
)
if(NOT GLSLC_EXECUTABLE)
    message(FATAL_ERROR
        "m2_hello_scene: glslc not found. Install the Vulkan SDK (https://vulkan.lunarg.com/) "
        "and make sure glslc is on PATH, or set the VULKAN_SDK environment variable.")
endif()

set(M2_SHADER_SOURCE_DIR "${CMAKE_SOURCE_DIR}/shaders")
set(M2_SHADER_OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/shaders")
set(M2_SHADER_SOURCES
    "${M2_SHADER_SOURCE_DIR}/m1_unlit.vert"
    "${M2_SHADER_SOURCE_DIR}/m1_unlit.frag"
)

set(M2_SHADER_OUTPUTS "")
foreach(shader_source ${M2_SHADER_SOURCES})
    get_filename_component(shader_name "${shader_source}" NAME)
    set(shader_output "${M2_SHADER_OUTPUT_DIR}/${shader_name}.spv")
    add_custom_command(
        OUTPUT "${shader_output}"
        COMMAND "${CMAKE_COMMAND}" -E make_directory "${M2_SHADER_OUTPUT_DIR}"
        COMMAND "${GLSLC_EXECUTABLE}" "${shader_source}" -o "${shader_output}"
        DEPENDS "${shader_source}"
        COMMENT "Compiling ${shader_name} to SPIR-V"
        VERBATIM
    )
    list(APPEND M2_SHADER_OUTPUTS "${shader_output}")
endforeach()

add_custom_target(m2_hello_scene_shaders DEPENDS ${M2_SHADER_OUTPUTS})

add_executable(m2_hello_scene main.cpp)
target_link_libraries(m2_hello_scene PRIVATE engine_core)
pi_engine_set_warnings(m2_hello_scene)
add_dependencies(m2_hello_scene m2_hello_scene_shaders)

target_compile_definitions(m2_hello_scene PRIVATE
    PI_ENGINE_SHADER_DIR="${M2_SHADER_OUTPUT_DIR}"
    PI_ENGINE_ASSET_DIR="${CMAKE_SOURCE_DIR}/assets"
)
