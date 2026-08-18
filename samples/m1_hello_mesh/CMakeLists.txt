# GLSL -> SPIR-V at build time (docs/03 section 5), not the Cooker yet. Requires the
# Vulkan SDK's glslc on PATH or discoverable via VULKAN_SDK (see root README.md).
find_program(GLSLC_EXECUTABLE
    NAMES glslc
    HINTS "$ENV{VULKAN_SDK}/bin" "$ENV{VULKAN_SDK}/Bin" "$ENV{VULKAN_SDK}/Bin32"
)
if(NOT GLSLC_EXECUTABLE)
    message(FATAL_ERROR
        "m1_hello_mesh: glslc not found. Install the Vulkan SDK (https://vulkan.lunarg.com/) "
        "and make sure glslc is on PATH, or set the VULKAN_SDK environment variable.")
endif()

set(M1_SHADER_SOURCE_DIR "${CMAKE_SOURCE_DIR}/shaders")
set(M1_SHADER_OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/shaders")
set(M1_SHADER_SOURCES
    "${M1_SHADER_SOURCE_DIR}/m1_unlit.vert"
    "${M1_SHADER_SOURCE_DIR}/m1_unlit.frag"
)

set(M1_SHADER_OUTPUTS "")
foreach(shader_source ${M1_SHADER_SOURCES})
    get_filename_component(shader_name "${shader_source}" NAME)
    set(shader_output "${M1_SHADER_OUTPUT_DIR}/${shader_name}.spv")
    add_custom_command(
        OUTPUT "${shader_output}"
        COMMAND "${CMAKE_COMMAND}" -E make_directory "${M1_SHADER_OUTPUT_DIR}"
        COMMAND "${GLSLC_EXECUTABLE}" "${shader_source}" -o "${shader_output}"
        DEPENDS "${shader_source}"
        COMMENT "Compiling ${shader_name} to SPIR-V"
        VERBATIM
    )
    list(APPEND M1_SHADER_OUTPUTS "${shader_output}")
endforeach()

add_custom_target(m1_hello_mesh_shaders DEPENDS ${M1_SHADER_OUTPUTS})

add_executable(m1_hello_mesh main.cpp)
target_link_libraries(m1_hello_mesh PRIVATE engine_core)
pi_engine_set_warnings(m1_hello_mesh)
add_dependencies(m1_hello_mesh m1_hello_mesh_shaders)

# Read back by main.cpp (ShaderPath()/AssetPath()) so the executable finds its shaders
# and test asset regardless of the process' current working directory.
target_compile_definitions(m1_hello_mesh PRIVATE
    PI_ENGINE_SHADER_DIR="${M1_SHADER_OUTPUT_DIR}"
    PI_ENGINE_ASSET_DIR="${CMAKE_SOURCE_DIR}/assets"
)
