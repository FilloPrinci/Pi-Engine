#include "BuildPipeline.h"
#include "ProjectHub.h"
#include "UndoStack.h"

#include "engine/asset/AssetGuid.h"
#include "engine/asset/AssetMeta.h"
#include "engine/core/Application.h"
#include "engine/core/Camera.h"
#include "engine/core/EngineVersion.h"
#include "engine/debug/Console.h"
#include "engine/debug/ImGuiOverlay.h"
#include "engine/ecs/World.h"
#include "engine/platform/InputSystem.h"
#include "engine/platform/SDL2DisplayBackend.h"
#include "engine/renderer/CookedMesh.h"
#include "engine/renderer/CookedTexture.h"
#include "engine/renderer/ForwardLitColorPipeline.h"
#include "engine/renderer/ForwardLitShadedPipeline.h"
#include "engine/renderer/ForwardLitTexturedColorPipeline.h"
#include "engine/renderer/ForwardVertexLitPipeline.h"
#include "engine/renderer/ForwardVertexLitTexturedPipeline.h"
#include "engine/renderer/MaterialData.h"
#include "engine/renderer/ShaderPropertySchema.h"
#include "engine/rhi/RHIBuffer.h"
#include "engine/rhi/RHIContext.h"
#include "engine/rhi/RHISwapchain.h"
#include "engine/rhi/RHITexture.h"
#include "engine/scene/Scene.h"

#include <imgui.h>
// imgui_internal.h -- DockBuilder*() (docking layout post-Editor-E8) lives here, not in
// imgui.h's public API. "Internal" in name only: this is Dear ImGui's own documented way
// to lay out a default dock configuration in code rather than requiring the user to
// manually drag every panel into place on first run -- the standard pattern used by every
// ImGui-based tool that ships a default docked layout (ImGui's own demo included).
#include <imgui_internal.h>
#include <volk.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <functional>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
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
using engine::platform::InputSystem;
using engine::platform::Key;
using engine::platform::SDL2DisplayBackend;
using engine::renderer::ForwardLitColorPipeline;
using engine::renderer::ForwardLitShadedPipeline;
using engine::renderer::ForwardLitTexturedColorPipeline;
using engine::renderer::ForwardVertexLitPipeline;
using engine::renderer::ForwardVertexLitTexturedPipeline;
using engine::renderer::FrameLightingData;
using engine::renderer::GpuLight;
using engine::renderer::kMaxLights;
using engine::renderer::FindMaterialShader;
using engine::renderer::MaterialData;
using engine::renderer::MaterialPropertyValue;
using engine::renderer::MeshData;
using engine::renderer::ShaderPropertyDecl;
using engine::renderer::ShaderPropertyType;
using engine::rhi::RHITexture;
using engine::rhi::RHIBuffer;
using engine::rhi::RHIContext;
using engine::rhi::RHISwapchain;

namespace {

constexpr int kMaxFramesInFlight = 2;

// This project's "missing material" indicator (the user's own explicit request) -- an
// unmistakable, deliberately garish flat purple/violet, never produced by any real
// material's own tintColor by convention (same "impossible to mistake for a real asset"
// reasoning behind Unity/Source's own magenta/pink missing-shader colors). Drawn through
// colorPipeline (see the render loop below) rather than a dedicated pipeline -- it's the
// exact same flat-tint shader, just with this hardcoded tint instead of one read from a
// MaterialData.
constexpr glm::vec4 kMissingMaterialColor(0.62f, 0.0f, 0.85f, 1.0f);

std::string ShaderPath(const char* fileName) {
    return std::string(PI_ENGINE_SHADER_DIR) + "/" + fileName;
}

std::string CookedAssetPath(const char* fileName) {
    return std::string(PI_ENGINE_COOKED_ASSET_DIR) + "/" + fileName;
}

std::string EditorAssetPath(const char* fileName) {
    return std::string(PI_ENGINE_EDITOR_ASSET_DIR) + "/" + fileName;
}

// Asset Browser (Editor step E6): filenames directly inside `dir`, sorted, one filesystem
// read at startup -- not refreshed live (no file-watcher; the Cooker's output doesn't
// change while the Editor is already running against it). `excludeMetaFiles` hides the
// GUID sidecars themselves from the "Source Assets" listing -- they're metadata about an
// asset, not an asset a scene would ever reference.
std::vector<std::string> ListDirectory(const std::filesystem::path& dir, bool excludeMetaFiles) {
    std::vector<std::string> names;
    std::error_code errorCode;
    if (!std::filesystem::exists(dir, errorCode)) {
        return names;
    }
    for (const auto& entry : std::filesystem::directory_iterator(dir, errorCode)) {
        if (!entry.is_regular_file()) {
            continue;
        }
        const std::string name = entry.path().filename().string();
        constexpr std::string_view kMetaSuffix = ".meta";
        if (excludeMetaFiles && name.size() >= kMetaSuffix.size() &&
            name.compare(name.size() - kMetaSuffix.size(), kMetaSuffix.size(), kMetaSuffix) == 0) {
            continue;
        }
        names.push_back(name);
    }
    std::sort(names.begin(), names.end());
    return names;
}

// Shared by the Hierarchy panel's tree and the Inspector's parent picker (post-
// Editor-E8) -- both need to show the same human-readable name for an entity, and neither
// has anything richer than the raw handle to name it with yet (no per-entity name field
// exists in EntityDesc/TransformComponent).
std::string EntityLabel(Entity entity) {
    char buffer[64];
    std::snprintf(buffer, sizeof(buffer), "Entity %u.%u", entity.index, entity.generation);
    return buffer;
}

// Undo/Redo (post-Editor-E8, docs/07-unity-parity-analysis.md) for a single Inspector
// field edited via a "drag" or "type a value" ImGui widget (DragFloat*/Checkbox all work
// the same way). `IsItemActivated()` fires the frame the widget starts being edited (mouse
// down, or double-click into text-edit mode); `IsItemDeactivatedAfterEdit()` fires the
// frame the whole gesture ends *and* the value actually changed -- calling this right
// after such a widget batches an entire click-drag-release into one undo step instead of
// one per intermediate value the widget reported mid-drag, matching Unity's own Inspector
// undo granularity. `capturedValue` is the field's value at the moment the gesture started
// -- must be a variable that survives across frames for the gesture's whole duration
// (declared alongside the Editor's other persistent per-session state in main(), never a
// per-frame temporary, since IsItemActivated() and IsItemDeactivatedAfterEdit() fire on
// different frames). `resolveField` re-locates the live field fresh *inside* the undo/redo
// closures when they actually run, rather than the closures capturing a raw component
// pointer up front -- never a permanent raw pointer to a component (CLAUDE.md rule 4:
// component storage can move in memory between frames).
template <typename T, typename ResolveFieldFn>
void TrackFieldEdit(UndoStack& undoStack, World& world, Entity entity, const T& liveValue,
                    T& capturedValue, ResolveFieldFn resolveField) {
    if (ImGui::IsItemActivated()) {
        capturedValue = liveValue;
    }
    if (ImGui::IsItemDeactivatedAfterEdit()) {
        const T oldValue = capturedValue;
        const T newValue = liveValue;
        if (oldValue == newValue) {
            return; // the gesture ended without actually changing anything.
        }
        undoStack.Push(
            [&world, entity, resolveField, oldValue]() {
                if (T* field = resolveField(world, entity)) {
                    *field = oldValue;
                }
            },
            [&world, entity, resolveField, newValue]() {
                if (T* field = resolveField(world, entity)) {
                    *field = newValue;
                }
            });
    }
}

// Material property editing (post-Editor-E8, ShaderPropertySchema.h) -- returns a mutable
// reference to `material`'s entry for `decl.name`, (re)initializing it from the shader's
// own declared default first if it's missing or stored as a different type than `decl`
// says it should be (a hand-edited file with a typo, or a material authored before this
// shader gained the property) -- so the Inspector always has a real, correctly-typed
// value to bind an ImGui widget to directly, never editing/misinterpreting the wrong
// union member.
MaterialPropertyValue& EnsureMaterialProperty(MaterialData& material,
                                              const ShaderPropertyDecl& decl) {
    auto it = material.properties.find(decl.name);
    if (it != material.properties.end() && it->second.type == decl.type) {
        return it->second;
    }
    MaterialPropertyValue value;
    value.type = decl.type;
    value.colorValue = decl.defaultColor;
    value.floatValue = decl.defaultFloat;
    value.textureGuid = engine::asset::kInvalidAssetGuid;
    material.properties[decl.name] = value;
    return material.properties[decl.name];
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

// --- Viewport object picking + translate gizmo (post-E8, docs/07-unity-parity-analysis.md's
//     #1 priority item) -- everything below is plain math, no new RHI pipeline: the gizmo
//     itself is drawn with ImGui's own foreground draw list (2D lines projected from world
//     space each frame), reusing the ImGui integration E1 already wired in rather than
//     standing up a whole new debug-line Vulkan pipeline just for Editor-only gizmos. ---

// Inverse of the camera's view-projection transform at one screen pixel: a world-space ray
// from the near plane through the far plane. `mouseX`/`mouseY` are window-space pixels,
// origin top-left (SDL's own convention, matches InputState::mouseX/mouseY) -- no Y-flip
// needed against NDC here because Camera::GetProjectionMatrix() already flips NDC Y to
// match Vulkan's (and so the screen's) Y-down convention, see its own comment.
void ScreenPointToRay(float mouseX, float mouseY, float viewportWidth, float viewportHeight,
                      const glm::mat4& viewProj, glm::vec3& outOrigin, glm::vec3& outDir) {
    const float ndcX = (2.0f * mouseX) / viewportWidth - 1.0f;
    const float ndcY = (2.0f * mouseY) / viewportHeight - 1.0f;
    const glm::mat4 invViewProj = glm::inverse(viewProj);
    // z=0/z=1 (not the OpenGL -1/1 range) -- GLM_FORCE_DEPTH_ZERO_TO_ONE is set project-wide
    // (engine/CMakeLists.txt) to match Vulkan's own depth range.
    glm::vec4 nearPoint = invViewProj * glm::vec4(ndcX, ndcY, 0.0f, 1.0f);
    glm::vec4 farPoint = invViewProj * glm::vec4(ndcX, ndcY, 1.0f, 1.0f);
    nearPoint /= nearPoint.w;
    farPoint /= farPoint.w;
    outOrigin = glm::vec3(nearPoint);
    outDir = glm::normalize(glm::vec3(farPoint - nearPoint));
}

// The other direction: a world point's window-space pixel position, for drawing the gizmo
// (project world-space axis endpoints to 2D each frame) -- same NDC/screen convention as
// ScreenPointToRay() above, just inverted. `outBehindCamera` lets the caller skip a line
// endpoint that projects from behind the near plane instead of drawing garbage across the
// screen.
glm::vec2 WorldToScreen(const glm::vec3& worldPos, float viewportWidth, float viewportHeight,
                        const glm::mat4& viewProj, bool& outBehindCamera) {
    const glm::vec4 clip = viewProj * glm::vec4(worldPos, 1.0f);
    outBehindCamera = clip.w <= 0.0001f;
    if (outBehindCamera) {
        return glm::vec2(0.0f);
    }
    const glm::vec3 ndc = glm::vec3(clip) / clip.w;
    return glm::vec2((ndc.x * 0.5f + 0.5f) * viewportWidth, (ndc.y * 0.5f + 0.5f) * viewportHeight);
}

// Closest-approach ray/sphere test (entity picking uses MeshComponent::boundsRadius as the
// sphere, scaled by the entity's largest scale axis -- an approximation, not exact against
// the real mesh silhouette, but consistent with boundsRadius already being an approximation
// everywhere else it's used). Returns the smallest positive hit distance in `outT`.
bool RaySphereIntersect(const glm::vec3& rayOrigin, const glm::vec3& rayDir,
                        const glm::vec3& sphereCenter, float sphereRadius, float& outT) {
    const glm::vec3 originToCenter = rayOrigin - sphereCenter;
    const float b = glm::dot(originToCenter, rayDir);
    const float c = glm::dot(originToCenter, originToCenter) - sphereRadius * sphereRadius;
    const float discriminant = b * b - c;
    if (discriminant < 0.0f) {
        return false;
    }
    const float sqrtDiscriminant = std::sqrt(discriminant);
    float t = -b - sqrtDiscriminant;
    if (t < 0.0f) {
        t = -b + sqrtDiscriminant; // ray origin is inside the sphere
    }
    if (t < 0.0f) {
        return false;
    }
    outT = t;
    return true;
}

// Point-to-segment distance in screen space -- how the gizmo hit-tests which axis (if any)
// a click landed on: whichever axis line's on-screen segment is closest to the mouse,
// within kGizmoPickPixels.
float DistancePointToSegment(const glm::vec2& point, const glm::vec2& segmentStart,
                             const glm::vec2& segmentEnd) {
    const glm::vec2 segment = segmentEnd - segmentStart;
    const float segmentLengthSq = glm::dot(segment, segment);
    const float t = segmentLengthSq > 0.0001f
                        ? glm::clamp(glm::dot(point - segmentStart, segment) / segmentLengthSq,
                                     0.0f, 1.0f)
                        : 0.0f;
    const glm::vec2 closest = segmentStart + segment * t;
    return glm::length(point - closest);
}

// Same GUID -> GPU-buffers cache pattern as samples/m7_scene_and_prefab/main.cpp -- see
// that file's comment for why this is a map keyed by GUID (not a hardcoded filename).
// Resolves any cooked mesh under assets_cooked/ (not just one hardcoded file, this file's
// former placeholder limitation, fixed alongside material assets' Texture property type
// when a demo material needed a mesh with real UVs -- see resolveMesh's own comment) via
// a GUID -> cooked-path index built once at startup by scanning PI_ENGINE_COOKED_ASSET_DIR
// for "*.mesh" files and reading each one's own embedded GUID (CookedMesh.h's format
// embeds it directly -- unlike materials/textures, no source .meta sidecar scan is needed
// here). Still not a real general-purpose Asset Manifest (engine/asset/README.md) -- just
// this Editor's own local index, rebuilt fresh every launch.
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

    // --- Mesh GUID -> cooked-path index (see MeshGpuData's own comment). ---
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

    // Object creation (post-Editor-E8, "make everything the Editor shows manageable") --
    // the Hierarchy panel's "Create Cube" button and the Inspector's "+ Mesh" button both
    // need *some* mesh to default a newly-added MeshComponent to; m1_cube.mesh is the one
    // every sample/demo scene in this project already treats as the default building
    // block. Resolved by filename here (kInvalidAssetGuid if it isn't cooked, e.g. a
    // trimmed manifest) rather than a hardcoded GUID string, so it stays correct even if
    // that file's Asset GUID is ever regenerated.
    AssetGuid defaultCubeMeshGuid = engine::asset::kInvalidAssetGuid;
    for (const auto& [guid, path] : meshGuidToPath) {
        if (std::filesystem::path(path).filename() == "m1_cube.mesh") {
            defaultCubeMeshGuid = guid;
            break;
        }
    }

    // --- Mesh cache (see MeshGpuData's own comment). ---
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

    // --- Scene: ECS world only -- no JobSystem/PhysicsWorld yet (read-only Scene View,
    //     see this file's own comment). ---
    World world;
    if (!engine::scene::LoadScene(scenePath.c_str(), world)) {
        std::fprintf(stderr, "editor: failed to load scene \"%s\"\n", scenePath.c_str());
        return EXIT_FAILURE;
    }
    std::printf("editor: loaded scene \"%s\" (%zu mesh entities)\n", scenePath.c_str(),
                world.Meshes().Data().size());
    RecordRecentProject(scenePath); // Editor step E7 -- Project Hub's recent-scenes list.

    // --- Asset Browser listing (Editor step E6) -- read once at startup, see
    //     ListDirectory()'s own comment for why this isn't refreshed live. ---
    const std::vector<std::string> sourceAssetNames = ListDirectory(PI_ENGINE_ASSETS_DIR, true);
    const std::vector<std::string> cookedAssetNames = ListDirectory(PI_ENGINE_COOKED_ASSET_DIR, false);
    const std::vector<std::string> cookedShaderNames =
        ListDirectory(std::string(PI_ENGINE_COOKED_ASSET_DIR) + "/shaders", false);

    // --- Material assets (post-Editor-E8, renderer/MaterialData.h) -- GUID -> path index,
    //     built once by scanning the same PI_ENGINE_ASSETS_DIR listing above for
    //     "*.material.json" files and reading each one's .meta sidecar (the same GUID
    //     mechanism the Source Assets panel already reads from). Unlike meshCache below
    //     (still hardcoded to a single known mesh path, that struct's own TODO), a material
    //     can be any file a scene author drops under assets/, so this needs an actual
    //     lookup rather than a "load this one path and check the GUID matches" shortcut --
    //     still no engine-wide GUID -> path manifest to lean on instead
    //     (engine/asset/README.md).
    std::unordered_map<AssetGuid, std::string> materialGuidToPath;
    for (const std::string& name : sourceAssetNames) {
        constexpr std::string_view kMaterialSuffix = ".material.json";
        if (name.size() < kMaterialSuffix.size() ||
            name.compare(name.size() - kMaterialSuffix.size(), kMaterialSuffix.size(),
                        kMaterialSuffix) != 0) {
            continue;
        }
        const std::string fullPath = std::string(PI_ENGINE_ASSETS_DIR) + "/" + name;
        AssetGuid guid;
        if (engine::asset::TryReadAssetMetaGuid(fullPath.c_str(), guid)) {
            materialGuidToPath.emplace(guid, fullPath);
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

    // --- Texture assets (post-Editor-E8, ShaderPropertySchema.h's "Texture" property
    //     type) -- GUID -> *source* path index, same scan-assets-for-a-suffix pattern as
    //     materialGuidToPath above, just for "*.png" instead of "*.material.json". Maps to
    //     the source image (assets/foo.png), not the cooked output -- resolveTexture()
    //     below derives the cooked .tex path from it (NAME_WE + ".tex", the same naming
    //     convention cmake/CookAssets.cmake's own texture-cooking rule uses) rather than
    //     this index storing cooked paths directly, so a texture picker in the Inspector
    //     can show/store the same source-asset identity a mesh or material reference
    //     already does. ---
    std::unordered_map<AssetGuid, std::string> textureGuidToPath;
    for (const std::string& name : sourceAssetNames) {
        constexpr std::string_view kPngSuffix = ".png";
        if (name.size() < kPngSuffix.size() ||
            name.compare(name.size() - kPngSuffix.size(), kPngSuffix.size(), kPngSuffix) != 0) {
            continue;
        }
        const std::string fullPath = std::string(PI_ENGINE_ASSETS_DIR) + "/" + name;
        AssetGuid guid;
        if (engine::asset::TryReadAssetMetaGuid(fullPath.c_str(), guid)) {
            textureGuidToPath.emplace(guid, fullPath);
        }
    }

    // --- Project Hub (Editor step E7) -- read once at startup like the listings above;
    //     re-read after RelaunchWithProject() wouldn't matter anyway since this process is
    //     about to quit once that succeeds. ---
    const std::vector<RecentProject> recentProjects = LoadRecentProjects();

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

    // Material assets (post-Editor-E8) -- a first concrete pipeline (CLAUDE.md rule 7),
    // bound for entities whose material targets "ForwardLitColor" *and* for entities with
    // no material assigned at all (kMissingMaterialColor's own comment) -- see the render
    // loop below. Note there is deliberately no `ForwardLitPipeline` instance in this
    // file: M1's own debug normal-color visualization used to be the Editor's "no
    // material" fallback, but the user explicitly asked for a flat purple indicator
    // instead. `ForwardLitPipeline` itself is untouched -- it's still M1's own exit
    // criterion, and several samples (m1_hello_mesh, m2_hello_scene, ...) still use it
    // directly with no material system involved at all; only this Editor's own dispatch
    // choice changed.
    ForwardLitColorPipeline colorPipeline;
    if (!colorPipeline.Init(context, renderPass, swapchain.GetExtent(),
                            ShaderPath("m_material_color.vert.spv").c_str(),
                            ShaderPath("m_material_color.frag.spv").c_str())) {
        return EXIT_FAILURE;
    }

    // Material assets, "ForwardLitTexturedColor" shader (post-Editor-E8) -- a second
    // separate concrete pipeline (CLAUDE.md rule 7), bound instead of `colorPipeline` for
    // entities whose material targets this specific shader (see the render loop below).
    ForwardLitTexturedColorPipeline texturedColorPipeline;
    if (!texturedColorPipeline.Init(context, renderPass, swapchain.GetExtent(),
                                    ShaderPath("m_material_textured_color.vert.spv").c_str(),
                                    ShaderPath("m_material_textured_color.frag.spv").c_str())) {
        return EXIT_FAILURE;
    }

    // Lighting phase A (post-Editor-E8, docs/01 section 8.3) -- a third separate concrete
    // pipeline (CLAUDE.md rule 7), bound for entities whose material targets
    // "ForwardLitShaded" (per-fragment Blinn-Phong; see the render loop below).
    ForwardLitShadedPipeline shadedPipeline;
    if (!shadedPipeline.Init(context, renderPass, swapchain.GetExtent(),
                             ShaderPath("m_forward_lit_shaded.vert.spv").c_str(),
                             ShaderPath("m_forward_lit_shaded.frag.spv").c_str())) {
        return EXIT_FAILURE;
    }

    // The engine's default/base lit material (the user's own explicit request) -- a
    // fourth separate concrete pipeline (CLAUDE.md rule 7), same lighting formula as
    // `shadedPipeline` but evaluated per-vertex (ForwardVertexLitPipeline.h's own
    // comment). Bound for entities whose material targets "ForwardVertexLit".
    ForwardVertexLitPipeline vertexLitPipeline;
    if (!vertexLitPipeline.Init(context, renderPass, swapchain.GetExtent(),
                                ShaderPath("m_forward_vertex_lit.vert.spv").c_str(),
                                ShaderPath("m_forward_vertex_lit.frag.spv").c_str())) {
        return EXIT_FAILURE;
    }

    // Texture-supporting sibling of `vertexLitPipeline` -- a fifth separate concrete
    // pipeline (CLAUDE.md rule 7), bound for entities whose material targets
    // "ForwardVertexLitTextured" (ForwardVertexLitTexturedPipeline.h's own comment).
    ForwardVertexLitTexturedPipeline vertexLitTexturedPipeline;
    if (!vertexLitTexturedPipeline.Init(
            context, renderPass, swapchain.GetExtent(),
            ShaderPath("m_forward_vertex_lit_textured.vert.spv").c_str(),
            ShaderPath("m_forward_vertex_lit_textured.frag.spv").c_str())) {
        return EXIT_FAILURE;
    }

    // --- Frame lighting UBO (FrameLightingData, ForwardLitShadedPipeline.h's own comment
    //     for why this is a UBO and not a push constant) -- one buffer + one descriptor
    //     set *per frame-in-flight* (kMaxFramesInFlight, same index as every other
    //     per-frame resource in this function), not one shared instance: without this,
    //     writing this frame's light data into the same buffer the GPU might still be
    //     reading from the *previous* frame's draw calls would be a real race. The
    //     descriptor sets are allocated and pointed at their own buffer once, up front --
    //     only the buffer's *contents* change every frame (RHIBuffer::UpdateData()),
    //     never which buffer a given frame's descriptor set points to. Allocated against
    //     `shadedPipeline`'s own set-0 layout, but bound into all three lit pipelines'
    //     layouts (`shadedPipeline`/`vertexLitPipeline`/`vertexLitTexturedPipeline`) --
    //     each declares an identically-defined set-0 binding, so this is Vulkan-spec-
    //     compatible (spec 14.2.2) without allocating three separate sets. ---
    RHIBuffer frameLightingBuffers[kMaxFramesInFlight];
    for (int i = 0; i < kMaxFramesInFlight; ++i) {
        FrameLightingData defaultData;
        if (!frameLightingBuffers[i].InitWithData(context, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                                   &defaultData, sizeof(FrameLightingData))) {
            return EXIT_FAILURE;
        }
    }

    VkDescriptorPoolSize frameLightingPoolSize{};
    frameLightingPoolSize.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    frameLightingPoolSize.descriptorCount = kMaxFramesInFlight;

    VkDescriptorPoolCreateInfo frameLightingPoolInfo{};
    frameLightingPoolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    frameLightingPoolInfo.maxSets = kMaxFramesInFlight;
    frameLightingPoolInfo.poolSizeCount = 1;
    frameLightingPoolInfo.pPoolSizes = &frameLightingPoolSize;

    VkDescriptorPool frameLightingDescriptorPool = VK_NULL_HANDLE;
    if (vkCreateDescriptorPool(device, &frameLightingPoolInfo, nullptr,
                               &frameLightingDescriptorPool) != VK_SUCCESS) {
        std::fprintf(stderr, "editor: vkCreateDescriptorPool (frame lighting) failed\n");
        return EXIT_FAILURE;
    }

    VkDescriptorSet frameLightingDescriptorSets[kMaxFramesInFlight];
    {
        VkDescriptorSetLayout frameLightingLayout = shadedPipeline.GetDescriptorSetLayout();
        VkDescriptorSetLayout layouts[kMaxFramesInFlight];
        for (int i = 0; i < kMaxFramesInFlight; ++i) {
            layouts[i] = frameLightingLayout;
        }
        VkDescriptorSetAllocateInfo frameLightingAllocInfo{};
        frameLightingAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        frameLightingAllocInfo.descriptorPool = frameLightingDescriptorPool;
        frameLightingAllocInfo.descriptorSetCount = kMaxFramesInFlight;
        frameLightingAllocInfo.pSetLayouts = layouts;
        if (vkAllocateDescriptorSets(device, &frameLightingAllocInfo, frameLightingDescriptorSets) !=
            VK_SUCCESS) {
            std::fprintf(stderr, "editor: vkAllocateDescriptorSets (frame lighting) failed\n");
            return EXIT_FAILURE;
        }
        for (int i = 0; i < kMaxFramesInFlight; ++i) {
            VkDescriptorBufferInfo bufferInfo{};
            bufferInfo.buffer = frameLightingBuffers[i].GetHandle();
            bufferInfo.offset = 0;
            bufferInfo.range = sizeof(FrameLightingData);

            VkWriteDescriptorSet write{};
            write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            write.dstSet = frameLightingDescriptorSets[i];
            write.dstBinding = 0;
            write.descriptorCount = 1;
            write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            write.pBufferInfo = &bufferInfo;
            vkUpdateDescriptorSets(device, 1, &write, 0, nullptr);
        }
    }

    // --- Descriptor pool for material texture properties -- one combined-image-sampler
    //     descriptor set per distinct texture GUID actually used by a resolved material
    //     this session (see resolveTexture() below), not per entity/draw. maxSets=32 is a
    //     fixed cap matching this Editor's actual demo-scale scenes, not a shipping game's
    //     -- same "small, fixed, revisit if it ever matters" choice already made for this
    //     project's other fixed-size Editor-only allocations. ---
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
        std::fprintf(stderr, "editor: vkCreateDescriptorPool (material textures) failed\n");
        return EXIT_FAILURE;
    }

    // --- Material texture GPU cache (see MeshGpuData's own comment for the identical
    //     "GUID -> uploaded GPU resource" pattern) -- RHITexture owns its own VkSampler
    //     (RHITexture.h), so no separate sampler object is needed here. ---
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

        // Derive the cooked .tex path from the source image's filename stem -- matches
        // cmake/CookAssets.cmake's own texture-cooking rule (NAME_WE + ".tex").
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
            std::fprintf(stderr, "editor: vkAllocateDescriptorSets (material texture) failed "
                                 "-- materialTextureDescriptorPool exhausted?\n");
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
    // Docking (post-Editor-E8) -- an Editor-only opt-in, not enabled inside ImGuiOverlay
    // itself (engine::debug/, shared by non-Editor consumers like samples/e1_imgui_overlay,
    // stays generic). Needs vcpkg's imgui "docking-experimental" feature (vcpkg.json) --
    // the pinned imgui version doesn't build DockBuilder/DockSpace support without it.
    ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_DockingEnable;

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
    glm::vec3 currentCameraWorldPosition(0.0f); // Lighting phase A -- Blinn-Phong's view
                                                // direction (m_forward_lit_shaded.frag).
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

    // --- Viewport picking + translate gizmo (post-E8) -- InputSystem (not raw
    //     InputState::keysHeld/mouseLeftHeld like the camera above) because gizmo
    //     interaction genuinely needs press/release *edges*, not just held state: a click
    //     starts a drag once, a release ends it once, regardless of how many frames the
    //     button stays down/up in between. ---
    InputSystem inputSystem;
    enum class GizmoAxis { None, X, Y, Z };
    GizmoAxis draggingAxis = GizmoAxis::None;
    glm::vec2 dragStartMouseScreen(0.0f);
    glm::vec3 dragStartEntityPosition(0.0f);
    glm::vec2 dragAxisScreenDir(0.0f);   // unit 2D direction of the dragged axis on screen
    float dragScreenPixelsPerWorldUnit = 1.0f;

    // --- Undo/Redo (post-Editor-E8, docs/07-unity-parity-analysis.md) -- one stack for
    //     the whole Editor session, not per-entity/per-field. Ctrl+Z/Ctrl+Y (also
    //     Ctrl+Shift+Z) undo/redo whatever was actually edited most recently, regardless
    //     of what's selected right now -- matches Unity: undo targets the edit, not the
    //     current selection. The "captured*" fields below are TrackFieldEdit()'s own
    //     per-widget "value when this edit gesture started" state -- see its own comment
    //     for why these have to live here (persistent across frames) rather than as
    //     per-frame locals. ---
    UndoStack undoStack;
    glm::vec3 capturedPosition(0.0f);
    glm::quat capturedRotation(1.0f, 0.0f, 0.0f, 0.0f);
    glm::vec3 capturedScale(1.0f);
    float capturedMeshBoundsRadius = 0.0f;
    glm::vec3 capturedColliderHalfExtents(0.0f);
    float capturedColliderRadius = 0.0f;
    bool capturedColliderIsTrigger = false;
    float capturedRigidbodyMass = 0.0f;
    bool capturedRigidbodyIsStatic = false;
    glm::vec3 capturedLightColor(0.0f);
    float capturedLightIntensity = 0.0f;
    float capturedLightRange = 0.0f;
    bool capturedLightIsStatic = false;
    bool capturedLightCastsShadow = false;

    // --- Save status (Editor step E4) -- shown for a few seconds after a Save click so
    //     the button press has visible feedback beyond a stderr line. ---
    std::string saveStatus;
    float saveStatusRemainingSeconds = 0.0f;

    // --- Play status (Editor step E8) -- same "shown for a few seconds" pattern as Save
    //     above. RunBuildAndCook() blocks the UI thread for the duration of the build/cook
    //     (see BuildPipeline.h's own comment on why that's an accepted v1 simplification),
    //     so the button visibly does nothing until it returns -- the status text is the
    //     only feedback while that's happening. ---
    std::string playStatus;
    float playStatusRemainingSeconds = 0.0f;
    const std::string buildDir = PI_ENGINE_BUILD_DIR;

    // --- Asset Browser selection (Editor step E6) -- filename only (not a full path);
    //     empty means nothing selected. ---
    std::string selectedSourceAsset;

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
        inputSystem.Update(input);

        overlay.NewFrame();
        console.Update();

        // Undo/Redo keyboard shortcuts -- gated on !WantCaptureKeyboard so Ctrl+Z inside
        // a DragFloat's own text-edit box means "undo my typing there" (ImGui's own
        // built-in behavior) rather than also firing the Editor's undo stack at the same
        // time. IsKeyPressed(..., false) (repeat=false) so holding the keys down doesn't
        // spam-undo every single frame.
        if (!ImGui::GetIO().WantCaptureKeyboard) {
            ImGuiIO& io = ImGui::GetIO();
            if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Z, false)) {
                if (io.KeyShift) {
                    undoStack.Redo();
                } else {
                    undoStack.Undo();
                }
            } else if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Y, false)) {
                undoStack.Redo();
            }
        }

        const float aspect = static_cast<float>(swapchain.GetExtent().width) /
                              static_cast<float>(swapchain.GetExtent().height);
        const glm::mat4 viewProj = camera.GetProjectionMatrix(aspect) * camera.GetViewMatrix();

        // --- Dockspace + default layout (post-Editor-E8) -- a Unity-style arrangement
        //     (Hierarchy left, Inspector right, Assets/Console/Project Hub tabbed along
        //     the bottom, the 3D Scene view filling whatever's left in the middle) built
        //     once via ImGui's DockBuilder API instead of leaving every panel wherever it
        //     last happened to float. The host window covers the whole viewport with
        //     ImGuiWindowFlags_NoBackground + the DockSpace's own
        //     ImGuiDockNodeFlags_PassthruCentralNode, so the empty central region neither
        //     paints over the 3D render already happening behind it nor blocks mouse
        //     input there -- the existing viewport picking/gizmo code (WantCaptureMouse-
        //     gated) needs no changes at all, since a click that lands in that empty
        //     central region was already correctly seen as "not over an ImGui window".
        //     DockBuilderGetNode()/IsSplitNode() below means the default layout is only
        //     ever *built* the first time this dockspace ID has no usable layout yet (a
        //     genuinely fresh launch, or imgui.ini deleted/never existed) -- once built,
        //     ImGui's own imgui.ini persistence takes over exactly like every other
        //     panel's position already does, so a user who manually drags a panel
        //     somewhere else keeps that arrangement across restarts instead of it
        //     snapping back to this default every launch. ---
        const ImGuiViewport* mainViewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(mainViewport->WorkPos);
        ImGui::SetNextWindowSize(mainViewport->WorkSize);
        ImGui::SetNextWindowViewport(mainViewport->ID);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
        constexpr ImGuiWindowFlags kDockHostFlags =
            ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse |
            ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus |
            ImGuiWindowFlags_NoBackground;
        ImGui::Begin("EditorDockSpaceHost", nullptr, kDockHostFlags);
        ImGui::PopStyleVar(3);

        const ImGuiID dockspaceId = ImGui::GetID("EditorDockSpace");
        ImGui::DockSpace(dockspaceId, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_PassthruCentralNode);
        if (ImGui::DockBuilderGetNode(dockspaceId) == nullptr ||
            ImGui::DockBuilderGetNode(dockspaceId)->IsSplitNode() == false) {
            ImGui::DockBuilderRemoveNode(dockspaceId);
            // ImGuiDockNodeFlags_DockSpace itself lives in imgui_internal.h's "Private"
            // dock-node-flags enum, not the public ImGuiDockNodeFlags_ one
            // ImGuiDockNodeFlags_PassthruCentralNode belongs to -- an explicit cast avoids
            // -Wdeprecated-enum-enum-conversion for OR-ing the two together, which is
            // exactly what DockBuilderAddNode() itself expects for a true dockspace node.
            ImGui::DockBuilderAddNode(
                dockspaceId, static_cast<ImGuiDockNodeFlags>(ImGuiDockNodeFlags_DockSpace) |
                                 ImGuiDockNodeFlags_PassthruCentralNode);
            ImGui::DockBuilderSetNodeSize(dockspaceId, mainViewport->WorkSize);

            ImGuiID centerId = dockspaceId;
            const ImGuiID leftId =
                ImGui::DockBuilderSplitNode(centerId, ImGuiDir_Left, 0.18f, nullptr, &centerId);
            const ImGuiID rightId =
                ImGui::DockBuilderSplitNode(centerId, ImGuiDir_Right, 0.22f, nullptr, &centerId);
            const ImGuiID bottomId =
                ImGui::DockBuilderSplitNode(centerId, ImGuiDir_Down, 0.28f, nullptr, &centerId);
            ImGuiID leftBottomId = leftId;
            const ImGuiID leftTopId =
                ImGui::DockBuilderSplitNode(leftId, ImGuiDir_Up, 0.30f, nullptr, &leftBottomId);

            ImGui::DockBuilderDockWindow("Pi-Engine Editor", leftTopId);
            ImGui::DockBuilderDockWindow("Hierarchy", leftBottomId);
            ImGui::DockBuilderDockWindow("Inspector", rightId);
            ImGui::DockBuilderDockWindow("Assets", bottomId);
            ImGui::DockBuilderDockWindow("Console", bottomId);
            ImGui::DockBuilderDockWindow("Project Hub", bottomId);
            ImGui::DockBuilderFinish(dockspaceId);
        }
        ImGui::End();

        ImGui::Begin("Pi-Engine Editor"); // Docked (see the layout block above) --
                                          // no SetNextWindowPos/Size, the dock node owns
                                          // this window's position/size now.
        ImGui::Text("Scene: %s", scenePath.c_str());
        ImGui::Text("Entities: %zu mesh", world.Meshes().Data().size());
        ImGui::Text("FPS: %.0f", lastReportedFps);
        ImGui::Separator();
        ImGui::TextUnformatted("Camera: A/D yaw, W/S pitch, Up/Down zoom");
        ImGui::TextUnformatted("Viewport: click to select, drag a gizmo axis to move");
        ImGui::Separator();
        // Undo/Redo (post-Editor-E8) -- Ctrl+Z/Ctrl+Y also work (see the keyboard
        // shortcut handling above), these buttons exist for discoverability and so the
        // feature is clickable without a keyboard at all.
        ImGui::BeginDisabled(!undoStack.CanUndo());
        if (ImGui::Button("Undo")) {
            undoStack.Undo();
        }
        ImGui::EndDisabled();
        ImGui::SameLine();
        ImGui::BeginDisabled(!undoStack.CanRedo());
        if (ImGui::Button("Redo")) {
            undoStack.Redo();
        }
        ImGui::EndDisabled();
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
        ImGui::Separator();
        // Editor step E8: build (incremental) + cook (incremental) + launch, one flow,
        // errors surfacing in the Console panel below since the child processes inherit
        // this process's already-redirected stdout/stderr (BuildPipeline.h). "Play in
        // Debug" runs the exact same thing under gdb in batch mode instead.
        auto runPlay = [&](bool debug) {
            playStatus = "Building...";
            if (!RunBuildAndCook(buildDir)) {
                playStatus = "Build/Cook failed -- see Console.";
            } else if (!LaunchPlayProcess(scenePath, debug)) {
                playStatus = "Launch failed -- not supported on this platform, or fork() failed.";
            } else {
                playStatus = debug ? "Launched under gdb." : "Launched.";
            }
            playStatusRemainingSeconds = 4.0f;
        };
        if (ImGui::Button("Play")) {
            runPlay(false);
        }
        ImGui::SameLine();
        if (ImGui::Button("Play in Debug")) {
            runPlay(true);
        }
        if (playStatusRemainingSeconds > 0.0f) {
            playStatusRemainingSeconds -= deltaSeconds;
            ImGui::SameLine();
            ImGui::TextUnformatted(playStatus.c_str());
        }
        ImGui::End();

        // --- Hierarchy panel (named "Scene" through E3-E8, renamed to match Unity's own
        //     naming and the reference layout post-Editor-E8): every entity with a
        //     Transform (docs/06-editor-roadmap.md, E3), shown as a hierarchy tree
        //     (post-Editor-E8, docs/07-unity-parity-analysis.md) -- root entities
        //     (TransformComponent::parent == kInvalidEntity) at the top level, each one's
        //     children indented underneath (always fully expanded here, no per-node
        //     collapse -- see renderEntityNode's own comment for why). Clicking a row
        //     still just selects it, same as the flat list this originally was. O(n)
        //     children-of-X scan per node (so O(n^2) for the whole tree) instead of a
        //     parent->children map -- fine at the scene sizes this project actually has;
        //     worth revisiting if a scene ever has enough entities for that to matter.
        //     Docked (see the layout block above) -- no SetNextWindowPos/Size. ---
        ImGui::Begin("Hierarchy");

        // Object creation (post-Editor-E8, "make everything the Editor shows manageable")
        // -- "Create Empty" is just world.CreateEntity() + AddTransform() (every panel
        // below already only ever looks at entities with a Transform, so this alone makes
        // it show up); "Create Cube" additionally adds a MeshComponent pointed at
        // defaultCubeMeshGuid, the one mesh every sample/demo scene already treats as the
        // default building block. Not pushed onto undoStack -- see this feature's own
        // README entry (editor/README.md) for why create/destroy stay outside Undo for
        // this pass, unlike every field-edit/reparent/component add-remove around it.
        if (ImGui::Button("Create Empty")) {
            const Entity newEntity = world.CreateEntity();
            world.AddTransform(newEntity);
            selectedEntity = newEntity;
        }
        ImGui::SameLine();
        if (ImGui::Button("Create Cube")) {
            const Entity newEntity = world.CreateEntity();
            world.AddTransform(newEntity);
            engine::ecs::MeshComponent mesh;
            mesh.meshGuid = defaultCubeMeshGuid;
            mesh.boundsRadius = 0.87f; // matches every other cube instance's own value.
            world.AddMesh(newEntity, mesh);
            selectedEntity = newEntity;
        }

        const auto& sceneEntities = world.Transforms().Entities();

        // Deferred deletion (post-Editor-E8) -- a right-click "Delete" sets this instead
        // of calling world.DestroyEntity() immediately, since that would rearrange
        // ComponentStorage mid-iteration of `sceneEntities` (renderEntityNode() below
        // walks it recursively while still running). Applied once, after the whole tree
        // has finished rendering.
        Entity entityPendingDelete = engine::ecs::kInvalidEntity;

        // Deliberately not ImGui::TreeNodeEx()'s own indent-stack push/pop -- an earlier
        // version used that (with expand/collapse arrows) and had a real indent leak bug
        // that only showed up with 3+ siblings at the same level following a node with
        // children (found via Pi4 screenshot testing: an unrelated *root* entity rendered
        // visually nested, even though its own TransformComponent::parent was correctly
        // kInvalidEntity the whole time -- a rendering bug, not a data bug, but a
        // hierarchy panel that visually misrepresents the hierarchy defeats its own
        // purpose). Explicit Indent()/Unindent() driven by a plain `depth` parameter is
        // trivially balanced by construction -- every row always indents and unindents by
        // exactly the same amount, no conditional push to ever leave unpopped. Trades away
        // per-node collapse/expand, an acceptable simplification at the scene sizes this
        // project actually has (a handful of entities, always cheap to show in full).
        std::function<void(Entity, int)> renderEntityNode = [&](Entity parent, int depth) {
            for (const Entity entity : sceneEntities) {
                const TransformComponent* transform = world.GetTransform(entity);
                const Entity entityParent =
                    transform != nullptr ? transform->parent : engine::ecs::kInvalidEntity;
                if (entityParent != parent) {
                    continue;
                }

                const std::string label = EntityLabel(entity);
                ImGui::Indent(static_cast<float>(depth) * 15.0f);
                if (ImGui::Selectable(label.c_str(), entity == selectedEntity)) {
                    selectedEntity = entity;
                }
                // Right-click context menu (post-Editor-E8) -- BeginPopupContextItem()
                // targets whichever ImGui item was rendered immediately before it, i.e.
                // this exact Selectable, so each row gets its own independent popup.
                if (ImGui::BeginPopupContextItem()) {
                    if (ImGui::MenuItem("Delete")) {
                        entityPendingDelete = entity;
                    }
                    ImGui::EndPopup();
                }
                ImGui::Unindent(static_cast<float>(depth) * 15.0f);

                renderEntityNode(entity, depth + 1);
            }
        };
        renderEntityNode(engine::ecs::kInvalidEntity, 0);

        if (entityPendingDelete != engine::ecs::kInvalidEntity) {
            // Orphan any children rather than cascade-deleting them -- World::DestroyEntity()
            // itself doesn't touch other entities' TransformComponent::parent (documented
            // gap), so this Editor-level fix-up is what keeps a child from being left
            // pointing at a now-dead handle. Same "missing parent degrades to root" spirit
            // SpawnEntities() already applies to an out-of-range parentIndex.
            for (const Entity candidate : sceneEntities) {
                if (candidate == entityPendingDelete) {
                    continue;
                }
                TransformComponent* candidateTransform = world.GetTransform(candidate);
                if (candidateTransform != nullptr &&
                    candidateTransform->parent == entityPendingDelete) {
                    candidateTransform->parent = engine::ecs::kInvalidEntity;
                }
            }
            if (selectedEntity == entityPendingDelete) {
                selectedEntity = engine::ecs::kInvalidEntity;
            }
            world.DestroyEntity(entityPendingDelete);
        }
        ImGui::End();

        // --- Inspector panel: the selected entity's components, read-only where editing
        //     wouldn't mean anything yet (Mesh GUID, Rigidbody body id) and live-editable
        //     where it does (Transform, Collider shape data) -- DragFloat*/Checkbox below
        //     mutate the ComponentStorage entry GetTransform()/GetCollider() point right
        //     into, no separate "apply" step, visible in the Scene View next frame.
        //     Docked (see the layout block above) -- no SetNextWindowPos/Size. ---
        ImGui::Begin("Inspector");
        if (world.IsAlive(selectedEntity)) {
            if (TransformComponent* transform = world.GetTransform(selectedEntity)) {
                if (ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen)) {
                    // Parent picker (post-Editor-E8, docs/07-unity-parity-analysis.md) --
                    // "None" plus every other alive entity that isn't `selectedEntity`
                    // itself or one of its own descendants (World::IsDescendantOf() rejects
                    // both, since either would create a cycle in the parent chain).
                    // Position/Rotation/Scale below are always *local* -- reparenting an
                    // entity deliberately doesn't compensate its local values to keep it
                    // visually in place, matching a plain drag-in-Hierarchy in Unity without
                    // Alt held (the simpler of its two reparent behaviors).
                    const std::string currentParentLabel = transform->parent == engine::ecs::kInvalidEntity
                                                                ? "None"
                                                                : EntityLabel(transform->parent);
                    // One combined undo step per reparent (an instant, discrete action --
                    // no drag gesture to batch, unlike the DragFloat*s below, so this
                    // pushes directly instead of going through TrackFieldEdit()).
                    auto reparentWithUndo = [&](Entity newParent) {
                        const Entity oldParent = transform->parent;
                        if (oldParent == newParent) {
                            return;
                        }
                        transform->parent = newParent;
                        undoStack.Push(
                            [&world, entity = selectedEntity, oldParent]() {
                                if (TransformComponent* t = world.GetTransform(entity)) {
                                    t->parent = oldParent;
                                }
                            },
                            [&world, entity = selectedEntity, newParent]() {
                                if (TransformComponent* t = world.GetTransform(entity)) {
                                    t->parent = newParent;
                                }
                            });
                    };
                    if (ImGui::BeginCombo("Parent", currentParentLabel.c_str())) {
                        if (ImGui::Selectable("None", transform->parent == engine::ecs::kInvalidEntity)) {
                            reparentWithUndo(engine::ecs::kInvalidEntity);
                        }
                        for (const Entity candidate : world.Transforms().Entities()) {
                            if (candidate == selectedEntity ||
                                world.IsDescendantOf(candidate, selectedEntity)) {
                                continue;
                            }
                            const std::string candidateLabel = EntityLabel(candidate);
                            if (ImGui::Selectable(candidateLabel.c_str(),
                                                  candidate == transform->parent)) {
                                reparentWithUndo(candidate);
                            }
                        }
                        ImGui::EndCombo();
                    }

                    auto resolvePosition = [](World& w, Entity e) -> glm::vec3* {
                        TransformComponent* t = w.GetTransform(e);
                        return t != nullptr ? &t->position : nullptr;
                    };
                    auto resolveRotation = [](World& w, Entity e) -> glm::quat* {
                        TransformComponent* t = w.GetTransform(e);
                        return t != nullptr ? &t->rotation : nullptr;
                    };
                    auto resolveScale = [](World& w, Entity e) -> glm::vec3* {
                        TransformComponent* t = w.GetTransform(e);
                        return t != nullptr ? &t->scale : nullptr;
                    };

                    ImGui::DragFloat3("Position", &transform->position.x, 0.05f);
                    TrackFieldEdit(undoStack, world, selectedEntity, transform->position,
                                   capturedPosition, resolvePosition);

                    glm::vec3 eulerDegrees = glm::degrees(glm::eulerAngles(transform->rotation));
                    if (ImGui::DragFloat3("Rotation", &eulerDegrees.x, 1.0f)) {
                        transform->rotation = glm::quat(glm::radians(eulerDegrees));
                    }
                    TrackFieldEdit(undoStack, world, selectedEntity, transform->rotation,
                                   capturedRotation, resolveRotation);

                    ImGui::DragFloat3("Scale", &transform->scale.x, 0.05f, 0.01f, 100.0f);
                    TrackFieldEdit(undoStack, world, selectedEntity, transform->scale,
                                   capturedScale, resolveScale);
                }
            }
            bool removeMeshRequested = false;
            if (engine::ecs::MeshComponent* mesh = world.GetMesh(selectedEntity)) {
                if (ImGui::CollapsingHeader("Mesh", ImGuiTreeNodeFlags_DefaultOpen)) {
                    ImGui::Text("GUID: %s", engine::asset::ToString(mesh->meshGuid).c_str());
                    ImGui::DragFloat("Bounds radius", &mesh->boundsRadius, 0.05f, 0.0f, 100.0f);
                    TrackFieldEdit(undoStack, world, selectedEntity, mesh->boundsRadius,
                                   capturedMeshBoundsRadius, [](World& w, Entity e) -> float* {
                                       engine::ecs::MeshComponent* m = w.GetMesh(e);
                                       return m != nullptr ? &m->boundsRadius : nullptr;
                                   });
                    // Remove Component (post-Editor-E8) -- deferred (not
                    // world.RemoveMesh() called directly here) since `mesh` is a pointer
                    // straight into ComponentStorage; removing mid-use would invalidate
                    // it while this very block still reads it below (the Material
                    // sub-section, materialGuid).
                    if (ImGui::Button("Remove Mesh")) {
                        removeMeshRequested = true;
                    }
                }

                // Material assets (post-Editor-E8, renderer/MaterialData.h) -- generic
                // property editing: whatever properties the material's own shader
                // declares (ShaderPropertySchema.h) get a widget here, not a hardcoded
                // "Tint" field. Each edit writes straight into the resolved MaterialData
                // (so the Scene View reflects it the very next frame, same as any other
                // Inspector field) and persists to the actual .material.json file on
                // IsItemDeactivatedAfterEdit() -- a material is a separate asset file, not
                // part of the scene document, so there's no "Save" button covering it; the
                // gesture-end write is the material's own save point. Not wired into
                // UndoStack (materials are a separate file from the scene, undo across
                // files is a different problem than this pass's scope -- an accepted gap,
                // same shape as "Save/Play aren't undoable" already is).
                if (mesh->materialGuid != engine::asset::kInvalidAssetGuid) {
                    if (ImGui::CollapsingHeader("Material", ImGuiTreeNodeFlags_DefaultOpen)) {
                        ImGui::Text("GUID: %s",
                                    engine::asset::ToString(mesh->materialGuid).c_str());
                        if (MaterialData* material = resolveMaterial(mesh->materialGuid)) {
                            ImGui::Text("Shader: %s", material->shaderName.c_str());
                            const auto* schema = FindMaterialShader(material->shaderName);
                            if (schema == nullptr) {
                                ImGui::TextColored(
                                    ImVec4(1.0f, 0.5f, 0.3f, 1.0f),
                                    "Unregistered shader -- this material won't render");
                            } else {
                                auto materialPathIt = materialGuidToPath.find(mesh->materialGuid);
                                auto persist = [&]() {
                                    if (materialPathIt != materialGuidToPath.end()) {
                                        engine::renderer::WriteMaterial(
                                            materialPathIt->second.c_str(), *material);
                                    }
                                };
                                for (const ShaderPropertyDecl& decl : schema->properties) {
                                    ImGui::PushID(decl.name.c_str());
                                    MaterialPropertyValue& value =
                                        EnsureMaterialProperty(*material, decl);
                                    switch (decl.type) {
                                        case ShaderPropertyType::Color: {
                                            ImGui::ColorEdit4(decl.name.c_str(),
                                                              &value.colorValue.x);
                                            if (ImGui::IsItemDeactivatedAfterEdit()) {
                                                persist();
                                            }
                                            break;
                                        }
                                        case ShaderPropertyType::Float: {
                                            ImGui::DragFloat(decl.name.c_str(), &value.floatValue,
                                                             0.01f);
                                            if (ImGui::IsItemDeactivatedAfterEdit()) {
                                                persist();
                                            }
                                            break;
                                        }
                                        case ShaderPropertyType::Texture: {
                                            std::string currentLabel = "None";
                                            if (value.textureGuid !=
                                                engine::asset::kInvalidAssetGuid) {
                                                auto texPathIt =
                                                    textureGuidToPath.find(value.textureGuid);
                                                currentLabel =
                                                    texPathIt != textureGuidToPath.end()
                                                        ? std::filesystem::path(texPathIt->second)
                                                              .filename()
                                                              .string()
                                                        : "(missing: " +
                                                              engine::asset::ToString(
                                                                  value.textureGuid) +
                                                              ")";
                                            }
                                            if (ImGui::BeginCombo(decl.name.c_str(),
                                                                  currentLabel.c_str())) {
                                                if (ImGui::Selectable(
                                                        "None", value.textureGuid ==
                                                                    engine::asset::kInvalidAssetGuid)) {
                                                    value.textureGuid =
                                                        engine::asset::kInvalidAssetGuid;
                                                    persist();
                                                }
                                                for (const auto& [texGuid, texPath] :
                                                     textureGuidToPath) {
                                                    const std::string label =
                                                        std::filesystem::path(texPath)
                                                            .filename()
                                                            .string();
                                                    if (ImGui::Selectable(
                                                            label.c_str(),
                                                            texGuid == value.textureGuid)) {
                                                        value.textureGuid = texGuid;
                                                        persist();
                                                    }
                                                }
                                                ImGui::EndCombo();
                                            }
                                            break;
                                        }
                                    }
                                    ImGui::PopID();
                                }
                            }
                        }
                        if (ImGui::Button("Remove Material")) {
                            mesh->materialGuid = engine::asset::kInvalidAssetGuid;
                        }
                    }
                } else if (!materialGuidToPath.empty()) {
                    // Assign Material (post-Editor-E8, "make everything the Editor shows
                    // manageable") -- a Mesh with no material yet gets a picker over every
                    // *.material.json under assets/, same combo shape as the Texture
                    // property's own asset picker above. Only shown when at least one
                    // material asset actually exists to pick from.
                    if (ImGui::CollapsingHeader("Material", ImGuiTreeNodeFlags_DefaultOpen)) {
                        if (ImGui::BeginCombo("Assign Material", "(none)")) {
                            for (const auto& [matGuid, matPath] : materialGuidToPath) {
                                const std::string label =
                                    std::filesystem::path(matPath).filename().string();
                                if (ImGui::Selectable(label.c_str())) {
                                    mesh->materialGuid = matGuid;
                                }
                            }
                            ImGui::EndCombo();
                        }
                    }
                }
            }
            if (removeMeshRequested) {
                world.RemoveMesh(selectedEntity);
            }
            if (ColliderComponent* collider = world.GetCollider(selectedEntity)) {
                if (ImGui::CollapsingHeader("Collider", ImGuiTreeNodeFlags_DefaultOpen)) {
                    const bool isBox = collider->shapeType == ColliderComponent::ShapeType::Box;
                    // Shape switching (post-Editor-E8, "make everything the Editor shows
                    // manageable" phase 3) -- pushed directly to undoStack rather than
                    // through TrackFieldEdit(), same reasoning reparentWithUndo() above
                    // gives: an instant, discrete combo selection has no drag gesture to
                    // batch, so there's no IsItemActivated()/IsItemDeactivatedAfterEdit()
                    // pair to hook.
                    auto setShapeWithUndo = [&](ColliderComponent::ShapeType newShape) {
                        const ColliderComponent::ShapeType oldShape = collider->shapeType;
                        if (oldShape == newShape) {
                            return;
                        }
                        collider->shapeType = newShape;
                        undoStack.Push(
                            [&world, entity = selectedEntity, oldShape]() {
                                if (ColliderComponent* c = world.GetCollider(entity)) {
                                    c->shapeType = oldShape;
                                }
                            },
                            [&world, entity = selectedEntity, newShape]() {
                                if (ColliderComponent* c = world.GetCollider(entity)) {
                                    c->shapeType = newShape;
                                }
                            });
                    };
                    if (ImGui::BeginCombo("Shape", isBox ? "Box" : "Sphere")) {
                        if (ImGui::Selectable("Box", isBox)) {
                            setShapeWithUndo(ColliderComponent::ShapeType::Box);
                        }
                        if (ImGui::Selectable("Sphere", !isBox)) {
                            setShapeWithUndo(ColliderComponent::ShapeType::Sphere);
                        }
                        ImGui::EndCombo();
                    }
                    if (isBox) {
                        ImGui::DragFloat3("Half extents", &collider->halfExtents.x, 0.05f, 0.01f,
                                          50.0f);
                        TrackFieldEdit(undoStack, world, selectedEntity, collider->halfExtents,
                                       capturedColliderHalfExtents,
                                       [](World& w, Entity e) -> glm::vec3* {
                                           ColliderComponent* c = w.GetCollider(e);
                                           return c != nullptr ? &c->halfExtents : nullptr;
                                       });
                    } else {
                        ImGui::DragFloat("Radius", &collider->radius, 0.05f, 0.01f, 50.0f);
                        TrackFieldEdit(undoStack, world, selectedEntity, collider->radius,
                                       capturedColliderRadius, [](World& w, Entity e) -> float* {
                                           ColliderComponent* c = w.GetCollider(e);
                                           return c != nullptr ? &c->radius : nullptr;
                                       });
                    }
                    ImGui::Checkbox("Is trigger", &collider->isTrigger);
                    TrackFieldEdit(undoStack, world, selectedEntity, collider->isTrigger,
                                   capturedColliderIsTrigger, [](World& w, Entity e) -> bool* {
                                       ColliderComponent* c = w.GetCollider(e);
                                       return c != nullptr ? &c->isTrigger : nullptr;
                                   });
                    if (ImGui::Button("Remove Collider")) {
                        world.RemoveCollider(selectedEntity);
                    }
                }
            }
            if (engine::ecs::RigidbodyComponent* rigidbody = world.GetRigidbody(selectedEntity)) {
                if (ImGui::CollapsingHeader("Rigidbody", ImGuiTreeNodeFlags_DefaultOpen)) {
                    ImGui::Text("Body id: %u", rigidbody->bodyId);
                    // isStatic (post-Editor-E8, RigidbodyComponent.h's own comment) --
                    // read once at PhysicsWorld::CreateBody() time (M4 scope, unchanged),
                    // so editing it here only takes effect the next time this entity
                    // spawns into a real PhysicsWorld (Play Mode) -- the Editor's own
                    // Scene View has no PhysicsWorld to show a live effect in, same
                    // "authoring data for Play Mode" story every other Rigidbody field
                    // already has.
                    ImGui::Checkbox("Is Static", &rigidbody->isStatic);
                    TrackFieldEdit(undoStack, world, selectedEntity, rigidbody->isStatic,
                                   capturedRigidbodyIsStatic, [](World& w, Entity e) -> bool* {
                                       engine::ecs::RigidbodyComponent* r = w.GetRigidbody(e);
                                       return r != nullptr ? &r->isStatic : nullptr;
                                   });
                    ImGui::DragFloat("Mass", &rigidbody->mass, 0.1f, 0.01f, 1000.0f);
                    TrackFieldEdit(undoStack, world, selectedEntity, rigidbody->mass,
                                   capturedRigidbodyMass, [](World& w, Entity e) -> float* {
                                       engine::ecs::RigidbodyComponent* r = w.GetRigidbody(e);
                                       return r != nullptr ? &r->mass : nullptr;
                                   });
                    if (ImGui::Button("Remove Rigidbody")) {
                        world.RemoveRigidbody(selectedEntity);
                    }
                }
            }
            if (engine::ecs::LightComponent* light = world.GetLight(selectedEntity)) {
                if (ImGui::CollapsingHeader("Light", ImGuiTreeNodeFlags_DefaultOpen)) {
                    // Type switching (lighting phase A, docs/01 section 8.3) -- same
                    // instant-discrete-choice-pushes-directly reasoning as Collider's own
                    // Shape combo (setShapeWithUndo() above).
                    const bool isDirectional =
                        light->type == engine::ecs::LightComponent::Type::Directional;
                    auto setLightTypeWithUndo = [&](engine::ecs::LightComponent::Type newType) {
                        const engine::ecs::LightComponent::Type oldType = light->type;
                        if (oldType == newType) {
                            return;
                        }
                        light->type = newType;
                        undoStack.Push(
                            [&world, entity = selectedEntity, oldType]() {
                                if (engine::ecs::LightComponent* l = world.GetLight(entity)) {
                                    l->type = oldType;
                                }
                            },
                            [&world, entity = selectedEntity, newType]() {
                                if (engine::ecs::LightComponent* l = world.GetLight(entity)) {
                                    l->type = newType;
                                }
                            });
                    };
                    if (ImGui::BeginCombo("Type", isDirectional ? "Directional" : "Point")) {
                        if (ImGui::Selectable("Directional", isDirectional)) {
                            setLightTypeWithUndo(engine::ecs::LightComponent::Type::Directional);
                        }
                        if (ImGui::Selectable("Point", !isDirectional)) {
                            setLightTypeWithUndo(engine::ecs::LightComponent::Type::Point);
                        }
                        ImGui::EndCombo();
                    }

                    ImGui::ColorEdit3("Color", &light->color.x);
                    TrackFieldEdit(undoStack, world, selectedEntity, light->color,
                                   capturedLightColor, [](World& w, Entity e) -> glm::vec3* {
                                       engine::ecs::LightComponent* l = w.GetLight(e);
                                       return l != nullptr ? &l->color : nullptr;
                                   });

                    ImGui::DragFloat("Intensity", &light->intensity, 0.05f, 0.0f, 100.0f);
                    TrackFieldEdit(undoStack, world, selectedEntity, light->intensity,
                                   capturedLightIntensity, [](World& w, Entity e) -> float* {
                                       engine::ecs::LightComponent* l = w.GetLight(e);
                                       return l != nullptr ? &l->intensity : nullptr;
                                   });

                    if (!isDirectional) {
                        ImGui::DragFloat("Range", &light->range, 0.1f, 0.01f, 1000.0f);
                        TrackFieldEdit(undoStack, world, selectedEntity, light->range,
                                       capturedLightRange, [](World& w, Entity e) -> float* {
                                           engine::ecs::LightComponent* l = w.GetLight(e);
                                           return l != nullptr ? &l->range : nullptr;
                                       });
                    }

                    // isStatic/castsShadow (lighting phase A) -- runtime hints only in
                    // this phase, see LightComponent.h's own comment: no bake-time effect
                    // yet, castsShadow is reserved for phase B's static shadow map.
                    ImGui::Checkbox("Is Static", &light->isStatic);
                    TrackFieldEdit(undoStack, world, selectedEntity, light->isStatic,
                                   capturedLightIsStatic, [](World& w, Entity e) -> bool* {
                                       engine::ecs::LightComponent* l = w.GetLight(e);
                                       return l != nullptr ? &l->isStatic : nullptr;
                                   });
                    ImGui::Checkbox("Casts Shadow", &light->castsShadow);
                    TrackFieldEdit(undoStack, world, selectedEntity, light->castsShadow,
                                   capturedLightCastsShadow, [](World& w, Entity e) -> bool* {
                                       engine::ecs::LightComponent* l = w.GetLight(e);
                                       return l != nullptr ? &l->castsShadow : nullptr;
                                   });

                    if (ImGui::Button("Remove Light")) {
                        world.RemoveLight(selectedEntity);
                    }
                }
            }

            // Add Component (post-Editor-E8, "make everything the Editor shows
            // manageable") -- only offers a component type the entity doesn't already
            // have. Not pushed onto undoStack, same accepted-gap reasoning as Create/
            // Delete Entity above (editor/README.md has the full note). Material isn't
            // listed here -- it's a property of MeshComponent (materialGuid), not its own
            // ECS component, so it's assigned from the Mesh section above instead (only
            // meaningful once an entity already has a Mesh to attach it to).
            if (!world.HasMesh(selectedEntity) || !world.HasCollider(selectedEntity) ||
                !world.HasRigidbody(selectedEntity) || !world.HasLight(selectedEntity)) {
                ImGui::Separator();
                ImGui::TextUnformatted("Add Component:");
                bool anyAddComponentButton = false;
                auto addComponentButton = [&](const char* label, bool alreadyHasComponent,
                                              const std::function<void()>& onClick) {
                    if (alreadyHasComponent) {
                        return;
                    }
                    if (anyAddComponentButton) {
                        ImGui::SameLine();
                    }
                    if (ImGui::Button(label)) {
                        onClick();
                    }
                    anyAddComponentButton = true;
                };
                addComponentButton("+ Mesh", world.HasMesh(selectedEntity), [&]() {
                    engine::ecs::MeshComponent mesh;
                    mesh.meshGuid = defaultCubeMeshGuid;
                    mesh.boundsRadius = 0.87f;
                    world.AddMesh(selectedEntity, mesh);
                });
                addComponentButton("+ Collider", world.HasCollider(selectedEntity),
                                   [&]() { world.AddCollider(selectedEntity); });
                addComponentButton("+ Rigidbody", world.HasRigidbody(selectedEntity),
                                   [&]() { world.AddRigidbody(selectedEntity); });
                addComponentButton("+ Light", world.HasLight(selectedEntity),
                                   [&]() { world.AddLight(selectedEntity); });
            }
        } else {
            ImGui::TextUnformatted("No entity selected -- click one in the Hierarchy panel.");
        }
        ImGui::End();

        // --- Console panel (Editor step E5): every stdout/stderr line the engine has
        //     printed since Console::Init() (main()'s first statement), stderr lines
        //     highlighted red. Not a new logging API -- every existing
        //     std::printf/std::fprintf(stderr, ...) call site across the engine is
        //     captured as-is (Console.h's own comment explains how). Docked (see the
        //     layout block above) tabbed alongside Assets/Project Hub -- no
        //     SetNextWindowPos/Size. ---
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

        // --- Assets panel (named "Asset Browser" through E6-E8, renamed to match the
        //     reference layout post-Editor-E8, minimal): assets/ (Cooker source assets,
        //     docs/01 section 12.1) and assets_cooked/ (this build's Cooker output) side
        //     by side. Selecting a source asset shows its .meta GUID, if one exists
        //     (engine::asset::TryReadAssetMetaGuid) -- no "New Script" template
        //     generation yet (its own larger sub-feature, deferred further). Docked (see
        //     the layout block above) tabbed alongside Console/Project Hub -- no
        //     SetNextWindowPos/Size. ---
        ImGui::Begin("Assets");
        if (ImGui::CollapsingHeader("Source Assets (assets/)", ImGuiTreeNodeFlags_DefaultOpen)) {
            for (const std::string& name : sourceAssetNames) {
                if (ImGui::Selectable(name.c_str(), name == selectedSourceAsset)) {
                    selectedSourceAsset = name;
                }
            }
            if (sourceAssetNames.empty()) {
                ImGui::TextDisabled("(empty)");
            }
        }
        if (!selectedSourceAsset.empty()) {
            ImGui::Separator();
            AssetGuid guid;
            const std::string fullPath =
                std::string(PI_ENGINE_ASSETS_DIR) + "/" + selectedSourceAsset;
            if (engine::asset::TryReadAssetMetaGuid(fullPath.c_str(), guid)) {
                ImGui::Text("%s -- GUID: %s", selectedSourceAsset.c_str(),
                           engine::asset::ToString(guid).c_str());
            } else {
                ImGui::Text("%s -- no .meta sidecar", selectedSourceAsset.c_str());
            }
        }
        ImGui::Separator();
        if (ImGui::CollapsingHeader("Cooked Output (assets_cooked/)")) {
            for (const std::string& name : cookedAssetNames) {
                ImGui::BulletText("%s", name.c_str());
            }
            if (!cookedShaderNames.empty()) {
                ImGui::TextUnformatted("shaders/");
                ImGui::Indent();
                for (const std::string& name : cookedShaderNames) {
                    ImGui::BulletText("%s", name.c_str());
                }
                ImGui::Unindent();
            }
            if (cookedAssetNames.empty() && cookedShaderNames.empty()) {
                ImGui::TextDisabled("(empty)");
            }
        }
        ImGui::End();

        // --- Project Hub panel (Editor step E7, minimal -- see ProjectHub.h's own
        //     comment for why this is a recent-scenes list rather than a full "shared
        //     engine installation, multiple project directories" system). Opening a
        //     *different* scene than the one this process already has open relaunches
        //     the Editor pointed at it (one process per open project, same as Unity Hub
        //     really works) instead of trying to hot-swap the running World -- nothing
        //     else in this engine supports live-reloading either. Docked (see the layout
        //     block above) tabbed alongside Assets/Console -- no SetNextWindowPos/Size. ---
        ImGui::Begin("Project Hub");
        if (recentProjects.empty()) {
            ImGui::TextDisabled("(no recent scenes yet)");
        }
        for (const RecentProject& project : recentProjects) {
            const bool isCurrent = project.scenePath == scenePath;
            ImGui::BeginDisabled(isCurrent);
            if (ImGui::Selectable(project.scenePath.c_str())) {
                if (RelaunchWithProject(project.scenePath)) {
                    displayBackend.RequestQuit();
                } else {
                    std::fprintf(stderr,
                                 "editor: couldn't relaunch for \"%s\" -- not supported on "
                                 "this platform, or the relaunch itself failed to start\n",
                                 project.scenePath.c_str());
                }
            }
            ImGui::EndDisabled();
            ImGui::TextDisabled("    opened %s", project.lastOpenedUtc.c_str());
            if (isCurrent) {
                ImGui::SameLine();
                ImGui::TextDisabled("(current)");
            }
        }
        ImGui::End();

        // --- Viewport picking + translate gizmo (post-E8) -- only ever acts on a fresh
        //     mouse press (ImGui::GetIO().WantCaptureMouse gates *starting* something new,
        //     so clicking an ImGui panel never also picks/drags in the viewport behind it),
        //     but an already-started drag keeps updating/ends cleanly even if the mouse
        //     strays over a panel mid-drag -- gating that too would strand the drag "stuck"
        //     on release. ---
        const float viewportWidth = static_cast<float>(swapchain.GetExtent().width);
        const float viewportHeight = static_cast<float>(swapchain.GetExtent().height);
        const bool mouseOverImGui = ImGui::GetIO().WantCaptureMouse;
        const glm::vec2 mouseScreen(inputSystem.GetMouseX(), inputSystem.GetMouseY());

        // Hierarchy (post-Editor-E8, docs/07-unity-parity-analysis.md): the gizmo is drawn
        // and hit-tested at an entity's *world* position (its own local position composed
        // through its parent chain), never its raw TransformComponent::position -- for a
        // root entity (no parent) the two are identical, so nothing changes there.
        auto worldPositionOf = [&](Entity entity) {
            return glm::vec3(world.GetWorldMatrix(entity)[3]);
        };
        // A drag moves the entity by a *world*-space amount along a world axis (the gizmo
        // itself is always world-aligned, see the "what's left" note in docs/07 -- no
        // local/global toggle yet), but TransformComponent::position is local to the
        // entity's own parent -- converting one to the other means undoing the parent's
        // own rotation+scale (its inverse upper-left 3x3), the standard "world delta ->
        // local delta" scene-graph conversion. A root entity's parent is identity, so this
        // is just `worldDelta` unchanged, same as before hierarchy existed.
        auto worldDeltaToLocal = [&](Entity entity, const glm::vec3& worldDelta) {
            const TransformComponent* t = world.GetTransform(entity);
            if (t == nullptr || t->parent == engine::ecs::kInvalidEntity) {
                return worldDelta;
            }
            const glm::mat3 parentRotationScale = glm::mat3(world.GetWorldMatrix(t->parent));
            return glm::inverse(parentRotationScale) * worldDelta;
        };

        if (draggingAxis != GizmoAxis::None) {
            if (inputSystem.IsMouseLeftHeld()) {
                const glm::vec2 mouseDelta = mouseScreen - dragStartMouseScreen;
                const float screenDistanceAlongAxis = glm::dot(mouseDelta, dragAxisScreenDir);
                const float worldDelta = screenDistanceAlongAxis / dragScreenPixelsPerWorldUnit;
                glm::vec3 axisWorld(0.0f);
                if (draggingAxis == GizmoAxis::X) {
                    axisWorld = glm::vec3(1.0f, 0.0f, 0.0f);
                } else if (draggingAxis == GizmoAxis::Y) {
                    axisWorld = glm::vec3(0.0f, 1.0f, 0.0f);
                } else {
                    axisWorld = glm::vec3(0.0f, 0.0f, 1.0f);
                }
                if (TransformComponent* transform = world.GetTransform(selectedEntity)) {
                    const glm::vec3 localDelta =
                        worldDeltaToLocal(selectedEntity, axisWorld * worldDelta);
                    transform->position = dragStartEntityPosition + localDelta;
                }
            }
            if (inputSystem.WasMouseLeftReleasedThisFrame()) {
                // One undo step for the whole drag gesture (post-Editor-E8), same
                // batching reasoning as TrackFieldEdit() above -- but the gizmo drives
                // transform->position directly from raw mouse state rather than through
                // an ImGui widget, so there's no IsItemActivated()/
                // IsItemDeactivatedAfterEdit() to hook; dragStartEntityPosition (already
                // captured when the drag began, see the drag-start block below) plays the
                // same role TrackFieldEdit()'s "captured" state does.
                if (TransformComponent* transform = world.GetTransform(selectedEntity)) {
                    const glm::vec3 oldPosition = dragStartEntityPosition;
                    const glm::vec3 newPosition = transform->position;
                    if (oldPosition != newPosition) {
                        undoStack.Push(
                            [&world, entity = selectedEntity, oldPosition]() {
                                if (TransformComponent* t = world.GetTransform(entity)) {
                                    t->position = oldPosition;
                                }
                            },
                            [&world, entity = selectedEntity, newPosition]() {
                                if (TransformComponent* t = world.GetTransform(entity)) {
                                    t->position = newPosition;
                                }
                            });
                    }
                }
                draggingAxis = GizmoAxis::None;
            }
        } else if (!mouseOverImGui && inputSystem.WasMouseLeftPressedThisFrame()) {
            // Try the gizmo first (only meaningful if something is already selected), then
            // fall back to picking a new entity, then fall back to deselecting.
            bool startedGizmoDrag = false;
            if (world.IsAlive(selectedEntity)) {
                if (TransformComponent* transform = world.GetTransform(selectedEntity)) {
                    const glm::vec3 worldPosition = worldPositionOf(selectedEntity);
                    const glm::vec3 cameraEye = glm::vec3(glm::inverse(camera.GetViewMatrix())[3]);
                    const float distanceToCamera =
                        std::max(glm::length(worldPosition - cameraEye), 0.001f);
                    const float gizmoWorldLength = std::max(distanceToCamera * 0.15f, 0.3f);

                    bool behindCamera = false;
                    const glm::vec2 originScreen = WorldToScreen(
                        worldPosition, viewportWidth, viewportHeight, viewProj, behindCamera);

                    struct AxisEntry {
                        GizmoAxis axis;
                        glm::vec3 direction;
                    };
                    const AxisEntry axes[] = {
                        {GizmoAxis::X, glm::vec3(1.0f, 0.0f, 0.0f)},
                        {GizmoAxis::Y, glm::vec3(0.0f, 1.0f, 0.0f)},
                        {GizmoAxis::Z, glm::vec3(0.0f, 0.0f, 1.0f)},
                    };

                    constexpr float kGizmoPickPixels = 10.0f;
                    float closestDistance = kGizmoPickPixels;
                    GizmoAxis hitAxis = GizmoAxis::None;
                    glm::vec2 hitAxisScreenDir(0.0f);
                    float hitScreenPixelsPerWorldUnit = 1.0f;

                    if (!behindCamera) {
                        for (const AxisEntry& entry : axes) {
                            bool tipBehindCamera = false;
                            const glm::vec2 tipScreen = WorldToScreen(
                                worldPosition + entry.direction * gizmoWorldLength,
                                viewportWidth, viewportHeight, viewProj, tipBehindCamera);
                            if (tipBehindCamera) {
                                continue;
                            }
                            const float distance =
                                DistancePointToSegment(mouseScreen, originScreen, tipScreen);
                            if (distance < closestDistance) {
                                closestDistance = distance;
                                hitAxis = entry.axis;
                                const glm::vec2 screenDelta = tipScreen - originScreen;
                                const float screenLength = glm::length(screenDelta);
                                hitAxisScreenDir = screenLength > 0.0001f
                                                       ? screenDelta / screenLength
                                                       : glm::vec2(0.0f);
                                hitScreenPixelsPerWorldUnit =
                                    std::max(screenLength / gizmoWorldLength, 0.0001f);
                            }
                        }
                    }

                    if (hitAxis != GizmoAxis::None) {
                        draggingAxis = hitAxis;
                        dragStartMouseScreen = mouseScreen;
                        dragStartEntityPosition = transform->position;
                        dragAxisScreenDir = hitAxisScreenDir;
                        dragScreenPixelsPerWorldUnit = hitScreenPixelsPerWorldUnit;
                        startedGizmoDrag = true;
                    }
                }
            }

            if (!startedGizmoDrag) {
                // No gizmo hit -- pick the closest entity the ray actually hits (Mesh's
                // boundsRadius as a sphere, scaled by the entity's largest scale axis), or
                // deselect if the click didn't hit anything (matches Unity's own "click
                // empty space to deselect").
                glm::vec3 rayOrigin(0.0f);
                glm::vec3 rayDir(0.0f);
                ScreenPointToRay(mouseScreen.x, mouseScreen.y, viewportWidth, viewportHeight,
                                 viewProj, rayOrigin, rayDir);

                Entity closestEntity = engine::ecs::kInvalidEntity;
                float closestT = std::numeric_limits<float>::max();
                const auto& meshesForPicking = world.Meshes().Data();
                const auto& meshEntitiesForPicking = world.Meshes().Entities();
                for (std::size_t i = 0; i < meshesForPicking.size(); ++i) {
                    const Entity candidate = meshEntitiesForPicking[i];
                    if (!world.HasTransform(candidate)) {
                        continue;
                    }
                    // World position/scale (hierarchy, post-Editor-E8) -- a child
                    // entity's own TransformComponent::scale is *local*, so the world
                    // matrix's basis-vector lengths are what actually determine its
                    // on-screen (and pickable) size, not the raw local scale alone.
                    const glm::mat4 candidateWorld = world.GetWorldMatrix(candidate);
                    const glm::vec3 worldPosition = glm::vec3(candidateWorld[3]);
                    const float scaleMax =
                        std::max({glm::length(glm::vec3(candidateWorld[0])),
                                 glm::length(glm::vec3(candidateWorld[1])),
                                 glm::length(glm::vec3(candidateWorld[2]))});
                    const float radius = meshesForPicking[i].boundsRadius * scaleMax;
                    float t = 0.0f;
                    if (RaySphereIntersect(rayOrigin, rayDir, worldPosition, radius, t) &&
                        t < closestT) {
                        closestT = t;
                        closestEntity = candidate;
                    }
                }
                selectedEntity = closestEntity;
            }
        }

        // --- Gizmo drawing -- ImGui foreground draw list (E1's overlay already renders
        //     ImGui on top of the 3D scene each frame, see onRender), only when something
        //     is selected. Same axis endpoints the hit-test above computes -- kept as a
        //     separate pass rather than merged into it since drawing must happen every
        //     frame regardless of whether a click happened this frame. ---
        if (world.IsAlive(selectedEntity) && world.HasTransform(selectedEntity)) {
            {
                const glm::vec3 worldPosition = worldPositionOf(selectedEntity);
                const glm::vec3 cameraEye = glm::vec3(glm::inverse(camera.GetViewMatrix())[3]);
                const float distanceToCamera =
                    std::max(glm::length(worldPosition - cameraEye), 0.001f);
                const float gizmoWorldLength = std::max(distanceToCamera * 0.15f, 0.3f);

                bool originBehindCamera = false;
                const glm::vec2 originScreen = WorldToScreen(
                    worldPosition, viewportWidth, viewportHeight, viewProj, originBehindCamera);
                if (!originBehindCamera) {
                    ImDrawList* drawList = ImGui::GetForegroundDrawList();
                    struct AxisDrawEntry {
                        glm::vec3 direction;
                        ImU32 color;
                        GizmoAxis axis;
                    };
                    const AxisDrawEntry axesToDraw[] = {
                        {glm::vec3(1.0f, 0.0f, 0.0f), IM_COL32(230, 60, 60, 255), GizmoAxis::X},
                        {glm::vec3(0.0f, 1.0f, 0.0f), IM_COL32(60, 220, 60, 255), GizmoAxis::Y},
                        {glm::vec3(0.0f, 0.0f, 1.0f), IM_COL32(70, 130, 240, 255), GizmoAxis::Z},
                    };
                    for (const AxisDrawEntry& entry : axesToDraw) {
                        bool tipBehindCamera = false;
                        const glm::vec2 tipScreen = WorldToScreen(
                            worldPosition + entry.direction * gizmoWorldLength,
                            viewportWidth, viewportHeight, viewProj, tipBehindCamera);
                        if (tipBehindCamera) {
                            continue;
                        }
                        const float thickness = draggingAxis == entry.axis ? 5.0f : 3.0f;
                        drawList->AddLine(ImVec2(originScreen.x, originScreen.y),
                                          ImVec2(tipScreen.x, tipScreen.y), entry.color, thickness);
                        drawList->AddCircleFilled(ImVec2(tipScreen.x, tipScreen.y), 4.0f, entry.color);
                    }
                }
            }
        }

        currentViewProj = viewProj;
        currentCameraWorldPosition = glm::vec3(glm::inverse(camera.GetViewMatrix())[3]);
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

        // Lighting phase A (post-Editor-E8, docs/01 section 8.3) -- collect up to
        // kMaxLights active LightComponents into this frame's FrameLightingData and
        // upload it into *this* frame-in-flight's own UBO instance, before any
        // ForwardLitShaded draw reads it below. Not gated on whether a ForwardLitShaded
        // entity actually exists this frame -- cheap enough (a handful of lights, one
        // small memcpy) that tracking "is it needed" wouldn't be worth the complexity.
        {
            FrameLightingData frameLighting;
            frameLighting.viewProj = currentViewProj;
            frameLighting.cameraWorldPosition = glm::vec4(currentCameraWorldPosition, 0.0f);

            const auto& lights = world.Lights().Data();
            const auto& lightEntities = world.Lights().Entities();
            int activeLightCount = 0;
            for (std::size_t i = 0; i < lights.size() && activeLightCount < kMaxLights; ++i) {
                if (world.GetTransform(lightEntities[i]) == nullptr) {
                    continue;
                }
                const glm::mat4 lightWorld = world.GetWorldMatrix(lightEntities[i]);
                GpuLight& gpuLight = frameLighting.lights[activeLightCount];
                if (lights[i].type == engine::ecs::LightComponent::Type::Point) {
                    gpuLight.positionOrDirection = glm::vec4(glm::vec3(lightWorld[3]), 1.0f);
                } else {
                    // Directional -- local forward is -Z (this project's own convention,
                    // ForwardLitShadedPipeline.h's own comment), transformed by the
                    // light's world rotation (mat3 of its world matrix -- same uniform-
                    // scale simplification the shader itself already documents).
                    const glm::vec3 worldForward =
                        glm::normalize(glm::mat3(lightWorld) * glm::vec3(0.0f, 0.0f, -1.0f));
                    gpuLight.positionOrDirection = glm::vec4(worldForward, 0.0f);
                }
                gpuLight.color = glm::vec4(lights[i].color, lights[i].intensity);
                gpuLight.params = glm::vec4(lights[i].range, 0.0f, 0.0f, 0.0f);
                ++activeLightCount;
            }
            // Flat ambient term, not yet scene-configurable -- a fixed, modest default
            // (post-Editor-E8, docs/01 section 8.3's scope doesn't call for an ambient
            // authoring control yet; add one only once a real shader/UI need shows up).
            frameLighting.ambientAndCount =
                glm::vec4(0.05f, 0.05f, 0.05f, static_cast<float>(activeLightCount));

            frameLightingBuffers[currentFrame].UpdateData(&frameLighting, sizeof(FrameLightingData));
        }

        vkCmdBeginRenderPass(cmd, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

        // Every entity with both a Transform and a Mesh gets drawn, resolved by GUID
        // through the caches above (same pattern as samples/m7_scene_and_prefab). Bound
        // pipeline depends on which shader the entity's material (if any) targets
        // (post-Editor-E8, ShaderPropertySchema.h) -- one pass per known shader rather
        // than rebinding per-entity, since vkCmdBindPipeline is a real state change and
        // entities aren't grouped by material in world.Meshes()'s own storage order. Only
        // two material shaders exist today (ForwardLitColor, ForwardLitTexturedColor) so
        // this stays a small, explicit enumeration rather than a generic dispatch table --
        // grows one `else if`/pass at a time alongside ShaderPropertySchema.h's own
        // registry, see that header's comment for why this doesn't become an uber-shader.
        const auto& meshes = world.Meshes().Data();
        const auto& meshEntities = world.Meshes().Entities();

        // Shared per-entity setup (transform lookup + mesh GPU data + world-space MVP) --
        // every pass below needs exactly this, only what gets pushed/bound afterward
        // differs per shader.
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
            // chain (post-Editor-E8 hierarchy); identical to transform->GetMatrix() for
            // any root entity, so no behavior change for the overwhelming majority of
            // entities that still have no parent.
            const glm::mat4 mvp = currentViewProj * world.GetWorldMatrix(meshEntities[i]);
            return MeshDrawContext{gpuData, mvp};
        };
        auto bindMeshBuffers = [&](const MeshGpuData& gpuData) {
            VkBuffer vertexBuffers[] = {gpuData.vertexBuffer.GetHandle()};
            VkDeviceSize offsets[] = {0};
            vkCmdBindVertexBuffers(cmd, 0, 1, vertexBuffers, offsets);
            vkCmdBindIndexBuffer(cmd, gpuData.indexBuffer.GetHandle(), 0, VK_INDEX_TYPE_UINT32);
        };

        // Pass 1: flat tint (colorPipeline) -- covers two cases with the same pipeline/
        // shader: a material targeting "ForwardLitColor" uses that material's own
        // tintColor; an entity with *no* material assigned at all uses
        // kMissingMaterialColor instead (this project's "missing material" indicator, the
        // user's own explicit request -- replaces what used to fall back to
        // ForwardLitPipeline's debug normal-color visualization here; see this file's own
        // comment by `colorPipeline`'s declaration for why ForwardLitPipeline itself
        // stays untouched).
        colorPipeline.Bind(cmd);
        for (std::size_t i = 0; i < meshes.size(); ++i) {
            glm::vec4 tint;
            if (meshes[i].materialGuid == engine::asset::kInvalidAssetGuid) {
                tint = kMissingMaterialColor;
            } else {
                MaterialData* material = resolveMaterial(meshes[i].materialGuid);
                if (material == nullptr || material->shaderName != "ForwardLitColor") {
                    continue;
                }
                tint = material->GetColor("tintColor", glm::vec4(1.0f));
            }
            std::optional<MeshDrawContext> draw = resolveMeshDraw(i);
            if (!draw) {
                continue;
            }
            bindMeshBuffers(*draw->gpuData);
            colorPipeline.PushMvpAndTint(cmd, draw->mvp, tint);
            vkCmdDrawIndexed(cmd, draw->gpuData->indexCount, 1, 0, 0, 0);
        }

        // Pass 2: material targeting "ForwardLitTexturedColor" -- albedo texture * tint.
        // An entity whose "albedoTexture" property can't be resolved (none assigned, or
        // the referenced asset is missing) is skipped entirely rather than sampling
        // garbage -- a checkerboard/missing-texture fallback would be nicer but isn't
        // built yet (v1 simplification, same spirit as every other "degrade, don't crash"
        // choice in this codebase).
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

        // Pass 3: material targeting "ForwardLitShaded" -- lit, per-fragment Blinn-Phong
        // (lighting phase A, docs/01 section 8.3). Binds the frame lighting descriptor set
        // once, before any entity in this pass draws (same data for every one of them,
        // unlike the per-draw texture descriptor set Pass 2 rebinds per material).
        shadedPipeline.Bind(cmd);
        shadedPipeline.BindFrameDescriptorSet(cmd, frameLightingDescriptorSets[currentFrame]);
        for (std::size_t i = 0; i < meshes.size(); ++i) {
            if (meshes[i].materialGuid == engine::asset::kInvalidAssetGuid) {
                continue;
            }
            MaterialData* material = resolveMaterial(meshes[i].materialGuid);
            if (material == nullptr || material->shaderName != "ForwardLitShaded") {
                continue;
            }
            if (world.GetTransform(meshEntities[i]) == nullptr) {
                continue;
            }
            MeshGpuData* gpuData = resolveMesh(meshes[i].meshGuid);
            if (gpuData == nullptr) {
                continue;
            }
            bindMeshBuffers(*gpuData);
            // The *model* matrix, not MVP -- this pipeline multiplies by the frame UBO's
            // viewProj itself (ForwardLitShadedPipeline.h's own comment for why).
            const glm::mat4 model = world.GetWorldMatrix(meshEntities[i]);
            const glm::vec4 tint = material->GetColor("tintColor", glm::vec4(1.0f));
            shadedPipeline.PushModelAndTint(cmd, model, tint);
            vkCmdDrawIndexed(cmd, gpuData->indexCount, 1, 0, 0, 0);
        }

        // Pass 4: material targeting "ForwardVertexLit" -- the engine's default/base lit
        // material (the user's own explicit request), per-vertex lighting instead of
        // Pass 3's per-fragment (ForwardVertexLitPipeline.h's own comment). Shares the
        // exact same frame lighting descriptor set Pass 3 already bound this frame (both
        // pipelines declare an identically-defined set-0 binding, see this file's own
        // comment where the descriptor sets are allocated), so no rebind is needed here.
        vertexLitPipeline.Bind(cmd);
        for (std::size_t i = 0; i < meshes.size(); ++i) {
            if (meshes[i].materialGuid == engine::asset::kInvalidAssetGuid) {
                continue;
            }
            MaterialData* material = resolveMaterial(meshes[i].materialGuid);
            if (material == nullptr || material->shaderName != "ForwardVertexLit") {
                continue;
            }
            if (world.GetTransform(meshEntities[i]) == nullptr) {
                continue;
            }
            MeshGpuData* gpuData = resolveMesh(meshes[i].meshGuid);
            if (gpuData == nullptr) {
                continue;
            }
            bindMeshBuffers(*gpuData);
            vertexLitPipeline.BindFrameDescriptorSet(cmd, frameLightingDescriptorSets[currentFrame]);
            const glm::mat4 model = world.GetWorldMatrix(meshEntities[i]);
            const glm::vec4 tint = material->GetColor("tintColor", glm::vec4(1.0f));
            vertexLitPipeline.PushModelAndTint(cmd, model, tint);
            vkCmdDrawIndexed(cmd, gpuData->indexCount, 1, 0, 0, 0);
        }

        // Pass 5: material targeting "ForwardVertexLitTextured" -- texture-supporting
        // sibling of Pass 4 (ForwardVertexLitTexturedPipeline.h's own comment). Needs
        // *two* descriptor sets bound (set 0 frame lighting, set 1 per-material texture)
        // -- reuses the exact same `resolveMaterialTexture` cache Pass 2 already
        // populates, since both pipelines declare an identically-defined set-1 binding.
        vertexLitTexturedPipeline.Bind(cmd);
        for (std::size_t i = 0; i < meshes.size(); ++i) {
            if (meshes[i].materialGuid == engine::asset::kInvalidAssetGuid) {
                continue;
            }
            MaterialData* material = resolveMaterial(meshes[i].materialGuid);
            if (material == nullptr || material->shaderName != "ForwardVertexLitTextured") {
                continue;
            }
            const AssetGuid textureGuid =
                material->GetTexture("albedoTexture", engine::asset::kInvalidAssetGuid);
            auto* textureGpuData = resolveMaterialTexture(textureGuid);
            if (textureGpuData == nullptr) {
                continue;
            }
            if (world.GetTransform(meshEntities[i]) == nullptr) {
                continue;
            }
            MeshGpuData* gpuData = resolveMesh(meshes[i].meshGuid);
            if (gpuData == nullptr) {
                continue;
            }
            bindMeshBuffers(*gpuData);
            vertexLitTexturedPipeline.BindFrameDescriptorSet(cmd,
                                                              frameLightingDescriptorSets[currentFrame]);
            vertexLitTexturedPipeline.BindMaterialDescriptorSet(cmd, textureGpuData->descriptorSet);
            const glm::mat4 model = world.GetWorldMatrix(meshEntities[i]);
            const glm::vec4 tint = material->GetColor("tintColor", glm::vec4(1.0f));
            vertexLitTexturedPipeline.PushModelAndTint(cmd, model, tint);
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
    colorPipeline.Shutdown();
    texturedColorPipeline.Shutdown();
    shadedPipeline.Shutdown();
    vertexLitPipeline.Shutdown();
    vertexLitTexturedPipeline.Shutdown();
    destroyDepthResources();
    vkDestroyRenderPass(device, renderPass, nullptr);

    // destroys every RHITexture -- must happen before context.Shutdown(), same reasoning
    // as meshCache.clear() below.
    materialTextureCache.clear();
    // also frees every descriptor set allocated from it.
    vkDestroyDescriptorPool(device, materialTextureDescriptorPool, nullptr);
    // Lighting phase A -- frees frameLightingDescriptorSets too, must happen before
    // frameLightingBuffers themselves are destroyed just below.
    vkDestroyDescriptorPool(device, frameLightingDescriptorPool, nullptr);
    for (int i = 0; i < kMaxFramesInFlight; ++i) {
        frameLightingBuffers[i].Shutdown();
    }
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
