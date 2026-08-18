#include "engine/rhi/RHIContext.h"

#include <cstdio>
#include <cstring>
#include <set>
#include <vector>

namespace engine::rhi {

namespace {

#ifndef NDEBUG
constexpr bool kEnableValidationLayers = true;
#else
constexpr bool kEnableValidationLayers = false;
#endif

constexpr const char* kValidationLayerName = "VK_LAYER_KHRONOS_validation";
constexpr const char* kSwapchainExtensionName = "VK_KHR_swapchain";

bool CheckVkResult(VkResult result, const char* what) {
    if (result != VK_SUCCESS) {
        std::fprintf(stderr, "RHIContext: %s failed (VkResult = %d)\n", what,
                     static_cast<int>(result));
        return false;
    }
    return true;
}

bool ValidationLayerAvailable() {
    std::uint32_t layerCount = 0;
    vkEnumerateInstanceLayerProperties(&layerCount, nullptr);
    std::vector<VkLayerProperties> layers(layerCount);
    vkEnumerateInstanceLayerProperties(&layerCount, layers.data());

    for (const VkLayerProperties& layer : layers) {
        if (std::strcmp(layer.layerName, kValidationLayerName) == 0) {
            return true;
        }
    }
    return false;
}

VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT severity,
                                              VkDebugUtilsMessageTypeFlagsEXT /*type*/,
                                              const VkDebugUtilsMessengerCallbackDataEXT* data,
                                              void* /*userData*/) {
    if (severity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
        std::fprintf(stderr, "[Vulkan] %s\n", data->pMessage);
    }
    return VK_FALSE;
}

} // namespace

RHIContext::~RHIContext() {
    Shutdown();
}

bool RHIContext::Init(platform::IDisplayBackend& backend, const char* appName) {
    if (volkInitialize() != VK_SUCCESS) {
        std::fprintf(stderr, "RHIContext: volkInitialize failed (Vulkan loader not found)\n");
        return false;
    }

    if (!CreateInstance(backend, appName)) {
        return false;
    }
    volkLoadInstance(m_instance);

    if (kEnableValidationLayers && !CreateDebugMessenger()) {
        // Non-fatal: validation is a development aid, not a hard requirement.
        std::fprintf(stderr, "RHIContext: continuing without a debug messenger\n");
    }

    m_surface = backend.CreateVulkanSurface(m_instance);
    if (m_surface == VK_NULL_HANDLE) {
        std::fprintf(stderr, "RHIContext: failed to create the Vulkan surface\n");
        return false;
    }

    if (!SelectPhysicalDevice()) {
        return false;
    }

    if (!CreateLogicalDevice()) {
        return false;
    }
    volkLoadDevice(m_device);

    if (!CreateAllocator()) {
        return false;
    }

    return true;
}

bool RHIContext::CreateInstance(platform::IDisplayBackend& backend, const char* appName) {
    VkApplicationInfo appInfo{};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = appName;
    appInfo.applicationVersion = VK_MAKE_VERSION(0, 1, 0);
    appInfo.pEngineName = "Pi-Engine";
    appInfo.engineVersion = VK_MAKE_VERSION(0, 1, 0);
    appInfo.apiVersion = VK_API_VERSION_1_2;

    std::vector<const char*> extensions = backend.GetRequiredVulkanExtensions();
    if (kEnableValidationLayers) {
        extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }

    std::vector<const char*> layers;
    const bool useValidation = kEnableValidationLayers && ValidationLayerAvailable();
    if (useValidation) {
        layers.push_back(kValidationLayerName);
    } else if (kEnableValidationLayers) {
        std::fprintf(stderr,
                     "RHIContext: validation layers requested but %s is not available\n",
                     kValidationLayerName);
    }

    VkInstanceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;
    createInfo.enabledExtensionCount = static_cast<std::uint32_t>(extensions.size());
    createInfo.ppEnabledExtensionNames = extensions.data();
    createInfo.enabledLayerCount = static_cast<std::uint32_t>(layers.size());
    createInfo.ppEnabledLayerNames = layers.data();

    return CheckVkResult(vkCreateInstance(&createInfo, nullptr, &m_instance), "vkCreateInstance");
}

bool RHIContext::CreateDebugMessenger() {
    if (vkCreateDebugUtilsMessengerEXT == nullptr) {
        return false; // Extension not loaded (instance created without VK_EXT_debug_utils).
    }

    VkDebugUtilsMessengerCreateInfoEXT createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
                                  VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                                  VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                              VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                              VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    createInfo.pfnUserCallback = DebugCallback;

    return CheckVkResult(
        vkCreateDebugUtilsMessengerEXT(m_instance, &createInfo, nullptr, &m_debugMessenger),
        "vkCreateDebugUtilsMessengerEXT");
}

RHIContext::QueueFamilyIndices RHIContext::FindQueueFamilies(VkPhysicalDevice device) const {
    QueueFamilyIndices indices;

    std::uint32_t count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &count, nullptr);
    std::vector<VkQueueFamilyProperties> families(count);
    vkGetPhysicalDeviceQueueFamilyProperties(device, &count, families.data());

    for (std::uint32_t i = 0; i < count; ++i) {
        if ((families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0) {
            indices.graphics = i;
        }

        VkBool32 presentSupport = VK_FALSE;
        vkGetPhysicalDeviceSurfaceSupportKHR(device, i, m_surface, &presentSupport);
        if (presentSupport == VK_TRUE) {
            indices.present = i;
        }

        if (indices.IsComplete()) {
            break;
        }
    }

    return indices;
}

bool RHIContext::IsDeviceSuitable(VkPhysicalDevice device) const {
    std::uint32_t extensionCount = 0;
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);
    std::vector<VkExtensionProperties> available(extensionCount);
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, available.data());

    bool hasSwapchain = false;
    for (const VkExtensionProperties& ext : available) {
        if (std::strcmp(ext.extensionName, kSwapchainExtensionName) == 0) {
            hasSwapchain = true;
            break;
        }
    }
    if (!hasSwapchain) {
        return false;
    }

    std::uint32_t formatCount = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(device, m_surface, &formatCount, nullptr);
    std::uint32_t presentModeCount = 0;
    vkGetPhysicalDeviceSurfacePresentModesKHR(device, m_surface, &presentModeCount, nullptr);
    if (formatCount == 0 || presentModeCount == 0) {
        return false;
    }

    return FindQueueFamilies(device).IsComplete();
}

bool RHIContext::SelectPhysicalDevice() {
    std::uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(m_instance, &deviceCount, nullptr);
    if (deviceCount == 0) {
        std::fprintf(stderr, "RHIContext: no Vulkan-capable physical device found\n");
        return false;
    }

    std::vector<VkPhysicalDevice> devices(deviceCount);
    vkEnumeratePhysicalDevices(m_instance, &deviceCount, devices.data());

    // Pi4/Pi5 only ever expose one GPU (VideoCore), so "first suitable device" is enough
    // for M0. A discrete-GPU preference for the Desktop hardware profile can be added
    // later without changing this function's contract.
    for (VkPhysicalDevice device : devices) {
        if (IsDeviceSuitable(device)) {
            m_physicalDevice = device;
            const QueueFamilyIndices indices = FindQueueFamilies(device);
            m_graphicsQueueFamily = indices.graphics.value();
            m_presentQueueFamily = indices.present.value();

            VkPhysicalDeviceProperties props{};
            vkGetPhysicalDeviceProperties(device, &props);
            std::fprintf(stderr, "RHIContext: selected physical device \"%s\"\n", props.deviceName);
            return true;
        }
    }

    std::fprintf(stderr, "RHIContext: no suitable physical device found\n");
    return false;
}

bool RHIContext::CreateLogicalDevice() {
    std::set<std::uint32_t> uniqueFamilies = {m_graphicsQueueFamily, m_presentQueueFamily};

    std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
    const float queuePriority = 1.0f;
    for (std::uint32_t family : uniqueFamilies) {
        VkDeviceQueueCreateInfo queueCreateInfo{};
        queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueCreateInfo.queueFamilyIndex = family;
        queueCreateInfo.queueCount = 1;
        queueCreateInfo.pQueuePriorities = &queuePriority;
        queueCreateInfos.push_back(queueCreateInfo);
    }

    VkPhysicalDeviceFeatures deviceFeatures{};

    const char* deviceExtensions[] = {kSwapchainExtensionName};

    VkDeviceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    createInfo.queueCreateInfoCount = static_cast<std::uint32_t>(queueCreateInfos.size());
    createInfo.pQueueCreateInfos = queueCreateInfos.data();
    createInfo.pEnabledFeatures = &deviceFeatures;
    createInfo.enabledExtensionCount =
        static_cast<std::uint32_t>(sizeof(deviceExtensions) / sizeof(deviceExtensions[0]));
    createInfo.ppEnabledExtensionNames = deviceExtensions;

    if (!CheckVkResult(vkCreateDevice(m_physicalDevice, &createInfo, nullptr, &m_device),
                        "vkCreateDevice")) {
        return false;
    }

    vkGetDeviceQueue(m_device, m_graphicsQueueFamily, 0, &m_graphicsQueue);
    vkGetDeviceQueue(m_device, m_presentQueueFamily, 0, &m_presentQueue);
    return true;
}

bool RHIContext::CreateAllocator() {
    // VMA_STATIC_VULKAN_FUNCTIONS=0 / VMA_DYNAMIC_VULKAN_FUNCTIONS=1 are set where
    // VMA_IMPLEMENTATION is defined (src/rhi/VmaImplementation.cpp). With volk providing
    // the loader, VMA only needs these two ProcAddr entry points -- it resolves every
    // other Vulkan function it needs by itself.
    VmaVulkanFunctions vulkanFunctions{};
    vulkanFunctions.vkGetInstanceProcAddr = vkGetInstanceProcAddr;
    vulkanFunctions.vkGetDeviceProcAddr = vkGetDeviceProcAddr;

    VmaAllocatorCreateInfo allocatorInfo{};
    allocatorInfo.vulkanApiVersion = VK_API_VERSION_1_2;
    allocatorInfo.physicalDevice = m_physicalDevice;
    allocatorInfo.device = m_device;
    allocatorInfo.instance = m_instance;
    allocatorInfo.pVulkanFunctions = &vulkanFunctions;

    return CheckVkResult(vmaCreateAllocator(&allocatorInfo, &m_allocator), "vmaCreateAllocator");
}

void RHIContext::Shutdown() {
    if (m_allocator != VK_NULL_HANDLE) {
        vmaDestroyAllocator(m_allocator);
        m_allocator = VK_NULL_HANDLE;
    }
    if (m_device != VK_NULL_HANDLE) {
        vkDestroyDevice(m_device, nullptr);
        m_device = VK_NULL_HANDLE;
    }
    if (m_surface != VK_NULL_HANDLE) {
        vkDestroySurfaceKHR(m_instance, m_surface, nullptr);
        m_surface = VK_NULL_HANDLE;
    }
    if (m_debugMessenger != VK_NULL_HANDLE && vkDestroyDebugUtilsMessengerEXT != nullptr) {
        vkDestroyDebugUtilsMessengerEXT(m_instance, m_debugMessenger, nullptr);
        m_debugMessenger = VK_NULL_HANDLE;
    }
    if (m_instance != VK_NULL_HANDLE) {
        vkDestroyInstance(m_instance, nullptr);
        m_instance = VK_NULL_HANDLE;
    }
}

} // namespace engine::rhi
