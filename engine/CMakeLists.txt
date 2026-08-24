find_package(Jolt CONFIG REQUIRED)                      # physics/PhysicsWorld (M4, docs/03 section 9)

# Jolt::Jolt's INTERFACE_COMPILE_OPTIONS bake in a fairly aggressive x86-64 ISA baseline
# (AVX2/FMA/BMI/F16C/LZCNT). target_link_libraries() applies a dependency's interface
# compile options to *every* source file of the target it's attached to, regardless of the
# PUBLIC/PRIVATE/INTERFACE keyword -- that keyword only controls propagation to *other*
# targets that link this one. So the only way to keep those flags off the ~20 other
# engine_core source files that have nothing to do with physics (and would otherwise
# silently require an AVX2-capable CPU to even start) is to compile the two files that
# actually touch Jolt as their own OBJECT library, with Jolt linked there instead of on
# engine_core itself. This is also exactly why physics/PhysicsWorld.h stays Jolt-free
# (forward declarations + unique_ptr<IncompleteType>): everything outside these two files
# never needs Jolt's headers, flags, or (at final-link time) libJolt.a itself -- any
# executable that actually calls into PhysicsWorld must add its own
# find_package(Jolt)/target_link_libraries(... Jolt::Jolt), see
# samples/m4_hello_physics/CMakeLists.txt.
add_library(engine_physics_jolt OBJECT
    src/physics/CollisionCallbackDispatcher.cpp
    src/physics/JoltJobSystemAdapter.cpp
    src/physics/PhysicsWorld.cpp
)
target_include_directories(engine_physics_jolt PRIVATE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)
target_compile_features(engine_physics_jolt PRIVATE cxx_std_20)
target_compile_definitions(engine_physics_jolt PRIVATE GLM_FORCE_DEPTH_ZERO_TO_ONE)
target_link_libraries(engine_physics_jolt PRIVATE Jolt::Jolt)
pi_engine_set_warnings(engine_physics_jolt)

# vcpkg's Jolt build disables RTTI (libJolt.a has zero `typeinfo for JPH::*` symbols) --
# linking a class derived from a polymorphic JPH:: base against it with RTTI enabled (this
# project's default) fails with "undefined reference to typeinfo for JPH::<Base>", since
# these are the two TUs where JoltJobSystemAdapter's/CollisionCallbackDispatcher's own
# vtable/typeinfo actually get emitted (all of their overrides are defined out-of-line,
# right here). No RTTI is used in either file, so disabling it here to match Jolt's own
# build costs nothing.
if(MSVC)
    set_source_files_properties(
        src/physics/CollisionCallbackDispatcher.cpp
        src/physics/JoltJobSystemAdapter.cpp
        PROPERTIES COMPILE_OPTIONS "/GR-"
    )
else()
    set_source_files_properties(
        src/physics/CollisionCallbackDispatcher.cpp
        src/physics/JoltJobSystemAdapter.cpp
        PROPERTIES COMPILE_OPTIONS "-fno-rtti"
    )
endif()

add_library(engine_core STATIC
    src/asset/AssetGuid.cpp
    src/asset/AssetMeta.cpp
    src/core/Application.cpp
    src/core/Camera.cpp
    src/core/EngineVersion.cpp
    src/debug/Console.cpp
    src/debug/ImGuiOverlay.cpp
    src/ecs/World.cpp
    src/ecs/components/TransformComponent.cpp
    src/jobs/JobSystem.cpp
    src/physics/PhysicsPhase.cpp
    src/platform/InputSystem.cpp
    src/platform/SDL2DisplayBackend.cpp
    src/renderer/CookedMesh.cpp
    src/renderer/CookedTexture.cpp
    src/renderer/ForwardLitPipeline.cpp
    src/renderer/ForwardLitColorPipeline.cpp
    src/renderer/ForwardLitShadedPipeline.cpp
    src/renderer/ForwardLitTexturedPipeline.cpp
    src/renderer/ForwardLitTexturedColorPipeline.cpp
    src/renderer/ForwardVertexLitPipeline.cpp
    src/renderer/ForwardVertexLitTexturedPipeline.cpp
    src/renderer/FrustumCuller.cpp
    src/renderer/MaterialData.cpp
    src/renderer/MeshLoader.cpp
    src/renderer/ShaderPropertySchema.cpp
    src/renderer/ShadowDepthPipeline.cpp
    src/rhi/RHIBuffer.cpp
    src/rhi/RHIContext.cpp
    src/rhi/RHIPipeline.cpp
    src/rhi/RHIShadowMap.cpp
    src/rhi/RHISwapchain.cpp
    src/rhi/RHITexture.cpp
    src/rhi/VmaImplementation.cpp
    src/scene/Prefab.cpp
    src/scene/Scene.cpp
    src/scene/SceneDocument.cpp
    src/script/ScriptRegistry.cpp
    "${CMAKE_SOURCE_DIR}/third_party/imgui_backends/imgui_impl_sdl2.cpp"
    "${CMAKE_SOURCE_DIR}/third_party/imgui_backends/imgui_impl_vulkan.cpp"
    $<TARGET_OBJECTS:engine_physics_jolt>
)

target_include_directories(engine_core
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    PRIVATE
        # cgltf.h/stb_image.h (docs/03 section 3.1: vendored single headers, no vcpkg
        # package) -- only MeshLoader.cpp/CookTexture.cpp (Cooker-only, not engine_core)
        # include those; never exposed through a public engine header.
        "${CMAKE_SOURCE_DIR}/third_party"
        # imgui_impl_sdl2.h/imgui_impl_vulkan.h (vendored, third_party/README.md explains
        # why) -- included with angle brackets matching upstream imgui's own convention,
        # so this subdirectory needs to be on the include path directly, not just its
        # third_party/ parent.
        "${CMAKE_SOURCE_DIR}/third_party/imgui_backends"
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
    # imgui_impl_sdl2.cpp/imgui_impl_vulkan.cpp (vendored, third_party/README.md) --
    # same reasoning as cgltf.h above, not ours to fix. debug/ImGuiOverlay.cpp needs the
    # same suppression even though it isn't itself vendored: imgui_impl_vulkan.h defines
    # ImGui_ImplVulkanH_Window's constructor inline (a C-style cast in a memset call), so
    # the warning fires wherever that header is included and the constructor gets
    # instantiated, not just inside imgui_impl_vulkan.cpp.
    set_source_files_properties(
        "${CMAKE_SOURCE_DIR}/third_party/imgui_backends/imgui_impl_sdl2.cpp"
        "${CMAKE_SOURCE_DIR}/third_party/imgui_backends/imgui_impl_vulkan.cpp"
        src/debug/ImGuiOverlay.cpp
        PROPERTIES COMPILE_OPTIONS "-Wno-old-style-cast;-Wno-conversion;-Wno-sign-conversion;-Wno-cast-align;-Wno-double-promotion;-Wno-switch-enum;-Wno-unused-parameter"
    )
endif()

# imgui_impl_vulkan.h defaults to including <vulkan/vulkan.h> directly, which would need a
# separately-linked libvulkan loader at link time; IMGUI_IMPL_VULKAN_USE_VOLK switches it
# to <volk.h> instead, so it dispatches through the same volk-loaded function pointers as
# every other Vulkan call in this project (RHIContext.cpp's volkInitialize()/
# volkLoadInstance()/volkLoadDevice()) -- see third_party/README.md's imgui_backends/
# entry. Needs to be visible to every TU that includes imgui_impl_vulkan.h, not just the
# vendored .cpp itself (debug/ImGuiOverlay.cpp does too) -- PRIVATE on the whole target is
# harmless since no other engine_core source includes that header.
target_compile_definitions(engine_core PRIVATE IMGUI_IMPL_VULKAN_USE_VOLK)

# Vulkan's clip space has Y pointing down and 0..1 depth (unlike the OpenGL convention
# GLM assumes by default) -- PUBLIC so every consumer's GLM usage stays consistent with
# engine/core/Camera.h. See that header's comment for the Y-flip half of this.
target_compile_definitions(engine_core PUBLIC GLM_FORCE_DEPTH_ZERO_TO_ONE)

# --- Dependencies pinned in vcpkg.json (docs/03, section 3.1) ---
# Wired in as each milestone starts actually consuming them; see the README.md in each
# engine/include/engine/<module>/ directory for the file-by-file, milestone-by-milestone plan.
# (Jolt is found above, ahead of engine_physics_jolt -- it's deliberately not linked here,
# see that target's own comment.)

find_package(volk CONFIG REQUIRED)                     # rhi/RHIContext (M0, docs/03 section 5)
find_package(VulkanMemoryAllocator CONFIG REQUIRED)     # rhi/RHIBuffer (M1, docs/03 section 6)
# platform/SDL2DisplayBackend (M0). The "vulkan" feature (see vcpkg.json) matters: vcpkg's
# sdl2 port builds with SDL_VULKAN=OFF by default, which makes SDL_Vulkan_CreateSurface()
# fail at runtime with "Vulkan support is not configured in SDL" even though everything
# links and compiles fine.
find_package(SDL2 CONFIG REQUIRED)
find_package(glm CONFIG REQUIRED)                       # core/Camera (M1, docs/03 section 6)
find_package(Threads REQUIRED)                          # jobs/JobSystem (M2, docs/01 section 9.3)
find_package(nlohmann_json CONFIG REQUIRED)             # scene/SceneDocument (M7, docs/01 section 12.2)
find_package(imgui CONFIG REQUIRED)                     # debug/ImGuiOverlay (Editor step E1, docs/06)

target_link_libraries(engine_core
    PUBLIC
        volk::volk
        GPUOpen::VulkanMemoryAllocator
        glm::glm
        Threads::Threads
        # PUBLIC: SDL2DisplayBackend.h is part of engine_core's public API, so anything
        # linking engine_core (the samples) needs SDL2's headers/libs transitively too.
        $<IF:$<TARGET_EXISTS:SDL2::SDL2>,SDL2::SDL2,SDL2::SDL2-static>
        # PRIVATE would be more accurate (nlohmann::json never appears in a public engine
        # header, see scene/SceneDocument.h's own comment) but this is a pure INTERFACE
        # (header-only) target with no compile options of its own to leak (confirmed by
        # reading its vcpkg-generated Config.cmake -- unlike Jolt's, see
        # engine_physics_jolt's comment above), so PUBLIC vs PRIVATE has no real
        # consequence here; kept PUBLIC for consistency with the rest of this list.
        nlohmann_json::nlohmann_json
        # PUBLIC: any consumer of debug/ImGuiOverlay (the future Editor, or a sample
        # adding a debug overlay) needs to call ImGui:: itself to build its own UI, so
        # imgui.h/the compiled library need to reach them transitively too -- same
        # reasoning as SDL2 above. Only vcpkg's plain "imgui" package (core library, no
        # features) -- the vulkan-binding/sdl2-binding halves are vendored instead, see
        # third_party/README.md.
        imgui::imgui
)
