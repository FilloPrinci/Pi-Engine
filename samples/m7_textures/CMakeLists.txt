# Uses the new textured-unlit pipeline (shaders/m7_textured.*) -- a separate concrete
# pipeline class from ForwardLitPipeline (CLAUDE.md rule 7), not a variant of it.

add_executable(m7_textures main.cpp)
target_link_libraries(m7_textures PRIVATE engine_core)
pi_engine_set_warnings(m7_textures)
# shaders/m7_textured.{vert,frag}.spv, assets_cooked/m7_quad.mesh, and
# assets_cooked/m7_checker.tex (docs/01 section 12.4, cmake/CookAssets.cmake) -- see that
# file's comment for what happens to these dependencies while cross-compiling.
if(TARGET cooked_shaders)
    add_dependencies(m7_textures cooked_shaders)
endif()
if(TARGET cooked_assets)
    add_dependencies(m7_textures cooked_assets)
endif()
if(TARGET cooked_textures)
    add_dependencies(m7_textures cooked_textures)
endif()

target_compile_definitions(m7_textures PRIVATE
    PI_ENGINE_SHADER_DIR="${PI_ENGINE_COOKED_SHADER_DIR}"
    PI_ENGINE_COOKED_ASSET_DIR="${PI_ENGINE_COOKED_ASSET_DIR}"
)
