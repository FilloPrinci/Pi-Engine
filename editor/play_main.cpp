#include "engine/asset/AssetGuid.h"
#include "engine/core/Application.h"
#include "engine/core/Camera.h"
#include "engine/core/EngineVersion.h"
#include "engine/debug/Console.h"
#include "engine/debug/ImGuiOverlay.h"
#include "engine/ecs/World.h"
#include "engine/jobs/JobSystem.h"
#include "engine/physics/PhysicsPhase.h"
#include "engine/physics/PhysicsWorld.h"
#include "engine/platform/InputSystem.h"
#include "engine/platform/SDL2DisplayBackend.h"
#include "engine/asset/AssetMeta.h"
#include "engine/renderer/CookedMesh.h"
#include "engine/renderer/CookedTexture.h"
#include "engine/renderer/ForwardLitColorPipeline.h"
#include "engine/renderer/ForwardLitPipeline.h"
#include "engine/renderer/ForwardLitTexturedColorPipeline.h"
#include "engine/renderer/MaterialData.h"
#include "engine/rhi/RHIBuffer.h"
#include "engine/rhi/RHIContext.h"
#include "engine/rhi/RHISwapchain.h"
#include "engine/rhi/RHITexture.h"
#include "engine/scene/Scene.h"
#include "engine/script/ScriptComponent.h"
#include "engine/script/ScriptRegistry.h"

#include "scripts/RotateScript.h"

#include <imgui.h>
#include <volk.h>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <optional>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

using engine::asset::AssetGuid;
using engine::core::Application;
using engine::core::Camera;
using engine::debug::Console;
using engine::debug::ImGuiOverlay;
using engine::ecs::Entity;
using engine::ecs::TransformComponent;
using engine::ecs::World;
using engine::jobs::JobSystem;
using engine::physics::PhysicsPhase;
using engine::physics::PhysicsWorld;
using engine::platform::InputSystem;
using engine::platform::Key;
using engine::platform::SDL2DisplayBackend;
using engine::renderer::ForwardLitColorPipeline;
using engine::renderer::ForwardLitPipeline;
using engine::renderer::ForwardLitTexturedColorPipeline;
using engine::renderer::MaterialData;
using engine::renderer::MeshData;
using engine::rhi::RHIBuffer;
using engine::rhi::RHIContext;
using engine::rhi::RHISwapchain;
using engine::rhi::RHITexture;
using engine::script::ScriptComponent;
using engine::script::ScriptRegistry;

namespace {

constexpr int kMaxFramesInFlight = 2;

std::string ShaderPath(const char* fileName) {
    return std::string(PI_ENGINE_SHADER_DIR) + "/" + fileName;
}

std::string CookedAssetPath(const char* fileName) {
    return std::string(PI_ENGINE_COOKED_ASSET_DIR) + "/" + fileName;
}

// Same depth-format fallback chain as every sample since M1.
VkFormat ChooseDepthFormat(VkPhysicalDevice physicalDevice) {
    const VkFormat candidates[] = {VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT,
                                    VK_FORMAT_D24_UNORM_S8_UINT};
    for (VkFormat format : candidates) {
        VkFormatProperties props{};
        vkGetPhysicalDeviceFormatProperties(physicalDevice, format, &props);
        if ((props.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) != 0) {
            return format;
        }
    }
    return VK_FORMAT_UNDEFINED;
}

// Same GUID -> GPU-buffers cache as editor/main.cpp -- see that file's own comment
// (mirrors samples/m7_scene_and_prefab's pattern; resolves any cooked mesh under
// assets_cooked/, not just one hardcoded file).
struct MeshGpuData {
    RHIBuffer vertexBuffer;
    RHIBuffer indexBuffer;
    std::uint32_t indexCount = 0;
};

} // namespace

// Editor step E8 -- editor_play (docs/06-editor-roadmap.md): "Play" launches this as a
// *separate executable* from `editor` (BuildPipeline.cpp's LaunchPlayProcess()), not the
// same binary re-invoked with a flag -- see this file's own CMakeLists.txt entry for why
// (physics/PhysicsWorld.h pulls in Jolt, and Jolt::Jolt's INTERFACE_COMPILE_OPTIONS would
// otherwise leak AVX2/FMA compile flags onto every one of `editor`'s own source files, not
// just this one -- [[vcpkg-dependency-isa-flag-isolation]], the same trap engine_core's
// own engine_physics_jolt OBJECT library isolation already exists to avoid). Renders and
// simulates physics for `scenePath` with no Editor panels -- Scene/Inspector/Asset
// Browser/Project Hub, none of which mean anything once the point is "play the game", not
// "edit it" -- keeping only a small "Play Mode" info overlay and the Console panel (E5),
// both useful standalone too.
//
// Deliberately not a real "game player" in the Unity Play Mode sense: there is no C++
// hot-reload in this engine (docs/01 section 6.1), so Play can never simulate *inside* the
// running Editor process the way Unity does -- a separate process is the only option, the
// same reason Editor step E7's Project Hub relaunches into a new process rather than
// hot-swapping the loaded scene. Runs a real Script phase now too (scene JSON's
// EntityDesc::scriptNames, attached via AttachScriptFn -- docs/07-unity-parity-analysis.md's
// former "data-driven scripting: missing" row), same phase order as m5_vertical_slice
// (Script -> barrier -> Physics -> barrier -> Collision Callback). The one remaining,
// structural limit: "data-driven" still means "already compiled into this executable",
// never true hot-loadable scripting -- a scene can only reference a script type
// editor_play itself was built with (today: RotateScript.h, see its own comment). A scene
// authored against a script type that isn't linked in just skips that attachment with a
// stderr warning, same "degrade, don't crash" precedent as everything else in this file.
//
// Usage: `editor_play <scene.json>` -- no default path (unlike `editor`'s own optional
// argv[1]): always launched by BuildPipeline.cpp with an explicit scene, never by hand.
int main(int argc, char** argv) {
    if (argc != 2) {
        std::fprintf(stderr, "usage: editor_play <scene.json>\n");
        return EXIT_FAILURE;
    }
    const std::string scenePath = argv[1];

    // Console::Init() must run before any other stdout/stderr I/O -- literally the first
    // statement in main(), same reasoning as editor/main.cpp's own Console::Init() call.
    Console console;
    const bool consoleAvailable = console.Init();

    std::printf("Pi-Engine %s -- Play Mode (%s)\n", engine::core::GetEngineVersionString(),
                scenePath.c_str());

    SDL2DisplayBackend displayBackend;
    if (!displayBackend.Init()) {
        return EXIT_FAILURE;
    }

    RHIContext context;
    if (!context.Init(displayBackend, "Pi-Engine -- Play Mode")) {
        return EXIT_FAILURE;
    }

    RHISwapchain swapchain;
    if (!swapchain.Init(context, displayBackend.GetDrawableSize())) {
        return EXIT_FAILURE;
    }

    VkDevice device = context.GetDevice();

    // Mesh GUID -> cooked-path index, same as editor/main.cpp's own (see that file's
    // comment) -- built by scanning PI_ENGINE_COOKED_ASSET_DIR for "*.mesh" files and
    // reading each one's own embedded GUID.
    std::unordered_map<AssetGuid, std::string> meshGuidToPath;
    {
        std::error_code errorCode;
        const std::filesystem::path cookedDir(PI_ENGINE_COOKED_ASSET_DIR);
        if (std::filesystem::exists(cookedDir, errorCode)) {
            for (const auto& entry : std::filesystem::directory_iterator(cookedDir, errorCode)) {
                if (!entry.is_regular_file() || entry.path().extension() != ".mesh") {
                    continue;
                }
                MeshData probe; // discarded -- resolveMesh() below loads it for real.
                AssetGuid guid;
                if (engine::renderer::LoadCookedMesh(entry.path().string().c_str(), probe, &guid)) {
                    meshGuidToPath.emplace(guid, entry.path().string());
                }
            }
        }
    }

    std::unordered_map<AssetGuid, std::unique_ptr<MeshGpuData>> meshCache;
    auto resolveMesh = [&](const AssetGuid& guid) -> MeshGpuData* {
        auto it = meshCache.find(guid);
        if (it != meshCache.end()) {
            return it->second.get();
        }

        auto pathIt = meshGuidToPath.find(guid);
        if (pathIt == meshGuidToPath.end()) {
            return nullptr;
        }

        MeshData meshData;
        if (!engine::renderer::LoadCookedMesh(pathIt->second.c_str(), meshData)) {
            return nullptr;
        }

        auto gpuData = std::make_unique<MeshGpuData>();
        if (!gpuData->vertexBuffer.InitWithData(
                context, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, meshData.vertices.data(),
                meshData.vertices.size() * sizeof(meshData.vertices[0])) ||
            !gpuData->indexBuffer.InitWithData(
                context, VK_BUFFER_USAGE_INDEX_BUFFER_BIT, meshData.indices.data(),
                meshData.indices.size() * sizeof(meshData.indices[0]))) {
            return nullptr;
        }
        gpuData->indexCount = static_cast<std::uint32_t>(meshData.indices.size());

        MeshGpuData* result = gpuData.get();
        meshCache.emplace(guid, std::move(gpuData));
        return result;
    };

    // Material assets (post-Editor-E8, renderer/MaterialData.h) -- same GUID -> path index
    // + load-on-demand cache as editor/main.cpp's own resolveMaterial (see that file's
    // comment for why this needs a real scan rather than meshCache's hardcoded-path
    // shortcut). Built once here since PI_ENGINE_ASSETS_DIR doesn't change while
    // editor_play is running.
    std::unordered_map<AssetGuid, std::string> materialGuidToPath;
    {
        std::error_code errorCode;
        const std::filesystem::path assetsDir(PI_ENGINE_ASSETS_DIR);
        if (std::filesystem::exists(assetsDir, errorCode)) {
            for (const auto& entry : std::filesystem::directory_iterator(assetsDir, errorCode)) {
                if (!entry.is_regular_file()) {
                    continue;
                }
                const std::string name = entry.path().filename().string();
                constexpr std::string_view kMaterialSuffix = ".material.json";
                if (name.size() < kMaterialSuffix.size() ||
                    name.compare(name.size() - kMaterialSuffix.size(), kMaterialSuffix.size(),
                                kMaterialSuffix) != 0) {
                    continue;
                }
                const std::string fullPath = entry.path().string();
                AssetGuid guid;
                if (engine::asset::TryReadAssetMetaGuid(fullPath.c_str(), guid)) {
                    materialGuidToPath.emplace(guid, fullPath);
                }
            }
        }
    }

    std::unordered_map<AssetGuid, std::unique_ptr<MaterialData>> materialCache;
    auto resolveMaterial = [&](const AssetGuid& guid) -> MaterialData* {
        auto it = materialCache.find(guid);
        if (it != materialCache.end()) {
            return it->second.get();
        }
        auto pathIt = materialGuidToPath.find(guid);
        if (pathIt == materialGuidToPath.end()) {
            return nullptr;
        }
        auto material = std::make_unique<MaterialData>();
        if (!engine::renderer::LoadMaterial(pathIt->second.c_str(), *material)) {
            return nullptr;
        }
        MaterialData* result = material.get();
        materialCache.emplace(guid, std::move(material));
        return result;
    };

    // Texture assets (post-Editor-E8, ShaderPropertySchema.h's "Texture" property type) --
    // same GUID -> source-path index shape as materialGuidToPath above, just for "*.png"
    // instead of "*.material.json"; see editor/main.cpp's own identical index for the full
    // reasoning (cooked .tex path derived from the source filename stem, matching
    // cmake/CookAssets.cmake's own naming convention).
    std::unordered_map<AssetGuid, std::string> textureGuidToPath;
    {
        std::error_code errorCode;
        const std::filesystem::path assetsDir(PI_ENGINE_ASSETS_DIR);
        if (std::filesystem::exists(assetsDir, errorCode)) {
            for (const auto& entry : std::filesystem::directory_iterator(assetsDir, errorCode)) {
                if (!entry.is_regular_file()) {
                    continue;
                }
                const std::string name = entry.path().filename().string();
                constexpr std::string_view kPngSuffix = ".png";
                if (name.size() < kPngSuffix.size() ||
                    name.compare(name.size() - kPngSuffix.size(), kPngSuffix.size(),
                                kPngSuffix) != 0) {
                    continue;
                }
                const std::string fullPath = entry.path().string();
                AssetGuid guid;
                if (engine::asset::TryReadAssetMetaGuid(fullPath.c_str(), guid)) {
                    textureGuidToPath.emplace(guid, fullPath);
                }
            }
        }
    }

    // --- Scene: ECS World + Job System + Physics World -- unlike the Editor's own
    //     read-only Scene View (editor/main.cpp), Play Mode is meant to actually run the
    //     scene, so every entity with both a Collider and a Rigidbody gets a live Jolt
    //     body via the same CreatePhysicsBodyFn callback SpawnBox() uses in m4/m5. No
    //     ScriptComponents run (see PlayRuntime.h's own comment on why -- scene JSON has
    //     no script field yet). ---
    World world;

    JobSystem jobSystem;
    if (!jobSystem.Init()) {
        std::fprintf(stderr, "play: JobSystem::Init failed\n");
        return EXIT_FAILURE;
    }

    PhysicsWorld physicsWorld;
    if (!physicsWorld.Init(jobSystem)) {
        std::fprintf(stderr, "play: PhysicsWorld::Init failed\n");
        return EXIT_FAILURE;
    }
    PhysicsPhase physicsPhase; // default fixed step: 1/60 s (docs/01 section 9.4)
    InputSystem inputSystem;

    // Scripts (docs/07-unity-parity-analysis.md's "data-driven scripting" gap, first
    // addressed here): engine::scene owns none of these -- SpawnEntities() only calls
    // AttachScriptFn per EntityDesc::scriptNames entry, this process is what actually
    // keeps the created ScriptComponents alive and drives their OnUpdate() (Script phase,
    // below, mirroring m5_vertical_slice's exact phase order). RotateScript.h is the one
    // script type currently linked into editor_play -- see its own comment for why it's
    // generic rather than demo-specific like samples/m3-m5's own scripts.
    std::vector<std::unique_ptr<ScriptComponent>> scripts;
    std::vector<ScriptComponent*> scriptPtrs; // Same objects as `scripts`, non-owning --
                                               // DispatchCollisionCallbacks() below wants
                                               // raw pointers, rebuilt fresh every frame
                                               // would be wasted work for no reason.
    auto attachScript = [&](World& w, Entity entity, const std::string& scriptName) {
        std::unique_ptr<ScriptComponent> script = ScriptRegistry::Create(scriptName);
        if (!script) {
            std::fprintf(stderr,
                         "play: scene references script \"%s\", but no REGISTER_SCRIPT(%s) "
                         "is linked into editor_play -- skipping it\n",
                         scriptName.c_str(), scriptName.c_str());
            return;
        }
        script->Attach(w, entity, inputSystem, &physicsWorld);
        script->OnStart();
        scriptPtrs.push_back(script.get());
        scripts.push_back(std::move(script));
    };

    auto createPhysicsBody = [&](World& w, Entity entity, bool isStatic) {
        physicsWorld.CreateBody(w, entity, isStatic);
    };
    if (!engine::scene::LoadScene(scenePath.c_str(), world, createPhysicsBody, attachScript)) {
        std::fprintf(stderr, "play: failed to load scene \"%s\"\n", scenePath.c_str());
        return EXIT_FAILURE;
    }
    std::printf("play: loaded scene \"%s\" (%zu mesh entities, %zu script(s))\n",
                scenePath.c_str(), world.Meshes().Data().size(), scripts.size());

    const VkFormat depthFormat = ChooseDepthFormat(context.GetPhysicalDevice());
    if (depthFormat == VK_FORMAT_UNDEFINED) {
        std::fprintf(stderr, "play: no supported depth/stencil format found\n");
        return EXIT_FAILURE;
    }

    VkImage depthImage = VK_NULL_HANDLE;
    VmaAllocation depthAllocation = VK_NULL_HANDLE;
    VkImageView depthView = VK_NULL_HANDLE;

    auto createDepthResources = [&](VkExtent2D extent) -> bool {
        VkImageCreateInfo imageInfo{};
        imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageInfo.imageType = VK_IMAGE_TYPE_2D;
        imageInfo.extent = {extent.width, extent.height, 1};
        imageInfo.mipLevels = 1;
        imageInfo.arrayLayers = 1;
        imageInfo.format = depthFormat;
        imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        imageInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
        imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        VmaAllocationCreateInfo allocInfo{};
        allocInfo.usage = VMA_MEMORY_USAGE_AUTO;

        if (vmaCreateImage(context.GetAllocator(), &imageInfo, &allocInfo, &depthImage,
                            &depthAllocation, nullptr) != VK_SUCCESS) {
            std::fprintf(stderr, "play: vmaCreateImage (depth) failed\n");
            return false;
        }

        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = depthImage;
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = depthFormat;
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.layerCount = 1;

        if (vkCreateImageView(device, &viewInfo, nullptr, &depthView) != VK_SUCCESS) {
            std::fprintf(stderr, "play: vkCreateImageView (depth) failed\n");
            return false;
        }
        return true;
    };

    auto destroyDepthResources = [&]() {
        if (depthView != VK_NULL_HANDLE) {
            vkDestroyImageView(device, depthView, nullptr);
            depthView = VK_NULL_HANDLE;
        }
        if (depthImage != VK_NULL_HANDLE) {
            vmaDestroyImage(context.GetAllocator(), depthImage, depthAllocation);
            depthImage = VK_NULL_HANDLE;
            depthAllocation = VK_NULL_HANDLE;
        }
    };

    if (!createDepthResources(swapchain.GetExtent())) {
        return EXIT_FAILURE;
    }

    VkAttachmentDescription colorAttachment{};
    colorAttachment.format = swapchain.GetImageFormat();
    colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentDescription depthAttachment{};
    depthAttachment.format = depthFormat;
    depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkAttachmentDescription attachments[] = {colorAttachment, depthAttachment};

    VkAttachmentReference colorAttachmentRef{};
    colorAttachmentRef.attachment = 0;
    colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentReference depthAttachmentRef{};
    depthAttachmentRef.attachment = 1;
    depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorAttachmentRef;
    subpass.pDepthStencilAttachment = &depthAttachmentRef;

    VkSubpassDependency dependency{};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
                               VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependency.srcAccessMask = 0;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
                               VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
                                VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

    VkRenderPassCreateInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = 2;
    renderPassInfo.pAttachments = attachments;
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpass;
    renderPassInfo.dependencyCount = 1;
    renderPassInfo.pDependencies = &dependency;

    VkRenderPass renderPass = VK_NULL_HANDLE;
    if (vkCreateRenderPass(device, &renderPassInfo, nullptr, &renderPass) != VK_SUCCESS) {
        std::fprintf(stderr, "play: vkCreateRenderPass failed\n");
        return EXIT_FAILURE;
    }

    ForwardLitPipeline pipeline;
    if (!pipeline.Init(context, renderPass, swapchain.GetExtent(),
                        ShaderPath("m1_unlit.vert.spv").c_str(),
                        ShaderPath("m1_unlit.frag.spv").c_str())) {
        return EXIT_FAILURE;
    }

    // Material assets (post-Editor-E8) -- same second pipeline as editor/main.cpp's own
    // colorPipeline, see that file's comment.
    ForwardLitColorPipeline colorPipeline;
    if (!colorPipeline.Init(context, renderPass, swapchain.GetExtent(),
                            ShaderPath("m_material_color.vert.spv").c_str(),
                            ShaderPath("m_material_color.frag.spv").c_str())) {
        return EXIT_FAILURE;
    }

    // Material assets, "ForwardLitTexturedColor" shader (post-Editor-E8) -- same third
    // pipeline as editor/main.cpp's own texturedColorPipeline, see that file's comment.
    ForwardLitTexturedColorPipeline texturedColorPipeline;
    if (!texturedColorPipeline.Init(context, renderPass, swapchain.GetExtent(),
                                    ShaderPath("m_material_textured_color.vert.spv").c_str(),
                                    ShaderPath("m_material_textured_color.frag.spv").c_str())) {
        return EXIT_FAILURE;
    }

    // Descriptor pool + GPU texture cache for material texture properties -- same shape
    // as editor/main.cpp's own materialTextureDescriptorPool/resolveMaterialTexture, see
    // that file's comment for the full reasoning (maxSets=32 fixed cap, RHITexture owns
    // its own sampler).
    VkDescriptorPoolSize materialTexturePoolSize{};
    materialTexturePoolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    materialTexturePoolSize.descriptorCount = 32;

    VkDescriptorPoolCreateInfo materialTexturePoolInfo{};
    materialTexturePoolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    materialTexturePoolInfo.maxSets = 32;
    materialTexturePoolInfo.poolSizeCount = 1;
    materialTexturePoolInfo.pPoolSizes = &materialTexturePoolSize;

    VkDescriptorPool materialTextureDescriptorPool = VK_NULL_HANDLE;
    if (vkCreateDescriptorPool(device, &materialTexturePoolInfo, nullptr,
                               &materialTextureDescriptorPool) != VK_SUCCESS) {
        std::fprintf(stderr, "play: vkCreateDescriptorPool (material textures) failed\n");
        return EXIT_FAILURE;
    }

    struct MaterialTextureGpuData {
        RHITexture texture;
        VkDescriptorSet descriptorSet = VK_NULL_HANDLE;
    };
    std::unordered_map<AssetGuid, std::unique_ptr<MaterialTextureGpuData>> materialTextureCache;
    auto resolveMaterialTexture = [&](const AssetGuid& guid) -> MaterialTextureGpuData* {
        auto it = materialTextureCache.find(guid);
        if (it != materialTextureCache.end()) {
            return it->second.get();
        }
        auto pathIt = textureGuidToPath.find(guid);
        if (pathIt == textureGuidToPath.end()) {
            return nullptr;
        }

        const std::filesystem::path sourcePath(pathIt->second);
        const std::string cookedPath =
            CookedAssetPath((sourcePath.stem().string() + ".tex").c_str());

        engine::renderer::TextureData textureData;
        AssetGuid loadedGuid;
        if (!engine::renderer::LoadCookedTexture(cookedPath.c_str(), textureData, &loadedGuid) ||
            loadedGuid != guid) {
            return nullptr;
        }

        auto gpuData = std::make_unique<MaterialTextureGpuData>();
        if (!gpuData->texture.InitWithData(context, textureData.width, textureData.height,
                                           textureData.pixels.data(),
                                           textureData.pixels.size())) {
            return nullptr;
        }

        VkDescriptorSetLayout layout = texturedColorPipeline.GetDescriptorSetLayout();
        VkDescriptorSetAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool = materialTextureDescriptorPool;
        allocInfo.descriptorSetCount = 1;
        allocInfo.pSetLayouts = &layout;
        if (vkAllocateDescriptorSets(device, &allocInfo, &gpuData->descriptorSet) != VK_SUCCESS) {
            std::fprintf(stderr, "play: vkAllocateDescriptorSets (material texture) failed -- "
                                 "materialTextureDescriptorPool exhausted?\n");
            return nullptr;
        }

        VkDescriptorImageInfo imageInfo{};
        imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        imageInfo.imageView = gpuData->texture.GetImageView();
        imageInfo.sampler = gpuData->texture.GetSampler();

        VkWriteDescriptorSet descriptorWrite{};
        descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrite.dstSet = gpuData->descriptorSet;
        descriptorWrite.dstBinding = 0;
        descriptorWrite.descriptorCount = 1;
        descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        descriptorWrite.pImageInfo = &imageInfo;
        vkUpdateDescriptorSets(device, 1, &descriptorWrite, 0, nullptr);

        MaterialTextureGpuData* result = gpuData.get();
        materialTextureCache.emplace(guid, std::move(gpuData));
        return result;
    };

    ImGuiOverlay overlay;
    if (!overlay.Init(context, displayBackend, renderPass, swapchain.GetImageCount(),
                       swapchain.GetImageCount())) {
        return EXIT_FAILURE;
    }

    std::vector<VkFramebuffer> framebuffers(swapchain.GetImageCount());
    auto createFramebuffers = [&]() -> bool {
        for (std::size_t i = 0; i < framebuffers.size(); ++i) {
            VkImageView fbAttachments[] = {swapchain.GetImageViews()[i], depthView};

            VkFramebufferCreateInfo framebufferInfo{};
            framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
            framebufferInfo.renderPass = renderPass;
            framebufferInfo.attachmentCount = 2;
            framebufferInfo.pAttachments = fbAttachments;
            framebufferInfo.width = swapchain.GetExtent().width;
            framebufferInfo.height = swapchain.GetExtent().height;
            framebufferInfo.layers = 1;

            if (vkCreateFramebuffer(device, &framebufferInfo, nullptr, &framebuffers[i]) !=
                VK_SUCCESS) {
                std::fprintf(stderr, "play: vkCreateFramebuffer failed for image %zu\n", i);
                return false;
            }
        }
        return true;
    };
    if (!createFramebuffers()) {
        return EXIT_FAILURE;
    }

    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    poolInfo.queueFamilyIndex = context.GetGraphicsQueueFamily();

    VkCommandPool commandPool = VK_NULL_HANDLE;
    if (vkCreateCommandPool(device, &poolInfo, nullptr, &commandPool) != VK_SUCCESS) {
        std::fprintf(stderr, "play: vkCreateCommandPool failed\n");
        return EXIT_FAILURE;
    }

    VkCommandBuffer commandBuffers[kMaxFramesInFlight];
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = commandPool;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = kMaxFramesInFlight;
    if (vkAllocateCommandBuffers(device, &allocInfo, commandBuffers) != VK_SUCCESS) {
        std::fprintf(stderr, "play: vkAllocateCommandBuffers failed\n");
        return EXIT_FAILURE;
    }

    VkSemaphore imageAvailable[kMaxFramesInFlight];
    VkSemaphore renderFinished[kMaxFramesInFlight];
    VkFence inFlight[kMaxFramesInFlight];

    VkSemaphoreCreateInfo semaphoreInfo{};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    for (int i = 0; i < kMaxFramesInFlight; ++i) {
        if (vkCreateSemaphore(device, &semaphoreInfo, nullptr, &imageAvailable[i]) != VK_SUCCESS ||
            vkCreateSemaphore(device, &semaphoreInfo, nullptr, &renderFinished[i]) != VK_SUCCESS ||
            vkCreateFence(device, &fenceInfo, nullptr, &inFlight[i]) != VK_SUCCESS) {
            std::fprintf(stderr, "play: failed to create sync objects for frame %d\n", i);
            return EXIT_FAILURE;
        }
    }

    // Same keyboard-only orbit camera as editor/main.cpp -- there's no in-game/main-camera
    // concept yet (docs/07-unity-parity-analysis.md), so Play Mode reuses the Editor's own
    // free-look-less orbit rather than inventing a second, different scheme.
    Camera camera;
    camera.target = glm::vec3(0.0f, 1.0f, 0.0f);
    camera.distance = 12.0f;
    camera.pitch = 0.5f;
    glm::mat4 currentViewProj(1.0f);
    int currentFrame = 0;

    std::uint32_t framesSinceReport = 0;
    auto lastFpsReportTime = std::chrono::steady_clock::now();
    double lastReportedFps = 0.0;

    Application::Callbacks callbacks;

    callbacks.onUpdate = [&](float deltaSeconds, const engine::platform::InputState& input) {
        constexpr float kYawSpeed = 1.2f;
        constexpr float kPitchSpeed = 1.0f;
        constexpr float kZoomSpeed = 8.0f;
        constexpr float kMinPitch = -1.5f;
        constexpr float kMaxPitch = 1.5f;
        constexpr float kMinDistance = 2.0f;
        constexpr float kMaxDistance = 60.0f;

        if (input.keysHeld[static_cast<std::size_t>(Key::A)]) {
            camera.yaw -= kYawSpeed * deltaSeconds;
        }
        if (input.keysHeld[static_cast<std::size_t>(Key::D)]) {
            camera.yaw += kYawSpeed * deltaSeconds;
        }
        if (input.keysHeld[static_cast<std::size_t>(Key::W)]) {
            camera.pitch = std::min(kMaxPitch, camera.pitch + kPitchSpeed * deltaSeconds);
        }
        if (input.keysHeld[static_cast<std::size_t>(Key::S)]) {
            camera.pitch = std::max(kMinPitch, camera.pitch - kPitchSpeed * deltaSeconds);
        }
        if (input.keysHeld[static_cast<std::size_t>(Key::Up)]) {
            camera.distance = std::max(kMinDistance, camera.distance - kZoomSpeed * deltaSeconds);
        }
        if (input.keysHeld[static_cast<std::size_t>(Key::Down)]) {
            camera.distance = std::min(kMaxDistance, camera.distance + kZoomSpeed * deltaSeconds);
        }
        // Esc ends Play Mode -- the only way out other than closing the window, since
        // there's no Editor UI here to click a "Stop" button in.
        if (input.keysHeld[static_cast<std::size_t>(Key::Escape)]) {
            displayBackend.RequestQuit();
        }

        // The full phase sequence from CLAUDE.md section 4, same order as
        // m5_vertical_slice: Script phase -> barrier -> Physics phase -> barrier ->
        // Collision Callback phase -> Post-Physics. inputSystem.Update() must run before
        // any script's OnUpdate() reads it (docs/01 section 11.4: input read once per
        // frame, before Script).
        inputSystem.Update(input);
        for (const std::unique_ptr<ScriptComponent>& script : scripts) {
            script->OnUpdate(deltaSeconds);
        }

        physicsPhase.Update(physicsWorld, world, deltaSeconds);
        physicsWorld.SyncTransforms(world);
        physicsWorld.DispatchCollisionCallbacks(scriptPtrs);

        overlay.NewFrame();
        console.Update();

        ImGui::SetNextWindowPos(ImVec2(10.0f, 10.0f), ImGuiCond_FirstUseEver);
        ImGui::Begin("Play Mode");
        ImGui::Text("Scene: %s", scenePath.c_str());
        ImGui::Text("FPS: %.0f", lastReportedFps);
        ImGui::TextUnformatted("Esc or close the window to stop.");
        ImGui::End();

        if (consoleAvailable) {
            ImGui::SetNextWindowPos(ImVec2(10.0f, 110.0f), ImGuiCond_FirstUseEver);
            ImGui::SetNextWindowSize(ImVec2(700.0f, 220.0f), ImGuiCond_FirstUseEver);
            ImGui::Begin("Console");
            ImGui::Text("%zu line(s)", console.GetLines().size());
            ImGui::Separator();
            ImGui::BeginChild("ConsoleScroll", ImVec2(0.0f, 0.0f), false,
                              ImGuiWindowFlags_HorizontalScrollbar);
            for (const Console::Line& line : console.GetLines()) {
                if (line.isError) {
                    ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "%s", line.text.c_str());
                } else {
                    ImGui::TextUnformatted(line.text.c_str());
                }
            }
            if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) {
                ImGui::SetScrollHereY(1.0f);
            }
            ImGui::EndChild();
            ImGui::End();
        }

        const float aspect = static_cast<float>(swapchain.GetExtent().width) /
                              static_cast<float>(swapchain.GetExtent().height);
        currentViewProj = camera.GetProjectionMatrix(aspect) * camera.GetViewMatrix();
    };

    callbacks.onRender = [&]() {
        vkWaitForFences(device, 1, &inFlight[currentFrame], VK_TRUE, UINT64_MAX);

        std::uint32_t imageIndex = 0;
        VkResult acquireResult = vkAcquireNextImageKHR(device, swapchain.GetHandle(), UINT64_MAX,
                                                        imageAvailable[currentFrame], VK_NULL_HANDLE,
                                                        &imageIndex);
        if (acquireResult == VK_ERROR_OUT_OF_DATE_KHR) {
            vkDeviceWaitIdle(device);
            for (VkFramebuffer framebuffer : framebuffers) {
                vkDestroyFramebuffer(device, framebuffer, nullptr);
            }
            destroyDepthResources();
            swapchain.Recreate(displayBackend.GetDrawableSize());
            createDepthResources(swapchain.GetExtent());
            createFramebuffers();
            return;
        }
        if (acquireResult != VK_SUCCESS && acquireResult != VK_SUBOPTIMAL_KHR) {
            std::fprintf(stderr, "play: vkAcquireNextImageKHR failed\n");
            return;
        }

        vkResetFences(device, 1, &inFlight[currentFrame]);

        VkCommandBuffer cmd = commandBuffers[currentFrame];
        vkResetCommandBuffer(cmd, 0);

        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        vkBeginCommandBuffer(cmd, &beginInfo);

        VkClearValue clearValues[2];
        clearValues[0].color = {{0.02f, 0.02f, 0.05f, 1.0f}};
        clearValues[1].depthStencil = {1.0f, 0};

        VkRenderPassBeginInfo renderPassBeginInfo{};
        renderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderPassBeginInfo.renderPass = renderPass;
        renderPassBeginInfo.framebuffer = framebuffers[imageIndex];
        renderPassBeginInfo.renderArea.offset = {0, 0};
        renderPassBeginInfo.renderArea.extent = swapchain.GetExtent();
        renderPassBeginInfo.clearValueCount = 2;
        renderPassBeginInfo.pClearValues = clearValues;

        vkCmdBeginRenderPass(cmd, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

        // Three-pass split by material shader, same reasoning as editor/main.cpp's own
        // render loop (post-Editor-E8 material assets, ShaderPropertySchema.h) -- see
        // that file's comment.
        const auto& meshes = world.Meshes().Data();
        const auto& meshEntities = world.Meshes().Entities();

        struct MeshDrawContext {
            MeshGpuData* gpuData;
            glm::mat4 mvp;
        };
        auto resolveMeshDraw = [&](std::size_t i) -> std::optional<MeshDrawContext> {
            if (world.GetTransform(meshEntities[i]) == nullptr) {
                return std::nullopt;
            }
            MeshGpuData* gpuData = resolveMesh(meshes[i].meshGuid);
            if (gpuData == nullptr) {
                return std::nullopt;
            }
            // World::GetWorldMatrix(), not transform->GetMatrix() -- composes the parent
            // chain (post-Editor-E8 hierarchy, docs/07-unity-parity-analysis.md); for a
            // root entity (the common case, still true of every mesh in the demo scenes
            // except the one hierarchy example) this is exactly transform->GetMatrix().
            const glm::mat4 mvp = currentViewProj * world.GetWorldMatrix(meshEntities[i]);
            return MeshDrawContext{gpuData, mvp};
        };
        auto bindMeshBuffers = [&](const MeshGpuData& gpuData) {
            VkBuffer vertexBuffers[] = {gpuData.vertexBuffer.GetHandle()};
            VkDeviceSize offsets[] = {0};
            vkCmdBindVertexBuffers(cmd, 0, 1, vertexBuffers, offsets);
            vkCmdBindIndexBuffer(cmd, gpuData.indexBuffer.GetHandle(), 0, VK_INDEX_TYPE_UINT32);
        };

        // Pass 1: no material -- ForwardLitPipeline's original debug normal-color
        // visualization (M1's exit criterion, untouched).
        pipeline.Bind(cmd);
        for (std::size_t i = 0; i < meshes.size(); ++i) {
            if (meshes[i].materialGuid != engine::asset::kInvalidAssetGuid) {
                continue;
            }
            std::optional<MeshDrawContext> draw = resolveMeshDraw(i);
            if (!draw) {
                continue;
            }
            bindMeshBuffers(*draw->gpuData);
            pipeline.PushModelViewProjection(cmd, draw->mvp);
            vkCmdDrawIndexed(cmd, draw->gpuData->indexCount, 1, 0, 0, 0);
        }

        // Pass 2: material targeting "ForwardLitColor" -- flat tint only.
        colorPipeline.Bind(cmd);
        for (std::size_t i = 0; i < meshes.size(); ++i) {
            if (meshes[i].materialGuid == engine::asset::kInvalidAssetGuid) {
                continue;
            }
            MaterialData* material = resolveMaterial(meshes[i].materialGuid);
            if (material == nullptr || material->shaderName != "ForwardLitColor") {
                continue;
            }
            std::optional<MeshDrawContext> draw = resolveMeshDraw(i);
            if (!draw) {
                continue;
            }
            bindMeshBuffers(*draw->gpuData);
            const glm::vec4 tint = material->GetColor("tintColor", glm::vec4(1.0f));
            colorPipeline.PushMvpAndTint(cmd, draw->mvp, tint);
            vkCmdDrawIndexed(cmd, draw->gpuData->indexCount, 1, 0, 0, 0);
        }

        // Pass 3: material targeting "ForwardLitTexturedColor" -- albedo texture * tint.
        texturedColorPipeline.Bind(cmd);
        for (std::size_t i = 0; i < meshes.size(); ++i) {
            if (meshes[i].materialGuid == engine::asset::kInvalidAssetGuid) {
                continue;
            }
            MaterialData* material = resolveMaterial(meshes[i].materialGuid);
            if (material == nullptr || material->shaderName != "ForwardLitTexturedColor") {
                continue;
            }
            const AssetGuid textureGuid =
                material->GetTexture("albedoTexture", engine::asset::kInvalidAssetGuid);
            auto* textureGpuData = resolveMaterialTexture(textureGuid);
            if (textureGpuData == nullptr) {
                continue;
            }
            std::optional<MeshDrawContext> draw = resolveMeshDraw(i);
            if (!draw) {
                continue;
            }
            bindMeshBuffers(*draw->gpuData);
            texturedColorPipeline.BindDescriptorSet(cmd, textureGpuData->descriptorSet);
            const glm::vec4 tint = material->GetColor("tintColor", glm::vec4(1.0f));
            texturedColorPipeline.PushMvpAndTint(cmd, draw->mvp, tint);
            vkCmdDrawIndexed(cmd, draw->gpuData->indexCount, 1, 0, 0, 0);
        }

        overlay.Render(cmd);

        vkCmdEndRenderPass(cmd);
        vkEndCommandBuffer(cmd);

        VkSemaphore waitSemaphores[] = {imageAvailable[currentFrame]};
        VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
        VkSemaphore signalSemaphores[] = {renderFinished[currentFrame]};

        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.waitSemaphoreCount = 1;
        submitInfo.pWaitSemaphores = waitSemaphores;
        submitInfo.pWaitDstStageMask = waitStages;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &cmd;
        submitInfo.signalSemaphoreCount = 1;
        submitInfo.pSignalSemaphores = signalSemaphores;

        if (vkQueueSubmit(context.GetGraphicsQueue(), 1, &submitInfo, inFlight[currentFrame]) !=
            VK_SUCCESS) {
            std::fprintf(stderr, "play: vkQueueSubmit failed\n");
            return;
        }

        VkSwapchainKHR swapchains[] = {swapchain.GetHandle()};
        VkPresentInfoKHR presentInfo{};
        presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        presentInfo.waitSemaphoreCount = 1;
        presentInfo.pWaitSemaphores = signalSemaphores;
        presentInfo.swapchainCount = 1;
        presentInfo.pSwapchains = swapchains;
        presentInfo.pImageIndices = &imageIndex;

        VkResult presentResult = vkQueuePresentKHR(context.GetPresentQueue(), &presentInfo);
        if (presentResult == VK_ERROR_OUT_OF_DATE_KHR || presentResult == VK_SUBOPTIMAL_KHR) {
            vkDeviceWaitIdle(device);
            for (VkFramebuffer framebuffer : framebuffers) {
                vkDestroyFramebuffer(device, framebuffer, nullptr);
            }
            destroyDepthResources();
            swapchain.Recreate(displayBackend.GetDrawableSize());
            createDepthResources(swapchain.GetExtent());
            createFramebuffers();
        } else if (presentResult != VK_SUCCESS) {
            std::fprintf(stderr, "play: vkQueuePresentKHR failed\n");
            return;
        }

        currentFrame = (currentFrame + 1) % kMaxFramesInFlight;

        ++framesSinceReport;
        const auto now = std::chrono::steady_clock::now();
        const std::chrono::duration<double> elapsed = now - lastFpsReportTime;
        if (elapsed.count() >= 0.5) {
            lastReportedFps = static_cast<double>(framesSinceReport) / elapsed.count();
            char title[96];
            std::snprintf(title, sizeof(title), "Pi-Engine -- Play Mode (%.0f FPS)", lastReportedFps);
            displayBackend.SetWindowTitle(title);
            framesSinceReport = 0;
            lastFpsReportTime = now;
        }
    };

    std::printf("play: running, Esc or close the window to stop.\n");
    Application application;
    application.Run(displayBackend, callbacks);

    vkDeviceWaitIdle(device);

    for (const std::unique_ptr<ScriptComponent>& script : scripts) {
        script->OnDestroy();
    }

    for (int i = 0; i < kMaxFramesInFlight; ++i) {
        vkDestroySemaphore(device, imageAvailable[i], nullptr);
        vkDestroySemaphore(device, renderFinished[i], nullptr);
        vkDestroyFence(device, inFlight[i], nullptr);
    }
    vkDestroyCommandPool(device, commandPool, nullptr);
    for (VkFramebuffer framebuffer : framebuffers) {
        vkDestroyFramebuffer(device, framebuffer, nullptr);
    }
    overlay.Shutdown();
    pipeline.Shutdown();
    colorPipeline.Shutdown();
    texturedColorPipeline.Shutdown();
    destroyDepthResources();
    vkDestroyRenderPass(device, renderPass, nullptr);

    // destroys every RHITexture -- must happen before context.Shutdown(), same reasoning
    // as meshCache.clear() below.
    materialTextureCache.clear();
    // also frees every descriptor set allocated from it.
    vkDestroyDescriptorPool(device, materialTextureDescriptorPool, nullptr);
    meshCache.clear();
    swapchain.Shutdown();
    context.Shutdown();
    displayBackend.Shutdown();

    std::printf("play: clean exit.\n");
    console.Shutdown();
    return EXIT_SUCCESS;
}
