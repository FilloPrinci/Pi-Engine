#include "engine/asset/AssetGuid.h"
#include "engine/core/Application.h"
#include "engine/core/Camera.h"
#include "engine/core/EngineVersion.h"
#include "engine/debug/Console.h"
#include "engine/debug/ImGuiOverlay.h"
#include "engine/ecs/World.h"
#include "engine/platform/SDL2DisplayBackend.h"
#include "engine/renderer/CookedMesh.h"
#include "engine/renderer/ForwardLitPipeline.h"
#include "engine/rhi/RHIBuffer.h"
#include "engine/rhi/RHIContext.h"
#include "engine/rhi/RHISwapchain.h"
#include "engine/scene/Scene.h"

#include <imgui.h>
#include <volk.h>

#include <algorithm>
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
using engine::debug::Console;
using engine::debug::ImGuiOverlay;
using engine::ecs::ColliderComponent;
using engine::ecs::Entity;
using engine::ecs::TransformComponent;
using engine::ecs::World;
using engine::platform::Key;
using engine::platform::SDL2DisplayBackend;
using engine::renderer::ForwardLitPipeline;
using engine::renderer::MeshData;
using engine::rhi::RHIBuffer;
using engine::rhi::RHIContext;
using engine::rhi::RHISwapchain;

namespace {

constexpr int kMaxFramesInFlight = 2;

std::string ShaderPath(const char* fileName) {
    return std::string(PI_ENGINE_SHADER_DIR) + "/" + fileName;
}

std::string CookedAssetPath(const char* fileName) {
    return std::string(PI_ENGINE_COOKED_ASSET_DIR) + "/" + fileName;
}

std::string EditorAssetPath(const char* fileName) {
    return std::string(PI_ENGINE_EDITOR_ASSET_DIR) + "/" + fileName;
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

// Same GUID -> GPU-buffers cache pattern as samples/m7_scene_and_prefab/main.cpp -- see
// that file's comment for why this is a map keyed by GUID (not a hardcoded filename) even
// though, like that sample, this demo scene only ever references one mesh. No GUID ->
// cooked-path manifest yet (engine/asset/README.md) -- a real Asset Browser (Editor step
// E6) will need one; this is a placeholder until then.
struct MeshGpuData {
    RHIBuffer vertexBuffer;
    RHIBuffer indexBuffer;
    std::uint32_t indexCount = 0;
};

} // namespace

// Editor step E2 -- editor/ app skeleton + Scene View (docs/06-editor-roadmap.md). Exit
// criterion: a separate application, client of engine_core, that loads an existing
// .scene.json (read-only -- no physics callback, see engine::scene::LoadScene's own
// comment) and renders it through the same RHI/ForwardLitPipeline every sample uses, with
// a keyboard-navigable camera. Also wires up engine::debug::ImGuiOverlay (built in E1) for
// a small info window -- the foundation E3's Inspector panel builds its real UI on top of.
//
// Usage: `editor [path/to/scene.json]` -- defaults to editor/assets/demo.scene.json if no
// path is given (no Project Hub yet to remember "the last opened project", Editor step E7).
int main(int argc, char** argv) {
    // Console::Init() must run before any other stdout/stderr I/O (see its own comment on
    // why) -- literally the first statement in main(), ahead of even the version printf
    // below. Not fatal if it fails (non-POSIX platform, or the redirect itself failed):
    // the Console panel just stays empty/disabled, everything else still runs normally
    // and still prints to the real terminal (see Console.h's "degrade, don't hard-fail").
    Console console;
    const bool consoleAvailable = console.Init();

    std::printf("Pi-Engine %s -- Editor\n", engine::core::GetEngineVersionString());
    if (!consoleAvailable) {
        std::fprintf(stderr, "editor: Console capture not available on this platform -- the "
                             "Console panel will stay empty (output still reaches this "
                             "terminal normally)\n");
    }

    const std::string scenePath = argc > 1 ? argv[1] : EditorAssetPath("demo.scene.json");

    SDL2DisplayBackend displayBackend;
    if (!displayBackend.Init()) {
        return EXIT_FAILURE;
    }

    RHIContext context;
    if (!context.Init(displayBackend, "Pi-Engine Editor")) {
        return EXIT_FAILURE;
    }

    RHISwapchain swapchain;
    if (!swapchain.Init(context, displayBackend.GetDrawableSize())) {
        return EXIT_FAILURE;
    }

    VkDevice device = context.GetDevice();

    // --- Mesh cache (see MeshGpuData's own comment). ---
    std::unordered_map<AssetGuid, std::unique_ptr<MeshGpuData>> meshCache;
    auto resolveMesh = [&](const AssetGuid& guid) -> MeshGpuData* {
        auto it = meshCache.find(guid);
        if (it != meshCache.end()) {
            return it->second.get();
        }

        MeshData meshData;
        AssetGuid loadedGuid;
        if (!engine::renderer::LoadCookedMesh(CookedAssetPath("m1_cube.mesh").c_str(), meshData,
                                              &loadedGuid) ||
            loadedGuid != guid) {
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

    // --- Scene: ECS world only -- no JobSystem/PhysicsWorld yet (read-only Scene View,
    //     see this file's own comment). ---
    World world;
    if (!engine::scene::LoadScene(scenePath.c_str(), world)) {
        std::fprintf(stderr, "editor: failed to load scene \"%s\"\n", scenePath.c_str());
        return EXIT_FAILURE;
    }
    std::printf("editor: loaded scene \"%s\" (%zu mesh entities)\n", scenePath.c_str(),
                world.Meshes().Data().size());

    // --- Depth buffer (same pattern as every sample since M1). ---
    const VkFormat depthFormat = ChooseDepthFormat(context.GetPhysicalDevice());
    if (depthFormat == VK_FORMAT_UNDEFINED) {
        std::fprintf(stderr, "editor: no supported depth/stencil format found\n");
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
            std::fprintf(stderr, "editor: vmaCreateImage (depth) failed\n");
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
            std::fprintf(stderr, "editor: vkCreateImageView (depth) failed\n");
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

    // --- Render pass: color + depth (same as every sample since M1). ImGuiOverlay draws
    //     into this same render pass/subpass. ---
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
        std::fprintf(stderr, "editor: vkCreateRenderPass failed\n");
        return EXIT_FAILURE;
    }

    ForwardLitPipeline pipeline;
    if (!pipeline.Init(context, renderPass, swapchain.GetExtent(),
                        ShaderPath("m1_unlit.vert.spv").c_str(),
                        ShaderPath("m1_unlit.frag.spv").c_str())) {
        return EXIT_FAILURE;
    }

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
                std::fprintf(stderr, "editor: vkCreateFramebuffer failed for image %zu\n", i);
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
        std::fprintf(stderr, "editor: vkCreateCommandPool failed\n");
        return EXIT_FAILURE;
    }

    VkCommandBuffer commandBuffers[kMaxFramesInFlight];
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = commandPool;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = kMaxFramesInFlight;
    if (vkAllocateCommandBuffers(device, &allocInfo, commandBuffers) != VK_SUCCESS) {
        std::fprintf(stderr, "editor: vkAllocateCommandBuffers failed\n");
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
            std::fprintf(stderr, "editor: failed to create sync objects for frame %d\n", i);
            return EXIT_FAILURE;
        }
    }

    // --- Camera: keyboard-navigable orbit (no mouse-look input plumbing exists yet, see
    //     engine::debug::ImGuiOverlay.h's own "not addressed yet" comment) -- W/S pitch,
    //     A/D yaw, Up/Down zoom. Reuses the plain InputState::keysHeld this sample already
    //     gets from Application's onUpdate, not engine::platform::InputSystem (no edge
    //     detection needed for continuous navigation). ---
    Camera camera;
    camera.target = glm::vec3(0.0f, 1.0f, 0.0f);
    camera.distance = 12.0f;
    camera.pitch = 0.5f;
    glm::mat4 currentViewProj(1.0f);
    int currentFrame = 0;

    std::uint32_t framesSinceReport = 0;
    auto lastFpsReportTime = std::chrono::steady_clock::now();
    double lastReportedFps = 0.0;

    // --- Selection state (Editor step E3) -- kInvalidEntity's generation (0) can never
    //     match a real, ever-alive entity (World::CreateEntity() always starts a slot's
    //     generation at 1), so it doubles as a safe "nothing selected" sentinel; combined
    //     with World::IsAlive() below it also naturally handles a selected entity having
    //     been destroyed since (not possible yet -- nothing destroys entities in the
    //     Editor -- but the check costs nothing and stays correct once something does). ---
    Entity selectedEntity = engine::ecs::kInvalidEntity;

    // --- Save status (Editor step E4) -- shown for a few seconds after a Save click so
    //     the button press has visible feedback beyond a stderr line. ---
    std::string saveStatus;
    float saveStatusRemainingSeconds = 0.0f;

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

        overlay.NewFrame();
        console.Update();

        ImGui::SetNextWindowPos(ImVec2(10.0f, 10.0f), ImGuiCond_FirstUseEver);
        ImGui::Begin("Pi-Engine Editor");
        ImGui::Text("Scene: %s", scenePath.c_str());
        ImGui::Text("Entities: %zu mesh", world.Meshes().Data().size());
        ImGui::Text("FPS: %.0f", lastReportedFps);
        ImGui::Separator();
        ImGui::TextUnformatted("Camera: A/D yaw, W/S pitch, Up/Down zoom");
        ImGui::Separator();
        // Writes back through the same JSON schema LoadScene() reads (docs/06-editor-
        // roadmap.md, E4) -- overwrites scenePath, no "Save As" yet (Editor step E7's
        // Project Hub is the more natural place to manage multiple scene files).
        if (ImGui::Button("Save")) {
            const bool ok = engine::scene::SaveScene(scenePath.c_str(), world);
            saveStatus = ok ? "Saved." : "Save failed -- see stderr.";
            saveStatusRemainingSeconds = 3.0f;
        }
        if (saveStatusRemainingSeconds > 0.0f) {
            saveStatusRemainingSeconds -= deltaSeconds;
            ImGui::SameLine();
            ImGui::TextUnformatted(saveStatus.c_str());
        }
        ImGui::End();

        // --- Scene panel: every entity with a Transform (docs/06-editor-roadmap.md, E3)
        //     -- every entity engine::scene::LoadScene spawns has one, so this is the
        //     full entity list, not a filtered subset. Clicking a row selects it. ---
        ImGui::SetNextWindowPos(ImVec2(10.0f, 150.0f), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(260.0f, 180.0f), ImGuiCond_FirstUseEver);
        ImGui::Begin("Scene");
        const auto& sceneEntities = world.Transforms().Entities();
        for (const Entity entity : sceneEntities) {
            char label[64];
            std::snprintf(label, sizeof(label), "Entity %u.%u", entity.index, entity.generation);
            if (ImGui::Selectable(label, entity == selectedEntity)) {
                selectedEntity = entity;
            }
        }
        ImGui::End();

        // --- Inspector panel: the selected entity's components, read-only where editing
        //     wouldn't mean anything yet (Mesh GUID, Rigidbody body id) and live-editable
        //     where it does (Transform, Collider shape data) -- DragFloat*/Checkbox below
        //     mutate the ComponentStorage entry GetTransform()/GetCollider() point right
        //     into, no separate "apply" step, visible in the Scene View next frame. ---
        ImGui::SetNextWindowPos(ImVec2(280.0f, 10.0f), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(340.0f, 320.0f), ImGuiCond_FirstUseEver);
        ImGui::Begin("Inspector");
        if (world.IsAlive(selectedEntity)) {
            if (TransformComponent* transform = world.GetTransform(selectedEntity)) {
                if (ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen)) {
                    ImGui::DragFloat3("Position", &transform->position.x, 0.05f);
                    glm::vec3 eulerDegrees = glm::degrees(glm::eulerAngles(transform->rotation));
                    if (ImGui::DragFloat3("Rotation", &eulerDegrees.x, 1.0f)) {
                        transform->rotation = glm::quat(glm::radians(eulerDegrees));
                    }
                    ImGui::DragFloat3("Scale", &transform->scale.x, 0.05f, 0.01f, 100.0f);
                }
            }
            if (engine::ecs::MeshComponent* mesh = world.GetMesh(selectedEntity)) {
                if (ImGui::CollapsingHeader("Mesh", ImGuiTreeNodeFlags_DefaultOpen)) {
                    ImGui::Text("GUID: %s", engine::asset::ToString(mesh->meshGuid).c_str());
                    ImGui::DragFloat("Bounds radius", &mesh->boundsRadius, 0.05f, 0.0f, 100.0f);
                }
            }
            if (ColliderComponent* collider = world.GetCollider(selectedEntity)) {
                if (ImGui::CollapsingHeader("Collider", ImGuiTreeNodeFlags_DefaultOpen)) {
                    const bool isBox = collider->shapeType == ColliderComponent::ShapeType::Box;
                    ImGui::Text("Shape: %s", isBox ? "Box" : "Sphere");
                    if (isBox) {
                        ImGui::DragFloat3("Half extents", &collider->halfExtents.x, 0.05f, 0.01f,
                                          50.0f);
                    } else {
                        ImGui::DragFloat("Radius", &collider->radius, 0.05f, 0.01f, 50.0f);
                    }
                    ImGui::Checkbox("Is trigger", &collider->isTrigger);
                }
            }
            if (engine::ecs::RigidbodyComponent* rigidbody = world.GetRigidbody(selectedEntity)) {
                if (ImGui::CollapsingHeader("Rigidbody", ImGuiTreeNodeFlags_DefaultOpen)) {
                    ImGui::Text("Body id: %u", rigidbody->bodyId);
                    ImGui::DragFloat("Mass", &rigidbody->mass, 0.1f, 0.01f, 1000.0f);
                }
            }
        } else {
            ImGui::TextUnformatted("No entity selected -- click one in the Scene panel.");
        }
        ImGui::End();

        // --- Console panel (Editor step E5): every stdout/stderr line the engine has
        //     printed since Console::Init() (main()'s first statement), stderr lines
        //     highlighted red. Not a new logging API -- every existing
        //     std::printf/std::fprintf(stderr, ...) call site across the engine is
        //     captured as-is (Console.h's own comment explains how). ---
        ImGui::SetNextWindowPos(ImVec2(10.0f, 470.0f), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(700.0f, 220.0f), ImGuiCond_FirstUseEver);
        ImGui::Begin("Console");
        if (!consoleAvailable) {
            ImGui::TextUnformatted("Console capture not available on this platform.");
        } else {
            if (ImGui::Button("Clear")) {
                console.Clear();
            }
            ImGui::SameLine();
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
                ImGui::SetScrollHereY(1.0f); // Auto-scroll, but only while already at the bottom.
            }
            ImGui::EndChild();
        }
        ImGui::End();

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
            std::fprintf(stderr, "editor: vkAcquireNextImageKHR failed\n");
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

        // Every entity with both a Transform and a Mesh gets drawn, resolved by GUID
        // through the cache above (same pattern as samples/m7_scene_and_prefab).
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

        // ImGui draws last, into the same render pass/subpass, on top of the scene.
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
            std::fprintf(stderr, "editor: vkQueueSubmit failed\n");
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
            std::fprintf(stderr, "editor: vkQueuePresentKHR failed\n");
            return;
        }

        currentFrame = (currentFrame + 1) % kMaxFramesInFlight;

        ++framesSinceReport;
        const auto now = std::chrono::steady_clock::now();
        const std::chrono::duration<double> elapsed = now - lastFpsReportTime;
        if (elapsed.count() >= 0.5) {
            lastReportedFps = static_cast<double>(framesSinceReport) / elapsed.count();
            char title[96];
            std::snprintf(title, sizeof(title), "Pi-Engine Editor (%.0f FPS)", lastReportedFps);
            displayBackend.SetWindowTitle(title);
            framesSinceReport = 0;
            lastFpsReportTime = now;
        }
    };

    std::printf("editor: running, close the window to exit.\n");
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
    overlay.Shutdown();
    pipeline.Shutdown();
    destroyDepthResources();
    vkDestroyRenderPass(device, renderPass, nullptr);

    meshCache.clear(); // destroys every RHIBuffer -- must happen before context.Shutdown()
    swapchain.Shutdown();
    context.Shutdown();
    displayBackend.Shutdown();

    std::printf("editor: clean exit.\n");
    console.Shutdown(); // Restores the real stdout/stderr fds (also happens in ~Console()
                        // regardless -- explicit here to match this function's own style
                        // of always calling Shutdown() rather than relying only on RAII).
    return EXIT_SUCCESS;
}
