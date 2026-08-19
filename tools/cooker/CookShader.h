#pragma once

// `cooker shader <input.vert|.frag> <output.spv>` (docs/01 section 12.2). Output is plain
// SPIR-V -- unlike meshes, there's no reason for a custom binary wrapper here: SPIR-V
// already *is* the "cooked" format Vulkan wants, RHIPipeline.cpp's ReadFile()/
// CreateShaderModule() load it exactly the way they always have (this milestone only
// changes *who produces* the .spv, via shaderc instead of every sample's own
// find_program(glslc) + add_custom_command, not what gets loaded at runtime).
int CookShader(int argc, char** argv);
