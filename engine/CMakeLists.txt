add_library(engine_core STATIC
    src/core/EngineVersion.cpp
)

target_include_directories(engine_core
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

target_compile_features(engine_core PUBLIC cxx_std_20)
pi_engine_set_warnings(engine_core)

# --- Dependencies pinned in vcpkg.json (docs/03, section 3.1) ---
# Wired in as each milestone starts actually consuming them; see the README.md in each
# engine/include/engine/<module>/ directory for the file-by-file, milestone-by-milestone plan.

find_package(volk CONFIG REQUIRED)                     # M0 — rhi/RHIContext (docs/03 section 5)
find_package(VulkanMemoryAllocator CONFIG REQUIRED)     # M1 — rhi/RHIBuffer (docs/03 section 6)
find_package(SDL2 CONFIG REQUIRED)                      # M0 — platform/SDL2DisplayBackend
find_package(glm CONFIG REQUIRED)                       # M1 — core/Camera (docs/03 section 6)

target_link_libraries(engine_core
    PUBLIC
        volk::volk
        GPUOpen::VulkanMemoryAllocator
        glm::glm
    PRIVATE
        $<IF:$<TARGET_EXISTS:SDL2::SDL2>,SDL2::SDL2,SDL2::SDL2-static>
)

# TODO(M2): find_package(imgui CONFIG REQUIRED) — debug overlay (docs/01 section 4, module 4).
# TODO(M4): find_package(Jolt CONFIG REQUIRED) — physics/PhysicsWorld (docs/03 section 9).
