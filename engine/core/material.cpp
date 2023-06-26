#include "material.hpp"

#include "core/log.hpp"

namespace core {

static std::shared_ptr<gfx::vulkan::DescriptorSetLayout> globalMaterialDescriptorSetLayout{nullptr};

std::shared_ptr<gfx::vulkan::DescriptorSetLayout> Material::getMaterialDescriptorSetLayout() {
    assert(globalMaterialDescriptorSetLayout);
    return globalMaterialDescriptorSetLayout;
}

void Material::init(std::shared_ptr<gfx::vulkan::Context> context) {
    globalMaterialDescriptorSetLayout = gfx::vulkan::DescriptorSetLayout::Builder{}
        .addLayoutBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT)
        // .addLayoutBinding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT)
        // .addLayoutBinding(2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT)
        .build(context);
}

void Material::destroy() {
    globalMaterialDescriptorSetLayout.reset();
}

void Material::update() {
    descriptorSet->write()
        .pushImageInfo(0, 1, VkDescriptorImageInfo{
            .sampler = diffuse->sampler(),
            .imageView = diffuse->imageView(),
            .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
        })
        // .pushImageInfo(1, 1, VkDescriptorImageInfo{
        //     .sampler = specular->sampler(),
        //     .imageView = specular->imageView(),
        //     .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
        // })
        // .pushImageInfo(2, 1, VkDescriptorImageInfo{
        //     .sampler = normal->sampler(),
        //     .imageView = normal->imageView(),
        //     .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
        // })
        .update();
}

} // namespace core
