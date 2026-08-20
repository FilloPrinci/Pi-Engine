# Pi-Engine Editor (docs/01 section 6, docs/06-editor-roadmap.md) -- "a separate
# application, client of the Engine Core (same RHI/Vulkan as the final game)". Unlike
# tools/cooker (build-time-only tooling, deliberately NOT linked to engine_core), this
# links engine_core fully: it needs the real ECS/RHI/renderer/scene loader to show a Scene
# View, not just offline conversion code.

add_executable(editor main.cpp ProjectHub.cpp BuildPipeline.cpp UndoStack.cpp)
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
#
# PI_ENGINE_BUILD_DIR (Editor step E8, BuildPipeline.cpp) -- the build directory Play's
# `cmake --build <dir> --target ...` calls run against. Always this build's own
# CMAKE_BINARY_DIR: Play rebuilds/re-cooks the exact same build tree the running Editor
# binary itself came from, never a different configuration.
target_compile_definitions(editor PRIVATE
    PI_ENGINE_SHADER_DIR="${PI_ENGINE_COOKED_SHADER_DIR}"
    PI_ENGINE_COOKED_ASSET_DIR="${PI_ENGINE_COOKED_ASSET_DIR}"
    PI_ENGINE_EDITOR_ASSET_DIR="${CMAKE_CURRENT_SOURCE_DIR}/assets"
    PI_ENGINE_ASSETS_DIR="${CMAKE_SOURCE_DIR}/assets"
    PI_ENGINE_BUILD_DIR="${CMAKE_BINARY_DIR}"
)

# editor_play (play_main.cpp, Editor step E8) -- Play Mode, a *separate* executable from
# `editor` above rather than the same binary re-invoked with a flag. Reason: this is the
# only file under editor/ that calls into physics/PhysicsWorld.h, which pulls in Jolt --
# engine_core links Jolt::Jolt PRIVATE only inside its own isolated engine_physics_jolt
# OBJECT library (engine/CMakeLists.txt) specifically so Jolt's INTERFACE_COMPILE_OPTIONS
# (vcpkg's port bakes in `-mavx2 -mbmi -mfma -mf16c -mlzcnt`) don't leak onto every one of
# engine_core's own source files; linking Jolt::Jolt directly onto `editor` itself would
# reintroduce exactly that leak, this time onto main.cpp/ProjectHub.cpp/BuildPipeline.cpp,
# none of which touch physics at all ([[vcpkg-dependency-isa-flag-isolation]]). A separate
# executable keeps that leak contained to the one target that actually needs it, matching
# m4_hello_physics/m5_vertical_slice's own precedent (their whole main.cpp is physics
# content, so there's nothing to isolate it from in the first place).
find_package(Jolt CONFIG REQUIRED)

add_executable(editor_play play_main.cpp)
target_link_libraries(editor_play PRIVATE engine_core Jolt::Jolt)
pi_engine_set_warnings(editor_play)

if(TARGET cooked_shaders)
    add_dependencies(editor_play cooked_shaders)
endif()
if(TARGET cooked_assets)
    add_dependencies(editor_play cooked_assets)
endif()

target_compile_definitions(editor_play PRIVATE
    PI_ENGINE_SHADER_DIR="${PI_ENGINE_COOKED_SHADER_DIR}"
    PI_ENGINE_COOKED_ASSET_DIR="${PI_ENGINE_COOKED_ASSET_DIR}"
)
