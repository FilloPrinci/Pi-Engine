# Reuses M1's unlit shader/pipeline (shaders/m1_unlit.*, ForwardLitPipeline) -- this step
# is about geometry (LOD generation), not a new pipeline variant.

add_executable(m7_lod main.cpp)
target_link_libraries(m7_lod PRIVATE engine_core)
pi_engine_set_warnings(m7_lod)
# shaders/m1_unlit.{vert,frag}.spv and assets_cooked/m7_lod_sphere.mesh (docs/01 section
# 12.4, cmake/CookAssets.cmake) -- see that file's comment for what happens to these
# dependencies while cross-compiling.
if(TARGET cooked_shaders)
    add_dependencies(m7_lod cooked_shaders)
endif()
if(TARGET cooked_assets)
    add_dependencies(m7_lod cooked_assets)
endif()

target_compile_definitions(m7_lod PRIVATE
    PI_ENGINE_SHADER_DIR="${PI_ENGINE_COOKED_SHADER_DIR}"
    PI_ENGINE_COOKED_ASSET_DIR="${PI_ENGINE_COOKED_ASSET_DIR}"
)
