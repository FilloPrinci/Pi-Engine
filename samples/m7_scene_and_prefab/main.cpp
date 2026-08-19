#include "engine/asset/AssetGuid.h"
#include "engine/core/Application.h"
#include "engine/core/Camera.h"
#include "engine/core/EngineVersion.h"
#include "engine/ecs/World.h"
#include "engine/jobs/JobSystem.h"
#include "engine/physics/PhysicsPhase.h"
#include "engine/physics/PhysicsWorld.h"
#include "engine/renderer/CookedMesh.h"
#include "engine/renderer/ForwardLitPipeline.h"
#include "engine/rhi/RHIBuffer.h"
#include "engine/rhi/RHIContext.h"
#include "engine/rhi/RHISwapchain.h"
#include "engine/platform/SDL2DisplayBackend.h"
#include "engine/scene/Prefab.h"
#include "engine/scene/Scene.h"

#include <volk.h>

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

using engine::asset::AssetGuid;
using engine::core::Application;
using engine::core::Camera;
using engine::ecs::Entity;
using engine::ecs::TransformComponent;
using engine::ecs::World;
using engine::jobs::JobSystem;
using engine::physics::PhysicsPhase;
using engine::physics::PhysicsWorld;
using engine::platform::SDL2DisplayBackend;
using engine::renderer::ForwardLitPipeline;
using engine::renderer::MeshData;
using engine::rhi::RHIBuffer;
using engine::rhi::RHIContext;
using engine::rhi::RHISwapchain;
using engine::scene::Prefab;

namespace {

constexpr int kMaxFramesInFlight = 2;

std::string ShaderPath(const char* fileName) {
    return std::string(PI_ENGINE_SHADER_DIR) + "/" + fileName;
}

std::string CookedAssetPath(const char* fileName) {
    return std::string(PI_ENGINE_COOKED_ASSET_DIR) + "/" + fileName;
}

std::string SampleAssetPath(const char* fileName) {
    return std::string(PI_ENGINE_SAMPLE_ASSET_DIR) + "/" + fileName;
}

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

// One entry per distinct mesh GUID a scene/prefab actually references (M7, docs/01
// section 12.3) -- this sample only ever has one (m1_cube.mesh), but the lookup is by
// GUID, not by a hardcoded filename, so it's already shaped the way a real multi-mesh
// scene would use it. unique_ptr because RHIBuffer is neither copyable nor movable
// (see that class), so it can't live in the map's value directly.
struct MeshGpuData {
    RHIBuffer vertexBuffer;
    RHIBuffer indexBuffer;
    std::uint32_t indexCount = 0;
};

} // namespace

// Milestone M7 (1 of 5 Asset Pipeline steps) -- Scene/Prefab (docs/01 sections 12.2, 13).
// Exit criterion (informal, this is post-vertical-slice work, not part of the numbered
// M0-M5 roadmap): a scene JSON file and a prefab JSON file both load real ECS + physics
// entities, resolved by Asset GUID instead of hardcoded paths -- no C++ code describes
// *where* anything is, only *how* to load/instantiate what the JSON describes.
//
// Scene: a static ground slab (assets/level.scene.json). Prefab: a dynamic cube
// (assets/cube.prefab.json), instantiated three times at different X positions above the
// ground -- three independent falling cubes from one prefab file, physically simulated
// (same PhysicsWorld/PhysicsPhase pattern as M4), not scripted (script attachment from a
// scene/prefab document is explicitly out of scope for this step, see
// engine/scene/README.md).
int main(int /*argc*/, char** /*argv*/) {
    std::printf("Pi-Engine %s -- m7_scene_and_prefab\n", engine::core::GetEngineVersionString());

    SDL2DisplayBackend displayBackend;
    if (!displayBackend.Init()) {
        return EXIT_FAILURE;
    }

    RHIContext context;
    if (!context.Init(displayBackend, "m7_scene_and_prefab")) {
        return EXIT_FAILURE;
    }

    RHISwapchain swapchain;
    if (!swapchain.Init(context, displayBackend.GetDrawableSize())) {
        return EXIT_FAILURE;
    }

    VkDevice device = context.GetDevice();

    // --- Mesh cache: resolves a MeshComponent::meshGuid to already-uploaded GPU buffers,
    // loading+uploading the first time a GUID is seen. ---
    std::unordered_map<AssetGuid, std::unique_ptr<MeshGpuData>> meshCache;
    auto resolveMesh = [&](const AssetGuid& guid) -> MeshGpuData* {
        auto it = meshCache.find(guid);
        if (it != meshCache.end()) {
            return it->second.get();
        }

        // No GUID -> cooked-path manifest yet (deferred, engine/asset/README.md) -- this
        // sample only ever cooks one mesh, so it just tries the one file it knows about
        // and lets LoadCookedMesh's own guid check (docs/01 section 12.3) confirm it's
        // really the asset being asked for.
        MeshData meshData;
        AssetGuid loadedGuid;
        if (!engine::renderer::LoadCookedMesh(CookedAssetPath("m1_cube.mesh").c_str(), meshData,
                                              &loadedGuid) ||
            loadedGuid != guid) {
            return nullptr;
        }

        auto gpuData = std::make_unique<MeshGpuData>();
        if (!gpuData->vertexBuffer.InitWithData(context, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                                                meshData.vertices.data(),
                                                meshData.vertices.size() * sizeof(meshData.vertices[0])) ||
            !gpuData->indexBuffer.InitWithData(context, VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                                               meshData.indices.data(),
                                               meshData.indices.size() * sizeof(meshData.indices[0]))) {
            return nullptr;
        }
        gpuData->indexCount = static_cast<std::uint32_t>(meshData.indices.size());

        MeshGpuData* result = gpuData.get();
        meshCache.emplace(guid, std::move(gpuData));
        return result;
    };

    // --- Scene: ECS world + Job System + Physics World (docs/03 section 9). ---
    World world;

    JobSystem jobSystem;
    if (!jobSystem.Init()) {
        std::fprintf(stderr, "m7_scene_and_prefab: JobSystem::Init failed\n");
        return EXIT_FAILURE;
    }
    std::printf("m7_scene_and_prefab: JobSystem started with %u worker thread(s)\n",
                jobSystem.GetWorkerCount());

    PhysicsWorld physicsWorld;
    if (!physicsWorld.Init(jobSystem)) {
        std::fprintf(stderr, "m7_scene_and_prefab: PhysicsWorld::Init failed\n");
        return EXIT_FAILURE;
    }
    PhysicsPhase physicsPhase;

    // The one place PhysicsWorld::CreateBody actually gets called from -- engine/scene/
    // itself never references physics/ at all (SceneDocument.h's own comment explains
    // why: a callback here instead of a physics::PhysicsWorld* keeps that module free of
    // Jolt even at link time, not just compile time).
    const auto createPhysicsBody = [&](World& w, Entity e, bool isStatic) {
        physicsWorld.CreateBody(w, e, isStatic);
    };

    if (!engine::scene::LoadScene(SampleAssetPath("level.scene.json").c_str(), world,
                                  createPhysicsBody)) {
        std::fprintf(stderr, "m7_scene_and_prefab: failed to load level.scene.json\n");
        return EXIT_FAILURE;
    }

    Prefab cubePrefab;
    if (!cubePrefab.Load(SampleAssetPath("cube.prefab.json").c_str())) {
        std::fprintf(stderr, "m7_scene_and_prefab: failed to load cube.prefab.json\n");
        return EXIT_FAILURE;
    }
    for (float x : {-3.0f, 0.0f, 3.0f}) {
        cubePrefab.Instantiate(world, glm::vec3(x, 5.0f, 0.0f), createPhysicsBody);
    }
    std::printf("m7_scene_and_prefab: loaded 1 scene entity + 3 prefab instances\n");

    // --- Depth buffer (same pattern as M1-M6). ---
    const VkFormat depthFormat = ChooseDepthFormat(context.GetPhysicalDevice());
    if (depthFormat == VK_FORMAT_UNDEFINED) {
        std::fprintf(stderr, "m7_scene_and_prefab: no supported depth/stencil format found\n");
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
            std::fprintf(stderr, "m7_scene_and_prefab: vmaCreateImage (depth) failed\n");
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
            std::fprintf(stderr, "m7_scene_and_prefab: vkCreateImageView (depth) failed\n");
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

    // --- Render pass: color + depth (same as M1-M6). ---
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
        std::fprintf(stderr, "m7_scene_and_prefab: vkCreateRenderPass failed\n");
        return EXIT_FAILURE;
    }

    ForwardLitPipeline pipeline;
    if (!pipeline.Init(context, renderPass, swapchain.GetExtent(),
                        ShaderPath("m1_unlit.vert.spv").c_str(),
                        ShaderPath("m1_unlit.frag.spv").c_str())) {
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
                std::fprintf(stderr, "m7_scene_and_prefab: vkCreateFramebuffer failed for image %zu\n",
                              i);
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
        std::fprintf(stderr, "m7_scene_and_prefab: vkCreateCommandPool failed\n");
        return EXIT_FAILURE;
    }

    VkCommandBuffer commandBuffers[kMaxFramesInFlight];
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = commandPool;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = kMaxFramesInFlight;
    if (vkAllocateCommandBuffers(device, &allocInfo, commandBuffers) != VK_SUCCESS) {
        std::fprintf(stderr, "m7_scene_and_prefab: vkAllocateCommandBuffers failed\n");
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
            std::fprintf(stderr, "m7_scene_and_prefab: failed to create sync objects for frame %d\n",
                         i);
            return EXIT_FAILURE;
        }
    }

    // --- Per-frame state shared between Application's onUpdate and onRender callbacks. ---
    Camera camera;
    camera.target = glm::vec3(0.0f, 1.0f, 0.0f);
    camera.distance = 16.0f;
    camera.pitch = 0.5f;
    glm::mat4 currentViewProj(1.0f);
    int currentFrame = 0;

    std::uint32_t framesSinceReport = 0;
    auto lastFpsReportTime = std::chrono::steady_clock::now();

    Application::Callbacks callbacks;

    callbacks.onUpdate = [&](float deltaSeconds, const engine::platform::InputState& /*input*/) {
        physicsPhase.Update(physicsWorld, world, deltaSeconds);
        physicsWorld.SyncTransforms(world);
        physicsWorld.DispatchCollisionCallbacks({}); // no scripts in this sample

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
            std::fprintf(stderr, "m7_scene_and_prefab: vkAcquireNextImageKHR failed\n");
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
        pipeline.Bind(cmd);

        // Every entity with both a Transform and a Mesh gets drawn, resolving its mesh by
        // GUID through the cache above (M7) -- unlike M1-M6, nothing here hardcodes "the
        // one shared cube", it just happens that every entity in these particular scene/
        // prefab files references the same GUID.
        const auto& meshes = world.Meshes().Data();
        const auto& meshEntities = world.Meshes().Entities();
        for (std::size_t i = 0; i < meshes.size(); ++i) {
            const TransformComponent* transform = world.GetTransform(meshEntities[i]);
            if (transform == nullptr) {
                continue;
            }
            MeshGpuData* gpuData = resolveMesh(meshes[i].meshGuid);
            if (gpuData == nullptr) {
                continue;
            }

            VkBuffer vertexBuffers[] = {gpuData->vertexBuffer.GetHandle()};
            VkDeviceSize offsets[] = {0};
            vkCmdBindVertexBuffers(cmd, 0, 1, vertexBuffers, offsets);
            vkCmdBindIndexBuffer(cmd, gpuData->indexBuffer.GetHandle(), 0, VK_INDEX_TYPE_UINT32);

            const glm::mat4 mvp = currentViewProj * transform->GetMatrix();
            pipeline.PushModelViewProjection(cmd, mvp);
            vkCmdDrawIndexed(cmd, gpuData->indexCount, 1, 0, 0, 0);
        }

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
            std::fprintf(stderr, "m7_scene_and_prefab: vkQueueSubmit failed\n");
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
            std::fprintf(stderr, "m7_scene_and_prefab: vkQueuePresentKHR failed\n");
            return;
        }

        currentFrame = (currentFrame + 1) % kMaxFramesInFlight;

        ++framesSinceReport;
        const auto now = std::chrono::steady_clock::now();
        const std::chrono::duration<double> elapsed = now - lastFpsReportTime;
        if (elapsed.count() >= 0.5) {
            const double fps = static_cast<double>(framesSinceReport) / elapsed.count();
            char title[96];
            std::snprintf(title, sizeof(title), "Pi-Engine -- m7_scene_and_prefab (%.0f FPS)", fps);
            displayBackend.SetWindowTitle(title);
            framesSinceReport = 0;
            lastFpsReportTime = now;
        }
    };

    std::printf("m7_scene_and_prefab: running, close the window to exit.\n");
    Application application;
    application.Run(displayBackend, callbacks);

    vkDeviceWaitIdle(device);

    for (int i = 0; i < kMaxFramesInFlight; ++i) {
        vkDestroySemaphore(device, imageAvailable[i], nullptr);
        vkDestroySemaphore(device, renderFinished[i], nullptr);
        vkDestroyFence(device, inFlight[i], nullptr);
    }
    vkDestroyCommandPool(device, commandPool, nullptr);
    for (VkFramebuffer framebuffer : framebuffers) {
        vkDestroyFramebuffer(device, framebuffer, nullptr);
    }
    pipeline.Shutdown();
    destroyDepthResources();
    vkDestroyRenderPass(device, renderPass, nullptr);

    physicsWorld.Shutdown();
    jobSystem.Shutdown();
    meshCache.clear(); // destroys every RHIBuffer -- must happen before context.Shutdown()
    swapchain.Shutdown();
    context.Shutdown();
    displayBackend.Shutdown();

    std::printf("m7_scene_and_prefab: clean exit.\n");
    return EXIT_SUCCESS;
}
