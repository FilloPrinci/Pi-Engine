#include "engine/core/Application.h"
#include "engine/core/Camera.h"
#include "engine/core/EngineVersion.h"
#include "engine/ecs/World.h"
#include "engine/jobs/JobSystem.h"
#include "engine/platform/SDL2DisplayBackend.h"
#include "engine/renderer/ForwardLitPipeline.h"
#include "engine/renderer/FrustumCuller.h"
#include "engine/renderer/MeshLoader.h"
#include "engine/rhi/RHIBuffer.h"
#include "engine/rhi/RHIContext.h"
#include "engine/rhi/RHISwapchain.h"

#include <volk.h>

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

using engine::core::Application;
using engine::core::Camera;
using engine::ecs::Entity;
using engine::ecs::MeshComponent;
using engine::ecs::TransformComponent;
using engine::ecs::World;
using engine::jobs::JobSystem;
using engine::platform::SDL2DisplayBackend;
using engine::renderer::ForwardLitPipeline;
using engine::renderer::FrustumCuller;
using engine::renderer::MeshData;
using engine::rhi::RHIBuffer;
using engine::rhi::RHIContext;
using engine::rhi::RHISwapchain;

namespace {

constexpr int kMaxFramesInFlight = 2;

// Unit cube (assets/m1_cube.glb, -0.5..0.5 per axis): circumscribed-sphere radius.
constexpr float kCubeBoundsRadius = 0.8660254f; // sqrt(0.5*0.5*3)

std::string ShaderPath(const char* fileName) {
    return std::string(PI_ENGINE_SHADER_DIR) + "/" + fileName;
}

std::string AssetPath(const char* fileName) {
    return std::string(PI_ENGINE_ASSET_DIR) + "/" + fileName;
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

// Populates `world` with a grid of cube entities (docs/03 section 7: "a handful of
// objects"), spread wide enough that the orbiting camera's frustum only ever covers part
// of it -- FrustumCuller has something real to cull.
void PopulateScene(World& world, int gridSize, float spacing) {
    const float half = static_cast<float>(gridSize - 1) * spacing * 0.5f;
    for (int x = 0; x < gridSize; ++x) {
        for (int z = 0; z < gridSize; ++z) {
            const Entity entity = world.CreateEntity();

            TransformComponent transform;
            transform.position =
                glm::vec3(static_cast<float>(x) * spacing - half, 0.0f,
                          static_cast<float>(z) * spacing - half);
            world.AddTransform(entity, transform);

            MeshComponent mesh;
            mesh.boundsRadius = kCubeBoundsRadius;
            world.AddMesh(entity, mesh);
        }
    }
}

} // namespace

// Milestone M2 -- Hello Scene (docs/02 section 4, docs/03 section 7).
// Exit criterion: a handful of objects in the scene, frustum culling active; minimal ECS
// (Transform + Mesh) working; first real use of the Job System (parallel culling).
//
// First sample to use core::Application for its frame loop (docs/03 section 7: "every
// milestone from here adds a phase to its loop") instead of a raw while(true) like
// M0/M1 -- Poll Input is now Application's job, this sample only supplies onUpdate
// (camera orbit + frustum cull) and onRender (the same per-frame Vulkan submission
// pattern as M1, now looping over every *visible* entity instead of one hardcoded cube).
int main(int /*argc*/, char** /*argv*/) {
    std::printf("Pi-Engine %s -- m2_hello_scene\n", engine::core::GetEngineVersionString());

    SDL2DisplayBackend displayBackend;
    if (!displayBackend.Init()) {
        return EXIT_FAILURE;
    }

    RHIContext context;
    if (!context.Init(displayBackend, "m2_hello_scene")) {
        return EXIT_FAILURE;
    }

    RHISwapchain swapchain;
    if (!swapchain.Init(context, displayBackend.GetDrawableSize())) {
        return EXIT_FAILURE;
    }

    VkDevice device = context.GetDevice();

    // --- Mesh: same shared cube as M1, instanced across every entity in the scene. ---
    MeshData mesh;
    if (!engine::renderer::LoadMesh(AssetPath("m1_cube.glb").c_str(), mesh)) {
        return EXIT_FAILURE;
    }

    RHIBuffer vertexBuffer;
    if (!vertexBuffer.InitWithData(context, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, mesh.vertices.data(),
                                    mesh.vertices.size() * sizeof(mesh.vertices[0]))) {
        return EXIT_FAILURE;
    }

    RHIBuffer indexBuffer;
    if (!indexBuffer.InitWithData(context, VK_BUFFER_USAGE_INDEX_BUFFER_BIT, mesh.indices.data(),
                                   mesh.indices.size() * sizeof(mesh.indices[0]))) {
        return EXIT_FAILURE;
    }

    // --- Scene: ECS world + Job System (docs/03 section 7). ---
    World world;
    PopulateScene(world, /*gridSize=*/8, /*spacing=*/3.0f);
    const std::size_t entityCount = world.Meshes().Size();
    std::printf("m2_hello_scene: populated %zu entities\n", entityCount);

    JobSystem jobSystem;
    if (!jobSystem.Init()) {
        std::fprintf(stderr, "m2_hello_scene: JobSystem::Init failed\n");
        return EXIT_FAILURE;
    }
    std::printf("m2_hello_scene: JobSystem started with %u worker thread(s)\n",
                jobSystem.GetWorkerCount());

    // --- Depth buffer (same pattern as M1). ---
    const VkFormat depthFormat = ChooseDepthFormat(context.GetPhysicalDevice());
    if (depthFormat == VK_FORMAT_UNDEFINED) {
        std::fprintf(stderr, "m2_hello_scene: no supported depth/stencil format found\n");
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
            std::fprintf(stderr, "m2_hello_scene: vmaCreateImage (depth) failed\n");
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
            std::fprintf(stderr, "m2_hello_scene: vkCreateImageView (depth) failed\n");
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

    // --- Render pass: color + depth (same as M1). ---
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
        std::fprintf(stderr, "m2_hello_scene: vkCreateRenderPass failed\n");
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
                std::fprintf(stderr, "m2_hello_scene: vkCreateFramebuffer failed for image %zu\n", i);
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
        std::fprintf(stderr, "m2_hello_scene: vkCreateCommandPool failed\n");
        return EXIT_FAILURE;
    }

    VkCommandBuffer commandBuffers[kMaxFramesInFlight];
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = commandPool;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = kMaxFramesInFlight;
    if (vkAllocateCommandBuffers(device, &allocInfo, commandBuffers) != VK_SUCCESS) {
        std::fprintf(stderr, "m2_hello_scene: vkAllocateCommandBuffers failed\n");
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
            std::fprintf(stderr, "m2_hello_scene: failed to create sync objects for frame %d\n", i);
            return EXIT_FAILURE;
        }
    }

    // --- Per-frame state shared between Application's onUpdate and onRender callbacks. ---
    Camera camera;
    camera.distance = 16.0f;
    glm::mat4 currentViewProj(1.0f);
    std::size_t visibleCount = 0;
    int currentFrame = 0;

    std::uint32_t framesSinceReport = 0;
    auto lastFpsReportTime = std::chrono::steady_clock::now();

    Application::Callbacks callbacks;

    callbacks.onUpdate = [&](float deltaSeconds) {
        // No InputSystem until M3: the camera orbits on its own, sweeping the frustum
        // across the grid so the visible count actually changes over time.
        constexpr float kOrbitRadiansPerSecond = 0.4f;
        camera.yaw += kOrbitRadiansPerSecond * deltaSeconds;

        const float aspect = static_cast<float>(swapchain.GetExtent().width) /
                              static_cast<float>(swapchain.GetExtent().height);
        currentViewProj = camera.GetProjectionMatrix(aspect) * camera.GetViewMatrix();

        const auto frustumPlanes = FrustumCuller::ExtractFrustumPlanes(currentViewProj);
        FrustumCuller::Cull(world, jobSystem, frustumPlanes);

        visibleCount = 0;
        for (const MeshComponent& meshComponent : world.Meshes().Data()) {
            if (meshComponent.visible) {
                ++visibleCount;
            }
        }
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
            std::fprintf(stderr, "m2_hello_scene: vkAcquireNextImageKHR failed\n");
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

        VkBuffer vertexBuffers[] = {vertexBuffer.GetHandle()};
        VkDeviceSize offsets[] = {0};
        vkCmdBindVertexBuffers(cmd, 0, 1, vertexBuffers, offsets);
        vkCmdBindIndexBuffer(cmd, indexBuffer.GetHandle(), 0, VK_INDEX_TYPE_UINT32);

        const auto& meshes = world.Meshes().Data();
        const auto& meshEntities = world.Meshes().Entities();
        for (std::size_t i = 0; i < meshes.size(); ++i) {
            if (!meshes[i].visible) {
                continue;
            }
            const TransformComponent* transform = world.GetTransform(meshEntities[i]);
            if (transform == nullptr) {
                continue;
            }
            const glm::mat4 mvp = currentViewProj * transform->GetMatrix();
            pipeline.PushModelViewProjection(cmd, mvp);
            vkCmdDrawIndexed(cmd, static_cast<std::uint32_t>(mesh.indices.size()), 1, 0, 0, 0);
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
            std::fprintf(stderr, "m2_hello_scene: vkQueueSubmit failed\n");
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
            std::fprintf(stderr, "m2_hello_scene: vkQueuePresentKHR failed\n");
            return;
        }

        currentFrame = (currentFrame + 1) % kMaxFramesInFlight;

        // FPS + visible-entity counter in the window title -- same stopgap as M0/M1 ahead
        // of the real Dear ImGui debug overlay (docs/01 section 4, module 4, useful "from
        // M2" per that section -- deferred here since M2's exit criterion doesn't need it
        // and it's a substantial addition of its own).
        ++framesSinceReport;
        const auto now = std::chrono::steady_clock::now();
        const std::chrono::duration<double> elapsed = now - lastFpsReportTime;
        if (elapsed.count() >= 0.5) {
            const double fps = static_cast<double>(framesSinceReport) / elapsed.count();
            char title[96];
            std::snprintf(title, sizeof(title), "Pi-Engine -- m2_hello_scene (%.0f FPS, %zu/%zu visible)",
                          fps, visibleCount, entityCount);
            displayBackend.SetWindowTitle(title);
            framesSinceReport = 0;
            lastFpsReportTime = now;
        }
    };

    std::printf("m2_hello_scene: running, close the window to exit.\n");
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

    jobSystem.Shutdown();
    indexBuffer.Shutdown();
    vertexBuffer.Shutdown();
    swapchain.Shutdown();
    context.Shutdown();
    displayBackend.Shutdown();

    std::printf("m2_hello_scene: clean exit.\n");
    return EXIT_SUCCESS;
}
