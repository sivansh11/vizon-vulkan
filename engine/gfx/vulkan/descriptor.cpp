#include "descriptor.hpp"

#include "core/log.hpp"

namespace gfx {
    
namespace vulkan {

DescriptorSetLayout::Builder& DescriptorSetLayout::Builder::addLayoutBinding(uint32_t binding, VkDescriptorType descriptorType, uint32_t count, VkShaderStageFlags shaderStageFlags) {
    VkDescriptorSetLayoutBinding descriptorSetLayoutBinding{};
    descriptorSetLayoutBinding.binding = binding;
    descriptorSetLayoutBinding.descriptorType = descriptorType;
    descriptorSetLayoutBinding.descriptorCount = count;
    descriptorSetLayoutBinding.stageFlags = shaderStageFlags;
    descriptorSetLayoutBindings.push_back(descriptorSetLayoutBinding);
    return *this;
}

std::shared_ptr<DescriptorSetLayout> DescriptorSetLayout::Builder::build(std::shared_ptr<Context> context) {
    VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo{};
    descriptorSetLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    descriptorSetLayoutCreateInfo.bindingCount = descriptorSetLayoutBindings.size();
    descriptorSetLayoutCreateInfo.pBindings = descriptorSetLayoutBindings.data();

    VkDescriptorSetLayout descriptorSetLayout{};

    if (vkCreateDescriptorSetLayout(context->device(), &descriptorSetLayoutCreateInfo, nullptr, &descriptorSetLayout) != VK_SUCCESS) {
        ERROR("Failed to create descriptor set");
        std::terminate();
    }

    return std::make_shared<DescriptorSetLayout>(context, descriptorSetLayout);
}

DescriptorSetLayout::DescriptorSetLayout(std::shared_ptr<Context> context, VkDescriptorSetLayout descriptorSetLayout) 
  : m_context(context),
    m_descriptorSetLayout(descriptorSetLayout) {

    TRACE("Created descriptor set layout");
}

DescriptorSetLayout::~DescriptorSetLayout() {
    vkDestroyDescriptorSetLayout(m_context->device(), m_descriptorSetLayout, nullptr);
    TRACE("Destroyed descriptor set layout");
}

std::shared_ptr<DescriptorSet> DescriptorSet::Builder::build(std::shared_ptr<Context> context, std::shared_ptr<DescriptorSetLayout> descriptorSetLayout) {
    VkDescriptorSetAllocateInfo descriptorSetAllocateInfo{};
    descriptorSetAllocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    descriptorSetAllocateInfo.descriptorPool = context->descriptorPool();
    descriptorSetAllocateInfo.descriptorSetCount = 1;
    VkDescriptorSetLayout p_descriptorSetLayout[] = { descriptorSetLayout->descriptorSetLayout() }; 
    descriptorSetAllocateInfo.pSetLayouts = p_descriptorSetLayout;

    VkDescriptorSet descriptorSet;

    if (vkAllocateDescriptorSets(context->device(), &descriptorSetAllocateInfo, &descriptorSet) != VK_SUCCESS) {
        ERROR("Failed to allocate descriptor set");
        std::terminate();
    }
    return std::make_shared<DescriptorSet>(context, descriptorSet);
}

DescriptorSet::DescriptorSet(std::shared_ptr<Context> context, VkDescriptorSet descriptorSet) 
  : m_context(context),
    m_descriptorSet(descriptorSet) {
    TRACE("Created descriptor set");
}

DescriptorSet::~DescriptorSet() {
    // vkFreeDescriptorSets(m_context->device(), m_context->descriptorPool(), 1, &m_descriptorSet);
    // TRACE("Destroyed descriptor set");
}

DescriptorSet::Write::Write(std::shared_ptr<Context> context, VkDescriptorSet descriptorSet) 
  : context(context),
    descriptorSet(descriptorSet) {
    
}

DescriptorSet::Write& DescriptorSet::write() {
    // not how I write code normally, kinda funny 
    return *new Write(m_context, m_descriptorSet);    
}

DescriptorSet::Write& DescriptorSet::Write::pushImageInfo(uint32_t binding, uint32_t count, const VkDescriptorImageInfo& descriptorImageInfo) {
    VkWriteDescriptorSet writeDescriptorset{};
    writeDescriptorset.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writeDescriptorset.dstBinding = binding;
    writeDescriptorset.descriptorCount = count;
    writeDescriptorset.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    writeDescriptorset.dstSet = descriptorSet;
    writeDescriptorset.pImageInfo = &descriptorImageInfo;
    writes.push_back(writeDescriptorset);
    return *this;
}

DescriptorSet::Write& DescriptorSet::Write::pushBufferInfo(uint32_t binding, uint32_t count, const VkDescriptorBufferInfo& descriptorBuffeInfo) {
    VkWriteDescriptorSet writeDescriptorset{};
    writeDescriptorset.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writeDescriptorset.dstBinding = binding;
    writeDescriptorset.descriptorCount = count;
    writeDescriptorset.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    writeDescriptorset.dstSet = descriptorSet;
    writeDescriptorset.pBufferInfo = &descriptorBuffeInfo;
    writes.push_back(writeDescriptorset);
    return *this;
}


void DescriptorSet::Write::update() {
    vkUpdateDescriptorSets(context->device(), writes.size(), writes.data(), 0, nullptr);
    delete this;
}

} // namespace vulkan

} // namespace gfx
