add_library(engine_core STATIC
    src/core/Application.cpp
    src/core/Camera.cpp
    src/core/EngineVersion.cpp
    src/ecs/World.cpp
    src/ecs/components/TransformComponent.cpp
    src/jobs/JobSystem.cpp
    src/platform/SDL2DisplayBackend.cpp
    src/renderer/ForwardLitPipeline.cpp
    src/renderer/FrustumCuller.cpp
    src/renderer/MeshLoader.cpp
    src/rhi/RHIBuffer.cpp
    src/rhi/RHIContext.cpp
    src/rhi/RHIPipeline.cpp
    src/rhi/RHISwapchain.cpp
    src/rhi/VmaImplementation.cpp
)

target_include_directories(engine_core
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    PRIVATE
        # cgltf.h (docs/03 section 3.1: vendored single header, no vcpkg package) -- only
        # MeshLoader.cpp includes it, never exposed through a public engine header.
        "${CMAKE_SOURCE_DIR}/third_party"
)

target_compile_features(engine_core PUBLIC cxx_std_20)
pi_engine_set_warnings(engine_core)

# cgltf.h (vendored third-party, third_party/README.md) triggers this project's strict
# warning flags on its own C-style code -- not ours to fix. Only MeshLoader.cpp defines
# CGLTF_IMPLEMENTATION and includes it.
if(NOT MSVC)
    set_source_files_properties(src/renderer/MeshLoader.cpp PROPERTIES
        COMPILE_OPTIONS "-Wno-old-style-cast;-Wno-conversion;-Wno-sign-conversion;-Wno-cast-align;-Wno-double-promotion"
    )
endif()

# Vulkan's clip space has Y pointing down and 0..1 depth (unlike the OpenGL convention
# GLM assumes by default) -- PUBLIC so every consumer's GLM usage stays consistent with
# engine/core/Camera.h. See that header's comment for the Y-flip half of this.
target_compile_definitions(engine_core PUBLIC GLM_FORCE_DEPTH_ZERO_TO_ONE)

# --- Dependencies pinned in vcpkg.json (docs/03, section 3.1) ---
# Wired in as each milestone starts actually consuming them; see the README.md in each
# engine/include/engine/<module>/ directory for the file-by-file, milestone-by-milestone plan.

find_package(volk CONFIG REQUIRED)                     # rhi/RHIContext (M0, docs/03 section 5)
find_package(VulkanMemoryAllocator CONFIG REQUIRED)     # rhi/RHIBuffer (M1, docs/03 section 6)
# platform/SDL2DisplayBackend (M0). The "vulkan" feature (see vcpkg.json) matters: vcpkg's
# sdl2 port builds with SDL_VULKAN=OFF by default, which makes SDL_Vulkan_CreateSurface()
# fail at runtime with "Vulkan support is not configured in SDL" even though everything
# links and compiles fine.
find_package(SDL2 CONFIG REQUIRED)
find_package(glm CONFIG REQUIRED)                       # core/Camera (M1, docs/03 section 6)
find_package(Threads REQUIRED)                          # jobs/JobSystem (M2, docs/01 section 9.3)

target_link_libraries(engine_core
    PUBLIC
        volk::volk
        GPUOpen::VulkanMemoryAllocator
        glm::glm
        Threads::Threads
        # PUBLIC: SDL2DisplayBackend.h is part of engine_core's public API, so anything
        # linking engine_core (the samples) needs SDL2's headers/libs transitively too.
        $<IF:$<TARGET_EXISTS:SDL2::SDL2>,SDL2::SDL2,SDL2::SDL2-static>
)

# TODO(M2): find_package(imgui CONFIG REQUIRED) — debug overlay (docs/01 section 4, module 4).
# TODO(M4): find_package(Jolt CONFIG REQUIRED) — physics/PhysicsWorld (docs/03 section 9).
