# Reuses M1's cube mesh/unlit pipeline verbatim -- this sample is about proving
# engine::debug::ImGuiOverlay's lifecycle (Editor step E1), not new geometry/shading.

add_executable(e1_imgui_overlay main.cpp)
target_link_libraries(e1_imgui_overlay PRIVATE engine_core)
pi_engine_set_warnings(e1_imgui_overlay)
# shaders/m1_unlit.{vert,frag}.spv and assets_cooked/m1_cube.mesh (docs/01 section 12.4,
# cmake/CookAssets.cmake) -- see that file's comment for what happens to these
# dependencies while cross-compiling.
if(TARGET cooked_shaders)
    add_dependencies(e1_imgui_overlay cooked_shaders)
endif()
if(TARGET cooked_assets)
    add_dependencies(e1_imgui_overlay cooked_assets)
endif()

target_compile_definitions(e1_imgui_overlay PRIVATE
    PI_ENGINE_SHADER_DIR="${PI_ENGINE_COOKED_SHADER_DIR}"
    PI_ENGINE_COOKED_ASSET_DIR="${PI_ENGINE_COOKED_ASSET_DIR}"
)
