#include "descriptor.hpp"

#include "core/log.hpp"

namespace gfx {
    
namespace vulkan {

descriptor_set_layout_builder_t& descriptor_set_layout_builder_t::addLayoutBinding(uint32_t binding, VkDescriptorType descriptorType, uint32_t count, VkShaderStageFlags shaderStageFlags) {
    VkDescriptorSetLayoutBinding descriptorSetLayoutBinding{};
    descriptorSetLayoutBinding.binding = binding;
    descriptorSetLayoutBinding.descriptorType = descriptorType;
    descriptorSetLayoutBinding.descriptorCount = count;
    descriptorSetLayoutBinding.stageFlags = shaderStageFlags;
    descriptorSetLayoutBindings.push_back(descriptorSetLayoutBinding);
    return *this;
}

core::ref<descriptor_set_layout_t> descriptor_set_layout_builder_t::build(core::ref<context_t> context) {
    VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo{};
    descriptorSetLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    descriptorSetLayoutCreateInfo.bindingCount = descriptorSetLayoutBindings.size();
    descriptorSetLayoutCreateInfo.pBindings = descriptorSetLayoutBindings.data();

    VkDescriptorSetLayout descriptorSetLayout{};

    if (vkCreateDescriptorSetLayout(context->device(), &descriptorSetLayoutCreateInfo, nullptr, &descriptorSetLayout) != VK_SUCCESS) {
        ERROR("Failed to create descriptor set");
        std::terminate();
    }

    return core::make_ref<descriptor_set_layout_t>(context, descriptorSetLayout);
}

descriptor_set_layout_t::descriptor_set_layout_t(core::ref<context_t> context, VkDescriptorSetLayout descriptorSetLayout) 
  : _context(context),
    _descriptor_set_layout(descriptorSetLayout) {

    TRACE("Created descriptor set layout");
}

descriptor_set_layout_t::~descriptor_set_layout_t() {
    vkDestroyDescriptorSetLayout(_context->device(), _descriptor_set_layout, nullptr);
    TRACE("Destroyed descriptor set layout");
}

core::ref<descriptor_set_t> descriptor_set_layout_t::new_descriptor_set() {
    return descriptor_set_builder_t{}
        .build(_context, _descriptor_set_layout);
}


core::ref<descriptor_set_t> descriptor_set_builder_t::build(core::ref<context_t> context, core::ref<descriptor_set_layout_t> descriptorSetLayout) {
    VkDescriptorSetAllocateInfo descriptorSetAllocateInfo{};
    descriptorSetAllocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    descriptorSetAllocateInfo.descriptorPool = context->descriptor_pool();
    descriptorSetAllocateInfo.descriptorSetCount = 1;
    VkDescriptorSetLayout p_descriptorSetLayout[] = { descriptorSetLayout->descriptor_set_layout() }; 
    descriptorSetAllocateInfo.pSetLayouts = p_descriptorSetLayout;

    VkDescriptorSet descriptorSet;

    if (vkAllocateDescriptorSets(context->device(), &descriptorSetAllocateInfo, &descriptorSet) != VK_SUCCESS) {
        ERROR("Failed to allocate descriptor set");
        std::terminate();
    }
    return core::make_ref<descriptor_set_t>(context, descriptorSet);
}

core::ref<descriptor_set_t> descriptor_set_builder_t::build(core::ref<context_t> context, VkDescriptorSetLayout descriptorSetLayout) {
    VkDescriptorSetAllocateInfo descriptorSetAllocateInfo{};
    descriptorSetAllocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    descriptorSetAllocateInfo.descriptorPool = context->descriptor_pool();
    descriptorSetAllocateInfo.descriptorSetCount = 1;
    VkDescriptorSetLayout p_descriptorSetLayout[] = { descriptorSetLayout }; 
    descriptorSetAllocateInfo.pSetLayouts = p_descriptorSetLayout;

    VkDescriptorSet descriptorSet;

    if (vkAllocateDescriptorSets(context->device(), &descriptorSetAllocateInfo, &descriptorSet) != VK_SUCCESS) {
        ERROR("Failed to allocate descriptor set");
        std::terminate();
    }
    return core::make_ref<descriptor_set_t>(context, descriptorSet);
}

descriptor_set_t::descriptor_set_t(core::ref<context_t> context, VkDescriptorSet descriptorSet) 
  : m_context(context),
    m_descriptorSet(descriptorSet) {
    TRACE("Created descriptor set");
}

descriptor_set_t::~descriptor_set_t() {
    // vkFreeDescriptorSets(m_context->device(), m_context->descriptorPool(), 1, &m_descriptorSet);
    // TRACE("Destroyed descriptor set");
}

descriptor_set_t::Write::Write(core::ref<context_t> context, VkDescriptorSet descriptorSet) 
  : context(context),
    descriptorSet(descriptorSet) {
    
}

descriptor_set_t::Write& descriptor_set_t::write() {
    // not how I write code normally, kinda funny 
    return *new Write(m_context, m_descriptorSet);    
}

descriptor_set_t::Write& descriptor_set_t::Write::pushImageInfo(uint32_t binding, uint32_t count, const VkDescriptorImageInfo& descriptorImageInfo) {
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

descriptor_set_t::Write& descriptor_set_t::Write::pushBufferInfo(uint32_t binding, uint32_t count, const VkDescriptorBufferInfo& descriptorBuffeInfo) {
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


void descriptor_set_t::Write::update() {
    vkUpdateDescriptorSets(context->device(), writes.size(), writes.data(), 0, nullptr);
    delete this;
}

} // namespace vulkan

} // namespace gfx
