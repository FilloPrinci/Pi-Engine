add_executable(m0_hello_vulkan main.cpp)
target_link_libraries(m0_hello_vulkan PRIVATE engine_core)
pi_engine_set_warnings(m0_hello_vulkan)
# shaders/m0_triangle.{vert,frag}.spv (docs/01 section 12.4, cmake/CookAssets.cmake) --
# see that file's comment for what happens to this dependency while cross-compiling.
if(TARGET cooked_shaders)
    add_dependencies(m0_hello_vulkan cooked_shaders)
endif()

# Read back by main.cpp (ShaderPath()) so the executable finds its shaders regardless of
# the process' current working directory.
target_compile_definitions(m0_hello_vulkan PRIVATE PI_ENGINE_SHADER_DIR="${PI_ENGINE_COOKED_SHADER_DIR}")
