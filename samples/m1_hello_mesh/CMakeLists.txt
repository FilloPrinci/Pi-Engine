add_executable(m1_hello_mesh main.cpp)
target_link_libraries(m1_hello_mesh PRIVATE engine_core)
pi_engine_set_warnings(m1_hello_mesh)
# shaders/m1_unlit.{vert,frag}.spv and assets_cooked/m1_cube.mesh (docs/01 section 12.4,
# cmake/CookAssets.cmake) -- see that file's comment for what happens to these
# dependencies while cross-compiling.
if(TARGET cooked_shaders)
    add_dependencies(m1_hello_mesh cooked_shaders)
endif()
if(TARGET cooked_assets)
    add_dependencies(m1_hello_mesh cooked_assets)
endif()

# Read back by main.cpp (ShaderPath()/CookedAssetPath()) so the executable finds its
# shaders and cooked mesh regardless of the process' current working directory.
target_compile_definitions(m1_hello_mesh PRIVATE
    PI_ENGINE_SHADER_DIR="${PI_ENGINE_COOKED_SHADER_DIR}"
    PI_ENGINE_COOKED_ASSET_DIR="${PI_ENGINE_COOKED_ASSET_DIR}"
)
