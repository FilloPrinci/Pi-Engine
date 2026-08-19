# Cooks assets/*.{glb,gltf} into assets_cooked/*.mesh, shaders/*.vert|frag into
# assets_cooked/shaders/*.spv, and assets/*.png into assets_cooked/*.tex, once, via the
# `cooker` CLI tool (docs/01 section 12.4) -- every sample that needs one of these depends
# on the shared `cooked_assets` / `cooked_shaders` / `cooked_textures` target instead of
# each declaring its own add_custom_command for the identical output (CMake requires
# exactly one rule per OUTPUT file across the whole build; a second add_custom_command
# targeting the same path would be a configure error). CMake's own OUTPUT/DEPENDS tracking
# gives incremental re-cooking for free (only reruns when a source file or the cooker
# binary itself changes), satisfying docs/01 section 12.4's "incremental cooking" without
# any hashing logic inside the tool.
#
# Only defined when the `cooker` target actually exists (root CMakeLists.txt skips
# building it while cross-compiling -- a build-time tool must run on the machine doing the
# build, not the cross-compilation target). A cross-compiled pi4/pi5 build currently
# expects assets_cooked/ to already exist from a prior native build (linux-pc, or pi4/pi5
# built natively on the device itself, which turns CMAKE_CROSSCOMPILING off); proper
# host-tool bootstrapping for true cross-compiles is future work, out of scope for this
# minimal Cooker milestone.
set(PI_ENGINE_COOKED_ASSET_DIR "${CMAKE_BINARY_DIR}/assets_cooked" CACHE PATH
    "Output directory for Cooker-produced (docs/01 section 12) assets")
set(PI_ENGINE_COOKED_SHADER_DIR "${PI_ENGINE_COOKED_ASSET_DIR}/shaders" CACHE PATH
    "Output directory for Cooker-produced (cooker shader, GLSL->SPIR-V via shaderc) shaders")

if(TARGET cooker)
    # Every mesh source under assets/ (docs/01 section 12.1) -- m1_cube.glb (M1, reused by
    # most samples), m7_quad.gltf (M7 textures step: a hand-authored quad with UVs, since
    # m1_cube.glb predates TEXCOORD_0 and isn't worth regenerating just for this), and
    # m7_lod_sphere.gltf (M7 LOD-generation step: a subdivided icosphere -- m1_cube.glb/
    # m7_quad.gltf are both too simple to meaningfully demonstrate mesh simplification).
    set(_pi_engine_mesh_sources
        "${CMAKE_SOURCE_DIR}/assets/m1_cube.glb"
        "${CMAKE_SOURCE_DIR}/assets/m7_quad.gltf"
        "${CMAKE_SOURCE_DIR}/assets/m7_lod_sphere.gltf"
    )
    set(_pi_engine_cooked_meshes "")
    foreach(mesh_source ${_pi_engine_mesh_sources})
        get_filename_component(mesh_name "${mesh_source}" NAME_WE)
        set(mesh_output "${PI_ENGINE_COOKED_ASSET_DIR}/${mesh_name}.mesh")
        add_custom_command(
            OUTPUT "${mesh_output}"
            COMMAND "${CMAKE_COMMAND}" -E make_directory "${PI_ENGINE_COOKED_ASSET_DIR}"
            COMMAND cooker mesh "${mesh_source}" "${mesh_output}"
            DEPENDS "${mesh_source}" cooker
            COMMENT "Cooking assets/${mesh_name} -> assets_cooked/${mesh_name}.mesh"
            VERBATIM
        )
        list(APPEND _pi_engine_cooked_meshes "${mesh_output}")
    endforeach()
    add_custom_target(cooked_assets DEPENDS ${_pi_engine_cooked_meshes})
    unset(_pi_engine_mesh_sources)
    unset(_pi_engine_cooked_meshes)

    # Every GLSL source under shaders/ (docs/03 section 5) -- shared across every sample
    # that uses it (m0_triangle.* only by m0_hello_vulkan, m1_unlit.* by most others,
    # m7_textured.* by samples/m7_textures), so this is a flat list rather than a
    # per-sample one, same reasoning as the mesh rule above.
    set(_pi_engine_shader_sources
        "${CMAKE_SOURCE_DIR}/shaders/m0_triangle.vert"
        "${CMAKE_SOURCE_DIR}/shaders/m0_triangle.frag"
        "${CMAKE_SOURCE_DIR}/shaders/m1_unlit.vert"
        "${CMAKE_SOURCE_DIR}/shaders/m1_unlit.frag"
        "${CMAKE_SOURCE_DIR}/shaders/m7_textured.vert"
        "${CMAKE_SOURCE_DIR}/shaders/m7_textured.frag"
    )
    set(_pi_engine_cooked_shaders "")
    foreach(shader_source ${_pi_engine_shader_sources})
        get_filename_component(shader_name "${shader_source}" NAME)
        set(shader_output "${PI_ENGINE_COOKED_SHADER_DIR}/${shader_name}.spv")
        add_custom_command(
            OUTPUT "${shader_output}"
            COMMAND "${CMAKE_COMMAND}" -E make_directory "${PI_ENGINE_COOKED_SHADER_DIR}"
            COMMAND cooker shader "${shader_source}" "${shader_output}"
            DEPENDS "${shader_source}" cooker
            COMMENT "Cooking shaders/${shader_name} -> assets_cooked/shaders/${shader_name}.spv"
            VERBATIM
        )
        list(APPEND _pi_engine_cooked_shaders "${shader_output}")
    endforeach()
    add_custom_target(cooked_shaders DEPENDS ${_pi_engine_cooked_shaders})
    unset(_pi_engine_shader_sources)
    unset(_pi_engine_cooked_shaders)

    # Every texture source under assets/ (M7 textures step) -- just the one demo checker
    # texture for now (samples/m7_textures).
    set(_pi_engine_texture_sources
        "${CMAKE_SOURCE_DIR}/assets/m7_checker.png"
    )
    set(_pi_engine_cooked_textures "")
    foreach(texture_source ${_pi_engine_texture_sources})
        get_filename_component(texture_name "${texture_source}" NAME_WE)
        set(texture_output "${PI_ENGINE_COOKED_ASSET_DIR}/${texture_name}.tex")
        add_custom_command(
            OUTPUT "${texture_output}"
            COMMAND "${CMAKE_COMMAND}" -E make_directory "${PI_ENGINE_COOKED_ASSET_DIR}"
            COMMAND cooker texture "${texture_source}" "${texture_output}"
            DEPENDS "${texture_source}" cooker
            COMMENT "Cooking assets/${texture_name}.png -> assets_cooked/${texture_name}.tex"
            VERBATIM
        )
        list(APPEND _pi_engine_cooked_textures "${texture_output}")
    endforeach()
    add_custom_target(cooked_textures DEPENDS ${_pi_engine_cooked_textures})
    unset(_pi_engine_texture_sources)
    unset(_pi_engine_cooked_textures)
endif()
