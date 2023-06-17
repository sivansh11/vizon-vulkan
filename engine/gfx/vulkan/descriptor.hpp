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
        
        std::shared_ptr<DescriptorSetLayout> build(std::shared_ptr<Context> context);

        std::vector<VkDescriptorSetLayoutBinding> descriptorSetLayoutBindings{};
    };

    DescriptorSetLayout(std::shared_ptr<Context> context, VkDescriptorSetLayout descriptorSetLayout);
    ~DescriptorSetLayout();

    VkDescriptorSetLayout& descriptorSetLayout() { return m_descriptorSetLayout; } 

    DescriptorSetLayout(const DescriptorSetLayout&) = delete;
    DescriptorSetLayout& operator=(const DescriptorSetLayout&) = delete;

private:
    std::shared_ptr<Context> m_context;
    VkDescriptorSetLayout m_descriptorSetLayout{};
};

class DescriptorSet {
public:
    struct Builder {
        std::shared_ptr<DescriptorSet> build(std::shared_ptr<Context> context, std::shared_ptr<DescriptorSetLayout> descriptorSetLayout);
    };

    DescriptorSet(std::shared_ptr<Context> context, VkDescriptorSet descriptorSet);
    ~DescriptorSet();

    DescriptorSet(const DescriptorSet&) = delete;
    DescriptorSet& operator=(const DescriptorSet&) = delete;

    VkDescriptorSet& descriptorSet() { return m_descriptorSet; }

    struct Write {
        Write(std::shared_ptr<Context> context, VkDescriptorSet descriptorSet);
        Write& pushImageInfo(uint32_t binding, uint32_t count, const VkDescriptorImageInfo& descriptorImageInfo);
        Write& pushBufferInfo(uint32_t binding, uint32_t count, const VkDescriptorBufferInfo& descriptorBuffeInfo);

        void update();

        std::vector<VkWriteDescriptorSet> writes;
        std::shared_ptr<Context> context;
        VkDescriptorSet descriptorSet{};
    };    

    Write& write();

private:
    std::shared_ptr<Context> m_context;
    VkDescriptorSet m_descriptorSet{}; 
};

} // namespace vulkan

} // namespace gfx

#endif