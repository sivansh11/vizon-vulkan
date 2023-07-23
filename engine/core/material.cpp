#include "material.hpp"

#include "core/log.hpp"

namespace core {

static core::ref<gfx::vulkan::descriptor_set_layout_t> globalMaterialDescriptorSetLayout{nullptr};

core::ref<gfx::vulkan::descriptor_set_layout_t> Material::getMaterialDescriptorSetLayout() {
    assert(globalMaterialDescriptorSetLayout);
    return globalMaterialDescriptorSetLayout;
}

void Material::init(core::ref<gfx::vulkan::context_t> context) {
    globalMaterialDescriptorSetLayout = gfx::vulkan::descriptor_set_layout_builder_t{}
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
