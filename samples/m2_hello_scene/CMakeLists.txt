# Reuses M1's unlit shader (shaders/m1_unlit.*) -- M2 doesn't need a new pipeline variant.
add_executable(m2_hello_scene main.cpp)
target_link_libraries(m2_hello_scene PRIVATE engine_core)
pi_engine_set_warnings(m2_hello_scene)
# shaders/m1_unlit.{vert,frag}.spv and assets_cooked/m1_cube.mesh (docs/01 section 12.4,
# cmake/CookAssets.cmake) -- see that file's comment for what happens to these
# dependencies while cross-compiling.
if(TARGET cooked_shaders)
    add_dependencies(m2_hello_scene cooked_shaders)
endif()
if(TARGET cooked_assets)
    add_dependencies(m2_hello_scene cooked_assets)
endif()

target_compile_definitions(m2_hello_scene PRIVATE
    PI_ENGINE_SHADER_DIR="${PI_ENGINE_COOKED_SHADER_DIR}"
    PI_ENGINE_COOKED_ASSET_DIR="${PI_ENGINE_COOKED_ASSET_DIR}"
)
