#pragma once

#include <volk.h>

#include <cstdint>

namespace engine::platform {
class SDL2DisplayBackend;
}
namespace engine::rhi {
class RHIContext;
}

namespace engine::debug {

// Reusable Dear ImGui <-> RHI integration (Editor step E1, docs/06-editor-roadmap.md) --
// wraps imgui_impl_vulkan and imgui_impl_sdl2, both vendored in
// third_party/imgui_backends/ (vcpkg's imgui port has no sdl2-binding feature, only
// sdl3-binding, and its vulkan-binding feature needs a directly-linked libvulkan loader
// instead of going through volk like the rest of this project -- see that directory's own
// comment). Not Editor-specific: any consumer that wants an immediate-mode overlay -- the
// Editor's panels, or a future in-sample debug overlay (the original M2 TODO this
// resolves, matching CLAUDE.md's dependency-table role for Dear ImGui: "Debug overlay") --
// can use this the same way.
//
// Draws into the *same* render pass/subpass the caller's own geometry draws into (no
// separate ImGui render pass): call Render() after your own draw calls but before
// vkCmdEndRenderPass, on the same command buffer. Matches every sample's single-subpass
// render pass since M1 (color + depth attachment) -- ImGui doesn't touch depth, so it
// draws as a final overlay layer with no extra render-pass/attachment wiring needed.
//
// Input handling (mouse/keyboard capture so the app doesn't also react to clicks/keys
// meant for an ImGui widget, e.g. `ImGuiIO::WantCaptureMouse`) is not addressed yet --
// fine for E1's demo (no interactive widgets competing with gameplay input); revisit once
// a Scene View coexists with real navigation input (Editor step E2+).
class ImGuiOverlay {
public:
    ImGuiOverlay() = default;
    ~ImGuiOverlay();

    ImGuiOverlay(const ImGuiOverlay&) = delete;
    ImGuiOverlay& operator=(const ImGuiOverlay&) = delete;

    // `context`/`displayBackend` must already be initialized and must outlive this
    // overlay. `renderPass` is the render pass Render() will be called within (subpass 0).
    // `minImageCount`/`imageCount` should match the swapchain's (RHISwapchain::GetImageCount()
    // for both -- this project doesn't use a distinct "min" vs "actual" swapchain image
    // count anywhere else). Also installs a raw-SDL-event hook on `displayBackend`
    // (SDL2DisplayBackend::SetRawEventHandler) -- overwrites any handler set before this
    // call, and clears it again in Shutdown().
    bool Init(rhi::RHIContext& context, platform::SDL2DisplayBackend& displayBackend,
              VkRenderPass renderPass, std::uint32_t minImageCount, std::uint32_t imageCount);
    void Shutdown();

    // Starts a new ImGui frame -- call once per frame, before any ImGui:: calls (including
    // any panel code the caller adds), after PollEvents() (so this frame's mouse/keyboard
    // state, fed via the SetRawEventHandler hook above, is current).
    void NewFrame();

    // Ends the frame and records ImGui's draw data into `commandBuffer` -- call after your
    // own geometry draw calls, before vkCmdEndRenderPass (see class comment).
    void Render(VkCommandBuffer commandBuffer);

    // Registers a sampled image for display via ImGui::Image()/ImageButton() -- the
    // Editor's own Asset Browser thumbnails (post-Editor-E8,
    // docs/07-unity-parity-analysis.md's Asset Browser row) are the first consumer, but
    // this is deliberately generic, not thumbnail-specific. `imageView`/`sampler` must
    // already be in (or transition to) VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL and must
    // outlive the returned handle -- this call does not take ownership of either, it only
    // registers them with the ImGui Vulkan backend's own descriptor pool. Returns the
    // opaque handle ImGui::Image() expects as its texture id; call UnregisterTexture()
    // with it once the underlying image is being destroyed (e.g. an RHITexture going out
    // of scope), or the backend's descriptor pool leaks that slot for the rest of the
    // process.
    VkDescriptorSet RegisterTexture(VkImageView imageView, VkSampler sampler);
    void UnregisterTexture(VkDescriptorSet descriptorSet);

private:
    rhi::RHIContext* m_context = nullptr;
    platform::SDL2DisplayBackend* m_displayBackend = nullptr;
    bool m_initialized = false;
};

} // namespace engine::debug
