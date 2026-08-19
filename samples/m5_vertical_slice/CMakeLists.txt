# Reuses M1's unlit shader (shaders/m1_unlit.*) -- M5 doesn't need a new pipeline variant.

# engine_core links Jolt::Jolt PRIVATE (engine/CMakeLists.txt) -- this sample calls into
# physics/PhysicsWorld directly (unlike m3_hello_script), so it needs its own find_package/
# link to pull libJolt.a's object code into its own final link (see m4_hello_physics's
# CMakeLists.txt, which needs the exact same thing).
find_package(Jolt CONFIG REQUIRED)

add_executable(m5_vertical_slice main.cpp)
target_link_libraries(m5_vertical_slice PRIVATE engine_core Jolt::Jolt)
pi_engine_set_warnings(m5_vertical_slice)
# shaders/m1_unlit.{vert,frag}.spv and assets_cooked/m1_cube.mesh (docs/01 section 12.4,
# cmake/CookAssets.cmake) -- see that file's comment for what happens to these
# dependencies while cross-compiling.
if(TARGET cooked_shaders)
    add_dependencies(m5_vertical_slice cooked_shaders)
endif()
if(TARGET cooked_assets)
    add_dependencies(m5_vertical_slice cooked_assets)
endif()

target_compile_definitions(m5_vertical_slice PRIVATE
    PI_ENGINE_SHADER_DIR="${PI_ENGINE_COOKED_SHADER_DIR}"
    PI_ENGINE_COOKED_ASSET_DIR="${PI_ENGINE_COOKED_ASSET_DIR}"
)
