#ifndef GFX_VULKAN_DESCRIPTOR_HPP
#define GFX_VULKAN_DESCRIPTOR_HPP

#include "context.hpp"
#include "buffer.hpp"

namespace gfx {

namespace vulkan {

class DescriptorSetLayout {
public:
    struct Builder {
        Builder& addLayoutBinding(uint32_t binding, VkDescriptorType descriptorType, uint32_t count, VkShaderStageFlags shaderStageFlags);
        
        core::ref<DescriptorSetLayout> build(core::ref<Context> context);

        std::vector<VkDescriptorSetLayoutBinding> descriptorSetLayoutBindings{};
    };

    DescriptorSetLayout(core::ref<Context> context, VkDescriptorSetLayout descriptorSetLayout);
    ~DescriptorSetLayout();

    VkDescriptorSetLayout& descriptorSetLayout() { return m_descriptorSetLayout; } 

    DescriptorSetLayout(const DescriptorSetLayout&) = delete;
    DescriptorSetLayout& operator=(const DescriptorSetLayout&) = delete;

private:
    core::ref<Context> m_context;
    VkDescriptorSetLayout m_descriptorSetLayout{};
};

class DescriptorSet {
public:
    struct Builder {
        core::ref<DescriptorSet> build(core::ref<Context> context, core::ref<DescriptorSetLayout> descriptorSetLayout);
    };

    DescriptorSet(core::ref<Context> context, VkDescriptorSet descriptorSet);
    ~DescriptorSet();

    DescriptorSet(const DescriptorSet&) = delete;
    DescriptorSet& operator=(const DescriptorSet&) = delete;

    VkDescriptorSet& descriptorSet() { return m_descriptorSet; }

    struct Write {
        Write(core::ref<Context> context, VkDescriptorSet descriptorSet);
        Write& pushImageInfo(uint32_t binding, uint32_t count, const VkDescriptorImageInfo& descriptorImageInfo);
        Write& pushBufferInfo(uint32_t binding, uint32_t count, const VkDescriptorBufferInfo& descriptorBuffeInfo);

        void update();

        std::vector<VkWriteDescriptorSet> writes;
        core::ref<Context> context;
        VkDescriptorSet descriptorSet{};
    };    

    Write& write();

private:
    core::ref<Context> m_context;
    VkDescriptorSet m_descriptorSet{}; 
};

} // namespace vulkan

} // namespace gfx

#endif