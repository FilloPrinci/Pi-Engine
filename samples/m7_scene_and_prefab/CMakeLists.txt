# Reuses M1's unlit shader (shaders/m1_unlit.*) -- this sample doesn't need a new pipeline
# variant.

# engine_core links Jolt::Jolt PRIVATE (engine/CMakeLists.txt) -- this sample calls into
# physics/PhysicsWorld directly (like m4_hello_physics/m5_vertical_slice), so it needs its
# own find_package/link to pull libJolt.a's object code into its own final link.
find_package(Jolt CONFIG REQUIRED)

add_executable(m7_scene_and_prefab main.cpp)
target_link_libraries(m7_scene_and_prefab PRIVATE engine_core Jolt::Jolt)
pi_engine_set_warnings(m7_scene_and_prefab)
# shaders/m1_unlit.{vert,frag}.spv and assets_cooked/m1_cube.mesh (docs/01 section 12.4,
# cmake/CookAssets.cmake) -- see that file's comment for what happens to these
# dependencies while cross-compiling.
if(TARGET cooked_shaders)
    add_dependencies(m7_scene_and_prefab cooked_shaders)
endif()
if(TARGET cooked_assets)
    add_dependencies(m7_scene_and_prefab cooked_assets)
endif()

# Read back by main.cpp (ShaderPath()/CookedAssetPath()/SampleAssetPath()). The scene/
# prefab JSON files live next to this sample (not under the top-level assets/, which is
# only for Cooker *source* assets, docs/01 section 12.1) and are read directly at runtime
# -- unlike meshes, scenes stay "raw" (text) even after M7, see engine/scene/README.md.
target_compile_definitions(m7_scene_and_prefab PRIVATE
    PI_ENGINE_SHADER_DIR="${PI_ENGINE_COOKED_SHADER_DIR}"
    PI_ENGINE_COOKED_ASSET_DIR="${PI_ENGINE_COOKED_ASSET_DIR}"
    PI_ENGINE_SAMPLE_ASSET_DIR="${CMAKE_CURRENT_SOURCE_DIR}/assets"
)
