#ifndef GFX_VULKAN_PIPELINE_HPP
#define GFX_VULKAN_PIPELINE_HPP

#include "context.hpp"
#include "descriptor.hpp"

#include <filesystem>
#include <vector>

namespace gfx {

namespace vulkan {

class pipeline_t;

struct pipeline_builder_t {
    pipeline_builder_t();
    pipeline_builder_t& add_shader(const std::filesystem::path& shader_path);
    pipeline_builder_t& add_dynamic_state(VkDynamicState state);

    pipeline_builder_t& add_descriptor_set_layout(core::ref<descriptor_set_layout_t> descriptor_set_layout);

    pipeline_builder_t& add_default_color_blend_attachment_state();
    pipeline_builder_t& add_color_blend_attachment_state(const VkPipelineColorBlendAttachmentState& pipeline_color_blend_attachment_state);

    pipeline_builder_t& set_depth_stencil_state(const VkPipelineDepthStencilStateCreateInfo& pipeline_depth_stencil_state);

    pipeline_builder_t& add_vertex_input_binding_description(uint32_t binding, uint32_t stride, VkVertexInputRate input_rate);
    pipeline_builder_t& add_vertex_input_attribute_description(uint32_t binding, uint32_t location, VkFormat format, uint32_t offset);   
    pipeline_builder_t& set_vertex_input_binding_description_vector(const std::vector<VkVertexInputBindingDescription>& val);
    pipeline_builder_t& set_vertex_input_attribute_description_vector(const std::vector<VkVertexInputAttributeDescription>& val);        

    core::ref<pipeline_t> build(core::ref<context_t> context, VkRenderPass renderpass);

    std::vector<VkDynamicState> dynamic_states;
    std::vector<std::filesystem::path> shader_paths;
    std::vector<VkVertexInputAttributeDescription> vertex_input_attribute_descriptions;
    std::vector<VkVertexInputBindingDescription> vertex_input_binding_descriptions;
    std::vector<VkDescriptorSetLayout> descriptor_set_layouts; 
    std::vector<VkPipelineColorBlendAttachmentState> pipeline_color_blend_attachment_states;
    VkPipelineDepthStencilStateCreateInfo pipeline_depth_stencil_state_create_info{};
};

class pipeline_t {
public:
    pipeline_t(core::ref<context_t> context, VkPipelineLayout pipeline_layout, VkPipeline pipeline, VkPipelineBindPoint pipeline_bind_point);
    ~pipeline_t();

    void bind(VkCommandBuffer command_buffer);

    VkPipelineLayout& pipeline_layout() { return _pipeline_layout; }

private:
    core::ref<context_t> _context;
    VkPipelineLayout _pipeline_layout;
    VkPipeline _pipeline;
    VkPipelineBindPoint _pipeline_bind_point;
};

} // namespace vulkan

} // namespace gfx

#endif