# Cooks assets/m1_cube.glb into assets_cooked/m1_cube.mesh, once, via the `cooker` CLI
# tool (docs/01 section 12.4) -- every sample that needs it depends on the same
# `cooked_assets` target instead of each declaring its own add_custom_command for the
# identical output (CMake requires exactly one rule per OUTPUT file across the whole
# build; a second add_custom_command targeting the same path would be a configure error).
# CMake's own OUTPUT/DEPENDS tracking gives incremental re-cooking for free (only reruns
# when the source .glb or the cooker binary itself changes) -- the same principle already
# used for shader compilation in each sample's CMakeLists.txt, satisfying docs/01 section
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

if(TARGET cooker)
    set(_pi_engine_m1_cube_cooked "${PI_ENGINE_COOKED_ASSET_DIR}/m1_cube.mesh")
    add_custom_command(
        OUTPUT "${_pi_engine_m1_cube_cooked}"
        COMMAND "${CMAKE_COMMAND}" -E make_directory "${PI_ENGINE_COOKED_ASSET_DIR}"
        COMMAND cooker "${CMAKE_SOURCE_DIR}/assets/m1_cube.glb" "${_pi_engine_m1_cube_cooked}"
        DEPENDS "${CMAKE_SOURCE_DIR}/assets/m1_cube.glb" cooker
        COMMENT "Cooking assets/m1_cube.glb -> assets_cooked/m1_cube.mesh"
        VERBATIM
    )
    add_custom_target(cooked_assets DEPENDS "${_pi_engine_m1_cube_cooked}")
    unset(_pi_engine_m1_cube_cooked)
endif()
