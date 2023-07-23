#ifndef GFX_VULKAN_PIPELINE_HPP
#define GFX_VULKAN_PIPELINE_HPP

#include "context.hpp"
#include "descriptor.hpp"

#include <filesystem>
#include <vector>

namespace gfx {

namespace vulkan {

class Pipeline {
public:

    struct Builder {
        Builder();
        Builder& addShader(const std::filesystem::path& shaderPath);
        Builder& addDynamicState(VkDynamicState state);

        Builder& addDescriptorSetLayout(core::ref<descriptor_set_layout_t> descriptorSetLayout);

        Builder& addDefaultColorBlendAttachmentState();
        Builder& addColorBlendAttachmentState(const VkPipelineColorBlendAttachmentState& pipelineColorBlendAttachmentState);

        Builder& setDepthStencilState(const VkPipelineDepthStencilStateCreateInfo& pipelineDepthStencilState);

        Builder& addVertexInputBindingDescription(uint32_t binding, uint32_t stride, VkVertexInputRate inputRate);
        Builder& addVertexInputAttributeDescription(uint32_t binding, uint32_t location, VkFormat format, uint32_t offset);   
        Builder& setVertexInputBindingDescriptionVector(const std::vector<VkVertexInputBindingDescription>& val);
        Builder& setVertexInputAttributeDescriptionVector(const std::vector<VkVertexInputAttributeDescription>& val);        

        core::ref<Pipeline> build(core::ref<context_t> context, VkRenderPass renderPass);

        std::vector<VkDynamicState> dynamicStates;
        std::vector<std::filesystem::path> shaderPaths;
        std::vector<VkVertexInputAttributeDescription> vertexInputAttributeDescriptions;
        std::vector<VkVertexInputBindingDescription> vertexInputBindingDescriptions;
        std::vector<VkDescriptorSetLayout> descriptorSetLayouts; 
        std::vector<VkPipelineColorBlendAttachmentState> pipelineColorBlendAttachmentStates;
        VkPipelineDepthStencilStateCreateInfo pipelineDepthStencilStateCreateInfo{};
    };

    Pipeline(core::ref<context_t> context, VkPipelineLayout pipelineLayout, VkPipeline pipeline, VkPipelineBindPoint pipelineBindPoint);
    ~Pipeline();

    void bind(VkCommandBuffer commandBuffer);

    VkPipelineLayout& pipelineLayout() { return m_pipelineLayout; }

private:
    core::ref<context_t> m_context;
    VkPipelineLayout m_pipelineLayout;
    VkPipeline m_pipeline;
    VkPipelineBindPoint m_pipelineBindPoint;
};

} // namespace vulkan

} // namespace gfx

#endif