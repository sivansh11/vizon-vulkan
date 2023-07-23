#ifndef GFX_VULKAN_DESCRIPTOR_HPP
#define GFX_VULKAN_DESCRIPTOR_HPP

#include "context.hpp"
#include "buffer.hpp"

namespace gfx {

namespace vulkan {

class descriptor_set_t;
class descriptor_set_layout_t;

struct descriptor_set_layout_builder_t {
    descriptor_set_layout_builder_t& addLayoutBinding(uint32_t binding, VkDescriptorType descriptorType, uint32_t count, VkShaderStageFlags shaderStageFlags);
    
    core::ref<descriptor_set_layout_t> build(core::ref<context_t> context);

    std::vector<VkDescriptorSetLayoutBinding> descriptorSetLayoutBindings{};
};

class descriptor_set_layout_t {
public:

    descriptor_set_layout_t(core::ref<context_t> context, VkDescriptorSetLayout descriptorSetLayout);
    ~descriptor_set_layout_t();

    core::ref<descriptor_set_t> new_descriptor_set();

    VkDescriptorSetLayout& descriptor_set_layout() { return _descriptor_set_layout; } 

    descriptor_set_layout_t(const descriptor_set_layout_t&) = delete;
    descriptor_set_layout_t& operator=(const descriptor_set_layout_t&) = delete;

private:
    core::ref<context_t> _context;
    VkDescriptorSetLayout _descriptor_set_layout{};
};

struct descriptor_set_builder_t {
    core::ref<descriptor_set_t> build(core::ref<context_t> context, core::ref<descriptor_set_layout_t> descriptorSetLayout);
    core::ref<descriptor_set_t> build(core::ref<context_t> context, VkDescriptorSetLayout descriptorSetLayout);
};

class descriptor_set_t {
public:

    descriptor_set_t(core::ref<context_t> context, VkDescriptorSet descriptorSet);
    ~descriptor_set_t();

    descriptor_set_t(const descriptor_set_t&) = delete;
    descriptor_set_t& operator=(const descriptor_set_t&) = delete;

    VkDescriptorSet& descriptorSet() { return m_descriptorSet; }

    struct Write {
        Write(core::ref<context_t> context, VkDescriptorSet descriptorSet);
        Write& pushImageInfo(uint32_t binding, uint32_t count, const VkDescriptorImageInfo& descriptorImageInfo);
        Write& pushBufferInfo(uint32_t binding, uint32_t count, const VkDescriptorBufferInfo& descriptorBuffeInfo);

        void update();

        std::vector<VkWriteDescriptorSet> writes;
        core::ref<context_t> context;
        VkDescriptorSet descriptorSet{};
    };    

    Write& write();

private:
    core::ref<context_t> m_context;
    VkDescriptorSet m_descriptorSet{}; 
};

} // namespace vulkan

} // namespace gfx

#endif