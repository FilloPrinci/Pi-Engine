# Pi-Engine Editor (docs/01 section 6, docs/06-editor-roadmap.md) -- "a separate
# application, client of the Engine Core (same RHI/Vulkan as the final game)". Unlike
# tools/cooker (build-time-only tooling, deliberately NOT linked to engine_core), this
# links engine_core fully: it needs the real ECS/RHI/renderer/scene loader to show a Scene
# View, not just offline conversion code.

add_executable(editor main.cpp ProjectHub.cpp)
target_link_libraries(editor PRIVATE engine_core)
pi_engine_set_warnings(editor)

# shaders/m1_unlit.{vert,frag}.spv and assets_cooked/m1_cube.mesh (docs/01 section 12.4,
# cmake/CookAssets.cmake) -- see that file's comment for what happens to these
# dependencies while cross-compiling.
if(TARGET cooked_shaders)
    add_dependencies(editor cooked_shaders)
endif()
if(TARGET cooked_assets)
    add_dependencies(editor cooked_assets)
endif()

# Read back by main.cpp (ShaderPath()/CookedAssetPath()/EditorAssetPath()). editor/assets/
# holds the Editor's own default demo scene -- not a Cooker *source* asset (assets/,
# docs/01 section 12.1) and not one sample's throwaway content either, so it gets its own
# directory rather than reusing either of those. PI_ENGINE_ASSETS_DIR (Editor step E6's
# Asset Browser) is the actual Cooker source directory (docs/01 section 12.1), distinct
# from PI_ENGINE_EDITOR_ASSET_DIR above.
target_compile_definitions(editor PRIVATE
    PI_ENGINE_SHADER_DIR="${PI_ENGINE_COOKED_SHADER_DIR}"
    PI_ENGINE_COOKED_ASSET_DIR="${PI_ENGINE_COOKED_ASSET_DIR}"
    PI_ENGINE_EDITOR_ASSET_DIR="${CMAKE_CURRENT_SOURCE_DIR}/assets"
    PI_ENGINE_ASSETS_DIR="${CMAKE_SOURCE_DIR}/assets"
)
