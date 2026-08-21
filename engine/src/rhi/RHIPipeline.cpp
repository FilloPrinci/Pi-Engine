#include "engine/rhi/RHIPipeline.h"

#include "engine/rhi/RHIContext.h"

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <fstream>

namespace engine::rhi {

namespace {

std::vector<char> ReadFile(const char* path) {
    std::ifstream file(path, std::ios::ate | std::ios::binary);
    if (!file.is_open()) {
        std::fprintf(stderr, "RHIPipeline: could not open shader file \"%s\"\n", path);
        return {};
    }
    const std::streamsize size = file.tellg();
    std::vector<char> buffer(static_cast<std::size_t>(size));
    file.seekg(0);
    file.read(buffer.data(), size);
    return buffer;
}

VkShaderModule CreateShaderModule(VkDevice device, const std::vector<char>& code) {
    if (code.empty()) {
        return VK_NULL_HANDLE;
    }

    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = code.size();
    createInfo.pCode = reinterpret_cast<const std::uint32_t*>(code.data());

    VkShaderModule module = VK_NULL_HANDLE;
    if (vkCreateShaderModule(device, &createInfo, nullptr, &module) != VK_SUCCESS) {
        std::fprintf(stderr, "RHIPipeline: vkCreateShaderModule failed\n");
        return VK_NULL_HANDLE;
    }
    return module;
}

} // namespace

RHIPipeline::~RHIPipeline() {
    Shutdown();
}

bool RHIPipeline::Init(RHIContext& context, const RHIPipelineDesc& desc) {
    m_context = &context;
    VkDevice device = context.GetDevice();

    VkShaderModule vertModule = CreateShaderModule(device, ReadFile(desc.vertexShaderPath));
    VkShaderModule fragModule = CreateShaderModule(device, ReadFile(desc.fragmentShaderPath));
    if (vertModule == VK_NULL_HANDLE || fragModule == VK_NULL_HANDLE) {
        if (vertModule != VK_NULL_HANDLE) {
            vkDestroyShaderModule(device, vertModule, nullptr);
        }
        if (fragModule != VK_NULL_HANDLE) {
            vkDestroyShaderModule(device, fragModule, nullptr);
        }
        return false;
    }

    VkPipelineShaderStageCreateInfo vertStage{};
    vertStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vertStage.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vertStage.module = vertModule;
    vertStage.pName = "main";

    VkPipelineShaderStageCreateInfo fragStage{};
    fragStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    fragStage.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    fragStage.module = fragModule;
    fragStage.pName = "main";

    VkPipelineShaderStageCreateInfo stages[] = {vertStage, fragStage};

    VkPipelineVertexInputStateCreateInfo vertexInput{};
    vertexInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInput.vertexBindingDescriptionCount = static_cast<std::uint32_t>(desc.vertexBindings.size());
    vertexInput.pVertexBindingDescriptions = desc.vertexBindings.data();
    vertexInput.vertexAttributeDescriptionCount =
        static_cast<std::uint32_t>(desc.vertexAttributes.size());
    vertexInput.pVertexAttributeDescriptions = desc.vertexAttributes.data();

    VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = static_cast<float>(desc.viewportExtent.width);
    viewport.height = static_cast<float>(desc.viewportExtent.height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    VkRect2D scissor{};
    scissor.offset = {0, 0};
    scissor.extent = desc.viewportExtent;

    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.pViewports = &viewport;
    viewportState.scissorCount = 1;
    viewportState.pScissors = &scissor;

    VkPipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.lineWidth = 1.0f;
    rasterizer.cullMode = desc.cullMode;
    rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;

    VkPipelineMultisampleStateCreateInfo multisampling{};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineDepthStencilStateCreateInfo depthStencil{};
    depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthStencil.depthTestEnable = desc.depthTestEnable ? VK_TRUE : VK_FALSE;
    depthStencil.depthWriteEnable = desc.depthWriteEnable ? VK_TRUE : VK_FALSE;
    depthStencil.depthCompareOp = VK_COMPARE_OP_LESS;

    VkPipelineColorBlendAttachmentState colorBlendAttachment{};
    colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                           VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    colorBlendAttachment.blendEnable = VK_FALSE; // Opaque geometry: keep HSR intact (docs/01 8.1.1).

    VkPipelineColorBlendStateCreateInfo colorBlending{};
    colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.attachmentCount = 1;
    colorBlending.pAttachments = &colorBlendAttachment;

    if (!desc.descriptorSetLayoutBindings.empty()) {
        VkDescriptorSetLayoutCreateInfo setLayoutInfo{};
        setLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        setLayoutInfo.bindingCount =
            static_cast<std::uint32_t>(desc.descriptorSetLayoutBindings.size());
        setLayoutInfo.pBindings = desc.descriptorSetLayoutBindings.data();

        if (vkCreateDescriptorSetLayout(device, &setLayoutInfo, nullptr, &m_descriptorSetLayout) !=
            VK_SUCCESS) {
            std::fprintf(stderr, "RHIPipeline: vkCreateDescriptorSetLayout failed\n");
            vkDestroyShaderModule(device, vertModule, nullptr);
            vkDestroyShaderModule(device, fragModule, nullptr);
            return false;
        }
    }

    // Second descriptor set (set = 1) -- see RHIPipelineDesc::secondDescriptorSetLayoutBindings'
    // own comment. Only ever created alongside set 0 above (no pipeline in this codebase
    // needs set 1 without set 0).
    if (!desc.secondDescriptorSetLayoutBindings.empty()) {
        VkDescriptorSetLayoutCreateInfo secondSetLayoutInfo{};
        secondSetLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        secondSetLayoutInfo.bindingCount =
            static_cast<std::uint32_t>(desc.secondDescriptorSetLayoutBindings.size());
        secondSetLayoutInfo.pBindings = desc.secondDescriptorSetLayoutBindings.data();

        if (vkCreateDescriptorSetLayout(device, &secondSetLayoutInfo, nullptr,
                                         &m_secondDescriptorSetLayout) != VK_SUCCESS) {
            std::fprintf(stderr, "RHIPipeline: vkCreateDescriptorSetLayout (second set) failed\n");
            if (m_descriptorSetLayout != VK_NULL_HANDLE) {
                vkDestroyDescriptorSetLayout(device, m_descriptorSetLayout, nullptr);
                m_descriptorSetLayout = VK_NULL_HANDLE;
            }
            vkDestroyShaderModule(device, vertModule, nullptr);
            vkDestroyShaderModule(device, fragModule, nullptr);
            return false;
        }
    }

    VkDescriptorSetLayout setLayouts[2] = {m_descriptorSetLayout, m_secondDescriptorSetLayout};
    std::uint32_t setLayoutCount = 0;
    if (m_descriptorSetLayout != VK_NULL_HANDLE) {
        ++setLayoutCount;
        if (m_secondDescriptorSetLayout != VK_NULL_HANDLE) {
            ++setLayoutCount;
        }
    }

    VkPipelineLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layoutInfo.setLayoutCount = setLayoutCount;
    layoutInfo.pSetLayouts = setLayouts;
    layoutInfo.pushConstantRangeCount = static_cast<std::uint32_t>(desc.pushConstantRanges.size());
    layoutInfo.pPushConstantRanges = desc.pushConstantRanges.data();

    if (vkCreatePipelineLayout(device, &layoutInfo, nullptr, &m_layout) != VK_SUCCESS) {
        std::fprintf(stderr, "RHIPipeline: vkCreatePipelineLayout failed\n");
        if (m_descriptorSetLayout != VK_NULL_HANDLE) {
            vkDestroyDescriptorSetLayout(device, m_descriptorSetLayout, nullptr);
            m_descriptorSetLayout = VK_NULL_HANDLE;
        }
        if (m_secondDescriptorSetLayout != VK_NULL_HANDLE) {
            vkDestroyDescriptorSetLayout(device, m_secondDescriptorSetLayout, nullptr);
            m_secondDescriptorSetLayout = VK_NULL_HANDLE;
        }
        vkDestroyShaderModule(device, vertModule, nullptr);
        vkDestroyShaderModule(device, fragModule, nullptr);
        return false;
    }

    VkGraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount = 2;
    pipelineInfo.pStages = stages;
    pipelineInfo.pVertexInputState = &vertexInput;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState = &multisampling;
    pipelineInfo.pDepthStencilState = &depthStencil;
    pipelineInfo.pColorBlendState = &colorBlending;
    pipelineInfo.layout = m_layout;
    pipelineInfo.renderPass = desc.renderPass;
    pipelineInfo.subpass = 0;

    const bool success =
        vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_pipeline) ==
        VK_SUCCESS;

    vkDestroyShaderModule(device, vertModule, nullptr);
    vkDestroyShaderModule(device, fragModule, nullptr);

    if (!success) {
        std::fprintf(stderr, "RHIPipeline: vkCreateGraphicsPipelines failed\n");
        vkDestroyPipelineLayout(device, m_layout, nullptr);
        m_layout = VK_NULL_HANDLE;
        if (m_descriptorSetLayout != VK_NULL_HANDLE) {
            vkDestroyDescriptorSetLayout(device, m_descriptorSetLayout, nullptr);
            m_descriptorSetLayout = VK_NULL_HANDLE;
        }
        if (m_secondDescriptorSetLayout != VK_NULL_HANDLE) {
            vkDestroyDescriptorSetLayout(device, m_secondDescriptorSetLayout, nullptr);
            m_secondDescriptorSetLayout = VK_NULL_HANDLE;
        }
        return false;
    }

    return true;
}

void RHIPipeline::Shutdown() {
    if (m_context == nullptr) {
        return;
    }
    VkDevice device = m_context->GetDevice();
    if (m_pipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(device, m_pipeline, nullptr);
        m_pipeline = VK_NULL_HANDLE;
    }
    if (m_layout != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(device, m_layout, nullptr);
        m_layout = VK_NULL_HANDLE;
    }
    if (m_descriptorSetLayout != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(device, m_descriptorSetLayout, nullptr);
        m_descriptorSetLayout = VK_NULL_HANDLE;
    }
    if (m_secondDescriptorSetLayout != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(device, m_secondDescriptorSetLayout, nullptr);
        m_secondDescriptorSetLayout = VK_NULL_HANDLE;
    }
    m_context = nullptr;
}

} // namespace engine::rhi
