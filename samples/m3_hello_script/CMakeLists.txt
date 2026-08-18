# GLSL -> SPIR-V at build time (docs/03 section 5), not the Cooker yet. Requires the
# Vulkan SDK's glslc on PATH or discoverable via VULKAN_SDK (see root README.md).
# Reuses M1's unlit shader (shaders/m1_unlit.*) -- M3 doesn't need a new pipeline variant.
find_program(GLSLC_EXECUTABLE
    NAMES glslc
    HINTS "$ENV{VULKAN_SDK}/bin" "$ENV{VULKAN_SDK}/Bin" "$ENV{VULKAN_SDK}/Bin32"
)
if(NOT GLSLC_EXECUTABLE)
    message(FATAL_ERROR
        "m3_hello_script: glslc not found. Install the Vulkan SDK (https://vulkan.lunarg.com/) "
        "and make sure glslc is on PATH, or set the VULKAN_SDK environment variable.")
endif()

set(M3_SHADER_SOURCE_DIR "${CMAKE_SOURCE_DIR}/shaders")
set(M3_SHADER_OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/shaders")
set(M3_SHADER_SOURCES
    "${M3_SHADER_SOURCE_DIR}/m1_unlit.vert"
    "${M3_SHADER_SOURCE_DIR}/m1_unlit.frag"
)

set(M3_SHADER_OUTPUTS "")
foreach(shader_source ${M3_SHADER_SOURCES})
    get_filename_component(shader_name "${shader_source}" NAME)
    set(shader_output "${M3_SHADER_OUTPUT_DIR}/${shader_name}.spv")
    add_custom_command(
        OUTPUT "${shader_output}"
        COMMAND "${CMAKE_COMMAND}" -E make_directory "${M3_SHADER_OUTPUT_DIR}"
        COMMAND "${GLSLC_EXECUTABLE}" "${shader_source}" -o "${shader_output}"
        DEPENDS "${shader_source}"
        COMMENT "Compiling ${shader_name} to SPIR-V"
        VERBATIM
    )
    list(APPEND M3_SHADER_OUTPUTS "${shader_output}")
endforeach()

add_custom_target(m3_hello_script_shaders DEPENDS ${M3_SHADER_OUTPUTS})

add_executable(m3_hello_script main.cpp)
target_link_libraries(m3_hello_script PRIVATE engine_core)
pi_engine_set_warnings(m3_hello_script)
add_dependencies(m3_hello_script m3_hello_script_shaders)

target_compile_definitions(m3_hello_script PRIVATE
    PI_ENGINE_SHADER_DIR="${M3_SHADER_OUTPUT_DIR}"
    PI_ENGINE_ASSET_DIR="${CMAKE_SOURCE_DIR}/assets"
)
