# GLSL -> SPIR-V at build time (docs/03 section 5), not the Cooker yet. Requires the
# Vulkan SDK's glslc on PATH or discoverable via VULKAN_SDK (see root README.md).
find_program(GLSLC_EXECUTABLE
    NAMES glslc
    HINTS "$ENV{VULKAN_SDK}/bin" "$ENV{VULKAN_SDK}/Bin" "$ENV{VULKAN_SDK}/Bin32"
)
if(NOT GLSLC_EXECUTABLE)
    message(FATAL_ERROR
        "m0_hello_vulkan: glslc not found. Install the Vulkan SDK (https://vulkan.lunarg.com/) "
        "and make sure glslc is on PATH, or set the VULKAN_SDK environment variable.")
endif()

set(M0_SHADER_SOURCE_DIR "${CMAKE_SOURCE_DIR}/shaders")
set(M0_SHADER_OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/shaders")
set(M0_SHADER_SOURCES
    "${M0_SHADER_SOURCE_DIR}/m0_triangle.vert"
    "${M0_SHADER_SOURCE_DIR}/m0_triangle.frag"
)

set(M0_SHADER_OUTPUTS "")
foreach(shader_source ${M0_SHADER_SOURCES})
    get_filename_component(shader_name "${shader_source}" NAME)
    set(shader_output "${M0_SHADER_OUTPUT_DIR}/${shader_name}.spv")
    add_custom_command(
        OUTPUT "${shader_output}"
        COMMAND "${CMAKE_COMMAND}" -E make_directory "${M0_SHADER_OUTPUT_DIR}"
        COMMAND "${GLSLC_EXECUTABLE}" "${shader_source}" -o "${shader_output}"
        DEPENDS "${shader_source}"
        COMMENT "Compiling ${shader_name} to SPIR-V"
        VERBATIM
    )
    list(APPEND M0_SHADER_OUTPUTS "${shader_output}")
endforeach()

add_custom_target(m0_hello_vulkan_shaders DEPENDS ${M0_SHADER_OUTPUTS})

add_executable(m0_hello_vulkan main.cpp)
target_link_libraries(m0_hello_vulkan PRIVATE engine_core)
pi_engine_set_warnings(m0_hello_vulkan)
add_dependencies(m0_hello_vulkan m0_hello_vulkan_shaders)

# Read back by main.cpp (ShaderPath()) so the executable finds its shaders regardless of
# the process' current working directory.
target_compile_definitions(m0_hello_vulkan PRIVATE PI_ENGINE_SHADER_DIR="${M0_SHADER_OUTPUT_DIR}")
