# Cooks assets/m1_cube.glb into assets_cooked/m1_cube.mesh, and shaders/*.vert|frag into
# assets_cooked/shaders/*.spv, once, via the `cooker` CLI tool (docs/01 section 12.4) --
# every sample that needs one of these depends on the shared `cooked_assets` /
# `cooked_shaders` target instead of each declaring its own add_custom_command for the
# identical output (CMake requires exactly one rule per OUTPUT file across the whole
# build; a second add_custom_command targeting the same path would be a configure error).
# CMake's own OUTPUT/DEPENDS tracking gives incremental re-cooking for free (only reruns
# when a source file or the cooker binary itself changes), satisfying docs/01 section
# 12.4's "incremental cooking" without any hashing logic inside the tool.
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
    set(_pi_engine_m1_cube_cooked "${PI_ENGINE_COOKED_ASSET_DIR}/m1_cube.mesh")
    add_custom_command(
        OUTPUT "${_pi_engine_m1_cube_cooked}"
        COMMAND "${CMAKE_COMMAND}" -E make_directory "${PI_ENGINE_COOKED_ASSET_DIR}"
        COMMAND cooker mesh "${CMAKE_SOURCE_DIR}/assets/m1_cube.glb" "${_pi_engine_m1_cube_cooked}"
        DEPENDS "${CMAKE_SOURCE_DIR}/assets/m1_cube.glb" cooker
        COMMENT "Cooking assets/m1_cube.glb -> assets_cooked/m1_cube.mesh"
        VERBATIM
    )
    add_custom_target(cooked_assets DEPENDS "${_pi_engine_m1_cube_cooked}")
    unset(_pi_engine_m1_cube_cooked)

    # Every GLSL source under shaders/ (docs/03 section 5) -- shared across every sample
    # that uses it (m0_triangle.* only by m0_hello_vulkan, m1_unlit.* by the rest), so this
    # is a flat list rather than a per-sample one, same reasoning as the mesh rule above.
    set(_pi_engine_shader_sources
        "${CMAKE_SOURCE_DIR}/shaders/m0_triangle.vert"
        "${CMAKE_SOURCE_DIR}/shaders/m0_triangle.frag"
        "${CMAKE_SOURCE_DIR}/shaders/m1_unlit.vert"
        "${CMAKE_SOURCE_DIR}/shaders/m1_unlit.frag"
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
endif()
