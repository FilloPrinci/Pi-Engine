// Vulkan Memory Allocator is a single-header ("STB-style") library: exactly one
// translation unit must define VMA_IMPLEMENTATION before including it to generate the
// actual function bodies. Every other file only sees declarations via <vk_mem_alloc.h>
// (pulled in transitively through engine/rhi/RHIContext.h).
//
// VMA_STATIC_VULKAN_FUNCTIONS=0: don't link directly against libvulkan -- this project
// loads Vulkan dynamically through volk (docs/01 section 3.2/4).
// VMA_DYNAMIC_VULKAN_FUNCTIONS=1: resolve the rest of the Vulkan API at vmaCreateAllocator
// time from the vkGetInstanceProcAddr/vkGetDeviceProcAddr pair volk provides
// (RHIContext::CreateAllocator, engine/src/rhi/RHIContext.cpp).
#define VMA_STATIC_VULKAN_FUNCTIONS 0
#define VMA_DYNAMIC_VULKAN_FUNCTIONS 1
#define VMA_IMPLEMENTATION
#include <vk_mem_alloc.h>
