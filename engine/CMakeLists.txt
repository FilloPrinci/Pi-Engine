add_library(engine_core STATIC
    src/core/EngineVersion.cpp
    src/platform/SDL2DisplayBackend.cpp
    src/rhi/RHIContext.cpp
    src/rhi/RHISwapchain.cpp
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
# M0 — platform/SDL2DisplayBackend. The "vulkan" feature (see vcpkg.json) matters: vcpkg's
# sdl2 port builds with SDL_VULKAN=OFF by default, which makes SDL_Vulkan_CreateSurface()
# fail at runtime with "Vulkan support is not configured in SDL" even though everything
# links and compiles fine.
find_package(SDL2 CONFIG REQUIRED)
find_package(glm CONFIG REQUIRED)                       # M1 — core/Camera (docs/03 section 6)

target_link_libraries(engine_core
    PUBLIC
        volk::volk
        GPUOpen::VulkanMemoryAllocator
        glm::glm
        # PUBLIC: SDL2DisplayBackend.h is part of engine_core's public API, so anything
        # linking engine_core (the samples) needs SDL2's headers/libs transitively too.
        $<IF:$<TARGET_EXISTS:SDL2::SDL2>,SDL2::SDL2,SDL2::SDL2-static>
)

# TODO(M2): find_package(imgui CONFIG REQUIRED) — debug overlay (docs/01 section 4, module 4).
# TODO(M4): find_package(Jolt CONFIG REQUIRED) — physics/PhysicsWorld (docs/03 section 9).
