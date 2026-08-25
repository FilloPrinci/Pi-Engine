#include "engine/debug/ImGuiOverlay.h"

#include "engine/platform/SDL2DisplayBackend.h"
#include "engine/rhi/RHIContext.h"

#include <imgui.h>
#include <imgui_impl_sdl2.h>
#include <imgui_impl_vulkan.h>

#include <SDL2/SDL.h>

#include <cstdio>

namespace engine::debug {

namespace {

void CheckVkResult(VkResult err) {
    if (err != VK_SUCCESS) {
        std::fprintf(stderr, "ImGuiOverlay: Vulkan error %d inside imgui_impl_vulkan\n",
                      static_cast<int>(err));
    }
}

} // namespace

ImGuiOverlay::~ImGuiOverlay() {
    Shutdown();
}

bool ImGuiOverlay::Init(rhi::RHIContext& context, platform::SDL2DisplayBackend& displayBackend,
                         VkRenderPass renderPass, std::uint32_t minImageCount,
                         std::uint32_t imageCount) {
    m_context = &context;
    m_displayBackend = &displayBackend;

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();

    if (!ImGui_ImplSDL2_InitForVulkan(displayBackend.GetSDLWindow())) {
        std::fprintf(stderr, "ImGuiOverlay: ImGui_ImplSDL2_InitForVulkan failed\n");
        ImGui::DestroyContext();
        m_context = nullptr;
        m_displayBackend = nullptr;
        return false;
    }

    ImGui_ImplVulkan_InitInfo initInfo{};
    initInfo.ApiVersion = VK_API_VERSION_1_2; // Matches RHIContext.cpp's VkApplicationInfo.
    initInfo.Instance = context.GetInstance();
    initInfo.PhysicalDevice = context.GetPhysicalDevice();
    initInfo.Device = context.GetDevice();
    initInfo.QueueFamily = context.GetGraphicsQueueFamily();
    initInfo.Queue = context.GetGraphicsQueue();
    // Convenience internal descriptor pool -- backend creates and owns it, no manual
    // VkDescriptorPool needed here. Comfortably above imgui_impl_vulkan.h's own documented
    // minimum (IMGUI_IMPL_VULKAN_MINIMUM_SAMPLED_IMAGE_POOL_SIZE=8 +
    // IMGUI_IMPL_VULKAN_MINIMUM_SAMPLER_POOL_SIZE=2).
    initInfo.DescriptorPoolSize = 16;
    initInfo.MinImageCount = minImageCount;
    initInfo.ImageCount = imageCount;
    initInfo.PipelineInfoMain.RenderPass = renderPass;
    initInfo.PipelineInfoMain.Subpass = 0;
    initInfo.CheckVkResultFn = CheckVkResult;

    if (!ImGui_ImplVulkan_Init(&initInfo)) {
        std::fprintf(stderr, "ImGuiOverlay: ImGui_ImplVulkan_Init failed\n");
        ImGui_ImplSDL2_Shutdown();
        ImGui::DestroyContext();
        m_context = nullptr;
        m_displayBackend = nullptr;
        return false;
    }

    displayBackend.SetRawEventHandler(
        [](const SDL_Event& event) { ImGui_ImplSDL2_ProcessEvent(&event); });

    m_initialized = true;
    return true;
}

void ImGuiOverlay::Shutdown() {
    if (!m_initialized) {
        return;
    }
    if (m_displayBackend != nullptr) {
        m_displayBackend->SetRawEventHandler(nullptr);
    }
    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();
    m_initialized = false;
    m_context = nullptr;
    m_displayBackend = nullptr;
}

void ImGuiOverlay::NewFrame() {
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplSDL2_NewFrame();
    ImGui::NewFrame();
}

void ImGuiOverlay::Render(VkCommandBuffer commandBuffer) {
    ImGui::Render();
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), commandBuffer);
}

VkDescriptorSet ImGuiOverlay::RegisterTexture(VkImageView imageView, VkSampler sampler) {
    return ImGui_ImplVulkan_AddTexture(sampler, imageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
}

void ImGuiOverlay::UnregisterTexture(VkDescriptorSet descriptorSet) {
    ImGui_ImplVulkan_RemoveTexture(descriptorSet);
}

} // namespace engine::debug
