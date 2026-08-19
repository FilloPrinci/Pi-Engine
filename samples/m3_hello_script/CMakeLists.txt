# Reuses M1's unlit shader (shaders/m1_unlit.*) -- M3 doesn't need a new pipeline variant.
add_executable(m3_hello_script main.cpp)
target_link_libraries(m3_hello_script PRIVATE engine_core)
pi_engine_set_warnings(m3_hello_script)
# shaders/m1_unlit.{vert,frag}.spv and assets_cooked/m1_cube.mesh (docs/01 section 12.4,
# cmake/CookAssets.cmake) -- see that file's comment for what happens to these
# dependencies while cross-compiling.
if(TARGET cooked_shaders)
    add_dependencies(m3_hello_script cooked_shaders)
endif()
if(TARGET cooked_assets)
    add_dependencies(m3_hello_script cooked_assets)
endif()

target_compile_definitions(m3_hello_script PRIVATE
    PI_ENGINE_SHADER_DIR="${PI_ENGINE_COOKED_SHADER_DIR}"
    PI_ENGINE_COOKED_ASSET_DIR="${PI_ENGINE_COOKED_ASSET_DIR}"
)
