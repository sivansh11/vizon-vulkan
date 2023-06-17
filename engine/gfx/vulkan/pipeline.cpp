#include "pipeline.hpp"

#include "core/log.hpp"

#include <fstream>

namespace utils {

static std::vector<char> readFile(const std::filesystem::path& filename) {
    std::ifstream file(filename, std::ios::ate | std::ios::binary);

    if (!file.is_open()) {
        ERROR("Failed to open file");
        std::terminate();
    }

    size_t fileSize = (size_t) file.tellg();
    std::vector<char> buffer(fileSize);

    file.seekg(0);
    file.read(buffer.data(), fileSize);

    file.close();

    return buffer;
}

} // namespace utils


namespace gfx::vulkan {

// Pipeline::Builder& Pipeline::Builder::
Pipeline::Builder& Pipeline::Builder::addShader(const std::filesystem::path& shaderPath) {
    shaderPaths.push_back(shaderPath);
    return *this;
}

Pipeline::Builder& Pipeline::Builder::addDynamicState(VkDynamicState state) {
    dynamicStates.push_back(state);
    return *this;
}

Pipeline::Builder& Pipeline::Builder::addDescriptorSetLayout(std::shared_ptr<DescriptorSetLayout> descriptorSetLayout) {
    descriptorSetLayouts.push_back(descriptorSetLayout->descriptorSetLayout());
    return *this;
}

Pipeline::Builder& Pipeline::Builder::addDefaultColorBlendAttachmentState() {
    VkPipelineColorBlendAttachmentState colorBlendAttachment{};
    colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    colorBlendAttachment.blendEnable = VK_FALSE;
    colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE; // Optional
    colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO; // Optional
    colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD; // Optional
    colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE; // Optional
    colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO; // Optional
    colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD; // Optional
    pipelineColorBlendAttachmentStates.push_back(colorBlendAttachment);
    return *this;
}

Pipeline::Builder& Pipeline::Builder::addColorBlendAttachmentState(const VkPipelineColorBlendAttachmentState& pipelineColorBlendAttachmenState) {
    pipelineColorBlendAttachmentStates.push_back(pipelineColorBlendAttachmenState);
    return *this;
}

Pipeline::Builder& Pipeline::Builder::addVertexInputBindingDescription(uint32_t binding, uint32_t stride, VkVertexInputRate inputRate) {
    VkVertexInputBindingDescription vertexInputBindingDescription{};
    vertexInputBindingDescription.binding = binding;
    vertexInputBindingDescription.stride = stride;
    vertexInputBindingDescription.inputRate = inputRate;
    vertexInputBindingDescriptions.push_back(vertexInputBindingDescription);
    return *this;
}

Pipeline::Builder& Pipeline::Builder::addVertexInputAttributeDescription(uint32_t binding, uint32_t location, VkFormat format, uint32_t offset) {
    VkVertexInputAttributeDescription vertexInputAttributeDescription{};
    vertexInputAttributeDescription.binding = binding;
    vertexInputAttributeDescription.format = format;
    vertexInputAttributeDescription.location = location;
    vertexInputAttributeDescription.offset = offset;
    vertexInputAttributeDescriptions.push_back(vertexInputAttributeDescription);
    return *this;
}

Pipeline::Builder& Pipeline::Builder::setVertexInputBindingDescriptionVector(const std::vector<VkVertexInputBindingDescription>& val) {
    vertexInputBindingDescriptions = val;
    return *this;
}

Pipeline::Builder& Pipeline::Builder::setVertexInputAttributeDescriptionVector(const std::vector<VkVertexInputAttributeDescription>& val) {
    vertexInputAttributeDescriptions = val;
    return *this;
}   



std::shared_ptr<Pipeline> Pipeline::Builder::build(std::shared_ptr<Context> context, VkRenderPass renderPass) {
    std::vector<VkPipelineShaderStageCreateInfo> pipelineShaderStageCreateInfos{};

    VkPipelineBindPoint pipelineBindPoint;

    for (auto& shaderPath : shaderPaths) {
        auto code = utils::readFile(shaderPath);
        
        VkShaderModuleCreateInfo shaderModuleCreateInfo{};
        shaderModuleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        shaderModuleCreateInfo.codeSize = code.size();
        shaderModuleCreateInfo.pCode = reinterpret_cast<const uint32_t *>(code.data());

        VkShaderModule shaderModule{};
        if (vkCreateShaderModule(context->device(), &shaderModuleCreateInfo, nullptr, &shaderModule) != VK_SUCCESS) {
            ERROR("Failed to create shader module");
            std::terminate();
        }

        VkPipelineShaderStageCreateInfo pipelineShaderStageCreateInfo{};
        pipelineShaderStageCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        pipelineShaderStageCreateInfo.module = shaderModule;
        pipelineShaderStageCreateInfo.pName = "main";

        if (shaderPath.string().find("vert") != std::string::npos) {
            pipelineShaderStageCreateInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
            // TODO: make this more robust
            pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        }
        if (shaderPath.string().find("frag") != std::string::npos) {
            pipelineShaderStageCreateInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        }

        pipelineShaderStageCreateInfos.push_back(pipelineShaderStageCreateInfo);
    }

    VkPipelineDynamicStateCreateInfo dynamicState{};
    dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
    dynamicState.pDynamicStates = reinterpret_cast<VkDynamicState *>(dynamicStates.data());

    VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
    vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInputInfo.vertexBindingDescriptionCount = vertexInputBindingDescriptions.size();
    vertexInputInfo.pVertexBindingDescriptions = vertexInputBindingDescriptions.data();
    vertexInputInfo.vertexAttributeDescriptionCount = vertexInputAttributeDescriptions.size();
    vertexInputInfo.pVertexAttributeDescriptions = vertexInputAttributeDescriptions.data();

    VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    inputAssembly.primitiveRestartEnable = VK_FALSE;

    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = (float) context->swapChainExtent().width;
    viewport.height = (float) context->swapChainExtent().height;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    VkRect2D scissor{};
    scissor.offset = {0, 0};
    scissor.extent = context->swapChainExtent();

    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.pViewports = &viewport;
    viewportState.scissorCount = 1;
    viewportState.pScissors = &scissor;

    VkPipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.depthClampEnable = VK_FALSE;
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.lineWidth = 1.0f;
    rasterizer.cullMode = VK_CULL_MODE_NONE;
    rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;
    rasterizer.depthBiasEnable = VK_FALSE;
    rasterizer.depthBiasConstantFactor = 0.0f; // Optional
    rasterizer.depthBiasClamp = 0.0f; // Optional
    rasterizer.depthBiasSlopeFactor = 0.0f; // Optional

    VkPipelineMultisampleStateCreateInfo multisampling{};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.sampleShadingEnable = VK_FALSE;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    multisampling.minSampleShading = 1.0f; // Optional
    multisampling.pSampleMask = nullptr; // Optional
    multisampling.alphaToCoverageEnable = VK_FALSE; // Optional
    multisampling.alphaToOneEnable = VK_FALSE; // Optional

    VkPipelineColorBlendStateCreateInfo colorBlending{};
    colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.logicOpEnable = VK_FALSE;
    colorBlending.logicOp = VK_LOGIC_OP_COPY; // Optional
    colorBlending.attachmentCount = pipelineColorBlendAttachmentStates.size();
    colorBlending.pAttachments = pipelineColorBlendAttachmentStates.data();
    colorBlending.blendConstants[0] = 0.0f; // Optional
    colorBlending.blendConstants[1] = 0.0f; // Optional
    colorBlending.blendConstants[2] = 0.0f; // Optional
    colorBlending.blendConstants[3] = 0.0f; // Optional

    VkPipelineDepthStencilStateCreateInfo depthStencil{};
    depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthStencil.depthTestEnable = VK_TRUE;
    depthStencil.depthWriteEnable = VK_TRUE;
    depthStencil.depthCompareOp = VK_COMPARE_OP_LESS;
    depthStencil.depthBoundsTestEnable = VK_FALSE;
    depthStencil.minDepthBounds = 0.0f; // Optional
    depthStencil.maxDepthBounds = 1.0f; // Optional
    depthStencil.stencilTestEnable = VK_FALSE;
    depthStencil.front = {}; // Optional
    depthStencil.back = {}; // Optional


    VkPipelineLayout pipelineLayout;
    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = descriptorSetLayouts.size();
    pipelineLayoutInfo.pSetLayouts = descriptorSetLayouts.data();
    pipelineLayoutInfo.pushConstantRangeCount = 0; // Optional
    pipelineLayoutInfo.pPushConstantRanges = nullptr; // Optional

    if (vkCreatePipelineLayout(context->device(), &pipelineLayoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS) {
        throw std::runtime_error("failed to create pipeline layout!");
    }

    VkGraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount = static_cast<uint32_t>(pipelineShaderStageCreateInfos.size());
    pipelineInfo.pStages = pipelineShaderStageCreateInfos.data();
    pipelineInfo.pVertexInputState = &vertexInputInfo;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState = &multisampling;
    pipelineInfo.pDepthStencilState = &depthStencil;
    pipelineInfo.pColorBlendState = &colorBlending;
    pipelineInfo.pDynamicState = &dynamicState;
    pipelineInfo.layout = pipelineLayout;
    pipelineInfo.renderPass = renderPass;
    pipelineInfo.subpass = 0;

    VkPipeline pipeline;
    if (vkCreateGraphicsPipelines(context->device(), VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipeline) != VK_SUCCESS) {
        throw std::runtime_error("failed to create graphics pipeline!");
    }

    for (auto pipelineShaderStageCreateInfo : pipelineShaderStageCreateInfos) {
        vkDestroyShaderModule(context->device(), pipelineShaderStageCreateInfo.module, nullptr);
    }

    return std::make_shared<Pipeline>(context, pipelineLayout, pipeline, pipelineBindPoint);
}

Pipeline::Pipeline(std::shared_ptr<Context> context, VkPipelineLayout pipelineLayout, VkPipeline pipeline, VkPipelineBindPoint pipelineBindPoint) 
  : m_context(context),
    m_pipelineLayout(pipelineLayout),
    m_pipeline(pipeline),
    m_pipelineBindPoint(pipelineBindPoint) {
    INFO("Created pipeline");
}

Pipeline::~Pipeline() {
    vkDestroyPipelineLayout(m_context->device(), m_pipelineLayout, nullptr);
    vkDestroyPipeline(m_context->device(), m_pipeline, nullptr);
}

void Pipeline::bind(VkCommandBuffer commandBuffer) {
    vkCmdBindPipeline(commandBuffer, m_pipelineBindPoint, m_pipeline);
}

} // namespace gfx::vulkan
