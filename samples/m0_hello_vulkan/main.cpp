#include "engine/core/EngineVersion.h"

#include <cstdio>

// Milestone M0 — Hello Vulkan (docs/02 section 4, docs/03 section 5).
// Exit criterion: colored triangle on screen; RHIContext initializes, swapchain works,
// first Vulkan pipeline compiles and runs, verified on Pi4.
//
// TODO(M0): SDL2DisplayBackend::Init() -> RHIContext::Init() -> minimal graphics pipeline
// from shaders/m0_triangle.vert/.frag -> clear + draw + present loop.
//
// This stub only proves the toolchain end to end (engine_core links against
// volk/VMA/SDL2/glm and runs) before the real rendering code lands.
int main() {
    std::printf("Pi-Engine %s -- m0_hello_vulkan stub, milestone not yet implemented.\n",
                engine::core::GetEngineVersionString());
    return 0;
}
