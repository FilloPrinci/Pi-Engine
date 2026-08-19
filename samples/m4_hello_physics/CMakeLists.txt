# Reuses M1's unlit shader (shaders/m1_unlit.*) -- M4 doesn't need a new pipeline variant.

# engine_core links Jolt::Jolt PRIVATE (engine/CMakeLists.txt), so this sample -- the only
# one that actually calls into physics/PhysicsWorld -- needs its own find_package/link to
# pull libJolt.a's object code into its own final link.
find_package(Jolt CONFIG REQUIRED)

add_executable(m4_hello_physics main.cpp)
target_link_libraries(m4_hello_physics PRIVATE engine_core Jolt::Jolt)
pi_engine_set_warnings(m4_hello_physics)
# shaders/m1_unlit.{vert,frag}.spv and assets_cooked/m1_cube.mesh (docs/01 section 12.4,
# cmake/CookAssets.cmake) -- see that file's comment for what happens to these
# dependencies while cross-compiling.
if(TARGET cooked_shaders)
    add_dependencies(m4_hello_physics cooked_shaders)
endif()
if(TARGET cooked_assets)
    add_dependencies(m4_hello_physics cooked_assets)
endif()

target_compile_definitions(m4_hello_physics PRIVATE
    PI_ENGINE_SHADER_DIR="${PI_ENGINE_COOKED_SHADER_DIR}"
    PI_ENGINE_COOKED_ASSET_DIR="${PI_ENGINE_COOKED_ASSET_DIR}"
)
